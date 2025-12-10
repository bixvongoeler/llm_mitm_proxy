#ifndef CONNECTION_H
#define CONNECTION_H

#include "utils.h"
#include "proxy.h"

connection_t *connection_new(proxy_context_t *ctx, int client_fd);
void          connection_free(connection_t **conn);
ssize_t       read_into_buffer(int fd, conn_buffer_t *buf);

#endif    // CONNECTION_H
