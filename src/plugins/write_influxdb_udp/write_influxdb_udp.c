// SPDX-License-Identifier: GPL-2.0-only
// Copyright (C) 2007-2009  Florian octo Forster
// Copyright (C) 2009       Aman Gupta
// Copyright (C) 2019       Carlos Peon Costa
// Authors:
//   Florian octo Forster <octo at collectd.org>
//   Aman Gupta <aman at tmm1.net>
//   Carlos Peon Costa <carlospeon at gmail.com>

#include "collectd.h"

#include "plugin.h"
#include "utils/common/common.h"
#include "utils_cache.h"
#include "utils_complain.h"

#if HAVE_NETDB_H
#include <netdb.h>
#endif
#if HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#if HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif
#if HAVE_POLL_H
#include <poll.h>
#endif
#if HAVE_NET_IF_H
#include <net/if.h>
#endif

struct sockent_client {
  int fd;
  struct sockaddr_storage *addr;
  socklen_t addrlen;
  cdtime_t next_resolve_reconnect;
  cdtime_t resolve_interval;
  struct sockaddr_storage *bind_addr;
};

typedef struct sockent {
  char *node;
  char *service;
  int interface;
  struct sockent_client client;
} sockent_t;

#define NET_DEFAULT_PACKET_SIZE 1452
#define NET_DEFAULT_PORT "8089"

/*
 * Private variables
 */

static int wifxudp_config_ttl;
static size_t wifxudp_config_packet_size = NET_DEFAULT_PACKET_SIZE;
static bool wifxudp_config_store_rates;

static sockent_t *sending_socket;

/* Buffer in which to-be-sent network packets are constructed. */
static char *send_buffer;
static char *send_buffer_ptr;
static int send_buffer_fill;
static cdtime_t send_buffer_last_update;
static pthread_mutex_t send_buffer_lock = PTHREAD_MUTEX_INITIALIZER;

static int set_ttl(const sockent_t *se, const struct addrinfo *ai) {

  if ((wifxudp_config_ttl < 1) || (wifxudp_config_ttl > 255))
    return -1;

  if (ai->ai_family == AF_INET) {
    struct sockaddr_in *addr = (struct sockaddr_in *)ai->ai_addr;
    int optname;

    if (IN_MULTICAST(ntohl(addr->sin_addr.s_addr)))
      optname = IP_MULTICAST_TTL;
    else
      optname = IP_TTL;

    if (setsockopt(se->client.fd, IPPROTO_IP, optname, &wifxudp_config_ttl,
                   sizeof(wifxudp_config_ttl)) != 0) {
      ERROR("write_influxdb_udp plugin: setsockopt (ipv4-ttl): %s", STRERRNO);
      return -1;
    }
  } else if (ai->ai_family == AF_INET6) {
    /* Useful example:
     * http://gsyc.escet.urjc.es/~eva/IPv6-web/examples/mcast.html */
    struct sockaddr_in6 *addr = (struct sockaddr_in6 *)ai->ai_addr;
    int optname;

    if (IN6_IS_ADDR_MULTICAST(&addr->sin6_addr))
      optname = IPV6_MULTICAST_HOPS;
    else
      optname = IPV6_UNICAST_HOPS;

    if (setsockopt(se->client.fd, IPPROTO_IPV6, optname, &wifxudp_config_ttl,
                   sizeof(wifxudp_config_ttl)) != 0) {
      ERROR("write_influxdb_udp plugin: setsockopt(ipv6-ttl): %s", STRERRNO);
      return -1;
    }
  }

  return 0;
} /* int set_ttl */

static int bind_socket_to_addr(sockent_t *se, const struct addrinfo *ai) {

  if (se->client.bind_addr == NULL)
    return 0;

  char pbuffer[64];

  if (ai->ai_family == AF_INET) {
    struct sockaddr_in *addr = (struct sockaddr_in *)(se->client.bind_addr);
    inet_ntop(AF_INET, &(addr->sin_addr), pbuffer, 64);

    if (bind(se->client.fd, (struct sockaddr *)addr, sizeof(*addr)) == -1)
      return -1;
  } else if (ai->ai_family == AF_INET6) {
    struct sockaddr_in6 *addr = (struct sockaddr_in6 *)(se->client.bind_addr);
    inet_ntop(AF_INET6, &(addr->sin6_addr), pbuffer, 64);

    if (bind(se->client.fd, (struct sockaddr *)addr, sizeof(*addr)) == -1)
      return -1;
  }

  return 0;
} /* int bind_socket_to_addr */

