#include <iostream>
#include "HttpClient.h"

void TestGet() {
    HttpClient  client;
    HttpRequest request;
    request.SetFullUrl( "http://127.0.0.1:3001/get?a=a&b=b&c=c" )
        .SetMethod( HttpRequest::GET )
        .SetHeader( "Connection", "close" );
    std::cout << "=======> GET request message:\n" << request.ToString() << std::endl;
    auto              ptr      = client.Send( request );
    HttpResponse::Ptr response = std::move( ptr );
    response->WaitForDone();
    std::cout << "=======> GET request success = " << response->IsSuccess() << std::endl;
    std::cout << "=======> GET response message:\n" << response->ToString() << std::endl;
}

void TestPost() {
    HttpClient  client;
    HttpRequest request;
    request.SetFullUrl( "http://127.0.0.1:3001/post" )
        .SetMethod( HttpRequest::POST )
        .SetHeader( "Connection", "close" )
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
    TestPost();
    return 0;
}