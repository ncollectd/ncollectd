// SPDX-License-Identifier: GPL-2.0-only OR MIT
// SPDX-FileCopyrightText: Copyright(c) 2018-2019 Intel Corporation.
// SPDX-FileContributor: Starzyk, Mateusz <mateuszx.starzyk at intel.com>
// SPDX-FileContributor: Wojciech Andralojc <wojciechx.andralojc at intel.com>
// SPDX-FileContributor: Michał Aleksiński <michalx.aleksinski at intel.com>

#include "plugin.h"
#include "libutils/common.h"
#include "plugins/intel_rdt/proc_pids.h"

void pids_list_free(pids_list_t *list)
{
    assert(list);

    free(list->pids);
    free(list);
}

int proc_pids_is_name_valid(const char *name)
{
    if (name != NULL) {
        unsigned len = strlen(name);
        if (len > 0 && len <= MAX_PROC_NAME_LEN) {
            return 1;
        } else {
            PLUGIN_DEBUG("Process name \'%s\' is too long. Max supported len is %d chars.",
                          name, MAX_PROC_NAME_LEN);
        }
    }

    return 0;
}

int pids_list_add_pid(pids_list_t *list, const pid_t pid)
{
    assert(list);

    if (list->allocated == list->size) {
        size_t new_allocated = list->allocated + 1 + list->allocated / 10;
        pid_t *new_pids = realloc(list->pids, sizeof(pid_t) * new_allocated);
        if (new_pids == NULL) {
            PLUGIN_ERROR("Alloc error\n");
            return -1;
        }

        list->pids = new_pids;
        list->allocated = new_allocated;
    }

    list->pids[list->size] = pid;
    list->size++;

    return 0;
}

int pids_list_add_list(pids_list_t *dst, pids_list_t *src)
{
    assert(dst);
    assert(src);

    if (dst->allocated < dst->size + src->size) {
        pid_t *new_pids = realloc(dst->pids, sizeof(pid_t) * (dst->size + src->size));
        if (new_pids == NULL) {
            PLUGIN_ERROR("Alloc error\n");
            return -1;
        }

        dst->allocated = dst->size + src->size;
        dst->pids = new_pids;
    }

    memcpy(dst->pids + dst->size, src->pids, src->size * sizeof(*(src->pids)));
    dst->size += src->size;

    return 0;
}

int pids_list_clear(pids_list_t *list)
{
    assert(list);

    if (list->pids != NULL)
        free(list->pids);

    list->size = 0;
    list->allocated = 0;

    return 0;
}

int pids_list_contains_pid(pids_list_t *list, const pid_t pid)
{
    assert(list);

    for (size_t i = 0; i < list->size; i++) {
        if (list->pids[i] == pid)
            return 1;
    }

    return 0;
}

/*
 * NAME
 *   read_proc_name
 *
 * DESCRIPTION
 *   Reads process name from given pid directory.
 *   Strips new-line character (\n).
 *
 * PARAMETERS
 *   `procfs_path' Path to systems proc directory (e.g. /proc)
 *   `pid_entry'     Dirent for PID directory
 *   `name'              Output buffer for process name, recommended proc_comm.
 *   `out_size'      Output buffer size, recommended sizeof(proc_comm)
 *
 * RETURN VALUE
 *   On success, the number of read bytes (includes stripped \n).
 *   -1 on file open error
 */
static int read_proc_name(const char *procfs_path, const struct dirent *pid_entry, char *name,
                                                   const size_t out_size)
{
    assert(pid_entry);
    assert(name);
    assert(out_size);
    memset(name, 0, out_size);

    const char *comm_file_name = "comm";

    char *path = ssnprintf_alloc("%s/%s/%s", procfs_path, pid_entry->d_name, comm_file_name);
    if (path == NULL)
        return -1;
    FILE *f = fopen(path, "r");
    if (f == NULL) {
        PLUGIN_ERROR("Failed to open comm file, error: %d\n", errno);
        free(path);
        return -1;
    }
    size_t read_length = fread(name, sizeof(char), out_size, f);
    name[out_size - 1] = '\0';
    fclose(f);
    free(path);
    /* strip new line ending */
    char *newline = strchr(name, '\n');
    if (newline)
        *newline = '\0';

    return read_length;
}

/*
 * NAME
 *   get_pid_number
 *
 * DESCRIPTION
 *   Gets pid number for given /proc/pid directory entry or
 *   returns error if input directory does not hold PID information.
 *
 * PARAMETERS
 *   `entry'        Dirent for PID directory
 *   `pid'          PID number to be filled
 *
 * RETURN VALUE
 *   0 on success. -1 on error.
 */
static int get_pid_number(struct dirent *entry, pid_t *pid)
{
    char *tmp_end; /* used for strtoul error check*/

    if ((pid == NULL) || (entry == NULL))
        return -1;

    if (entry->d_type != DT_DIR)
        return -1;

    /* trying to get pid number from directory name*/
    *pid = strtoul(entry->d_name, &tmp_end, 10);
    if (*tmp_end != '\0')
        return -1; /* conversion failed, not proc-pid */
    /* all checks passed, marking as success */
    return 0;
}