static sockent_t *sockent_create() {
  sockent_t *se = calloc(1, sizeof(*se));
  if (se == NULL)
    return NULL;

  se->node = NULL;
  se->service = NULL;
  se->interface = 0;

  se->client.fd = -1;
  se->client.addr = NULL;
  se->client.bind_addr = NULL;
  se->client.resolve_interval = 0;
  se->client.next_resolve_reconnect = 0;

  return se;
} /* sockent_t *sockent_create */

static int sockent_client_disconnect(sockent_t *se) {

  if (se == NULL)
    return EINVAL;

  struct sockent_client *client = &se->client;
  if (client->fd >= 0) /* connected */
  {
    close(client->fd);
    client->fd = -1;
  }

  sfree(client->addr);
  client->addrlen = 0;

  return 0;
} /* int sockent_client_disconnect */

static int sockent_client_connect(sockent_t *se) {
  struct addrinfo *ai_list;
  static c_complain_t complaint = C_COMPLAIN_INIT_STATIC;
  bool reconnect = false;

  if (se == NULL)
    return EINVAL;

  struct sockent_client *client = &se->client;

  cdtime_t now = cdtime();
  if (client->resolve_interval != 0 && client->next_resolve_reconnect < now) {
    DEBUG("write_influxdb_udp plugin: "
          "Reconnecting socket, resolve_interval = %lf, "
          "next_resolve_reconnect = %lf",
          CDTIME_T_TO_DOUBLE(client->resolve_interval),
          CDTIME_T_TO_DOUBLE(client->next_resolve_reconnect));
    reconnect = true;
  }

  if (client->fd >= 0 && !reconnect) /* already connected and not stale*/
    return 0;

  struct addrinfo ai_hints = {.ai_family = AF_UNSPEC,
                              .ai_flags = AI_ADDRCONFIG,
                              .ai_protocol = IPPROTO_UDP,
                              .ai_socktype = SOCK_DGRAM};

  int status = getaddrinfo(
      se->node, (se->service != NULL) ? se->service : NET_DEFAULT_PORT,
      &ai_hints, &ai_list);
  if (status != 0) {
    c_complain(LOG_ERR, &complaint,
               "write_influxdb_udp plugin: getaddrinfo (%s, %s) failed: %s",
               (se->node == NULL) ? "(null)" : se->node,
               (se->service == NULL) ? "(null)" : se->service,
               gai_strerror(status));
    return -1;
  } else {
    c_release(LOG_NOTICE, &complaint,
              "write_influxdb_udp plugin: Successfully resolved \"%s\".",
              se->node);
  }

  for (struct addrinfo *ai_ptr = ai_list; ai_ptr != NULL;
       ai_ptr = ai_ptr->ai_next) {
    if (client->fd >= 0) /* when we reconnect */
      sockent_client_disconnect(se);

    client->fd =
        socket(ai_ptr->ai_family, ai_ptr->ai_socktype, ai_ptr->ai_protocol);
    if (client->fd < 0) {
      ERROR("write_influxdb_udp plugin: socket(2) failed: %s", STRERRNO);
      continue;
    }

    client->addr = calloc(1, sizeof(*client->addr));
    if (client->addr == NULL) {
      ERROR("write_influxdb_udp plugin: calloc failed.");
      close(client->fd);
      client->fd = -1;
      continue;
    }

    assert(sizeof(*client->addr) >= ai_ptr->ai_addrlen);
    memcpy(client->addr, ai_ptr->ai_addr, ai_ptr->ai_addrlen);
    client->addrlen = ai_ptr->ai_addrlen;

    set_ttl(se, ai_ptr);
    bind_socket_to_addr(se, ai_ptr);

    /* We don't open more than one write-socket per
     * node/service pair.. */
    break;
  }

  freeaddrinfo(ai_list);
  if (client->fd < 0)
    return -1;

  if (client->resolve_interval > 0)
    client->next_resolve_reconnect = now + client->resolve_interval;
  return 0;
} /* int sockent_client_connect */

static void free_sockent_client(struct sockent_client *sec) {
  if (sec->fd >= 0) {
    close(sec->fd);
    sec->fd = -1;
  }
  sfree(sec->addr);
  sfree(sec->bind_addr);
} /* void free_sockent_client */

