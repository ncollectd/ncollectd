// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: Copyright (C) 2010 Andres J. Diaz
// SPDX-FileCopyrightText: Copyright (C) 2010-2024 Manuel Sanmartín
// SPDX-FileContributor: Andres J. Diaz <ajdiaz at connectical.com>
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

/* Many of this code is based on busybox ipc implementation, which is:
 *   (C) Rodney Radford <rradford@mindspring.com> and distributed under GPLv2.
 */

#include "plugin.h"
#include "libutils/common.h"

#ifdef KERNEL_LINUX
/* _GNU_SOURCE is needed for struct shm_info.used_ids on musl libc */
#define _GNU_SOURCE

/* X/OPEN tells us to use <sys/{types,ipc,sem}.h> for semctl() */
/* X/OPEN tells us to use <sys/{types,ipc,msg}.h> for msgctl() */
/* X/OPEN tells us to use <sys/{types,ipc,shm}.h> for shmctl() */
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/types.h>

/* For older kernels the same holds for the defines below */
#ifndef MSG_STAT
#define MSG_STAT 11
#define MSG_INFO 12
#endif

#ifndef SHM_STAT
#define SHM_STAT 13
#define SHM_INFO 14
struct shm_info {
    int used_ids;
    ulong shm_tot; /* total allocated shm */
    ulong shm_rss; /* total resident shm */
    ulong shm_swp; /* total swapped shm */
    ulong swap_attempts;
    ulong swap_successes;
};
#endif

#ifndef SEM_STAT
#define SEM_STAT 18
#define SEM_INFO 19
#endif

/* The last arg of semctl is a union semun, but where is it defined?
     X/OPEN tells us to define it ourselves, but until recently
     Linux include files would also define it. */
#if defined(__GNU_LIBRARY__) && !defined(_SEM_SEMUN_UNDEFINED)
/* union semun is defined by including <sys/sem.h> */
#else
/* according to X/OPEN we have to define it ourselves */
union semun {
    int val;
    struct semid_ds *buf;
    unsigned short *array;
    struct seminfo *__buf;
};
#endif
static long pagesize_g;
/* #endif  KERNEL_LINUX */
#elif defined(KERNEL_AIX)
#include <sys/ipc_info.h>
/* #endif KERNEL_AIX */
#else
#error "No applicable input method."
#endif

enum {
    FAM_IPC_SEM_SETS,
    FAM_IPC_SEM_SEMAPHORES,
    FAM_IPC_SHM_SEGMENTS,
    FAM_IPC_SHM_TOTAL,
    FAM_IPC_SHM_RSS,
    FAM_IPC_SHM_SWAPPED,
    FAM_IPC_MSG_QUEUES,
    FAM_IPC_MSG_MESSAGES,
    FAM_IPC_MSG_BYTES,
    FAM_IPC_MAX,
};

static metric_family_t fams[FAM_IPC_MAX] = {
    [FAM_IPC_SEM_SETS] = {
        .name = "system_ipc_semaphore_sets",
        .type = METRIC_TYPE_GAUGE,
        .help = "The number of semaphore sets that currently exist on the system.",
    },
    [FAM_IPC_SEM_SEMAPHORES] = {
        .name = "system_ipc_semaphores",
        .type = METRIC_TYPE_GAUGE,
        .help = "The number of semaphores in all semaphore sets on the system.",
    },
    [FAM_IPC_SHM_SEGMENTS] = {
        .name = "system_ipc_shm_segments",
        .type = METRIC_TYPE_GAUGE,
        .help = "The number of currently existing segments on the system.",
    },
    [FAM_IPC_SHM_TOTAL] = {
        .name = "system_ipc_shm_total_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Shared memory bytes on the system.",
    },
    [FAM_IPC_SHM_RSS] = {
        .name = "system_ipc_shm_rss_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Resident shared memory bytes on the system.",
    },
    [FAM_IPC_SHM_SWAPPED] = {
        .name = "system_ipc_shm_swapped_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "Swapped shared memory bytes on the system.",
    },
    [FAM_IPC_MSG_QUEUES] = {
        .name = "system_ipc_msg_queues",
        .type = METRIC_TYPE_GAUGE,
        .help = "The number of message queues that currently exist on the system.",
    },
    [FAM_IPC_MSG_MESSAGES] = {
        .name = "system_ipc_msg_messages",
        .type = METRIC_TYPE_GAUGE,
        .help = "The number of messages in all queues on the system.",
    },
    [FAM_IPC_MSG_BYTES] = {
        .name = "system_ipc_msg_bytes",
        .type = METRIC_TYPE_GAUGE,
        .help = "The number of bytes in all messages in all queues on the system.",
    },
};