int proc_pids_init(char **procs_names_array, size_t procs_names_array_size,
                                             proc_pids_t **proc_pids[])
{

    proc_pids_t **proc_pids_array;
    assert(proc_pids);
    assert(NULL == *proc_pids);

    /* Copy procs names to output array. Initialize pids list with NULL value. */
    proc_pids_array = calloc(procs_names_array_size, sizeof(*proc_pids_array));
    if (proc_pids_array == NULL)
        return -1;

    for (size_t i = 0; i < procs_names_array_size; ++i) {
        proc_pids_array[i] = calloc(1, sizeof(**proc_pids_array));
        if (proc_pids_array[i] == NULL)
            goto proc_pids_init_error;

        sstrncpy(proc_pids_array[i]->process_name, procs_names_array[i],
                 STATIC_ARRAY_SIZE(proc_pids_array[i]->process_name));
        proc_pids_array[i]->prev = NULL;
        proc_pids_array[i]->curr = NULL;
    }

    *proc_pids = proc_pids_array;

    return 0;
proc_pids_init_error:
    if (proc_pids_array != NULL) {
        for (size_t i = 0; i < procs_names_array_size; ++i) {
            free(proc_pids_array[i]);
        }
        free(proc_pids_array);
    }
    return -1;
}

static void swap_proc_pids(proc_pids_t **proc_pids, size_t proc_pids_num)
{
    for (size_t i = 0; i < proc_pids_num; i++) {
        pids_list_t *swap = proc_pids[i]->prev;
        proc_pids[i]->prev = proc_pids[i]->curr;
        proc_pids[i]->curr = swap;
    }
}

int proc_pids_update(const char *procfs_path, proc_pids_t **proc_pids, size_t proc_pids_num)
{
    assert(procfs_path);
    assert(proc_pids);

    DIR *proc_dir = opendir(procfs_path);
    if (proc_dir == NULL) {
        PLUGIN_ERROR("Could not open %s directory, error: %d", procfs_path, errno);
        return -1;
    }

    swap_proc_pids(proc_pids, proc_pids_num);

    for (size_t i = 0; i < proc_pids_num; i++) {
        if (proc_pids[i]->curr == NULL)
            proc_pids[i]->curr = calloc(1, sizeof(*(proc_pids[i]->curr)));

        if (proc_pids[i]->curr == NULL) {
            PLUGIN_ERROR("calloc error\n");
            swap_proc_pids(proc_pids, proc_pids_num);
            closedir(proc_dir);
            return -1;
        }

        proc_pids[i]->curr->size = 0;
    }

    /* Go through procfs and find PIDS and their comms */
    struct dirent *entry;
    while ((entry = readdir(proc_dir)) != NULL) {
        pid_t pid;
        int pid_conversion = get_pid_number(entry, &pid);
        if (pid_conversion < 0)
            continue;

        proc_comm_t comm;
        int read_result = read_proc_name(procfs_path, entry, comm, sizeof(proc_comm_t));
        if (read_result <= 0)
            continue;

        /* Try to find comm in input procs array */
        for (size_t i = 0; i < proc_pids_num; ++i) {
            if (strncmp(comm, proc_pids[i]->process_name, STATIC_ARRAY_SIZE(comm)) == 0)
                pids_list_add_pid(proc_pids[i]->curr, pid);
        }
    }

    int close_result = closedir(proc_dir);
    if (close_result != 0) {
        PLUGIN_ERROR("failed to close /proc directory, error: %d", errno);
        swap_proc_pids(proc_pids, proc_pids_num);
        return -1;
    }

    return 0;
}

int pids_list_diff(proc_pids_t *proc, pids_list_t *added, pids_list_t *removed)
{
    assert(proc);
    assert(added);
    assert(removed);

    added->size = 0;
    removed->size = 0;

    if ((proc->prev == NULL) || (proc->prev->size == 0)) {
        /* append all PIDs from curr to added*/
        return pids_list_add_list(added, proc->curr);
    } else if ((proc->curr == NULL) || (proc->curr->size == 0)) {
        /* append all PIDs from prev to removed*/
        return pids_list_add_list(removed, proc->prev);
    }

    for (size_t i = 0; i < proc->prev->size; i++) {
        if (pids_list_contains_pid(proc->curr, proc->prev->pids[i]) ==  0) {
            int add_result = pids_list_add_pid(removed, proc->prev->pids[i]);
            if (add_result < 0)
                return add_result;
        }
    }

    for (size_t i = 0; i < proc->curr->size; i++) {
        if (pids_list_contains_pid(proc->prev, proc->curr->pids[i]) == 0) {
            int add_result = pids_list_add_pid(added, proc->curr->pids[i]);
            if (add_result < 0)
                return add_result;
        }
    }

    return 0;
}

int proc_pids_free(proc_pids_t *proc_pids[], size_t proc_pids_num)
{
    for (size_t i = 0; i < proc_pids_num; i++) {
        if (proc_pids[i]->curr != NULL)
            pids_list_free(proc_pids[i]->curr);
        if (proc_pids[i]->prev != NULL)
            pids_list_free(proc_pids[i]->prev);
        free(proc_pids[i]);
    }
    free(proc_pids);

    return 0;
}
