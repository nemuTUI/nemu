#include <nm_core.h>
#include <nm_dbus.h>
#include <nm_utils.h>
#include <nm_string.h>
#include <nm_window.h>
#include <nm_cfg_file.h>
#include <nm_usb_devices.h>
#include <nm_qmp_control.h>

#include <sys/socket.h>
#include <sys/un.h>
#include <mqueue.h>

#include <json.h>

enum {
    NM_QMP_STATE_DONE = 0,
    NM_QMP_STATE_MORE,
    NM_QMP_STATE_NEXT,
    NM_QMP_STATE_REPEAT,
    NM_QMP_STATE_UNDEF
};

static const char NM_QMP_CMD_INIT[]     = "{\"execute\":\"qmp_capabilities\"}";
static const char NM_QMP_CMD_VM_SHUT[]  = "{\"execute\":\"system_powerdown\"}";
static const char NM_QMP_CMD_VM_QUIT[]  = "{\"execute\":\"quit\"}";
static const char NM_QMP_CMD_VM_RESET[] = "{\"execute\":\"system_reset\"}";
static const char NM_QMP_CMD_VM_STOP[]  = "{\"execute\":\"stop\"}";
static const char NM_QMP_CMD_VM_CONT[]  = "{\"execute\":\"cont\"}";
static const char NM_QMP_CMD_JOBS[]     = "{\"execute\":\"query-jobs\"}";

static const char NM_QMP_CMD_SAVEVM[]   = \
    "{\"execute\":\"snapshot-save\",\"arguments\":{\"job-id\":" \
    "\"vmsave-%s-%s\",\"tag\":\"%s\",\"vmstate\":\"%s\",\"devices\":[%s]}}";

static const char NM_QMP_CMD_LOADVM[]   = \
    "{\"execute\":\"snapshot-load\",\"arguments\":{\"job-id\":" \
    "\"vmload-%s-%s\",\"tag\":\"%s\",\"vmstate\":\"%s\",\"devices\":[%s]}}";

static const char NM_QMP_CMD_DELVM[]    = \
    "{\"execute\":\"snapshot-delete\",\"arguments\":{\"job-id\":" \
    "\"vmdel-%s-%s\",\"tag\":\"%s\",\"devices\":[%s]}}";

static const char NM_QMP_CMD_USB_ADD[]  = \
    "{\"execute\":\"device_add\",\"arguments\":{\"driver\":\"usb-host\"," \
    "\"hostbus\":\"%u\",\"hostaddr\":\"%u\",\"id\":\"usb-%s-%s-%s\"}}";

static const char NM_QMP_CMD_USB_DEL[]  = \
    "{\"execute\":\"device_del\",\"arguments\":{\"id\":\"usb-%s-%s-%s\"}}";

static const char NM_QMP_NET_TAP_ADD[]  = \
    "{\"execute\":\"netdev_add\",\"arguments\":{\"type\":\"tap\"," \
    "\"ifname\":\"%s\",\"id\":\"net-%s\",\"script\":\"no\"," \
    "\"downscript\":\"no\",\"vhost\":%s}}";

static const char NM_QMP_NET_TAP_FD_ADD[]  = \
    "{\"execute\":\"netdev_add\",\"arguments\":{\"type\":\"tap\"," \
    "\"ifname\":\"%s\",\"id\":\"net-%s\",\"script\":\"no\"," \
    "\"downscript\":\"no\",\"vhost\":%s,\"fd\":%d}}";

static const char NM_QMP_NET_USER_ADD[]  = \
    "{\"execute\":\"netdev_add\",\"arguments\":{\"type\":\"user\"," \
    "\"id\":\"net-%s\"}}";

static const char NM_QMP_NET_USER_FWD_ADD[]  = \
    "{\"execute\":\"netdev_add\",\"arguments\":{\"type\":\"user\"," \
    "\"id\":\"net-%s\",\"hostfwd\":\"%s\"}}";

static const char NM_QMP_NET_USER_SMB_ADD[]  = \
    "{\"execute\":\"netdev_add\",\"arguments\":{\"type\":\"user\"," \
    "\"id\":\"net-%s\",\"smb\":\"%s\"}}";

