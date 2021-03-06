// SPDX-License-Identifier: GPL-2.0-only
// Copyright (C) 2007-2010  Florian octo Forster
// Copyright (C) 2007-2009  Sebastian Harl
// Copyright (C) 2008       Peter Holik
// Authors:
//   Florian octo Forster <octo at collectd.org>
//   Sebastian Harl <sh at tokkee.org>
//   Peter Holik <peter at holik.at>

#define _DEFAULT_SOURCE
#define _BSD_SOURCE /* For setgroups */

/* _GNU_SOURCE is needed in Linux to use execvpe */
#define _GNU_SOURCE

#include "collectd.h"

#include "plugin.h"
#include "utils/common/common.h"

#include "utils/cmds/putnotif.h"
#include "utils/cmds/putval.h"

#include <grp.h>
#include <poll.h>
#include <pwd.h>
#include <signal.h>
#include <sys/types.h>

#ifdef HAVE_SYS_CAPABILITY_H
#include <sys/capability.h>
#endif

extern char **environ;

#define PL_NORMAL 0x01
#define PL_NOTIF_ACTION 0x02

#define PL_RUNNING 0x10

/*
 * Private data types
 */
/*
 * Access to this structure is serialized using the `pl_lock' lock and the
 * `PL_RUNNING' flag. The execution of notifications is *not* serialized, so
 * all functions used to handle notifications MUST NOT write to this structure.
 * The `pid' and `status' fields are thus unused if the `PL_NOTIF_ACTION' flag
 * is set.
 * The `PL_RUNNING' flag is set in `exec_read' and unset in `exec_read_one'.
 */
struct program_list_s;
typedef struct program_list_s program_list_t;
struct program_list_s {
  char *user;
  char *group;
  char *exec;
  char **argv;
  int pid;
  int status;
  int flags;
  program_list_t *next;
};

typedef struct program_list_and_notification_s {
  program_list_t *pl;
  notification_t n;
} program_list_and_notification_t;

/*
 * constants
 */
const long int MAX_GRBUF_SIZE = 65536;

/*
 * Private variables
 */
static program_list_t *pl_head;
static pthread_mutex_t pl_lock = PTHREAD_MUTEX_INITIALIZER;

/*
 * Functions
 */
static void sigchld_handler(int __attribute__((unused)) signal)
{
  pid_t pid;
  int status;
  while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
    program_list_t *pl;
    for (pl = pl_head; pl != NULL; pl = pl->next)
      if (pl->pid == pid)
        break;
    if (pl != NULL)
      pl->status = status;
  } /* while (waitpid) */
} /* void sigchld_handler }}} */

