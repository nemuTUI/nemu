#include <nm_core.h>
#include <nm_ncurses.h>

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
    {
        /* TODO: print error */
        exit(NM_ERR);
    }

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
