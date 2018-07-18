#include <nm_core.h>
#include <nm_utils.h>
#include <nm_ncurses.h>

#if (NM_DEBUG)
/* ncurses must be compiled with --disable-leaks option */
void _nc_freeall(void);
#endif

inline void nm_ncurses_init(void)
{
    initscr();
    raw();
    noecho();
    curs_set(0);
#if NCURSES_REENTRANT 
    set_escdelay(1);
#else
    ESCDELAY = 1;
#endif
    start_color();
    use_default_colors();
}

inline void nm_curses_deinit(void)
{
    clear();
    refresh();
    endwin();
#if (NM_DEBUG)
    _nc_freeall();
#endif
}

nm_window_t *nm_init_window(const nm_cord_t *pos)
{
    nm_window_t *w;

    w = newwin(pos->lines, pos->cols, pos->y, pos->x);
    if (w == NULL)
        nm_bug(_("%s: cannot create window"), __func__);

    keypad(w, TRUE);

    return w;
}

void nm_clear_screen(void)
{
    int x, y;

    getmaxyx(stdscr, y, x);

    for (int i = 0; i <= y; i++)
        mvhline(i, 0, ' ', x);
}

/* vim:set ts=4 sw=4 fdm=marker: */