static int exec_config_exec(oconfig_item_t *ci)
{
  program_list_t *pl;
  char buffer[128];
  int i;

  if (ci->children_num != 0) {
    WARNING("exec plugin: The config option `%s' may not be a block.", ci->key);
    return -1;
  }
  if (ci->values_num < 2) {
    WARNING("exec plugin: The config option `%s' needs at least two arguments.",
            ci->key);
    return -1;
  }
  if ((ci->values[0].type != OCONFIG_TYPE_STRING) ||
      (ci->values[1].type != OCONFIG_TYPE_STRING)) {
    WARNING("exec plugin: The first two arguments to the `%s' option must "
            "be string arguments.",
            ci->key);
    return -1;
  }

  pl = calloc(1, sizeof(*pl));
  if (pl == NULL) {
    ERROR("exec plugin: calloc failed.");
    return -1;
  }

  if (strcasecmp("NotificationExec", ci->key) == 0)
    pl->flags |= PL_NOTIF_ACTION;
  else
    pl->flags |= PL_NORMAL;

  pl->user = strdup(ci->values[0].value.string);
  if (pl->user == NULL) {
    ERROR("exec plugin: strdup failed.");
    sfree(pl);
    return -1;
  }

  pl->group = strchr(pl->user, ':');
  if (pl->group != NULL) {
    *pl->group = '\0';
    pl->group++;
  }

  pl->exec = strdup(ci->values[1].value.string);
  if (pl->exec == NULL) {
    ERROR("exec plugin: strdup failed.");
    sfree(pl->user);
    sfree(pl);
    return -1;
  }

  pl->argv = calloc(ci->values_num, sizeof(*pl->argv));
  if (pl->argv == NULL) {
    ERROR("exec plugin: calloc failed.");
    sfree(pl->exec);
    sfree(pl->user);
    sfree(pl);
    return -1;
  }

  {
    char *tmp = strrchr(ci->values[1].value.string, '/');
    if (tmp == NULL)
      sstrncpy(buffer, ci->values[1].value.string, sizeof(buffer));
    else
      sstrncpy(buffer, tmp + 1, sizeof(buffer));
  }
  pl->argv[0] = strdup(buffer);
  if (pl->argv[0] == NULL) {
    ERROR("exec plugin: strdup failed.");
    sfree(pl->argv);
    sfree(pl->exec);
    sfree(pl->user);
    sfree(pl);
    return -1;
  }

  for (i = 1; i < (ci->values_num - 1); i++) {
    if (ci->values[i + 1].type == OCONFIG_TYPE_STRING) {
      pl->argv[i] = strdup(ci->values[i + 1].value.string);
    } else {
      if (ci->values[i + 1].type == OCONFIG_TYPE_NUMBER) {
        snprintf(buffer, sizeof(buffer), "%lf", ci->values[i + 1].value.number);
      } else {
        if (ci->values[i + 1].value.boolean)
          sstrncpy(buffer, "true", sizeof(buffer));
        else
          sstrncpy(buffer, "false", sizeof(buffer));
      }

      pl->argv[i] = strdup(buffer);
    }

    if (pl->argv[i] == NULL) {
      ERROR("exec plugin: strdup failed.");
      break;
    }
  } /* for (i) */

  if (i < (ci->values_num - 1)) {
    while ((--i) >= 0) {
      sfree(pl->argv[i]);
    }
    sfree(pl->argv);
    sfree(pl->exec);
    sfree(pl->user);
    sfree(pl);
    return -1;
  }

  for (i = 0; pl->argv[i] != NULL; i++) {
    DEBUG("exec plugin: argv[%i] = %s", i, pl->argv[i]);
  }

  pl->next = pl_head;
  pl_head = pl;

  return 0;
}

static int exec_config(oconfig_item_t *ci) 
{
  for (int i = 0; i < ci->children_num; i++) {
    oconfig_item_t *child = ci->children + i;
    if ((strcasecmp("Exec", child->key) == 0) ||
        (strcasecmp("NotificationExec", child->key) == 0))
      exec_config_exec(child);
    else {
      WARNING("exec plugin: Unknown config option `%s'.", child->key);
    }
  } /* for (i) */

  return 0;
}

__attribute__((noreturn)) static void exec_child(program_list_t *pl,
                                                 char **envp, int uid, int gid,
                                                 int egid)
{
  int status;

#if HAVE_SETGROUPS
  if (getuid() == 0) {
    gid_t glist[2];
    size_t glist_len;

    glist[0] = gid;
    glist_len = 1;

    if ((gid != egid) && (egid != -1)) {
      glist[1] = egid;
      glist_len = 2;
    }

    setgroups(glist_len, glist);
  }
#endif /* HAVE_SETGROUPS */

  status = setgid(gid);
  if (status != 0) {
    ERROR("exec plugin: setgid (%i) failed: %s", gid, STRERRNO);
    exit(-1);
  }

  if (egid != -1) {
    status = setegid(egid);
    if (status != 0) {
      ERROR("exec plugin: setegid (%i) failed: %s", egid, STRERRNO);
      exit(-1);
    }
  }

  status = setuid(uid);
  if (status != 0) {
    ERROR("exec plugin: setuid (%i) failed: %s", uid, STRERRNO);
    exit(-1);
  }

#ifdef HAVE_EXECVPE
  execvpe(pl->exec, pl->argv, envp);
#else
  environ = envp;
  execvp(pl->exec, pl->argv);
#endif

  ERROR("exec plugin: Failed to execute ``%s'': %s", pl->exec, STRERRNO);
  exit(-1);
}

static void reset_signal_mask(void)
{
  sigset_t ss;

  sigemptyset(&ss);
  sigprocmask(SIG_SETMASK, &ss, /* old mask = */ NULL);
}

static int create_pipe(int fd_pipe[2])
{
  int status;

  status = pipe(fd_pipe);
  if (status != 0) {
    ERROR("exec plugin: pipe failed: %s", STRERRNO);
    return -1;
  }

  return 0;
}

