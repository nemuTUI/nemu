#include <nm_core.h>

#define NM_MAIN_MSG "Use arrow keys to go up and down, "\
    "Press enter to select a choice, F10 - exit"

static inline void nm_clear_screen(void);

void nm_print_main_window(void)
{
    int col;
    size_t msg_len;
    const char *help_msg = _(NM_MAIN_MSG);

    col = getmaxx(stdscr);
    msg_len = mbstowcs(NULL, help_msg, strlen(help_msg));

    nm_clear_screen();
    border(0,0,0,0,0,0,0,0);
    mvprintw(1, (col - msg_len) / 2, help_msg);
    refresh();
}

static inline void nm_clear_screen(void)
{
    int x, y;

    getmaxyx(stdscr, x, y);

    for (int i = 0; i <= y; i++)
        mvhline(i, 0, ' ', x);
}

/* vim:set ts=4 sw=4 fdm=marker: */
