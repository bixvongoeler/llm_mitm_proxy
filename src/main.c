#include "utils.h"
#include "proxy.h"

static inline int create_bind_listen(unsigned short port);

/*!
 * @brief Main entry point. Handles arg parsing and validation, then calls main run loop.
 */
int main(int argc, char *argv[])
{
        /* Ignore SIGPIPE to prevent crashes when writing to closed sockets */
        signal(SIGPIPE, SIG_IGN);

        /* Validate Args */
        if (argc != 4) {
                fprintf(stderr,
                        "Usage: %s <port> <ca_cert_path> <ca_key_path>\n",
                        argv[0]);
                return 1;
        }

        /* Parse Port */
        int port = atoi(argv[1]);
        if (port <= PORT_MIN || port > PORT_MAX) {
                fprintf(stderr, "Invalid port value: %d\n", port);
                return 1;
        }

        /* Parse ca_cert_filename */
        char ca_cert_filename[FILENAME_BUFLENGTH];
        strncpy(ca_cert_filename, argv[2], FILENAME_BUFLENGTH);

        /* Parse ca_key_filename */
        char ca_key_filename[FILENAME_BUFLENGTH];
        strncpy(ca_key_filename, argv[3], FILENAME_BUFLENGTH);

        log_debug("Read filenames:\n - ca_cert: `%s`\n - ca_key: `%s`\n",
                  ca_cert_filename,
                  ca_key_filename);

        /* Validate cert files can be read */
        assert(file_readable(ca_cert_filename));
        assert(file_readable(ca_key_filename));

        log_info("Setting Up Event Loop");

        /* Load CA certificate */
        FILE *cert_file = fopen(ca_cert_filename, "r");
        if (!cert_file) {
                log_error("Failed to open CA certificate file: %s", strerror(errno));
                return 1;
        }
        X509 *ca_cert = PEM_read_X509(cert_file, NULL, NULL, NULL);
        fclose(cert_file);
        if (!ca_cert) {
                log_error("Failed to parse CA certificate: %s",
                          ERR_error_string(ERR_get_error(), NULL));
                return 1;
        }
        log_info("Loaded CA certificate from %s", ca_cert_filename);

        /* Load CA private key */
        FILE *key_file = fopen(ca_key_filename, "r");
        if (!key_file) {
                log_error("Failed to open CA key file: %s", strerror(errno));
                X509_free(ca_cert);
                return 1;
        }
        EVP_PKEY *ca_key = PEM_read_PrivateKey(key_file, NULL, NULL, NULL);
        fclose(key_file);
        if (!ca_key) {
                log_error("Failed to parse CA private key: %s",
                          ERR_error_string(ERR_get_error(), NULL));
                X509_free(ca_cert);
                return 1;
        }
        log_info("Loaded CA private key from %s", ca_key_filename);

        proxy_context_t ctx;
        ctx.loop      = EV_DEFAULT;
        ctx.listen_fd = create_bind_listen(port);
        ctx.ca_cert   = ca_cert;
        ctx.ca_key    = ca_key;

        ev_io_init(&ctx.accept_watcher,
                   accept_incoming_connections,
                   ctx.listen_fd,
                   EV_READ);
        ctx.accept_watcher.data = &ctx;
        ev_io_start(ctx.loop, &ctx.accept_watcher);

        log_info("Starting Event Loop");
        ev_run(ctx.loop, 0);

        /* We will never reach this point in normal operation */
        log_info("Execution Halted: Cleaning up");

        close(ctx.listen_fd);

        log_info("Exiting...");

        return 0;
}

/* Setup and bind listening socket */
static int create_bind_listen(unsigned short port)
{
        int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
        if (listen_fd < 0) {
                log_error("Failed to create listening socket: %s", strerror(errno));
                exit(EXIT_FAILURE);
        }

        /* Allow reuse of address */
        int opt = 1;
        if (setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
                log_error("Failed to set SO_REUSEADDR option: %s", strerror(errno));
                close(listen_fd);
                exit(EXIT_FAILURE);
        }

        /* Set nonblocking */
        if (set_socket_blocking(listen_fd, false) == false) {
                log_error("Failed to set listen socket as nonblocking");
                close(listen_fd);
                exit(EXIT_FAILURE);
        }

        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family      = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port        = htons(port);

        if (bind(listen_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
                log_error("Failed to bind listening socket: %s to port %d",
                          strerror(errno),
                          port);
                close(listen_fd);
                return -1;
        }

        if (listen(listen_fd, SOMAXCONN) < 0) {
                log_error("Failed to listen on socket: %s", strerror(errno));
                close(listen_fd);
                return -1;
        }

        log_info("Listening from fd: %d, on port %d", listen_fd, port);

        return listen_fd;
}