static void close_pipe(int fd_pipe[2])
{
  if (fd_pipe[0] != -1)
    close(fd_pipe[0]);

  if (fd_pipe[1] != -1)
    close(fd_pipe[1]);
}

/*
 * Get effective group ID from group name.
 * Input arguments:
 *       pl  :program list struct with group name
 *       gid :group id to fallback in case egid cannot be determined.
 * Returns:
 *       egid effective group id if successfull,
 *            -1 if group is not defined/not found.
 *            -2 for any buffer allocation error.
 */
static int getegr_id(program_list_t *pl, int gid)
{
  if (pl->group == NULL) {
    return -1;
  }
  if (strcmp(pl->group, "") == 0) {
    return gid;
  }
  struct group *gr_ptr = NULL;
  struct group gr;

  long int grbuf_size = sysconf(_SC_GETGR_R_SIZE_MAX);
  if (grbuf_size <= 0)
    grbuf_size = sysconf(_SC_PAGESIZE);
  if (grbuf_size <= 0)
    grbuf_size = 4096;

  char *temp = NULL;
  char *grbuf = NULL;

  do {
    temp = realloc(grbuf, grbuf_size);
    if (temp == NULL) {
      ERROR("exec plugin: getegr_id for %s: realloc buffer[%ld] failed ",
            pl->group, grbuf_size);
      sfree(grbuf);
      return -2;
    }
    grbuf = temp;
    if (getgrnam_r(pl->group, &gr, grbuf, grbuf_size, &gr_ptr) == 0) {
      sfree(grbuf);
      if (gr_ptr == NULL) {
        ERROR("exec plugin: No such group: `%s'", pl->group);
        return -1;
      }
      return gr.gr_gid;
    } else if (errno == ERANGE) {
      grbuf_size += grbuf_size; // increment buffer size and try again
    } else {
      ERROR("exec plugin: getegr_id failed %s", STRERRNO);
      sfree(grbuf);
      return -2;
    }
  } while (grbuf_size <= MAX_GRBUF_SIZE);
  ERROR("exec plugin: getegr_id Max grbuf size reached  for %s", pl->group);
  sfree(grbuf);
  return -2;
}

/*
 * Creates three pipes (one for reading, one for writing and one for errors),
 * forks a child, sets up the pipes so that fd_in is connected to STDIN of
 * the child and fd_out is connected to STDOUT and fd_err is connected to STDERR
 * of the child. Then is calls `exec_child'.
 */
