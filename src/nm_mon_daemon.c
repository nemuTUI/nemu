#if defined (NM_OS_LINUX)
# define _GNU_SOURCE
#endif
#include <nm_core.h>
#include <nm_dbus.h>
#include <nm_utils.h>
#include <nm_cfg_file.h>
#include <nm_qmp_control.h>

#include <sys/wait.h> /* waitpid(2) */
#include <time.h> /* nanosleep(2) */
#include <pthread.h>
#include <mqueue.h>

#include <json.h>

/* TODO get len from /proc/sys/fs/mqueue/msgsize_max */
#define MQ_LEN 8192

static volatile sig_atomic_t nm_mon_rebuild = 0;

static void nm_mon_check_vms(const nm_vect_t *mon_list);
static void nm_mon_build_list(nm_vect_t *list, nm_vect_t *vms);
static void nm_mon_signals_handler(int signal);
static int nm_mon_store_pid(void);

typedef struct nm_mon_item {
    nm_str_t *name;
    int8_t state;
} nm_mon_item_t;

typedef struct nm_qmp_data {
    bool stop;
} nm_qmp_data_t;

typedef struct nm_qmp_w_data {
    nm_str_t *cmd;
    pthread_barrier_t *barrier;
} nm_qmp_w_data_t;

typedef struct nm_clean_data {
    nm_vect_t *mon_list;
    nm_vect_t *vm_list;
    pthread_t *qmp_worker;
    nm_qmp_data_t qmp_data;
} nm_clean_data_t;

#define NM_ITEM_INIT (nm_mon_item_t) { NULL, -1 }
#define NM_QMP_INIT (nm_qmp_data_t) { false }
#define NM_QMP_W_INIT (nm_qmp_w_data_t) { NULL, NULL }
#define NM_CLEAN_INIT (nm_clean_data_t) { NULL, NULL, NULL, NM_QMP_INIT }

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

    data->qmp_data.stop = true;
    pthread_join(*data->qmp_worker, NULL);

    unlink(nm_cfg_get()->daemon_pid.data);
    nm_exit_core();
}
#endif /* NM_OS_LINUX */

void *nm_qmp_worker(void *data)
{
    struct json_object *parsed, *args, *jobid;
    nm_str_t vmname = NM_INIT_STR;
    nm_qmp_w_data_t *arg = data;
    nm_str_t cmd = NM_INIT_STR;
    const char *jobid_str, *it;
    bool get_name = false;

    nm_str_copy(&cmd, arg->cmd);
    pthread_barrier_wait(arg->barrier);

    pthread_detach(pthread_self());

    /* TODO error handling */
    /* extract VM name */
    parsed = json_tokener_parse(cmd.data);
    json_object_object_get_ex(parsed, "arguments", &args);
    json_object_object_get_ex(args, "job-id", &jobid);
    jobid_str = json_object_get_string(jobid);

    it = jobid_str;

    while (*it) {
        if (*it == '-' && !get_name) {
            get_name = true;
            it++;
        } else if (*it == '-' && get_name) {
            break;
        }

        if (get_name) {
            nm_str_add_char_opt(&vmname, *it);
        }

        it++;
    }

    nm_qmp_vm_exec_async(&vmname, cmd.data, jobid_str);

    json_object_put(parsed);

    nm_str_free(&cmd);
    nm_str_free(&vmname);

    pthread_exit(NULL);
}

