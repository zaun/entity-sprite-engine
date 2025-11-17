/*
 * project: Entity Sprite Engine
 *
 * HTTP client implementation for making asynchronous HTTP GET requests using
 * pthreads. Provides a simple API for making HTTP requests with callbacks and
 * timeouts.
 *
 * Copyright (c) 2025-2026 Entity Sprite Engine
 * See LICENSE.md for details.
 */

#define _POSIX_C_SOURCE 200112L
#define ESE_HTTP_IMPLEMENTATION

#include "utility/job_queue.h"
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

// mbedTLS includes
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/entropy.h"
#include "mbedtls/error.h"
#include "mbedtls/net_sockets.h"
#include "mbedtls/ssl.h"
#include "mbedtls/x509_crt.h"

#include "core/engine.h"
#include "core/memory_manager.h"
#include "platform/time.h"
#include "scripting/lua_engine.h"
#include "types/http.h"
#include "utility/log.h"

// ========================================
// PRIVATE STRUCT DEFINITION
// ========================================

/**
 * @brief Internal structure for EseHttpRequest
 *
 * @details Contains all information needed to make an HTTP GET request
 * including parsed URL components, timeout settings, callback, and user data.
 */
struct EseHttpRequest {
    char *url;                /** Original URL string */
    bool is_https;            /** Whether this is an HTTPS request */
    char *host;               /** Parsed hostname */
    char *path;               /** Parsed path */
    char *port;               /** Parsed port number */
    long timeout_ms;          /** Timeout in milliseconds */
    http_callback_t callback; /** Callback function */
    void *user_data;          /** User data pointer */

    // Response data - set in the main thread after worker thread completes
    int status_code; /** HTTP status code, -1 if not completed */
    char *headers;   /** Response headers */
    char *body;      /** Response body */
    bool done;       /** Whether request is completed */

    // Lua integration
    lua_State *lua_state; /** Associated Lua state */
    int lua_ref;          /** Lua registry reference */
    int lua_ref_count;    /** Reference count for Lua GC */
};

typedef struct EseHttpRequestState {
    // Redirect handling
    int redirect_count;   /** Number of redirects followed */
    int max_redirects;    /** Maximum number of redirects to follow */
    char **redirect_urls; /** Array of URLs visited during redirects */

    // Current URL state (for redirects)
    char *current_host;    /** Current hostname */
    char *current_path;    /** Current path */
    char *current_port;    /** Current port number */
    bool current_is_https; /** Whether current URL is HTTPS */

    // SSL/TLS support
    mbedtls_net_context server_fd;     /** mbedTLS network context */
    mbedtls_ssl_context ssl;           /** mbedTLS SSL context */
    mbedtls_ssl_config conf;           /** mbedTLS SSL configuration */
    mbedtls_entropy_context entropy;   /** mbedTLS entropy context */
    mbedtls_ctr_drbg_context ctr_drbg; /** mbedTLS CTR DRBG context */
} EseHttpRequestState;

typedef struct EseHttpRequestResult {
    int status_code;
    char *headers;
    char *body;
    uint8_t *raw;
    size_t raw_len;
} EseHttpRequestResult;

// ========================================
// PRIVATE FORWARD DECLARATIONS
// ========================================

// URL parsing
static bool _http_parse_url(EseHttpRequest *request);
static bool _http_parse_redirect_url_to_state(EseHttpRequestState *state, const char *location);

// Socket helpers
static int _http_parse_status_and_headers(const uint8_t *raw, size_t raw_len, int *status_out,
                                          char **headers_out, size_t *hdr_len_out);

// Redirect handling
static char *_http_extract_location_header(const char *headers);
static bool _http_is_redirect_status(int status_code);
static bool _http_should_follow_redirect(int status_code, EseHttpRequestState *state);
static void _http_add_redirect_url(const char *url, EseHttpRequestState *state);

// Threading / Job Queue
static JobResult _http_worker_thread(void *thread_data, const void *user_data,
                                     volatile bool *canceled);
static void _http_worker_callback(ese_job_id_t job_id, void *user_data, void *result);

// Result handling
static void *_http_result_mem_copy(const void *wr, size_t wr_sz, size_t *out_sz);
static void _http_result_free(void *wr);

// Internal Lua ref setter forward declarations (implemented later in this file)
void _ese_http_request_set_lua_ref(EseHttpRequest *request, int ref);
void _ese_http_request_set_lua_ref_count(EseHttpRequest *request, int count);

// ========================================
// PRIVATE FUNCTIONS
// ========================================

// URL parsing
/**
 * @brief Parses the URL into host, port, and path components.
 *
 * @details Parses URLs in the format "http://host[:port]/path" and populates
 * the request structure with the parsed components.
 *
 * @param request The HTTP request to parse
 * @return true on success, false on parse error
 */
static bool _http_parse_url(EseHttpRequest *request) {
    const char *p = request->url;
    const char *scheme = NULL;
    size_t scheme_len = 0;
    int default_port = 80;

    // Check for HTTP or HTTPS scheme
    if (strncmp(p, "http://", 7) == 0) {
        scheme = "http://";
        scheme_len = 7;
        default_port = 80;
        request->is_https = false;
    } else if (strncmp(p, "https://", 8) == 0) {
        scheme = "https://";
        scheme_len = 8;
        default_port = 443;
        request->is_https = true;
    } else {
        return false;
    }

    p += scheme_len;
    const char *slash = strchr(p, '/');
    const char *hostport_end = slash ? slash : p + strlen(p);
    const char *colon = NULL;

    for (const char *q = p; q < hostport_end; ++q) {
        if (*q == ':') {
            colon = q;
            break;
        }
    }

    if (colon) {
        size_t host_len = (size_t)(colon - p);
        request->host = (char *)memory_manager.malloc(host_len + 1, MMTAG_HTTP);
        if (!request->host) {
            return false;
        }
        memcpy(request->host, p, host_len);
        request->host[host_len] = '\0';

        size_t port_len = (size_t)(hostport_end - colon - 1);
        request->port = (char *)memory_manager.malloc(port_len + 1, MMTAG_HTTP);
        if (!request->port) {
            return false;
        }
        memcpy(request->port, colon + 1, port_len);
        request->port[port_len] = '\0';
    } else {
        size_t host_len = (size_t)(hostport_end - p);
        request->host = (char *)memory_manager.malloc(host_len + 1, MMTAG_HTTP);
        if (!request->host) {
            return false;
        }
        memcpy(request->host, p, host_len);
        request->host[host_len] = '\0';

        request->port = (char *)memory_manager.malloc(8, MMTAG_HTTP);
        if (!request->port) {
            return false;
        }
        snprintf(request->port, 8, "%d", default_port);
    }

    if (slash) {
        size_t path_len = strlen(slash);
        request->path = (char *)memory_manager.malloc(path_len + 1, MMTAG_HTTP);
        if (!request->path) {
            return false;
        }
        strcpy(request->path, slash);
    } else {
        request->path = (char *)memory_manager.malloc(2, MMTAG_HTTP);
        if (!request->path) {
            return false;
        }
        strcpy(request->path, "/");
    }

    return true;
}

