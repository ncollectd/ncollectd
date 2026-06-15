// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2024 Manuel Sanmartín
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "libtest/testing.h"
#include "libutils/common.h"
#include "libutils/socket.h"

#include <pthread.h>

extern void module_register(void);

static int sfd;

static int read_command(int fd, char *base_path)
{
    char command[256];
    ssize_t size = read(fd, command, sizeof(command)-1);
    if (size <= 0) {
        close(fd);
        return -1;
    }
    command[size] = '\0';

    if (strcmp(command, "{\"command\": \"statistic-get-all\"}") == 0) {
        test_dump_file(base_path, "statistic-get-all.json", fd);
    } else if (strcmp(command, "{\"command\": \"config-get\"}") == 0) {
        test_dump_file(base_path, "config-get.json", fd);
    } else if (strcmp(command, "{\"command\": \"config-hash-get\"}") == 0) {
        test_dump_file(base_path, "config-hash-get.json",fd);
    }

    close(fd);

    return 0;
}

static void *kea_thread(void *data)
{
    char *base_path= data;

    while (true) {
        int fd = accept(sfd, NULL, NULL);
        if (fd < 0)
            break;

        if (read_command(fd, base_path) < 0)
            break;

        close(fd);
    }

    pthread_exit(NULL);
}

DEF_TEST(test01)
{
    pthread_t thread_id;

    char *sfile = "src/plugins/kea/test01/kea.socket";

    sfd = socket_listen_unix_stream(sfile, 0, NULL, 0660, true, 0);
    EXPECT_EQ_INT(1, sfd >= 0);

    char *config = "instance local {\n"
                   "    socket-path \"src/plugins/kea/test01/kea.socket\"\n"
                   "}";
    config_item_t *ci = config_parse_buffer(config, strlen(config));
    CHECK_NOT_NULL(ci);

    pthread_create(&thread_id, NULL, kea_thread, "src/plugins/kea/test01");

    EXPECT_EQ_INT(0, plugin_test_do_read(NULL, NULL, ci, "src/plugins/kea/test01/expect.txt"));

    pthread_cancel(thread_id);
    pthread_join(thread_id, NULL);

    close(sfd);
    unlink("src/plugins/kea/test01/kea.socket");
    config_free(ci);

    return 0;
}

int main(void)
{
    module_register();

    RUN_TEST(test01);

    plugin_test_reset();

    END_TEST;
}
