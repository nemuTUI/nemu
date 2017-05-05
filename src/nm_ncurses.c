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

nm_window_t *nm_init_window(int nlines, int ncols, int begin_y)
{
    nm_window_t *w;
    int col;

    start_color();
    use_default_colors();
    col = getmaxx(stdscr);

    w = newwin(nlines, ncols, begin_y, (col - ncols) / 2);
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
