#if defined (NM_OS_LINUX)
# define _GNU_SOURCE
#endif
#include <nm_core.h>
#include <nm_dbus.h>
#include <nm_utils.h>
#include <nm_cfg_file.h>
#include <nm_mon_daemon.h>
#include <nm_remote_api.h>
#include <nm_qmp_control.h>

#include <sys/wait.h> /* waitpid(2) */
#include <time.h> /* nanosleep(2) */
#include <pthread.h>
#include <mqueue.h>

#include <json.h>

static volatile sig_atomic_t nm_mon_rebuild = 0;

static void nm_mon_check_vms(const nm_vect_t *mon_list);
static void nm_mon_build_list(nm_vect_t *list, nm_vect_t *vms);
static void nm_mon_signals_handler(int signal);
static int nm_mon_store_pid(void);

typedef struct nm_qmp_w_data {
    nm_str_t *cmd;
    pthread_barrier_t *barrier;
} nm_qmp_w_data_t;

typedef struct nm_clean_data {
    nm_mon_vms_t vms;
#if defined (NM_OS_LINUX)
    nm_vect_t *vm_list;
    pthread_t *qmp_worker;
    pthread_t *api_server;
#endif
    nm_thr_ctrl_t qmp_ctrl;
    nm_thr_ctrl_t api_ctrl;
} nm_clean_data_t;

#define NM_QMP_W_INIT (nm_qmp_w_data_t) { NULL, NULL }
#define NM_CLEAN_INIT (nm_clean_data_t) \
    { NM_MON_VMS_INIT, NULL, NULL, NULL, NM_THR_CTRL_INIT, NM_THR_CTRL_INIT }

#if defined (NM_OS_LINUX)
static void nm_mon_cleanup(int rc, void *arg)
{
    nm_clean_data_t *data = arg;
    const nm_cfg_t *cfg = nm_cfg_get();

    nm_debug("mon daemon exited: %d\n", rc);

    nm_vect_free(data->vms.list, NULL);
    nm_vect_free(data->vm_list, nm_str_vect_free_cb);

#if defined (NM_WITH_DBUS)
    nm_dbus_disconnect();
#endif
    data->qmp_ctrl.stop = true;
    pthread_join(*data->qmp_worker, NULL);
#if defined (NM_WITH_REMOTE)
    if (cfg->api_server) {
        data->api_ctrl.stop = true;
        pthread_join(*data->api_server, NULL);
    }
#endif

    if (unlink(cfg->daemon_pid.data) != 0) {
        nm_debug("error delete mon daemon pidfile: %s\n", strerror(rc));
    }
    nm_exit_core();
}
#endif /* NM_OS_LINUX */

void *nm_qmp_worker(void *data)
{
    struct json_object *parsed, *args, *jobid;
    nm_str_t jobid_copy = NM_INIT_STR;
    nm_str_t vmname = NM_INIT_STR;
    nm_qmp_w_data_t *arg = data;
    nm_str_t cmd = NM_INIT_STR;
    const char *jobid_str;
    char *name_start;

    nm_str_copy(&cmd, arg->cmd);
    pthread_barrier_wait(arg->barrier);

    pthread_detach(pthread_self());

    parsed = json_tokener_parse(cmd.data);

    if (!parsed) {
        nm_debug("%s: cannot parse json\n", __func__);
        pthread_exit(NULL);
    }
    json_object_object_get_ex(parsed, "arguments", &args);
    json_object_object_get_ex(args, "job-id", &jobid);

    if (!args || !jobid) {
        nm_debug("%s: malformed json\n", __func__);
        pthread_exit(NULL);
    }
    jobid_str = json_object_get_string(jobid);

    /*
     *  Get VM name from job-id
     *  input string example: vmdel-vmname-2021-05-27-15-14-12-tVusSMWY
     */
    nm_str_format(&jobid_copy, "%s", jobid_str);

    /*
     *  Cut job-id, we have 7 dashes in UID.
     *  input:  vmdel-vmname-2021-05-27-15-14-12-tVusSMWY
     *                      <----<--<--<--<--<--<--------
     *                      7    6  5  4  3  2  1
     *  result: vmdel-vmname
     */
    for (size_t sep = 0; sep < 7; sep++) {
        char *dash = strrchr(jobid_copy.data, '-');
        if (dash) {
            *dash = '\0';
        } else {
            break;
        }
    }

    /*
     *  Cut VM name:
     *  input:  vmdel-vmname
     *          ----->
     *  result: -vmname
     */
    name_start = strchr(jobid_copy.data, '-');
    if (!name_start) {
        nm_debug("%s: error get VM name from job-id\n", __func__);
        pthread_exit(NULL);
    }

    name_start++; /* skip dash */
    nm_str_format(&vmname, "%s", name_start);

    nm_qmp_vm_exec_async(&vmname, cmd.data, jobid_str);

    json_object_put(parsed);

    nm_str_free(&cmd);
    nm_str_free(&vmname);
    nm_str_free(&jobid_copy);

    pthread_exit(NULL);
}

