// SPDX-License-Identifier: GPL-2.0-only OR ISC
// SPDX-FileCopyrightText: Copyright (C) 2009 Florian octo Forster
// SPDX-FileContributor: Florian octo Forster <octo at verplant.org>

// from librouteros - src/main.c

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include <errno.h>
#include <assert.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <sys/time.h>
#include <fcntl.h>

#include "ros_md5.h"
#include "ros_api.h"

static int login_handler (ros_connection_t *c, const ros_reply_t *r, void *user_data);

struct ros_connection_s {
    int fd;
};

struct ros_reply_s {
    unsigned int params_num;
    char *status;
    char **keys;
    char **values;

    ros_reply_t *next;
};

typedef struct {
    const char *username;
    const char *password;
} ros_login_data_t;

static int read_exact (int fd, void *buffer, size_t buffer_size)
{
    char *buffer_ptr;
    size_t have_bytes;

    if ((fd < 0) || (buffer == NULL))
        return EINVAL;

    if (buffer_size == 0)
        return 0;

    buffer_ptr = buffer;
    have_bytes = 0;
    while (have_bytes < buffer_size) {
        size_t want_bytes;
        ssize_t status;

        want_bytes = buffer_size - have_bytes;
        errno = 0;
        status = read (fd, buffer_ptr, want_bytes);
        if (status < 0) {
            if (errno == EINTR)
                continue;
            else
                return (errno);
        } else if (status == 0) {
            return EINVAL;
        }

        assert (((size_t) status) <= want_bytes);
        have_bytes += status;
        buffer_ptr += status;
    }

    return 0;
}

static ros_reply_t *reply_alloc (void)
{
    ros_reply_t *r;

    r = malloc (sizeof (*r));
    if (r == NULL)
        return NULL;

    memset (r, 0, sizeof (*r));
    r->keys = NULL;
    r->values = NULL;
    r->next = NULL;

    return r;
}

static int reply_add_keyval (ros_reply_t *r, const char *key, const char *val)
{
    char **tmp;

    tmp = realloc (r->keys, (r->params_num + 1) * sizeof (*tmp));
    if (tmp == NULL)
        return ENOMEM;
    r->keys = tmp;

    tmp = realloc (r->values, (r->params_num + 1) * sizeof (*tmp));
    if (tmp == NULL)
        return ENOMEM;
    r->values = tmp;

    r->keys[r->params_num] = strdup (key);
    if (r->keys[r->params_num] == NULL)
        return ENOMEM;

    r->values[r->params_num] = strdup (val);
    if (r->values[r->params_num] == NULL) {
        free (r->keys[r->params_num]);
        r->keys[r->params_num] = NULL;
        return ENOMEM;
    }

    r->params_num++;
    return 0;
}

static void reply_free (ros_reply_t *r)
{
    ros_reply_t *next;
    unsigned int i;

    if (r == NULL)
        return;

    next = r->next;

    for (i = 0; i < r->params_num; i++) {
        free (r->keys[i]);
        free (r->values[i]);
    }

    free (r->status);
    free (r->keys);
    free (r->values);

    free (r);

    reply_free (next);
}

static int buffer_init (unsigned char **ret_buffer, size_t *ret_buffer_size)
{
    if ((ret_buffer == NULL) || (ret_buffer_size == NULL))
        return EINVAL;

    if (*ret_buffer_size < 1)
        return EINVAL;

    return (0);
}

