// SPDX-License-Identifier: GPL-2.0-only

#include <stddef.h>
#include <stdbool.h>
#include <poll.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/un.h>
#include <arpa/inet.h>

#include <sys/stat.h>
#include <sys/un.h>
#include <grp.h>

#include "libhttp/parser.h"
#include "libhttp/httpd.h"
#include "libutils/common.h"
#include "libutils/buf.h"

#include "log.h"

#define STATIC_ARRAY_SIZE(a) (sizeof(a) / sizeof(*(a)))

#define BUFFER_SIZE      4096
#define REQUEST_MAX_SIZE 65536
#define HEADER_SIZE      32

typedef enum {
    HTTPD_CLIENT_STATE_READ_REQUEST,
    HTTPD_CLIENT_STATE_READ_DATA,
    HTTPD_CLIENT_STATE_DONE
} httpd_client_state_t;

struct httpd_client_s {
    union {
        struct sockaddr_un su;
        struct sockaddr_in in;
        struct sockaddr_in6 in6;
    };
    httpd_client_state_t state;
    int fd;
    http_parse_header_t headers[HEADER_SIZE];
    http_parse_request_t request;
    size_t offset_data;
    ssize_t content_length;
    size_t bin_last_len;
    buf_t bin;
    buf_t bout;
};

struct httpd_listen_s {
    int num;
    int *fds;
};

struct httpd_s {
    int timeout;

    bool run;

    int listeners;

    size_t npfds_max;
    struct pollfd *pfds;

    size_t nclients;
    httpd_client_t **clients;
};

#define MAX_VALUE_TO_MULTIPLY ((LLONG_MAX / 10) + (LLONG_MAX % 10))
static ssize_t parse_integer(const char *number, size_t length)
{
    ssize_t sign = 1;
    const unsigned char *start = (const unsigned char *)number;
    const unsigned char *pos = (const unsigned char *)number;

    if (*pos == '-') {
        pos++;
        sign = -1;
    }

    if (*pos == '+')
        pos++;

    ssize_t ret  = 0;
    while (pos < start + length) {
        if (ret > MAX_VALUE_TO_MULTIPLY)  {
            errno = ERANGE;
            return sign == 1 ? LLONG_MAX : LLONG_MIN;
        }
        ret *= 10;
        if (LLONG_MAX - ret < (*pos - '0')) {
            errno = ERANGE;
            return sign == 1 ? LLONG_MAX : LLONG_MIN;
        }
        if (*pos < '0' || *pos > '9') {
            errno = ERANGE;
            return sign == 1 ? LLONG_MAX : LLONG_MIN;
        }
        ret += (*pos++ - '0');
    }

    return sign * ret;
}

httpd_listen_t *httpd_listen_init(void)
{
    httpd_listen_t *httpd_listen = calloc(1, sizeof(*httpd_listen));
    if (httpd_listen == NULL) {
// FIXME ERROR
        return NULL;
    }

    return httpd_listen;
}

void httpd_listen_free(httpd_listen_t *httpd_listen)
{
    if (httpd_listen == NULL)
        return;

    for (int i = 0; i < httpd_listen->num; i++) {
        close(httpd_listen->fds[i]);
    }

    free(httpd_listen->fds);
    free(httpd_listen);
}

static void httpd_client_free(httpd_client_t *client)
{
    if (client == NULL)
        return;

    buf_destroy(&client->bin);
    buf_destroy(&client->bout);
    free(client);
}

static httpd_client_t *httpd_client_alloc(int fd)
{
    httpd_client_t *client = calloc(1, sizeof(httpd_client_t));
    if (client == NULL)
        return NULL;

    client->fd = fd;
    client->state = HTTPD_CLIENT_STATE_READ_REQUEST;
    client->content_length = -1;
    client->request.headers = client->headers;
    client->request.num_headers = STATIC_ARRAY_SIZE(client->headers);

    int status = buf_resize(&client->bin, BUFFER_SIZE);
    if (status != 0) {
        httpd_client_free(client);
        return NULL;
    }

    status = buf_resize(&client->bout, BUFFER_SIZE);
    if (status != 0) {
        httpd_client_free(client);
        return NULL;
    }

    return client;
}