static int fork_child(program_list_t *pl, int *fd_in, int *fd_out, int *fd_err)
{
  int fd_pipe_in[2] = {-1, -1};
  int fd_pipe_out[2] = {-1, -1};
  int fd_pipe_err[2] = {-1, -1};
  int status;
  int pid;

  int uid;
  int gid;
  int egid;

  struct passwd *sp_ptr;
  struct passwd sp;

  if (pl->pid != 0)
    return -1;

  long int nambuf_size = sysconf(_SC_GETPW_R_SIZE_MAX);
  if (nambuf_size <= 0)
    nambuf_size = sysconf(_SC_PAGESIZE);
  if (nambuf_size <= 0)
    nambuf_size = 4096;
  char nambuf[nambuf_size];

  if ((create_pipe(fd_pipe_in) == -1) || (create_pipe(fd_pipe_out) == -1) ||
      (create_pipe(fd_pipe_err) == -1))
    goto failed;

  sp_ptr = NULL;
  status = getpwnam_r(pl->user, &sp, nambuf, sizeof(nambuf), &sp_ptr);
  if (status != 0) {
    ERROR("exec plugin: Failed to get user information for user ``%s'': %s",
          pl->user, STRERROR(status));
    goto failed;
  }

  if (sp_ptr == NULL) {
    ERROR("exec plugin: No such user: `%s'", pl->user);
    goto failed;
  }

  uid = sp.pw_uid;
  gid = sp.pw_gid;
  if (uid == 0) {
    ERROR("exec plugin: Cowardly refusing to exec program as root.");
    goto failed;
  }

  /* The group configured in the configfile is set as effective group, because
   * this way the forked process can (re-)gain the user's primary group. */
  egid = getegr_id(pl, gid);
  if (egid == -2) {
    goto failed;
  }

  double interval = CDTIME_T_TO_DOUBLE(plugin_get_interval());

  pid = fork();
  if (pid < 0) {
    ERROR("exec plugin: fork failed: %s", STRERRNO);
    goto failed;
  } else if (pid == 0) {
    char interval_buf[128];
    snprintf(interval_buf, sizeof(interval_buf), "NCOLLECTD_INTERVAL=%.3f",
             interval);

    /* max hostname len is 255, so this should be enough */
    char hostname_buf[300];
    snprintf(hostname_buf, sizeof(hostname_buf), "NCOLLECTD_HOSTNAME=%s",
             hostname_g);

    size_t env_size = 0;
    while (environ[env_size] != NULL) {
      ++env_size;
    }

    /* Copy the environment variables */
    char *envp[env_size + 3];
    size_t envp_idx;
    for (envp_idx = 0; environ[envp_idx] != NULL && envp_idx < env_size;
         ++envp_idx) {
      envp[envp_idx] = environ[envp_idx];
    }

    /* Add the collectd environment variables */
    envp[envp_idx++] = interval_buf;
    envp[envp_idx++] = hostname_buf;
    envp[envp_idx++] = NULL;

    /* Close all file descriptors but the pipe end we need. */
    int fd_num = getdtablesize();
    for (int fd = 0; fd < fd_num; fd++) {
      if ((fd == fd_pipe_in[0]) || (fd == fd_pipe_out[1]) ||
          (fd == fd_pipe_err[1]))
        continue;
      close(fd);
    }

    /* Connect the `in' pipe to STDIN */
    if (fd_pipe_in[0] != STDIN_FILENO) {
      dup2(fd_pipe_in[0], STDIN_FILENO);
      close(fd_pipe_in[0]);
    }

    /* Now connect the `out' pipe to STDOUT */
    if (fd_pipe_out[1] != STDOUT_FILENO) {
      dup2(fd_pipe_out[1], STDOUT_FILENO);
      close(fd_pipe_out[1]);
    }

    /* Now connect the `err' pipe to STDERR */
    if (fd_pipe_err[1] != STDERR_FILENO) {
      dup2(fd_pipe_err[1], STDERR_FILENO);
      close(fd_pipe_err[1]);
    }

    /* Unblock all signals */
    reset_signal_mask();

    exec_child(pl, envp, uid, gid, egid);
    /* does not return */
  }

  close(fd_pipe_in[0]);
  close(fd_pipe_out[1]);
  close(fd_pipe_err[1]);

  if (fd_in != NULL)
    *fd_in = fd_pipe_in[1];
  else
    close(fd_pipe_in[1]);

  if (fd_out != NULL)
    *fd_out = fd_pipe_out[0];
  else
    close(fd_pipe_out[0]);

  if (fd_err != NULL)
    *fd_err = fd_pipe_err[0];
  else
    close(fd_pipe_err[0]);

  return pid;

failed:
  close_pipe(fd_pipe_in);
  close_pipe(fd_pipe_out);
  close_pipe(fd_pipe_err);

  return -1;
}

static int parse_line(char *buffer)
{
  if (strncasecmp("PUTGAUGE", buffer, strlen("PUTGAUGE")) == 0)
    return cmd_handle_putval(stdout, buffer);
  if (strncasecmp("PUTCOUNTER", buffer, strlen("PUTCOUNTER")) == 0)
    return cmd_handle_putval(stdout, buffer);
  if (strncasecmp("PUTINFO", buffer, strlen("PUTINFO")) == 0)
    return cmd_handle_putval(stdout, buffer);
  else if (strncasecmp("PUTNOTIF", buffer, strlen("PUTNOTIF")) == 0)
    return handle_putnotif(stdout, buffer);
  else {
    ERROR("exec plugin: Unable to parse command, ignoring line: \"%s\"",
          buffer);
    return -1;
  }
}

