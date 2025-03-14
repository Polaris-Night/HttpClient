#pragma once

#include <string>

struct ssl_ctx_st;
typedef struct ssl_ctx_st SSL_CTX;

struct ssl_st;
typedef struct ssl_st SSL;

class SSLConfig {
public:
    explicit SSLConfig();
    SSLConfig( const std::string &cert_path );
    ~SSLConfig();

    SSL_CTX *GetContext() const;

    SSL *CreateSSL( const std::string &host );

    /**
     * @brief
     * It is unnecessary to call this function when ssl given bufferevent_openssl_socket_new and set
     * BEV_OPT_CLOSE_ON_FREE, it will be freed automatically.
     *
     * @param ssl
     * If ssl is nullptr, it will do nothing.
     */
    void FreeSSL( SSL *ssl );

    static std::string SSLErrorString();

private:
    void Init();

    void InitializeOpenSSL();
    void CreateContext();
    void LoadCertificates();
    void SetHostnameValidation();

    std::string cert_path_;
    SSL_CTX    *context_ = nullptr;
};