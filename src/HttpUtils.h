#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <string>

struct evhttp_uri;

class UrlParser {
public:
    UrlParser( const std::string &url );
    ~UrlParser();

    std::optional<std::string> Host() const;

    std::optional<uint16_t> Port() const;

    std::optional<std::string> Path() const;

    std::optional<std::string> Scheme() const;

    std::optional<std::string> Query() const;

private:
    evhttp_uri *uri_ = nullptr;
};

std::string JoinQuery( const std::map<std::string, std::string> &query_map, bool with_query_start = true );

std::map<std::string, std::string> ParseQuery( const std::string &query );
