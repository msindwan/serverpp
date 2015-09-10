//#include <openssl/applink.c>
//#include <spp/ssl.h>
//
//
//SSL* spp_set_certificate(const char* cert)
//{
//    SSL_CTX* context;
//
//    context = SSL_CTX_new(SSLv23_server_method());
//    SSL_CTX_set_options(context, SSL_OP_SINGLE_DH_USE);
//
//    SSL_CTX_use_certificate_file(context, "6a9ba10b-cb28-409c-960c-bb5353fc0d12.public.pem", SSL_FILETYPE_PEM);
//    SSL_CTX_use_PrivateKey_file(context, "6a9ba10b-cb28-409c-960c-bb5353fc0d12.private.pem", SSL_FILETYPE_PEM);
//
//    return SSL_new(context);
//}
//
//void spp_init_ssl(void)
//{
//    SSL_load_error_strings();
//    SSL_library_init();
//    OpenSSL_add_all_algorithms();
//}
//
//void spp_stop_ssl(SSL* ssl)
//{
//    SSL_shutdown(ssl);
//    SSL_free(ssl);
//}
//
//void spp_free_ssl(void)
//{
//    ERR_free_strings();
//    EVP_cleanup();
//}