/**
 * @brief Parses a redirect URL (Location header) and updates the worker state.
 *
 * @details Handles both absolute URLs and relative URLs. Works with state
 * instead of the const request object so it can be used in the worker thread.
 *
 * @param state The HTTP request state to update
 * @param location The Location header value
 * @return true on success, false on parse error
 */
static bool _http_parse_redirect_url_to_state(EseHttpRequestState *state, const char *location) {
    if (!location || strlen(location) == 0 || !state) {
        return false;
    }

    // If it's an absolute URL, parse it directly
    if (strncmp(location, "http://", 7) == 0 || strncmp(location, "https://", 8) == 0) {
        const char *p = location;
        const char *scheme = NULL;
        size_t scheme_len = 0;
        int default_port = 80;
        bool is_https = false;

        // Determine scheme
        if (strncmp(p, "http://", 7) == 0) {
            scheme_len = 7;
            default_port = 80;
            is_https = false;
        } else if (strncmp(p, "https://", 8) == 0) {
            scheme_len = 8;
            default_port = 443;
            is_https = true;
        }

        p += scheme_len;
        const char *slash = strchr(p, '/');
        const char *hostport_end = slash ? slash : p + strlen(p);
        const char *colon = NULL;

        for (const char *q = p; q < hostport_end; ++q) {
            if (*q == ':') {
                colon = q;
                break;
            }
        }

        char *new_host;
        char *new_port;
        char *new_path;

        if (colon) {
            size_t host_len = (size_t)(colon - p);
            new_host = (char *)memory_manager.malloc(host_len + 1, MMTAG_HTTP);
            if (!new_host)
                return false;
            memcpy(new_host, p, host_len);
            new_host[host_len] = '\0';

            size_t port_len = (size_t)(hostport_end - colon - 1);
            new_port = (char *)memory_manager.malloc(port_len + 1, MMTAG_HTTP);
            if (!new_port) {
                memory_manager.free(new_host);
                return false;
            }
            memcpy(new_port, colon + 1, port_len);
            new_port[port_len] = '\0';
        } else {
            size_t host_len = (size_t)(hostport_end - p);
            new_host = (char *)memory_manager.malloc(host_len + 1, MMTAG_HTTP);
            if (!new_host)
                return false;
            memcpy(new_host, p, host_len);
            new_host[host_len] = '\0';

            new_port = (char *)memory_manager.malloc(8, MMTAG_HTTP);
            if (!new_port) {
                memory_manager.free(new_host);
                return false;
            }
            snprintf(new_port, 8, "%d", default_port);
        }

        if (slash) {
            size_t path_len = strlen(slash);
            new_path = (char *)memory_manager.malloc(path_len + 1, MMTAG_HTTP);
            if (!new_path) {
                memory_manager.free(new_host);
                memory_manager.free(new_port);
                return false;
            }
            strcpy(new_path, slash);
        } else {
            new_path = (char *)memory_manager.malloc(2, MMTAG_HTTP);
            if (!new_path) {
                memory_manager.free(new_host);
                memory_manager.free(new_port);
                return false;
            }
            strcpy(new_path, "/");
        }

        // Free old values and update state
        if (state->current_host)
            memory_manager.free(state->current_host);
        if (state->current_path)
            memory_manager.free(state->current_path);
        if (state->current_port)
            memory_manager.free(state->current_port);

        state->current_host = new_host;
        state->current_port = new_port;
        state->current_path = new_path;
        state->current_is_https = is_https;

        return true;
    } else {
        // Relative URL - construct full URL
        size_t total_len =
            strlen(state->current_host) + strlen(state->current_port) + strlen(location) + 30;
        char *full_url = (char *)memory_manager.malloc(total_len, MMTAG_HTTP);
        if (!full_url) {
            return false;
        }

        snprintf(full_url, total_len, "%s://%s:%s%s", state->current_is_https ? "https" : "http",
                 state->current_host, state->current_port, location);

        // Parse the constructed URL (simplified - just extract new path)
        if (strncmp(full_url, "http://", 7) == 0 || strncmp(full_url, "https://", 8) == 0) {
            const char *p = strchr(full_url, '/') + 2; // Skip http[s]://
            const char *slash2 = strchr(p, '/');

            char *new_host;
            char *new_port;
            char *new_path;

            const char *hostport_end = slash2 ? slash2 : full_url + strlen(full_url);
            const char *colon = NULL;

            for (const char *q = p; q < hostport_end; ++q) {
                if (*q == ':') {
                    colon = q;
                    break;
                }
            }

            if (colon) {
                size_t host_len = (size_t)(colon - p);
                new_host = (char *)memory_manager.malloc(host_len + 1, MMTAG_HTTP);
                if (!new_host) {
                    memory_manager.free(full_url);
                    return false;
                }
                memcpy(new_host, p, host_len);
                new_host[host_len] = '\0';

                size_t port_len = (size_t)(hostport_end - colon - 1);
                new_port = (char *)memory_manager.malloc(port_len + 1, MMTAG_HTTP);
                if (!new_port) {
                    memory_manager.free(new_host);
                    memory_manager.free(full_url);
                    return false;
                }
                memcpy(new_port, colon + 1, port_len);
                new_port[port_len] = '\0';
            } else {
                size_t host_len = (size_t)(hostport_end - p);
                new_host = (char *)memory_manager.malloc(host_len + 1, MMTAG_HTTP);
                if (!new_host) {
                    memory_manager.free(full_url);
                    return false;
                }
                memcpy(new_host, p, host_len);
                new_host[host_len] = '\0';

                new_port = (char *)memory_manager.malloc(8, MMTAG_HTTP);
                if (!new_port) {
                    memory_manager.free(new_host);
                    memory_manager.free(full_url);
                    return false;
                }
                snprintf(new_port, 8, "%d", state->current_is_https ? 443 : 80);
            }

            if (slash2) {
                size_t path_len = strlen(slash2);
                new_path = (char *)memory_manager.malloc(path_len + 1, MMTAG_HTTP);
                if (!new_path) {
                    memory_manager.free(new_host);
                    memory_manager.free(new_port);
                    memory_manager.free(full_url);
                    return false;
                }
                strcpy(new_path, slash2);
            } else {
                new_path = (char *)memory_manager.malloc(2, MMTAG_HTTP);
                if (!new_path) {
                    memory_manager.free(new_host);
                    memory_manager.free(new_port);
                    memory_manager.free(full_url);
                    return false;
                }
                strcpy(new_path, "/");
            }

            memory_manager.free(full_url);

            // Free old values and update state
            if (state->current_host)
                memory_manager.free(state->current_host);
            if (state->current_path)
                memory_manager.free(state->current_path);
            if (state->current_port)
                memory_manager.free(state->current_port);

            state->current_host = new_host;
            state->current_port = new_port;
            state->current_path = new_path;

            return true;
        }

        memory_manager.free(full_url);
        return false;
    }
}

