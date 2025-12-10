/**
 * connecting.c - HTTPS CONNECT handling with TLS MITM
 *
 * Flow:
 * 1. Send "200 Connection Established" to client
 * 2. Read ClientHello from client, extract SNI
 * 3. DNS lookup for target server
 * 4. Connect to target server
 * 5. TLS handshake with server, extract CN/SANs
 * 6. Generate spoofed certificate
 * 7. TLS handshake with client using spoofed cert
 * 8. Begin tunneling
 */

#include "proxy.h"
#include "connection.h"

#include <openssl/x509.h>
#include <openssl/x509v3.h>
#include <openssl/pem.h>
#include <openssl/evp.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#define CONNECTION_ESTABLISHED "HTTP/1.1 200 Connection Established\r\n\r\n"

/* Forward declarations */
static void send_200_connection_established(struct ev_loop *loop, ev_io *w, int revents);
static void extract_sni_from_client(struct ev_loop *loop, ev_io *w, int revents);
static void connect_to_target(connection_t *conn);
static void on_server_connect(struct ev_loop *loop, ev_io *w, int revents);
static void do_server_tls_handshake(struct ev_loop *loop, ev_io *w, int revents);
static void do_client_tls_handshake(struct ev_loop *loop, ev_io *w, int revents);

/* Implemented in tunneling.c */
void start_tunneling(connection_t *conn);

/**
 * Parse SNI (Server Name Indication) from TLS ClientHello
 * Returns 0 on success, -1 on failure/incomplete
 */
static int parse_sni_from_client_hello(const unsigned char *data,
                                       size_t               len,
                                       char                *sni_out,
                                       size_t               sni_size)
{
        if (len < 43) return -1; /* Minimum ClientHello size */

        /* Check TLS record header */
        if (data[0] != 0x16) return -1; /* Not a handshake record */

        /* Get record length */
        size_t record_len = (data[3] << 8) | data[4];
        if (len < 5 + record_len) return -1; /* Incomplete record */

        /* Skip to handshake message */
        const unsigned char *p = data + 5;
        if (*p != 0x01) return -1; /* Not ClientHello */

        /* Skip handshake header (1 type + 3 length) */
        p += 4;

        /* Skip client version (2) + random (32) */
        p += 34;

        /* Skip session ID */
        if (p >= data + len) return -1;
        size_t session_id_len  = *p++;
        p                     += session_id_len;

        /* Skip cipher suites */
        if (p + 2 > data + len) return -1;
        size_t cipher_suites_len  = (p[0] << 8) | p[1];
        p                        += 2 + cipher_suites_len;

        /* Skip compression methods */
        if (p >= data + len) return -1;
        size_t compression_len  = *p++;
        p                      += compression_len;

        /* Check for extensions */
        if (p + 2 > data + len) return -1;
        size_t extensions_len  = (p[0] << 8) | p[1];
        p                     += 2;

        const unsigned char *extensions_end = p + extensions_len;
        if (extensions_end > data + len) return -1;

        /* Parse extensions looking for SNI (type 0x0000) */
        while (p + 4 <= extensions_end) {
                uint16_t ext_type  = (p[0] << 8) | p[1];
                uint16_t ext_len   = (p[2] << 8) | p[3];
                p                 += 4;

                if (p + ext_len > extensions_end) return -1;

                if (ext_type == 0x0000) { /* SNI extension */
                        /* SNI extension format:
                         * 2 bytes: list length
                         * 1 byte: name type (0 = hostname)
                         * 2 bytes: name length
                         * N bytes: name
                         */
                        if (ext_len < 5) return -1;

                        const unsigned char *sni_data  = p;
                        /* Skip list length */
                        sni_data                      += 2;

                        uint8_t name_type = *sni_data++;
                        if (name_type != 0) return -1; /* Not a hostname */

                        uint16_t name_len  = (sni_data[0] << 8) | sni_data[1];
                        sni_data          += 2;

                        if (name_len >= sni_size) return -1;
                        memcpy(sni_out, sni_data, name_len);
                        sni_out[name_len] = '\0';
                        return 0;
                }

                p += ext_len;
        }

        return -1; /* SNI not found */
}