static ssize_t httpd_client_get_content_length(httpd_client_t *client)
{
    for (size_t i = 0; i < client->request.num_headers; i++) {
        if (client->request.headers[i].header == HTTP_HEADER_CONTENT_LENGTH) {
            return parse_integer(client->request.headers[i].value,
                                 client->request.headers[i].value_len);
        }
    }
    return -1;
}

ssize_t httpd_write(int fd,  buf_t *buf)
{
    size_t len = buf_len(buf);
    ssize_t wsize = write(fd, buf->ptr, len);

    return wsize;
}

int httpd_response(httpd_client_t *client, http_version_t version, http_status_code_t status_code,
                   http_header_set_t *headers, void *content, size_t content_length)
{
fprintf(stderr, "httpd_response 1\n");
    const char *sversion = http_get_version(version);
    if (sversion == NULL)
       return -1;

    const int nstatus_code = http_get_status(status_code);
fprintf(stderr, "httpd_response status_code: %d\n", nstatus_code);

    const char *status_reason = http_get_status_reason(status_code);
    if (status_reason == NULL)
       return -1;
fprintf(stderr, "httpd_response 3\n");

    int status = 0;
    status |= buf_put(&client->bout, sversion, strlen(sversion));
    status |= buf_putchar(&client->bout, ' ');
    status |= buf_putitoa(&client->bout, nstatus_code);
    status |= buf_putchar(&client->bout, ' ');
    status |= buf_put(&client->bout, status_reason, strlen(status_reason));
    status |= buf_put(&client->bout, "\r\n", 2);

    if (headers != NULL) {
        for (size_t i=0; i < headers->num; i++) {
            const char *header = http_get_header(headers->ptr[i].header_name);
            if (header != NULL) {
                status |= buf_put(&client->bout, header, strlen(header));
                status |= buf_put(&client->bout, ": ", 2);
                status |= buf_put(&client->bout, headers->ptr[i].value,
                                                 strlen(headers->ptr[i].value));
                status |= buf_put(&client->bout, "\r\n", 2);
            }
        }
    }

    const char *header = http_get_header(HTTP_HEADER_CONTENT_LENGTH);
    if (header != NULL) {
        status |= buf_put(&client->bout, header, strlen(header));
        status |= buf_put(&client->bout, ": ", 2);
        status |= buf_putitoa(&client->bout, content_length);
        status |= buf_put(&client->bout, "\r\n", 2);
    }

    status |= buf_put(&client->bout, "\r\n", 2);

    if ((content != NULL) && (content_length > 0))
        status |= buf_put(&client->bout, content, content_length);

fprintf(stderr, "httpd_response 4\n");
    httpd_write(client->fd, &client->bout);

    return status;
}

ssize_t httpd_read(int fd, buf_t *buf, size_t min_size)
{
    ssize_t size = 0;
    while (true) {
        fprintf(stderr, "httpd_read: %d\n", (int)size);
        if (buf_avail(buf) < min_size) {
            if (buf_resize(buf, min_size) != 0)
                return -1;
        }

        size_t avail = buf_avail(buf);
        ssize_t rsize = read(fd, buf->ptr + buf->pos, avail);
        fprintf(stderr, "httpd_read: read: %d\n", (int)rsize);
        if (rsize < 0) {
            if (errno == EINTR)
                continue;
        // if ((errno == EAGAIN) || (errno == EWOULDBLOCK)) {
            if (errno == EAGAIN) {
        // XXX
            }
            return rsize;
        } else if (rsize == 0) {
            return size;
        }

        buf->pos += rsize;
        size += rsize;

        return size;
    }

    return size;
}

