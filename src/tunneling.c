/**
 * tunneling.c
 */

#include "proxy.h"
#include "connection.h"
#include "llm_client.h"

#define TUNNEL_BUF_SIZE      16384
#define X_PROXY_HEADER       "X-Proxy:CS112\r\n"
#define LLM_BUFFER_INIT_SIZE (256 * 1024)      /* 256KB initial */
#define LLM_BUFFER_MAX_SIZE  (4 * 1024 * 1024) /* 4MB max */

typedef enum { LINK_CLIENT_TO_SERVER, LINK_SERVER_TO_CLIENT } link_direction_t;

typedef struct tunnel_link {
        ev_io watcher;

        connection_t    *conn;
        link_direction_t direction;

        SSL *read_ssl;
        SSL *write_ssl;
        int  read_fd;
        int  write_fd;

        char   buffer[TUNNEL_BUF_SIZE];
        size_t buf_len;
        size_t buf_sent;

        bool header_injected;
        bool read_closed;

        /* LLM buffering for SIS URLs */
        bool   llm_buffer_enabled; /* True if buffering for LLM processing */
        char  *llm_buffer;         /* Accumulated response data */
        size_t llm_buffer_len;     /* Current data length */
        size_t llm_buffer_cap;     /* Buffer capacity */
        bool   llm_processed;      /* True after LLM processing complete */

        /* HTTP response parsing for detecting completion */
        bool   llm_headers_complete; /* True when we've seen \r\n\r\n */
        size_t llm_content_length;   /* Content-Length value, or 0 if unknown */
        size_t llm_body_start;       /* Offset where body starts in llm_buffer */
} tunnel_link_t;

/* Forward declarations */
static void link_wait_to_read(tunnel_link_t *link);
static void link_wait_to_write(tunnel_link_t *link);
static void handle_link_readability(struct ev_loop *loop, ev_io *w, int revents);
static void handle_link_writability(struct ev_loop *loop, ev_io *w, int revents);
static void cleanup_tunnel(connection_t *conn, tunnel_link_t *c2s, tunnel_link_t *s2c);
static int  inject_proxy_header(tunnel_link_t *link);
static int  llm_buffer_append(tunnel_link_t *link, const char *data, size_t len);
static void llm_process_and_forward(tunnel_link_t *link);
static bool llm_response_complete(tunnel_link_t *link);

/**
 * Entry point for tunneling after TLS handshakes complete.
 * Sets up bidirectional forwarding between client and server.
 */
void start_tunneling(connection_t *conn)
{
        log_info("Starting bidirectional tunneling for %s:%d",
                 conn->sni_hostname,
                 conn->target_port);

        /* Allocate link structures */
        tunnel_link_t *c2s = calloc(1, sizeof(tunnel_link_t));
        tunnel_link_t *s2c = calloc(1, sizeof(tunnel_link_t));

        if (!c2s || !s2c) {
                log_error("Failed to allocate tunnel links");
                if (c2s) free(c2s);
                if (s2c) free(s2c);
                connection_free(&conn);
                return;
        }

        /* Initialize client-to-server link */
        c2s->conn            = conn;
        c2s->direction       = LINK_CLIENT_TO_SERVER;
        c2s->read_ssl        = conn->client_ssl;
        c2s->write_ssl       = conn->server_ssl;
        c2s->read_fd         = conn->client_fd;
        c2s->write_fd        = conn->server_fd;
        c2s->buf_len         = 0;
        c2s->buf_sent        = 0;
        c2s->header_injected = false;
        c2s->read_closed     = false;

        /* Initialize server-to-client link */
        s2c->conn            = conn;
        s2c->direction       = LINK_SERVER_TO_CLIENT;
        s2c->read_ssl        = conn->server_ssl;
        s2c->write_ssl       = conn->client_ssl;
        s2c->read_fd         = conn->server_fd;
        s2c->write_fd        = conn->client_fd;
        s2c->buf_len         = 0;
        s2c->buf_sent        = 0;
        s2c->header_injected = false;
        s2c->read_closed     = false;

        /* Check if this connection should be buffered for LLM processing */
        s2c->llm_buffer_enabled   = llm_should_process(conn->sni_hostname);
        s2c->llm_buffer           = NULL;
        s2c->llm_buffer_len       = 0;
        s2c->llm_buffer_cap       = 0;
        s2c->llm_processed        = false;
        s2c->llm_headers_complete = false;
        s2c->llm_content_length   = 0;
        s2c->llm_body_start       = 0;

        if (s2c->llm_buffer_enabled) {
                log_info("LLM buffering enabled for %s", conn->sni_hostname);
                s2c->llm_buffer = malloc(LLM_BUFFER_INIT_SIZE);
                if (s2c->llm_buffer) {
                        s2c->llm_buffer_cap = LLM_BUFFER_INIT_SIZE;
                } else {
                        log_warn("Failed to allocate LLM buffer, disabling");
                        s2c->llm_buffer_enabled = false;
                }
        }

        /* Store link pointers in connection's buffer pointers for access in callbacks */
        conn->to_client.start = (char *)s2c;
        conn->to_server.start = (char *)c2s;

        /* Start reading from both sides */
        link_wait_to_read(c2s);
        link_wait_to_read(s2c);
}