static int buffer_add (unsigned char **ret_buffer, size_t *ret_buffer_size, const char *string)
{
    unsigned char *buffer;
    size_t buffer_size;
    size_t string_size;
    size_t req_size;

    if ((ret_buffer == NULL) || (ret_buffer_size == NULL) || (string == NULL))
        return (EINVAL);

    buffer = *ret_buffer;
    buffer_size = *ret_buffer_size;

    string_size = strlen (string);
    if (string_size == 0)
        return EINVAL;

    if (string_size >= 0x10000000)
        req_size = 5 + string_size;
    else if (string_size >= 0x200000)
        req_size = 4 + string_size;
    else if (string_size >= 0x4000)
        req_size = 3 + string_size;
    else if (string_size >= 0x80)
        req_size = 2 + string_size;
    else
        req_size = 1 + string_size;

    if (*ret_buffer_size < req_size)
        return ENOMEM;

    if (string_size >= 0x10000000) {
        buffer[0] = 0xF0;
        buffer[1] = (string_size >> 24) & 0xff;
        buffer[2] = (string_size >> 16) & 0xff;
        buffer[3] = (string_size >>  8) & 0xff;
        buffer[4] = (string_size      ) & 0xff;
        buffer += 5;
        buffer_size -= 5;
    } else if (string_size >= 0x200000) {
        buffer[0] = (string_size >> 24) & 0x1f;
        buffer[0] |= 0xE0;
        buffer[1] = (string_size >> 16) & 0xff;
        buffer[2] = (string_size >>  8) & 0xff;
        buffer[3] = (string_size      ) & 0xff;
        buffer += 4;
        buffer_size -= 4;
    } else if (string_size >= 0x4000) {
        buffer[0] = (string_size >> 16) & 0x3f;
        buffer[0] |= 0xC0;
        buffer[1] = (string_size >>  8) & 0xff;
        buffer[2] = (string_size      ) & 0xff;
        buffer += 3;
        buffer_size -= 3;
    } else if (string_size >= 0x80) {
        buffer[0] = (string_size >>  8) & 0x7f;
        buffer[0] |= 0x80;
        buffer[1] = (string_size      ) & 0xff;
        buffer += 2;
        buffer_size -= 2;
    } else {
        buffer[0] = (char) string_size;
        buffer += 1;
        buffer_size -= 1;
    }

    assert (buffer_size >= string_size);
    memcpy (buffer, string, string_size);
    buffer += string_size;
    buffer_size -= string_size;

    *ret_buffer = buffer;
    *ret_buffer_size = buffer_size;

    return 0;
}

static int buffer_end (unsigned char **ret_buffer, size_t *ret_buffer_size)
{
    if ((ret_buffer == NULL) || (ret_buffer_size == NULL))
        return EINVAL;

    if (*ret_buffer_size < 1)
        return EINVAL;

    /* Add empty word. */
    (*ret_buffer)[0] = 0;
    (*ret_buffer)++;
    (*ret_buffer_size)--;

    return 0;
}

static int send_command (ros_connection_t *c, const char *command,
                         size_t args_num, const char * const *args)
{
    unsigned char buffer[4096];
    unsigned char *buffer_ptr;
    size_t buffer_size;

    size_t i;
    int status;

    assert (c != NULL);
    assert (command != NULL);

    if ((args == NULL) && (args_num > 0))
        return EINVAL;

    /* FIXME: For debugging only */
    memset (buffer, 0, sizeof (buffer));

    buffer_ptr = buffer;
    buffer_size = sizeof (buffer);

    status = buffer_init (&buffer_ptr, &buffer_size);
    if (status != 0)
        return status;

//    ros_debug ("send_command: command = %s;\n", command);
    status = buffer_add (&buffer_ptr, &buffer_size, command);
    if (status != 0)
        return status;

    for (i = 0; i < args_num; i++)
    {
        if (args[i] == NULL)
            return EINVAL;

//        ros_debug ("send_command: arg[%zu] = %s;\n", i, args[i]);
        status = buffer_add (&buffer_ptr, &buffer_size, args[i]);
        if (status != 0)
            return status;
    }

    status = buffer_end (&buffer_ptr, &buffer_size);
    if (status != 0)
        return status;

    buffer_ptr = buffer;
    buffer_size = sizeof (buffer) - buffer_size;
    while (buffer_size > 0) {
        ssize_t bytes_written;

        errno = 0;
        bytes_written = write (c->fd, buffer_ptr, buffer_size);
        if (bytes_written < 0) {
            if (errno == EAGAIN)
                continue;
            else
                return errno;
        }
        assert (((size_t) bytes_written) <= buffer_size);

        buffer_ptr += bytes_written;
        buffer_size -= bytes_written;
    }

    return 0;
}

