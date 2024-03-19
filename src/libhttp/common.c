// SPDX-License-Identifier: GPL-2.0-only

#include "libhttp/http.h"

const char *http_get_status_reason(http_status_code_t status_code)
{
    switch (status_code) {
    case HTTP_STATUS_100:
        return "Continue";
    case HTTP_STATUS_101:
        return "Switching Protocols";
    case HTTP_STATUS_103:
        return "Early Hints";
    case HTTP_STATUS_200:
        return "OK";
    case HTTP_STATUS_201:
        return "Created";
    case HTTP_STATUS_202:
        return "Accepted";
    case HTTP_STATUS_203:
        return "Non-Authoritative Information";
    case HTTP_STATUS_204:
        return "No Content";
    case HTTP_STATUS_205:
        return "Reset Content";
    case HTTP_STATUS_206:
        return "Partial Content";
    case HTTP_STATUS_300:
        return "Multiple Choices";
    case HTTP_STATUS_301:
        return "Moved Permanently";
    case HTTP_STATUS_302:
        return "Found";
    case HTTP_STATUS_303:
        return "See Other";
    case HTTP_STATUS_304:
        return "Not Modified";
    case HTTP_STATUS_305:
        return "Use Proxy";
    case HTTP_STATUS_307:
        return "Temporary Redirect";
    case HTTP_STATUS_308:
        return "Permanent Redirect";
    case HTTP_STATUS_400:
        return "Bad Request";
    case HTTP_STATUS_401:
        return "Unauthorized";
    case HTTP_STATUS_402:
        return "Payment Required";
    case HTTP_STATUS_403:
        return "Forbidden";
    case HTTP_STATUS_404:
        return "Not Found";
    case HTTP_STATUS_405:
        return "Method Not Allowed";
    case HTTP_STATUS_406:
        return "Not Acceptable";
    case HTTP_STATUS_407:
        return "Proxy Authentication Required";
    case HTTP_STATUS_408:
        return "Request Timeout";
    case HTTP_STATUS_409:
        return "Conflict";
    case HTTP_STATUS_410:
        return "Gone";
    case HTTP_STATUS_411:
        return "Length Required";
    case HTTP_STATUS_412:
        return "Precondition Failed";
    case HTTP_STATUS_413:
        return "Payload Too Large";
    case HTTP_STATUS_414:
        return "URI Too Long";
    case HTTP_STATUS_415:
        return "Unsupported Media Type";
    case HTTP_STATUS_416:
        return "Range Not Satisfiable";
    case HTTP_STATUS_417:
        return "Expectation Failed";
    case HTTP_STATUS_421:
        return "Misdirected Request";
    case HTTP_STATUS_425:
        return "Too Early";
    case HTTP_STATUS_426:
        return "Upgrade Required";
    case HTTP_STATUS_428:
        return "Precondition Required";
    case HTTP_STATUS_429:
        return "Too Many Requests";
    case HTTP_STATUS_431:
        return "Request Header Fields Too Large";
    case HTTP_STATUS_451:
        return "Unavailable For Legal Reasons";
    case HTTP_STATUS_500:
        return "Internal Server Error";
    case HTTP_STATUS_501:
        return "Not Implemented";
    case HTTP_STATUS_502:
        return "Bad Gateway";
    case HTTP_STATUS_503:
        return "Service Unavailable";
    case HTTP_STATUS_504:
        return "Gateway Timeout";
    case HTTP_STATUS_505:
        return "HTTP Version Not Supported";
    case HTTP_STATUS_506:
        return "Variant Also Negotiates";
    case HTTP_STATUS_510:
        return "Not Extended";
    case HTTP_STATUS_511:
        return "Network Authentication Required";
    }
    return NULL;
}

int http_get_status(http_status_code_t status_code)
{
    switch (status_code) {
    case HTTP_STATUS_100:
        return 100;
    case HTTP_STATUS_101:
        return 101;
    case HTTP_STATUS_103:
        return 103;
    case HTTP_STATUS_200:
        return 200;
    case HTTP_STATUS_201:
        return 201;
    case HTTP_STATUS_202:
        return 202;
    case HTTP_STATUS_203:
        return 203;
    case HTTP_STATUS_204:
        return 204;
    case HTTP_STATUS_205:
        return 205;
    case HTTP_STATUS_206:
        return 206;
    case HTTP_STATUS_300:
        return 300;
    case HTTP_STATUS_301:
        return 301;
    case HTTP_STATUS_302:
        return 302;
    case HTTP_STATUS_303:
        return 303;
    case HTTP_STATUS_304:
        return 304;
    case HTTP_STATUS_305:
        return 305;
    case HTTP_STATUS_307:
        return 307;
    case HTTP_STATUS_308:
        return 308;
    case HTTP_STATUS_400:
        return 400;
    case HTTP_STATUS_401:
        return 401;
    case HTTP_STATUS_402:
        return 402;
    case HTTP_STATUS_403:
        return 403;
    case HTTP_STATUS_404:
        return 404;
    case HTTP_STATUS_405:
        return 405;
    case HTTP_STATUS_406:
        return 406;
    case HTTP_STATUS_407:
        return 407;
    case HTTP_STATUS_408:
        return 408;
    case HTTP_STATUS_409:
        return 409;
    case HTTP_STATUS_410:
        return 410;
    case HTTP_STATUS_411:
        return 411;
    case HTTP_STATUS_412:
        return 412;
    case HTTP_STATUS_413:
        return 413;
    case HTTP_STATUS_414:
        return 414;
    case HTTP_STATUS_415:
        return 415;
    case HTTP_STATUS_416:
        return 416;
    case HTTP_STATUS_417:
        return 417;
    case HTTP_STATUS_421:
        return 421;
    case HTTP_STATUS_425:
        return 425;
    case HTTP_STATUS_426:
        return 426;
    case HTTP_STATUS_428:
        return 428;
    case HTTP_STATUS_429:
        return 429;
    case HTTP_STATUS_431:
        return 431;
    case HTTP_STATUS_451:
        return 451;
    case HTTP_STATUS_500:
        return 500;
    case HTTP_STATUS_501:
        return 501;
    case HTTP_STATUS_502:
        return 502;
    case HTTP_STATUS_503:
        return 503;
    case HTTP_STATUS_504:
        return 504;
    case HTTP_STATUS_505:
        return 505;
    case HTTP_STATUS_506:
        return 506;
    case HTTP_STATUS_510:
        return 510;
    case HTTP_STATUS_511:
        return 511;
    }
    return HTTP_STATUS_500;
}

