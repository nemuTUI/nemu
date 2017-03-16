#include <nm_core.h>
#include <nm_menu.h>
#include <nm_window.h>
#include <nm_ncurses.h>

static void signals_handler(int signal);
static volatile sig_atomic_t redraw_window = 0;

int main(void)
{
    struct sigaction sa;
    uint32_t highlight = 1;
    uint32_t ch, nemu = 0;
    WINDOW *main_window = NULL;

    setlocale(LC_ALL,"");
    bindtextdomain(NM_PROGNAME, NM_LOCALE_PATH);
    textdomain(NM_PROGNAME);

    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sa.sa_handler = signals_handler;
    sigaction(SIGWINCH, &sa, NULL);

    nm_ncurses_init();

    /* {{{ Main loop start, print main window */
    for (;;)
    {
        uint32_t choice = 0;

        main_window = nm_init_window(10, 30, 7);
        nm_print_main_window();
        nm_print_main_menu(main_window, highlight);

        /* {{{ Get user input */
        while ((ch = wgetch(main_window)))
        {
            switch (ch) {
            case KEY_UP:
                if (highlight == 1)
                    highlight = NM_MAIN_CHOICES;
                else
                    highlight--;
                break;

            case KEY_DOWN:
                if (highlight == NM_MAIN_CHOICES)
                    highlight = 1;
                else
                    highlight++;
                break;

            case 10:
                choice = highlight;
                break;

            case KEY_F(10):
                nm_curses_deinit();
                exit(NM_OK);
            }
            
            nm_print_main_menu(main_window, highlight);

            if (choice != 0)
                break;

        } /* }}} User input */
    } /* }}} Main loop */

    return NM_OK;
}

static void signals_handler(int signal)
{
    switch (signal) {
    case SIGWINCH:
        redraw_window = 1;
        break;
    }
}
/* vim:set ts=4 sw=4 fdm=marker: */