static int read_word (ros_connection_t *c, char *buffer, size_t *buffer_size)
{
    size_t req_size;
    uint8_t word_length[5];
    int status;

    if ((buffer == NULL) || (*buffer_size < 1))
        return EINVAL;

    assert (c != NULL);

    /* read one byte from the socket */
    status = read_exact (c->fd, word_length, 1);
    if (status != 0)
        return (status);

    /* Calculate `req_size' */
    if (((unsigned char) word_length[0]) == 0xF0) {
        status = read_exact (c->fd, &word_length[1], 4);
        if (status != 0)
            return status;

        req_size = (size_t)(word_length[1] << 24) | (size_t)(word_length[2] << 16) |
                   (size_t)(word_length[3] << 8) | (size_t)word_length[4];
    } else if ((word_length[0] & 0xE0) == 0xE0) {
        status = read_exact (c->fd, &word_length[1], 3);
        if (status != 0)
            return status;

        req_size = (size_t)((word_length[0] & 0x1F) << 24) | (size_t)(word_length[1] << 16) |
                   (size_t)(word_length[2] << 8) | (size_t)word_length[3];
    } else if ((word_length[0] & 0xC0) == 0xC0) {
        status = read_exact (c->fd, &word_length[1], 2);
        if (status != 0)
            return status;

        req_size = ((size_t)(word_length[0] & 0x3F) << 16) |
                    (size_t)(word_length[1] << 8) | (size_t)word_length[2];
    }
    else if ((word_length[0] & 0x80) == 0x80) {
        status = read_exact (c->fd, &word_length[1], 1);
        if (status != 0)
            return status;

        req_size = (size_t)((word_length[0] & 0x7F) << 8) | (size_t)word_length[1];
    } else if ((word_length[0] & 0x80) == 0) {
        req_size = (size_t)word_length[0];
    } else {
        /* First nibble is `F' but second nibble is not `0'. */
        return EPROTO;
    }

    if (*buffer_size < req_size)
        return ENOMEM;

    /* Empty word. This ends a `sentence' and must therefore be valid. */
    if (req_size == 0) {
        buffer[0] = 0;
        *buffer_size = 0;
        return 0;
    }

    status = read_exact (c->fd, buffer, req_size);
    if (status != 0)
        return status;
    *buffer_size = req_size;

    return 0;
}

static ros_reply_t *receive_sentence (ros_connection_t *c)
{
    char buffer[4096];
    size_t buffer_size;
    int status;

    ros_reply_t *r;

    r = reply_alloc ();
    if (r == NULL)
        return NULL;

    while (true) {
        buffer_size = sizeof (buffer) - 1;
        status = read_word (c, buffer, &buffer_size);
        if (status != 0)
            break;
        assert (buffer_size < sizeof (buffer));
        buffer[buffer_size] = 0;

        /* Empty word means end of reply */
        if (buffer_size == 0)
            break;

        if (buffer[0] == '!') {
            if (r->status != NULL)
                free (r->status);
            r->status = strdup (&buffer[1]);
            if (r->status == NULL)
                break;
        } else if (buffer[0] == '=') {
            char *key = &buffer[1];
            char *val = strchr (key, '=');
            if (val == NULL) {
                fprintf (stderr, "Ignoring misformed word: %s\n", buffer);
                continue;
            }
            *val = 0;
            val++;

            reply_add_keyval (r, key, val);
        } else {
//            ros_debug ("receive_sentence: Ignoring unknown word: %s\n", buffer);
        }
    }

    if (r->status == NULL) {
        reply_free (r);
        return NULL;
    }

    return r;
}

