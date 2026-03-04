// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2026 Manuel Sanmartín
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "libtest/testing.h"
#include "libutils/common.h"
#include "libutils/socket.h"
#include "libconfig/config.h"

#include <pthread.h>

extern void module_register(void);

static pthread_mutex_t lock;
static pthread_cond_t cond;
static bool running;

#define LISTEN_PORT 32222

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
        int status = write(fdw, buffer, size);
        if (status < 0)
            fprintf(stderr, "Failed to write\n");
    }

    close(fdr);

    return 0;
}

static void *httpd_thread(void *data)
{
    char *base_path = data;

    int sfd = socket_listen_tcp("127.0.0.1", LISTEN_PORT, AF_INET, 15, true);

    pthread_mutex_lock(&lock);
    pthread_cond_broadcast(&cond);
    running = true;
    pthread_mutex_unlock(&lock);

    if (sfd < 0)
        goto quit;

    while (true) {
        int fd = accept(sfd, NULL, NULL);
        if (fd < 0)
            break;

        char buffer[4096];
        int status = read(fd, buffer, sizeof(buffer));
        if (status < 0)
            fprintf(stderr, "Failed to read\n");

        dump_file(base_path, "response", fd);

        close(fd);
    }

    close(sfd);

quit:
    pthread_exit(NULL);
}

DEF_TEST(test01)
{
    pthread_t thread_id;

    pthread_cond_init(&cond, NULL);
    pthread_mutex_init(&lock, NULL);

    pthread_create(&thread_id, NULL, httpd_thread, "src/plugins/redfish/test01");

    config_item_t *ci;
    CHECK_NOT_NULL(ci = config_parse_file("src/plugins/redfish/test01/config.txt"));

    pthread_mutex_lock(&lock);
    if (!running)
        pthread_cond_wait(&cond, &lock);
    pthread_mutex_unlock(&lock);

    EXPECT_EQ_INT(0, plugin_test_do_read(NULL, NULL, ci->children,
                                         "src/plugins/redfish/test01/expect.txt"));

    config_free(ci);

    pthread_cancel(thread_id);

    pthread_join(thread_id, NULL);

    return 0;
}

int main(void)
{
    module_register();

    RUN_TEST(test01);

    plugin_test_reset();

    END_TEST;
}