static void *exec_read_one(void *arg)
{
  program_list_t *pl = (program_list_t *)arg;
  int fd, fd_err;
  struct pollfd fds[2] = {{0}};
  int status;
  char buffer[1200]; /* if not completely read */
  char buffer_err[1024];
  char *pbuffer = buffer;
  char *pbuffer_err = buffer_err;

  status = fork_child(pl, NULL, &fd, &fd_err);
  if (status < 0) {
    /* Reset the "running" flag */
    pthread_mutex_lock(&pl_lock);
    pl->flags &= ~PL_RUNNING;
    pthread_mutex_unlock(&pl_lock);
    pthread_exit((void *)1);
  }
  pl->pid = status;

  assert(pl->pid != 0);

  fds[0].fd = fd;
  fds[0].events = POLLIN;
  fds[1].fd = fd_err;
  fds[1].events = POLLIN;

  while (1) {
    int len;

    status = poll(fds, STATIC_ARRAY_SIZE(fds), -1);
    if (status < 0) {
      if (errno == EINTR)
        continue;
      break;
    }

    if (fds[0].revents & (POLLIN | POLLHUP)) {
      char *pnl;

      len = read(fd, pbuffer, sizeof(buffer) - 1 - (pbuffer - buffer));

      if (len < 0) {
        if (errno == EAGAIN || errno == EINTR)
          continue;
        break;
      } else if (len == 0)
        break; /* We've reached EOF */

      pbuffer[len] = '\0';

      len += pbuffer - buffer;
      pbuffer = buffer;

      while ((pnl = strchr(pbuffer, '\n'))) {
        *pnl = '\0';
        if (*(pnl - 1) == '\r')
          *(pnl - 1) = '\0';

        parse_line(pbuffer);

        pbuffer = ++pnl;
      }
      /* not completely read ? */
      if (pbuffer - buffer < len) {
        len -= pbuffer - buffer;
        memmove(buffer, pbuffer, len);
        pbuffer = buffer + len;
      } else
        pbuffer = buffer;
    } else if (fds[0].revents & (POLLERR | POLLNVAL)) {
      ERROR("exec plugin: Failed to read pipe from `%s'.", pl->exec);
      break;
    } else if (fds[1].revents & (POLLIN | POLLHUP)) {
      char *pnl;

      len = read(fd_err, pbuffer_err,
                 sizeof(buffer_err) - 1 - (pbuffer_err - buffer_err));

      if (len < 0) {
        if (errno == EAGAIN || errno == EINTR)
          continue;
        break;
      } else if (len == 0) {
        /* We've reached EOF */
        NOTICE("exec plugin: Program `%s' has closed STDERR.", pl->exec);

        /* Clean up file descriptor */
        close(fd_err);
        fd_err = -1;
        fds[1].fd = -1;
        fds[1].events = 0;
        continue;
      }

      pbuffer_err[len] = '\0';

      len += pbuffer_err - buffer_err;
      pbuffer_err = buffer_err;

      while ((pnl = strchr(pbuffer_err, '\n'))) {
        *pnl = '\0';
        if (*(pnl - 1) == '\r')
          *(pnl - 1) = '\0';

        ERROR("exec plugin: exec_read_one: error = %s", pbuffer_err);

        pbuffer_err = ++pnl;
      }
      /* not completely read ? */
      if (pbuffer_err - buffer_err < len) {
        len -= pbuffer_err - buffer_err;
        memmove(buffer_err, pbuffer_err, len);
        pbuffer_err = buffer_err + len;
      } else
        pbuffer_err = buffer_err;
    } else if (fds[1].revents & (POLLERR | POLLNVAL)) {
      WARNING("exec plugin: Ignoring STDERR for program `%s'.", pl->exec);
      /* Clean up file descriptor */
      if ((fds[1].revents & POLLNVAL) == 0) {
        close(fd_err);
        fd_err = -1;
      }
      fds[1].fd = -1;
      fds[1].events = 0;
    }
  }

  DEBUG("exec plugin: exec_read_one: Waiting for `%s' to exit.", pl->exec);
  if (waitpid(pl->pid, &status, 0) > 0)
    pl->status = status;

  DEBUG("exec plugin: Child %i exited with status %i.", (int)pl->pid,
        pl->status);

  pl->pid = 0;

  pthread_mutex_lock(&pl_lock);
  pl->flags &= ~PL_RUNNING;
  pthread_mutex_unlock(&pl_lock);

  close(fd);
  if (fd_err >= 0)
    close(fd_err);

  pthread_exit((void *)0);
  return NULL;
}

