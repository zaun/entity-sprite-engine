/*
 * project: Entity Sprite Engine
 *
 * HTTP client implementation for making asynchronous HTTP GET requests using pthreads.
 * Provides a simple API for making HTTP requests with callbacks and timeouts.
 * 
 * Copyright (c) 2025-2026 Entity Sprite Engine
 * See LICENSE.md for details.
 */

#define _POSIX_C_SOURCE 200112L
#define ESE_HTTP_IMPLEMENTATION

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include "utility/job_queue.h"
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
#include "mbedtls/net_sockets.h"
#include "mbedtls/ssl.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/error.h"
#include "mbedtls/x509_crt.h"

#include "core/memory_manager.h"
#include "utility/log.h"
#include "types/http.h"
#include "scripting/lua_engine.h"
#include "core/engine.h"

// ========================================
// PRIVATE STRUCT DEFINITION
// ========================================

/**
 * @brief Internal structure for EseHttpRequest
 * 
 * @details Contains all information needed to make an HTTP GET request including
 * parsed URL components, timeout settings, callback, and user data.
 */
struct EseHttpRequest {
    char *url;              /** Original URL string */
    char *host;             /** Parsed hostname */
    char *path;             /** Parsed path */
    char *port;             /** Parsed port number */
    long timeout_ms;        /** Timeout in milliseconds */
    http_callback_t callback; /** Callback function */
    void *user_data;        /** User data pointer */
    
    // Response data
    int status_code;        /** HTTP status code, -1 if not completed */
    char *headers;          /** Response headers */
    char *body;             /** Response body */
    bool done;              /** Whether request is completed */
    
    // Redirect handling
    int redirect_count;     /** Number of redirects followed */
    int max_redirects;      /** Maximum number of redirects to follow */
    char **redirect_urls;   /** Array of URLs visited during redirects */
    
    // SSL/TLS support
    bool is_https;          /** Whether this is an HTTPS request */
    mbedtls_net_context server_fd; /** mbedTLS network context */
    mbedtls_ssl_context ssl;       /** mbedTLS SSL context */
    mbedtls_ssl_config conf;       /** mbedTLS SSL configuration */
    mbedtls_entropy_context entropy; /** mbedTLS entropy context */
    mbedtls_ctr_drbg_context ctr_drbg; /** mbedTLS CTR DRBG context */
    
    // Lua integration
    lua_State *lua_state;   /** Associated Lua state */
    int lua_ref;            /** Lua registry reference */
    int lua_ref_count;      /** Reference count for Lua GC */
};

// ========================================
// PRIVATE FORWARD DECLARATIONS
// ========================================

// URL parsing
static bool _http_parse_url(EseHttpRequest *request);
static bool _http_parse_redirect_url(EseHttpRequest *request, const char *location);

// Socket helpers
static int _http_connect_with_timeout(const char *host, const char *port, long timeout_ms, int *out_errno);
static int _http_recv_all_with_timeout(int fd, long timeout_ms, uint8_t **out_raw, size_t *out_len);
static int _http_parse_status_and_headers(const uint8_t *raw, size_t raw_len, int *status_out, char **headers_out, size_t *hdr_len_out);

// SSL/TLS helpers
static int _http_ssl_connect(EseHttpRequest *request);
static int _http_ssl_send(EseHttpRequest *request, const void *buf, size_t len);
static int _http_ssl_recv(EseHttpRequest *request, void *buf, size_t len);
static void _http_ssl_cleanup(EseHttpRequest *request);

// Redirect handling
static char *_http_extract_location_header(const char *headers);
static bool _http_is_redirect_status(int status_code);
static bool _http_should_follow_redirect(EseHttpRequest *request, int status_code);
static void _http_add_redirect_url(EseHttpRequest *request, const char *url);

// Threading / Job Queue
static void *_http_worker_thread(void *thread_data, void *user_data);
static void _http_job_noop_cleanup(ese_job_id_t job_id, void *user_data, void *result);

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
 * @brief Parses a redirect URL (Location header) and updates the request.
 * 
 * @details Handles both absolute URLs and relative URLs. For relative URLs,
 * constructs the full URL using the current request's host and port.
 * 
 * @param request The HTTP request to update
 * @param location The Location header value
 * @return true on success, false on parse error
 */