/**
 * Wait for data to be readable from source
 */
static void link_wait_to_read(tunnel_link_t *link)
{
        ev_io_init(&link->watcher, handle_link_readability, link->read_fd, EV_READ);
        ev_io_start(link->conn->ctx->loop, &link->watcher);
}

/**
 * Wait for destination to be writable
 */
static void link_wait_to_write(tunnel_link_t *link)
{
        ev_io_init(&link->watcher, handle_link_writability, link->write_fd, EV_WRITE);
        ev_io_start(link->conn->ctx->loop, &link->watcher);
}

/**
 * Inject X-Proxy: CS112 header into HTTP response.
 * Looks for the end of the first line (HTTP status) and inserts the header.
 * Returns 0 on success, -1 if not a valid HTTP response or already injected.
 */
static int inject_proxy_header(tunnel_link_t *link)
{
        if (link->header_injected || link->buf_len < 12) {
                return -1;
        }

        /* Check if this looks like an HTTP response */
        if (memcmp(link->buffer, "HTTP/", 5) != 0) {
                link->header_injected = true; /* Not HTTP, don't try again */
                return -1;
        }

        /* Find the end of the status line (\r\n) */
        char *crlf = memmem(link->buffer, link->buf_len, "\r\n", 2);
        if (!crlf) {
                return -1; /* Status line not complete yet */
        }

        /* Position after first \r\n is where we insert */
        size_t insert_pos = (crlf - link->buffer) + 2;
        size_t header_len = strlen(X_PROXY_HEADER);

        /* Check if we have room */
        if (link->buf_len + header_len > TUNNEL_BUF_SIZE) {
                log_warn("Buffer too full to inject header");
                link->header_injected = true;
                return -1;
        }

        /* Shift data after insert point */
        memmove(link->buffer + insert_pos + header_len,
                link->buffer + insert_pos,
                link->buf_len - insert_pos);

        /* Insert the header */
        memcpy(link->buffer + insert_pos, X_PROXY_HEADER, header_len);
        link->buf_len += header_len;

        link->header_injected = true;
        log_debug("Injected X-Proxy header into response");

        return 0;
}

/**
 * Handle readable event - read from source SSL into buffer
 */