/**
 * Extract Common Name (CN) from X509 certificate
 */
static int extract_cn_from_cert(X509 *cert, char *cn_out, size_t cn_size)
{
        X509_NAME *subject = X509_get_subject_name(cert);
        if (!subject) return -1;

        int idx = X509_NAME_get_index_by_NID(subject, NID_commonName, -1);
        if (idx < 0) return -1;

        X509_NAME_ENTRY *entry = X509_NAME_get_entry(subject, idx);
        if (!entry) return -1;

        ASN1_STRING *data = X509_NAME_ENTRY_get_data(entry);
        if (!data) return -1;

        const unsigned char *cn     = ASN1_STRING_get0_data(data);
        int                  cn_len = ASN1_STRING_length(data);

        if ((size_t)cn_len >= cn_size) cn_len = cn_size - 1;
        memcpy(cn_out, cn, cn_len);
        cn_out[cn_len] = '\0';

        return 0;
}

/**
 * Generate a spoofed certificate signed by our CA
 * Returns cert on success, NULL on failure. Key is stored in *key_out.
 */
static X509 *generate_spoofed_certificate(X509      *server_cert,
                                          X509      *ca_cert,
                                          EVP_PKEY  *ca_key,
                                          EVP_PKEY **key_out)
{
        *key_out = NULL;

        log_debug("generate_spoofed_certificate: starting");

        if (!server_cert) {
                log_error("server_cert is NULL");
                return NULL;
        }
        if (!ca_cert) {
                log_error("ca_cert is NULL");
                return NULL;
        }
        if (!ca_key) {
                log_error("ca_key is NULL");
                return NULL;
        }

        X509 *cert = X509_new();
        if (!cert) {
                log_error("Failed to create X509 object");
                return NULL;
        }
        log_debug("generate_spoofed_certificate: X509 created");

        /* Set version to X509v3 */
        X509_set_version(cert, 2);

        /* Generate random serial number */
        ASN1_INTEGER *serial = ASN1_INTEGER_new();
        BIGNUM       *bn     = BN_new();
        BN_rand(bn, 64, BN_RAND_TOP_ANY, BN_RAND_BOTTOM_ANY);
        BN_to_ASN1_INTEGER(bn, serial);
        X509_set_serialNumber(cert, serial);
        ASN1_INTEGER_free(serial);
        BN_free(bn);

        /* Set validity period */
        X509_gmtime_adj(X509_getm_notBefore(cert), 0);
        X509_gmtime_adj(X509_getm_notAfter(cert), 365 * 24 * 60 * 60);

        /* Copy subject from server cert */
        X509_set_subject_name(cert, X509_get_subject_name(server_cert));

        /* Set issuer to CA */
        X509_set_issuer_name(cert, X509_get_subject_name(ca_cert));

        /* Generate new key pair for this certificate (OpenSSL 3.0+ API) */
        EVP_PKEY     *pkey     = NULL;
        EVP_PKEY_CTX *pkey_ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, NULL);
        if (!pkey_ctx) {
                log_error("Failed to create EVP_PKEY_CTX");
                X509_free(cert);
                return NULL;
        }
        if (EVP_PKEY_keygen_init(pkey_ctx) <= 0 ||
            EVP_PKEY_CTX_set_rsa_keygen_bits(pkey_ctx, 2048) <= 0 ||
            EVP_PKEY_keygen(pkey_ctx, &pkey) <= 0) {
                log_error("Failed to generate RSA key pair");
                EVP_PKEY_CTX_free(pkey_ctx);
                X509_free(cert);
                return NULL;
        }
        EVP_PKEY_CTX_free(pkey_ctx);
        log_debug("generate_spoofed_certificate: key generated");

        X509_set_pubkey(cert, pkey);

        /* Copy Subject Alternative Names from server cert */
        int san_idx = X509_get_ext_by_NID(server_cert, NID_subject_alt_name, -1);
        if (san_idx >= 0) {
                X509_EXTENSION *san_ext = X509_get_ext(server_cert, san_idx);
                if (san_ext) {
                        X509_add_ext(cert, san_ext, -1);
                }
        }

        /* Add basic constraints (not a CA) */
        X509V3_CTX ctx;
        X509V3_set_ctx_nodb(&ctx);
        X509V3_set_ctx(&ctx, ca_cert, cert, NULL, NULL, 0);

        X509_EXTENSION *ext =
                X509V3_EXT_conf_nid(NULL, &ctx, NID_basic_constraints, "CA:FALSE");
        if (ext) {
                X509_add_ext(cert, ext, -1);
                X509_EXTENSION_free(ext);
        }

        log_debug("generate_spoofed_certificate: signing with CA key...");

        /* Sign with CA key */
        if (X509_sign(cert, ca_key, EVP_sha256()) <= 0) {
                unsigned long err = ERR_get_error();
                log_error("Failed to sign certificate: %s", ERR_error_string(err, NULL));
                EVP_PKEY_free(pkey);
                X509_free(cert);
                return NULL;
        }

        log_info("Generated spoofed certificate successfully");
        *key_out = pkey;
        return cert;
}

