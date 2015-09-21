/**
 * Serverpp SSL implementation.
 *
 * Author: Mayank Sindwani
 * Date: 2015-09-18
 */

#include <spp/ssl.h>

/**
 * Initialize SSL
 *
 * @description: Initializes SSL
 */
void spp::init_ssl(void)
{
    SSL_load_error_strings();
    SSL_library_init();
    OpenSSL_add_all_algorithms();
}

/**
 * Initialize SSL
 *
 * @description: Initializes SSL
 * @param[out] {ctx}  // The SSL context.
 * @param[out] {cert} // The SSL certificate.
 * @param[out] {key}  // The SSL key.
 * @returns // The status code.
 */
int spp::load_certificates(SSL_CTX* ctx, const char* cert, const char* key)
{
    // Use the certificate.
    if (SSL_CTX_use_certificate_file(ctx, cert, SSL_FILETYPE_PEM) == 0)
        return -1;

    // Use the private key.
    if (SSL_CTX_use_PrivateKey_file(ctx, key, SSL_FILETYPE_PEM) == 0)
        return -1;

    // Validate the key.
    if (SSL_CTX_check_private_key(ctx) == 0)
        return -1;

    return 0;
}

/**
 * Destroy SSL
 *
 * @description: Destroys SSL
 */
void spp::destroy_ssl(void)
{
    ERR_free_strings();
    EVP_cleanup();
}