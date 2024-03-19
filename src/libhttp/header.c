// SPDX-License-Identifier: GPL-2.0-only

#include "libutils/common.h"
#include "libhttp/header.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "libhttp/lookup.h"

int http_header_append(http_header_set_t *set, http_header_name_t header_name,
                                               const char *name, size_t name_len,
                                               const char *value, size_t value_len)
{
    char *value_dup = sstrndup(value, value_len);
    if (value_dup == NULL)
       return -1;

    char *name_dup = NULL;
    if (header_name == HTTP_HEADER_UNKOWN) {
        name_dup = sstrndup(name, name_len);
        if (name_dup == NULL) {
            free(value_dup);
            return -1;
        }
    }

    for (size_t i=0; i < set->num; i++) {
        if (set->ptr[i].header_name == header_name) {
            if (header_name == HTTP_HEADER_UNKOWN) {
                if (strcasecmp(set->ptr[i].name, name_dup) == 0) {
                    free(set->ptr[i].value);
                    set->ptr[i].value = value_dup;
                    free(name_dup);
                    return 0;
                }
            } else {
                free(set->ptr[i].value);
                set->ptr[i].value = value_dup;
                free(name_dup);
                return 0;
            }
        }
    }

    http_header_t *tmp = realloc(set->ptr, sizeof(set->ptr[0])*(set->num+1));
    if (tmp == NULL) {
        free(name_dup);
        free(value_dup);
        return -1;
    }

    set->ptr = tmp;
    set->ptr[set->num].header_name = header_name;
    set->ptr[set->num].name = name_dup;
    set->ptr[set->num].value = value_dup;
    set->num++;

    return 0;
}

char *http_header_get(http_header_set_t *set, http_header_name_t header_name, char *name)
{
    for (size_t i=0; i < set->num; i++) {
        if (set->ptr[i].header_name == header_name) {
            if (header_name == HTTP_HEADER_UNKOWN) {
                if (strcasecmp(set->ptr[i].name, name) == 0) {
                    return set->ptr[i].value;
                }
            } else {
                return set->ptr[i].value;
            }
        }
    }

    return NULL;
}

void http_header_reset(http_header_set_t *set)
{
    for (size_t i=0; i < set->num; i++) {
        free(set->ptr[i].name);
        free(set->ptr[i].value);
    }
    free(set->ptr);
    set->ptr = NULL;
    set->num = 0;
}

