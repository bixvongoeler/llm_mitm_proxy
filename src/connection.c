#include "connection.h"

connection_t *connection_new(proxy_context_t *ctx, int client_fd)
{
        assert(ctx);

        connection_t *conn = calloc(1, sizeof(connection_t));
        if (!conn) {
                log_error("Failed to allocate memory for connection");
                return NULL;
        }
        conn->ctx = ctx;

        conn->client_fd = client_fd;
        conn->server_fd = -1;

        char *buffer              = malloc(BUFFER_SIZE * sizeof(char));
        conn->to_server.start     = buffer;
        conn->to_server.read_ptr  = buffer;
        conn->to_server.write_ptr = buffer;

        buffer                    = malloc(BUFFER_SIZE * sizeof(char));
        conn->to_client.start     = buffer;
        conn->to_client.read_ptr  = buffer;
        conn->to_client.write_ptr = buffer;

        return conn;
}

void connection_free(connection_t **conn)
{
        assert(conn && *conn);
        connection_t *c = *conn;

        ev_io_stop(c->ctx->loop, &c->client_watcher);
        ev_io_stop(c->ctx->loop, &c->server_watcher);

        /* Clean up SSL resources */
        if (c->client_ssl) {
                SSL_shutdown(c->client_ssl);
                SSL_free(c->client_ssl);
        }
        if (c->client_ssl_ctx) {
                SSL_CTX_free(c->client_ssl_ctx);
        }
        if (c->server_ssl) {
                SSL_shutdown(c->server_ssl);
                SSL_free(c->server_ssl);
        }
        if (c->server_ssl_ctx) {
                SSL_CTX_free(c->server_ssl_ctx);
        }
        if (c->spoofed_key) {
                EVP_PKEY_free(c->spoofed_key);
        }

        if (c->to_server.start) free(c->to_server.start);
        if (c->to_client.start) free(c->to_client.start);

        if (c->client_fd >= 0) {
                shutdown(c->client_fd, SHUT_RDWR);
                close(c->client_fd);
        }
        if (c->server_fd >= 0) {
                shutdown(c->server_fd, SHUT_RDWR);
                close(c->server_fd);
        }

        free(c);
        *conn = NULL;
}

/**
 * @return:
 *      -2 if buffer is full,
 *      -1 if read error,
 *      else num bytes read.
 */
ssize_t read_into_buffer(int fd, conn_buffer_t *buf)
{
        /* Make sure we have enough space */
        buf->write_ptr[0] = '\0';
        size_t rem        = BUFFER_SIZE - 1 - (buf->write_ptr - buf->start);
        if (rem <= 0) {
                return -2;
        }
        /* Read the Data */
        ssize_t bytes_read = read(fd, buf->write_ptr, rem);
        if (bytes_read < 0) {
                return bytes_read;
        }
        /* Update the Trackers */
        buf->write_ptr    += bytes_read;
        buf->write_ptr[0]  = '\0';
        return bytes_read;
}
