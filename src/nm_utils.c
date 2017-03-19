#include <nm_core.h>
#include <nm_ncurses.h>

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

void *nm_realloc(void *p, size_t size)
{
    void *p_new;

    if ((p_new = realloc(p, size)) == NULL)
        nm_bug(_("realloc: %s\n"), strerror(errno));

    return p_new;
}

/* vim:set ts=4 sw=4 fdm=marker: */
