#include "HttpClient.h"
#include <event2/buffer.h>
#include <event2/bufferevent.h>
#ifdef BUILD_WITH_SSL
    #include <event2/bufferevent_ssl.h>
#endif
#include <event2/event.h>
#include <event2/thread.h>
#include <event2/util.h>
#include <evhttp.h>
#include <string.h>
#include <iostream>
#include <thread>

#include "HttpUtils.h"

#define EV_HTTP_VERSION "HTTP/1.1"  // version from evhttp_make_request

#define EV_RAII( type )                                         \
    struct type##_deleter {                                     \
        void operator()( struct type *p ) { type##_free( p ); } \
    };                                                          \
    using raii_##type = std::unique_ptr<struct type, type##_deleter>;

EV_RAII( evhttp_request )
EV_RAII( evhttp_connection )

namespace {

evhttp_cmd_type ToEvType( HttpRequest::Method method ) {
    switch ( method ) {
        case HttpRequest::Method::GET:
            return EVHTTP_REQ_GET;
        case HttpRequest::Method::POST:
        default:
            return EVHTTP_REQ_POST;
    }
}

}  // namespace

void OnRequestDone( evhttp_request *req, void *arg ) {
    if ( arg == nullptr ) {
        return;
    }
    HttpResponse *resp = reinterpret_cast<HttpResponse *>( arg );
    if ( req == nullptr ) {
        resp->promise_.set_value( true );
        return;
    }
    // HTTP version
    char buf[10] = {};
    std::snprintf( buf, std::size( buf ), "HTTP/%d.%d", req->major, req->minor );
    resp->http_version_ = buf;
    // status
    resp->status_code_ = evhttp_request_get_response_code( req );
    auto *code_line    = evhttp_request_get_response_code_line( req );
    if ( code_line != nullptr ) {
        resp->status_phrase_ = std::string( code_line );
    }
    // response headers
    auto *headers = evhttp_request_get_input_headers( req );
    for ( evkeyval *header = headers->tqh_first; header != nullptr; header = header->next.tqe_next ) {
        resp->header_[header->key] = header->value;
    }
    // response data
    auto *buffer = evhttp_request_get_input_buffer( req );
    if ( buffer ) {
        size_t len = evbuffer_get_length( buffer );
        resp->body_.resize( len );
        evbuffer_copyout( buffer, resp->body_.data(), len );
    }
    // error message
    if ( !resp->IsSuccess() ) {
        std::string error = "[Socket error: ";
        error += evutil_socket_error_to_string( errno );
        error += "]; [SSL error: " + SSLConfig::SSLErrorString() + "]";
        resp->error_ = std::move( error );
    }
    resp->promise_.set_value( true );
}

HttpRequest &HttpRequest::SetMethod( Method method ) {
    method_ = method;
    return *this;
}

HttpRequest &HttpRequest::SetFullUrl( const std::string &url ) {
    UrlObject parser( url );
    auto      scheme = parser.Scheme();
    auto      host   = parser.Host();
    auto      port   = parser.Port();
    auto      path   = parser.Path();
    auto      query  = parser.Query();
    if ( scheme ) {
        scheme_ = *scheme;
    }
    if ( host ) {
        host_ = *host;
    }
    if ( port ) {
        port_ = *port;
    }
    if ( path ) {
        path_ = *path;
    }
    if ( query ) {
        query_ = ParseQuery( *query );
    }
    return *this;
}

HttpRequest &HttpRequest::SetScheme( const std::string &scheme ) {
    scheme_ = scheme;
    return *this;
}

HttpRequest &HttpRequest::SetHost( const std::string &host, uint16_t port ) {
    host_ = host;
    port_ = port;
    return *this;
}

HttpRequest &HttpRequest::SetPath( const std::string &path ) {
    path_ = path;
    return *this;
}

HttpRequest &HttpRequest::SetHeader( const std::string &key, const std::string &value ) {
    header_[key] = value;
    return *this;
}

HttpRequest &HttpRequest::SetHeader( const std::map<std::string, std::string> &header ) {
    header_ = header;
    return *this;
}

HttpRequest &HttpRequest::SetQuery( const std::string &key, const std::string &value ) {
    query_[key] = value;
    return *this;
}

HttpRequest &HttpRequest::SetQuery( const std::map<std::string, std::string> &query ) {
    query_ = query;
    return *this;
}

HttpRequest &HttpRequest::SetBody( const std::string &body ) {
    body_ = body;
    return *this;
}

HttpRequest &HttpRequest::SetBody( std::string &&body ) {
    body_ = std::move( body );
    return *this;
}

HttpRequest::Method HttpRequest::GetMethod() const {
    return method_;
}

const std::string &HttpRequest::GetScheme() const {
    return scheme_;
}

const std::string &HttpRequest::GetHost() const {
    return host_;
}

uint16_t HttpRequest::GetPort() const {
    return port_;
}

