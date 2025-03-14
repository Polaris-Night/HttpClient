#include <iostream>
#include "HttpClient.h"

void TestGet() {
    HttpClient  client;
    HttpRequest request;
    // request.SetFullUrl( "http://www.baidu.com:80/" ).SetMethod( HttpRequest::GET );
    request.SetFullUrl( "https://www.baidu.com:443/" ).SetMethod( HttpRequest::GET );
    std::cout << "=======> GET request message:\n" << request.ToString() << std::endl;
    SSLConfig         ssl_config;
    auto              ptr      = client.Send( request, ssl_config );
    HttpResponse::Ptr response = std::move( ptr );
    response->WaitForDone();
    std::cout << "=======> GET request success = " << response->IsSuccess() << std::endl;
    std::cout << "=======> GET response message:\n" << response->ToString() << std::endl;
    std::cout << "=======> GET error: " << response->ErrorString() << std::endl;
}

void TestPost() {
    HttpClient  client;
    HttpRequest request;
    request.SetFullUrl( "http://127.0.0.1:3001/post" )
        .SetMethod( HttpRequest::POST )
        .SetHeader( "Connection", "keep-alive" )
        .SetBody( "a=a&b=b&c=c" );
    std::cout << "=======> POST request message:\n" << request.ToString() << std::endl;
    auto              ptr      = client.Send( request );
    HttpResponse::Ptr response = std::move( ptr );
    response->WaitForDone();
    std::cout << "=======> POST request success = " << response->IsSuccess() << std::endl;
    std::cout << "=======> POST response message:\n" << response->ToString() << std::endl;
}

int main() {
    TestGet();
    std::cout << "============================" << std::endl;
    // TestPost();
    return 0;
}