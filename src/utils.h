#ifndef UTILS_H
#define UTILS_H

#define _GNU_SOURCE

/* Std C Headers */
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <assert.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>

#include <sys/types.h>
#include <sys/time.h>
#include <sys/stat.h>

/* Networking */
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <fcntl.h>
#include <poll.h>
#include <limits.h>
#include <ev.h>

/* OpenSSL */
#include <openssl/ssl.h>
#include <openssl/err.h>

/* External Libraries */
#define LOGLEVEL 1
// #define NDEBUG
#include "log.h"
#include "picohttpparser.h"

#define PORT_MIN           0
#define PORT_MAX           65535
#define FILENAME_BUFLENGTH 256

bool file_readable(const char *filename);
bool set_socket_blocking(int fd, bool blocking);

/**
 * Parse a "host:port" string into separate host and port.
 * @param str       - input string (not null-terminated, from picohttpparser)
 * @param str_len   - length of input string
 * @param host_out  - buffer to write host into
 * @param host_size - size of host buffer
 * @param port_out  - pointer to store port
 * @param default_port - port to use if none specified
 * @return 0 on success, -1 on error
 */
int parse_host_port(const char *str,
                    size_t      str_len,
                    char       *host_out,
                    size_t      host_size,
                    int        *port_out,
                    int         default_port);

#endif /* UTILS_H */
