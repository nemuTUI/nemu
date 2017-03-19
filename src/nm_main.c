#include <nm_core.h>
#include <nm_main.h>
#include <nm_menu.h>
#include <nm_string.h>
#include <nm_window.h>
#include <nm_ncurses.h>
#include <nm_cfg_file.h>

static void signals_handler(int signal);
static volatile sig_atomic_t redraw_window = 0;

int main(void)
{
    struct sigaction sa;
    uint32_t highlight = 1;
    uint32_t ch, nemu = 0;
    const nm_cfg_t *cfg;
    nm_window_t *main_window = NULL;

    setlocale(LC_ALL,"");
    bindtextdomain(NM_PROGNAME, NM_LOCALE_PATH);
    textdomain(NM_PROGNAME);

    nm_cfg_init();
    cfg = nm_cfg_get();
    exit(NM_OK); /* XXX tmp */

    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sa.sa_handler = signals_handler;
    sigaction(SIGWINCH, &sa, NULL);

    nm_ncurses_init();

    /* {{{ Main loop start, print main window */
    for (;;)
    {
        uint32_t choice = 0;

        if (main_window)
        {
            delwin(main_window);
            main_window = NULL;
        }
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

            if (redraw_window)
            {
                endwin();
                refresh();
                clear();
                if (main_window)
                {
                    delwin(main_window);
                    main_window = NULL;
                }
                main_window = nm_init_window(10, 30, 7);
                nm_print_main_window();
                nm_print_main_menu(main_window, highlight);
                redraw_window = 0;
            }
            else
                nm_print_main_menu(main_window, highlight);

            if (choice != 0)
                break;

        } /* }}} User input */

        /* {{{ Print VM list */
        if (choice == NM_CHOICE_VM_LIST)
        {
            /* Print VM list here */
        } /* }}} VM list */

        /* {{{ Install VM window */
        else if (choice == NM_CHOICE_VM_INST)
        {
            /* Print install VM here */
        } /* }}} install VM */

        /* {{{ exit nEMU */
        else if (choice == NM_CHOICE_QUIT)
        {
            nm_curses_deinit();
            exit(NM_OK);
        } /* }}} */
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
