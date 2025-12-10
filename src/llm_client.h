/**
 * llm_client.h - Client for communicating with Python LLM injection server
 *
 * Sends HTTP responses to Python server via Unix domain socket,
 * receives modified content back.
 */

#ifndef LLM_CLIENT_H
#define LLM_CLIENT_H

#include <stddef.h>

#define LLM_SOCKET_PATH "/tmp/llm_proxy.sock"

/**
 * Send HTTP response to LLM server for processing.
 *
 * @param url           The request URL (e.g., "sis.it.tufts.edu/path")
 * @param http_response Raw HTTP response (headers + body)
 * @param response_len  Length of HTTP response
 * @param modified_len  Output: length of modified response (if any)
 *
 * @return Pointer to modified response (caller must free), or NULL if:
 *         - Server not running
 *         - Server returned no modification (len=0)
 *         - Error occurred
 */
char *llm_process_response(const char *url,
                           const char *http_response,
                           size_t      response_len,
                           size_t     *modified_len);

/**
 * Check if a hostname should be processed by the LLM server.
 *
 * @param hostname The target hostname
 * @return true if this host's responses should be sent to LLM server
 */
int llm_should_process(const char *hostname);

#endif /* LLM_CLIENT_H */