int httpd_client_read(httpd_client_t *client)
{
fprintf(stderr, "httpd_client_read: %u\n", client->state);
    switch (client->state) {
    case HTTPD_CLIENT_STATE_READ_REQUEST: {
        ssize_t rsize = httpd_read(client->fd, &client->bin, BUFFER_SIZE);
        if (rsize <= 0)
            return -1;
/* returns number of bytes consumed if successful, -2 if request is partial, -1 if failed */
// const char *buf, size_t len, size_t last_len);
        int status = http_parse_request(&client->request, (char *)client->bin.ptr,
                                        buf_len(&client->bin), client->bin_last_len);
 fprintf(stderr, "httpd_client_read:  http_parse_request %d\n", status);
        client->bin_last_len = buf_len(&client->bin);
        if (status > 0) {
            client->content_length = httpd_client_get_content_length(client);
            if (client->content_length > 0)
                client->state = HTTPD_CLIENT_STATE_READ_DATA;
            else
                client->state = HTTPD_CLIENT_STATE_DONE;
            client->offset_data = status;
            return status;
//            break; /* successfully parsed the request */
        } else if (status == -1) {
//            return ParseError;
            return -1;
        }
        /* request is incomplete, continue the loop */
//        assert(pret == -2);
        if (buf_len(&client->bin) >= REQUEST_MAX_SIZE)
            return -1; // RequestIsTooLongError;
    }
        break;
    case HTTPD_CLIENT_STATE_READ_DATA: {
fprintf(stderr, "httpd_client_read:  HTTPD_CLIENT_STATE_READ_DATA\n");
        ssize_t rsize = httpd_read(client->fd, &client->bin, BUFFER_SIZE);
        if (rsize <= 0) {
            client->state = HTTPD_CLIENT_STATE_DONE;
            return -1;
        } else if (client->content_length >= (ssize_t)(buf_len(&client->bin) - client->offset_data)) {
            client->state = HTTPD_CLIENT_STATE_DONE;
            return 0;
        }
fprintf(stderr, "httpd_client_read:  HTTPD_CLIENT_STATE_READ_DATA :%d\n", (int)rsize);
    }   break;
    case HTTPD_CLIENT_STATE_DONE:
        break;
    }
    return 0;
}

int httpd_loop(httpd_t *httpd, httpd_request_t request_cb)
{
    while (httpd->run) {
        int result = poll(httpd->pfds, httpd->nclients+1, httpd->timeout);
fprintf(stderr, "httpd_loop: poll result: %d\n", result);

        if (result > 0) {
            for (int i = 0; i < httpd->listeners; i++) {
                if (httpd->pfds[i].revents & POLLIN) {
                    struct sockaddr_in cliaddr;
                    socklen_t addrlen = sizeof(cliaddr);

                    int client_socket = accept(httpd->pfds[i].fd, (struct sockaddr *)&cliaddr, &addrlen);
fprintf(stderr, "httpd_loop: accept %d\n", client_socket);
printf("accept success %s\n", inet_ntoa(cliaddr.sin_addr));

                    size_t j;
                    for (j = httpd->listeners; j < httpd->npfds_max; j++) {
                        if (httpd->pfds[j].fd == -1) {
                            int flags = fcntl (client_socket, F_GETFL, 0);
                            if (flags == -1)
                                flags = 0;
                            int status = fcntl (client_socket, F_SETFL, flags | O_NONBLOCK);
                            if (status < 0) {

                            }
                            httpd->clients[j] = httpd_client_alloc(client_socket);
                            if (httpd->clients[j] != NULL) {
 fprintf(stderr, "httpd_loop: httpd_client_alloc \n" );
                                httpd->pfds[j].fd = client_socket;
                                httpd->pfds[j].events = POLLIN | POLLPRI;
                                httpd->nclients++;
                            }
                            break;
                        }
                    }
                    if (j == httpd->npfds_max) {
                        if (client_socket >= 0)
                            close(client_socket);
                    }
                }
            }
            for (size_t i = httpd->listeners; i < httpd->npfds_max; i++) {
                if ((httpd->pfds[i].fd > 0) && (httpd->pfds[i].revents & POLLIN)) {
                    int status = httpd_client_read(httpd->clients[i]);
 fprintf(stderr, "httpd_loop: httpd_client_read %d \n", status );
                    if (status == -1) {
                        close(httpd->pfds[i].fd);
                        httpd->pfds[i].fd = -1;
                        httpd->pfds[i].events = 0;
                        httpd->pfds[i].revents = 0;
                        httpd_client_free(httpd->clients[i]);
                        httpd->clients[i] = NULL;
                        httpd->nclients--;
#if 0
                    } else if (status == 0) {
                        close(httpd->pfds[i].fd);
                        httpd->pfds[i].fd = -1;
                        httpd->pfds[i].events = 0;
                        httpd->pfds[i].revents = 0;
                        httpd_client_free(httpd->clients[i]);
                        httpd->clients[i] = NULL;
                        httpd->nclients--;
#endif
                    } else {
                        if (httpd->clients[i]->state == HTTPD_CLIENT_STATE_DONE) {


                            if (request_cb != NULL) {
                                httpd_client_t *client = httpd->clients[i];
#if  0
                                if (client->request.path_len > 0){
                                    client->request.path[client->request.path_len-1] = '\0';
                                } else {
                                    client->request.path = NULL;
                                }
#endif
                                if (client->content_length > 0) {
                                    request_cb(client, client->request.http_version,
                                               client->request.http_method,
                                               (const char *)client->bin.ptr + client->request.path_offset,
                                               client->request.path_len,
                                               NULL,
                                               client->bin.ptr + client->offset_data,
                                               client->content_length);
                                } else {
                                    request_cb(client, client->request.http_version,
                                               client->request.http_method,
                                               (const char *)client->bin.ptr + client->request.path_offset,
                                               client->request.path_len,
                                               NULL, NULL, 0);
                                }
fprintf(stderr, "httpd_client_t request_cb end\n");
                            }

                            close(httpd->pfds[i].fd);
                            httpd->pfds[i].fd = -1;
                            httpd->pfds[i].events = 0;
                            httpd->pfds[i].revents = 0;
                            httpd_client_free(httpd->clients[i]);
                            httpd->clients[i] = NULL;
                            httpd->nclients--;
                        }
                    }
                }
            }
        }
    }

    return 0;
}

