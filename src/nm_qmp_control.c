#include <nm_core.h>
#include <nm_string.h>
#include <nm_window.h>
#include <nm_cfg_file.h>

#include <sys/un.h>
#include <sys/socket.h>

#define NM_QMP_CMD_INIT    "{\"execute\":\"qmp_capabilities\"}"
#define NM_QMP_CMD_VM_SHUT "{\"execute\":\"system_powerdown\"}"

#define NM_INIT_QMP { .sd = -1 }

typedef struct {
    int sd;
    struct sockaddr_un sock;
} nm_qmp_handle_t;

static int nm_qmp_init_cmd(nm_qmp_handle_t *h);
static void nm_qmp_sock_path(const nm_str_t *name, nm_str_t *path);
static int nm_qmp_talk(int sd, const char *cmd, size_t len);

void nm_qmp_vm_shut(const nm_str_t *name)
{
    nm_str_t sock_path = NM_INIT_STR;
    nm_qmp_handle_t qmp = NM_INIT_QMP;

    nm_qmp_sock_path(name, &sock_path);

    qmp.sock.sun_family = AF_UNIX;
    strncpy(qmp.sock.sun_path, sock_path.data, sock_path.len);

    if (nm_qmp_init_cmd(&qmp) == NM_ERR)
        goto out;

    nm_qmp_talk(qmp.sd, NM_QMP_CMD_VM_SHUT, strlen(NM_QMP_CMD_VM_SHUT));
    close(qmp.sd);

out:
    nm_str_free(&sock_path);
}

static int nm_qmp_init_cmd(nm_qmp_handle_t *h)
{
    socklen_t len = sizeof(h->sock);

    if ((h->sd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1)
    {
        nm_print_warn(3, 6, _("cannot create socket for QMP connection"));
        return NM_ERR;
    }

    if (connect(h->sd, (struct sockaddr *) &h->sock, len) == -1)
    {
        close(h->sd);
        nm_print_warn(3, 6, _("cannot connect to QMP socket"));
        return NM_ERR;
    }

    return nm_qmp_talk(h->sd, NM_QMP_CMD_INIT, strlen(NM_QMP_CMD_INIT));
}

static int nm_qmp_talk(int sd, const char *cmd, size_t len)
{
    if (write(sd, cmd, len) == -1)
    {
        close(sd);
        nm_print_warn(3, 6, _("error send message to QMP socket"));
        return NM_ERR;
    }

    return NM_OK;
}

static void nm_qmp_sock_path(const nm_str_t *name, nm_str_t *path)
{
    nm_str_alloc_str(path, &nm_cfg_get()->vm_dir);
    nm_str_add_char(path, '/');
    nm_str_add_str(path, name);
    nm_str_add_text(path, "/qmp.sock");
}

/* vim:set ts=4 sw=4 fdm=marker: */
