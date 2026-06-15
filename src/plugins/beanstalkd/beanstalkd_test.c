// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2024 Manuel Sanmartín
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "libtest/testing.h"
#include "libutils/common.h"
#include "libutils/socket.h"

#include <pthread.h>

extern void module_register(void);

static int sfd;

static void *beanstalkd_thread(void *data)
{
    char *base_path = data;

    while (true) {
        int fd = accept(sfd, NULL, NULL);
        if (fd < 0)
            break;

        char buffer[4096];
        int status = read(fd, buffer, sizeof(buffer));
        if (status < 0)
            fprintf(stderr, "Failed to write\n");

        test_dump_file(base_path, "stats", fd);

        close(fd);
    }

    pthread_exit(NULL);
}

DEF_TEST(test01)
{
    pthread_t thread_id;

    sfd = socket_listen_tcp("127.0.0.1", 0, AF_INET, 15, true);
    EXPECT_EQ_INT(1, sfd >= 0);

    int port = socket_get_port(sfd);
    EXPECT_EQ_INT(1, port >= 0);

    char config[1024];
    ssnprintf(config, sizeof(config), "instance local {\n"
                                      "    host \"127.0.0.1\"\n"
                                      "    port %d\n"
                                      "}", port);
    config_item_t *ci = config_parse_buffer(config, strlen(config));
    CHECK_NOT_NULL(ci);

    pthread_create(&thread_id, NULL, beanstalkd_thread, "src/plugins/beanstalkd/test01");

    EXPECT_EQ_INT(0, plugin_test_do_read(NULL, NULL, ci, "src/plugins/beanstalkd/test01/expect.txt"));

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

    END_TEST;
}