static int httpd_listen_add(httpd_listen_t *httpd_listen, int fd)
{
    int *tmp = realloc(httpd_listen->fds, sizeof(httpd_listen->fds[0]) * (httpd_listen->num + 1));
    if (tmp == NULL) {
// FIXME
        return -1;
    }

    httpd_listen->fds = tmp;
    httpd_listen->fds[httpd_listen->num] = fd;
    httpd_listen->num++;

    return 0;
}

int httpd_open_unix_socket(httpd_listen_t *httpd_listen, char const *file, int backlog,
                                           char const *group, int perms, bool delete)
{
    int fd = socket(PF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) {
        ERROR("socket failed: %s", STRERRNO);
        return -1;
    }

    struct sockaddr_un sa = {0};
    sa.sun_family = AF_UNIX;
    sstrncpy(sa.sun_path, file, sizeof(sa.sun_path));

    DEBUG("socket path = %s", sa.sun_path);

    if (delete) {
        errno = 0;
        int status = unlink(sa.sun_path);
        if ((status != 0) && (errno != ENOENT)) {
            WARNING("Deleting socket file \"%s\" failed: %s", sa.sun_path, STRERRNO);
        } else if (status == 0) {
            INFO("Successfully deleted socket file \"%s\".", sa.sun_path);
        }
    }

    int status = bind(fd, (struct sockaddr *)&sa, sizeof(sa));
    if (status != 0) {
        ERROR("bind failed: %s", STRERRNO);
        close(fd);
        return -1;
    }

    status = chmod(sa.sun_path, perms);
    if (status == -1) {
        ERROR("chmod failed: %s", STRERRNO);
        close(fd);
        return -1;
    }

    status = listen(fd, backlog);
    if (status != 0) {
        ERROR("listen failed: %s", STRERRNO);
        close(fd);
        return -1;
    }

    long int grbuf_size = sysconf(_SC_GETGR_R_SIZE_MAX);
    if (grbuf_size <= 0)
        grbuf_size = sysconf(_SC_PAGESIZE);
    if (grbuf_size <= 0)
        grbuf_size = 4096;

    struct group *g = NULL;
    char grbuf[grbuf_size];
    struct group sg;
    status = getgrnam_r(group, &sg, grbuf, sizeof(grbuf), &g);
    if (status != 0) {
        WARNING("getgrnam_r (%s) failed: %s", group, STRERROR(status));
        return httpd_listen_add(httpd_listen, fd);
    }
    if (g == NULL) {
        WARNING("No such group: `%s'", group);
        return httpd_listen_add(httpd_listen, fd);
    }

    if (chown(file, (uid_t)-1, g->gr_gid) != 0) {
        WARNING("chown (%s, -1, %i) failed: %s", file, (int)g->gr_gid, STRERRNO);
    }

    return httpd_listen_add(httpd_listen, fd);
}

