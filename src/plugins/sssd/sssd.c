// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2025 Manuel Sanmartín
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"

#include <systemd/sd-daemon.h>
#include <systemd/sd-bus.h>

#ifdef HAVE_LINUX_CONFIG_H
#include <linux/config.h>
#endif

enum {
    FAM_SSSD_UP,
    FAM_SSSD_DOMAIN_ONLINE,
    FAM_SSSD_DOMAIN_ACTIVE_SERVER,
    FAM_SSSD_MAX
};

static metric_family_t fams[FAM_SSSD_MAX] = {
    [FAM_SSSD_UP] = {
        .name = "sssd_up",
        .type = METRIC_TYPE_GAUGE,
        .help = "Could the sssd server be reached.",
    },
    [FAM_SSSD_DOMAIN_ONLINE] = {
        .name = "sssd_domain_online",
        .type = METRIC_TYPE_GAUGE,
        .help = "Check if this domain available."
    },
    [FAM_SSSD_DOMAIN_ACTIVE_SERVER] = {
        .name = "sssd_domain_active_server",
        .type = METRIC_TYPE_INFO,
        .help = "Active server for this domain and service."
    },
};

#define BUS_ERROR_MSG(e, error) bus_error_msg(e, error, (char[ERRBUF_SIZE]){0}, ERRBUF_SIZE)

static const char* bus_error_msg(const sd_bus_error *e, int error, char *buf, size_t buflen)
{
    if (sd_bus_error_has_name(e, SD_BUS_ERROR_ACCESS_DENIED))
        return "Access denied";

    if ((e != NULL) && (e->message != NULL))
        return e->message;

    return sstrerror(error, buf, buflen);
}

static int sssd_active_server(sd_bus *bus, char *domain, char *decode_domain, char *service)
{
    sd_bus_message *reply = NULL;
    sd_bus_error error = SD_BUS_ERROR_NULL;

    sd_bus_message *m = NULL;
    int status = sd_bus_message_new_method_call(bus, &m,
                                                "org.freedesktop.sssd.infopipe", domain,
                                                "org.freedesktop.sssd.infopipe.Domains.Domain",
                                                "ActiveServer");
    if (status < 0) {
        PLUGIN_ERROR("Failed to crate call to "
                     "org.freedesktop.sssd.infopipe.Domains.Domain.ActiveServer.");
        sd_bus_error_free(&error);
        return -1;
    }

    status = sd_bus_message_append_basic(m, 's', service);
    if (status < 0) {
        PLUGIN_ERROR("Failed to add argument to "
                     "org.freedesktop.sssd.infopipe.Domains.Domain.ActiveServer.");
        sd_bus_message_unref(m);
        return -1;
    }

    status = sd_bus_call(bus, m, 0, &error, &reply);
    if (status < 0) {
        PLUGIN_ERROR("Call to org.freedesktop.sssd.infopipe.Domains.Domain.ActiveServer"
                     "failed: %s.", BUS_ERROR_MSG(&error, status));
        sd_bus_error_free(&error);
        sd_bus_message_unref(m);
        return -1;
    }

    sd_bus_error_free(&error);
    sd_bus_message_unref(m);

    char *active = NULL;
    status = sd_bus_message_read(reply, "s", &active);
    if (status <= 0) {
        PLUGIN_ERROR("Failed to read active server from "
                     "org.freedesktop.sssd.infopipe.Domains.Domain.ActiveServer.");
        sd_bus_message_unref(reply);
        return -1;
    }

    label_set_t info = {
        .num = 1,
        .ptr = (label_pair_t[]) {
            {.name="active", .value=active}
        }
    };
    metric_family_append(&fams[FAM_SSSD_DOMAIN_ACTIVE_SERVER], VALUE_INFO(info), NULL,
                         &(label_pair_const_t){.name="domain", .value=decode_domain},
                         &(label_pair_const_t){.name="service", .value=service}, NULL);

    sd_bus_message_unref(reply);
    return 0;
}

static int sssd_list_services(sd_bus *bus, char *domain, char *decode_domain)
{
    sd_bus_message *reply = NULL;
    sd_bus_error error = SD_BUS_ERROR_NULL;

    int status = sd_bus_call_method(bus, "org.freedesktop.sssd.infopipe", domain,
                                         "org.freedesktop.sssd.infopipe.Domains.Domain",
                                         "ListServices", &error, &reply, NULL);

    if (status < 0) {
        PLUGIN_ERROR("Call to org.freedesktop.sssd.infopipe.Domains.Domain.ListServices "
                     "failed: %s.", BUS_ERROR_MSG(&error, status));
        sd_bus_error_free(&error);
        return -1;
    }

    sd_bus_error_free(&error);

    status = sd_bus_message_enter_container(reply, SD_BUS_TYPE_ARRAY, "s");
    if (status < 0) {
        PLUGIN_ERROR("Failed to enter container from response "
                     "of org.freedesktop.sssd.infopipe.Domains.Domain.ListServices.");
        sd_bus_message_unref(reply);
        return -1;
    }

    while (true) {
        char *service = NULL;

        status = sd_bus_message_read(reply, "s", &service);
        if (status <= 0)
            break;

        sssd_active_server(bus, domain, decode_domain, service);
    }

    sd_bus_message_exit_container(reply);
    sd_bus_message_unref(reply);

    return 0;
}