static void sockent_destroy(sockent_t *se) {

  if (se != NULL) {
    sfree(se->node);
    sfree(se->service);

    free_sockent_client(&se->client);

    sfree(se);
  }
} /* void sockent_destroy */

static void write_influxdb_udp_init_buffer(void) {
  memset(send_buffer, 0, wifxudp_config_packet_size);
  send_buffer_ptr = send_buffer;
  send_buffer_fill = 0;
  send_buffer_last_update = 0;
} /* write_influxdb_udp_init_buffer */

static void write_influxdb_udp_send_buffer(const char *buffer,
                                           size_t buffer_size) {
  while (42) {
    int status = sockent_client_connect(sending_socket);
    if (status != 0)
      return;

    status =
        sendto(sending_socket->client.fd, buffer, buffer_size,
               /* flags = */ 0, (struct sockaddr *)sending_socket->client.addr,
               sending_socket->client.addrlen);
    if (status < 0) {
      if ((errno == EINTR) || (errno == EAGAIN))
        continue;

      ERROR("write_influxdb_udp plugin: "
            "sendto failed: %s. Closing sending socket.",
            STRERRNO);
      sockent_client_disconnect(sending_socket);
      return;
    }

    break;
  } /* while (42) */
} /* void write_influxdb_udp_send_buffer */

static void flush_buffer(void) {
  write_influxdb_udp_send_buffer(send_buffer, (size_t)send_buffer_fill);
  write_influxdb_udp_init_buffer();
}

static void fill_send_buffer(char const *buffer, size_t len) {
  assert(len <= wifxudp_config_packet_size);

  pthread_mutex_lock(&send_buffer_lock);
  if (wifxudp_config_packet_size - send_buffer_fill < len)
    flush_buffer();
  memcpy(send_buffer_ptr, buffer, len);
  send_buffer_fill += len;
  send_buffer_ptr += len;
  send_buffer_last_update = cdtime();
  pthread_mutex_unlock(&send_buffer_lock);
}

static int wifxudp_escape_string(char *buffer, size_t buffer_size,
                                 const char *string) {

  if ((buffer == NULL) || (string == NULL))
    return -EINVAL;

  int dst_pos = 0;

#define BUFFER_ADD(c)                                                          \
  do {                                                                         \
    if (dst_pos >= (buffer_size - 1)) {                                        \
      buffer[buffer_size - 1] = 0;                                             \
      return dst_pos++;                                                        \
    }                                                                          \
    buffer[dst_pos] = (c);                                                     \
    dst_pos++;                                                                 \
  } while (0)

  /* Escape special characters */
  for (int src_pos = 0; string[src_pos] != 0; src_pos++) {
    if ((string[src_pos] == '\\') || (string[src_pos] == ' ') ||
        (string[src_pos] == ',') || (string[src_pos] == '=') ||
        (string[src_pos] == '"')) {
      BUFFER_ADD('\\');
      BUFFER_ADD(string[src_pos]);
    } else
      BUFFER_ADD(string[src_pos]);
  } /* for */
  buffer[dst_pos] = 0;

#undef BUFFER_ADD

  return dst_pos;
} /* int wifxudp_escape_string */