/**
 * Step 1: Start the HTTPS CONNECT process
 * Wait for client socket to be writable, then send 200
 */
void start_connection_process(connection_t *conn)
{
        log_info("Starting HTTPS CONNECT to %s:%d", conn->target_host, conn->target_port);

        /* Initialize SSL pointers to NULL */
        conn->client_ssl_ctx   = NULL;
        conn->client_ssl       = NULL;
        conn->server_ssl_ctx   = NULL;
        conn->server_ssl       = NULL;
        conn->client_hello_len = 0;
        conn->sni_hostname[0]  = '\0';

        /* Wait for client socket to be writable to send 200 */
        ev_io_init(&conn->client_watcher,
                   send_200_connection_established,
                   conn->client_fd,
                   EV_WRITE);
        ev_io_start(conn->ctx->loop, &conn->client_watcher);
}

/**
 * Step 2: Send "200 Connection Established" to client
 */
static void send_200_connection_established(struct ev_loop *loop, ev_io *w, int revents)
{
        connection_t *conn = (connection_t *)w;
        ev_io_stop(loop, w);

        const char *response = CONNECTION_ESTABLISHED;
        ssize_t     len      = strlen(response);

        ssize_t sent = write(conn->client_fd, response, len);
        if (sent < 0) {
                log_error("Failed to send 200 Connection Established: %s",
                          strerror(errno));
                connection_free(&conn);
                return;
        }

        if (sent < len) {
                /* Partial write - for simplicity, treat as error */
                log_error("Partial write of 200 response");
                connection_free(&conn);
                return;
        }

        log_debug("Sent 200 Connection Established to client");

        /* Use target_host from CONNECT request as SNI (they should match) */
        strncpy(conn->sni_hostname, conn->target_host, sizeof(conn->sni_hostname) - 1);
        conn->sni_hostname[sizeof(conn->sni_hostname) - 1] = '\0';

        log_info("Using hostname from CONNECT: %s", conn->sni_hostname);

        /* Skip ClientHello parsing, go directly to server connection */
        connect_to_target(conn);
}

/**
 * Step 3: Read ClientHello from client, extract SNI
 */
