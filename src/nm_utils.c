#include <nm_core.h>
#include <nm_utils.h>
#include <nm_ncurses.h>

#if defined (NM_OS_LINUX)
#include <sys/sendfile.h>
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
    //...
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
