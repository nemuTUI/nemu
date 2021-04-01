#include <nm_core.h>
#include <nm_utils.h>
#include <nm_ncurses.h>
#include <nm_cfg_file.h>

/*
 * ncurses must be compiled with --disable-leaks option
 * if you want a clean leak check
 */
void _nc_freeall(void);

inline void nm_ncurses_init(void)
{
    uint32_t cursor_style = nm_cfg_get()->cursor_style;
    initscr();
    raw();
    noecho();
    curs_set(0);
    if (cursor_style)
        printf("\033[%d q\n", cursor_style);
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
    if (nm_cfg_get()->cursor_style)
        puts("\033[0 q"); /* DECSCUSR 0 - set cursor to default (or blinking block) */
    clear();
    refresh();
    endwin();
    _nc_freeall();
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

/* vim:set ts=4 sw=4: */
