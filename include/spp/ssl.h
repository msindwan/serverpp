#ifndef SSL_SPP_H
#define SSL_SPP_H

#include <openssl/bio.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

namespace spp
{
    void init_ssl(void);
    int  load_certificates(SSL_CTX*, const char*, const char*);
    void destroy_ssl(void);
}

#endif