#include "SSLConfig.h"
#ifdef BUILD_WITH_SSL
    #include <openssl/err.h>
    #include <openssl/rand.h>
    #include <openssl/ssl.h>
    #include <openssl/x509.h>
    #include <openssl/x509v3.h>
#endif
#include <atomic>
#include <iostream>

static std::atomic_bool ssl_init_ = false;

namespace {
class OpenSSLErrorHandler {
public:
    static std::string getOpenSSLErrors() {
        std::string errors;
#ifdef BUILD_WITH_SSL
        unsigned long err;
        while ( ( err = ERR_get_error() ) != 0 ) {
            char buf[256];
            ERR_error_string_n( err, buf, sizeof( buf ) );
            errors += buf;
            errors += "\n";
        }
#endif
        return errors.empty() ? "No additional OpenSSL errors" : errors;
    }
};
}  // namespace

SSLConfig::SSLConfig() {
    Init();
}

SSLConfig::SSLConfig( const std::string &cert_path ) : cert_path_( cert_path ) {
    Init();
}

SSLConfig::~SSLConfig() {
    if ( context_ ) {
#ifdef BUILD_WITH_SSL
        SSL_CTX_free( context_ );
#endif
        context_ = nullptr;
    }
}

SSL_CTX *SSLConfig::GetContext() const {
    return context_;
}

SSL *SSLConfig::CreateSSL( const std::string &host ) {
#ifdef BUILD_WITH_SSL
    auto *ssl = SSL_new( context_ );
    if ( ssl == nullptr ) {
        std::cerr << "Failed to create SSL object: " << OpenSSLErrorHandler::getOpenSSLErrors() << std::endl;
        return nullptr;
    }
    #ifdef SSL_CTRL_SET_TLSEXT_HOSTNAME
    // Set hostname for SNI extension
    SSL_set_tlsext_host_name( ssl, host.c_str() );
    #endif
    return ssl;
#else
    return nullptr;
#endif
}

void SSLConfig::FreeSSL( SSL *ssl ) {
#ifdef BUILD_WITH_SSL
    if ( ssl != nullptr ) {
        SSL_free( ssl );
    }
#endif
}

void SSLConfig::Init() {
    InitializeOpenSSL();
    CreateContext();
    LoadCertificates();
    SetHostnameValidation();
}

void SSLConfig::InitializeOpenSSL() {
#ifdef BUILD_WITH_SSL
    if ( ssl_init_ ) {
        return;
    }
    ssl_init_ = true;
    #if ( OPENSSL_VERSION_NUMBER < 0x10100000L ) || \
        ( defined( LIBRESSL_VERSION_NUMBER ) && LIBRESSL_VERSION_NUMBER < 0x20700000L )
    SSL_library_init();
    ERR_load_crypto_strings();
    SSL_load_error_strings();
    OpenSSL_add_all_algorithms();
    #else
    SSL_library_init();
    #endif
    if ( RAND_poll() == 0 ) {
        std::cerr << "Failed to initialize OpenSSL random number generator." << std::endl;
        ssl_init_ = false;
        throw std::runtime_error( "RAND_poll failed" );
    }
#endif
}

void SSLConfig::CreateContext() {
#ifdef BUILD_WITH_SSL
    context_ = SSL_CTX_new( SSLv23_method() );
    if ( context_ == nullptr ) {
        throw std::runtime_error( "SSL_CTX_new failed: " + OpenSSLErrorHandler::getOpenSSLErrors() );
    }
#endif
}

void SSLConfig::LoadCertificates() {
#ifdef BUILD_WITH_SSL
    if ( cert_path_.empty() ) {
        X509_STORE *store;
        store = SSL_CTX_get_cert_store( context_ );
    #ifdef _WIN32
        if ( add_cert_for_store( store, "CA" ) < 0 || add_cert_for_store( store, "AuthRoot" ) < 0 ||
             add_cert_for_store( store, "ROOT" ) < 0 ) {
            throw std::runtime_error( "Failed to load system certificates" );
        }
    #else   // _WIN32
        if ( X509_STORE_set_default_paths( store ) != 1 ) {
            throw std::runtime_error( "X509_STORE_set_default_paths failed: " +
                                      OpenSSLErrorHandler::getOpenSSLErrors() );
        }
    #endif  // _WIN32
    }
    else {
        if ( SSL_CTX_load_verify_locations( context_, cert_path_.c_str(), nullptr ) != 1 ) {
            throw std::runtime_error( "SSL_CTX_load_verify_locations failed, path: " + cert_path_ +
                                      " , error: " + OpenSSLErrorHandler::getOpenSSLErrors() );
        }
    }
#endif
}

void SSLConfig::SetHostnameValidation() {
#ifdef BUILD_WITH_SSL
    SSL_CTX_set_verify( context_, SSL_VERIFY_PEER, nullptr );
    SSL_CTX_set_cert_verify_callback(
        context_,
        []( auto, auto ) {
            // TODO: implement
            return 1;
        },
        nullptr );
#endif
}

std::string SSLConfig::SSLErrorString() {
    return OpenSSLErrorHandler::getOpenSSLErrors();
}
