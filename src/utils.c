#include "utils.h"

bool file_readable(const char *filename)
{
        if (access(filename, F_OK) == 0) {
                log_debug("Validated file `%s` is readable", filename);
                return true;
        } else {
                log_error("File `%s` does not exist", filename);
                return false;
        }
}

/**
 * @brief Helper to set a socket to non-blocking mode.
 * Essential for our poll-based event loop.
 */
bool set_socket_blocking(int fd, bool blocking)
{
        if (fd < 0) {
                log_error("Invalid file descriptor");
                return false;
        }
        int flags = fcntl(fd, F_GETFL, 0);
        if (flags == -1) {
                log_error("Failed to get file descriptor flags");
                return false;
        }
        flags = blocking ? (flags & ~O_NONBLOCK) : (flags | O_NONBLOCK);
        return (fcntl(fd, F_SETFL, flags) == 0);
}

int parse_host_port(const char *str,
                    size_t      str_len,
                    char       *host_out,
                    size_t      host_size,
                    int        *port_out,
                    int         default_port)
{
        if (str == NULL || host_out == NULL || port_out == NULL) {
                return -1;
        }

        const char *colon = memchr(str, ':', str_len);
        size_t      host_len;

        if (colon) {
                host_len  = colon - str;
                *port_out = atoi(colon + 1);
        } else {
                host_len  = str_len;
                *port_out = default_port;
        }

        /* Check bounds */
        if (host_len >= host_size) {
                return -1;
        }

        memcpy(host_out, str, host_len);
        host_out[host_len] = '\0';

        return 0;
}
