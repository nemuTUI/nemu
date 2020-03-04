#include <nm_core.h>
#include <nm_dbus.h>
#include <nm_utils.h>
#include <nm_cfg_file.h>
#include <nm_qmp_control.h>

#include <sys/wait.h> /* waitpid(2) */
#include <time.h> /* nanosleep(2) */

static volatile sig_atomic_t nm_mon_rebuild = 0;

static void nm_mon_check_vms(const nm_vect_t *mon_list);
static void nm_mon_build_list(nm_vect_t *list, nm_vect_t *vms);
static void nm_mon_signals_handler(int signal);
static int nm_mon_store_pid(void);

typedef struct nm_mon_item {
    nm_str_t *name;
    int8_t state;
} nm_mon_item_t;

typedef struct nm_clean_data {
    nm_vect_t *mon_list;
    nm_vect_t *vm_list;
} nm_clean_data_t;

#define NM_ITEM_INIT (nm_mon_item_t) { NULL, -1 }
#define NM_CLEAN_INIT (nm_clean_data_t) { NULL, NULL }

static inline int8_t nm_mon_item_get_status(const nm_vect_t *v, const size_t idx)
{
    return ((nm_mon_item_t *) nm_vect_at(v, idx))->state;
}

static inline void nm_mon_item_set_status(const nm_vect_t *v, const size_t idx, int8_t s)
{
    ((nm_mon_item_t *) nm_vect_at(v, idx))->state = s;
}

static inline nm_str_t *nm_mon_item_get_name(const nm_vect_t *v, const size_t idx)
{
    return ((nm_mon_item_t *) nm_vect_at(v, idx))->name;
}

static inline char *nm_mon_item_get_name_cstr(const nm_vect_t *v, const size_t idx)
{
    return ((nm_mon_item_t *) nm_vect_at(v, idx))->name->data;
}

#if defined (NM_OS_LINUX)
static void nm_mon_cleanup(int rc, void *arg)
{
    nm_clean_data_t *data = arg;

    nm_debug("mon daemon exited: %d\n", rc);

    nm_vect_free(data->mon_list, NULL);
    nm_vect_free(data->vm_list, nm_str_vect_free_cb);

#if defined (NM_WITH_DBUS)
    nm_dbus_disconnect();
#endif

    unlink(nm_cfg_get()->daemon_pid.data);
    nm_exit_core();
}
#endif /* NM_OS_LINUX */

void nm_mon_start(void)
{
    pid_t pid, wpid;
    int wstatus = 0;
    const nm_cfg_t *cfg = nm_cfg_get();

    if (!cfg->start_daemon)
        return;

    if (access(cfg->daemon_pid.data, R_OK) != -1) {
        return;
    }

    pid = fork();

    switch (pid) {
    case 0: /* child */
        if (execlp(NM_PROGNAME, NM_PROGNAME, "--daemon", NULL) == -1) {
            fprintf(stderr, "%s: execlp error: %s\n", __func__, strerror(errno));
            exit(EXIT_FAILURE);
        }
        break;
    case -1: /* error */
        fprintf(stderr, "%s: fork error: %s\n", __func__, strerror(errno));
        exit(EXIT_FAILURE);
    default: /* parent */
        wpid = waitpid(pid, &wstatus, 0);
        if ((wpid == pid) && (WEXITSTATUS(wstatus) != 0)) {
            fprintf(stderr, "%s: failed to start daemon\n", __func__);
            exit(EXIT_FAILURE);
        }
        break;
    }
}

void nm_mon_ping(void)
{
    pid_t pid;
    int fd;
    struct stat info;
    const char *path = nm_cfg_get()->daemon_pid.data;
    char *buf;

    memset(&info, 0x0, sizeof(info));

    if ((stat(path, &info) == -1) || (!info.st_size))
        return;

    buf = nm_calloc(1, info.st_size + 1);
    fd = open(path, O_RDONLY);
    if (fd == -1) {
        nm_debug("%s: error open pid file: %s: %s",
                __func__, path, strerror(errno));
        free(buf);
        return;
    }

    if (read(fd, buf, info.st_size) < 0) {
        nm_debug("%s: error read pid file: %s: %s",
                __func__, path, strerror(errno));
        goto out;
    }

    pid = nm_str_ttoul(buf, 10);

    if (kill(pid, SIGUSR1) < 0) {
        nm_debug("%s: error send signal to pid %d: %s",
                __func__, pid, strerror(errno));
    }
out:
    close(fd);
    free(buf);
}