static void handle_link_readability(struct ev_loop *loop, ev_io *w, int revents)
{
        tunnel_link_t *link = (tunnel_link_t *)w;
        connection_t  *conn = link->conn;

        ev_io_stop(loop, w);

        /* Read into buffer */
        int bytes = SSL_read(link->read_ssl,
                             link->buffer + link->buf_len,
                             TUNNEL_BUF_SIZE - link->buf_len);

        if (bytes > 0) {
                link->buf_len += bytes;
                log_debug("[%s] Read %d bytes (buf now %zu)",
                          link->direction == LINK_CLIENT_TO_SERVER ? "C->S" : "S->C",
                          bytes,
                          link->buf_len);

                /* If LLM buffering is enabled for S->C, accumulate instead of forwarding
                 */
                if (link->llm_buffer_enabled && !link->llm_processed) {
                        if (llm_buffer_append(link, link->buffer, link->buf_len) < 0) {
                                /* Buffer full or error, disable and forward normally */
                                log_warn(
                                        "LLM buffer overflow, forwarding without processing");
                                link->llm_buffer_enabled = false;
                        } else {
                                /* Check if response is complete */
                                if (llm_response_complete(link)) {
                                        log_info(
                                                "HTTP response complete (%zu bytes), processing with LLM",
                                                link->llm_buffer_len);
                                        llm_process_and_forward(link);
                                        link_wait_to_write(link);
                                        return;
                                }
                                /* Data accumulated, reset buffer and read more */
                                link->buf_len  = 0;
                                link->buf_sent = 0;
                                link_wait_to_read(link);
                                return;
                        }
                }

                /* If this is server-to-client, try to inject header */
                if (link->direction == LINK_SERVER_TO_CLIENT && !link->header_injected) {
                        inject_proxy_header(link);
                }

                /* Have data to write - wait for writability */
                link_wait_to_write(link);
                return;
        }

        int ssl_error = SSL_get_error(link->read_ssl, bytes);

        if (ssl_error == SSL_ERROR_WANT_READ) {
                /* Need more data from source, keep waiting */
                link_wait_to_read(link);
                return;
        }

        if (ssl_error == SSL_ERROR_WANT_WRITE) {
                /* SSL needs to write (renegotiation?) - wait for write on read_fd */
                ev_io_init(&link->watcher,
                           handle_link_readability,
                           link->read_fd,
                           EV_WRITE);
                ev_io_start(loop, &link->watcher);
                return;
        }

        if (ssl_error == SSL_ERROR_ZERO_RETURN) {
                /* Clean SSL shutdown from peer */
                log_info("[%s] SSL connection closed by peer",
                         link->direction == LINK_CLIENT_TO_SERVER ? "C->S" : "S->C");
        } else if (ssl_error == SSL_ERROR_SYSCALL) {
                /* Check for EAGAIN - normal for non-blocking I/O, just retry */
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                        link_wait_to_read(link);
                        return;
                }
                /* Connection reset or EOF */
                if (errno == 0 || errno == ECONNRESET || errno == EPIPE) {
                        log_debug("[%s] Connection closed (EOF/reset)",
                                  link->direction == LINK_CLIENT_TO_SERVER ? "C->S" :
                                                                             "S->C");
                } else {
                        log_warn("[%s] SSL syscall error: %s",
                                 link->direction == LINK_CLIENT_TO_SERVER ? "C->S" :
                                                                            "S->C",
                                 strerror(errno));
                }
        } else if (ssl_error == SSL_ERROR_SSL) {
                /* OpenSSL protocol error - get the actual error */
                unsigned long err = ERR_get_error();
                log_debug("[%s] SSL protocol error: %s",
                          link->direction == LINK_CLIENT_TO_SERVER ? "C->S" : "S->C",
                          ERR_error_string(err, NULL));
        } else {
                log_warn("[%s] SSL_read error: %d, errno: %s",
                         link->direction == LINK_CLIENT_TO_SERVER ? "C->S" : "S->C",
                         ssl_error,
                         strerror(errno));
        }

        /* Mark this direction as closed */
        link->read_closed = true;

        /* If LLM buffering is enabled and we have data, process it now */
        if (link->llm_buffer_enabled && link->llm_buffer_len > 0 &&
            !link->llm_processed) {
                log_info("Processing %zu bytes with LLM server", link->llm_buffer_len);
                llm_process_and_forward(link);
                /* llm_process_and_forward sets up the buffer for writing */
                link_wait_to_write(link);
                return;
        }

        /* If we still have buffered data, try to write it */
        if (link->buf_len > link->buf_sent) {
                link_wait_to_write(link);
                return;
        }

        /* Get the other link */
        tunnel_link_t *c2s   = (tunnel_link_t *)conn->to_server.start;
        tunnel_link_t *s2c   = (tunnel_link_t *)conn->to_client.start;
        tunnel_link_t *other = (link == c2s) ? s2c : c2s;

        /* If both directions are closed, clean up */
        if (other->read_closed) {
                cleanup_tunnel(conn, c2s, s2c);
        }
}

