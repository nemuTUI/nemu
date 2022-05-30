#include <nm_core.h>
#include <nm_utils.h>
#include <nm_string.h>
#include <nm_vector.h>
#include <nm_ncurses.h>
#include <nm_vm_control.h>
#include <nm_ftw.h>

#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <limits.h>
#include <libgen.h>
#include <string.h>
#include <errno.h>
#include <time.h>

enum {
    NM_BLKSIZE      = 131072, /* 128KiB */
    NM_SOCK_READLEN = 1024,
};

static int nemu_rc;
static char nemu_path[PATH_MAX];

#if defined (NM_OS_LINUX) && defined (NM_WITH_SENDFILE)
#include <sys/sendfile.h>
#endif

#if defined (NM_OS_LINUX) && defined (NM_WITH_SENDFILE)
static void nm_copy_file_sendfile(int in_fd, int out_fd);
#else
static void nm_copy_file_default(int in_fd, int out_fd);
#endif

void nm_bug(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);

    nm_curses_deinit();

    vfprintf(stderr, fmt, args);
    putc('\n', stderr);
    va_end(args);

    nm_exit(NM_ERR);
}

void *nm_alloc(size_t size)
{
    void *p;

    if ((p = malloc(size)) == NULL)
        nm_bug("malloc: %s\n", strerror(errno));

    return p;
}

void *nm_calloc(size_t nmemb, size_t size)
{
    void *p;

    if ((p = calloc(nmemb, size)) == NULL)
        nm_bug("cmalloc: %s\n", strerror(errno));

    return p;
}

void *nm_realloc(void *p, size_t size)
{
    void *p_new;

    if ((p_new = realloc(p, size)) == NULL)
        nm_bug("realloc: %s\n", strerror(errno));

    return p_new;
}

void nm_map_file(nm_file_map_t *file)
{
    struct stat stat;

    if ((file->fd = open(file->name->data, O_RDONLY)) == -1) {
        nm_bug(_("Cannot open file %s:%s"),
            file->name->data, strerror(errno));
    }

    if (fstat(file->fd, &stat) == -1) {
        close(file->fd);
        nm_bug(_("Cannot get file info %s:%s"),
            file->name->data, strerror(errno));
    }

    file->size = stat.st_size;
    file->mp = mmap(0, file->size, PROT_READ, MAP_PRIVATE, file->fd, 0);

    if (file->mp == MAP_FAILED) {
        close(file->fd);
        nm_bug(_("%s: cannot map file %s:%s"),
            __func__, file->name->data, strerror(errno));
    }
}

void nm_unmap_file(const nm_file_map_t *file)
{
    munmap(file->mp, file->size);
    close(file->fd);
}

void nm_copy_file(const nm_str_t *src, const nm_str_t *dst)
{
    int in_fd, out_fd;

    if ((in_fd = open(src->data, O_RDONLY)) == -1) {
        nm_bug("%s: cannot open file %s: %s",
            __func__, src->data, strerror(errno));
    }

    if ((out_fd = open(dst->data, O_WRONLY | O_CREAT | O_EXCL, 0644)) == -1) {
        close(in_fd);
        nm_bug("%s: cannot open file %s: %s",
            __func__, dst->data, strerror(errno));
    }

#if defined (NM_OS_LINUX) && defined (NM_WITH_SENDFILE)
    nm_copy_file_sendfile(in_fd, out_fd);
#else
    nm_copy_file_default(in_fd, out_fd);
#endif

    close(in_fd);
    close(out_fd);
}

#if defined (NM_OS_LINUX) && defined (NM_WITH_SENDFILE)
static void nm_copy_file_sendfile(int in_fd, int out_fd)
{
    off_t offset = 0;
    struct stat file_info;

    memset(&file_info, 0, sizeof(file_info));

    if (fstat(in_fd, &file_info) != 0)
        nm_bug("%s: cannot get file info %d: %s", __func__, in_fd, strerror(errno));

    while (offset < file_info.st_size) {
        int rc;

        if ((rc = sendfile(out_fd, in_fd, &offset, file_info.st_size)) == -1)
            nm_bug("%s: cannot copy file: %s", __func__, strerror(errno));

        if (rc == 0)
            break;
    }

    if (offset != file_info.st_size)
            nm_bug("%s: incomplete transfer from sendfile", __func__);
}
#else
static void nm_copy_file_default(int in_fd, int out_fd)
{
    char *buf = nm_alloc(NM_BLKSIZE);
    ssize_t nread;

    posix_fadvise(in_fd, 0, 0, POSIX_FADV_SEQUENTIAL);

    while ((nread = read(in_fd, buf, NM_BLKSIZE)) > 0) {
        char *bufsp = buf;

        do {
            ssize_t nwrite = write(out_fd, bufsp, NM_BLKSIZE);

            if (nwrite >= 0) {
                nread -= nwrite;
                bufsp += nwrite;
            } else if (errno != EINTR) {
                nm_bug("%s: copy file failed: %s", __func__, strerror(errno));
            }

        } while (nread > 0);
    }

    if (nread != 0)
        nm_bug("%s: copy was not complete.", __func__);

    free(buf);
}
#endif

