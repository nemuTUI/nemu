#ifndef NM_NCURSES_H_
#define NM_NCURSES_H_

#include <curses.h>

typedef WINDOW nm_window_t;

void nm_ncurses_init(void);
void nm_curses_deinit(void);
nm_window_t *nm_init_window(int nlines, int ncols, int begin_x);
void nm_clear_screen(void);

#endif /* NM_NCURSES_H_*/
/* vim:set ts=4 sw=4 fdm=marker: */