static bool _http_parse_redirect_url(EseHttpRequest *request, const char *location) {
    if (!location || strlen(location) == 0) {
        return false;
    }
    
    // If it's an absolute URL, parse it directly
    if (strncmp(location, "http://", 7) == 0 || strncmp(location, "https://", 8) == 0) {
        // Free old URL components
        if (request->host) {
            memory_manager.free(request->host);
            request->host = NULL;
        }
        if (request->path) {
            memory_manager.free(request->path);
            request->path = NULL;
        }
        if (request->port) {
            memory_manager.free(request->port);
            request->port = NULL;
        }
        
        // Update the URL
        if (request->url) {
            memory_manager.free(request->url);
        }
        request->url = (char *)memory_manager.malloc(strlen(location) + 1, MMTAG_HTTP);
        if (!request->url) {
            return false;
        }
        strcpy(request->url, location);
        
        return _http_parse_url(request);
    } else {
        // Relative URL - construct full URL
        char *full_url = (char *)memory_manager.malloc(strlen(request->host) + strlen(request->port) + strlen(location) + 20, MMTAG_HTTP);
        if (!full_url) {
            return false;
        }
        
        snprintf(full_url, strlen(request->host) + strlen(request->port) + strlen(location) + 20,
                "http://%s:%s%s", request->host, request->port, location);
        
        // Free old URL components
        if (request->host) {
            memory_manager.free(request->host);
            request->host = NULL;
        }
        if (request->path) {
            memory_manager.free(request->path);
            request->path = NULL;
        }
        if (request->port) {
            memory_manager.free(request->port);
            request->port = NULL;
        }
        
        // Update the URL
        if (request->url) {
            memory_manager.free(request->url);
        }
        request->url = full_url;
        
        return _http_parse_url(request);
    }
}

// Socket helpers
/**
 * @brief Connects to a host and port with timeout.
 * 
 * @details Attempts to connect to the specified host and port with the given timeout.
 * Uses non-blocking sockets with select() for timeout handling.
 * 
 * @param host Hostname to connect to
 * @param port Port number as string
 * @param timeout_ms Timeout in milliseconds
 * @param out_errno Pointer to store errno on failure
 * @return Socket file descriptor on success, -1 on error
 */
static int _http_connect_with_timeout(const char *host, const char *port, long timeout_ms, int *out_errno) {
    struct addrinfo hints;
    struct addrinfo *res = NULL;
    int sfd = -1;
    int gai_err = 0;
    
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    
    if ((gai_err = getaddrinfo(host, port, &hints, &res)) != 0) {
        if (out_errno) {
            *out_errno = EINVAL;
        }
        return -1;
    }
    
    struct addrinfo *rp;
    for (rp = res; rp != NULL; rp = rp->ai_next) {
        sfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (sfd < 0) {
            continue;
        }
        
        // Non-blocking connect with select timeout
        int flags = fcntl(sfd, F_GETFL, 0);
        if (flags < 0) {
            close(sfd);
            sfd = -1;
            continue;
        }
        
        if (fcntl(sfd, F_SETFL, flags | O_NONBLOCK) < 0) {
            close(sfd);
            sfd = -1;
            continue;
        }
        
        int rc = connect(sfd, rp->ai_addr, rp->ai_addrlen);
        if (rc == 0) {
            // Connected immediately, restore blocking
            fcntl(sfd, F_SETFL, flags);
            break;
        } else if (errno != EINPROGRESS) {
            close(sfd);
            sfd = -1;
            continue;
        }
        
        // Wait for socket writable or error
        fd_set wfds;
        FD_ZERO(&wfds);
        FD_SET(sfd, &wfds);
        
        struct timeval tv;
        struct timeval *tvp = NULL;
        if (timeout_ms > 0) {
            tv.tv_sec = timeout_ms / 1000;
            tv.tv_usec = (timeout_ms % 1000) * 1000;
            tvp = &tv;
        }
        
        int s = select(sfd + 1, NULL, &wfds, NULL, tvp);
        if (s > 0 && FD_ISSET(sfd, &wfds)) {
            int soerr = 0;
            socklen_t len = sizeof(soerr);
            if (getsockopt(sfd, SOL_SOCKET, SO_ERROR, &soerr, &len) < 0) {
                close(sfd);
                sfd = -1;
                continue;
            }
            
            if (soerr == 0) {
                // Success, restore blocking
                fcntl(sfd, F_SETFL, flags);
                break;
            } else {
                close(sfd);
                sfd = -1;
                if (out_errno) {
                    *out_errno = soerr;
                }
                continue;
            }
        } else {
            // Timeout or select error
            close(sfd);
            sfd = -1;
            if (out_errno) {
                *out_errno = (s == 0 ? ETIMEDOUT : errno);
            }
            continue;
        }
    }
    
    freeaddrinfo(res);
    return sfd;
}