/**
 * @brief Parses HTTP status code and headers from response.
 *
 * @details Extracts the status code and headers from the raw HTTP response.
 * Headers are returned as a NUL-terminated string.
 *
 * @param raw Raw response data
 * @param raw_len Length of raw response data
 * @param status_out Pointer to store status code
 * @param headers_out Pointer to store headers string
 * @param hdr_len_out Pointer to store headers length
 * @return 0 on success, negative on error
 */
static int _http_parse_status_and_headers(const uint8_t *raw, size_t raw_len, int *status_out,
                                          char **headers_out, size_t *hdr_len_out) {
    if (!raw || raw_len == 0) {
        return -1;
    }

    // Find "\r\n"
    const char *s = (const char *)raw;
    const char *raw_end = s + raw_len;
    const char *line_end = strstr(s, "\r\n");
    if (!line_end) {
        return -1;
    }

    // Parse status code
    // Expect "HTTP/1.x " then 3 digit code
    const char *code_pos = strchr(s, ' ');
    if (!code_pos || code_pos + 1 >= line_end) {
        return -1;
    }

    // Skip spaces
    while (code_pos < line_end && *code_pos == ' ') {
        ++code_pos;
    }

    int code = 0;
    if (code_pos + 3 <= line_end) {
        char tmp[4] = {0};
        size_t avail = (size_t)(line_end - code_pos);
        size_t tocopy = avail >= 3 ? 3 : avail;
        memcpy(tmp, code_pos, tocopy);
        tmp[3] = '\0';
        code = atoi(tmp);
    } else {
        code = -1;
    }

    // Find end of headers (\r\n\r\n)
    const char *hdr_end = strstr(s, "\r\n\r\n");
    if (!hdr_end) {
        // No headers terminator; headers are everything after status line
        size_t headers_len = raw_len - (size_t)(line_end - s) - 2;
        char *headers = (char *)memory_manager.malloc(headers_len + 1, MMTAG_HTTP);
        if (!headers) {
            return -1;
        }
        memcpy(headers, line_end + 2, headers_len);
        headers[headers_len] = '\0';
        *status_out = code;
        *headers_out = headers;
        if (hdr_len_out) {
            *hdr_len_out = headers_len;
        }
        return 0;
    }

    size_t headers_len = (size_t)(hdr_end - (line_end + 2));
    char *headers = (char *)memory_manager.malloc(headers_len + 1, MMTAG_HTTP);
    if (!headers) {
        return -1;
    }
    memcpy(headers, line_end + 2, headers_len);
    headers[headers_len] = '\0';
    *status_out = code;
    *headers_out = headers;
    if (hdr_len_out) {
        *hdr_len_out = headers_len;
    }
    return 0;
}

// Redirect handling
/**
 * @brief Extracts the Location header from HTTP response headers.
 *
 * @details Searches for the "Location:" header (case-insensitive) and returns
 * the value without leading/trailing whitespace.
 *
 * @param headers The HTTP response headers string
 * @return The Location header value, or NULL if not found
 */