static const char NM_QMP_NET_USER_FWD_SMB_ADD[]  = \
    "{\"execute\":\"netdev_add\",\"arguments\":{\"type\":\"user\"," \
    "\"id\":\"net-%s\",\"hostfwd\":\"%s\",\"smb\":\"%s\"}}";

static const char NM_QMP_NET_DEL[]  = \
    "{\"execute\":\"netdev_del\",\"arguments\":{\"id\":\"net-%s\"}}";

static const char NM_QMP_DEV_NET_ADD[]  = \
    "{\"execute\":\"device_add\",\"arguments\":{\"driver\":\"%s\"," \
    "\"id\":\"dev-%s\",\"netdev\":\"net-%s\",\"mac\":\"%s\"}}";

static const char NM_QMP_DEV_DEL[]  = \
    "{\"execute\":\"device_del\",\"arguments\":{\"id\":\"dev-%s\"}}";

enum {NM_QMP_READLEN = 1024};

typedef struct {
    int sd;
    struct sockaddr_un sock;
} nm_qmp_handle_t;

#define NM_INIT_QMP (nm_qmp_handle_t) { .sd = -1 }

static int nm_qmp_vm_exec(const nm_str_t *name, const char *cmd,
                          struct timeval *tv);
static int nm_qmp_init_cmd(nm_qmp_handle_t *h);
static void nm_qmp_sock_path(const nm_str_t *name, nm_str_t *path);
static int nm_qmp_talk(int sd, const char *cmd,
                       size_t len, struct timeval *tv);
static void nm_qmp_talk_async(int sd, const char *cmd,
        size_t len, const char *jobid);
static int nm_qmp_send(const nm_str_t *cmd);
static int nm_qmp_check_answer(const nm_str_t *answer);
static int nm_qmp_parse(const char *jobid, const nm_str_t *answer);
static int nm_qmp_check_job(const char *jobid, const nm_str_t *answer);

void nm_qmp_vm_shut(const nm_str_t *name)
{
    struct timeval tv = { .tv_sec = 0, .tv_usec = 100000 }; /* 0.1s */

    nm_qmp_vm_exec(name, NM_QMP_CMD_VM_SHUT, &tv);
}

void nm_qmp_vm_stop(const nm_str_t *name)
{
    struct timeval tv = { .tv_sec = 0, .tv_usec = 100000 }; /* 0.1s */

    nm_qmp_vm_exec(name, NM_QMP_CMD_VM_QUIT, &tv);
}

void nm_qmp_vm_reset(const nm_str_t *name)
{
    struct timeval tv = { .tv_sec = 0, .tv_usec = 100000 }; /* 0.1s */

    nm_qmp_vm_exec(name, NM_QMP_CMD_VM_RESET, &tv);
}

void nm_qmp_vm_pause(const nm_str_t *name)
{
    struct timeval tv = { .tv_sec = 0, .tv_usec = 1000000 }; /* 1s */

    nm_qmp_vm_exec(name, NM_QMP_CMD_VM_STOP, &tv);
}

void nm_qmp_vm_resume(const nm_str_t *name)
{
    struct timeval tv = { .tv_sec = 0, .tv_usec = 1000000 }; /* 1s */

    nm_qmp_vm_exec(name, NM_QMP_CMD_VM_CONT, &tv);
}

int nm_qmp_savevm(const nm_str_t *name, const nm_str_t *snap)
{
    nm_vect_t drives = NM_INIT_VECT;
    nm_str_t query = NM_INIT_STR;
    nm_str_t devs = NM_INIT_STR;
    nm_str_t uid = NM_INIT_STR;
    nm_str_t cmd = NM_INIT_STR;
    size_t drives_count;
    int rc;

    nm_str_format(&query, NM_VM_GET_DRIVES_SQL, name->data);
    nm_db_select(query.data, &drives);
    drives_count = drives.n_memb / NM_DRV_IDX_COUNT;

    for (size_t n = 0; n < drives_count; n++) {
        nm_str_append_format(&devs, "%s\"hd%zu\"", (!n) ? "" : ", ", n);
    }

    nm_gen_uid(&uid);

    nm_str_format(&cmd, NM_QMP_CMD_SAVEVM, name->data, uid.data, snap->data,
            "hd0", devs.data);
    rc = nm_qmp_send(&cmd);

    nm_vect_free(&drives, nm_str_vect_free_cb);
    nm_str_free(&devs);
    nm_str_free(&cmd);
    nm_str_free(&query);
    nm_str_free(&uid);

    return rc;
}

