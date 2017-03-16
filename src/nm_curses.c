#include <nm_curses.h>

inline void nm_curses_init(void)
{
    initscr();
    raw();
    noecho();
    curs_set(0);
}

inline void nm_curses_finish(void)
{
    clear();
    refresh();
    endwin();
}
/* vim:set ts=4 sw=4 fdm=marker: */
