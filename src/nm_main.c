#include <nm_core.h>
#include <nm_menu.h>
#include <nm_string.h>
#include <nm_window.h>
#include <nm_machine.h>
#include <nm_add_vm.h>
#include <nm_vm_list.h>
#include <nm_database.h>
#include <nm_cfg_file.h>
#include <nm_ovf_import.h>
#include <nm_lan_settings.h>

static void signals_handler(int signal);
static void nm_process_args(int argc, char **argv);
volatile sig_atomic_t redraw_window = 0;

int main(int argc, char **argv)
{
    struct sigaction sa;
    uint32_t highlight = NM_CHOICE_VM_LIST;
    uint32_t ch = 0;
    nm_window_t *main_window = NULL;

    setlocale(LC_ALL,"");
    bindtextdomain(NM_PROGNAME, NM_LOCALE_PATH);
    textdomain(NM_PROGNAME);

    nm_process_args(argc, argv);

    nm_cfg_init();
    nm_db_init();

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

            case NM_KEY_ENTER:
                choice = highlight;
                break;

            case NM_KEY_ESC:
                nm_curses_deinit();
                nm_db_close();
                nm_cfg_free();
                nm_mach_free();
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
                redraw_window = 0;
            }

            nm_print_main_menu(main_window, highlight);

            if (choice != 0)
                break;

        } /* }}} User input */

        if (choice == NM_CHOICE_VM_LIST)
        {
            nm_print_vm_list();
        }

        else if (choice == NM_CHOICE_VM_INST)
        {
            nm_add_vm();
        }

        else if (choice == NM_CHOICE_VM_IMPORT)
        {
            nm_import_vm();
        }
#if defined (NM_WITH_OVF_SUPPORT)
        else if (choice == NM_CHOICE_OVF_IMPORT)
        {
            nm_ovf_import();
        }
#endif
#if defined (NM_OS_LINUX)
        else if (choice == NM_CHOICE_NETWORK)
        {
            nm_lan_settings();
        }
#endif
        else if (choice == NM_CHOICE_QUIT)
        {
            nm_curses_deinit();
            nm_db_close();
            nm_cfg_free();
            nm_mach_free();
            exit(NM_OK);
        }
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

static void nm_process_args(int argc, char **argv)
{
    int opt;

    static const struct option longopts[] = {
        { "version", no_argument, NULL, 'v' },
        { "help",    no_argument, NULL, 'h' },
        { NULL,      0,           NULL,  0  }
    };

    while ((opt = getopt_long(argc, argv, "vh", longopts, NULL)) != -1)
    {
        switch (opt) {
        case 'v':
            printf("nEMU %s\n", NM_VERSION);
            exit(NM_OK);
            break;
        case 'h':
            printf("%s\n", _("-v, --version:   show version"));
            printf("%s\n", _("-h, --help:      show help"));
            exit(NM_OK);
            break;
        }
    }
}
/* vim:set ts=4 sw=4 fdm=marker: */
