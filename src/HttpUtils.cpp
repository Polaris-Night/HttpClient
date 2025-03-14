#include "HttpUtils.h"
#include <event2/http.h>
#include <string_view>

UrlObject::UrlObject( const std::string &url ) : uri_( evhttp_uri_parse( url.c_str() ) ) {}

UrlObject::UrlObject( UrlObject &&other ) {
    *this = std::move( other );
}

UrlObject &UrlObject::operator=( UrlObject &&other ) {
    uri_ = std::exchange( other.uri_, nullptr );
    return *this;
}

UrlObject::~UrlObject() {
    if ( uri_ != nullptr ) {
        evhttp_uri_free( uri_ );
    }
}

std::optional<std::string> UrlObject::Host() const {
    if ( uri_ == nullptr ) {
        return std::nullopt;
    }
    auto *host = evhttp_uri_get_host( uri_ );
    if ( host == nullptr ) {
        return std::nullopt;
    }
    return std::string( host );
}

std::optional<uint16_t> UrlObject::Port() const {
    if ( uri_ == nullptr ) {
        return std::nullopt;
    }
    auto port = evhttp_uri_get_port( uri_ );
    if ( port == -1 ) {
        return std::nullopt;
    }
    return static_cast<uint16_t>( port );
}

std::optional<std::string> UrlObject::Path() const {
    if ( uri_ == nullptr ) {
        return std::nullopt;
    }
    auto *path = evhttp_uri_get_path( uri_ );
    if ( path == nullptr ) {
        return std::nullopt;
    }
    return std::string( path );
}

std::optional<std::string> UrlObject::Scheme() const {
    if ( uri_ == nullptr ) {
        return std::nullopt;
    }
    auto *scheme = evhttp_uri_get_scheme( uri_ );
    if ( scheme == nullptr ) {
        return std::nullopt;
    }
    return std::string( scheme );
}

std::optional<std::string> UrlObject::Query() const {
    if ( uri_ == nullptr ) {
        return std::nullopt;
    }
    auto *query = evhttp_uri_get_query( uri_ );
    if ( query == nullptr ) {
        return std::nullopt;
    }
    return std::string( query );
}

std::string JoinQuery( const std::map<std::string, std::string> &query_map, bool with_query_start ) {
    std::string query;
    if ( with_query_start ) {
        query += "?";
    }
    for ( const auto &[key, value] : query_map ) {
        query += key + "=" + value + "&";
    }
    if ( !query.empty() ) {
        query.pop_back();
    }
    return query;
}

std::map<std::string, std::string> ParseQuery( const std::string &query ) {
    std::map<std::string, std::string> result;
    std::string_view                   qs( query );
    size_t                             start = 0;
    while ( start < qs.size() ) {
        size_t end = qs.find( '&', start );
        if ( end == std::string_view::npos )
            end = qs.size();

        std::string_view pair = qs.substr( start, end - start );
        if ( !pair.empty() ) {
            size_t           delim = pair.find( '=' );
            std::string_view key   = pair.substr( 0, delim );

            std::string_view value = ( delim != std::string_view::npos ) ? pair.substr( delim + 1 ) : "";

            if ( !key.empty() ) {
                result.emplace( std::piecewise_construct, std::forward_as_tuple( key ),
                                std::forward_as_tuple( value ) );
            }
        }
        start = end + 1;
    }
    return result;
}