#ifdef KERNEL_LINUX
static int ipc_read_sem(metric_family_t *fam)
{
    struct seminfo seminfo;
    union semun arg;
    int status;

    arg.array = (void *)&seminfo;

    status = semctl(/* id = */ 0, /* num = */ 0, SEM_INFO, arg);
    if (status == -1) {
        PLUGIN_ERROR("semctl(2) failed: %s. Maybe the kernel is not configured for semaphores?",
                     STRERRNO);
        return -1;
    }

    metric_family_append(&fam[FAM_IPC_SEM_SETS], VALUE_GAUGE(seminfo.semusz), NULL, NULL);
    metric_family_append(&fam[FAM_IPC_SEM_SEMAPHORES], VALUE_GAUGE(seminfo.semaem), NULL, NULL);

    return 0;
}

static int ipc_read_shm(metric_family_t *fam)
{
    struct shm_info shm_info;
    int status;

    status = shmctl(/* id = */ 0, SHM_INFO, (void *)&shm_info);
    if (status == -1) {
        PLUGIN_ERROR("shmctl(2) failed: %s. Maybe the kernel is not configured for shared memory?",
                     STRERRNO);
        return -1;
    }

    metric_family_append(&fam[FAM_IPC_SHM_SEGMENTS],
                         VALUE_GAUGE(shm_info.used_ids), NULL, NULL);
    metric_family_append(&fam[FAM_IPC_SHM_TOTAL],
                         VALUE_GAUGE(shm_info.shm_tot * pagesize_g), NULL, NULL);
    metric_family_append(&fam[FAM_IPC_SHM_RSS],
                         VALUE_GAUGE(shm_info.shm_rss * pagesize_g), NULL, NULL);
    metric_family_append(&fam[FAM_IPC_SHM_SWAPPED],
                         VALUE_GAUGE(shm_info.shm_swp * pagesize_g), NULL, NULL);

    return 0;
}

static int ipc_read_msg(metric_family_t *fam)
{
    struct msginfo msginfo;

    if (msgctl(0, MSG_INFO, (struct msqid_ds *)(void *)&msginfo) < 0) {
        PLUGIN_ERROR("Kernel is not configured for message queues");
        return -1;
    }

    metric_family_append(&fam[FAM_IPC_MSG_QUEUES], VALUE_GAUGE(msginfo.msgpool), NULL, NULL);
    metric_family_append(&fam[FAM_IPC_MSG_MESSAGES], VALUE_GAUGE(msginfo.msgmap), NULL, NULL);
    metric_family_append(&fam[FAM_IPC_MSG_BYTES], VALUE_GAUGE(msginfo.msgtql), NULL, NULL);

    return 0;
}

static int ipc_init(void)
{
    pagesize_g = sysconf(_SC_PAGESIZE);
    return 0;
}
/* #endif KERNEL_LINUX */

#elif defined(KERNEL_AIX)
static caddr_t ipc_get_info(cid_t cid, int cmd, int version, int stsize, int *nmemb)
{
    int size = 0;
    caddr_t buff = NULL;

    if (get_ipc_info(cid, cmd, version, buff, &size) < 0) {
        if (errno != ENOSPC) {
            PLUGIN_WARNING("get_ipc_info: %s", STRERRNO);
            return NULL;
        }
    }

    if (size == 0)
        return NULL;

    if (size % stsize) {
        PLUGIN_ERROR("ipc_get_info: missmatch struct size and buffer size");
        return NULL;
    }

    *nmemb = size / stsize;

    buff = malloc(size);
    if (buff == NULL) {
        PLUGIN_ERROR("ipc_get_info malloc failed.");
        return NULL;
    }

    if (get_ipc_info(cid, cmd, version, buff, &size) < 0) {
        PLUGIN_WARNING("get_ipc_info: %s", STRERRNO);
        free(buff);
        return NULL;
    }

    return buff;
}