/**
 * Handle writable event - write from buffer to destination SSL
 */
static void handle_link_writability(struct ev_loop *loop, ev_io *w, int revents)
{
        tunnel_link_t *link = (tunnel_link_t *)w;
        connection_t  *conn = link->conn;

        ev_io_stop(loop, w);

        size_t to_write = link->buf_len - link->buf_sent;
        if (to_write == 0) {
                /* Nothing to write, go back to reading */
                if (!link->read_closed) {
                        link_wait_to_read(link);
                }
                return;
        }

        int bytes = SSL_write(link->write_ssl, link->buffer + link->buf_sent, to_write);

        if (bytes > 0) {
                link->buf_sent += bytes;
                log_debug("[%s] Wrote %d bytes (%zu remaining)",
                          link->direction == LINK_CLIENT_TO_SERVER ? "C->S" : "S->C",
                          bytes,
                          link->buf_len - link->buf_sent);

                if (link->buf_sent >= link->buf_len) {
                        /* All data written, reset buffer */
                        link->buf_len  = 0;
                        link->buf_sent = 0;

                        /* If we're sending from LLM buffer and there's more, copy next
                         * chunk */
                        if (link->llm_processed && link->llm_buffer) {
                                /* llm_buffer_cap is reused as "bytes sent" from
                                 * llm_buffer */
                                size_t llm_sent      = link->llm_buffer_cap;
                                size_t llm_remaining = link->llm_buffer_len - llm_sent;

                                if (llm_remaining > 0) {
                                        size_t chunk = (llm_remaining < TUNNEL_BUF_SIZE) ?
                                                               llm_remaining :
                                                               TUNNEL_BUF_SIZE;
                                        memcpy(link->buffer,
                                               link->llm_buffer + llm_sent,
                                               chunk);
                                        link->buf_len        = chunk;
                                        link->buf_sent       = 0;
                                        link->llm_buffer_cap = llm_sent + chunk;
                                        link_wait_to_write(link);
                                        return;
                                }
                                /* All LLM buffer sent, free it */
                                free(link->llm_buffer);
                                link->llm_buffer     = NULL;
                                link->llm_buffer_len = 0;
                        }

                        if (!link->read_closed) {
                                link_wait_to_read(link);
                        } else {
                                /* Source closed and buffer empty - check if done */
                                tunnel_link_t *c2s =
                                        (tunnel_link_t *)conn->to_server.start;
                                tunnel_link_t *s2c =
                                        (tunnel_link_t *)conn->to_client.start;
                                tunnel_link_t *other = (link == c2s) ? s2c : c2s;

                                if (other->read_closed &&
                                    other->buf_len == other->buf_sent) {
                                        cleanup_tunnel(conn, c2s, s2c);
                                }
                        }
                } else {
                        /* More data to write */
                        link_wait_to_write(link);
                }
                return;
        }

        int ssl_error = SSL_get_error(link->write_ssl, bytes);

        if (ssl_error == SSL_ERROR_WANT_WRITE) {
                /* SSL buffers full, wait for writability */
                link_wait_to_write(link);
                return;
        }

        if (ssl_error == SSL_ERROR_WANT_READ) {
                /* SSL needs to read? - wait for read on write_fd */
                ev_io_init(&link->watcher,
                           handle_link_writability,
                           link->write_fd,
                           EV_READ);
                ev_io_start(loop, &link->watcher);
                return;
        }

        /* Check for EAGAIN on syscall error - not error for non block call */
        if (ssl_error == SSL_ERROR_SYSCALL && (errno == EAGAIN || errno == EWOULDBLOCK)) {
                link_wait_to_write(link);
                return;
        }

        /* Write error - connection closed by peer */
        if (ssl_error == SSL_ERROR_SYSCALL &&
            (errno == EPIPE || errno == ECONNRESET || errno == 0)) {
                log_debug("[%s] Connection closed during write",
                          link->direction == LINK_CLIENT_TO_SERVER ? "C->S" : "S->C");
        } else {
                log_warn("[%s] SSL_write error: %d",
                         link->direction == LINK_CLIENT_TO_SERVER ? "C->S" : "S->C",
                         ssl_error);
        }

        tunnel_link_t *c2s = (tunnel_link_t *)conn->to_server.start;
        tunnel_link_t *s2c = (tunnel_link_t *)conn->to_client.start;
        cleanup_tunnel(conn, c2s, s2c);
}