void nm_mon_loop(void)
{
    struct timespec ts;
    struct sigaction sa;
    pid_t pid;
    nm_vect_t mon_list = NM_INIT_VECT;
    nm_vect_t vm_list = NM_INIT_VECT;
    const nm_cfg_t *cfg;
    nm_clean_data_t clean = NM_CLEAN_INIT;

    nm_cfg_init();
    cfg = nm_cfg_get();
    if (access(cfg->daemon_pid.data, R_OK) != -1) {
        return;
    }

    pid = fork();

    switch(pid) {
    case 0: /* child */
        break;
    case -1: /* error */
        fprintf(stderr, "%s: fork error: %s\n", __func__, strerror(errno));
        exit(EXIT_FAILURE);
    default: /* parent */
        nm_exit_core();
    }

    if (setsid() < 0) {
        fprintf(stderr, "%s: setsid error: %s\n", __func__, strerror(errno));
        exit(EXIT_FAILURE);
    }

    if (chdir("/") < 0) {
        fprintf(stderr, "%s: chdir error: %s\n", __func__, strerror(errno));
        exit(EXIT_FAILURE);
    }

#if defined (NM_OS_LINUX)
    clean.mon_list = &mon_list;
    clean.vm_list = &vm_list;

    if (on_exit(nm_mon_cleanup, &clean) != 0) {
        fprintf(stderr, "%s: on_exit(3) failed\n", __func__);
        exit(EXIT_FAILURE);
    }
#endif

    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);

    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sa.sa_handler = nm_mon_signals_handler;
    sigaction(SIGUSR1, &sa, NULL);
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    ts.tv_sec = cfg->daemon_sleep / 1000;
    ts.tv_nsec = (cfg->daemon_sleep % 1000) * 1e+6;

    if (nm_mon_store_pid() != NM_OK) {
        exit(EXIT_FAILURE);
    }

    nm_db_init();
    nm_mon_build_list(&mon_list, &vm_list);
#if defined (NM_WITH_DBUS)
    if (nm_dbus_connect() != NM_OK) {
        exit(EXIT_FAILURE);
    }
#endif
    
    for (;;) {
        if (nm_mon_rebuild) {
            nm_mon_build_list(&mon_list, &vm_list);
            nm_mon_rebuild = 0;
        }
        nm_mon_check_vms(&mon_list);
        nanosleep(&ts, NULL);
    }
}

static void nm_mon_check_vms(const nm_vect_t *mon_list)
{
    nm_str_t body = NM_INIT_STR;

    for (size_t n = 0; n < mon_list->n_memb; n++) {
        char *name = nm_mon_item_get_name_cstr(mon_list, n);
        int8_t status = nm_mon_item_get_status(mon_list, n);

        if (nm_qmp_test_socket(nm_mon_item_get_name(mon_list, n)) == NM_OK) {
            if (!status) {
                nm_str_format(&body, "%s started", name);
#if defined (NM_WITH_DBUS)
                nm_dbus_send_notify("VM status changed:", body.data);
#endif
            }
            nm_mon_item_set_status(mon_list, n, NM_TRUE);
        } else {
            if (status == 1) {
                nm_str_format(&body, "%s stoped", name);
#if defined (NM_WITH_DBUS)
                nm_dbus_send_notify("VM status changed:", body.data);
#endif
            }
            nm_mon_item_set_status(mon_list, n, NM_FALSE);
        }
        nm_str_free(&body);
    }
}

static void nm_mon_build_list(nm_vect_t *list, nm_vect_t *vms)
{
    nm_vect_free(list, NULL);
    nm_vect_free(vms, nm_str_vect_free_cb);

    nm_db_select(NM_GET_VMS_SQL, vms);

    for (size_t n = 0; n < vms->n_memb; n++) {
        nm_mon_item_t item = NM_ITEM_INIT;

        item.name = nm_vect_str(vms, n);
        nm_vect_insert(list, &item, sizeof(nm_mon_item_t), NULL);
    }
}

static void nm_mon_signals_handler(int signal)
{
    switch (signal) {
    case SIGUSR1:
        nm_mon_rebuild = 1;
        break;
    case SIGINT:
        exit(EXIT_SUCCESS);
    case SIGTERM:
        exit(EXIT_FAILURE);
    }
}

static int nm_mon_store_pid(void)
{
    int fd, rc = NM_OK;
    pid_t pid;
    char *path = nm_cfg_get()->daemon_pid.data;
    nm_str_t res = NM_INIT_STR;

    fd = open(path, O_WRONLY | O_CREAT, 0644);
    if (fd == -1) {
        nm_debug("%s: error create pid file: %s: %s",
                __func__, path, strerror(errno));
        return NM_ERR;
    }

    pid = getpid();
    nm_str_format(&res, "%d", pid);

    if (write(fd, res.data, res.len) < 0) {
        nm_debug("%s: error save pid number\n", __func__);
        rc = NM_ERR;
    }

    close(fd);
    nm_str_free(&res);

    return rc;
}
/* vim:set ts=4 sw=4: */