static ros_reply_t *receive_reply (ros_connection_t *c)
{
    ros_reply_t *head;
    ros_reply_t *tail;

    if (c == NULL)
        return (NULL);

    head = NULL;
    tail = NULL;

    while (true) {
        ros_reply_t *tmp = receive_sentence (c);
        if (tmp == NULL)
            break;

        if (tail == NULL) {
            head = tmp;
            tail = tmp;
        } else {
            tail->next = tmp;
            tail = tmp;
        }

        if (strcmp ("done", tmp->status) == 0)
            break;
    }

    return head;
}

static int connect_socket_timeout (struct addrinfo *ai_ptr, unsigned int timeout_sec)
{
    struct timeval timeout = {
        .tv_sec = timeout_sec,
    };

    /* value of 0 means no timeout, select treats NULL as no timeout */
    struct timeval *timeout_ptr = timeout_sec ? &timeout : NULL;

    int fd;
    int status;

    fd = socket (ai_ptr->ai_family, ai_ptr->ai_socktype, ai_ptr->ai_protocol);
    if (fd < 0) {
//        ros_debug ("connect_socket_timeout: socket(2) failed.\n");
        return 0;
    }

    /* set socket nonblocking just for the connect() - allows us to call select() on connecting socket */
    status = fcntl(fd, F_SETFL, O_NONBLOCK);
    if (status != 0) {
        close (fd);
        return 0;
    }

    status = connect (fd, ai_ptr->ai_addr, ai_ptr->ai_addrlen);
    if (status != 0 && errno != EINPROGRESS) {
        /* connect() failed due to some reason other than a timeout */
//        ros_debug ("connect_socket_timeout: connect(2) failed.\n");
        close (fd);
        return 0;
    }

    fd_set fdset;
    FD_ZERO (&fdset);
    FD_SET (fd, &fdset);

    if (select (fd + 1, NULL, &fdset, NULL, timeout_ptr) == 1) {
        /* find out what happened to the socket */
        int socket_error;
        socklen_t len = sizeof (socket_error);

        if (getsockopt (fd, SOL_SOCKET, SO_ERROR, &socket_error, &len) != 0) {
//            ros_debug ("connect_socket_timeout: getsockopt(2) failed\n");
            close (fd);
            return (0);
        }

        if (socket_error != 0) {
//            ros_debug ("connect_socket_timeout: connect(2) failed.\n");
            close (fd);

            /* fill in errno so the caller knows what happened */
            errno = socket_error;
            return 0;
        }
    } else {
//        ros_debug ("connect_socket_timeout: select(2) failed.\n");
        close (fd);

        /* replace error code as ETIMEDOUT is more representative than EINPROGRESS */
        if (errno == EINPROGRESS)
            errno = ETIMEDOUT;

        return 0;
    }

    fcntl (fd, F_SETFL, 0);
    return fd;
}

static int create_socket (const char *node, const char *service,
                          const ros_connect_opts_t *connect_opts)
{
    struct addrinfo  ai_hint;
    struct addrinfo *ai_list;
    struct addrinfo *ai_ptr;
    int status;

//    ros_debug ("create_socket (node = %s, service = %s);\n", node, service);

    memset (&ai_hint, 0, sizeof (ai_hint));
#ifdef AI_ADDRCONFIG
    ai_hint.ai_flags |= AI_ADDRCONFIG;
#endif
    ai_hint.ai_family = AF_UNSPEC;
    ai_hint.ai_socktype = SOCK_STREAM;

    ai_list = NULL;
    status = getaddrinfo (node, service, &ai_hint, &ai_list);
    if (status != 0)
        return -1;
    assert (ai_list != NULL);

    for (ai_ptr = ai_list; ai_ptr != NULL; ai_ptr = ai_ptr->ai_next) {
        int fd;

        /* timeout value of 0 means inf timeout */
        fd = connect_socket_timeout (ai_ptr, connect_opts ? connect_opts->connect_timeout : 0);
        if (fd == 0) {
            /* connection failed, try the next host */
            continue;
        }

        /* set receive timeout on the socket if one is set */
        if (connect_opts && connect_opts->receive_timeout) {
            struct timeval timeout = {
                .tv_sec = connect_opts->receive_timeout,
            };

            if (setsockopt (fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof (timeout)) < 0) {
//                ros_debug ("create_socket: setsockopt(2) failed.\n");
                close (fd);
                continue;
            }
        }

        freeaddrinfo (ai_list);
        return fd;
    }

    freeaddrinfo (ai_list);
    return -1;
}