/**
 * Clean up tunnel resources and connection
 */
static void cleanup_tunnel(connection_t *conn, tunnel_link_t *c2s, tunnel_link_t *s2c)
{
        log_info("Closing tunnel for %s:%d", conn->sni_hostname, conn->target_port);

        /* Stop any active watchers */
        ev_io_stop(conn->ctx->loop, &c2s->watcher);
        ev_io_stop(conn->ctx->loop, &s2c->watcher);

        /* Free LLM buffer if allocated */
        if (s2c->llm_buffer) {
                free(s2c->llm_buffer);
                s2c->llm_buffer = NULL;
        }

        /* Clear the stored pointers before freeing */
        conn->to_client.start = NULL;
        conn->to_server.start = NULL;

        free(c2s);
        free(s2c);

        connection_free(&conn);
}

/**
 * Append data to LLM buffer, growing if necessary.
 * Returns 0 on success, -1 if buffer would exceed max size.
 */
static int llm_buffer_append(tunnel_link_t *link, const char *data, size_t len)
{
        if (!link->llm_buffer || len == 0) return -1;

        /* Check if we need to grow the buffer */
        while (link->llm_buffer_len + len > link->llm_buffer_cap) {
                size_t new_cap = link->llm_buffer_cap * 2;
                if (new_cap > LLM_BUFFER_MAX_SIZE) {
                        log_warn("LLM buffer would exceed max size (%zu bytes)",
                                 (size_t)LLM_BUFFER_MAX_SIZE);
                        return -1;
                }

                char *new_buf = realloc(link->llm_buffer, new_cap);
                if (!new_buf) {
                        log_error("Failed to grow LLM buffer to %zu bytes", new_cap);
                        return -1;
                }

                link->llm_buffer     = new_buf;
                link->llm_buffer_cap = new_cap;
                log_debug("Grew LLM buffer to %zu bytes", new_cap);
        }

        /* Append data */
        memcpy(link->llm_buffer + link->llm_buffer_len, data, len);
        link->llm_buffer_len += len;

        return 0;
}

/**
 * Process buffered response with LLM server and prepare for forwarding.
 * Modifies link->buffer/buf_len/buf_sent for the write loop to send.
 */
