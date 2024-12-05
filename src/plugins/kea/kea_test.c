// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2024 Manuel Sanmartín
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "libtest/testing.h"
#include "libutils/common.h"
#include "libutils/socket.h"

#include <pthread.h>

extern void module_register(void);

static pthread_mutex_t lock;
static pthread_cond_t cond;

static int dump_file(char *base_path, char *file, int fdw)
{
    char file_path[PATH_MAX];
    ssnprintf(file_path, sizeof(file_path), "%s/%s", base_path, file);

    int fdr = open(file_path, O_RDONLY);
    if (fdr < 0)
        return -1;

    char buffer[8192];
    ssize_t size = 0;
    while ((size = read(fdr, buffer, sizeof(buffer))) > 0) {
        write(fdw, buffer, size);
    }

    close(fdr);

    return 0;
}

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
        dump_file(base_path, "statistic-get-all.json", fd);
    } else if (strcmp(command, "{\"command\": \"config-get\"}") == 0) {
        dump_file(base_path, "config-get.json", fd);
    } else if (strcmp(command, "{\"command\": \"config-hash-get\"}") == 0) {
        dump_file(base_path, "config-hash-get.json",fd);
    }

    close(fd);

    return 0;
}

static void *kea_thread(void *data)
{
    char *base_path= data;

    char sfile[PATH_MAX];
    ssnprintf(sfile, sizeof(sfile), "%s/kea.socket", base_path);

    unlink(sfile);

    int sfd = socket_listen_unix_stream(sfile, 0, NULL, 0660, true, 0);

    pthread_mutex_lock(&lock);
    pthread_cond_broadcast(&cond);
    pthread_mutex_unlock(&lock);

    if (sfd < 0)
        goto quit;

    while (true) {
        int fd = accept(sfd, NULL, NULL);
        if (fd < 0)
            break;

        if (read_command(fd, base_path) < 0)
            break;
    }

    close(sfd);
    unlink(sfile);

quit:
    pthread_exit(NULL);
}

DEF_TEST(test01)
{
    pthread_t thread_id;

    pthread_cond_init(&cond, NULL);
    pthread_mutex_init(&lock, NULL);

    pthread_create(&thread_id, NULL, kea_thread, "src/plugins/kea/test01");

    config_item_t ci = (config_item_t) {
        .key = "plugin",
        .values_num = 1,
        .values = (config_value_t[]) {{.type = CONFIG_TYPE_STRING, .value.string ="kea"}},
        .children_num = 1,
        .children = (config_item_t[]) {
            {
                .key = "instance",
                .values_num = 1,
                .values = (config_value_t[]) {{.type = CONFIG_TYPE_STRING, .value.string ="local"}},
                .children_num = 1,
                .children = (config_item_t[]) {
                    {
                        .key = "socket-path",
                        .values_num = 1,
                        .values = (config_value_t[]) {
                            {
                                .type = CONFIG_TYPE_STRING,
                                .value.string = "src/plugins/kea/test01/kea.socket"
                            },
                        }
                    }
                }
            },
        }
    };

    pthread_mutex_lock(&lock);
    pthread_cond_wait(&cond, &lock);
    pthread_mutex_unlock(&lock);

    EXPECT_EQ_INT(0, plugin_test_do_read(NULL, NULL, &ci, "src/plugins/kea/test01/expect.txt"));

    pthread_cancel(thread_id);

    pthread_join(thread_id, NULL);

    unlink("src/plugins/kea/test01/kea.socket");

    return 0;
}

int main(void)
{
    module_register();

    RUN_TEST(test01);

    plugin_test_reset();

    END_TEST;
}