static void extract_sni_from_client(struct ev_loop *loop, ev_io *w, int revents)
{
        connection_t *conn = (connection_t *)w;

        /* Read into client_hello buffer */
        ssize_t bytes_read = read(conn->client_fd,
                                  conn->client_hello + conn->client_hello_len,
                                  sizeof(conn->client_hello) - conn->client_hello_len);

        if (bytes_read < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                        return; /* Wait for more data */
                }
                log_error("Failed to read ClientHello: %s", strerror(errno));
                ev_io_stop(loop, w);
                connection_free(&conn);
                return;
        }

        if (bytes_read == 0) {
                log_warn("Client closed connection before sending ClientHello");
                ev_io_stop(loop, w);
                connection_free(&conn);
                return;
        }

        conn->client_hello_len += bytes_read;
        log_debug("Read %zd bytes of ClientHello (total: %zu)",
                  bytes_read,
                  conn->client_hello_len);

        /* Try to parse SNI */
        if (parse_sni_from_client_hello((unsigned char *)conn->client_hello,
                                        conn->client_hello_len,
                                        conn->sni_hostname,
                                        sizeof(conn->sni_hostname)) < 0) {
                /* Check if we have a complete TLS record */
                if (conn->client_hello_len >= 5) {
                        size_t record_len = ((unsigned char)conn->client_hello[3] << 8) |
                                            (unsigned char)conn->client_hello[4];
                        if (conn->client_hello_len >= 5 + record_len) {
                                /* Complete record but no SNI - use target_host from
                                 * CONNECT */
                                log_warn(
                                        "No SNI in ClientHello, using CONNECT target: %s",
                                        conn->target_host);
                                strncpy(conn->sni_hostname,
                                        conn->target_host,
                                        sizeof(conn->sni_hostname) - 1);
                        } else {
                                /* Incomplete record, wait for more */
                                return;
                        }
                } else {
                        /* Need more data */
                        return;
                }
        }

        ev_io_stop(loop, w);
        log_info("Extracted SNI: %s", conn->sni_hostname);

        /* Now connect to the target server */
        connect_to_target(conn);
}

/**
 * Step 4: DNS lookup and connect to target
 */
static void connect_to_target(connection_t *conn)
{
        struct addrinfo hints, *result;

        memset(&hints, 0, sizeof(hints));
        hints.ai_family   = AF_INET;
        hints.ai_socktype = SOCK_STREAM;

        char port_str[6];
        snprintf(port_str, sizeof(port_str), "%d", conn->target_port);

        /* Use SNI hostname for DNS lookup */
        const char *lookup_host = conn->sni_hostname[0] ? conn->sni_hostname :
                                                          conn->target_host;

        int status = getaddrinfo(lookup_host, port_str, &hints, &result);
        if (status != 0) {
                log_warn("DNS lookup failed for %s: %s",
                         lookup_host,
                         gai_strerror(status));
                connection_free(&conn);
                return;
        }

        conn->server_fd = socket(result->ai_family,
                                 result->ai_socktype,
                                 result->ai_protocol);
        if (conn->server_fd < 0) {
                log_error("Failed to create server socket: %s", strerror(errno));
                freeaddrinfo(result);
                connection_free(&conn);
                return;
        }

        if (!set_socket_blocking(conn->server_fd, false)) {
                log_error("Failed to set server socket non-blocking");
                freeaddrinfo(result);
                connection_free(&conn);
                return;
        }

        int ret = connect(conn->server_fd, result->ai_addr, result->ai_addrlen);
        freeaddrinfo(result);

        if (ret < 0 && errno != EINPROGRESS) {
                log_error("Connect to %s:%d failed: %s",
                          lookup_host,
                          conn->target_port,
                          strerror(errno));
                connection_free(&conn);
                return;
        }

        /* Wait for connect to complete */
        ev_io_init(&conn->server_watcher, on_server_connect, conn->server_fd, EV_WRITE);
        conn->server_watcher.data = conn;
        ev_io_start(conn->ctx->loop, &conn->server_watcher);
}

/**
 * Step 5: Handle server connect completion, start TLS handshake
 */
