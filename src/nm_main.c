#include <nm_core.h>
#include <nm_main.h>
#include <nm_menu.h>
#include <nm_string.h>
#include <nm_window.h>
#include <nm_machine.h>
#include <nm_ncurses.h>
#include <nm_add_vm.h>
#include <nm_edit_vm.h>
#include <nm_clone_vm.h>
#include <nm_database.h>
#include <nm_cfg_file.h>
#include <nm_snapshot.h>
#include <nm_edit_net.h>
#include <nm_add_drive.h>
#include <nm_edit_boot.h>
#include <nm_vm_control.h>
#include <nm_qmp_control.h>

static void signals_handler(int signal);
static void nm_process_args(int argc, char **argv);
static volatile sig_atomic_t redraw_window = 0;

int main(int argc, char **argv)
{
    struct sigaction sa;
    uint32_t highlight = 1;
    uint32_t ch, nemu = 0;
    const nm_cfg_t *cfg;
    nm_window_t *main_window = NULL;
    nm_window_t *vm_window = NULL;

    setlocale(LC_ALL,"");
    bindtextdomain(NM_PROGNAME, NM_LOCALE_PATH);
    textdomain(NM_PROGNAME);

    nm_process_args(argc, argv);

    nm_cfg_init();
    nm_db_init();
    cfg = nm_cfg_get();

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

        /* {{{ Print VM list */
        if (choice == NM_CHOICE_VM_LIST)
        {
            nm_vect_t vm_list = NM_INIT_VECT;
            nm_db_select("SELECT name FROM vms ORDER BY name ASC", &vm_list);

            if (vm_list.n_memb == 0)
            {
                nm_print_warn(3, 6, _("No VMs installed"));
            }
            else
            {
                uint32_t list_max = cfg->list_max;
                nm_vm_list_t vms = NM_INIT_VMS_LIST;
                nm_vect_t vms_v = NM_INIT_VECT;

                vms.highlight = 1;

                if (list_max > vm_list.n_memb)
                    list_max = vm_list.n_memb;

                vms.vm_last = list_max;

                for (size_t n = 0; n < vm_list.n_memb; n++)
                {
                    nm_vm_t vm = NM_INIT_VM;
                    vm.name = (nm_str_t *) nm_vect_at(&vm_list, n);
                    nm_vect_insert(&vms_v, &vm, sizeof(vm), NULL);
                }

                vms.v = &vms_v;

                if (vm_window)
                {
                    delwin(vm_window);
                    vm_window = NULL;
                }
                vm_window = nm_init_window(list_max + 4, 32, 7);
                nm_print_vm_window();
                nm_print_vm_menu(vm_window, &vms);

                wtimeout(vm_window, 500);

                for (;;)
                {
                    ch = wgetch(vm_window);

                    if ((ch == KEY_UP) && (vms.highlight == 1) &&
                        (vms.vm_first == 0) && (list_max < vms.v->n_memb))
                    {
                        vms.highlight = list_max;
                        vms.vm_first = vms.v->n_memb - list_max;
                        vms.vm_last = vms.v->n_memb;
                    }

                    else if (ch == KEY_UP)
                    {
                        if ((vms.highlight == 1) && (vms.v->n_memb <= list_max))
                            vms.highlight = vms.v->n_memb;
                        else if ((vms.highlight == 1) && (vms.vm_first != 0))
                        {
                            vms.vm_first--;
                            vms.vm_last--;
                        }
                        else
                        {
                            vms.highlight--;
                        }
                    }

                    else if ((ch == KEY_DOWN) && (vms.highlight == list_max) &&
                             (vms.vm_last == vms.v->n_memb))
                    {
                        vms.highlight = 1;
                        vms.vm_first = 0;
                        vms.vm_last = list_max;
                    }

                    else if (ch == KEY_DOWN)
                    {
                        if ((vms.highlight == vms.v->n_memb) && (vms.v->n_memb <= list_max))
                            vms.highlight = 1;
                        else if ((vms.highlight == list_max) && (vms.vm_last < vms.v->n_memb))
                        {
                            vms.vm_first++;
                            vms.vm_last++;
                        }
                        else
                        {
                            vms.highlight++;
                        }
                    }

                    else if (ch == NM_KEY_ENTER)
                    {
                        nm_print_vm_info(nm_vect_vm_name_cur(vms));
                    }

                    else if (ch == NM_KEY_E)
                    {
                        nm_edit_vm(nm_vect_vm_name_cur(vms));
                    }

                    else if (ch == NM_KEY_M)
                    {
                        nm_print_cmd(nm_vect_vm_name_cur(vms));
                    }

                    else if (ch == NM_KEY_A)
                    {
                        nm_add_drive(nm_vect_vm_name_cur(vms));
                    }

                    else if (ch == NM_KEY_V)
                    {
                        nm_del_drive(nm_vect_vm_name_cur(vms));
                    }

                    else if (ch == NM_KEY_B)
                    {
                        nm_edit_boot(nm_vect_vm_name_cur(vms));
                    }

                    else if (ch == NM_KEY_U)
                    {
                        nm_vmctl_clear_tap();
                    }

                    /* {{{ Edit network settings */
                    else if (ch == NM_KEY_I)
                    {
                        const nm_str_t *vm = nm_vect_vm_name_cur(vms);
                        nm_vmctl_data_t vm_data = NM_VMCTL_INIT_DATA;
                        nm_vmctl_get_data(vm, &vm_data);

                        if (vm_data.ifs.n_memb == 0)
                        {
                            nm_print_warn(3, 6, _("No network configured"));
                        }
                        else
                        {
                            nm_edit_net(vm, &vm_data);
                        }
                        nm_vmctl_free_data(&vm_data);
                    } /* }}} net settings */

                    /* {{{ Start VM */
                    else if (ch == NM_KEY_R)
                    {
                        nm_print_vm_menu(vm_window, &vms);
                        const nm_str_t *vm = nm_vect_vm_name_cur(vms);
                        int vm_status = nm_vect_vm_status_cur(vms);

                        if (vm_status)
                            nm_print_warn(3, 6, _("already running"));
                        else
                            nm_vmctl_start(vm, 0);
                    } /* }}} start VM */

                    /* {{{ Start VM in temporary mode */
                    else if (ch == NM_KEY_T)
                    {
                        nm_print_vm_menu(vm_window, &vms);
                        const nm_str_t *vm = nm_vect_vm_name_cur(vms);
                        int vm_status = nm_vect_vm_status_cur(vms);

                        if (vm_status)
                            nm_print_warn(3, 6, _("already running"));
                        else
                            nm_vmctl_start(vm, NM_VMCTL_TEMP);
                    } /* }}} start VM (temporary) */

                    /* {{{ Create drive snapshot */
                    else if (ch == NM_KEY_S)
                    {
                        nm_print_vm_menu(vm_window, &vms);
                        const nm_str_t *vm = nm_vect_vm_name_cur(vms);
                        int vm_status = nm_vect_vm_status_cur(vms);

                        if (vm_status)
                            nm_snapshot_create(vm);
                        else
                            nm_print_warn(3, 6, _("VM must be running"));
                    } /* }}} drive snapshot */

                    /* {{{ Revert to snapshot */
                    else if (ch == NM_KEY_X)
                    {
                        nm_print_vm_menu(vm_window, &vms);
                        const nm_str_t *vm = nm_vect_vm_name_cur(vms);
                        int vm_status = nm_vect_vm_status_cur(vms);

                        if (!vm_status)
                            nm_snapshot_revert(vm);
                        else
                            nm_print_warn(3, 6, _("VM must be stopped"));
                    } /* }}} revert to snapshot */

                    /* {{{ Poweroff VM */
                    else if (ch == NM_KEY_P)
                    {
                        nm_print_vm_menu(vm_window, &vms);
                        const nm_str_t *vm = nm_vect_vm_name_cur(vms);
                        int vm_status = nm_vect_vm_status_cur(vms);

                        if (vm_status)
                            nm_qmp_vm_shut(vm);
                    } /* }}} Poweroff VM */

                    /* {{{ stop VM */
                    else if (ch == NM_KEY_F)
                    {
                        nm_print_vm_menu(vm_window, &vms);
                        const nm_str_t *vm = nm_vect_vm_name_cur(vms);
                        int vm_status = nm_vect_vm_status_cur(vms);

                        if (vm_status)
                            nm_qmp_vm_stop(vm);
                    } /* }}} stop VM */

                    /* {{{ reset VM */
                    else if (ch == NM_KEY_Z)
                    {
                        nm_print_vm_menu(vm_window, &vms);
                        const nm_str_t *vm = nm_vect_vm_name_cur(vms);
                        int vm_status = nm_vect_vm_status_cur(vms);

                        if (vm_status)
                            nm_qmp_vm_reset(vm);
                    } /* }}} reset VM */

                    /* {{{ kill VM */
                    else if (ch == NM_KEY_K)
                    {
                        nm_print_vm_menu(vm_window, &vms);
                        const nm_str_t *vm = nm_vect_vm_name_cur(vms);
                        int vm_status = nm_vect_vm_status_cur(vms);

                        if (vm_status)
                            nm_vmctl_kill(vm);
                    } /* }}} kill VM */

                    /* {{{ Delete VM */
                    else if (ch == NM_KEY_D)
                    {
                        nm_print_vm_menu(vm_window, &vms);
                        const nm_str_t *vm = nm_vect_vm_name_cur(vms);
                        int vm_status = nm_vect_vm_status_cur(vms);

                        if (vm_status)
                        {
                            nm_print_warn(3, 6, _("VM must be stopped"));
                        }
                        else
                        {
                            int ch = nm_print_warn(3, 6, _("Proceed? (y/n)"));
                            if (ch == 'y')
                            {
                                nm_vmctl_delete(vm);
                                /* TODO: get rid of exit from loop here */
                                /* exit from loop to reread guests */
                                break;
                            }
                        }
                    } /*}}} delete VM */

                    /* {{{ Clone VM */
                    else if (ch == NM_KEY_L)
                    {
                        nm_print_vm_menu(vm_window, &vms);
                        const nm_str_t *vm = nm_vect_vm_name_cur(vms);
                        int vm_status = nm_vect_vm_status_cur(vms);

                        if (vm_status)
                        {
                            nm_print_warn(3, 6, _("VM must be stopped"));
                        }
                        else
                        {
                            nm_clone_vm(vm);
                            /* TODO: get rid of exit from loop here */
                            /* exit from loop to reread guests */
                            break;
                        }
                    } /*}}} clone VM */

#ifdef NM_WITH_VNC_CLIENT
                    /* {{{ Connect to VM */
                    else if (ch == NM_KEY_C)
                    {
                        const nm_str_t *vm = nm_vect_vm_name_cur(vms);
                        int vm_status = nm_vect_vm_status_cur(vms);

                        if (vm_status)
                            nm_vmctl_connect(vm);
                    } /* }}} conenct to VM */
#endif

                    /* {{{ Nemu */
                    else if (ch == 0x6e || ch == 0x45 || ch == 0x4d || ch == 0x55)
                    {
                        if (ch == 0x6e && !nemu)
                             nemu++;
                        if (ch == 0x45 && nemu == 1)
                             nemu++;
                        if (ch == 0x4d && nemu == 2)
                             nemu++;
                        if (ch == 0x55 && nemu == 3)
                        {
                            nm_print_nemu();
                            nemu = 0;
                        }
                    } /* }}} */

                    /* {{{ Print help */
                    else if (ch == KEY_F(1))
                    {
#ifdef NM_WITH_VNC_CLIENT
                        nm_window_t *help_window = nm_init_window(18, 40, 1);
#else
                        nm_window_t *help_window = nm_init_window(17, 40, 1);
#endif
                        nm_print_help(help_window);
                        delwin(help_window);
                    } /* }}} help */

                    /* {{{ Back to main window */
                    else if (ch == KEY_F(10))
                    {
                        if (vm_window)
                        {
                            delwin(vm_window);
                            vm_window = NULL;
                        }
                        /*refresh();
                        endwin(); XXX*/

                        break;
                    } /* }}} */

                    if (redraw_window)
                    {
                        endwin();
                        refresh();
                        clear();
                        if (vm_window)
                        {
                            delwin(vm_window);
                            vm_window = NULL;
                        }
                        vm_window = nm_init_window(list_max + 4, 32, 7);
                        wtimeout(vm_window, 500);
                        redraw_window = 0;
                    }

                    nm_print_vm_window();
                    nm_print_vm_menu(vm_window, &vms);
                }
                nm_vect_free(&vms_v, NULL);
            }
            nm_vect_free(&vm_list, nm_str_vect_free_cb);
        } /* }}} VM list */

        /* {{{ Install VM window */
        else if (choice == NM_CHOICE_VM_INST)
        {
            nm_add_vm();
        } /* }}} install VM */

        /* {{{ exit nEMU */
        else if (choice == NM_CHOICE_QUIT)
        {
            nm_curses_deinit();
            nm_db_close();
            nm_cfg_free();
            nm_mach_free();
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