/**
 * @brief Receives all data from socket with timeout.
 * 
 * @details Reads all available data from the socket until EOF or error.
 * The buffer grows as needed to accommodate the full response.
 * 
 * @param fd Socket file descriptor
 * @param timeout_ms Timeout in milliseconds
 * @param out_raw Pointer to store raw response data
 * @param out_len Pointer to store response length
 * @return 0 on success, negative on error
 */
static int _http_recv_all_with_timeout(int fd, long timeout_ms, uint8_t **out_raw, size_t *out_len) {
    const size_t chunk = 4096;
    uint8_t *buf = (uint8_t *)memory_manager.malloc(chunk, MMTAG_HTTP);
    if (!buf) {
        return -1;
    }
    
    size_t cap = chunk;
    size_t len = 0;
    
    // Set recv timeout if specified
    if (timeout_ms > 0) {
        struct timeval tv;
        tv.tv_sec = timeout_ms / 1000;
        tv.tv_usec = (timeout_ms % 1000) * 1000;
        setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    }
    
    while (1) {
        if (len + 1 >= cap) {
            size_t ncap = cap * 2;
            uint8_t *nb = (uint8_t *)memory_manager.realloc(buf, ncap, MMTAG_HTTP);
            if (!nb) {
                memory_manager.free(buf);
                return -1;
            }
            buf = nb;
            cap = ncap;
        }
        
        ssize_t r = recv(fd, buf + len, (ssize_t)(cap - len - 1), 0);
        if (r > 0) {
            len += (size_t)r;
            continue;
        } else if (r == 0) {
            break;
        } else {
            if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
                // Treat as timeout or temporary; break
                break;
            }
            memory_manager.free(buf);
            return -1;
        }
    }
    
    // Final NUL for safety when treating as string
    if (len + 1 > cap) {
        uint8_t *nb = (uint8_t *)memory_manager.realloc(buf, len + 1, MMTAG_HTTP);
        if (!nb) {
            memory_manager.free(buf);
            return -1;
        }
        buf = nb;
        cap = len + 1;
    }
    
    buf[len] = 0;
    *out_raw = buf;
    *out_len = len;
    return 0;
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
static int _http_parse_status_and_headers(const uint8_t *raw, size_t raw_len, int *status_out, char **headers_out, size_t *hdr_len_out) {
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

// SSL/TLS helpers
/**
 * @brief Establishes an SSL/TLS connection using mbedTLS.
 * 
 * @details Initializes mbedTLS contexts, sets up SSL configuration,
 * and performs the SSL handshake.
 * 
 * @param request The HTTP request with SSL contexts initialized
 * @return 0 on success, negative mbedTLS error code on failure
 */
static int _http_ssl_connect(EseHttpRequest *request) {
    int ret;
    char port_str[6];
    
    // Convert port to string
    snprintf(port_str, sizeof(port_str), "%s", request->port);
    
    // Initialize the RNG and the session data
    mbedtls_entropy_init(&request->entropy);
    if ((ret = mbedtls_ctr_drbg_seed(&request->ctr_drbg, mbedtls_entropy_func, &request->entropy,
                                     (const unsigned char *)"ese_http_client", 15)) != 0) {
        log_debug("HTTP", "mbedtls_ctr_drbg_seed returned %d", ret);
        return ret;
    }
    
    // Initialize the SSL context
    mbedtls_ssl_init(&request->ssl);
    mbedtls_ssl_config_init(&request->conf);
    
    // Load defaults
    if ((ret = mbedtls_ssl_config_defaults(&request->conf,
                                          MBEDTLS_SSL_IS_CLIENT,
                                          MBEDTLS_SSL_TRANSPORT_STREAM,
                                          MBEDTLS_SSL_PRESET_DEFAULT)) != 0) {
        log_debug("HTTP", "mbedtls_ssl_config_defaults returned %d", ret);
        return ret;
    }
    
    // Set the RNG callback
    mbedtls_ssl_conf_rng(&request->conf, mbedtls_ctr_drbg_random, &request->ctr_drbg);
    
    // Set auth mode to none for testing (in production, use MBEDTLS_SSL_VERIFY_REQUIRED)
    mbedtls_ssl_conf_authmode(&request->conf, MBEDTLS_SSL_VERIFY_NONE);
    
    // Set TLS version to 1.2 for better compatibility
    mbedtls_ssl_conf_min_version(&request->conf, MBEDTLS_SSL_MAJOR_VERSION_3, MBEDTLS_SSL_MINOR_VERSION_3);
    mbedtls_ssl_conf_max_version(&request->conf, MBEDTLS_SSL_MAJOR_VERSION_3, MBEDTLS_SSL_MINOR_VERSION_3);
    
    // Enable session tickets for better compatibility
    mbedtls_ssl_conf_session_tickets(&request->conf, MBEDTLS_SSL_SESSION_TICKETS_ENABLED);
    
    // Set renegotiation to prevent issues
    mbedtls_ssl_conf_renegotiation(&request->conf, MBEDTLS_SSL_RENEGOTIATION_DISABLED);
    
    // Use default cipher suites for better compatibility
    // mbedTLS will negotiate the best available cipher suite
    
    // Set hostname for SNI
    if ((ret = mbedtls_ssl_set_hostname(&request->ssl, request->host)) != 0) {
        log_debug("HTTP", "mbedtls_ssl_set_hostname returned %d", ret);
        return ret;
    }
    
    // Set up the SSL context
    if ((ret = mbedtls_ssl_setup(&request->ssl, &request->conf)) != 0) {
        log_debug("HTTP", "mbedtls_ssl_setup returned %d", ret);
        return ret;
    }
    
    // Connect to the server
    log_debug("HTTP", "Connecting to %s:%s", request->host, port_str);
    if ((ret = mbedtls_net_connect(&request->server_fd, request->host, port_str,
                                   MBEDTLS_NET_PROTO_TCP)) != 0) {
        char error_buf[256];
        mbedtls_strerror(ret, error_buf, sizeof(error_buf));
        log_debug("HTTP", "mbedtls_net_connect returned %d: %s", ret, error_buf);
        return ret;
    }
    log_debug("HTTP", "TCP connection established");
    
    // Set the underlying I/O callbacks
    mbedtls_ssl_set_bio(&request->ssl, &request->server_fd, mbedtls_net_send, mbedtls_net_recv, NULL);
    
    // Perform the SSL/TLS handshake
    while ((ret = mbedtls_ssl_handshake(&request->ssl)) != 0) {
        if (ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE) {
            char error_buf[256];
            mbedtls_strerror(ret, error_buf, sizeof(error_buf));
            log_debug("HTTP", "mbedtls_ssl_handshake returned %d: %s", ret, error_buf);
            return ret;
        }
    }
    
    // Verify the server certificate
    uint32_t flags = mbedtls_ssl_get_verify_result(&request->ssl);
    if (flags != 0) {
        char vrfy_buf[512];
        mbedtls_x509_crt_verify_info(vrfy_buf, sizeof(vrfy_buf), "  ! ", flags);
        log_debug("HTTP", "Certificate verification failed: %s", vrfy_buf);
        // Note: We continue anyway for now, but in production you might want to fail
    }
    
    log_verbose("HTTP", "SSL/TLS connection established successfully");
    return 0;
}

/**
 * @brief Sends data over the SSL/TLS connection.
 * 
 * @param request The HTTP request with active SSL context
 * @param buf Data to send
 * @param len Length of data to send
 * @return Number of bytes sent, or negative mbedTLS error code
 */
static int _http_ssl_send(EseHttpRequest *request, const void *buf, size_t len) {
    int ret = mbedtls_ssl_write(&request->ssl, (const unsigned char *)buf, len);
    if (ret < 0) {
        log_debug("HTTP", "mbedtls_ssl_write returned %d", ret);
    }
    return ret;
}

/**
 * @brief Receives data from the SSL/TLS connection.
 * 
 * @param request The HTTP request with active SSL context
 * @param buf Buffer to receive data
 * @param len Maximum length to receive
 * @return Number of bytes received, or negative mbedTLS error code
 */
static int _http_ssl_recv(EseHttpRequest *request, void *buf, size_t len) {
    int ret = mbedtls_ssl_read(&request->ssl, (unsigned char *)buf, len);
    if (ret < 0 && ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE) {
        log_debug("HTTP", "mbedtls_ssl_read returned %d", ret);
    }
    return ret;
}

/**
 * @brief Cleans up SSL/TLS resources.
 * 
 * @param request The HTTP request to clean up
 */
static void _http_ssl_cleanup(EseHttpRequest *request) {
    if (request && request->is_https) {
        log_debug("HTTP", "SSL_CLEANUP: Cleaning up SSL for URL: %s", request->url ? request->url : "unknown");
        mbedtls_ssl_free(&request->ssl);
        mbedtls_ssl_config_free(&request->conf);
        mbedtls_net_free(&request->server_fd);
        mbedtls_ctr_drbg_free(&request->ctr_drbg);
        mbedtls_entropy_free(&request->entropy);
        // Mark as cleaned up to prevent double-cleanup
        request->is_https = false;
        log_debug("HTTP", "SSL_CLEANUP: SSL cleanup completed");
    } else {
        log_debug("HTTP", "SSL_CLEANUP: Skipping SSL cleanup (request=%p, is_https=%s)", 
                 request, request ? (request->is_https ? "true" : "false") : "N/A");
    }
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
            if (!p) break;
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
    while (end > location && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\r' || end[-1] == '\n')) {
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
 * @param request The HTTP request
 * @param status_code The HTTP status code
 * @return true if the redirect should be followed
 */
static bool _http_should_follow_redirect(EseHttpRequest *request, int status_code) {
    if (!_http_is_redirect_status(status_code)) {
        return false;
    }
    
    if (request->redirect_count >= request->max_redirects) {
        log_debug("HTTP", "Maximum redirects (%d) exceeded", request->max_redirects);
        return false;
    }
    
    return true;
}

/**
 * @brief Adds a URL to the redirect history.
 * 
 * @details Tracks URLs visited during redirects to detect redirect loops.
 * 
 * @param request The HTTP request
 * @param url The URL to add to the history
 */
static void _http_add_redirect_url(EseHttpRequest *request, const char *url) {
    if (!url) {
        return;
    }
    
    // Reallocate the redirect URLs array
    char **new_urls = (char **)memory_manager.realloc(request->redirect_urls, 
                                                     (request->redirect_count + 1) * sizeof(char *), 
                                                     MMTAG_HTTP);
    if (!new_urls) {
        return;
    }
    
    request->redirect_urls = new_urls;
    
    // Allocate and copy the URL
    request->redirect_urls[request->redirect_count] = (char *)memory_manager.malloc(strlen(url) + 1, MMTAG_HTTP);
    if (request->redirect_urls[request->redirect_count]) {
        strcpy(request->redirect_urls[request->redirect_count], url);
        request->redirect_count++;
    }
}

// Threading
/**
 * @brief Worker thread function for HTTP requests.
 * 
 * @details Executes the HTTP request on a background thread. Handles connection,
 * request sending, response receiving, parsing, and callback invocation.
 * 
 * @param arg Pointer to EseHttpRequest structure
 * @return NULL (thread return value)
 */
static void *_http_worker_thread(void *thread_data, void *user_data) {
    (void)thread_data;
    EseHttpRequest *request = (EseHttpRequest *)user_data;
    int status_code = -1;
    char *headers = NULL;
    uint8_t *raw = NULL;
    size_t raw_len = 0;
    char *body_cstr = NULL;
    
    log_debug("HTTP", "Worker thread started for URL: %s", request->url);
    
    // Add the initial URL to redirect history
    _http_add_redirect_url(request, request->url);
    
    while (true) {
        // Clean up previous iteration
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
        
        int sock_errno = 0;
        int sfd = -1;
        
        if (request->is_https) {
            // Use mbedTLS for HTTPS
            int ssl_ret = _http_ssl_connect(request);
            if (ssl_ret != 0) {
                log_debug("HTTP", "SSL connection failed for %s:%s (error: %d), falling back to HTTP", request->host, request->port, ssl_ret);
                // Fall back to HTTP by changing the port and disabling SSL
                memory_manager.free(request->port);
                request->port = (char *)memory_manager.malloc(3, MMTAG_HTTP);
                if (request->port) {
                    strcpy(request->port, "80");
                }
                request->is_https = false;
                
                // Try HTTP connection instead
                sfd = _http_connect_with_timeout(request->host, request->port, request->timeout_ms, &sock_errno);
                if (sfd < 0) {
                    log_debug("HTTP", "HTTP fallback also failed for %s:%s (errno: %d)", request->host, request->port, sock_errno);
                    request->status_code = -1;
                    request->done = true;
                    if (request->callback) {
                        request->callback(-1, "", NULL, 0, "", request->user_data);
                    }
                    return NULL;
                }
                log_verbose("HTTP", "HTTP fallback connected to %s:%s", request->host, request->port);
            } else {
                log_verbose("HTTP", "SSL/TLS connected to %s:%s", request->host, request->port);
            }
        } else {
            // Use regular socket for HTTP
            sfd = _http_connect_with_timeout(request->host, request->port, request->timeout_ms, &sock_errno);
            if (sfd < 0) {
                // Connect failed
                log_debug("HTTP", "Connection failed for %s:%s (errno: %d)", request->host, request->port, sock_errno);
                request->status_code = -1;
                request->done = true;
                if (request->callback) {
                    request->callback(-1, "", NULL, 0, "", request->user_data);
                }
                return NULL;
            }
            log_verbose("HTTP", "Connected to %s:%s", request->host, request->port);
        }
        
        // Set send timeout as well if requested (only for HTTP)
        if (!request->is_https && request->timeout_ms > 0) {
            struct timeval tv;
            tv.tv_sec = request->timeout_ms / 1000;
            tv.tv_usec = (request->timeout_ms % 1000) * 1000;
            setsockopt(sfd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
        }
        
        // Prepare request
        char reqbuf[4096];
        int n = snprintf(reqbuf, sizeof(reqbuf),
                         "GET %s HTTP/1.1\r\n"
                         "Host: %s\r\n"
                         "Connection: close\r\n"
                         "User-Agent: Entity-Sprite-Engine/1.0\r\n"
                         "\r\n",
                         request->path ? request->path : "/", 
                         request->host ? request->host : "");
        
        if (n < 0 || (size_t)n >= sizeof(reqbuf)) {
            if (!request->is_https) {
                close(sfd);
            }
            request->status_code = -1;
            request->done = true;
            if (request->callback) {
                request->callback(-1, "", NULL, 0, "", request->user_data);
            }
            return NULL;
        }
        
        ssize_t w;
        if (request->is_https) {
            w = _http_ssl_send(request, reqbuf, (size_t)n);
        } else {
            w = write(sfd, reqbuf, (size_t)n);
        }
        
        if (w < 0) {
            if (!request->is_https) {
                close(sfd);
            }
            request->status_code = -1;
            request->done = true;
            if (request->callback) {
                request->callback(-1, "", NULL, 0, "", request->user_data);
            }
            return NULL;
        }
        
        // Receive
        if (request->is_https) {
            // Use SSL/TLS receive
            const size_t chunk = 4096;
            uint8_t *buf = (uint8_t *)memory_manager.malloc(chunk, MMTAG_HTTP);
            if (!buf) {
                request->status_code = -1;
                request->done = true;
                if (request->callback) {
                    request->callback(-1, "", NULL, 0, "", request->user_data);
                }
                return NULL;
            }
            
            size_t cap = chunk;
            size_t len = 0;
            
            while (1) {
                if (len + 1 >= cap) {
                    size_t ncap = cap * 2;
                    uint8_t *nb = (uint8_t *)memory_manager.realloc(buf, ncap, MMTAG_HTTP);
                    if (!nb) {
                        memory_manager.free(buf);
                        request->status_code = -1;
                        request->done = true;
                        if (request->callback) {
                            request->callback(-1, "", NULL, 0, "", request->user_data);
                        }
                        return NULL;
                    }
                    buf = nb;
                    cap = ncap;
                }
                
                int r = _http_ssl_recv(request, buf + len, cap - len - 1);
                if (r > 0) {
                    len += (size_t)r;
                    continue;
                } else if (r == 0) {
                    break;
                } else if (r == MBEDTLS_ERR_SSL_WANT_READ || r == MBEDTLS_ERR_SSL_WANT_WRITE) {
                    continue;
                } else if (r == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY) {
                    // Server closed the connection gracefully - this is normal
                    break;
                } else {
                    char error_buf[256];
                    mbedtls_strerror(r, error_buf, sizeof(error_buf));
                    log_debug("HTTP", "SSL read error: %d (%s)", r, error_buf);
                    // Assign buf to raw so it gets freed properly
                    raw = buf;
                    raw_len = 0;
                    request->status_code = -1;
                    request->done = true;
                    if (request->callback) {
                        request->callback(-1, "", NULL, 0, "", request->user_data);
                    }
                    return NULL;
                }
            }
            
            // Final NUL for safety when treating as string
            if (len + 1 > cap) {
                uint8_t *nb = (uint8_t *)memory_manager.realloc(buf, len + 1, MMTAG_HTTP);
                if (!nb) {
                    memory_manager.free(buf);
                    request->status_code = -1;
                    request->done = true;
                    if (request->callback) {
                        request->callback(-1, "", NULL, 0, "", request->user_data);
                    }
                    return NULL;
                }
                buf = nb;
                cap = len + 1;
            }
            
            buf[len] = 0;
            raw = buf;
            raw_len = len;
            // Note: raw now points to buf, so we need to free raw at the end
        } else {
            // Use regular socket receive
            if (_http_recv_all_with_timeout(sfd, request->timeout_ms, &raw, &raw_len) != 0) {
                close(sfd);
                if (raw) {
                    memory_manager.free(raw);
                }
                request->status_code = -1;
                request->done = true;
                if (request->callback) {
                    request->callback(-1, "", NULL, 0, "", request->user_data);
                }
                return NULL;
            }
            close(sfd);
        }
        
        // Parse status and headers
        if (_http_parse_status_and_headers(raw, raw_len, &status_code, &headers, NULL) != 0) {
            // Parsing failed; still deliver raw
            request->status_code = -1;
            request->done = true;
            if (request->callback) {
                request->callback(-1, "", raw, raw_len, (const char *)(raw ? (const char *)raw : ""), request->user_data);
            }
            if (headers) {
                memory_manager.free(headers);
            }
            if (raw) {
                memory_manager.free(raw);
            }
            return NULL;
        }
        
        // Check if this is a redirect
        if (_http_should_follow_redirect(request, status_code)) {
            char *location = _http_extract_location_header(headers);
            if (location) {
                log_debug("HTTP", "Following redirect %d to: %s", request->redirect_count, location);
                
                // Check for redirect loop
                bool is_loop = false;
                for (int i = 0; i < request->redirect_count; i++) {
                    if (request->redirect_urls[i] && strcmp(request->redirect_urls[i], location) == 0) {
                        log_debug("HTTP", "Redirect loop detected at: %s", location);
                        is_loop = true;
                        break;
                    }
                }
                
                if (is_loop) {
                    memory_manager.free(location);
                    request->status_code = status_code;
                    request->headers = headers;
                    request->body = (char *)memory_manager.malloc(1, MMTAG_HTTP);
                    if (request->body) {
                        request->body[0] = '\0';
                    }
                    request->done = true;
                    if (request->callback) {
                        request->callback(status_code, headers ? headers : "", raw, raw_len, "", request->user_data);
                    }
                    if (raw) {
                        memory_manager.free(raw);
                    }
                    return NULL;
                }
                
                // Parse the redirect URL and update the request
                if (_http_parse_redirect_url(request, location)) {
                    _http_add_redirect_url(request, location);
                    memory_manager.free(location);
                    continue; // Follow the redirect
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
        
        // Not a redirect or redirect handling failed - process as final response
        break;
    }
    
    // Find body start: locate "\r\n\r\n"
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
    
    // Store results in request
    request->status_code = status_code;
    request->headers = headers;
    request->body = body_cstr;
    request->done = true;
    
    log_debug("HTTP", "HTTP request completed with status %d for URL: %s", status_code, request->url);
    log_verbose("HTTP", "Response body length: %zu bytes", body_cstr ? strlen(body_cstr) : 0);
    
    if (request->callback) {
        log_verbose("HTTP", "Invoking callback for completed request");
        request->callback(status_code, headers ? headers : "", raw, raw_len, body_cstr ? body_cstr : "", request->user_data);
    }
    
    // Note: We no longer free raw here when running under job queue; the job returns it
    return raw;
}

// Job queue cleanup: free raw buffer returned by worker (if any)
static void _http_job_noop_cleanup(ese_job_id_t job_id, void *user_data, void *result) {
    (void)job_id;
    (void)user_data;
    if (result) {
        memory_manager.free(result);
    }
}

// Cleanup that frees the HTTP request and detaches Lua proxy safely
static void _http_job_cleanup(ese_job_id_t job_id, void *user_data, void *result) {
    (void)job_id;
    EseHttpRequest *request = (EseHttpRequest *)user_data;

    if (result) {
        memory_manager.free(result);
    }

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
        return NULL;
    }
    strcpy(request->url, url);
    
    if (!_http_parse_url(request)) {
        log_debug("HTTP", "Failed to parse URL: %s", url);
        return NULL;
    }
    
    log_verbose("HTTP", "Parsed URL - Host: %s, Port: %s, Path: %s", 
                request->host, request->port, request->path);
    
    return request;
}

void ese_http_request_destroy(EseHttpRequest *request) {
    if (!request) {
        log_debug("HTTP", "DESTROY: Called with NULL request");
        return;
    }
    
    log_debug("HTTP", "DESTROY: Destroying HTTP request for URL: %s", request->url ? request->url : "unknown");
    
    // Clean up SSL/TLS resources
    _http_ssl_cleanup(request);
    
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
    if (request->redirect_urls) {
        for (int i = 0; i < request->redirect_count; i++) {
            if (request->redirect_urls[i]) {
                memory_manager.free(request->redirect_urls[i]);
            }
        }
        memory_manager.free(request->redirect_urls);
    }
    memory_manager.free(request);
}

size_t ese_http_request_sizeof(void) {
    return sizeof(EseHttpRequest);
}

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

void ese_http_request_set_callback(EseHttpRequest *request, http_callback_t callback, void *user_data) {
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

    ese_job_id_t job_id = ese_job_queue_push(
        queue,
        _http_worker_thread,
        NULL, // no main-thread callback; HTTP worker invokes user's callback directly
        _http_job_cleanup,
        (void *)request
    );
    if (job_id == ESE_JOB_NOT_QUEUED) {
        log_debug("HTTP", "Failed to queue HTTP request job");
        return -1;
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
    EseHttpRequest *request = (EseHttpRequest *)memory_manager.malloc(sizeof(EseHttpRequest), MMTAG_HTTP);
    if (!request) {
        return NULL;
    }
    
    request->url = NULL;
    request->host = NULL;
    request->path = NULL;
    request->port = NULL;
    request->timeout_ms = 0;
    request->callback = NULL;
    request->user_data = NULL;
    request->status_code = -1;
    request->headers = NULL;
    request->body = NULL;
    request->done = false;
    request->redirect_count = 0;
    request->max_redirects = 10; // Default maximum of 10 redirects
    request->redirect_urls = NULL;
    request->is_https = false;
    
    // Initialize mbedTLS contexts
    mbedtls_net_init(&request->server_fd);
    mbedtls_ssl_init(&request->ssl);
    mbedtls_ssl_config_init(&request->conf);
    mbedtls_entropy_init(&request->entropy);
    mbedtls_ctr_drbg_init(&request->ctr_drbg);
    
    request->lua_state = NULL;
    request->lua_ref = LUA_NOREF;
    request->lua_ref_count = 0;
    
    return request;
}