static char *_http_extract_location_header(const char *headers) {
    if (!headers) {
        return NULL;
    }

    const char *location = strstr(headers, "Location:");
    if (!location) {
        // Try case-insensitive search
        const char *p = headers;
        while (*p) {
            if (strncasecmp(p, "location:", 9) == 0) {
                location = p;
                break;
            }
            p = strchr(p + 1, '\n');
            if (!p)
                break;
            p++; // Skip the newline
        }
    }

    if (!location) {
        return NULL;
    }

    // Skip "Location:" and any whitespace
    location += 9;
    while (*location == ' ' || *location == '\t') {
        location++;
    }

    // Find end of line
    const char *end = strchr(location, '\r');
    if (!end) {
        end = strchr(location, '\n');
    }
    if (!end) {
        end = location + strlen(location);
    }

    // Trim trailing whitespace
    while (end > location &&
           (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\r' || end[-1] == '\n')) {
        end--;
    }

    if (end <= location) {
        return NULL;
    }

    // Allocate and copy the location value
    size_t len = (size_t)(end - location);
    char *result = (char *)memory_manager.malloc(len + 1, MMTAG_HTTP);
    if (!result) {
        return NULL;
    }

    memcpy(result, location, len);
    result[len] = '\0';

    return result;
}

/**
 * @brief Checks if a status code indicates a redirect.
 *
 * @param status_code The HTTP status code
 * @return true if the status code is a redirect (301, 302, 307, 308)
 */
static bool _http_is_redirect_status(int status_code) {
    return (status_code == 301 || status_code == 302 || status_code == 307 || status_code == 308);
}

/**
 * @brief Determines if a redirect should be followed.
 *
 * @details Checks if the redirect count is below the maximum and if the
 * status code indicates a redirect that should be followed.
 *
 * @param status_code The HTTP status code
 * @param state The request state
 * @return true if the redirect should be followed
 */
static bool _http_should_follow_redirect(int status_code, EseHttpRequestState *state) {
    if (!_http_is_redirect_status(status_code)) {
        return false;
    }

    if (state->redirect_count >= state->max_redirects) {
        log_debug("HTTP", "Maximum redirects (%d) exceeded", state->max_redirects);
        return false;
    }

    return true;
}

/**
 * @brief Adds a URL to the redirect history.
 *
 * @details Tracks URLs visited during redirects to detect redirect loops.
 *
 * @param url The URL to add to the history
 * @param state The request state
 */
static void _http_add_redirect_url(const char *url, EseHttpRequestState *state) {
    if (!url || !state) {
        return;
    }

    // Reallocate the redirect URLs array
    char **new_urls = (char **)memory_manager.realloc(
        state->redirect_urls, (state->redirect_count + 1) * sizeof(char *), MMTAG_HTTP);
    if (!new_urls) {
        return;
    }

    state->redirect_urls = new_urls;

    // Allocate and copy the URL
    state->redirect_urls[state->redirect_count] =
        (char *)memory_manager.malloc(strlen(url) + 1, MMTAG_HTTP);
    if (state->redirect_urls[state->redirect_count]) {
        strcpy(state->redirect_urls[state->redirect_count], url);
        state->redirect_count++;
    }
}

static void *_http_result_mem_copy(const void *wr, size_t wr_sz, size_t *out_sz) {
    (void)wr_sz;
    const EseHttpRequestResult *src = (const EseHttpRequestResult *)wr;
    if (!src)
        return NULL;

    EseHttpRequestResult *dst =
        (EseHttpRequestResult *)memory_manager.malloc(sizeof(*dst), MMTAG_HTTP);
    if (!dst)
        return NULL;
    memset(dst, 0, sizeof(*dst));

    dst->status_code = src->status_code;

    if (src->headers) {
        size_t n = strlen(src->headers) + 1;
        dst->headers = (char *)memory_manager.malloc(n, MMTAG_HTTP);
        if (dst->headers)
            memcpy(dst->headers, src->headers, n);
    }
    if (src->body) {
        size_t n = strlen(src->body) + 1;
        dst->body = (char *)memory_manager.malloc(n, MMTAG_HTTP);
        if (dst->body)
            memcpy(dst->body, src->body, n);
    }
    dst->raw_len = src->raw_len;
    if (src->raw && src->raw_len) {
        dst->raw = (uint8_t *)memory_manager.malloc(src->raw_len, MMTAG_HTTP);
        if (dst->raw)
            memcpy(dst->raw, src->raw, src->raw_len);
    }

    if (out_sz)
        *out_sz = sizeof(*dst);
    return dst;
}

static void _http_result_free(void *wr) {
    EseHttpRequestResult *r = (EseHttpRequestResult *)wr;
    if (!r)
        return;
    if (r->headers)
        memory_manager.free(r->headers);
    if (r->body)
        memory_manager.free(r->body);
    if (r->raw)
        memory_manager.free(r->raw);
    memory_manager.free(r);
}

// Threading
/**
 * @brief Worker thread function for HTTP requests.
 *
 * @details Executes the HTTP request on a background thread. Handles
 * connection, request sending, response receiving, parsing, and callback
 * invocation.
 *
 * @param arg Pointer to EseHttpRequest structure
 * @return NULL (thread return value)
 */
static JobResult _http_worker_thread(void *thread_data, const void *user_data,
                                     volatile bool *canceled) {
    (void)thread_data;
    const EseHttpRequest *request = (const EseHttpRequest *)user_data;

    log_debug("HTTP", "Worker thread started for URL: %s", request->url);

    int status_code = -1;
    char *headers = NULL;
    uint8_t *raw = NULL;
    size_t raw_len = 0;
    char *body_cstr = NULL;

    bool timeout = false;
    bool error = false;

    uint64_t step = 0;
    uint64_t prev_time = 0;

    // Connection state variables
    int sfd = -1;
    struct addrinfo hints;
    struct addrinfo *addrinfo_res = NULL;
    struct addrinfo *addrinfo_current = NULL;
    int socket_flags = 0;
    size_t raw_capacity = 0;
    bool result_claimed = false; // Track if pointers have been transferred to result

    // Setup the request state
    EseHttpRequestState *state =
        (EseHttpRequestState *)memory_manager.malloc(sizeof(EseHttpRequestState), MMTAG_HTTP);
    if (!state) {
        JobResult res = {.result = NULL, .size = 0};
        return res;
    }
    state->redirect_count = 0;
    state->max_redirects = 10; // Default maximum of 10 redirects
    state->redirect_urls = NULL;

    // Initialize current URL state from the const request
    state->current_host = (char *)memory_manager.malloc(strlen(request->host) + 1, MMTAG_HTTP);
    state->current_path = (char *)memory_manager.malloc(strlen(request->path) + 1, MMTAG_HTTP);
    state->current_port = (char *)memory_manager.malloc(strlen(request->port) + 1, MMTAG_HTTP);
    if (state->current_host && state->current_path && state->current_port) {
        strcpy(state->current_host, request->host);
        strcpy(state->current_path, request->path);
        strcpy(state->current_port, request->port);
        state->current_is_https = request->is_https;
    } else {
        // Failed to allocate
        if (state->current_host)
            memory_manager.free(state->current_host);
        if (state->current_path)
            memory_manager.free(state->current_path);
        if (state->current_port)
            memory_manager.free(state->current_port);
        memory_manager.free(state);
        JobResult res = {.result = NULL, .size = 0};
        return res;
    }

    // Initialize the result
    EseHttpRequestResult *result =
        (EseHttpRequestResult *)memory_manager.malloc(sizeof(EseHttpRequestResult), MMTAG_HTTP);
    result->status_code = -1;
    result->headers = NULL;
    result->body = NULL;
    result->raw = NULL;
    result->raw_len = 0;

    // Add the initial URL to redirect history
    _http_add_redirect_url(request->url, state);

    while (!*canceled && !timeout && !error) {
        switch (step) {
        case 0: {
            // SSL Setup
            if (state->current_is_https) {
                char port_str[6];
                int ret;

                // Convert port to string
                snprintf(port_str, sizeof(port_str), "%s", state->current_port);

                // Initialize mbedTLS contexts
                mbedtls_net_init(&state->server_fd);
                mbedtls_ssl_init(&state->ssl);
                mbedtls_ssl_config_init(&state->conf);
                mbedtls_entropy_init(&state->entropy);
                mbedtls_ctr_drbg_init(&state->ctr_drbg);

                // Initialize the RNG and the session data
                if ((ret = mbedtls_ctr_drbg_seed(
                         &state->ctr_drbg, mbedtls_entropy_func, &state->entropy,
                         (const unsigned char *)"ese_http_client", 15)) != 0) {
                    log_debug("HTTP", "mbedtls_ctr_drbg_seed returned %d", ret);
                    error = true;
                    break;
                }

                // Load defaults
                if ((ret = mbedtls_ssl_config_defaults(&state->conf, MBEDTLS_SSL_IS_CLIENT,
                                                       MBEDTLS_SSL_TRANSPORT_STREAM,
                                                       MBEDTLS_SSL_PRESET_DEFAULT)) != 0) {
                    log_debug("HTTP", "mbedtls_ssl_config_defaults returned %d", ret);
                    error = true;
                    break;
                }

                // Set the RNG callback
                mbedtls_ssl_conf_rng(&state->conf, mbedtls_ctr_drbg_random, &state->ctr_drbg);

                // Set auth mode to none for testing (in production, use
                // MBEDTLS_SSL_VERIFY_REQUIRED)
                mbedtls_ssl_conf_authmode(&state->conf, MBEDTLS_SSL_VERIFY_NONE);

                // Set TLS version to 1.2 for better compatibility
                mbedtls_ssl_conf_min_version(&state->conf, MBEDTLS_SSL_MAJOR_VERSION_3,
                                             MBEDTLS_SSL_MINOR_VERSION_3);
                mbedtls_ssl_conf_max_version(&state->conf, MBEDTLS_SSL_MAJOR_VERSION_3,
                                             MBEDTLS_SSL_MINOR_VERSION_3);

                // Enable session tickets for better compatibility
                mbedtls_ssl_conf_session_tickets(&state->conf, MBEDTLS_SSL_SESSION_TICKETS_ENABLED);

                // Set renegotiation to prevent issues
                mbedtls_ssl_conf_renegotiation(&state->conf, MBEDTLS_SSL_RENEGOTIATION_DISABLED);

                // Use default cipher suites for better compatibility
                // mbedTLS will negotiate the best available cipher suite

                // Set hostname for SNI
                if ((ret = mbedtls_ssl_set_hostname(&state->ssl, state->current_host)) != 0) {
                    log_debug("HTTP", "mbedtls_ssl_set_hostname returned %d", ret);
                    error = true;
                    break;
                }

                // Set up the SSL context
                if ((ret = mbedtls_ssl_setup(&state->ssl, &state->conf)) != 0) {
                    log_debug("HTTP", "mbedtls_ssl_setup returned %d", ret);
                    error = true;
                    break;
                }

                // Connect to the server
                log_debug("HTTP", "Connecting to %s:%s", state->current_host, port_str);
                if ((ret = mbedtls_net_connect(&state->server_fd, state->current_host, port_str,
                                               MBEDTLS_NET_PROTO_TCP)) != 0) {
                    char error_buf[256];
                    mbedtls_strerror(ret, error_buf, sizeof(error_buf));
                    log_debug("HTTP", "mbedtls_net_connect returned %d: %s", ret, error_buf);
                    error = true;
                    break;
                }
                log_debug("HTTP", "TCP connection established");

                // Set the underlying I/O callbacks
                mbedtls_ssl_set_bio(&state->ssl, &state->server_fd, mbedtls_net_send,
                                    mbedtls_net_recv, NULL);

                step = 1;
                prev_time = time_now();

                log_verbose("HTTP", "SSL/TLS connection established successfully");
            } else {
                step = 2;
            }
            break;
        }
        case 1: {
            // Wait for SSL handshake to complete
            int ret = mbedtls_ssl_handshake(&state->ssl);

            // Perform the SSL/TLS handshake
            if (ret == 0) {
                // Handshake completed successfully
                // Verify the server certificate
                uint32_t flags = mbedtls_ssl_get_verify_result(&state->ssl);
                if (flags != 0) {
                    char vrfy_buf[512];
                    mbedtls_x509_crt_verify_info(vrfy_buf, sizeof(vrfy_buf), "  ! ", flags);
                    log_debug("HTTP", "Certificate verification failed: %s", vrfy_buf);
                    error = true;
                    break;
                }

                log_verbose("HTTP", "SSL/TLS handshake completed successfully");
                step = 5; // Move to send request5
            } else if (ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE) {
                char error_buf[256];
                mbedtls_strerror(ret, error_buf, sizeof(error_buf));
                log_debug("HTTP", "mbedtls_ssl_handshake returned %d: %s", ret, error_buf);
                error = true;
                break;
            }

            // Check timeout (convert nanoseconds to milliseconds)
            if ((time_now() - prev_time) / 1000000 >= (uint64_t)request->timeout_ms) {
                log_debug("HTTP", "SSL handshake timeout");
                timeout = true;
            }
            break;
        }
        case 2: {
            // Initiate connection (DNS lookup and start non-blocking connect)
            memset(&hints, 0, sizeof(hints));
            hints.ai_family = AF_UNSPEC;
            hints.ai_socktype = SOCK_STREAM;

            int gai_err =
                getaddrinfo(state->current_host, state->current_port, &hints, &addrinfo_res);
            if (gai_err != 0) {
                log_debug("HTTP", "getaddrinfo failed for %s:%s", state->current_host,
                          state->current_port);
                error = true;
                break;
            }

            addrinfo_current = addrinfo_res;
            prev_time = time_now();
            step = 3;
            break;
        }
        case 3: {
            // Try to connect to next address
            if (!addrinfo_current) {
                // No more addresses to try
                log_debug("HTTP", "Connection failed for all addresses");
                if (addrinfo_res) {
                    freeaddrinfo(addrinfo_res);
                    addrinfo_res = NULL;
                }
                error = true;
                break;
            }

            // Create socket
            sfd = socket(addrinfo_current->ai_family, addrinfo_current->ai_socktype,
                         addrinfo_current->ai_protocol);
            if (sfd < 0) {
                log_debug("HTTP", "socket() failed, trying next address");
                addrinfo_current = addrinfo_current->ai_next;
                break;
            }

            // Set non-blocking
            socket_flags = fcntl(sfd, F_GETFL, 0);
            if (socket_flags < 0) {
                close(sfd);
                sfd = -1;
                addrinfo_current = addrinfo_current->ai_next;
                break;
            }

            if (fcntl(sfd, F_SETFL, socket_flags | O_NONBLOCK) < 0) {
                close(sfd);
                sfd = -1;
                addrinfo_current = addrinfo_current->ai_next;
                break;
            }

            // Start connection
            int rc = connect(sfd, addrinfo_current->ai_addr, addrinfo_current->ai_addrlen);
            if (rc == 0) {
                // Connected immediately, restore blocking
                fcntl(sfd, F_SETFL, socket_flags);
                log_verbose("HTTP", "Connected to %s:%s", state->current_host, state->current_port);
                freeaddrinfo(addrinfo_res);
                addrinfo_res = NULL;
                step = 5; // Move to send request
                break;
            } else if (errno != EINPROGRESS) {
                // Connection failed immediately
                close(sfd);
                sfd = -1;
                addrinfo_current = addrinfo_current->ai_next;
                break;
            }

            // Connection in progress, move to wait for completion
            step = 4;
            break;
        }
        case 4: {
            // Wait for connection to complete
            fd_set wfds;
            FD_ZERO(&wfds);
            FD_SET(sfd, &wfds);

            struct timeval tv;
            tv.tv_sec = 0;
            tv.tv_usec = 1000; // 1ms poll

            int s = select(sfd + 1, NULL, &wfds, NULL, &tv);
            if (s > 0 && FD_ISSET(sfd, &wfds)) {
                // Socket is writable, check for errors
                int soerr = 0;
                socklen_t len = sizeof(soerr);
                if (getsockopt(sfd, SOL_SOCKET, SO_ERROR, &soerr, &len) < 0) {
                    close(sfd);
                    sfd = -1;
                    addrinfo_current = addrinfo_current->ai_next;
                    step = 3; // Try next address
                    break;
                }

                if (soerr == 0) {
                    // Success, restore blocking
                    fcntl(sfd, F_SETFL, socket_flags);
                    log_verbose("HTTP", "Connected to %s:%s", state->current_host,
                                state->current_port);
                    freeaddrinfo(addrinfo_res);
                    addrinfo_res = NULL;
                    step = 5; // Move to send request
                    break;
                } else {
                    // Connection failed
                    close(sfd);
                    sfd = -1;
                    addrinfo_current = addrinfo_current->ai_next;
                    step = 3; // Try next address
                    break;
                }
            } else if (s < 0) {
                // Select error
                close(sfd);
                sfd = -1;
                addrinfo_current = addrinfo_current->ai_next;
                step = 3; // Try next address
                break;
            }
            // s == 0 means poll timeout, continue waiting

            // Check overall timeout (convert nanoseconds to milliseconds)
            if ((time_now() - prev_time) / 1000000 >= (uint64_t)request->timeout_ms) {
                log_debug("HTTP", "Connection timeout");
                if (sfd >= 0) {
                    close(sfd);
                    sfd = -1;
                }
                if (addrinfo_res) {
                    freeaddrinfo(addrinfo_res);
                    addrinfo_res = NULL;
                }
                timeout = true;
            }
            break;
        }
        case 5: {
            // Send the request
            char reqbuf[4096];
            int n = snprintf(reqbuf, sizeof(reqbuf),
                             "GET %s HTTP/1.1\r\n"
                             "Host: %s\r\n"
                             "Connection: close\r\n"
                             "User-Agent: Entity-Sprite-Engine/1.0\r\n"
                             "\r\n",
                             state->current_path ? state->current_path : "/",
                             state->current_host ? state->current_host : "");

            if (n < 0 || (size_t)n >= sizeof(reqbuf)) {
                log_debug("HTTP", "Failed to format request");
                error = true;
                break;
            }

            ssize_t w;
            if (state->current_is_https) {
                w = mbedtls_ssl_write(&state->ssl, (const unsigned char *)reqbuf, (size_t)n);
            } else {
                w = write(sfd, reqbuf, (size_t)n);
            }

            if (w < 0) {
                log_debug("HTTP", "Failed to send request");
                error = true;
                break;
            }

            log_verbose("HTTP", "Request sent successfully");

            // Allocate initial buffer for response
            const size_t chunk = 4096;
            raw = (uint8_t *)memory_manager.malloc(chunk, MMTAG_HTTP);
            if (!raw) {
                error = true;
                break;
            }
            raw_len = 0;
            raw_capacity = chunk;

            // Move to receive state
            prev_time = time_now();
            step = 6;
            break;
        }
        case 6: {
            // Receive data from the server
            const size_t chunk = 4096;

            // Ensure buffer has space
            if (raw_len + chunk + 1 >= raw_capacity) {
                size_t new_cap = (raw_len + chunk + 1) * 2;
                uint8_t *new_buf = (uint8_t *)memory_manager.realloc(raw, new_cap, MMTAG_HTTP);
                if (!new_buf) {
                    error = true;
                    break;
                }
                raw = new_buf;
                raw_capacity = new_cap;
            }

            ssize_t r;
            if (state->current_is_https) {
                r = mbedtls_ssl_read(&state->ssl, raw + raw_len, chunk);

                if (r > 0) {
                    raw_len += (size_t)r;
                } else if (r == 0 || r == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY) {
                    // Connection closed, move to parse
                    raw[raw_len] = 0; // NUL terminate
                    log_verbose("HTTP", "Response received (%zu bytes)", raw_len);
                    step = 7;
                    break;
                } else if (r == MBEDTLS_ERR_SSL_WANT_READ || r == MBEDTLS_ERR_SSL_WANT_WRITE) {
                    // Would block, continue
                } else {
                    char error_buf[256];
                    mbedtls_strerror(r, error_buf, sizeof(error_buf));
                    log_debug("HTTP", "SSL read error: %d (%s)", r, error_buf);
                    error = true;
                    break;
                }
            } else {
                // Set non-blocking for regular socket
                int flags = fcntl(sfd, F_GETFL, 0);
                fcntl(sfd, F_SETFL, flags | O_NONBLOCK);

                r = recv(sfd, raw + raw_len, chunk, 0);

                if (r > 0) {
                    raw_len += (size_t)r;
                } else if (r == 0) {
                    // Connection closed, move to parse
                    raw[raw_len] = 0; // NUL terminate
                    log_verbose("HTTP", "Response received (%zu bytes)", raw_len);
                    step = 7;
                    break;
                } else {
                    if (errno == EAGAIN || errno == EWOULDBLOCK) {
                        // Would block, continue
                    } else if (errno == EINTR) {
                        // Interrupted, retry
                    } else {
                        log_debug("HTTP", "Socket read error: %d", errno);
                        error = true;
                        break;
                    }
                }
            }

            // Check timeout (convert nanoseconds to milliseconds)
            if ((time_now() - prev_time) / 1000000 >= (uint64_t)request->timeout_ms) {
                log_debug("HTTP", "Response timeout");
                timeout = true;
            }
            break;
        }
        case 7: {
            // Parse the response
            size_t hdr_len = 0;
            if (_http_parse_status_and_headers(raw, raw_len, &status_code, &headers, &hdr_len) !=
                0) {
                log_debug("HTTP", "Failed to parse response");
                error = true;
                break;
            }

            log_verbose("HTTP", "Parsed response - status: %d", status_code);

            // Check if this is a redirect
            if (_http_should_follow_redirect(status_code, state)) {
                char *location = _http_extract_location_header(headers);
                if (location) {
                    log_debug("HTTP", "Following redirect %d to: %s", state->redirect_count,
                              location);

                    // Check for redirect loop
                    bool is_loop = false;
                    for (int i = 0; i < state->redirect_count; i++) {
                        if (state->redirect_urls[i] &&
                            strcmp(state->redirect_urls[i], location) == 0) {
                            log_debug("HTTP", "Redirect loop detected at: %s", location);
                            is_loop = true;
                            break;
                        }
                    }

                    if (is_loop) {
                        memory_manager.free(location);
                        // Treat as final response
                        step = 8;
                        break;
                    }

                    // Parse the redirect URL and update the state
                    if (_http_parse_redirect_url_to_state(state, location)) {
                        _http_add_redirect_url(location, state);
                        memory_manager.free(location);

                        // Clean up current response data
                        if (headers) {
                            memory_manager.free(headers);
                            headers = NULL;
                        }
                        if (raw) {
                            memory_manager.free(raw);
                            raw = NULL;
                        }
                        raw_len = 0;
                        raw_capacity = 0;

                        // Close current connection
                        if (state->current_is_https) {
                            mbedtls_ssl_close_notify(&state->ssl);
                        }
                        if (sfd >= 0) {
                            close(sfd);
                            sfd = -1;
                        }
                        if (addrinfo_res) {
                            freeaddrinfo(addrinfo_res);
                            addrinfo_res = NULL;
                        }
                        addrinfo_current = NULL;

                        // Restart from connection state
                        if (state->current_is_https) {
                            step = 0; // SSL setup
                        } else {
                            step = 2; // Regular connection
                        }
                        break;
                    } else {
                        log_debug("HTTP", "Failed to parse redirect URL: %s", location);
                        memory_manager.free(location);
                        // Fall through to return the redirect response
                    }
                } else {
                    log_debug("HTTP", "Redirect status %d but no Location header", status_code);
                    // Fall through to return the redirect response
                }
            }

            // Not a redirect or redirect handling failed - process as final
            // response Find body start: locate "\r\n\r\n"
            const char *raw_s = (const char *)raw;
            const char *hdr_term = strstr(raw_s, "\r\n\r\n");
            if (hdr_term) {
                const uint8_t *body_ptr = (const uint8_t *)(hdr_term + 4);
                size_t body_len = raw_len - (size_t)(body_ptr - raw);
                size_t cstr_len = body_len + 1;
                body_cstr = (char *)memory_manager.malloc(cstr_len, MMTAG_HTTP);
                if (body_cstr) {
                    memcpy(body_cstr, body_ptr, body_len);
                    body_cstr[body_len] = '\0';
                } else {
                    // Fallback: empty string
                    body_cstr = (char *)memory_manager.malloc(1, MMTAG_HTTP);
                    if (body_cstr) {
                        body_cstr[0] = '\0';
                    }
                }
            } else {
                body_cstr = (char *)memory_manager.malloc(1, MMTAG_HTTP);
                if (body_cstr) {
                    body_cstr[0] = '\0';
                }
            }

            // Move to finalize
            step = 8;
            break;
        }
        case 8: {
            // Store results in result
            result->status_code = status_code;
            result->headers = headers;
            result->raw = raw;
            result->raw_len = raw_len;
            result->body = body_cstr;
            result_claimed = true; // Mark that pointers have been transferred

            log_debug("HTTP", "HTTP request completed with status %d for URL: %s", status_code,
                      request->url);
            log_verbose("HTTP", "Response body length: %zu bytes",
                        body_cstr ? strlen(body_cstr) : 0);

            // Move to final cleanup
            step = 9;
            break;
        }
        case 9: {
            // Clean up and return the result
            // Note: We don't free headers and body_cstr here as they're stored
            // in the request The raw buffer is returned to the caller
            if (sfd >= 0) {
                close(sfd);
                sfd = -1;
            }
            if (addrinfo_res) {
                freeaddrinfo(addrinfo_res);
                addrinfo_res = NULL;
            }

            // Clean up SSL resources
            if (state && state->current_is_https) {
                mbedtls_ssl_free(&state->ssl);
                mbedtls_ssl_config_free(&state->conf);
                mbedtls_net_free(&state->server_fd);
                mbedtls_ctr_drbg_free(&state->ctr_drbg);
                mbedtls_entropy_free(&state->entropy);
            }

            // Free current URL state
            if (state) {
                if (state->current_host)
                    memory_manager.free(state->current_host);
                if (state->current_path)
                    memory_manager.free(state->current_path);
                if (state->current_port)
                    memory_manager.free(state->current_port);
            }

            // Free redirect URLs
            if (state && state->redirect_urls) {
                for (int i = 0; i < state->redirect_count; i++) {
                    if (state->redirect_urls[i]) {
                        memory_manager.free(state->redirect_urls[i]);
                    }
                }
                memory_manager.free(state->redirect_urls);
            }

            if (state) {
                memory_manager.free(state);
                state = NULL;
            }

            JobResult res = {
                .result = result,
                .size = sizeof(EseHttpRequestResult),
                .copy_fn = _http_result_mem_copy,
                .free_fn = _http_result_free,
            };
            return res;
        }
        }
    }

    // Job was canceled or error occurred
    // Cleanup and return NULL
    log_debug("HTTP", "Worker thread completed with error or cancellation");
    if (sfd >= 0) {
        close(sfd);
    }
    if (addrinfo_res) {
        freeaddrinfo(addrinfo_res);
    }

    // Only free local variables if they haven't been transferred to result
    if (!result_claimed) {
        if (headers) {
            memory_manager.free(headers);
            headers = NULL;
        }
        if (raw) {
            memory_manager.free(raw);
            raw = NULL;
        }
        if (body_cstr) {
            memory_manager.free(body_cstr);
            body_cstr = NULL;
        }
    }

    // Clean up SSL resources
    if (state && state->current_is_https) {
        mbedtls_ssl_free(&state->ssl);
        mbedtls_ssl_config_free(&state->conf);
        mbedtls_net_free(&state->server_fd);
        mbedtls_ctr_drbg_free(&state->ctr_drbg);
        mbedtls_entropy_free(&state->entropy);
    }

    // Free current URL state
    if (state) {
        if (state->current_host)
            memory_manager.free(state->current_host);
        if (state->current_path)
            memory_manager.free(state->current_path);
        if (state->current_port)
            memory_manager.free(state->current_port);
    }

    // Free redirect URLs
    if (state && state->redirect_urls) {
        for (int i = 0; i < state->redirect_count; i++) {
            if (state->redirect_urls[i]) {
                memory_manager.free(state->redirect_urls[i]);
            }
        }
        memory_manager.free(state->redirect_urls);
    }

    if (state) {
        memory_manager.free(state);
        state = NULL;
    }

    // Clean up unclaimed result
    if (result) {
        _http_result_free(result);
        result = NULL;
    }

    JobResult res = {
        .result = NULL,
        .size = 0,
        .copy_fn = NULL,
        .free_fn = NULL,
    };
    return res;
}

// Called by the main thread to invoke the callback for a completed job
static void _http_worker_callback(ese_job_id_t job_id, void *user_data, void *result) {
    (void)job_id;
    EseHttpRequest *request = (EseHttpRequest *)user_data;
    EseHttpRequestResult *res = (EseHttpRequestResult *)result;

    if (res) {
        // Store status code
        request->status_code = res->status_code;

        // Store headers copy
        if (res->headers) {
            size_t len = strlen(res->headers) + 1;
            request->headers = (char *)memory_manager.malloc(len, MMTAG_HTTP);
            if (request->headers) {
                strcpy(request->headers, res->headers);
            }
        }

        // Store body copy
        if (res->body) {
            size_t len = strlen(res->body) + 1;
            request->body = (char *)memory_manager.malloc(len, MMTAG_HTTP);
            if (request->body) {
                strcpy(request->body, res->body);
            }
        }

        request->done = true;

        // Invoke callback if set
        if (request->callback) {
            log_verbose("HTTP", "Invoking callback for completed http request");
            request->callback(res->status_code, res->headers ? res->headers : "", res->raw,
                              res->raw_len, res->body ? res->body : "", request->user_data);
        }
        // NOTE: We do NOT free `res` here. The job cleanup function is
        // responsible for freeing the JobResult so that results are also
        // reclaimed when callbacks are skipped during shutdown.
    }
}

// Cleanup that frees the HTTP request and detaches Lua proxy safely
static void _http_job_cleanup(ese_job_id_t job_id, void *user_data, void *result) {
    (void)job_id;
    EseHttpRequest *request = (EseHttpRequest *)user_data;

    if (request) {
        // If we created a Lua proxy ref, just unref it so Lua owns lifetime;
        // do NOT destroy here so Lua can still read fields after done=true.
        lua_State *L = ese_http_request_get_state(request);
        int ref = ese_http_request_get_lua_ref(request);
        if (L && ref != LUA_NOREF) {
            luaL_unref(L, LUA_REGISTRYINDEX, ref);
            _ese_http_request_set_lua_ref(request, LUA_NOREF);
            _ese_http_request_set_lua_ref_count(request, 0);
        }

        if (request && request->lua_state) {
            ese_http_request_unref(request);
        }
    }

    // Always free the JobResult produced by the worker thread on the main
    // thread. This runs whether or not the callback was invoked (e.g. during
    // shutdown when callbacks are skipped), ensuring no HTTP-tagged buffers
    // are leaked.
    if (result) {
        _http_result_free(result);
    }
}

// ========================================
// PUBLIC FUNCTIONS
// ========================================

// Core lifecycle
EseHttpRequest *ese_http_request_create(EseLuaEngine *engine, const char *url) {
    log_assert("HTTP", url != NULL, "URL cannot be NULL");
    log_debug("HTTP", "Creating HTTP request for URL: %s", url);

    EseHttpRequest *request = _ese_http_request_make();
    if (!request) {
        log_debug("HTTP", "Failed to allocate HTTP request structure");
        return NULL;
    }

    request->url = (char *)memory_manager.malloc(strlen(url) + 1, MMTAG_HTTP);
    if (!request->url) {
        log_debug("HTTP", "Failed to allocate URL string");
        ese_http_request_destroy(request);
        return NULL;
    }
    strcpy(request->url, url);

    if (!_http_parse_url(request)) {
        log_debug("HTTP", "Failed to parse URL: %s", url);
        ese_http_request_destroy(request);
        return NULL;
    }

    log_verbose("HTTP", "Parsed URL - Host: %s, Port: %s, Path: %s", request->host, request->port,
                request->path);

    return request;
}

void ese_http_request_destroy(EseHttpRequest *request) {
    if (!request) {
        log_debug("HTTP", "DESTROY: Called with NULL request");
        return;
    }

    log_debug("HTTP", "DESTROY: Destroying HTTP request for URL: %s",
              request->url ? request->url : "unknown");

    if (request->url) {
        memory_manager.free(request->url);
    }
    if (request->host) {
        memory_manager.free(request->host);
    }
    if (request->path) {
        memory_manager.free(request->path);
    }
    if (request->port) {
        memory_manager.free(request->port);
    }
    if (request->headers) {
        memory_manager.free(request->headers);
    }
    if (request->body) {
        memory_manager.free(request->body);
    }
    memory_manager.free(request);
}

size_t ese_http_request_sizeof(void) { return sizeof(EseHttpRequest); }

// Property access
const char *ese_http_request_get_url(const EseHttpRequest *request) {
    if (!request) {
        return NULL;
    }
    return request->url;
}

int ese_http_request_get_status(const EseHttpRequest *request) {
    if (!request) {
        return -1;
    }
    return request->status_code;
}

const char *ese_http_request_get_body(const EseHttpRequest *request) {
    if (!request) {
        return NULL;
    }
    return request->body;
}

const char *ese_http_request_get_headers(const EseHttpRequest *request) {
    if (!request) {
        return NULL;
    }
    return request->headers;
}

bool ese_http_request_is_done(const EseHttpRequest *request) {
    if (!request) {
        return false;
    }
    return request->done;
}

void ese_http_request_set_timeout(EseHttpRequest *request, long timeout_ms) {
    if (!request) {
        return;
    }
    request->timeout_ms = timeout_ms;
}

void ese_http_request_set_callback(EseHttpRequest *request, http_callback_t callback,
                                   void *user_data) {
    if (!request) {
        return;
    }
    request->callback = callback;
    request->user_data = user_data;
}

int ese_http_request_start(EseHttpRequest *request) {
    if (!request) {
        log_debug("HTTP", "Cannot start HTTP request: request is NULL");
        return -1;
    }

    log_debug("HTTP", "Starting HTTP request for URL: %s", request->url);

    // Obtain engine job queue from Lua state
    lua_State *L = ese_http_request_get_state(request);
    EseEngine *engine = NULL;
    EseJobQueue *queue = NULL;
    if (L) {
        engine = (EseEngine *)lua_engine_get_registry_key(L, ENGINE_KEY);
    }
    if (engine) {
        queue = engine_get_job_queue(engine);
    }
    if (!queue) {
        log_debug("HTTP", "Failed to obtain job queue; cannot start request");
        return -1;
    }

    ese_job_id_t job_id = ese_job_queue_push(queue, _http_worker_thread, _http_worker_callback,
                                             _http_job_cleanup, (void *)request);
    if (job_id == ESE_JOB_NOT_QUEUED) {
        log_debug("HTTP", "Failed to queue HTTP request job");
        return -1;
    }

    if (request->lua_state) {
        ese_http_request_ref(request); // increment lua_ref_count / store registry ref
    }

    log_verbose("HTTP", "HTTP request job queued successfully (job_id=%d)", (int)job_id);
    return 0;
}

// Lua-related access
lua_State *ese_http_request_get_state(const EseHttpRequest *request) {
    if (!request) {
        return NULL;
    }
    return request->lua_state;
}

int ese_http_request_get_lua_ref(const EseHttpRequest *request) {
    if (!request) {
        return LUA_NOREF;
    }
    return request->lua_ref;
}

int ese_http_request_get_lua_ref_count(const EseHttpRequest *request) {
    if (!request) {
        return 0;
    }
    return request->lua_ref_count;
}

void ese_http_request_set_state(EseHttpRequest *request, lua_State *state) {
    if (!request) {
        return;
    }
    request->lua_state = state;
}

// Internal setter functions for Lua fields
void _ese_http_request_set_lua_ref(EseHttpRequest *request, int ref) {
    if (!request) {
        return;
    }
    request->lua_ref = ref;
}

void _ese_http_request_set_lua_ref_count(EseHttpRequest *request, int count) {
    if (!request) {
        return;
    }
    request->lua_ref_count = count;
}

EseHttpRequest *_ese_http_request_make(void) {
    EseHttpRequest *request =
        (EseHttpRequest *)memory_manager.malloc(sizeof(EseHttpRequest), MMTAG_HTTP);
    if (!request) {
        return NULL;
    }

    request->url = NULL;
    request->host = NULL;
    request->path = NULL;
    request->port = NULL;
    request->timeout_ms = 10 * 1000; // 10 seconds default timeout
    request->callback = NULL;
    request->user_data = NULL;
    request->status_code = -1;
    request->headers = NULL;
    request->body = NULL;
    request->done = false;
    request->is_https = false;

    request->lua_state = NULL;
    request->lua_ref = LUA_NOREF;
    request->lua_ref_count = 0;

    return request;
}
