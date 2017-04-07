#include <nm_core.h>
#include <nm_utils.h>
#include <nm_ncurses.h>

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
        nm_bug(_("malloc: %s\n"), strerror(errno));

    return p;
}

void *nm_calloc(size_t nmemb, size_t size)
{
    void *p;

    if ((p = calloc(nmemb, size)) == NULL)
        nm_bug(_("cmalloc: %s\n"), strerror(errno));

    return p;
}

void *nm_realloc(void *p, size_t size)
{
    void *p_new;

    if ((p_new = realloc(p, size)) == NULL)
        nm_bug(_("realloc: %s\n"), strerror(errno));

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

    memset(&file_info, 0, sizeof(file_info));

    if (fstat(in_fd, &file_info) != 0)
        nm_bug("%s: cannot get file info %d: %s", __func__, in_fd, strerror(errno));

    if (sendfile(out_fd, in_fd, &offset, file_info.st_size) == -1)
        nm_bug("%s: cannot copy file: %s", __func__, strerror(errno));
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