int nm_spawn_process(const nm_vect_t *argv, nm_str_t *answer)
{
    int rc = NM_OK;
    int fd[2];
    pid_t child_pid = 0;

    if (socketpair(AF_UNIX, SOCK_STREAM, 0, fd) == -1)
        nm_bug("%s: error create socketpair: %s", __func__, strerror(errno));

    switch (child_pid = fork()) {
    case (-1):  /* error*/
        nm_bug("%s: fork: %s", __func__, strerror(errno));
        break;

    case (0):   /* child */
        close(fd[0]);
        dup2(fd[1], STDOUT_FILENO);
        dup2(fd[1], STDERR_FILENO);

        execvp(((char *const *) argv->data)[0], (char *const *) argv->data);
        nm_bug("%s: unreachable reached", __func__);
        break;

    default:    /* parent */
        {
            int wstatus = 0;
            pid_t w_rc;
            char buf[NM_SOCK_READLEN] = {0};

            close(fd[1]);
            w_rc = waitpid(child_pid, &wstatus, 0);

            if ((w_rc == child_pid) && (WEXITSTATUS(wstatus) != 0)) {
                nm_str_t err_msg = NM_INIT_STR;

                while (read(fd[0], buf, sizeof(buf) - 1) > 0) {
                    nm_str_add_text(&err_msg, buf);
                    memset(&buf, 0, sizeof(buf));
                }
                nm_vmctl_log_last(&err_msg);
                nm_debug("exec_error: %s", err_msg.data);
                nm_str_free(&err_msg);
                rc = NM_ERR;
            } else if (answer && (w_rc == child_pid) && (WEXITSTATUS(wstatus) == 0)) {
                while (read(fd[0], buf, sizeof(buf) - 1) > 0) {
                    nm_str_add_text(answer, buf);
                    memset(buf, 0, sizeof(buf));
                }
            }

            close(fd[0]);
        }
    }

    return rc;
}

void nm_debug(const char *fmt, ...)
{
    const nm_cfg_t *cfg = nm_cfg_get();
    if(!cfg->debug)
        return;

    va_list args;
    FILE *fp;

    if ((fp = fopen(cfg->debug_path.data, "a+")) == NULL)
        return;

    va_start(args, fmt);
    vfprintf(fp, fmt, args);
    va_end(args);

    fclose(fp);
}

void nm_cmd_str(nm_str_t *str, const nm_vect_t *argv)
{
    if (str->len > 0)
        nm_str_trunc(str, 0);

    for (size_t m = 0; m < argv->n_memb; m++) {
        nm_str_append_format(str, "%s ", (char *)nm_vect_at(argv, m));
    }
}

/* SMP format: sockets:cores?:threads?
 * if only one value is specified we assume that
 * N processors with one core are used */
void nm_parse_smp(nm_cpu_t *cpu, const char *src)
{
    nm_str_t buf = NM_INIT_STR;
    char *column;
    char *saveptr;
    size_t ncol = 0;

    nm_str_format(&buf, "%s", src);
    saveptr = buf.data;

    while ((column = strtok_r(saveptr, ":", &saveptr))) {
        switch (ncol) {
        case 0: /* sockets */
            cpu->sockets = nm_str_ttoul(column, 10);
            break;
        case 1: /* cores */
            cpu->cores = nm_str_ttoul(column, 10);
            break;
        case 2: /* threads */
            cpu->threads = nm_str_ttoul(column, 10);
            break;
        }
        ncol++;
    }

    if (ncol == 1) {
        cpu->smp = cpu->sockets;
        cpu->sockets = 0;
    } else {
        cpu->smp = cpu->sockets * cpu->cores * ((cpu->threads) ? cpu->threads : 1);
    }

    nm_str_free(&buf);
}

void nm_exit(int status)
{
    if(nm_db_in_transaction())
        nm_db_rollback();

    nemu_rc = status;
    exit(status);
}

int nm_rc()
{
    return nemu_rc;
}