static int login2_handler (__attribute__((unused)) ros_connection_t *c,
                           const ros_reply_t *r, void *user_data)
{
    const char *ret;
    if (r == NULL)
        return EINVAL;

    /* check if we need to use old auth method */
    ret = ros_reply_param_val_by_key (r, "ret");
    if (ret != NULL) {
//        ros_debug ("login2_handler: Reply does contain \"ret\" with challenge parameter.\n"
//                "Falling back to old (pre-v6.43) auth method.\n");
        /* re-process result with the old handler */
        return login_handler(c, r, user_data);
    }

    if (strcmp (r->status, "trap") == 0) {
//        ros_debug ("login2_handler: Logging in failed: %s.\n",
//                ros_reply_param_val_by_key (r, "message"));
        return EACCES;
    } else if (strcmp (r->status, "done") != 0) {
//        ros_debug ("login2_handler: Unexpected status: %s.\n", r->status);
        return EPROTO;
    }

    return 0;
}

static void hash_binary_to_hex (char hex[33], uint8_t binary[16])
{
    for (int i = 0; i < 16; i++) {
        char tmp[3];
        snprintf (tmp, 3, "%02"PRIx8, binary[i]);
        tmp[2] = 0;
        hex[2*i] = tmp[0];
        hex[2*i+1] = tmp[1];
    }
    hex[32] = 0;
}

static void hash_hex_to_binary (uint8_t binary[16], char hex[33])
{
    for (int i = 0; i < 16; i++) {
        char tmp[3];

        tmp[0] = hex[2*i];
        tmp[1] = hex[2*i + 1];
        tmp[2] = 0;

        binary[i] = (uint8_t) strtoul (tmp, /* endptr = */ NULL, /* base = */ 16);
    }
}

static void make_password_hash (char response_hex[33],
                                const char *password, size_t password_length,
                                char challenge_hex[33])
{
    uint8_t challenge_bin[16];
    uint8_t response_bin[16];
    char data_buffer[password_length+17];
    MD5_CTX md5;

    hash_hex_to_binary (challenge_bin, challenge_hex);

    data_buffer[0] = 0;
    memcpy (&data_buffer[1], password, password_length);
    memcpy (&data_buffer[1+password_length], challenge_bin, 16);

    MD5_Init (&md5);
    MD5_Update (&md5, data_buffer, sizeof (data_buffer));
    MD5_Final (response_bin, &md5);

    hash_binary_to_hex (response_hex, response_bin);
}

static int login_handler (ros_connection_t *c, const ros_reply_t *r, void *user_data)
{
    const char *ret;
    char challenge_hex[33];
    char response_hex[33];
    ros_login_data_t *login_data;

    const char *params[2];
    char param_name[1024];
    char param_response[64];

    if (r == NULL)
        return (EINVAL);

    /* The expected result looks like this:
     * -- 8< --
     *  !done
     *  =ret=ebddd18303a54111e2dea05a92ab46b4
     * -- >8 --
     */
    if (strcmp (r->status, "done") != 0) {
//        ros_debug ("login_handler: Unexpected status: %s.\n", r->status);
        return EPROTO;
    }

    login_data = user_data;
    if (login_data == NULL)
        return EINVAL;

    ret = ros_reply_param_val_by_key(r, "ret");
    if (ret == NULL) {
//        ros_debug ("login_handler: Reply does not have parameter \"ret\".\n");
        return EPROTO;
    }
//    ros_debug ("login_handler: ret = %s;\n", ret);

    if (strlen(ret) != 32) {
//        ros_debug ("login_handler: Unexpected length of the \"ret\" argument.\n");
        return EPROTO;
    }

    strncpy(challenge_hex, ret, sizeof(challenge_hex));
    challenge_hex[sizeof(challenge_hex)-1] = '\0';

    make_password_hash (response_hex, login_data->password, strlen (login_data->password),
                        challenge_hex);

    snprintf(param_name, sizeof (param_name), "=name=%s", login_data->username);
    snprintf(param_response, sizeof (param_response), "=response=00%s", response_hex);
    params[0] = param_name;
    params[1] = param_response;

    return ros_query(c, "/login", 2, params, login2_handler, /* user data = */ NULL);
}