void *nm_qmp_dispatcher(void *thr)
{
    const nm_cfg_t *cfg = nm_cfg_get();
    nm_qmp_data_t *args = thr;
    struct timespec ts;
    ssize_t rcv_len;
    FILE *log;
    mqd_t mq;

    if ((log = fopen(cfg->log_path.data, "w")) == NULL) {
        pthread_exit(NULL);
    }

    if ((mq = mq_open("/nemu-qmp", O_RDWR | O_CREAT,
                    0600, NULL)) == -1) {
        fprintf(log, "%s:cannot open mq: %s\n", __func__, strerror(errno));
        fflush(log);
        goto out;
    }

    ts.tv_sec = 1;
    ts.tv_nsec = 0;

    while (!args->stop) {
        char msg[MQ_LEN + 1] = {0};

        rcv_len = mq_timedreceive(mq, msg, MQ_LEN, NULL, &ts);
        if (rcv_len > 0) {
            nm_qmp_w_data_t data = NM_QMP_W_INIT;
            nm_str_t cmd = NM_INIT_STR;
            pthread_barrier_t barr;
            pthread_t thr;

            if (pthread_barrier_init(&barr, NULL, 2) != 0) {
                fprintf(log, "%s:cannot init barrier\n", __func__);
                fflush(log);
            }

            data.barrier = &barr;
            data.cmd = &cmd;

            nm_str_format(&cmd, "%s", msg);

            if (pthread_create(&thr, NULL, nm_qmp_worker, &data) != 0) {
                nm_exit(EXIT_FAILURE);
            }
#if defined (NM_OS_LINUX)
            pthread_setname_np(thr, "nemu-qmp-worker");
#endif
            pthread_barrier_wait(&barr);
            pthread_barrier_destroy(&barr);
            nm_str_free(&cmd);
        }
    }

out:
    fclose(log);
    mq_close(mq);
    pthread_exit(NULL);
}

void nm_mon_start(void)
{
    const nm_cfg_t *cfg = nm_cfg_get();
    pid_t pid, wpid;
    int wstatus = 0;

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
            nm_exit(EXIT_FAILURE);
        }
        break;
    case -1: /* error */
        fprintf(stderr, "%s: fork error: %s\n", __func__, strerror(errno));
        nm_exit(EXIT_FAILURE);
    default: /* parent */
        wpid = waitpid(pid, &wstatus, 0);
        if ((wpid == pid) && (WEXITSTATUS(wstatus) != 0)) {
            fprintf(stderr, "%s: failed to start daemon\n", __func__);
            nm_exit(EXIT_FAILURE);
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
    nm_clean_data_t clean = NM_CLEAN_INIT;
    nm_vect_t mon_list = NM_INIT_VECT;
    nm_vect_t vm_list = NM_INIT_VECT;
    const nm_cfg_t *cfg;
    struct sigaction sa;
    struct timespec ts;
    pthread_t qmp_thr;
    pid_t pid;

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
        nm_exit(EXIT_FAILURE);
    default: /* parent */
        nm_exit_core();
    }

    if (setsid() < 0) {
        fprintf(stderr, "%s: setsid error: %s\n", __func__, strerror(errno));
        nm_exit(EXIT_FAILURE);
    }

    if (chdir("/") < 0) {
        fprintf(stderr, "%s: chdir error: %s\n", __func__, strerror(errno));
        nm_exit(EXIT_FAILURE);
    }

#if defined (NM_OS_LINUX)

    clean.mon_list = &mon_list;
    clean.vm_list = &vm_list;
    clean.qmp_worker = &qmp_thr;

    if (on_exit(nm_mon_cleanup, &clean) != 0) {
        fprintf(stderr, "%s: on_exit(3) failed\n", __func__);
        nm_exit(EXIT_FAILURE);
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
        nm_exit(EXIT_FAILURE);
    }

    nm_db_init();
    nm_mon_build_list(&mon_list, &vm_list);
#if defined (NM_WITH_DBUS)
    if (nm_dbus_connect() != NM_OK) {
        nm_exit(EXIT_FAILURE);
    }
#endif
    if (pthread_create(&qmp_thr, NULL, nm_qmp_dispatcher, &clean.qmp_data) != 0) {
        nm_exit(EXIT_FAILURE);
    }
#if defined (NM_OS_LINUX)
    pthread_setname_np(qmp_thr, "nemu-qmp-dsp");
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
                nm_str_format(&body, "%s stopped", name);
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
        nm_exit(EXIT_SUCCESS);
    case SIGTERM:
        nm_exit(EXIT_FAILURE);
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
