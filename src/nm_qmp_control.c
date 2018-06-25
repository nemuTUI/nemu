#include <nm_core.h>
#include <nm_utils.h>
#include <nm_string.h>
#include <nm_window.h>
#include <nm_cfg_file.h>
#include <nm_usb_devices.h>

#include <sys/time.h>
#include <sys/un.h>
#include <sys/socket.h>

#define NM_QMP_CMD_INIT     "{\"execute\":\"qmp_capabilities\"}"
#define NM_QMP_CMD_VM_SHUT  "{\"execute\":\"system_powerdown\"}"
#define NM_QMP_CMD_VM_QUIT  "{\"execute\":\"quit\"}"
#define NM_QMP_CMD_VM_RESET "{\"execute\":\"system_reset\"}"
#define NM_QMP_CMD_VM_STOP  "{\"execute\":\"stop\"}"
#define NM_QMP_CMD_VM_CONT  "{\"execute\":\"cont\"}"
#define NM_QMP_CMD_USB_ADD \
    "{\"execute\":\"device_add\",\"arguments\":{\"driver\":\"usb-host\"," \
    "\"hostbus\":\"%u\",\"hostaddr\":\"%u\",\"id\":\"usb-%s-%s-%s\"}}"
#define NM_QMP_CMD_USB_DEL \
    "{\"execute\":\"device_del\",\"arguments\":{\"id\":\"usb-%s-%s-%s\"}}"

#if 0
    /* Get peripheral qmp command example */
    input:

    {"execute": "qom-list", "arguments": { "path": "/machine/peripheral" }}

    output:

    {"return": [{"name": "type", "type": "string"}, {"name": "dev1", "type": "child<usb-host>"},
        {"name": "dev2", "type": "child<usb-host>"}]}
#endif

#define NM_INIT_QMP { .sd = -1 }
#define NM_QMP_READLEN 1024

typedef struct {
    int sd;
    struct sockaddr_un sock;
} nm_qmp_handle_t;

static int nm_qmp_vm_exec(const nm_str_t *name, const char *cmd,
                          struct timeval *tv);
static int nm_qmp_init_cmd(nm_qmp_handle_t *h);
static void nm_qmp_sock_path(const nm_str_t *name, nm_str_t *path);
static int nm_qmp_talk(int sd, const char *cmd,
                       size_t len, struct timeval *tv);
static int nm_qmp_vmsnap(const nm_str_t *name, const nm_str_t *snap,
                         const char *cmd);
static int nm_qmp_check_answer(const nm_str_t *answer);

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

int nm_qmp_drive_snapshot(const nm_str_t *name, const nm_str_t *drive,
                          const nm_str_t *path)
{
    nm_str_t qmp_query = NM_INIT_STR;
    struct timeval tv;
    int rc;

    tv.tv_sec = 10;
    tv.tv_usec = 0; /* 10 s */

    nm_str_format(&qmp_query,
        "{\"execute\":\"blockdev-snapshot-sync\",\"arguments\":{\"device\":\"%s\","
        "\"snapshot-file\":\"%s\",\"format\":\"qcow2\"}}",
        drive->data, path->data);

    nm_debug("exec qmp: %s\n", qmp_query.data);
    rc = nm_qmp_vm_exec(name, qmp_query.data, &tv);

    nm_str_free(&qmp_query);

    return rc;
}

int nm_qmp_savevm(const nm_str_t *name, const nm_str_t *snap)
{
    return nm_qmp_vmsnap(name, snap, "savevm");
}

int nm_qmp_loadvm(const nm_str_t *name, const nm_str_t *snap)
{
    return nm_qmp_vmsnap(name, snap, "loadvm");
}

int nm_qmp_delvm(const nm_str_t *name, const nm_str_t *snap)
{
    return nm_qmp_vmsnap(name, snap, "delvm");
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

static int nm_qmp_vmsnap(const nm_str_t *name, const nm_str_t *snap,
                         const char *cmd)
{
    nm_str_t qmp_query = NM_INIT_STR;
    struct timeval tv;
    int rc;

    /* this operation can take a long time */
    tv.tv_sec = 300;
    tv.tv_usec = 0; /* 5 m */

    nm_str_format(&qmp_query,
        "{\"execute\":\"%s\",\"arguments\":{\"name\":\"%s\"}}",
        cmd, snap->data);

    nm_debug("exec qmp: %s\n", qmp_query.data);
    rc = nm_qmp_vm_exec(name, qmp_query.data, &tv);

    nm_str_free(&qmp_query);

    return rc;
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

static int nm_qmp_init_cmd(nm_qmp_handle_t *h)
{
    socklen_t len = sizeof(h->sock);
    struct timeval tv;

    tv.tv_sec = 0;
    tv.tv_usec = 100000; /* 0.1 s */

    if ((h->sd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1)
    {
        nm_warn(_(NM_MSG_Q_CR_ERR));
        return NM_ERR;
    }

    if (fcntl(h->sd, F_SETFL, O_NONBLOCK) == -1)
    {
        close(h->sd);
        nm_warn(_(NM_MSG_Q_FL_ERR));
        return NM_ERR;
    }

    if (connect(h->sd, (struct sockaddr *) &h->sock, len) == -1)
    {
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

    if (regcomp(&reg, regex, REG_NOSUB | REG_EXTENDED) != 0)
    {
        nm_bug("%s: regcomp failed", __func__);
    }

    if (regexec(&reg, answer->data, 0, NULL, 0) != 0)
    {
        rc = NM_ERR;
    }

    regfree(&reg);

    return rc;
}

static int nm_qmp_talk(int sd, const char *cmd,
                       size_t len, struct timeval *tv)
{
    nm_str_t answer = NM_INIT_STR;
    char buf[NM_QMP_READLEN] = {0};
    ssize_t nread;
    fd_set readset;
    int ret, read_done = 0;
    int rc = NM_OK;

    FD_ZERO(&readset);
    FD_SET(sd, &readset);

    if (write(sd, cmd, len) == -1)
    {
        close(sd);
        nm_warn(_(NM_MSG_Q_SE_ERR));
        return NM_ERR;
    }

    while (!read_done)
    {
        ret = select(sd + 1, &readset, NULL, NULL, tv);
        if (ret == -1)
            nm_bug("%s: select error: %s", __func__, strerror(errno));
        else if (ret && FD_ISSET(sd, &readset)) /* data is available */
        {
            memset(buf, 0, NM_QMP_READLEN);
            nread = read(sd, buf, NM_QMP_READLEN);
            if (nread > 1)
            {
                buf[nread - 2] = '\0';
                nm_str_add_text(&answer, buf);
                /* check for command succesfully executed here
                 * and return if it done */
                if ((rc = nm_qmp_check_answer(&answer)) == NM_OK)
                    goto out;
            }
            else if (nread == 0) /* socket closed */
                read_done = 1;
        }
        else /* timeout, nothing happens */
            read_done = 1;
    }

    if (answer.len == 0)
    {
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

static void nm_qmp_sock_path(const nm_str_t *name, nm_str_t *path)
{
    nm_str_alloc_str(path, &nm_cfg_get()->vm_dir);
    nm_str_add_char(path, '/');
    nm_str_add_str(path, name);
    nm_str_add_text(path, "/qmp.sock");
}

/* vim:set ts=4 sw=4 fdm=marker: */
