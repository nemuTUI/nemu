#include <nm_core.h>
#include <nm_ncurses.h>

#define NM_VM_MSG   "F1 - help, F10 - main menu "
#define NM_MAIN_MSG "Use arrow keys to go up and down, "\
    "Press enter to select a choice, F10 - exit"

static void nm_print_window(const char *msg);

void nm_print_main_window(void)
{
    nm_print_window(_(NM_MAIN_MSG));
}

void nm_print_vm_window(void)
{
    nm_print_window(_(NM_VM_MSG));
}

void nm_print_warn(nm_window_t *w, const char *msg)
{
    box(w, 0, 0);
    mvwprintw(w, 1, 1, "%s", msg);
    wrefresh(w);
    wgetch(w);
}

static void nm_print_window(const char *msg)
{
    int col;
    size_t msg_len;

    col = getmaxx(stdscr);
    msg_len = mbstowcs(NULL, msg, strlen(msg));

    nm_clear_screen();
    border(0,0,0,0,0,0,0,0);
    mvprintw(1, (col - msg_len) / 2, msg);
    refresh();
}

/* vim:set ts=4 sw=4 fdm=marker: */
