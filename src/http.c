#include "proxy.h"
#include "connection.h"

static void on_forward_to_client(struct ev_loop *loop, ev_io *w, int revents);

/* TODO: Inject Header for http path */

/**
 * Resolve hostname and initiate non-blocking connect.
 * @return 0 on success (connect in progress), -1 on error
 */
static int resolve_and_connect(connection_t *conn)
{
        struct addrinfo hints, *result;

        memset(&hints, 0, sizeof(hints));
        hints.ai_family   = AF_INET;
        hints.ai_socktype = SOCK_STREAM;

        char port_str[6];
        snprintf(port_str, sizeof(port_str), "%d", conn->target_port);

        int status = getaddrinfo(conn->target_host, port_str, &hints, &result);
        if (status != 0) {
                log_error("DNS lookup failed for %s: %s",
                          conn->target_host,
                          gai_strerror(status));
                return -1;
        }

        /* Create socket */
        conn->server_fd = socket(result->ai_family,
                                 result->ai_socktype,
                                 result->ai_protocol);
        if (conn->server_fd < 0) {
                log_error("Failed to create server socket: %s", strerror(errno));
                freeaddrinfo(result);
                return -1;
        }

        /* Set non-blocking */
        if (!set_socket_blocking(conn->server_fd, false)) {
                log_error("Failed to set server socket non-blocking");
                close(conn->server_fd);
                conn->server_fd = -1;
                freeaddrinfo(result);
                return -1;
        }

        /* Initiate non-blocking connect */
        int ret = connect(conn->server_fd, result->ai_addr, result->ai_addrlen);
        freeaddrinfo(result); /* Done with address info */

        if (ret < 0 && errno != EINPROGRESS) {
                log_error("Connect failed: %s", strerror(errno));
                close(conn->server_fd);
                conn->server_fd = -1;
                return -1;
        }

        /* ret == 0 means connected immediately (rare, usually local)
         * errno == EINPROGRESS means connect is in progress (normal) */
        return 0;
}

static void on_server_response(struct ev_loop *loop, ev_io *w, int revents)
{
        connection_t  *conn = (connection_t *)w->data;
        conn_buffer_t *buf  = &conn->to_client;

        /* Read from server */
        ssize_t bytes_read = read_into_buffer(conn->server_fd, buf);

        if (bytes_read < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                        return; /* Wait for more data */
                }
                log_error("Read from server failed: %s", strerror(errno));
                ev_io_stop(loop, w);
                connection_free(&conn);
                return;
        }

        if (bytes_read == 0) {
                /* Server closed connection - we're done */
                log_debug("Server closed connection");
                ev_io_stop(loop, w);
                connection_free(&conn);
                return;
        }

        /* Got data - now forward to client */
        log_debug("Read %zd bytes from server", bytes_read);
        ev_io_stop(loop, w);
        ev_io_init(&conn->client_watcher, on_forward_to_client, conn->client_fd, EV_WRITE);
        ev_io_start(loop, &conn->client_watcher);
}

static void on_forward_to_client(struct ev_loop *loop, ev_io *w, int revents)
{
        connection_t  *conn = (connection_t *)w; /* client_watcher is first */
        conn_buffer_t *buf  = &conn->to_client;

        size_t  to_send = buf->write_ptr - buf->read_ptr;
        ssize_t sent    = write(conn->client_fd, buf->read_ptr, to_send);

        if (sent < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                        return;
                }
                log_error("Write to client failed: %s", strerror(errno));
                ev_io_stop(loop, w);
                connection_free(&conn);
                return;
        }

        buf->read_ptr += sent;

        if (buf->read_ptr < buf->write_ptr) {
                return; /* Partial write */
        }

        /* Done forwarding this chunk - read more from server */
        log_debug("Forwarded %zd bytes to client", sent);
        buf->read_ptr = buf->write_ptr = buf->start; /* Reset buffer */

        ev_io_stop(loop, w);
        ev_io_init(&conn->server_watcher, on_server_response, conn->server_fd, EV_READ);
        conn->server_watcher.data = conn;
        ev_io_start(loop, &conn->server_watcher);
}

static void on_forward_request(struct ev_loop *loop, ev_io *w, int revents)
{
        connection_t  *conn = (connection_t *)w->data;
        conn_buffer_t *buf  = &conn->to_server;

        size_t  to_send = buf->write_ptr - buf->read_ptr;
        ssize_t sent    = write(conn->server_fd, buf->read_ptr, to_send);

        if (sent < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                        return; /* Watcher will fire again */
                }
                log_error("Write to server failed: %s", strerror(errno));
                ev_io_stop(loop, w);
                connection_free(&conn);
                return;
        }

        buf->read_ptr += sent;

        if (buf->read_ptr < buf->write_ptr) {
                return; /* Partial write, watcher stays active */
        }

        /* Request fully sent - now wait for response */
        log_debug("Request forwarded, waiting for response");
        ev_io_stop(loop, w);
        ev_io_init(w, on_server_response, conn->server_fd, EV_READ);
        w->data = conn;
        ev_io_start(loop, w);
}

static void on_server_connect(struct ev_loop *loop, ev_io *w, int revents)
{
        connection_t *conn = (connection_t *)w->data;
        ev_io_stop(loop, w);

        /* Check if connect actually succeeded */
        int       error;
        socklen_t len = sizeof(error);
        if (getsockopt(conn->server_fd, SOL_SOCKET, SO_ERROR, &error, &len) < 0) {
                log_error("getsockopt failed: %s", strerror(errno));
                connection_free(&conn);
                return;
        }

        if (error != 0) {
                log_error("Connect to %s:%d failed: %s",
                          conn->target_host,
                          conn->target_port,
                          strerror(error));
                connection_free(&conn);
                return;
        }

        /* Connection succeeded! Now forward the request */
        log_debug("Connected to %s:%d", conn->target_host, conn->target_port);

        ev_io_stop(loop, w);
        ev_io_init(w, on_forward_request, conn->server_fd, EV_WRITE);
        w->data = conn;
        ev_io_start(loop, w);
}

void handle_http_get_request(connection_t *conn)
{
        log_debug("Handling HTTP GET to %s:%d", conn->target_host, conn->target_port);

        if (resolve_and_connect(conn) < 0) {
                connection_free(&conn);
                return;
        }

        /* Connect is in progress - watch for writeability to know when it completes */
        ev_io_init(&conn->server_watcher, on_server_connect, conn->server_fd, EV_WRITE);
        conn->server_watcher.data = conn; /* server_watcher isn't first, so use data ptr*/
        ev_io_start(conn->ctx->loop, &conn->server_watcher);
}
