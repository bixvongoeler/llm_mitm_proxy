#include "proxy.h"
#include "connection.h"

/**
 * @return
 *      -1 on error (can close connection),
 *      0 on success,
 *      1 if we need to wait for more data
 */
int read_connect_request(connection_t *conn)
{
        conn_buffer_t *buf        = &conn->to_server;
        size_t         bytes_read = read_into_buffer(conn->client_fd, buf);

        if (bytes_read < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) return 1;
                log_error("reading from client request failed: received %td bytes",
                          buf->write_ptr - buf->start);
                return -1;
        }
        if (bytes_read == 0) {
                log_warn(
                        "Client closed connection before sending full request: received %td bytes",
                        buf->write_ptr - buf->start);
                return -1;
        }

        /* Try to parse */
        const char       *method, *path;
        size_t            method_len, path_len;
        int               minor_version;
        struct phr_header headers[100];
        size_t            num_headers = 100;

        size_t buflen = buf->write_ptr - buf->start;

        int pret = phr_parse_request(buf->start,
                                     buflen,
                                     &method,
                                     &method_len,
                                     &path,
                                     &path_len,
                                     &minor_version,
                                     headers,
                                     &num_headers,
                                     0);

        if (pret == -2) {
                // Incomplete request, need more data
                return 1;
        }
        if (pret == -1) {
                log_error("malformed HTTP request");
                return -1;
        }

        conn->is_connect = (method_len == 7 && memcmp(method, "CONNECT", 7) == 0);

        if (conn->is_connect) {
                if (parse_host_port(path,
                                    path_len,
                                    conn->target_host,
                                    sizeof(conn->target_host),
                                    &conn->target_port,
                                    443) < 0) {
                        log_error("Failed to parse CONNECT target");
                        return -1;
                }
        } else {
                // HTTP GET - find Host header
                for (size_t i = 0; i < num_headers; i++) {
                        if (headers[i].name_len == 4 &&
                            strncasecmp(headers[i].name, "Host", 4) == 0) {
                                if (parse_host_port(headers[i].value,
                                                    headers[i].value_len,
                                                    conn->target_host,
                                                    sizeof(conn->target_host),
                                                    &conn->target_port,
                                                    80) < 0) {
                                        log_error("Failed to parse Host header");
                                        return -1;
                                }
                                break;
                        }
                }
        }

        log_debug("Parsed request: %.*s %.*s",
                  (int)method_len,
                  method,
                  (int)path_len,
                  path);

        return 0;    // Success
}

void handle_readable_client_request(struct ev_loop *loop, ev_io *w, int revents)
{
        connection_t *conn = (connection_t *)w;

        int result = read_connect_request(conn);
        if (result == -1) {
                ev_io_stop(loop, w);
                connection_free(&conn);
                return;
        }
        if (result == 1) {
                /* need More Data, wait for next call */
                return;
        }
        log_info("Target: %s:%d (is_connect=%d)",
                 conn->target_host,
                 conn->target_port,
                 conn->is_connect);

        /* Else, result = 0: Full request parsed */
        ev_io_stop(loop, w);
        /* Transfer based on request type */
        if (conn->is_connect) {
                start_connection_process(conn);
        } else {
                handle_http_get_request(conn);
        }
}

void accept_incoming_connections(struct ev_loop *loop, ev_io *w, int revents)
{
        proxy_context_t *ctx = (proxy_context_t *)w->data;
        while (true) {
                struct sockaddr_in client_addr;
                socklen_t          client_addr_len = sizeof(struct sockaddr_in);

                int client_fd = accept(w->fd,
                                       (struct sockaddr *)&client_addr,
                                       &client_addr_len);
                if (client_fd < 0) {
                        if (errno == EAGAIN || errno == EWOULDBLOCK) {
                                /* Done processing incoming connections */
                                break;
                        } else {
                                /* Unexpected Error while Accepting */
                                log_error("Failed to accept incoming connection: %s",
                                          strerror(errno));
                                break;
                        }
                }
                bool set = set_socket_blocking(client_fd, false);
                if (!set) {
                        log_error("Failed to set socket blocking");
                }

                connection_t *conn = connection_new(ctx, client_fd);
                log_info("Accepted connection from %s:%d",
                         inet_ntoa(client_addr.sin_addr),
                         ntohs(client_addr.sin_port));

                ev_io_init(&conn->client_watcher,
                           handle_readable_client_request,
                           client_fd,
                           EV_READ);
                ev_io_start(loop, &conn->client_watcher);
        }
}
