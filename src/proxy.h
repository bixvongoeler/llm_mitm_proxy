#ifndef PROXY_H
#define PROXY_H

#include "utils.h"

#define BUFFER_SIZE (1024 * 16)

typedef struct proxy_context {
        struct ev_loop *loop;
        ev_io           accept_watcher;
        int             listen_fd;
        X509           *ca_cert;
        EVP_PKEY       *ca_key;
} proxy_context_t;

typedef struct conn_buffer {
        char *start;
        char *read_ptr;
        char *write_ptr;
} conn_buffer_t;

typedef struct connection {
        ev_io client_watcher;
        ev_io server_watcher;

        int client_fd;
        int server_fd;

        conn_buffer_t to_client;
        conn_buffer_t to_server;

        /* Parsed Request Info */
        char target_host[256];
        int  target_port;
        bool is_connect;

        /* SSL/TLS for HTTPS MITM */
        SSL_CTX *client_ssl_ctx;
        SSL     *client_ssl;
        SSL_CTX *server_ssl_ctx;
        SSL     *server_ssl;

        /* Spoofed certificate key */
        EVP_PKEY *spoofed_key;

        /* SNI hostname extracted from ClientHello */
        char sni_hostname[256];

        /* For storing ClientHello to replay after server handshake */
        char   client_hello[16384];
        size_t client_hello_len;

        proxy_context_t *ctx;
} connection_t;

void accept_incoming_connections(struct ev_loop *loop, ev_io *w, int revents);

void handle_http_get_request(connection_t *conn);

/* HTTPS CONNECT handling */
void start_connection_process(connection_t *conn);

/* Bidirectional tunneling */
void start_tunneling(connection_t *conn);

#endif /* PROXY_H */