static int sssd_is_online(sd_bus *bus, char *domain)
{
    sd_bus_message *reply = NULL;
    sd_bus_error error = SD_BUS_ERROR_NULL;

    int status = sd_bus_call_method(bus, "org.freedesktop.sssd.infopipe", domain,
                                         "org.freedesktop.sssd.infopipe.Domains.Domain",
                                         "IsOnline", &error, &reply, NULL);
    if (status < 0)  {
        PLUGIN_ERROR("Call to org.freedesktop.sssd.infopipe.Domains.Domain.IsOnline failed: %s.",
                     BUS_ERROR_MSG(&error, status));
        sd_bus_error_free(&error);
        return -1;
    }

    sd_bus_error_free(&error);

    int online = 0;
    status = sd_bus_message_read(reply, "b", &online);
    if (status <= 0) {
        PLUGIN_ERROR("Failed to read reponse from "
                     "org.freedesktop.sssd.infopipe.Domains.Domain.IsOnline.");
        sd_bus_message_unref(reply);
        return -1;
    }

    sd_bus_message_unref(reply);
    return 0;
}

static int sssd_list_domains(sd_bus *bus)
{
    sd_bus_message *reply = NULL;
    sd_bus_error error = SD_BUS_ERROR_NULL;

    int status = sd_bus_call_method(bus, "org.freedesktop.sssd.infopipe",
                                         "/org/freedesktop/sssd/infopipe",
                                         "org.freedesktop.sssd.infopipe",
                                         "ListDomains", &error, &reply, NULL);
    if (status < 0) {
       PLUGIN_ERROR("Call to org.freedesktop.sssd.infopipe.ListDomains failed: %s.",
                     BUS_ERROR_MSG(&error, status));
       sd_bus_error_free(&error);
       return -1;
    }

    sd_bus_error_free(&error);

    status = sd_bus_message_enter_container(reply, SD_BUS_TYPE_ARRAY, "o");
    if (status < 0) {
        PLUGIN_ERROR("Failed to enter container from response "
                     "of org.freedesktop.sssd.infopipe.ListDomains.");
        sd_bus_message_unref(reply);
        return -1;
    }

    while (true) {
        char *domain = NULL;

        status = sd_bus_message_read(reply, "o", &domain);
        if (status <= 0)
            break;
        if (domain == NULL)
            break;

        char *decode_domain = NULL;
        status = sd_bus_path_decode(domain, "/org/freedesktop/sssd/infopipe/Domains",
                                    &decode_domain);
        if (status < 0) {
            PLUGIN_ERROR("Failed to decode domain from org.freedesktop.sssd.infopipe.ListDomains.");
            continue;
        }
        if (decode_domain == NULL) {
            PLUGIN_ERROR("Missing decode domain from org.freedesktop.sssd.infopipe.ListDomains.");
            continue;
        }

        value_t online = VALUE_GAUGE(0);
        status = sssd_is_online(bus, domain);
        if (status == 0)
            online = VALUE_GAUGE(1);

        metric_family_append(&fams[FAM_SSSD_DOMAIN_ONLINE], online, NULL,
                             &(label_pair_const_t){.name="domain", .value=decode_domain}, NULL);

        sssd_list_services(bus, domain, decode_domain);

        free(decode_domain);
    }

    sd_bus_message_exit_container(reply);
    sd_bus_message_unref(reply);

    return 0;
}

static int sssd_ping(sd_bus *bus)
{
    sd_bus_message *m = NULL;
    sd_bus_message *reply = NULL;
    sd_bus_error error = SD_BUS_ERROR_NULL;

    int status = sd_bus_message_new_method_call(bus, &m, "org.freedesktop.sssd.infopipe",
                                                         "/org/freedesktop/sssd/infopipe",
                                                         "org.freedesktop.sssd.infopipe",
                                                         "Ping");
    if (status < 0) {
        PLUGIN_ERROR("Failed to created method call to org.freedesktop.sssd.infopipe.Ping.");
        return -1;
    }

    status = sd_bus_message_append_basic(m, 's', "PING");
    if (status < 0) {
        PLUGIN_ERROR("Failed to add argument to org.freedesktop.sssd.infopipe.Ping.");
        sd_bus_message_unref(m);
        return -1;
    }

    status = sd_bus_call(bus, m, 0, &error, &reply);
    if (status < 0) {
        PLUGIN_ERROR("Call to org.freedesktop.sssd.infopipe.Ping failed: %s.",
                     BUS_ERROR_MSG(&error, status));
        sd_bus_error_free(&error);
        sd_bus_message_unref(m);
        return -1;
    }

    sd_bus_message_unref(m);
    sd_bus_error_free(&error);

    char *pong = NULL;
    status = sd_bus_message_read(reply, "s", &pong);
    if (status <= 0) {
        PLUGIN_ERROR("Failed to read reply from org.freedesktop.sssd.infopipe.Ping.");
        sd_bus_message_unref(reply);
        return -1;
    }

    if ((pong != NULL) && (strcmp(pong, "PONG") == 0)) {
        sd_bus_message_unref(reply);
        return 0;
    }

    sd_bus_message_unref(reply);
    return -1;
}

static int sssd_read(void)
{
    sd_bus *bus = NULL;

    if (sd_booted() <= 0)
        return -1;

    sd_bus_default_system(&bus);

    int status = sssd_ping(bus);
    if (status != 0) {
        sd_bus_unref(bus);
        metric_family_append(&fams[FAM_SSSD_UP], VALUE_GAUGE(0), NULL, NULL);
        plugin_dispatch_metric_family(&fams[FAM_SSSD_UP], 0);
        return 0;
    }

    metric_family_append(&fams[FAM_SSSD_UP], VALUE_GAUGE(1), NULL, NULL);

    sssd_list_domains(bus);

    sd_bus_unref(bus);

    plugin_dispatch_metric_family_array(fams, FAM_SSSD_MAX, 0);

    return 0;
}

void module_register(void)
{
    plugin_register_read("sssd", sssd_read);
}