const std::string &HttpRequest::GetPath() const {
    return path_;
}

const std::map<std::string, std::string> &HttpRequest::GetHeader() const {
    return header_;
}

std::string HttpRequest::GetHeader( const std::string &key ) const {
    return header_.count( key ) > 0 ? header_.at( key ) : "";
}

const std::map<std::string, std::string> &HttpRequest::GetQuery() const {
    return query_;
}

std::string HttpRequest::GetQuery( const std::string &key ) const {
    return query_.count( key ) > 0 ? query_.at( key ) : "";
}

std::string HttpRequest::GetUri() const {
    if ( method_ == Method::GET ) {
        return path_ + JoinQuery( query_ );
    }
    return path_;
}

const std::string &HttpRequest::GetBody() const {
    return body_;
}

std::string HttpRequest::ToString() const {
    // e.g.

    // GET /index.html HTTP/1.1
    // Host: www.example.com
    // User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64; rv:91.0) Gecko/20100101 Firefox/91.0
    // Accept: text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,*/*;q=0.8
    // Accept-Encoding: gzip, deflate
    // Connection: keep-alive
    std::string ret =
        std::string( method_ == Method::GET ? "GET" : "POST" ) + " " + GetUri() + " " + EV_HTTP_VERSION + "\r\n";
    for ( auto &[key, value] : header_ ) {
        ret += key + ": " + value + "\r\n";
    }
    ret += "\r\n" + body_;
    return ret;
}

evhttp_request *HttpRequest::ToEvRequest( DoneCallback &&cb, void *cb_arg ) const {
    raii_evhttp_request req( evhttp_request_new( cb, cb_arg ) );
    if ( !req ) {
        return nullptr;
    }
    // header
    auto *req_headers = evhttp_request_get_output_headers( req.get() );
    if ( header_.count( "Host" ) == 0 ) {
        std::string host = host_ + ":" + std::to_string( port_ );
        evhttp_add_header( req_headers, "Host", host.c_str() );
    }
    for ( auto &[key, value] : header_ ) {
        evhttp_add_header( req_headers, key.c_str(), value.c_str() );
    }
    // data
    auto *req_buffer = evhttp_request_get_output_buffer( req.get() );
    if ( method_ != Method::GET ) {
        if ( evbuffer_add( req_buffer, body_.c_str(), body_.size() ) != 0 ) {
            return nullptr;
        }
    }
    return req.release();
}

HttpResponse::HttpResponse() : future_( promise_.get_future() ) {}

HttpResponse::HttpResponse( HttpResponse &&other ) {
    *this = std::move( other );
}

HttpResponse &HttpResponse::operator=( HttpResponse &&other ) {
    is_done_       = other.is_done_.load();
    other.is_done_ = false;
    promise_       = std::move( other.promise_ );
    future_        = std::move( other.future_ );

    status_code_ = other.status_code_;
    body_        = std::move( other.body_ );
    header_      = std::move( other.header_ );
    connection_  = std::exchange( other.connection_, nullptr );
    request_     = std::exchange( other.request_, nullptr );
    return *this;
}

HttpResponse::~HttpResponse() {
    if ( request_ ) {
        evhttp_cancel_request( request_ );
        evhttp_request_free( request_ );
        request_ = nullptr;
    }
    if ( connection_ ) {
        evhttp_connection_free( connection_ );
        connection_ = nullptr;
    }
}

bool HttpResponse::IsDone() {
    if ( !is_done_ ) {
        is_done_ = WaitFor( 0 );
    }
    return is_done_;
}

void HttpResponse::WaitForDone() {
    if ( !is_done_ && future_.valid() ) {
        is_done_ = future_.get();
    }
}

bool HttpResponse::WaitFor( int timeout_ms ) {
    if ( !is_done_ && future_.valid() ) {
        auto status = future_.wait_for( std::chrono::milliseconds( timeout_ms ) );
        if ( status == std::future_status::ready ) {
            is_done_ = future_.get();
        }
    }
    return is_done_;
}

int HttpResponse::StatusCode() const {
    return status_code_;
}

const std::string &HttpResponse::StatusPhrase() const {
    return status_phrase_;
}

const std::string &HttpResponse::Body() const {
    return body_;
}

const std::map<std::string, std::string> &HttpResponse::Header() const {
    return header_;
}

std::string HttpResponse::Header( const std::string &key ) const {
    auto it = header_.find( key );
    return it != header_.end() ? it->second : "";
}

bool HttpResponse::IsSuccess() const {
    return status_code_ >= 200 && status_code_ < 300;
}

const std::string &HttpResponse::ErrorString() const {
    return error_;
}