static void *exec_notification_one(void *arg)
{
  program_list_t *pl = ((program_list_and_notification_t *)arg)->pl;
  int fd;

  int pid = fork_child(pl, &fd, NULL, NULL);
  if (pid < 0) {
    sfree(arg);
    pthread_exit((void *)1);
  }

  FILE *fh = fdopen(fd, "w");
  if (fh == NULL) {
    ERROR("exec plugin: fdopen (%i) failed: %s", fd, STRERRNO);
    kill(pid, SIGTERM);
    close(fd);
    sfree(arg);
    pthread_exit((void *)1);
  }

  notification_t *n = &((program_list_and_notification_t *)arg)->n;
  strbuf_t buf = STRBUF_CREATE;
  notification_marshal(&buf, n);

  fputs(buf.ptr, fh);
  strbuf_destroy(&buf);

  fflush(fh);
  fclose(fh);

  int status;
  waitpid(pid, &status, 0);

  DEBUG("exec plugin: Child %i exited with status %i.", pid, status);

  sfree(arg);
  pthread_exit((void *)0);
  return NULL;
}

static int exec_init(void)
{
  struct sigaction sa = {.sa_handler = sigchld_handler};

  sigaction(SIGCHLD, &sa, NULL);

#if defined(HAVE_SYS_CAPABILITY_H) && defined(CAP_SETUID) && defined(CAP_SETGID)
  if ((check_capability(CAP_SETUID) != 0) ||
      (check_capability(CAP_SETGID) != 0)) {
    if (getuid() == 0)
      WARNING(
          "exec plugin: Running collectd as root, but the CAP_SETUID "
          "or CAP_SETGID capabilities are missing. The plugin's read function "
          "will probably fail. Is your init system dropping capabilities?");
    else
      WARNING(
          "exec plugin: collectd doesn't have the CAP_SETUID or "
          "CAP_SETGID capabilities. If you don't want to run collectd as root, "
          "try running \"setcap 'cap_setuid=ep cap_setgid=ep'\" on the "
          "collectd binary.");
  }
#endif

  return 0;
}

static int exec_read(void)
{
  for (program_list_t *pl = pl_head; pl != NULL; pl = pl->next) {
    pthread_t t;

    /* Only execute `normal' style executables here. */
    if ((pl->flags & PL_NORMAL) == 0)
      continue;

    pthread_mutex_lock(&pl_lock);
    /* Skip if a child is already running. */
    if ((pl->flags & PL_RUNNING) != 0) {
      pthread_mutex_unlock(&pl_lock);
      continue;
    }
    pl->flags |= PL_RUNNING;
    pthread_mutex_unlock(&pl_lock);

    int status =
        plugin_thread_create(&t, exec_read_one, (void *)pl, "exec read");
    if (status == 0) {
      pthread_detach(t);
    } else {
      ERROR("exec plugin: plugin_thread_create failed.");
    }
  } /* for (pl) */

  return 0;
}

static int exec_notification(const notification_t *n,
                             user_data_t __attribute__((unused)) * user_data)
{
  program_list_and_notification_t *pln;

  for (program_list_t *pl = pl_head; pl != NULL; pl = pl->next) {
    pthread_t t;

    /* Only execute `notification' style executables here. */
    if ((pl->flags & PL_NOTIF_ACTION) == 0)
      continue;

    /* Skip if a child is already running. */
    if (pl->pid != 0)
      continue;

    pln = malloc(sizeof(*pln));
    if (pln == NULL) {
      ERROR("exec plugin: malloc failed.");
      continue;
    }

    pln->pl = pl;
    memcpy(&pln->n, n, sizeof(notification_t));

    int status = plugin_thread_create(&t, exec_notification_one, (void *)pln,
                                      "exec notify");
    if (status == 0) {
      pthread_detach(t);
    } else {
      ERROR("exec plugin: plugin_thread_create failed.");
    }
  } /* for (pl) */

  return 0;
}

static int exec_shutdown(void)
{
  program_list_t *pl;
  program_list_t *next;

  pl = pl_head;
  while (pl != NULL) {
    next = pl->next;

    if (pl->pid > 0) {
      kill(pl->pid, SIGTERM);
      INFO("exec plugin: Sent SIGTERM to %hu", (unsigned short int)pl->pid);
    }

    for (int i = 0; pl->argv[i] != NULL; i++) {
      sfree(pl->argv[i]);
    }
    sfree(pl->argv);
    sfree(pl->exec);
    sfree(pl->user);
    sfree(pl);

    pl = next;
  } /* while (pl) */
  pl_head = NULL;

  return 0;
}

void module_register(void)
{
  plugin_register_complex_config("exec", exec_config);
  plugin_register_init("exec", exec_init);
  plugin_register_read("exec", exec_read);
  plugin_register_notification("exec", exec_notification, /* user_data = */ NULL);
  plugin_register_shutdown("exec", exec_shutdown);
}
