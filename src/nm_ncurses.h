#ifndef NM_NCURSES_H_
#define NM_NCURSES_H_

#if defined(CURSES_HAVE_NCURSES_CURSES_H)
#include <ncursesw/curses.h>
#include <ncursesw/panel.h>
#elif defined(CURSES_HAVE_CURSES_H)
#include <curses.h>
#include <panel.h>
#endif

typedef WINDOW nm_window_t;
typedef PANEL nm_panel_t;

typedef struct {
    int x;
    int y;
    int lines;
    int cols;
} nm_cord_t;

typedef struct {
    char **kwds;
    int count;
    bool checkcase;
    bool checkunique;
} nm_args_t;

#define NM_INIT_POS (nm_cord_t) { 0, 0, 0, 0 }
#define NM_SET_POS(l, c, x_, y_) \
    (nm_cord_t) { .lines  = l, .cols = c, .x = x_, .y = y_ }

void nm_ncurses_init(void);
void nm_curses_deinit(void);
nm_window_t *nm_init_window(const nm_cord_t *pos);
void nm_clear_screen(void);

#endif /* NM_NCURSES_H_*/
/* vim:set ts=4 sw=4: */
