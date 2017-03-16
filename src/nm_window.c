#include <nm_core.h>
#include <nm_ncurses.h>

#define NM_MAIN_MSG "Use arrow keys to go up and down, "\
    "Press enter to select a choice, F10 - exit"

void nm_print_main_window(void)
{
    int col;
    size_t msg_len;
    const char *help_msg = _(NM_MAIN_MSG);

    col = getmaxx(stdscr);
    msg_len = mbstowcs(NULL, help_msg, strlen(help_msg));

    clear();
    border(0,0,0,0,0,0,0,0);
    mvprintw(1, (col - msg_len) / 2, help_msg);
    refresh();
}

/* vim:set ts=4 sw=4 fdm=marker: */