static void on_server_connect(struct ev_loop *loop, ev_io *w, int revents)
{
        connection_t *conn = (connection_t *)w->data;
        ev_io_stop(loop, w);

        /* Check if connect succeeded */
        int       error;
        socklen_t len = sizeof(error);
        if (getsockopt(conn->server_fd, SOL_SOCKET, SO_ERROR, &error, &len) < 0) {
                log_error("getsockopt failed: %s", strerror(errno));
                connection_free(&conn);
                return;
        }

        if (error != 0) {
                log_error("Connect to server failed: %s", strerror(error));
                connection_free(&conn);
                return;
        }

        log_debug("Connected to server %s:%d", conn->sni_hostname, conn->target_port);

        /* Create SSL context for server connection */
        conn->server_ssl_ctx = SSL_CTX_new(TLS_client_method());
        if (!conn->server_ssl_ctx) {
                log_error("Failed to create server SSL context");
                connection_free(&conn);
                return;
        }

        conn->server_ssl = SSL_new(conn->server_ssl_ctx);
        if (!conn->server_ssl) {
                log_error("Failed to create server SSL object");
                connection_free(&conn);
                return;
        }

        SSL_set_fd(conn->server_ssl, conn->server_fd);

        /* Set SNI for server connection */
        if (conn->sni_hostname[0]) {
                SSL_set_tlsext_host_name(conn->server_ssl, conn->sni_hostname);
        }

        /* Start non-blocking TLS handshake with server */
        ev_io_init(&conn->server_watcher,
                   do_server_tls_handshake,
                   conn->server_fd,
                   EV_WRITE);
        conn->server_watcher.data = conn;
        ev_io_start(loop, &conn->server_watcher);
}

/**
 * Step 6: Perform TLS handshake with server (non-blocking)
 */