static int nm_ftw_remove_cb(const char *path, NM_UNUSED const struct stat *st,
        NM_UNUSED enum nm_ftw_type type, nm_ftw_t *ftw, NM_UNUSED void *ctx)
{
    if (ftw->level != 0) {
        if (remove(path) != 0) {
            nm_bug(_("%s: %s: %s"), __func__, path, strerror(errno));
        }
    }

    return NM_OK;
}

int nm_cleanup_dir(const nm_str_t *path)
{
    return nm_ftw(path, nm_ftw_remove_cb, NULL,
            NM_FTW_DEPTH_UNLIM, NM_FTW_DNFSL | NM_FTW_MOUNT | NM_FTW_DEPTH);
}

int nm_mkdir_parent(const nm_str_t *path, mode_t mode)
{
    int rc = NM_OK;
    nm_vect_t path_tok = NM_INIT_VECT;
    nm_str_t buf = NM_INIT_STR;

    if (path->len > PATH_MAX)
        nm_bug(_("%s: path \"%s\" too long"), __func__, path->data);

    nm_str_append_to_vect(path, &path_tok, "/");

    for (size_t n = 0; n < path_tok.n_memb; n++) {
        nm_str_append_format(&buf, "/%s", (char *)path_tok.data[n]);
        rc = mkdir(buf.data, mode);
        if (rc < 0 && errno != EEXIST) {
            nm_debug(_("%s: failed to create directory \"%s\" (%s)"),
                    __func__, buf.data, strerror(errno));
            fprintf(stderr, _("%s: failed to create directory \"%s\" (%s)\n"),
                    __func__, buf.data, strerror(errno));
            break;
        } else {
            rc = NM_OK;
        }
    }

    nm_str_free(&buf);
    nm_vect_free(&path_tok, NULL);
    return rc;
}

const char *nm_nemu_path()
{
    if (readlink("/proc/self/exe", nemu_path, PATH_MAX) < 0)
        nm_bug(_("%s: failed getting nemu binary path (%s)"),
            __func__, strerror(errno)
        );
    return nemu_path;
}

void nm_get_time(nm_str_t *res, const char *fmt)
{
    struct tm tm;
    time_t now;

    if (!res || !fmt) {
        return;
    }

    if (time(&now) == -1) {
        nm_bug(_("%s: cannot get time"), __func__);
    }

    if (localtime_r(&now, &tm) == NULL) {
        nm_bug(_("%s: cannot transform time"), __func__);
    }

    if (res->len) {
        nm_str_trunc(res, 0);
    }

    /* strftime(3) is bad desined, so we write own... */
    while (*fmt) {
        char ch = *fmt;
        char ch_next = *(fmt + 1);

        if (ch == '%' && ch_next != '\0') {
            fmt++;
            switch (ch_next) {
            case 'Y':
                nm_str_append_format(res, "%d", tm.tm_year + 1900);
                break;
            case 'm':
                nm_str_append_format(res, "%02d", tm.tm_mon + 1);
                break;
            case 'd':
                nm_str_append_format(res, "%02d", tm.tm_mday);
                break;
            case 'H':
                nm_str_append_format(res, "%02d", tm.tm_hour);
                break;
            case 'M':
                nm_str_append_format(res, "%02d", tm.tm_min);
                break;
            case 'S':
                nm_str_append_format(res, "%02d", tm.tm_sec);
                break;
            case '%':
                nm_str_add_char_opt(res, '%');
                break;
            default:
                nm_bug(_("%s: bad format: %%%c"), __func__, ch_next);
            }
        } else {
            nm_str_add_char_opt(res, ch);
        }

        fmt++;
    }
}

void nm_gen_rand_str(nm_str_t *res, size_t len)
{
    const char rnd[] = "0123456789"
        "abcdefghijklmnopqrstuvwxyz"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    struct timespec ts;

    if (!res) {
        return;
    }

    if (res->len) {
        nm_str_trunc(res, 0);
    }

    clock_gettime(CLOCK_MONOTONIC, &ts);
    srand((time_t) ts.tv_nsec);

    while (len) {
        nm_str_add_char_opt(res, rnd[rand() % (sizeof(rnd) - 1)]);
        len--;
    }
}

void nm_gen_uid(nm_str_t *res)
{
    const char fmt[] = "%Y-%m-%d-%H-%M-%S";
    nm_str_t time = NM_INIT_STR;
    nm_str_t rnd = NM_INIT_STR;

    if (!res) {
        return;
    }

    if (res->len) {
        nm_str_trunc(res, 0);
    }

    nm_get_time(&time, fmt);
    nm_gen_rand_str(&rnd, 8);

    nm_str_format(res, "%s-%s", time.data, rnd.data);

    nm_str_free(&time);
    nm_str_free(&rnd);
}
/* vim:set ts=4 sw=4: */
