/* SPDX-License-Identifier: GPL-2.0-only OR ISC                      */
/* SPDX-FileCopyrightText: Copyright (C) 2009 Florian octo Forster   */
/* SPDX-FileContributor: Florian octo Forster <octo at verplant.org> */
/* from librouteros - src/routeros_api.h                             */
#pragma once

#include <stdint.h>
#include <inttypes.h>
#include <stdbool.h>

#define ROUTEROS_API_PORT "8728"

struct ros_connection_s;
typedef struct ros_connection_s ros_connection_t;

struct ros_reply_s;
typedef struct ros_reply_s ros_reply_t;

typedef int (*ros_reply_handler_t) (ros_connection_t *c, const ros_reply_t *r,
        void *user_data);

/*
 * Connect options struct
 */
struct ros_connect_opts_s
{
    /* receive_timeout is the receive timeout in seconds. */
    unsigned int receive_timeout;
    /* connect_timeout is the connect timeout in seconds. */
    unsigned int connect_timeout;
};
typedef struct ros_connect_opts_s ros_connect_opts_t;

/*
 * Connection handling
 */
ros_connection_t *ros_connect (const char *node, const char *service,
        const char *username, const char *password);
ros_connection_t *ros_connect_with_options (const char *node, const char *service,
        const char *username, const char *password, const ros_connect_opts_t *connect_opts);
int ros_disconnect (ros_connection_t *con);

/*
 * Command execution
 */
int ros_query (ros_connection_t *c,
        const char *command,
        size_t args_num, const char * const *args,
        ros_reply_handler_t handler, void *user_data);

/*
 * Reply handling
 */
const ros_reply_t *ros_reply_next (const ros_reply_t *r);
int ros_reply_num (const ros_reply_t *r);

const char *ros_reply_status (const ros_reply_t *r);

/* Receiving reply parameters */
const char *ros_reply_param_key_by_index (const ros_reply_t *r,
        unsigned int index);
const char *ros_reply_param_val_by_index (const ros_reply_t *r,
        unsigned int index);
const char *ros_reply_param_val_by_key (const ros_reply_t *r, const char *key);

/* High-level function for accessing /interface */
struct ros_interface_s;
typedef struct ros_interface_s ros_interface_t;
struct ros_interface_s
{
    /* Name of the interface */
    const char *name;
    const char *type;
    const char *comment;

    /* Packet, octet and error counters. */
    uint64_t rx_packets;
    uint64_t tx_packets;
    uint64_t rx_bytes;
    uint64_t tx_bytes;
    uint64_t rx_errors;
    uint64_t tx_errors;
    uint64_t rx_drops;
    uint64_t tx_drops;

    /* Maximum transfer unit */
    unsigned int mtu;
    unsigned int l2mtu;

    /* Interface flags */
    bool dynamic;
    bool running;
    bool enabled;

    /* Next interface */
    ros_interface_t *next;
};

/* Callback function */
typedef int (*ros_interface_handler_t) (ros_connection_t *c,
        const ros_interface_t *i, void *user_data);

int ros_interface (ros_connection_t *c,
        ros_interface_handler_t handler, void *user_data);

/* High-level function for accessing /interface/wireless/registration-table */
struct ros_registration_table_s;
typedef struct ros_registration_table_s ros_registration_table_t;
struct ros_registration_table_s
{
    /* Name of the interface */
    const char *interface;
    /* Name of the remote radio */
    const char *radio_name;
    /* MAC address of the registered client */
    const char *mac_address;

    /* ap is set to true, if the REMOTE radio is an access point. */
    bool ap;
    /* whether the connected client is using wds or not */
    bool wds;

    /* Receive and transmit rate in MBit/s */
    double rx_rate;
    double tx_rate;

    /* Packet, octet and frame counters. */
    uint64_t rx_packets;
    uint64_t tx_packets;
    uint64_t rx_bytes;
    uint64_t tx_bytes;
    uint64_t rx_frames;
    uint64_t tx_frames;
    uint64_t rx_frame_bytes;
    uint64_t tx_frame_bytes;
    uint64_t rx_hw_frames;
    uint64_t tx_hw_frames;
    uint64_t rx_hw_frame_bytes;
    uint64_t tx_hw_frame_bytes;

    /* Signal quality information (in dBm) */
    double rx_signal_strength;
    double tx_signal_strength;
    double signal_to_noise;

    /* Overall connection quality (in percent) */
    double rx_ccq;
    double tx_ccq;

    /* Next interface */
    ros_registration_table_t *next;
};

/* Callback function */
typedef int (*ros_registration_table_handler_t) (ros_connection_t *c,
        const ros_registration_table_t *r, void *user_data);

int ros_registration_table (ros_connection_t *c,
        ros_registration_table_handler_t handler, void *user_data);

/* High-level function for accessing /system/resource */
struct ros_system_resource_s;
typedef struct ros_system_resource_s ros_system_resource_t;
struct ros_system_resource_s
{
    uint64_t uptime;

    const char *version;
    const char *architecture_name;
    const char *board_name;

    const char *cpu_model;
    unsigned int cpu_count;
    unsigned int cpu_load;
    uint64_t cpu_frequency;

    uint64_t free_memory;
    uint64_t total_memory;

    uint64_t free_hdd_space;
    uint64_t total_hdd_space;

    uint64_t write_sect_since_reboot;
    uint64_t write_sect_total;
    uint64_t bad_blocks;
};

/* Callback function */
typedef int (*ros_system_resource_handler_t) (ros_connection_t *c,
              const ros_system_resource_t *r, void *user_data);

int ros_system_resource (ros_connection_t *c,
                         ros_system_resource_handler_t handler, void *user_data);

/* High-level function for accessing /system/health  */
struct ros_system_health_s;
typedef struct ros_system_health_s ros_system_health_t;
struct ros_system_health_s
{
    double voltage;
    double temperature;
};

/* Callback function */
typedef int (*ros_system_health_handler_t) (ros_connection_t *c,
                                                const ros_system_health_t *r, void *user_data);

int ros_system_health (ros_connection_t *c, ros_system_health_handler_t handler, void *user_data);
