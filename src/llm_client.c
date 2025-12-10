/**
 * llm_client.c - Client for Python LLM injection server
 *
 * Uses blocking Unix domain socket for simplicity.
 * Local IPC is fast enough that blocking is acceptable for demo.
 */

#include "llm_client.h"
#include "utils.h"

#include <sys/un.h>

/* Timeout for LLM server connection (seconds) */
#define LLM_CONNECT_TIMEOUT 2
#define LLM_READ_TIMEOUT    30

/**
 * Set socket timeouts for read/write operations.
 */
static int set_socket_timeout(int fd, int seconds)
{
        struct timeval tv;
        tv.tv_sec  = seconds;
        tv.tv_usec = 0;

        if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
                return -1;
        }
        if (setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv)) < 0) {
                return -1;
        }
        return 0;
}

/**
 * Write exactly n bytes to socket.
 * Returns 0 on success, -1 on error.
 */
static int write_all(int fd, const void *buf, size_t n)
{
        const char *ptr   = buf;
        size_t      total = 0;

        while (total < n) {
                ssize_t written = write(fd, ptr + total, n - total);
                if (written < 0) {
                        if (errno == EINTR) continue;
                        return -1;
                }
                if (written == 0) return -1;
                total += written;
        }
        return 0;
}

/**
 * Read exactly n bytes from socket.
 * Returns 0 on success, -1 on error.
 */
static int read_all(int fd, void *buf, size_t n)
{
        char  *ptr   = buf;
        size_t total = 0;

        while (total < n) {
                ssize_t bytes = read(fd, ptr + total, n - total);
                if (bytes < 0) {
                        if (errno == EINTR) continue;
                        return -1;
                }
                if (bytes == 0) return -1; /* EOF */
                total += bytes;
        }
        return 0;
}

int llm_should_process(const char *hostname)
{
        if (!hostname) return 0;

        /* Only process SIS pages */
        return (strstr(hostname, "sis.it.tufts.edu") != NULL);
}

char *llm_process_response(const char *url,
                           const char *http_response,
                           size_t      response_len,
                           size_t     *modified_len)
{
        int                 sock = -1;
        struct sockaddr_un  addr;
        char               *result = NULL;

        *modified_len = 0;

        /* Create Unix domain socket */
        sock = socket(AF_UNIX, SOCK_STREAM, 0);
        if (sock < 0) {
                log_error("Failed to create Unix socket: %s", strerror(errno));
                return NULL;
        }

        /* Set timeouts */
        set_socket_timeout(sock, LLM_READ_TIMEOUT);

        /* Connect to Python server */
        memset(&addr, 0, sizeof(addr));
        addr.sun_family = AF_UNIX;
        strncpy(addr.sun_path, LLM_SOCKET_PATH, sizeof(addr.sun_path) - 1);

        if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
                if (errno == ENOENT || errno == ECONNREFUSED) {
                        log_debug("LLM server not running, forwarding original");
                } else {
                        log_warn("Failed to connect to LLM server: %s", strerror(errno));
                }
                close(sock);
                return NULL;
        }

        log_debug("Connected to LLM server, sending %zu bytes", response_len);

        /* Send: url_len (4 bytes big-endian) + url + content_len + content */
        size_t   url_len     = strlen(url);
        uint32_t url_len_net = htonl((uint32_t)url_len);

        if (write_all(sock, &url_len_net, 4) < 0) {
                log_error("Failed to send url_len");
                goto cleanup;
        }

        if (write_all(sock, url, url_len) < 0) {
                log_error("Failed to send url");
                goto cleanup;
        }

        uint32_t content_len_net = htonl((uint32_t)response_len);
        if (write_all(sock, &content_len_net, 4) < 0) {
                log_error("Failed to send content_len");
                goto cleanup;
        }

        if (write_all(sock, http_response, response_len) < 0) {
                log_error("Failed to send content");
                goto cleanup;
        }

        /* Receive: response_len (4 bytes big-endian) + response */
        uint32_t resp_len_net;
        if (read_all(sock, &resp_len_net, 4) < 0) {
                log_error("Failed to read response length");
                goto cleanup;
        }

        uint32_t resp_len = ntohl(resp_len_net);
        if (resp_len == 0) {
                log_debug("LLM server: no modification");
                goto cleanup;
        }

        /* Allocate and read modified response */
        result = malloc(resp_len);
        if (!result) {
                log_error("Failed to allocate %u bytes for response", resp_len);
                goto cleanup;
        }

        if (read_all(sock, result, resp_len) < 0) {
                log_error("Failed to read response content");
                free(result);
                result = NULL;
                goto cleanup;
        }

        *modified_len = resp_len;
        log_info("LLM server: modified response (%u bytes)", resp_len);

cleanup:
        close(sock);
        return result;
}