int nm_qmp_loadvm(const nm_str_t *name, const nm_str_t *snap)
{
    nm_vect_t drives = NM_INIT_VECT;
    nm_str_t query = NM_INIT_STR;
    nm_str_t devs = NM_INIT_STR;
    nm_str_t uid = NM_INIT_STR;
    nm_str_t cmd = NM_INIT_STR;
    size_t drives_count;
    int rc;

    nm_str_format(&query, NM_VM_GET_DRIVES_SQL, name->data);
    nm_db_select(query.data, &drives);
    drives_count = drives.n_memb / NM_DRV_IDX_COUNT;

    for (size_t n = 0; n < drives_count; n++) {
        nm_str_append_format(&devs, "%s\"hd%zu\"", (!n) ? "" : ", ", n);
    }

    nm_gen_uid(&uid);

    nm_str_format(&cmd, NM_QMP_CMD_LOADVM, name->data, uid.data, snap->data,
            "hd0", devs.data);
    rc = nm_qmp_send(&cmd);

    nm_vect_free(&drives, nm_str_vect_free_cb);
    nm_str_free(&devs);
    nm_str_free(&cmd);
    nm_str_free(&query);
    nm_str_free(&uid);

    return rc;
}

int nm_qmp_delvm(const nm_str_t *name, const nm_str_t *snap)
{
    nm_vect_t drives = NM_INIT_VECT;
    nm_str_t query = NM_INIT_STR;
    nm_str_t devs = NM_INIT_STR;
    nm_str_t uid = NM_INIT_STR;
    nm_str_t cmd = NM_INIT_STR;
    size_t drives_count;
    int rc;

    nm_str_format(&query, NM_VM_GET_DRIVES_SQL, name->data);
    nm_db_select(query.data, &drives);
    drives_count = drives.n_memb / NM_DRV_IDX_COUNT;

    for (size_t n = 0; n < drives_count; n++) {
        nm_str_append_format(&devs, "%s\"hd%zu\"", (!n) ? "" : ", ", n);
    }

    nm_gen_uid(&uid);

    nm_str_format(&cmd, NM_QMP_CMD_DELVM, name->data, uid.data,
            snap->data, devs.data);
    rc = nm_qmp_send(&cmd);

    nm_vect_free(&drives, nm_str_vect_free_cb);
    nm_str_free(&devs);
    nm_str_free(&cmd);
    nm_str_free(&query);
    nm_str_free(&uid);

    return rc;
}

int nm_qmp_usb_attach(const nm_str_t *name, const nm_usb_data_t *usb)
{
    nm_str_t qmp_query = NM_INIT_STR;
    int rc;

    struct timeval tv = { .tv_sec = 0, .tv_usec = 5000000 }; /* 5s */

    nm_str_format(&qmp_query, NM_QMP_CMD_USB_ADD,
                  usb->dev->bus_num, usb->dev->dev_addr,
                  usb->dev->vendor_id.data, usb->dev->product_id.data,
                  (usb->serial.len) ? usb->serial.data : "NULL");

    nm_debug("exec qmp: %s\n", qmp_query.data);
    rc = nm_qmp_vm_exec(name, qmp_query.data, &tv);

    nm_str_free(&qmp_query);

    return rc;
}

int nm_qmp_usb_detach(const nm_str_t *name, const nm_usb_data_t *usb)
{
    nm_str_t qmp_query = NM_INIT_STR;
    int rc;

    struct timeval tv = { .tv_sec = 0, .tv_usec = 1000000 }; /* 1s */

    nm_str_format(&qmp_query, NM_QMP_CMD_USB_DEL,
                  usb->dev->vendor_id.data,
                  usb->dev->product_id.data,
                  (usb->serial.len) ? usb->serial.data : "NULL");

    nm_debug("exec qmp: %s\n", qmp_query.data);
    rc = nm_qmp_vm_exec(name, qmp_query.data, &tv);

    nm_str_free(&qmp_query);

    return rc;
}

