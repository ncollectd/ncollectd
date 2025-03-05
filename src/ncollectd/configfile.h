/* SPDX-License-Identifier: GPL-2.0-only OR MIT                         */
/* SPDX-FileCopyrightText: Copyright (C) 2005-2011 Florian octo Forster */
/* SPDX-FileContributor: Florian octo Forster <octo at collectd.org>    */

#pragma once

#include "ncollectd.h"

#include "libmetric/metric.h"
#include "libconfig/config.h"
#include "libutils/time.h"

void cf_unregister_all(void);
/*
 * DESCRIPTION
 *  Remove a registered plugin from the internal data structures.
 *
 * PARAMETERS
 *  `type'      Name of the plugin (must be the same as passed to
 *              `plugin_register'
 */
void cf_unregister(const char *type);

/*
 * DESCRIPTION
 *  `cf_register' is called by plugins that wish to receive config keys. The
 *  plugin will then receive all keys it registered for if they're found in a
 *  `<Plugin $type>' section.
 *
 * PARAMETERS
 *  `type'      Name of the plugin (must be the same as passed to
 *              `plugin_register'
 *  `callback'  Pointer to the callback function. The callback must return zero
 *              upon success, a value smaller than zero if it doesn't know how
 *              to handle the `key' passed to it (the first argument) or a
 *              value greater than zero if it knows how to handle the key but
 *              failed.
 *  `keys'      Array of key values this plugin wished to receive. The last
 *              element must be a NULL-pointer.
 *  `keys_num'  Number of elements in the array (not counting the last NULL-
 *              pointer.
 *
 * NOTES
 *  `cf_unregister' will be called for `type' to make sure only one record
 *  exists for each `type' at any time. This means that `cf_register' may be
 *  called multiple times, but only the last call will have an effect.
 */

int cf_register(const char *type, int (*callback)(config_item_t *));

/*
 * DESCRIPTION
 *  `cf_read' reads the config file `filename' and dispatches the read
 *  information to functions/variables. Most important: Is calls `plugin_load'
 *  to load specific plugins, depending on the current mode of operation.
 *
 * PARAMETERS
 *  `filename'  An additional filename to look for. This function calls
 *              `lc_process' which already searches many standard locations..
 *              If set to NULL will use the `CONFIGFILE' define.
 *
 * RETURN VALUE
 *  Returns zero upon success and non-zero otherwise. A error-message will have
 *  been printed in this case.
 */
int cf_read(const char *filename, bool dump);

int global_option_set(const char *option, const char *value, bool from_cli);
const char *global_option_get(const char *option);
long global_option_get_long(const char *option, long default_value);

cdtime_t global_option_get_time(char const *option, cdtime_t default_value);

cdtime_t cf_get_default_interval(void);

int global_option_get_cpumap(const char *name);

void global_options_free (void);
