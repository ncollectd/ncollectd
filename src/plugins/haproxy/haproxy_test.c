// SPDX-FileCopyrightText: Copyright (C) 2026 Manuel Sanmartín
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>
// SPDX-License-Identifier: GPL-2.0-only

#include "libtest/testing.h"
#include "libutils/common.h"
#include "libutils/socket.h"
#include "libconfig/config.h"

#include <pthread.h>

extern void module_register(void);

static int sfd;

static void *haproxy_thread(void *data)
{
    char *base_path = data;

    while (true) {
        int fd = accept(sfd, NULL, NULL);
        if (fd < 0)
            break;

        char command[256];
        ssize_t size = read(fd, command, sizeof(command) - 1);
        if (size > 0) {
            command[size] = '\0';
            if (strcmp(command, "show info\n") == 0)
                test_dump_file(base_path, "info", fd);
            else if (strcmp(command, "show stat\n") == 0)
                test_dump_file(base_path, "stat", fd);
            else if (strcmp(command, "show table\n") == 0)
                test_dump_file(base_path, "table", fd);
        }

        close(fd);
    }

    pthread_exit(NULL);
}

static void *httpd_thread(void *data)
{
    char *base_path = data;

    while (true) {
        int fd = accept(sfd, NULL, NULL);
        if (fd < 0)
            break;

        char buffer[4096];
        if (read(fd, buffer, sizeof(buffer)) > 0)
            test_dump_file(base_path, "response", fd);

        close(fd);
    }

    pthread_exit(NULL);
}

DEF_TEST(test01)
{
    pthread_t thread_id;

    char *sfile = "src/plugins/haproxy/test01/haproxy.socket";
    sfd = socket_listen_unix_stream(sfile, 0, NULL, 0660, true, 0);
    EXPECT_EQ_INT(1, sfd >= 0);

    char *config = "instance local {\n"
                   "    socket-path \"src/plugins/haproxy/test01/haproxy.socket\"\n"
                   "    label proxy edge1\n"
                   "}";
    config_item_t *ci = config_parse_buffer(config, strlen(config));
    CHECK_NOT_NULL(ci);

    pthread_create(&thread_id, NULL, haproxy_thread, "src/plugins/haproxy/test01");

    EXPECT_EQ_INT(0, plugin_test_do_read(NULL, NULL, ci, "src/plugins/haproxy/test01/expect.txt"));

    pthread_cancel(thread_id);
    pthread_join(thread_id, NULL);

    close(sfd);
    unlink(sfile);
    config_free(ci);

    return 0;
}

DEF_TEST(test02)
{
    pthread_t thread_id;

    sfd = socket_listen_tcp("127.0.0.1", 0, AF_INET, 15, true);
    EXPECT_EQ_INT(1, sfd >= 0);

    int port = socket_get_port(sfd);
    EXPECT_EQ_INT(1, port >= 0);

    char config[1024];
    ssnprintf(config, sizeof(config), "instance local {\n"
                                      "    url \"http://127.0.0.1:%d/stats;csv\"\n"
                                      "}", port);
    config_item_t *ci = config_parse_buffer(config, strlen(config));
    CHECK_NOT_NULL(ci);

    pthread_create(&thread_id, NULL, httpd_thread, "src/plugins/haproxy/test02");

    EXPECT_EQ_INT(0, plugin_test_do_read(NULL, NULL, ci, "src/plugins/haproxy/test02/expect.txt"));

    pthread_cancel(thread_id);
    pthread_join(thread_id, NULL);

    config_free(ci);
    close(sfd);

    return 0;
}

int main(void)
{
    module_register();

    RUN_TEST(test01);

    plugin_test_reset();

    module_register();

    RUN_TEST(test02);

    plugin_test_reset();

    END_TEST;
}
