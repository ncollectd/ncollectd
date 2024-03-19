/* SPDX-License-Identifier: GPL-2.0-only                             */
/* SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín  */
/* SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com> */

#pragma once

typedef struct {
    char *user;
    char *group;
    char *exec;
    char **argv;
    char **envp;
} cexec_t;

int cf_util_exec_append_env(config_item_t *ci, cexec_t *pm);

int cf_util_exec_cmd(config_item_t *ci, cexec_t *pm);

int cexec_append_env(cexec_t *pm, const char *key, const char *value);

void exec_reset(cexec_t *pm);

int exec_fork_child(cexec_t *pm, bool can_be_root, int *fd_in, int *fd_out, int *fd_err);