int httpd_open_socket(httpd_listen_t *httpd_listen, const char *node, const char *service, int backlog)
{
    struct addrinfo ai_hints = {
        .ai_family = AF_UNSPEC,
        .ai_flags = AI_ADDRCONFIG | AI_PASSIVE,
        .ai_protocol = IPPROTO_TCP,
        .ai_socktype = SOCK_STREAM,
    };

    struct addrinfo *ai_list = NULL;
    int status = getaddrinfo(node, service, &ai_hints, &ai_list);
    if (status != 0) {
        ERROR("getaddrinfo (%s, %s) failed: %s", (node == NULL) ? "(null)" : node,
              (service == NULL) ? "(null)" : service, gai_strerror(status));
        return -1;
    }

    for (struct addrinfo *ai = ai_list; ai != NULL; ai = ai->ai_next) {
        int fd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
        if (fd < 0) {
            ERROR("socket(2) failed: %s", STRERRNO);
            continue;
        }

        if (bind(fd, ai->ai_addr, ai->ai_addrlen) == -1) {
            ERROR("bind: %s", STRERRNO);
            freeaddrinfo(ai_list);
            close(fd);
            return -1;
        }

        status = listen(fd, backlog);
        if (status != 0) {
            ERROR("listen failed: %s", STRERRNO);
            freeaddrinfo(ai_list);
            close(fd);
            return -1;
        }

        httpd_listen_add(httpd_listen, fd);
    }

    freeaddrinfo(ai_list);

    return 0;
}

void httpd_stop(httpd_t *httpd)
{
    if (httpd == NULL)
        return;

    httpd->run = false;
}

void httpd_free(httpd_t *httpd)
{
    if (httpd == NULL)
        return;

    free(httpd->pfds);

    for (size_t i=0; i < httpd->nclients; i++) {
        httpd_client_free(httpd->clients[i]);
    }
    free(httpd->clients);

    free(httpd);
}

httpd_t *httpd_init(httpd_listen_t *httpd_listen, int max, int timeout)
{
    if (httpd_listen == NULL)
        return NULL;

    if (httpd_listen->num >= max)
        return NULL;

    httpd_t *httpd = calloc(1,sizeof(*httpd));
    if (httpd == NULL)
        return NULL;

    httpd->run = true;
    httpd->timeout = timeout;
    httpd->npfds_max = max;
    httpd->nclients  = 0;

    httpd->pfds = malloc(sizeof(httpd->pfds[0])*max);
    if (httpd->pfds == NULL) {
        free(httpd);
        return NULL;
    }

    for (int i = 0; i < httpd_listen->num; i++) {
        httpd->pfds[i].fd = httpd_listen->fds[i];
        httpd->pfds[i].events = POLLIN | POLLPRI;
    }

    httpd->listeners = httpd_listen->num;

    for (int i = httpd_listen->num; i < max; i++) {
        httpd->pfds[i].fd = -1;
        httpd->pfds[i].events = 0;
        httpd->pfds[i].revents = 0;
    }

    httpd->clients = calloc(max, sizeof(httpd->clients[0]));
    if (httpd->clients == NULL) {
        free(httpd->pfds);
        free(httpd);
        return NULL;;
    }

    return httpd;
}