static void llm_process_and_forward(tunnel_link_t *link)
{
        link->llm_processed = true;

        /* Build URL for LLM server */
        char url[512];
        snprintf(url,
                 sizeof(url),
                 "%s:%d%s",
                 link->conn->sni_hostname,
                 link->conn->target_port,
                 link->conn->target_host);

        /* Send to LLM server */
        size_t modified_len = 0;
        char  *modified     = llm_process_response(url,
                                                   link->llm_buffer,
                                                   link->llm_buffer_len,
                                                   &modified_len);

        if (modified && modified_len > 0) {
                /* Use modified response */
                log_info("Using LLM-modified response (%zu -> %zu bytes)",
                         link->llm_buffer_len,
                         modified_len);

                /* Replace llm_buffer with modified content */
                free(link->llm_buffer);
                link->llm_buffer     = modified;
                link->llm_buffer_len = modified_len;
                link->llm_buffer_cap = modified_len;
        } else {
                log_debug("LLM server returned no modification, using original");
        }

        /* Set up for forwarding: copy first chunk to buffer */
        size_t chunk = (link->llm_buffer_len < TUNNEL_BUF_SIZE) ? link->llm_buffer_len :
                                                                  TUNNEL_BUF_SIZE;

        memcpy(link->buffer, link->llm_buffer, chunk);
        link->buf_len  = chunk;
        link->buf_sent = 0;

        /* If there's more data, we'll handle it in the write callback */
        /* Mark how much of llm_buffer we've copied */
        link->llm_buffer_cap = chunk; /* Reuse cap as "sent" pointer for llm_buffer */
}

/**
 * Check if the buffered HTTP response is complete.
 * Parses Content-Length header and checks if we've received the full body.
 * Returns true when complete, false if more data needed.
 */
static bool llm_response_complete(tunnel_link_t *link)
{
        if (!link->llm_buffer || link->llm_buffer_len == 0) {
                return false;
        }

        /* First, find headers end if we haven't yet */
        if (!link->llm_headers_complete) {
                char *headers_end =
                        memmem(link->llm_buffer, link->llm_buffer_len, "\r\n\r\n", 4);
                if (!headers_end) {
                        /* Headers not complete yet */
                        return false;
                }

                link->llm_headers_complete = true;
                link->llm_body_start       = (headers_end - link->llm_buffer) + 4;

                /* Parse Content-Length header */
                /* Search case-insensitively for Content-Length */
                char *cl_start = NULL;
                for (char *p = link->llm_buffer; p < headers_end - 14; p++) {
                        if ((p[0] == 'C' || p[0] == 'c') &&
                            (p[1] == 'o' || p[1] == 'O') &&
                            (p[2] == 'n' || p[2] == 'N') &&
                            (p[3] == 't' || p[3] == 'T') &&
                            (p[4] == 'e' || p[4] == 'E') &&
                            (p[5] == 'n' || p[5] == 'N') &&
                            (p[6] == 't' || p[6] == 'T') && p[7] == '-' &&
                            (p[8] == 'L' || p[8] == 'l') &&
                            (p[9] == 'e' || p[9] == 'E') &&
                            (p[10] == 'n' || p[10] == 'N') &&
                            (p[11] == 'g' || p[11] == 'G') &&
                            (p[12] == 't' || p[12] == 'T') &&
                            (p[13] == 'h' || p[13] == 'H') && p[14] == ':') {
                                cl_start = p + 15;
                                break;
                        }
                }

                if (cl_start) {
                        /* Skip whitespace */
                        while (*cl_start == ' ' || *cl_start == '\t')
                                cl_start++;
                        link->llm_content_length = (size_t)atol(cl_start);
                        log_debug("Content-Length: %zu, body starts at %zu",
                                  link->llm_content_length,
                                  link->llm_body_start);
                } else {
                        /* No Content-Length - check for chunked encoding or assume
                         * complete */
                        /* For simplicity, if no Content-Length, we'll wait for EOF */
                        /* This handles chunked encoding implicitly (wait for close) */
                        log_debug("No Content-Length found, will wait for EOF");
                        link->llm_content_length = 0;
                        return false;
                }
        }

        /* Check if we have the full body */
        if (link->llm_content_length > 0) {
                size_t body_received = link->llm_buffer_len - link->llm_body_start;
                if (body_received >= link->llm_content_length) {
                        log_debug("Body complete: %zu >= %zu",
                                  body_received,
                                  link->llm_content_length);
                        return true;
                }
        }

        return false;
}
