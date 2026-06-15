// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2026 Manuel Sanmartín
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "libtest/testing.h"
#include "libutils/common.h"
#include "libutils/socket.h"
#include "libconfig/config.h"

#include <pthread.h>

extern void module_register(void);

static int sfd;

static void *httpd_thread(void *data)
{
    char *base_path = data;

    while (true) {
        int fd = accept(sfd, NULL, NULL);
        if (fd < 0)
            break;

        char buffer[4096];
        int status = read(fd, buffer, sizeof(buffer));
        if (status < 0)
            fprintf(stderr, "Failed to read\n");

        test_dump_file(base_path, "response", fd);

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

    char config[2048];
    ssnprintf(config, sizeof(config), "query \"Power\" {\n"
                                      "    end-point \"/Chassis/1/Power\"\n"
                                      "    resource \"PowerSupplies\" {\n"
                                      "        property \"LastPowerOutputWatts\" {\n"
                                      "            metric \"bmc_powersupply_output_watts\"\n"
                                      "            type gauge\n"
                                      "            label-from \"id\" \"MemberId\"\n"
                                      "            label-from \"sn\" \"SerialNumber\"\n"
                                      "        }\n"
                                      "        property \"PowerCapacityWatts\" {\n"
                                      "            metric \"bmc_powersupply_capacity_watts\"\n"
                                      "            type gauge\n"
                                      "            label-from \"id\" \"MemberId\"\n"
                                      "            label-from \"sn\" \"SerialNumber\"\n"
                                      "        }\n"
                                      "        property \"LineInputVoltage\" {\n"
                                      "            metric \"bmc_powersupply_input_volts\"\n"
                                      "            type gauge\n"
                                      "            label-from \"id\" \"MemberId\"\n"
                                      "            label-from \"sn\" \"SerialNumber\"\n"
                                      "        }\n"
                                      "        property \"Status\" {\n"
                                      "            metric \"bmc_powersupply_status\"\n"
                                      "            type gauge\n"
                                      "            label-from \"id\" \"MemberId\"\n"
                                      "            label-from \"sn\" \"SerialNumber\"\n"
                                      "        }\n"
                                      "    }\n"
                                      "    resource \"Redundancy\" {\n"
                                      "        property \"Status\" {\n"
                                      "            metric \"bmc_power_redundancy_status\"\n"
                                      "            type gauge\n"
                                      "        }\n"
                                      "    }\n"
                                      "    resource \"PowerControl\" {\n"
                                      "        property \"PowerConsumedWatts\" {\n"
                                      "            metric \"bmc_powercontrol_consumed_watts\"\n"
                                      "            type gauge\n"
                                      "            label-from \"id\" \"MemberId\"\n"
                                      "        }\n"
                                      "        property \"PowerCapacityWatts\" {\n"
                                      "            metric \"bmc_powercontrol_capacity_watts\"\n"
                                      "            type gauge\n"
                                      "            label-from \"id\" \"MemberId\"\n"
                                      "        }\n"
                                      "    }\n"
                                      "}\n"
                                      "instance \"local\" {\n"
                                      "    url \"http://127.0.0.1:%d\"\n"
                                      "    query \"Power\"\n"
                                      "}\n", port);
    config_item_t *ci = config_parse_buffer(config, strlen(config));
    CHECK_NOT_NULL(ci);

    pthread_create(&thread_id, NULL, httpd_thread, "src/plugins/redfish/test01");

    EXPECT_EQ_INT(0, plugin_test_do_read(NULL, NULL, ci,
                                         "src/plugins/redfish/test01/expect.txt"));

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