http_status_code_t http_get_status_code(int status)
{
    switch (status) {
    case 100:
        return HTTP_STATUS_100;
    case 101:
        return HTTP_STATUS_101;
    case 103:
        return HTTP_STATUS_103;
    case 200:
        return HTTP_STATUS_200;
    case 201:
        return HTTP_STATUS_201;
    case 202:
        return HTTP_STATUS_202;
    case 203:
        return HTTP_STATUS_203;
    case 204:
        return HTTP_STATUS_204;
    case 205:
        return HTTP_STATUS_205;
    case 206:
        return HTTP_STATUS_206;
    case 300:
        return HTTP_STATUS_300;
    case 301:
        return HTTP_STATUS_301;
    case 302:
        return HTTP_STATUS_302;
    case 303:
        return HTTP_STATUS_303;
    case 304:
        return HTTP_STATUS_304;
    case 305:
        return HTTP_STATUS_305;
    case 307:
        return HTTP_STATUS_307;
    case 308:
        return HTTP_STATUS_308;
    case 400:
        return HTTP_STATUS_400;
    case 401:
        return HTTP_STATUS_401;
    case 402:
        return HTTP_STATUS_402;
    case 403:
        return HTTP_STATUS_403;
    case 404:
        return HTTP_STATUS_404;
    case 405:
        return HTTP_STATUS_405;
    case 406:
        return HTTP_STATUS_406;
    case 407:
        return HTTP_STATUS_407;
    case 408:
        return HTTP_STATUS_408;
    case 409:
        return HTTP_STATUS_409;
    case 410:
        return HTTP_STATUS_410;
    case 411:
        return HTTP_STATUS_411;
    case 412:
        return HTTP_STATUS_412;
    case 413:
        return HTTP_STATUS_413;
    case 414:
        return HTTP_STATUS_414;
    case 415:
        return HTTP_STATUS_415;
    case 416:
        return HTTP_STATUS_416;
    case 417:
        return HTTP_STATUS_417;
    case 421:
        return HTTP_STATUS_421;
    case 425:
        return HTTP_STATUS_425;
    case 426:
        return HTTP_STATUS_426;
    case 428:
        return HTTP_STATUS_428;
    case 429:
        return HTTP_STATUS_429;
    case 431:
        return HTTP_STATUS_431;
    case 451:
        return HTTP_STATUS_451;
    case 500:
        return HTTP_STATUS_500;
    case 501:
        return HTTP_STATUS_501;
    case 502:
        return HTTP_STATUS_502;
    case 503:
        return HTTP_STATUS_503;
    case 504:
        return HTTP_STATUS_504;
    case 505:
        return HTTP_STATUS_505;
    case 506:
        return HTTP_STATUS_506;
    case 510:
        return HTTP_STATUS_510;
    case 511:
        return HTTP_STATUS_511;
    }
    return 999;
}

const char *http_get_method(http_method_t method)
{
    switch(method) {
    case HTTP_METHOD_GET:
        return "GET";
    case HTTP_METHOD_PUT:
        return "PUT";
    case HTTP_METHOD_POST:
        return "POST";
    case HTTP_METHOD_HEAD:
        return "HEAD";
    case HTTP_METHOD_PATCH:
        return "PATH";
    case HTTP_METHOD_TRACE:
        return "TRACE";
    case HTTP_METHOD_DELETE:
        return "DELETE";
    case HTTP_METHOD_OPTIONS:
        return "OPTIONS";
    case HTTP_METHOD_CONNECT:
        return "CONNECT";
    case HTTP_METHOD_UNKNOW:
        return NULL;
    }
    return NULL;
}

const char *http_get_version(http_version_t version)
{
    switch(version) {
    case HTTP_VERSION_1_0:
        return "HTTP/1.0";
    case HTTP_VERSION_1_1:
        return "HTTP/1.1";
    case HTTP_VERSION_UNKNOW:
        return NULL;
    }
    return NULL;
}