static int write_influxdb_point(char *buffer, int buffer_len, metric_t metric) {
  int offset = 0;
  bool have_values = false;

#define BUFFER_ADD_ESCAPE(...)                                                 \
  do {                                                                         \
    int status = wifxudp_escape_string(buffer + offset, buffer_len - offset,   \
                                       __VA_ARGS__);                           \
    if (status < 0)                                                            \
      return status;                                                           \
    offset += status;                                                          \
    if (status >= (buffer_len - offset))                                       \
      return offset;                                                           \
  } while (0)

#define BUFFER_ADD(...)                                                        \
  do {                                                                         \
    int status = snprintf(buffer + offset, buffer_len - offset, __VA_ARGS__);  \
    if (status < 0)                                                            \
      return status;                                                           \
    offset += status;                                                          \
    if (status >= (buffer_len - offset))                                       \
      return offset;                                                           \
  } while (0)

  have_values = false;
  BUFFER_ADD_ESCAPE(metric.family->name);
  for (size_t j = 0; j < metric.label.num; j++) {
    label_pair_t label = metric.label.ptr[j];
    BUFFER_ADD(",");
    BUFFER_ADD_ESCAPE(label.name);
    BUFFER_ADD("=");
    BUFFER_ADD_ESCAPE(label.value);
  }
  BUFFER_ADD(" ");

  if (wifxudp_config_store_rates &&
      (metric.family->type == METRIC_TYPE_COUNTER)) {
    double rate;
    if (uc_get_rate(&metric, &rate) != 0) {
      WARNING("write_influxdb_udp plugin: "
              "uc_get_rate failed.");
      return -1;
    }
    if (!isnan(rate)) {
      BUFFER_ADD("value=" GAUGE_FORMAT, rate);
      have_values = true;
    }
  } else {
    switch (metric.family->type) {
    case METRIC_TYPE_UNKNOWN:
      if (metric.value.unknown.type == UNKNOWN_FLOAT64) {
        if (!isnan(metric.value.unknown.float64)) {
          BUFFER_ADD("value=" GAUGE_FORMAT, metric.value.unknown.float64);
          have_values = true;
        }
      } else if (metric.value.unknown.type == UNKNOWN_INT64) {
        BUFFER_ADD("value=%" PRIi64, metric.value.unknown.int64);
        have_values = true;
      }
    case METRIC_TYPE_GAUGE:
      if (metric.value.gauge.type == GAUGE_FLOAT64) {
        if (!isnan(metric.value.gauge.float64)) {
          BUFFER_ADD("value=" GAUGE_FORMAT, metric.value.gauge.float64);
          have_values = true;
        }
      } else if (metric.value.gauge.type == GAUGE_INT64) {
        BUFFER_ADD("value=%" PRIi64, metric.value.gauge.int64);
        have_values = true;
      }
      break;
    case METRIC_TYPE_COUNTER:
      if (metric.value.counter.type == COUNTER_UINT64) {
        BUFFER_ADD("value=" COUNTER_FORMAT, metric.value.counter.uint64);
        have_values = true;
      } else if (metric.value.counter.type == COUNTER_FLOAT64) {
        if (!isnan(metric.value.counter.float64)) {
          BUFFER_ADD("value=" GAUGE_FORMAT, metric.value.counter.float64);
          have_values = true;
        }
      }
      break;
    case METRIC_TYPE_DISTRIBUTION:
      // FIXME
      break;
    default:
      WARNING("write_influxdb_udp plugin: "
              "unknown family type.");
      return -1;
      break;
    }
  }

  if (!have_values)
    return 0;

  BUFFER_ADD(" %" PRIu64 "\n", CDTIME_T_TO_MS(metric.time));

#undef BUFFER_ADD_ESCAPE
#undef BUFFER_ADD

  return offset;
} /* int write_influxdb_point */

static int write_influxdb_udp_write(metric_family_t const *fam,
                                    user_data_t __attribute__((unused)) *
                                        user_data) {
  if (fam == NULL)
    return EINVAL;

  char buffer[NET_DEFAULT_PACKET_SIZE];
  int buffer_len = NET_DEFAULT_PACKET_SIZE;
  int offset = 0;

  if (wifxudp_config_packet_size < buffer_len)
    buffer_len = wifxudp_config_packet_size;

  for (size_t i = 0; i < fam->metric.num; i++) {
    metric_t metric = fam->metric.ptr[i];
    int status =
        write_influxdb_point(buffer + offset, buffer_len - offset, metric);
    if (status < 0) { // error
      ERROR("write_influxdb_udp plugin: write_influxdb_udp_write failed.");
      return -1;
    } else if (status >= buffer_len - offset) { // full
      fill_send_buffer(buffer, offset);
      offset = 0;
      buffer[0] = 0;
    } else {
      offset += status;
    }
  }
  fill_send_buffer(buffer, offset);
  return 0;
} /* int write_influxdb_udp_write */

static int wifxudp_config_set_ttl(const oconfig_item_t *ci) {
  int tmp = 0;

  if (cf_util_get_int(ci, &tmp) != 0)
    return -1;
  else if ((tmp > 0) && (tmp <= 255))
    wifxudp_config_ttl = tmp;
  else {
    WARNING("write_influxdb_udp plugin: "
            "The `TimeToLive' must be between 1 and 255.");
    return -1;
  }

  return 0;
} /* int wifxudp_config_set_ttl */