void *nm_qmp_dispatcher(void *ctx)
{
    const nm_cfg_t *cfg = nm_cfg_get();
    nm_thr_ctrl_t *args = ctx;
    struct mq_attr mq_attr;
    char *msg = NULL;
    ssize_t rcv_len;
    FILE *log;
    mqd_t mq;

    if ((log = fopen(cfg->log_path.data, "w")) == NULL) {
        pthread_exit(NULL);
    }

    if ((mq = mq_open(NM_MQ_PATH, O_RDWR | O_CREAT,
                    0600, NULL)) == (mqd_t) -1) {
        fprintf(log, "%s:cannot open mq: %s\n", __func__, strerror(errno));
        fflush(log);
        pthread_exit(NULL);
    }

    memset(&mq_attr, 0, sizeof(mq_attr));
    if (mq_getattr(mq, &mq_attr) == -1) {
        fprintf(log, "%s:cannot get mq attrs: %s\n", __func__, strerror(errno));
        goto out;
    }

    msg = nm_calloc(1, mq_attr.mq_msgsize + 1);

    while (!args->stop) {
        struct timespec ts;

        memset(msg, 0, mq_attr.mq_msgsize + 1);
        memset(&ts, 0, sizeof(ts));
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_sec += 1;

        rcv_len = mq_timedreceive(mq, msg, mq_attr.mq_msgsize, NULL, &ts);
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
    free(msg);
    fclose(log);
    mq_close(mq);
    pthread_exit(NULL);
}

static bool nm_mon_check_version(pid_t *opid)
{
    const char *path = nm_cfg_get()->daemon_pid.data;
    struct stat info;
    bool res = false;
    char *buf, *nl;
    int fd;

    memset(&info, 0x0, sizeof(info));

    if ((stat(path, &info) == -1) || (!info.st_size))
        return false;

    buf = nm_calloc(1, info.st_size + 1);
    fd = open(path, O_RDONLY);
    if (fd == -1) {
        nm_debug("%s: error open pid file: %s: %s",
                __func__, path, strerror(errno));
        free(buf);
        return false;
    }

    if (read(fd, buf, info.st_size) < 0) {
        nm_debug("%s: error read pid file: %s: %s",
                __func__, path, strerror(errno));
        goto out;
    }

    if ((nl = strchr(buf, '\n')) != NULL) {
        /* nEMU >= 3.0.0, cut version number */
        nm_debug("%s: daemon version: %s [actual: %s]\n",
                __func__, nl + 1, NM_VERSION);
        if (nm_str_cmp_tt(nl + 1, NM_VERSION) != NM_OK) {
            res = true;
            *nl = '\0';
            nm_debug("%s: daemon version different, need restart\n", __func__);
        }
    } else {
        res = true;
        nm_debug("%s: nEMU < 3.0.0, cannot check version,"
                " restart anyway\n", __func__);
    }

    *opid = nm_str_ttoul(buf, 10);

out:
    close(fd);
    free(buf);
    return res;
}

void nm_mon_start(void)
{
    const nm_cfg_t *cfg = nm_cfg_get();
    bool need_restart = false;
    pid_t pid, wpid, opid;
    int wstatus = 0;

    if (!cfg->start_daemon)
        return;

    if (access(cfg->daemon_pid.data, R_OK) != -1) {
        if ((need_restart = nm_mon_check_version(&opid)) == false) {
            return;
        }
    }

    if (need_restart) {
        struct timespec ts;
        bool restart_ok = false;

        memset(&ts, 0, sizeof(ts));
        ts.tv_nsec = 5e+7; /* 0.05sec */

        if (kill(opid, SIGINT) < 0) {
            nm_bug("%s: error send signal to pid %d: %s",
                    __func__, opid, strerror(errno));
        }

        /* wait for daemon shutdown */
        for (int try = 0; try < 300; try++) {
            nm_debug("%s: try #%d\n", __func__, try);
            if (kill(opid, 0) < 0) {
                restart_ok = true;
                break;
            }
            nanosleep(&ts, NULL);
        }

        if (!restart_ok) {
            nm_bug("%s: after 15 seconds the daemon has not exited\n", __func__);
        }
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
    char *buf, *nl;

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

    if ((nl = strchr(buf, '\n')) != NULL) {
        /* nEMU >= 3.0.0, cut version number */
        *nl = '\0';
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
#if defined (NM_WITH_REMOTE)
    nm_api_ctx_t api_ctx = NM_API_CTX_INIT;
#endif
    nm_clean_data_t clean = NM_CLEAN_INIT;
    nm_vect_t mon_list = NM_INIT_VECT;
    nm_vect_t vm_list = NM_INIT_VECT;
    pthread_t qmp_thr, api_srv;
    const nm_cfg_t *cfg;
    struct sigaction sa;
    struct timespec ts;
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
    clean.vms.list = &mon_list;
    clean.vm_list = &vm_list;
    clean.qmp_worker = &qmp_thr;
    clean.api_server = &api_srv;

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
    if (pthread_create(&qmp_thr, NULL, nm_qmp_dispatcher, &clean.qmp_ctrl) != 0) {
        nm_exit(EXIT_FAILURE);
    }
#if defined (NM_OS_LINUX)
    pthread_setname_np(qmp_thr, "nemu-qmp-dsp");
#endif

#if defined (NM_WITH_REMOTE)
    if (cfg->api_server) {
        api_ctx.vms = &clean.vms;
        api_ctx.ctrl = &clean.api_ctrl;
        if (pthread_create(&api_srv, NULL, nm_api_server, &api_ctx) != 0) {
            nm_exit(EXIT_FAILURE);
        }
#if defined (NM_OS_LINUX)
        pthread_setname_np(api_srv, "nemu-api");
#endif
    }
#endif /* NM_WITH_REMOTE */

    for (;;) {
        if (nm_mon_rebuild) {
            pthread_mutex_lock(&clean.vms.mtx);
            nm_mon_build_list(&mon_list, &vm_list);
            pthread_mutex_unlock(&clean.vms.mtx);
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
    nm_str_format(&res, "%d\n%s", pid, NM_VERSION);

    if (write(fd, res.data, res.len) < 0) {
        nm_debug("%s: error save pid number\n", __func__);
        rc = NM_ERR;
    }

    close(fd);
    nm_str_free(&res);

    return rc;
}
/* vim:set ts=4 sw=4: */
