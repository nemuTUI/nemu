#include <nm_core.h>
#include <nm_utils.h>
#include <nm_vector.h>
#include <nm_ncurses.h>

#include <sys/wait.h>

#define NM_BLKSIZE 131072 /* 128KiB */

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

    exit(NM_ERR);
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

    if ((file->fd = open(file->name->data, O_RDONLY)) == -1)
    {
        nm_bug(_("Cannot open file %s:%s"),
            file->name->data, strerror(errno));
    }

    if (fstat(file->fd, &stat) == -1)
    {
        close(file->fd);
        nm_bug(_("Cannot get file info %s:%s"),
            file->name->data, strerror(errno));
    }

    file->size = stat.st_size;
    file->mp = mmap(0, file->size, PROT_READ, MAP_PRIVATE, file->fd, 0);

    if (file->mp == MAP_FAILED)
    {
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

    if ((in_fd = open(src->data, O_RDONLY)) == -1)
    {
        nm_bug("%s: cannot open file %s: %s",
            __func__, src->data, strerror(errno));
    }

    if ((out_fd = open(dst->data, O_WRONLY | O_CREAT | O_EXCL, 0644)) == -1)
    {
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
    int rc;

    memset(&file_info, 0, sizeof(file_info));

    if (fstat(in_fd, &file_info) != 0)
        nm_bug("%s: cannot get file info %d: %s", __func__, in_fd, strerror(errno));

    while (offset < file_info.st_size)
    {
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

    while ((nread = read(in_fd, buf, NM_BLKSIZE)) > 0)
    {
        ssize_t nwrite;
        char *bufsp = buf;

        do {
            nwrite = write(out_fd, bufsp, NM_BLKSIZE);

            if (nwrite >= 0)
            {
                nread -= nwrite;
                bufsp += nwrite;
            }
            else if (errno != EINTR)
            {
                nm_bug("%s: copy file failed: %s", __func__, strerror(errno));
            }

        } while (nread > 0);
    }

    if (nread != 0)
        nm_bug("%s: copy was not compleete.", __func__);

    free(buf);
}
#endif

int nm_spawn_process(const nm_str_t *p)
{
    int rc = NM_OK;
    pid_t child_pid = 0;
    nm_vect_t argv = NM_INIT_VECT;
    nm_str_t path = NM_INIT_STR;
    nm_str_t tmp_p = NM_INIT_STR;

    nm_str_copy(&tmp_p, p);

    {
        int n = 0;
        char *token;
        char *saveptr = tmp_p.data;

        while ((token = strtok_r(saveptr, " ", &saveptr)))
        {
            if (n == 0)
                nm_str_alloc_text(&path, token);

            nm_vect_insert(&argv, token, strlen(token) + 1, NULL);

            n++;
        }
    }

    nm_vect_end_zero(&argv);

    switch (child_pid = fork()) {
    case (-1):  /* error*/
        nm_bug("%s: fork: %s", __func__, strerror(errno));
        break;

    case (0):   /* child */
        close(STDOUT_FILENO);
        close(STDERR_FILENO);

        execvp(path.data, (char *const *) argv.data);
        nm_bug("%s: unreachable reached", __func__);
        break;

    default:    /* parent */
        {
            int wstatus = 0;
            pid_t w_rc;

            w_rc = waitpid(child_pid, &wstatus, 0);
            if ((w_rc == child_pid) && (WEXITSTATUS(wstatus) != 0))
            {
                rc = NM_ERR;
                break;
            }
        }
    }

    nm_vect_free(&argv, NULL);
    nm_str_free(&path);
    nm_str_free(&tmp_p);

    return rc;
}

#ifdef NM_DEBUG
void nm_debug(const char *fmt, ...)
{
    va_list args;
    FILE *fp;

    if ((fp = fopen("/tmp/nemu_debug.log", "a+")) == NULL)
        return;

    va_start(args, fmt);
    vfprintf(fp, fmt, args);
    va_end(args);

    fclose(fp);
}
#endif

/* vim:set ts=4 sw=4 fdm=marker: */