int nm_qmp_nic_attach(const nm_str_t *name, const nm_iface_t *nic)
{
    nm_str_t qmp_query = NM_INIT_STR;
    nm_str_t id = NM_INIT_STR;
    int rc;

    nm_str_copy(&id, &nic->maddr);
    nm_str_remove_char(&id, ':');

    struct timeval tv = { .tv_sec = 0, .tv_usec = 5000000 }; /* 5s */

    if (nm_str_cmp_st(&nic->netuser, "yes") == NM_OK) {
        if (nic->hostfwd.len && nic->smb.len) {
            nm_str_format(&qmp_query, NM_QMP_NET_USER_FWD_SMB_ADD,
                    id.data, nic->hostfwd.data, nic->smb.data);
        } else if (nic->hostfwd.len) {
            nm_str_format(&qmp_query, NM_QMP_NET_USER_FWD_ADD,
                    id.data, nic->hostfwd.data);
        } else if (nic->smb.len) {
            nm_str_format(&qmp_query, NM_QMP_NET_USER_SMB_ADD,
                    id.data, nic->smb.data);
        } else {
            nm_str_format(&qmp_query, NM_QMP_NET_USER_ADD, id.data);
        }
    } else {
#if defined (NM_OS_LINUX)
        if (nic->tap_fd == -1) {
            nm_str_format(&qmp_query, NM_QMP_NET_TAP_ADD,
                    nic->name.data, id.data,
                    (nm_str_cmp_st(&nic->vhost, "yes") == NM_OK) ?
                    "true" : "false");
        } else {
            nm_str_format(&qmp_query, NM_QMP_NET_TAP_FD_ADD,
                    nic->name.data, id.data,
                    (nm_str_cmp_st(&nic->vhost, "yes") == NM_OK) ?
                    "true" : "false", nic->tap_fd);
        }
    }
#else
    nm_str_format(&qmp_query, NM_QMP_NET_TAP_ADD,
            nic->name.data, id.data, "false");
#endif /* NM_OS_LINUX */

    nm_debug("exec qmp: %s\n", qmp_query.data);
    rc = nm_qmp_vm_exec(name, qmp_query.data, &tv);
    if (rc != NM_OK) {
        goto out;
    }

    nm_str_format(&qmp_query, NM_QMP_DEV_NET_ADD,
                  nic->drv.data, id.data, id.data, nic->maddr.data);

    nm_debug("exec qmp: %s\n", qmp_query.data);
    rc = nm_qmp_vm_exec(name, qmp_query.data, &tv);

out:
    nm_str_free(&qmp_query);
    nm_str_free(&id);

    return rc;
}

int nm_qmp_nic_detach(const nm_str_t *name, const nm_iface_t *nic)
{
    nm_str_t qmp_query = NM_INIT_STR;
    nm_str_t id = NM_INIT_STR;
    int rc;

    nm_str_copy(&id, &nic->maddr);
    nm_str_remove_char(&id, ':');

    struct timeval tv = { .tv_sec = 0, .tv_usec = 5000000 }; /* 5s */
    nm_str_format(&qmp_query, NM_QMP_DEV_DEL, id.data);

    nm_debug("exec qmp: %s\n", qmp_query.data);
    rc = nm_qmp_vm_exec(name, qmp_query.data, &tv);
    if (rc != NM_OK) {
        goto out;
    }

    nm_str_format(&qmp_query, NM_QMP_NET_DEL, id.data);
    nm_debug("exec qmp: %s\n", qmp_query.data);
    rc = nm_qmp_vm_exec(name, qmp_query.data, &tv);
out:
    nm_str_free(&qmp_query);
    nm_str_free(&id);
    return rc;
}

