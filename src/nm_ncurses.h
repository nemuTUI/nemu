#ifndef NM_NCURSES_H_
#define NM_NCURSES_H_

void nm_ncurses_init(void);
void nm_curses_deinit(void);
WINDOW *nm_init_window(int nlines, int ncols, int begin_x);

#endif /* NM_NCURSES_H_*/
/* vim:set ts=4 sw=4 fdm=marker: */