static int wifxudp_config_set_buffer_size(const oconfig_item_t *ci) {
  int tmp = 0;

  if (cf_util_get_int(ci, &tmp) != 0)
    return -1;
  else if ((tmp >= 1024) && (tmp <= 65535))
    wifxudp_config_packet_size = tmp;
  else {
    WARNING("write_influxdb_udp plugin: "
            "The `MaxPacketSize' must be between 1024 and 65535.");
    return -1;
  }

  return 0;
} /* int wifxudp_config_set_buffer_size */

static int wifxudp_config_set_server(const oconfig_item_t *ci) {
  if ((ci->values_num < 1) || (ci->values_num > 2) ||
      (ci->values[0].type != OCONFIG_TYPE_STRING) ||
      ((ci->values_num > 1) && (ci->values[1].type != OCONFIG_TYPE_STRING))) {
    ERROR("write_influxdb_udp plugin: The `%s' config option needs "
          "one or two string arguments.",
          ci->key);
    return -1;
  }

  sending_socket = sockent_create();
  if (sending_socket == NULL) {
    ERROR("write_influxdb_udp plugin: sockent_create failed.");
    return -1;
  }

  sending_socket->node = strdup(ci->values[0].value.string);
  if (ci->values_num >= 2)
    sending_socket->service = strdup(ci->values[1].value.string);

  return 0;
} /* int wifxudp_config_set_server */

static int write_influxdb_udp_config(oconfig_item_t *ci) {
  for (int i = 0; i < ci->children_num; i++) {
    oconfig_item_t *child = ci->children + i;

    if (strcasecmp("Server", child->key) == 0)
      wifxudp_config_set_server(child);
    else if (strcasecmp("TimeToLive", child->key) == 0) {
      wifxudp_config_set_ttl(child);
    } else if (strcasecmp("MaxPacketSize", child->key) == 0)
      wifxudp_config_set_buffer_size(child);
    else if (strcasecmp("StoreRates", child->key) == 0)
      cf_util_get_boolean(child, &wifxudp_config_store_rates);
    else {
      WARNING("write_influxdb_udp plugin: "
              "Option `%s' is not allowed here.",
              child->key);
    }
  }

  return 0;
} /* int write_influxdb_udp_config */

static int write_influxdb_udp_shutdown(void) {
  if (send_buffer_fill > 0)
    flush_buffer();

  sfree(send_buffer);

  if (sending_socket != NULL) {
    sockent_client_disconnect(sending_socket);
    sockent_destroy(sending_socket);
  }

  plugin_unregister_config("write_influxdb_udp");
  plugin_unregister_init("write_influxdb_udp");
  plugin_unregister_write("write_influxdb_udp");
  plugin_unregister_shutdown("write_influxdb_udp");

  return 0;
} /* int write_influxdb_udp_shutdown */

static int write_influxdb_udp_init(void) {
  static bool have_init;

  /* Check if we were already initialized. If so, just return - there's
   * nothing more to do (for now, that is). */
  if (have_init)
    return 0;
  have_init = true;

  plugin_register_shutdown("write_influxdb_udp", write_influxdb_udp_shutdown);

  send_buffer = malloc(wifxudp_config_packet_size);
  if (send_buffer == NULL) {
    ERROR("write_influxdb_udp plugin: malloc failed.");
    return -1;
  }
  write_influxdb_udp_init_buffer();

  /* setup socket(s) and so on */
  if (sending_socket != NULL) {
    plugin_register_write("write_influxdb_udp", write_influxdb_udp_write,
                          /* user_data = */ NULL);
  }

  return 0;
} /* int write_influxdb_udp_init */

static int write_influxdb_udp_flush(cdtime_t timeout,
                                    __attribute__((unused))
                                    const char *identifier,
                                    __attribute__((unused))
                                    user_data_t *user_data) {
  pthread_mutex_lock(&send_buffer_lock);

  if (send_buffer_fill > 0) {
    if (timeout > 0) {
      cdtime_t now = cdtime();
      if ((send_buffer_last_update + timeout) > now) {
        pthread_mutex_unlock(&send_buffer_lock);
        return 0;
      }
    }
    flush_buffer();
  }
  pthread_mutex_unlock(&send_buffer_lock);

  return 0;
} /* int write_influxdb_udp_flush */

void module_register(void) {
  plugin_register_complex_config("write_influxdb_udp",
                                 write_influxdb_udp_config);
  plugin_register_init("write_influxdb_udp", write_influxdb_udp_init);
  plugin_register_flush("write_influxdb_udp", write_influxdb_udp_flush, NULL);
} /* void module_register */