int nm_qmp_test_socket(const nm_str_t *name)
{
    int rc = NM_ERR;
    nm_str_t sock_path = NM_INIT_STR;
    nm_qmp_handle_t qmp = NM_INIT_QMP;

    nm_qmp_sock_path(name, &sock_path);

    qmp.sock.sun_family = AF_UNIX;
    nm_strlcpy(qmp.sock.sun_path, sock_path.data, sizeof(qmp.sock.sun_path));

    if ((qmp.sd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1)
        goto out;

    if (fcntl(qmp.sd, F_SETFL, O_NONBLOCK) == -1) {
        close(qmp.sd);
        goto out;
    }

    if (connect(qmp.sd, (struct sockaddr *) &qmp.sock, sizeof(qmp.sock)) != 0) {
        if (errno == EAGAIN) {
            rc = NM_OK;
        }
    } else {
        rc = NM_OK;
    }

    close(qmp.sd);
out:
    nm_str_free(&sock_path);

    return rc;
}

static int nm_qmp_send(const nm_str_t *cmd)
{
    mqd_t mq;

    if ((mq = mq_open(NM_MQ_PATH, O_WRONLY | O_CREAT | O_NONBLOCK,
                    0600, NULL)) == (mqd_t) -1) {
        return NM_ERR;
    }

    if (mq_send(mq, cmd->data, cmd->len, 0) == EAGAIN) {
        return NM_ERR;
    }

    mq_close(mq);

    return NM_OK;
}

static int nm_qmp_vm_exec(const nm_str_t *name, const char *cmd,
                          struct timeval *tv)
{
    nm_str_t sock_path = NM_INIT_STR;
    nm_qmp_handle_t qmp = NM_INIT_QMP;
    int rc = NM_ERR;

    nm_qmp_sock_path(name, &sock_path);

    qmp.sock.sun_family = AF_UNIX;
    nm_strlcpy(qmp.sock.sun_path, sock_path.data, sizeof(qmp.sock.sun_path));

    if (nm_qmp_init_cmd(&qmp) == NM_ERR)
        goto out;

    rc = nm_qmp_talk(qmp.sd, cmd, strlen(cmd), tv);
    close(qmp.sd);

out:
    nm_str_free(&sock_path);
    return rc;
}

void nm_qmp_vm_exec_async(const nm_str_t *name, const char *cmd,
        const char *jobid)
{
    nm_str_t sock_path = NM_INIT_STR;
    nm_qmp_handle_t qmp = NM_INIT_QMP;

    nm_qmp_sock_path(name, &sock_path);

    qmp.sock.sun_family = AF_UNIX;
    nm_strlcpy(qmp.sock.sun_path, sock_path.data, sizeof(qmp.sock.sun_path));

    if (nm_qmp_init_cmd(&qmp) == NM_ERR)
        goto out;

    nm_qmp_talk_async(qmp.sd, cmd, strlen(cmd), jobid);
    close(qmp.sd);

out:
    nm_str_free(&sock_path);
}

static int nm_qmp_init_cmd(nm_qmp_handle_t *h)
{
    socklen_t len = sizeof(h->sock);
    struct timeval tv;

    tv.tv_sec = 0;
    tv.tv_usec = 100000; /* 0.1 s */

    if ((h->sd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
        nm_warn(_(NM_MSG_Q_CR_ERR));
        return NM_ERR;
    }

    if (fcntl(h->sd, F_SETFL, O_NONBLOCK) == -1) {
        close(h->sd);
        nm_warn(_(NM_MSG_Q_FL_ERR));
        return NM_ERR;
    }

    if (connect(h->sd, (struct sockaddr *) &h->sock, len) == -1) {
        close(h->sd);
        nm_warn(_(NM_MSG_Q_CN_ERR));
        return NM_ERR;
    }

    return nm_qmp_talk(h->sd, NM_QMP_CMD_INIT, strlen(NM_QMP_CMD_INIT), &tv);
}

static int nm_qmp_check_answer(const nm_str_t *answer)
{
    /* {"return": {}} from answer means OK
     * TODO: use JSON parser instead, e.g: json-c */
    const char *regex = ".*\\{\"return\":[[:space:]]\\{\\}\\}.*";
    regex_t reg;
    int rc = NM_OK;

    if (regcomp(&reg, regex, REG_NOSUB | REG_EXTENDED) != 0) {
        nm_bug("%s: regcomp failed", __func__);
    }

    if (regexec(&reg, answer->data, 0, NULL, 0) != 0) {
        rc = NM_ERR;
    }

    regfree(&reg);

    return rc;
}

static int nm_qmp_parse(const char *jobid, const nm_str_t *answer)
{
    int state = NM_QMP_STATE_UNDEF;
    nm_vect_t json = NM_INIT_VECT;
    char *saveptr;
    char *token;

    saveptr = answer->data;

    while ((token = strtok_r(saveptr, "\n", &saveptr))) {
        nm_str_t js = NM_INIT_STR;
        nm_str_format(&js, "%s", token);
        nm_vect_insert(&json, &js, sizeof(nm_str_t), nm_str_vect_ins_cb);
        nm_str_free(&js);
    }

    if (json.n_memb == 1) {
        nm_debug("%s: single json\n", __func__);
        state = nm_qmp_check_job(jobid, nm_vect_at(&json, 0));
        goto out;
    }

    nm_debug("%s: multiple json: %zu\n", __func__, json.n_memb);
    for (size_t n = 0; n < json.n_memb; n++) {
        if ((state = nm_qmp_check_job(jobid, nm_vect_at(&json, n)))
                == NM_QMP_STATE_DONE) {
            break;
        }
    }

out:
    nm_vect_free(&json, nm_str_vect_free_cb);

    return state;
}

static int nm_qmp_check_job(const char *jobid, const nm_str_t *answer)
{
    struct json_object *parsed, *ret, *job, *id, *err, *status;
    int state = NM_QMP_STATE_DONE;
    nm_str_t body = NM_INIT_STR;
    int jobs = 0;

    nm_debug("%s: checking job: %s\n", __func__, jobid);
    nm_debug("%s: answer: %s\n", __func__, answer->data);

    parsed = json_tokener_parse(answer->data);
    if (!parsed) {
        nm_debug("%s: [more] need more data\n", __func__);
        state = NM_QMP_STATE_MORE;
        goto out;
    }

    json_object_object_get_ex(parsed, "return", &ret);
    if (ret == NULL) {
        nm_debug("%s: [next] need more data\n", __func__);
        state = NM_QMP_STATE_NEXT;
        goto out;
    }

    /* skip {"return": {}} */
    if (json_object_get_type(ret) != json_type_array) {
        nm_debug("%s: [ret next] need more data\n", __func__);
        state = NM_QMP_STATE_NEXT;
        goto out;
    }

    jobs = json_object_array_length(ret);
    if (!jobs) {
        nm_debug("%s: [ret next] need more data\n", __func__);
        state = NM_QMP_STATE_REPEAT;
        goto out;
    }

    nm_debug("%s: got %d jobs\n", __func__, jobs);
    for (int i = 0; i < jobs; i++) {
        const char *id_str;

        job = json_object_array_get_idx(ret, i);
        json_object_object_get_ex(job, "id", &id);
        id_str = json_object_get_string(id);

        if (nm_str_cmp_tt(id_str, jobid) == NM_OK) {
            const char *status_str;
            nm_debug("%s: job found: %s, checking status\n", __func__, id_str);

            json_object_object_get_ex(job, "status", &status);
            status_str = json_object_get_string(status);
            if (nm_str_cmp_tt(status_str, "concluded") != NM_OK) {
                nm_debug("%s: job %s is not finished yet\n", __func__, id_str);
                state = NM_QMP_STATE_REPEAT;
                break;
            }

            json_object_object_get_ex(job, "error", &err);
            if (err) { /* job executed with errors */
                const char *err_str;
                err_str = json_object_get_string(err);
                nm_debug("%s: job %s executed with error: %s\n",
                        __func__, id_str, err_str);
#if defined (NM_WITH_DBUS)
                nm_str_format(&body, "%s - %s", id_str, err_str);
                nm_dbus_send_notify("Job finished with error:", body.data);
#endif
                break;
            }

            /*job finished successfully */
            nm_debug("%s: job %s executed successfully\n", __func__, id_str);
#if defined (NM_WITH_DBUS)
            nm_str_format(&body, "%s", id_str);
            nm_dbus_send_notify("Job finished successfully:", body.data);
#endif
            break;
        }
    }
out:
    json_object_put(parsed);
    nm_str_free(&body);

    return state;
}

static int nm_qmp_talk(int sd, const char *cmd,
                       size_t len, struct timeval *tv)
{
    nm_str_t answer = NM_INIT_STR;
    char buf[NM_QMP_READLEN];
    ssize_t nread;
    fd_set readset;
    int ret, read_done = 0;
    int rc = NM_OK;

    FD_ZERO(&readset);
    FD_SET(sd, &readset);

    if (write(sd, cmd, len) == -1) {
        close(sd);
        nm_warn(_(NM_MSG_Q_SE_ERR));
        return NM_ERR;
    }

    while (!read_done) {
        ret = select(sd + 1, &readset, NULL, NULL, tv);
        if (ret == -1) {
            nm_bug("%s: select error: %s", __func__, strerror(errno));
        } else if (ret && FD_ISSET(sd, &readset)) { /* data is available */
            memset(buf, 0, NM_QMP_READLEN);
            nread = read(sd, buf, NM_QMP_READLEN);
            if (nread > 1) {
                buf[nread - 2] = '\0';
                nm_str_add_text(&answer, buf);
                /* check for command successfully executed here
                 * and return if it done */
                if ((rc = nm_qmp_check_answer(&answer)) == NM_OK)
                    goto out;
            } else if (nread == 0) { /* socket closed */
                read_done = 1;
            }
        } else { /* timeout, nothing happens */
            read_done = 1;
        }
    }

    if (answer.len == 0) {
        nm_warn(_(NM_MSG_Q_NO_ANS));
        rc = NM_ERR;
        goto err;
    }

out:
    nm_debug("QMP: %s\n", answer.data);
    if (rc != NM_OK)
        nm_warn(_(NM_MSG_Q_EXEC_E));
err:
    nm_str_free(&answer);

    return rc;
}

static void nm_qmp_talk_async(int sd, const char *cmd,
                       size_t len, const char *jobid)
{
    char buf[NM_QMP_READLEN + 1] = {0};
    nm_str_t answer = NM_INIT_STR;
    int ret, read_done = 0;
    struct timeval tv;
    int state = NM_QMP_STATE_UNDEF;
    fd_set readset;
    ssize_t nread;

    tv.tv_sec = 300;
    tv.tv_usec = 0;

    FD_ZERO(&readset);
    FD_SET(sd, &readset);

    if (write(sd, cmd, len) == -1) {
        close(sd);
        //nm_warn(_(NM_MSG_Q_SE_ERR));
        return;
    }

    if (write(sd, NM_QMP_CMD_JOBS, sizeof(NM_QMP_CMD_JOBS) - 1) == -1) {
        close(sd);
        //nm_warn(_(NM_MSG_Q_SE_ERR));
        return;
    }

    while (!read_done) {
        /* query jobs */
        if (state == NM_QMP_STATE_REPEAT) {
            if (write(sd, NM_QMP_CMD_JOBS, sizeof(NM_QMP_CMD_JOBS) - 1) == -1) {
                close(sd);
                //nm_warn(_(NM_MSG_Q_SE_ERR));
                return;
            }
            state = NM_QMP_STATE_UNDEF;
        }

        ret = select(sd + 1, &readset, NULL, NULL, &tv);
        if (ret == -1) {
            nm_bug("%s: select error: %s", __func__, strerror(errno));
        } else if (ret && FD_ISSET(sd, &readset)) { /* data is available */
            memset(buf, 0, NM_QMP_READLEN);
            nread = read(sd, buf, NM_QMP_READLEN);
            if (nread > 1) {
                if (buf[nread - 2] == '\r') {
                    buf[nread - 2] = '\0';
                }
                if (state == NM_QMP_STATE_MORE) {
                    nm_str_add_text(&answer, buf);
                } else {
                    nm_str_format(&answer, "%s", buf);
                }
                /* check for job successfully finished
                 * and return if it done */
                if ((state = nm_qmp_parse(jobid, &answer)) == NM_QMP_STATE_DONE)
                    goto out;
            } else if (nread == 0) { /* socket closed */
                read_done = 1;
            }
        } else { /* timeout, nothing happens */
            read_done = 1;
        }
    }

    if (answer.len == 0) {
        //nm_warn(_(NM_MSG_Q_NO_ANS));
    }

out:
    nm_str_free(&answer);
}

static void nm_qmp_sock_path(const nm_str_t *name, nm_str_t *path)
{
    nm_str_format(path, "%s/%s/qmp.sock",
        nm_cfg_get()->vm_dir.data, name->data);
}

/* vim:set ts=4 sw=4: */