static int ipc_read_sem(metric_family_t *fam)
{
    ipcinfo_sem_t *ipcinfo_sem;
    unsigned short sem_nsems = 0;
    unsigned short sems = 0;
    int n;

    ipcinfo_sem = (ipcinfo_sem_t *)ipc_get_info(0, GET_IPCINFO_SEM_ALL, IPCINFO_SEM_VERSION,
                                                sizeof(ipcinfo_sem_t), &n);
    if (ipcinfo_sem == NULL)
        return -1;

    for (int i = 0; i < n; i++) {
        sem_nsems += ipcinfo_sem[i].sem_nsems;
        sems++;
    }
    free(ipcinfo_sem);

    metric_family_append(&fam[FAM_IPC_SEM_SETS], VALUE_GAUGE(sem_nsems), NULL, NULL);
    metric_family_append(&fam[FAM_IPC_SEM_SEMAPHORES], VALUE_GAUGE(sems), NULL, NULL);

    return 0;
}

static int ipc_read_shm(metric_family_t *fam)
{
    ipcinfo_shm_t *ipcinfo_shm;
    ipcinfo_shm_t *pshm;
    unsigned int shm_segments = 0;
    size64_t shm_bytes = 0;
    int i, n;

    ipcinfo_shm = (ipcinfo_shm_t *)ipc_get_info(0, GET_IPCINFO_SHM_ALL, IPCINFO_SHM_VERSION,
                                                sizeof(ipcinfo_shm_t), &n);
    if (ipcinfo_shm == NULL)
        return -1;

    for (i = 0, pshm = ipcinfo_shm; i < n; i++, pshm++) {
        shm_segments++;
        shm_bytes += pshm->shm_segsz;
    }
    free(ipcinfo_shm);

    metric_family_append(&fam[FAM_IPC_SHM_SEGMENTS], VALUE_GAUGE(shm_segments), NULL, NULL);
    metric_family_append(&fam[FAM_IPC_SHM_TOTAL], VALUE_GAUGE(shm_bytes), NULL, NULL);

    return 0;
}

static int ipc_read_msg(metric_family_t *fam)
{
    ipcinfo_msg_t *ipcinfo_msg;
    uint32_t msg_used_space = 0;
    uint32_t msg_alloc_queues = 0;
    msgqnum32_t msg_qnum = 0;
    int n;

    ipcinfo_msg = (ipcinfo_msg_t *)ipc_get_info(0, GET_IPCINFO_MSG_ALL, IPCINFO_MSG_VERSION,
                                                sizeof(ipcinfo_msg_t), &n);
    if (ipcinfo_msg == NULL)
        return -1;

    for (int i = 0; i < n; i++) {
        msg_alloc_queues++;
        msg_used_space += ipcinfo_msg[i].msg_cbytes;
        msg_qnum += ipcinfo_msg[i].msg_qnum;
    }
    free(ipcinfo_msg);

    metric_family_append(&fam[FAM_IPC_MSG_QUEUES], VALUE_GAUGE(msg_alloc_queues), NULL, NULL);
    metric_family_append(&fam[FAM_IPC_MSG_MESSAGES], VALUE_GAUGE(msg_qnum), NULL, NULL);
    metric_family_append(&fam[FAM_IPC_MSG_BYTES], VALUE_GAUGE(msg_used_space), NULL, NULL);

    return 0;
}
#endif

static int ipc_read(void)
{
    int status = 0;
    status |= ipc_read_shm(fams);
    status |= ipc_read_sem(fams);
    status |= ipc_read_msg(fams);

    plugin_dispatch_metric_family_array(fams, FAM_IPC_MAX, 0);
    return status;
}

void module_register(void)
{
#ifdef KERNEL_LINUX
    plugin_register_init("ipc", ipc_init);
#endif
    plugin_register_read("ipc", ipc_read);
}
