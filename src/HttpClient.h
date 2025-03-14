#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <future>
#include <map>
#include <memory>
#include <string>

#include "SSLConfig.h"

struct evhttp_request;
struct evhttp_connection;
struct event_base;

class HttpClient;

class HttpRequest final {
public:
    enum Method
    {
        POST,
        GET,
    };

    HttpRequest &SetMethod( Method method );
    HttpRequest &SetFullUrl( const std::string &url );               // e.g. http://www.example.com/path?query=value
    HttpRequest &SetScheme( const std::string &scheme );             // e.g. http, https
    HttpRequest &SetHost( const std::string &host, uint16_t port );  // e.g. host: www.example.com port: 80
    HttpRequest &SetPath( const std::string &path );                 // e.g. "/", "/path"
    HttpRequest &SetHeader( const std::string &key, const std::string &value );
    HttpRequest &SetHeader( const std::map<std::string, std::string> &header );
    HttpRequest &SetQuery( const std::string &key, const std::string &value );
    HttpRequest &SetQuery( const std::map<std::string, std::string> &query );
    HttpRequest &SetBody( const std::string &body );
    HttpRequest &SetBody( std::string &&body );

    Method                                    GetMethod() const;
    const std::string                        &GetScheme() const;
    const std::string                        &GetHost() const;
    uint16_t                                  GetPort() const;
    const std::string                        &GetPath() const;  // Should not be empty, at least "/" on request
    const std::map<std::string, std::string> &GetHeader() const;
    std::string                               GetHeader( const std::string &key ) const;
    const std::map<std::string, std::string> &GetQuery() const;
    std::string                               GetQuery( const std::string &key ) const;
    std::string                               GetUri() const;
    const std::string                        &GetBody() const;

    std::string ToString() const;


private:
    friend class HttpClient;
    using DoneCallback = void ( * )( evhttp_request *, void * );
    [[nodiscard( "must be free by evhttp_request_free()" )]] evhttp_request *ToEvRequest( DoneCallback &&cb,
                                                                                          void          *cb_arg ) const;

    Method                             method_ = Method::POST;
    std::string                        scheme_;
    std::string                        host_;
    uint16_t                           port_ = 80;
    std::string                        path_;
    std::map<std::string, std::string> header_;
    std::map<std::string, std::string> query_;
    std::string                        body_;
};

class HttpResponse final {
public:
    using Ptr = std::unique_ptr<HttpResponse>;

    HttpResponse( HttpResponse &&other );
    HttpResponse &operator=( HttpResponse &&other );
    HttpResponse( const HttpResponse & )            = delete;
    HttpResponse &operator=( const HttpResponse & ) = delete;
    ~HttpResponse();

    bool IsDone();
    void WaitForDone();
    bool WaitFor( int timeout_ms );

    int                                       StatusCode() const;
    const std::string                        &StatusPhrase() const;
    const std::string                        &Body() const;
    const std::map<std::string, std::string> &Header() const;
    std::string                               Header( const std::string &key ) const;
    bool                                      IsSuccess() const;
    const std::string                        &ErrorString() const;


    std::string ToString() const;

private:
    explicit HttpResponse();
    friend class HttpClient;

    std::atomic_bool   is_done_ = false;
    std::promise<bool> promise_;
    std::future<bool>  future_;

    std::string                        http_version_;
    int                                status_code_ = -1;
    std::string                        status_phrase_;
    std::map<std::string, std::string> header_;
    std::string                        body_;
    std::string                        error_;

private:
    friend void OnRequestDone( evhttp_request *, void * );

    evhttp_connection *connection_ = nullptr;
    evhttp_request    *request_    = nullptr;
};

class HttpClient final {
public:
    explicit HttpClient();
    HttpClient( event_base *base );
    ~HttpClient();

    /**
     * @brief HTTP request
     *
     * @param request
     * @return HttpResponse::Ptr
     */
    [[nodiscard]] HttpResponse::Ptr Send( const HttpRequest &request );

    /**
     * @brief HTTPS request
     * If build without SSL support, this function is the same as Send(const HttpRequest &)
     *
     * @param request
     * @param ssl_config
     * @return HttpResponse::Ptr
     */
    [[nodiscard]] HttpResponse::Ptr Send( const HttpRequest &request, SSLConfig &ssl_config );

private:
    void StartEventLoop();
    void StopEventLoop();

    event_base      *base_ = nullptr;
    std::thread      worker_;
    std::atomic_bool running_ = false;
};
