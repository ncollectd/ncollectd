// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2025 Manuel Sanmartín
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "plugin.h"
#include "libutils/common.h"
#include "libutils/strbuf.h"

#include <libpq-fe.h>

static void normalize (char *unit, double *value, char **suffix)
{
    switch (unit[0]){
    case 'u':
        if ((unit[1] == 's') && (unit[2] == '\0')) {
            *value /= 1000000;
            *suffix = "_seconds";
        }
        break;
    case 's':
        if (unit[1] == '\0')
            *suffix = "_seconds";
        break;
    case 'm':
        if ((unit[1] == 's') && (unit[2] == '\0')) {
            *value /= 1000;
            *suffix = "_seconds";
        } else if ((unit[1] == 'i') && (unit[2] == 'n') && (unit[3] == '\0')) {
            *value *= 60;
            *suffix = "_seconds";
        }
        break;
    case 'h':
        if (unit[1] == '\0') {
            *value *= 3600;
            *suffix = "_seconds";
        }
        break;
    case 'd':
        if (unit[1] == '\0') {
            *value *= 86400;
            *suffix = "_seconds";
        }
        break;
    case 'B':
        if (unit[1] == '\0')
            *suffix = "_seconds";
        break;
    case 'k':
        if ((unit[1] == 'B') && (unit[2] == '\0')) {
            *value *= 1024;
            *suffix = "_bytes";
        }
        break;
    case 'M':
        if ((unit[1] == 'B') && (unit[2] == '\0')) {
            *value *= 1048576;
            *suffix = "_bytes";
        }
        break;
    case 'G':
        if ((unit[1] == 'B') && (unit[2] == '\0')) {
            *value *= 1073741824;
            *suffix = "_bytes";
        }
        break;
    case 'T':
        if ((unit[1] == 'B') && (unit[2] == '\0')) {
            *value *= 1099511627776L;
            *suffix = "_bytes";
        }
        break;
    case '1':
        if ((unit[1] == 'k') && (unit[2] == 'B') && (unit[3] == '\0')) {
            *value *= 1024;
            *suffix = "_bytes";
        } else if ((unit[1] == '6') && (unit[2] == 'k') && (unit[3] == 'B') && (unit[4] == '\0')) {
            *value *= 16384;
            *suffix = "_bytes";
        } else if ((unit[1] == '6') && (unit[2] == 'M') && (unit[3] == 'B') && (unit[4] == '\0')) {
            *value *= 16777216;
            *suffix = "_bytes";
        }
        break;
    case '2':
        if ((unit[1] == 'k') && (unit[2] == 'B') && (unit[3] == '\0')) {
            *value *= 2048;
            *suffix = "_bytes";
        }
        break;
    case '3':
        if ((unit[1] == '2') && (unit[2] == 'k') && (unit[3] == 'B') && (unit[4] == '\0')) {
            *value *= 32768;
            *suffix = "_bytes";
        } else if ((unit[1] == '2') && (unit[2] == 'M') && (unit[3] == 'B') && (unit[4] == '\0')) {
            *value *= 33554432;
            *suffix = "_bytes";
        }
        break;
    case '4':
        if ((unit[1] == 'k') && (unit[2] == 'B') && (unit[3] == '\0')) {
            *value *= 4096;
            *suffix = "_bytes";
        }
        break;
    case '6':
        if ((unit[1] == '4') && (unit[2] == 'k') && (unit[3] == 'B') && (unit[4] == '\0')) {
            *value *= 65536;
            *suffix = "_bytes";
        } else if ((unit[1] == '4') && (unit[2] == 'M') && (unit[3] == 'B') && (unit[4] == '\0')) {
            *value *= 67108864;
            *suffix = "_bytes";
        }
        break;
    case '8':
        if ((unit[1] == 'k') && (unit[2] == 'B') && (unit[3] == '\0')) {
            *value *= 8192;
            *suffix = "_bytes";
        }
        break;
    }
}

int pg_settings(PGconn *conn, int version, label_set_t *labels, cdtime_t submit)
{
    if (version < 70300)
        return 0;

    char *stmt = "SELECT name, setting, unit, short_desc, vartype "
                 "  FROM pg_settings "
                 " WHERE vartype IN ('bool', 'integer', 'real')";

    PGresult *res = PQprepare(conn, "", stmt, 0, NULL);
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        PLUGIN_ERROR("PQprepare failed: %s", PQerrorMessage(conn));
        PQclear(res);
        return -1;
    }

    PQclear(res);

    res = PQexecPrepared(conn, "", 0, NULL, NULL, NULL, 0);
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        PLUGIN_ERROR("PQexecPrepared failed: %s", PQerrorMessage(conn));
        PQclear(res);
        return -1;
    }

    int fields = PQnfields(res);
    if (fields < 5) {
        PQclear(res);
        return 0;
    }

    for (int i = 0; i < PQntuples(res); i++) {
        if (PQgetisnull(res, i, 0))
            continue;
        char *pg_name = PQgetvalue(res, i, 0);

        if (PQgetisnull(res, i, 1))
            continue;
        char *pg_setting = PQgetvalue(res, i, 1);

        char *pg_unit = NULL;
        if (!PQgetisnull(res, i, 2))
            pg_unit = PQgetvalue(res, i, 2);

        char *pg_help = NULL;
        if (!PQgetisnull(res, i, 3))
            pg_help = PQgetvalue(res, i, 3);

        if (PQgetisnull(res, i, 4))
            continue;
        char *pg_vartype = PQgetvalue(res, i, 4);

        char *suffix = NULL;
        value_t value = {0};

        if (strcmp(pg_vartype, "bool") == 0) {
            value = VALUE_GAUGE(strcmp(pg_setting, "on") == 0 ? 1 : 0);
        } else if ((strcmp(pg_vartype, "integer") == 0) || (strcmp(pg_vartype, "real") == 0)) {
            char *endptr = NULL;
            double number = (double)strtod(pg_setting, &endptr);
            if (pg_setting == endptr) {
                PLUGIN_DEBUG("Failed to parse string as double: \"%s\".", pg_setting);
                continue;
            } else if ((NULL != endptr) && ('\0' != *endptr)) {
                PLUGIN_DEBUG("Ignoring trailing garbage \"%s\" after double value. "
                             "Input string was \"%s\".", endptr, pg_setting);
            }

            if ((pg_unit != NULL) && (pg_unit[0] != '\0'))
                normalize(pg_unit, &number, &suffix);

            value = VALUE_GAUGE(number);
        } else {
            continue;
        }

        char name[512];
        sstrncpy(name, pg_name, sizeof(name));
        char *c = name;
        while (*c != '\0') {
            if ((*c == '.') || (*c == '-'))
                *c = '_';
            c++;
        }

        char full_name[1024];
        strbuf_t full_buf = STRBUF_CREATE_STATIC(full_name);
        strbuf_putstr(&full_buf, "pg_settings_");
        strbuf_putstr(&full_buf, name);
        if (suffix != NULL)
            strbuf_putstr(&full_buf, suffix);

        metric_family_t fam = {
            .name = full_buf.ptr,
            .type = METRIC_TYPE_GAUGE,
            .help = pg_help
        };

        metric_family_append(&fam, value, labels, NULL);
        plugin_dispatch_metric_family(&fam, submit);
    }

    PQclear(res);

    return 0;
}