static void do_server_tls_handshake(struct ev_loop *loop, ev_io *w, int revents)
{
        connection_t *conn = (connection_t *)w->data;
        ev_io_stop(loop, w);

        int ret = SSL_connect(conn->server_ssl);

        if (ret == 1) {
                /* Handshake complete */
                log_info("TLS handshake with server complete");

                /* Extract server certificate info */
                X509 *server_cert = SSL_get_peer_certificate(conn->server_ssl);
                if (!server_cert) {
                        log_error("Server did not provide certificate");
                        connection_free(&conn);
                        return;
                }

                char cn[256];
                if (extract_cn_from_cert(server_cert, cn, sizeof(cn)) == 0) {
                        log_debug("Server cert CN: %s", cn);
                }

                log_info("Generating spoofed certificate...");

                /* Generate spoofed certificate */
                EVP_PKEY *spoofed_key  = NULL;
                X509     *spoofed_cert = generate_spoofed_certificate(server_cert,
                                                                      conn->ctx->ca_cert,
                                                                      conn->ctx->ca_key,
                                                                      &spoofed_key);
                X509_free(server_cert);

                if (!spoofed_cert || !spoofed_key) {
                        log_error("Failed to generate spoofed certificate");
                        if (spoofed_cert) X509_free(spoofed_cert);
                        if (spoofed_key) EVP_PKEY_free(spoofed_key);
                        connection_free(&conn);
                        return;
                }

                /* Store key in connection for cleanup later */
                conn->spoofed_key = spoofed_key;

                /* Create SSL context for client connection */
                conn->client_ssl_ctx = SSL_CTX_new(TLS_server_method());
                if (!conn->client_ssl_ctx) {
                        log_error("Failed to create client SSL context");
                        X509_free(spoofed_cert);
                        connection_free(&conn);
                        return;
                }

                if (SSL_CTX_use_certificate(conn->client_ssl_ctx, spoofed_cert) != 1) {
                        log_error("Failed to set certificate: %s",
                                  ERR_error_string(ERR_get_error(), NULL));
                        X509_free(spoofed_cert);
                        connection_free(&conn);
                        return;
                }

                if (SSL_CTX_use_PrivateKey(conn->client_ssl_ctx, spoofed_key) != 1) {
                        log_error("Failed to set private key: %s",
                                  ERR_error_string(ERR_get_error(), NULL));
                        X509_free(spoofed_cert);
                        connection_free(&conn);
                        return;
                }

                if (SSL_CTX_check_private_key(conn->client_ssl_ctx) != 1) {
                        log_error("Private key does not match certificate: %s",
                                  ERR_error_string(ERR_get_error(), NULL));
                        X509_free(spoofed_cert);
                        connection_free(&conn);
                        return;
                }

                X509_free(spoofed_cert);
                log_info("SSL context configured with spoofed certificate");

                conn->client_ssl = SSL_new(conn->client_ssl_ctx);
                if (!conn->client_ssl) {
                        log_error("Failed to create client SSL object");
                        connection_free(&conn);
                        return;
                }

                /* Now that we don't pre-read the ClientHello, we can just use the socket
                 * directly */
                SSL_set_fd(conn->client_ssl, conn->client_fd);

                log_info("Starting TLS handshake with client");

                /* Start watching for client data and begin handshake */
                ev_io_init(&conn->client_watcher,
                           do_client_tls_handshake,
                           conn->client_fd,
                           EV_READ);
                ev_io_start(loop, &conn->client_watcher);
                return;
        }

        int ssl_error = SSL_get_error(conn->server_ssl, ret);

        if (ssl_error == SSL_ERROR_WANT_READ) {
                ev_io_init(&conn->server_watcher,
                           do_server_tls_handshake,
                           conn->server_fd,
                           EV_READ);
                conn->server_watcher.data = conn;
                ev_io_start(loop, &conn->server_watcher);
        } else if (ssl_error == SSL_ERROR_WANT_WRITE) {
                ev_io_init(&conn->server_watcher,
                           do_server_tls_handshake,
                           conn->server_fd,
                           EV_WRITE);
                conn->server_watcher.data = conn;
                ev_io_start(loop, &conn->server_watcher);
        } else {
                log_error("Server TLS handshake failed: %s",
                          ERR_error_string(ERR_get_error(), NULL));
                connection_free(&conn);
        }
}

/**
 * Step 7: Perform TLS handshake with client (non-blocking)
 */
static void do_client_tls_handshake(struct ev_loop *loop, ev_io *w, int revents)
{
        connection_t *conn = (connection_t *)w;
        ev_io_stop(loop, w);

        log_debug("do_client_tls_handshake called");

        int ret = SSL_accept(conn->client_ssl);
        log_debug("SSL_accept returned %d", ret);

        if (ret == 1) {
                /* Handshake complete */
                log_info("TLS handshake with client complete - starting tunneling");
                log_info("HTTPS MITM connection established to %s:%d",
                         conn->sni_hostname,
                         conn->target_port);

                start_tunneling(conn);
                return;
        }

        int ssl_error = SSL_get_error(conn->client_ssl, ret);

        if (ssl_error == SSL_ERROR_WANT_READ) {
                ev_io_init(&conn->client_watcher,
                           do_client_tls_handshake,
                           conn->client_fd,
                           EV_READ);
                ev_io_start(loop, &conn->client_watcher);
        } else if (ssl_error == SSL_ERROR_WANT_WRITE) {
                ev_io_init(&conn->client_watcher,
                           do_client_tls_handshake,
                           conn->client_fd,
                           EV_WRITE);
                ev_io_start(loop, &conn->client_watcher);
        } else {
                /* Client disconnected or other TLS error - this is common when
                 * browsers speculatively open connections then close them */
                unsigned long err = ERR_get_error();
                log_warn("Client TLS handshake failed: %s", ERR_error_string(err, NULL));
                connection_free(&conn);
        }
}
