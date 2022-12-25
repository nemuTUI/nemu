#include <nm_core.h>

#if defined(NM_OS_DARWIN)
#include <sys/ipc.h>
#include <sys/msg.h>

#define NM_SYSV_MAX_MSGSIZE 2048
#define NM_SYSV_QUEUE_PATH  "/tmp/nemu_sysv_queue"
#define NM_SYSV_PROJ_ID     'N'
#define NM_SYSV_MSG_TYPE    1

typedef struct {
    long mtype;
    char mtext[NM_SYSV_MAX_MSGSIZE];
} nm_sysv_msg_t;

static int nm_sysv_create_id_file(void)
{
    struct stat info;

    memset(&info, 0, sizeof(info));
    if (stat(NM_SYSV_QUEUE_PATH, &info) == 0) {
        return NM_OK;
    }

    if (open(NM_SYSV_QUEUE_PATH, O_CREAT, S_IRUSR) == -1) {
        nm_debug("%s: cannot create %s: %s\n",
                __func__, NM_SYSV_QUEUE_PATH, strerror(errno));
        return NM_ERR;
    }

    return NM_OK;
}

static int nm_sysv_queue_open(void)
{
    key_t key;
    int queue_id = -1;
    struct msqid_ds id;

    if (nm_sysv_create_id_file() == NM_ERR) {
        goto out;
    }

    if ((key = ftok(NM_SYSV_QUEUE_PATH, NM_SYSV_PROJ_ID)) == -1) {
        nm_debug("%s: ftok: %s\n", __func__, strerror(errno));
        goto out;
    }

    queue_id = msgget(key, IPC_CREAT | 0600);
    if (msgctl(queue_id, IPC_STAT, &id) == -1) {
        nm_debug("%s: msgctl: %s\n", __func__, strerror(errno));
    }

out:
    return queue_id;
}

bool nm_sysv_queue_send(const nm_str_t *msg)
{
    int queue_id;
    nm_sysv_msg_t qmsg;

    memset(&qmsg, 0, sizeof(qmsg));

    if ((queue_id = nm_sysv_queue_open()) == -1) {
        nm_debug("%s: %s\n", __func__, strerror(errno));
        return false;
    }

    if (msg->len > NM_SYSV_MAX_MSGSIZE) {
        return false;
    }

    qmsg.mtype = NM_SYSV_MSG_TYPE;
    memcpy(qmsg.mtext, msg->data, msg->len);

    if (msgsnd(queue_id, &qmsg, sizeof(qmsg.mtext), IPC_NOWAIT) == -1) {
        nm_debug("%s: cannot send msg: %s\n", __func__, strerror(errno));
        return false;
    }

    return true;
}

bool nm_sysv_queue_recv(nm_str_t *msg)
{
    int queue_id;
    nm_sysv_msg_t qmsg;

    memset(&qmsg, 0, sizeof(qmsg));

    if ((queue_id = nm_sysv_queue_open()) == -1) {
        nm_debug("%s: %s\n", __func__, strerror(errno));
        return false;
    }

    if (msgrcv(queue_id, &qmsg, sizeof(qmsg.mtext),
                NM_SYSV_MSG_TYPE, IPC_NOWAIT) == -1) {
        if (errno != ENOMSG) {
            nm_debug("%s: %s\n", __func__, strerror(errno));
        }
        return false;
    }

    nm_str_format(msg, "%s", qmsg.mtext);

    return true;
}

#endif /* NM_OS_DARWIN */

/* vim:set ts=4 sw=4: */