std::string HttpResponse::ToString() const {
    // e.g.

    // HTTP/1.1 200 OK
    // Date: Wed, 18 Apr 2024 12:00:00 GMT
    // Server: Apache/2.4.1 (Unix)
    // Last-Modified: Wed, 18 Apr 2024 11:00:00 GMT
    // Content-Length: 12345
    // Content-Type: text/html; charset=UTF-8

    // <!DOCTYPE html>
    // <html>
    // <head>
    //     <title>Example Page</title>
    // </head>
    // <body>
    //     <h1>Hello, World!</h1>
    //     <!-- The rest of the HTML content -->
    // </body>
    // </html>
    std::string ret = http_version_ + " " + std::to_string( status_code_ ) + " " + status_phrase_ + "\r\n";
    for ( auto &[key, value] : header_ ) {
        ret += key + ": " + value + "\r\n";
    }
    ret += "\r\n" + body_;
    return ret;
}

HttpClient::HttpClient() {
    StartEventLoop();
}

HttpClient::HttpClient( event_base *base ) : base_( base ) {
    if ( base_ == nullptr ) {
        throw std::invalid_argument( "event base can not be null" );
    }
}

HttpClient::~HttpClient() {
    if ( running_ && base_ ) {
        StopEventLoop();
    }
}

HttpResponse::Ptr HttpClient::Send( const HttpRequest &request ) {
    HttpResponse::Ptr response( new HttpResponse() );
    // connection
    raii_evhttp_connection connection(
        evhttp_connection_base_new( base_, nullptr, request.GetHost().c_str(), request.GetPort() ) );
    if ( !connection ) {
        return nullptr;
    }
    // request
    raii_evhttp_request req( request.ToEvRequest( OnRequestDone, response.get() ) );
    if ( !req ) {
        return nullptr;
    }
    evhttp_request_own( req.get() );
    evhttp_request_set_error_cb( req.get(), []( evhttp_request_error error, void * ) {
        std::cerr << "http request error: " << error << std::endl;
    } );
    if ( evhttp_make_request( connection.get(), req.get(), ToEvType( request.GetMethod() ),
                              request.GetUri().c_str() ) != 0 ) {
        return nullptr;
    }
    // all success
    response->connection_ = connection.release();
    response->request_    = req.release();
    return response;
}

HttpResponse::Ptr HttpClient::Send( const HttpRequest &request, SSLConfig &ssl_config ) {
#ifdef BUILD_WITH_SSL
    std::cerr << request.GetHost() << " " << request.GetPort() << " " << request.GetScheme() << " " << request.GetPath()
              << std::endl;

    HttpResponse::Ptr response( new HttpResponse() );
    // buffer
    bufferevent *bufev = nullptr;
    if ( request.GetScheme() != "https" ) {
        bufev = bufferevent_socket_new( base_, -1, BEV_OPT_CLOSE_ON_FREE );
    }
    else {
        auto *ssl = ssl_config.CreateSSL( request.GetHost() );
        if ( ssl == nullptr ) {
            return nullptr;
        }
        bufev = bufferevent_openssl_socket_new( base_, -1, ssl, BUFFEREVENT_SSL_CONNECTING,
                                                BEV_OPT_CLOSE_ON_FREE | BEV_OPT_DEFER_CALLBACKS );
    }
    if ( !bufev ) {
        return nullptr;
    }
    bufferevent_openssl_set_allow_dirty_shutdown( bufev, 1 );
    // connection
    raii_evhttp_connection connection(
        evhttp_connection_base_bufferevent_new( base_, nullptr, bufev, request.GetHost().c_str(), request.GetPort() ) );
    if ( !connection ) {
        return nullptr;
    }
    // request
    raii_evhttp_request req( request.ToEvRequest( OnRequestDone, response.get() ) );
    if ( !req ) {
        return nullptr;
    }
    evhttp_request_own( req.get() );
    evhttp_request_set_error_cb( req.get(), []( evhttp_request_error error, void * ) {
        std::cerr << "http request error: " << error << std::endl;
    } );
    if ( evhttp_make_request( connection.get(), req.get(), ToEvType( request.GetMethod() ),
                              request.GetUri().c_str() ) != 0 ) {
        return nullptr;
    }
    // all success
    response->connection_ = connection.release();
    response->request_    = req.release();
    return response;
#else
    (void)ssl_config;
    return Send( request );
#endif
}

void HttpClient::StartEventLoop() {
    if ( running_ && base_ ) {
        StopEventLoop();
    }
    evthread_use_pthreads();
    base_ = event_base_new();
    if ( !base_ ) {
        throw std::runtime_error( "Failed to create event base" );
    }
    running_ = true;
    worker_  = std::thread( [this]() {
        std::chrono::milliseconds ms( 300 );
        while ( running_ ) {
            event_base_dispatch( base_ );
            std::this_thread::sleep_for( ms );
        }
    } );
}

void HttpClient::StopEventLoop() {
    running_ = false;
    event_base_loopexit( base_, nullptr );
    if ( worker_.joinable() ) {
        worker_.join();
    }
    event_base_free( base_ );
    base_ = nullptr;
}