const char *http_get_header(http_header_name_t header)
{
    switch(header) {
    case HTTP_HEADER_UNKOWN:
        return NULL;
    case HTTP_HEADER_ACCEPT:
        return "Accept";
    case HTTP_HEADER_ACCEPT_CH:
        return "Accept-CH";
    case HTTP_HEADER_ACCEPT_CHARSET:
        return "Accept-Charset";
    case HTTP_HEADER_ACCEPT_DATETIME:
        return "Accept-Datetime";
    case HTTP_HEADER_ACCEPT_ENCODING:
        return "Accept-Encoding";
    case HTTP_HEADER_ACCEPT_LANGUAGE:
        return "Accept-Language";
    case HTTP_HEADER_ACCEPT_PATCH:
        return "Accept-Patch";
    case HTTP_HEADER_ACCEPT_RANGES:
        return "Accept-Ranges";
    case HTTP_HEADER_ACCESS_CONTROL_ALLOW_CREDENTIALS:
        return "Access-Control-Allow-Credentials";
    case HTTP_HEADER_ACCESS_CONTROL_ALLOW_HEADERS:
        return "Access-Control-Allow-Headers";
    case HTTP_HEADER_ACCESS_CONTROL_ALLOW_METHODS:
        return "Access-Control-Allow-Methods";
    case HTTP_HEADER_ACCESS_CONTROL_ALLOW_ORIGIN:
        return "Access-Control-Allow-Origin";
    case HTTP_HEADER_ACCESS_CONTROL_EXPOSE_HEADERS:
        return "Access-Control-Expose-Headers";
    case HTTP_HEADER_ACCESS_CONTROL_MAX_AGE:
        return "Access-Control-Max-Age";
    case HTTP_HEADER_ACCESS_CONTROL_REQUEST_HEADERS:
        return "Access-Control-Request-Headers";
    case HTTP_HEADER_ACCESS_CONTROL_REQUEST_METHOD:
        return "Access-Control-Request-Method";
    case HTTP_HEADER_AGE:
        return "Age";
    case HTTP_HEADER_A_IM:
        return "A-IM";
    case HTTP_HEADER_ALLOW:
        return "Allow";
    case HTTP_HEADER_ALT_SVC:
        return "Alt-Svc";
    case HTTP_HEADER_AUTHORIZATION:
        return "Authorization";
    case HTTP_HEADER_CACHE_CONTROL:
        return "Cache-Control";
    case HTTP_HEADER_CONNECTION:
        return "Connection";
    case HTTP_HEADER_CONTENT_DISPOSITION:
        return "Content-Disposition";
    case HTTP_HEADER_CONTENT_ENCODING:
        return "Content-Encoding";
    case HTTP_HEADER_CONTENT_LANGUAGE:
        return "Content-Language";
    case HTTP_HEADER_CONTENT_LENGTH:
        return "Content-Length";
    case HTTP_HEADER_CONTENT_LOCATION:
        return "Content-Location";
    case HTTP_HEADER_CONTENT_RANGE:
        return "Content-Range";
    case HTTP_HEADER_CONTENT_SECURITY_POLICY:
        return "Content-Security-Policy";
    case HTTP_HEADER_CONTENT_TYPE:
        return "Content-Type";
    case HTTP_HEADER_COOKIE:
        return "Cookie";
    case HTTP_HEADER_DATE:
        return "Date";
    case HTTP_HEADER_DELTA_BASE:
        return "Delta-Base";
    case HTTP_HEADER_DNT:
        return "DNT";
    case HTTP_HEADER_ETAG:
        return "ETag";
    case HTTP_HEADER_EXPECT:
        return "Expect";
    case HTTP_HEADER_EXPECT_CT:
        return "Expect-CT";
    case HTTP_HEADER_EXPIRES:
        return "Expires";
    case HTTP_HEADER_FORWARDED:
        return "Forwarded";
    case HTTP_HEADER_FROM:
        return "From";
    case HTTP_HEADER_FRONT_END_HTTPS:
        return "Front-End-Https";
    case HTTP_HEADER_HOST:
        return "Host";
    case HTTP_HEADER_IF_MATCH:
        return "If-Match";
    case HTTP_HEADER_IF_MODIFIED_SINCE:
        return "If-Modified-Since";
    case HTTP_HEADER_IF_NONE_MATCH:
        return "If-None-Match";
    case HTTP_HEADER_IF_RANGE:
        return "If-Range";
    case HTTP_HEADER_IF_UNMODIFIED_SINCE:
        return "If-Unmodified-Since";
    case HTTP_HEADER_IM:
        return "IM";
    case HTTP_HEADER_LAST_MODIFIED:
        return "Last-Modified";
    case HTTP_HEADER_LINK:
        return "Link";
    case HTTP_HEADER_LOCATION:
        return "Location";
    case HTTP_HEADER_MAX_FORWARDS:
        return "Max-Forwards";
    case HTTP_HEADER_NEL:
        return "NEL";
    case HTTP_HEADER_ORIGIN:
        return "Origin";
    case HTTP_HEADER_P3P:
        return "P3P";
    case HTTP_HEADER_PERMISSIONS_POLICY:
        return "Permissions-Policy";
    case HTTP_HEADER_PRAGMA:
        return "Pragma";
    case HTTP_HEADER_PREFER:
        return "Prefer";
    case HTTP_HEADER_PREFERENCE_APPLIED:
        return "Preference-Applied";
    case HTTP_HEADER_PROXY_AUTHENTICATE:
        return "Proxy-Authenticate";
    case HTTP_HEADER_PROXY_AUTHORIZATION:
        return "Proxy-Authorization";
    case HTTP_HEADER_PROXY_CONNECTION:
        return "Proxy-Connection";
    case HTTP_HEADER_PUBLIC_KEY_PINS:
        return "Public-Key-Pins";
    case HTTP_HEADER_RANGE:
        return "Range";
    case HTTP_HEADER_REFERER:
        return "Referer";
    case HTTP_HEADER_REFRESH:
        return "Refresh";
    case HTTP_HEADER_REPORT_TO:
        return "Report-To";
    case HTTP_HEADER_RETRY_AFTER:
        return "Retry-After";
    case HTTP_HEADER_SAVE_DATA:
        return "Save-Data";
    case HTTP_HEADER_SERVER:
        return "Server";
    case HTTP_HEADER_SET_COOKIE:
        return "Set-Cookie";
    case HTTP_HEADER_STATUS:
        return "Status";
    case HTTP_HEADER_STRICT_TRANSPORT_SECURITY:
        return "Strict-Transport-Security";
    case HTTP_HEADER_TE:
        return "TE";
    case HTTP_HEADER_TIMING_ALLOW_ORIGIN:
        return "Timing-Allow-Origin";
    case HTTP_HEADER_TK:
        return "Tk";
    case HTTP_HEADER_TRAILER:
        return "Trailer";
    case HTTP_HEADER_TRANSFER_ENCODING:
        return "Transfer-Encoding";
    case HTTP_HEADER_UPGRADE:
        return "Upgrade";
    case HTTP_HEADER_UPGRADE_INSECURE_REQUESTS:
        return "Upgrade-Insecure-Requests";
    case HTTP_HEADER_USER_AGENT:
        return "User-Agent";
    case HTTP_HEADER_VARY:
        return "Vary";
    case HTTP_HEADER_VIA:
        return "Via";
    case HTTP_HEADER_WARNING:
        return "Warning";
    case HTTP_HEADER_WWW_AUTHENTICATE:
        return "WWW-Authenticate";
    case HTTP_HEADER_X_ATT_DEVICEID:
        return "X-ATT-DeviceId";
    case HTTP_HEADER_X_CONTENT_DURATION:
        return "X-Content-Duration";
    case HTTP_HEADER_X_CONTENT_SECURITY_POLICY:
        return "X-Content-Security-Policy";
    case HTTP_HEADER_X_CONTENT_TYPE_OPTIONS:
        return "X-Content-Type-Options";
    case HTTP_HEADER_X_CORRELATION_ID:
        return "X-Correlation-ID";
    case HTTP_HEADER_X_CSRF_TOKEN:
        return "X-Csrf-Token";
    case HTTP_HEADER_X_FORWARDED_FOR:
        return "X-Forwarded-For";
    case HTTP_HEADER_X_FORWARDED_HOST:
        return "X-Forwarded-Host";
    case HTTP_HEADER_X_FORWARDED_PROTO:
        return "X-Forwarded-Proto";
    case HTTP_HEADER_X_HTTP_METHOD_OVERRIDE:
        return "X-Http-Method-Override";
    case HTTP_HEADER_X_POWERED_BY:
        return "X-Powered-By";
    case HTTP_HEADER_X_REDIRECT_BY:
        return "X-Redirect-By";
    case HTTP_HEADER_X_REQUESTED_WITH:
        return "X-Requested-With";
    case HTTP_HEADER_X_REQUEST_ID:
        return "X-Request-ID";
    case HTTP_HEADER_X_UA_COMPATIBLE:
        return "X-UA-Compatible";
    case HTTP_HEADER_X_UIDH:
        return "X-UIDH";
    case HTTP_HEADER_X_WAP_PROFILE:
        return "X-Wap-Profile";
    case HTTP_HEADER_X_WEBKIT_CSP:
        return "X-WebKit-CSP";
    case HTTP_HEADER_X_XSS_PROTECTION:
        return "X-XSS-Protection";
    }
    return NULL;
}

http_header_name_t http_get_header_name(const char *hdr, size_t len)
{
    const struct http_header_lookup *lookup = http_header_lookup (hdr, len);
    if (lookup != NULL)
        return lookup->hdr_name;

    return HTTP_HEADER_UNKOWN;
}