ros_connection_t *ros_connect (const char *node, const char *service,
                               const char *username, const char *password)
{
    return ros_connect_with_options(node, service, username, password, NULL);
}

ros_connection_t *ros_connect_with_options (const char *node, const char *service,
                                            const char *username, const char *password,
                                            const ros_connect_opts_t *connect_opts)
{
    int fd;
    ros_connection_t *c;
    int status;
    ros_login_data_t user_data;

    const char *params[2];
    char param_username[1024];
    char param_password[1024];

    if ((node == NULL) || (username == NULL) || (password == NULL))
        return (NULL);

    fd = create_socket (node, (service != NULL) ? service : ROUTEROS_API_PORT, connect_opts);
    if (fd < 0)
        return NULL;

    c = malloc (sizeof (*c));
    if (c == NULL) {
        close (fd);
        return NULL;
    }
    memset (c, 0, sizeof (*c));

    c->fd = fd;

    user_data.username = username;
    user_data.password = password;

    /* first we are trying to login using new post-v6.43 method
        with username and password filled in */
    snprintf (param_username, sizeof (param_username), "=name=%s", username);
    snprintf (param_password, sizeof (param_password), "=password=%s", password);
    params[0] = param_username;
    params[1] = param_password;
    status = ros_query (c, "/login", 2, params, login2_handler, &user_data);

    if (status != 0) {
        ros_disconnect (c);
        errno = status;
        return NULL;
    }

    return c;
}

int ros_disconnect (ros_connection_t *c)
{
    if (c == NULL)
        return EINVAL;

    if (c->fd >= 0) {
        close (c->fd);
        c->fd = -1;
    }

    free (c);

    return 0;
}

int ros_query (ros_connection_t *c, const char *command, size_t args_num, const char * const *args,
               ros_reply_handler_t handler, void *user_data)
{
    int status;
    ros_reply_t *r;

    if ((c == NULL) || (command == NULL) || (handler == NULL))
        return EINVAL;

    status = send_command (c, command, args_num, args);
    if (status != 0)
        return status;

    r = receive_reply (c);
    if (r == NULL)
        return EPROTO;

    /* Call the callback function with the data we received. */
    status = (*handler) (c, r, user_data);

    /* Free the allocated memory ... */
    reply_free (r);

    /* ... and return. */
    return status;
}

const ros_reply_t *ros_reply_next (const ros_reply_t *r)
{
    if (r == NULL)
        return NULL;

    return r->next;
}

int ros_reply_num (const ros_reply_t *r)
{
    int ret = 0;
    for (const ros_reply_t *ptr = r; ptr != NULL; ptr = ptr->next)
        ret++;

    return ret;
}

const char *ros_reply_status (const ros_reply_t *r)
{
    if (r == NULL)
        return (NULL);
    return r->status;
}

const char *ros_reply_param_key_by_index (const ros_reply_t *r, unsigned int index)
{
    if (r == NULL)
        return NULL;

    if (index >= r->params_num)
        return NULL;

    return r->keys[index];
}

const char *ros_reply_param_val_by_index (const ros_reply_t *r, unsigned int index)
{
    if (r == NULL)
        return NULL;

    if (index >= r->params_num)
        return NULL;

    return r->values[index];
}

const char *ros_reply_param_val_by_key (const ros_reply_t *r, const char *key)
{
    unsigned int i;

    if ((r == NULL) || (key == NULL))
        return NULL;

    for (i = 0; i < r->params_num; i++) {
        if (strcmp (r->keys[i], key) == 0)
            return (r->values[i]);
    }

    return NULL;
}

