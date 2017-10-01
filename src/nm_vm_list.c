#include <nm_core.h>
#include <nm_menu.h>
#include <nm_string.h>
#include <nm_window.h>
#include <nm_edit_vm.h>
#include <nm_clone_vm.h>
#include <nm_database.h>
#include <nm_cfg_file.h>
#include <nm_snapshot.h>
#include <nm_edit_net.h>
#include <nm_9p_share.h>
#include <nm_add_drive.h>
#include <nm_edit_boot.h>
#include <nm_vm_control.h>
#include <nm_qmp_control.h>

#define NM_SQL_GET_VM "SELECT name FROM vms ORDER BY name ASC"

extern sig_atomic_t redraw_window;

void nm_print_vm_list(void)
{
    int ch, nemu = 0;
    int regen_data = 1, redraw_title = 0;
    uint32_t list_max = 0, old_hl = 0;
    nm_window_t *vm_window = NULL;
    nm_menu_data_t vms = NM_INIT_MENU_DATA;
    nm_vect_t vms_v = NM_INIT_VECT;
    nm_vect_t vm_list = NM_INIT_VECT;

    nm_print_vm_window();

    for (;;)
    {
        if (redraw_title)
        {
            nm_print_vm_window();
            redraw_title = 0;
        }

        if (regen_data)
        {
            nm_vect_free(&vm_list, nm_str_vect_free_cb);
            nm_vect_free(&vms_v, NULL);
            nm_db_select(NM_SQL_GET_VM, &vm_list);
        }

        if (vm_list.n_memb == 0)
        {
            int col;
            size_t msg_len;
            const char *msg = _("No VMs installed");

            col = getmaxx(stdscr);
            msg_len = mbstowcs(NULL, msg, strlen(msg));
            mvprintw(4, (col - msg_len) / 2, msg);
            refresh();
        }

        if (regen_data)
        {
            vms.highlight = 1;
            list_max = nm_cfg_get()->list_max;

            if (old_hl > 1)
            {
                if (vm_list.n_memb < old_hl)
                    vms.highlight = (old_hl - 1);
                else
                    vms.highlight = old_hl;
                old_hl = 0;
            }

            if (list_max > vm_list.n_memb)
                list_max = vm_list.n_memb;

            vms.item_last = list_max;

            for (size_t n = 0; n < vm_list.n_memb; n++)
            {
                nm_menu_item_t vm = NM_INIT_MENU_ITEM;
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

            wtimeout(vm_window, 500);
            regen_data = 0;
        }

        if (vm_list.n_memb > 0)
        {
            if (!vm_window)
            {
                vm_window = nm_init_window(list_max + 4, 32, 7);
                wtimeout(vm_window, 500);
            }

            nm_print_vm_menu(vm_window, &vms);
        }

        ch = wgetch(vm_window);

        if ((ch != ERR) && (ch != KEY_UP) && (ch != KEY_DOWN))
            redraw_title = 1;

        if ((ch == KEY_UP) && (vms.highlight == 1) && (vms.item_first == 0) &&
            (list_max < vms.v->n_memb) && (vm_list.n_memb > 0))
        {
            vms.highlight = list_max;
            vms.item_first = vms.v->n_memb - list_max;
            vms.item_last = vms.v->n_memb;
        }

        else if ((ch == KEY_UP) && (vm_list.n_memb > 0))
        {
            if ((vms.highlight == 1) && (vms.v->n_memb <= list_max))
                vms.highlight = vms.v->n_memb;
            else if ((vms.highlight == 1) && (vms.item_first != 0))
            {
                vms.item_first--;
                vms.item_last--;
            }
            else
            {
                vms.highlight--;
            }
        }

        else if ((ch == KEY_DOWN) && (vms.highlight == list_max) &&
                 (vms.item_last == vms.v->n_memb) && (vm_list.n_memb > 0))
        {
            vms.highlight = 1;
            vms.item_first = 0;
            vms.item_last = list_max;
        }

        else if (ch == KEY_DOWN && vm_list.n_memb > 0)
        {
            if ((vms.highlight == vms.v->n_memb) && (vms.v->n_memb <= list_max))
                vms.highlight = 1;
            else if ((vms.highlight == list_max) && (vms.item_last < vms.v->n_memb))
            {
                vms.item_first++;
                vms.item_last++;
            }
            else
            {
                vms.highlight++;
            }
        }

        else if (ch == NM_KEY_ENTER && vm_list.n_memb > 0)
        {
            nm_print_vm_info(nm_vect_item_name_cur(vms));
        }

        else if (ch == NM_KEY_E && vm_list.n_memb > 0)
        {
            nm_edit_vm(nm_vect_item_name_cur(vms));
        }

        else if (ch == NM_KEY_M && vm_list.n_memb > 0)
        {
            nm_print_cmd(nm_vect_item_name_cur(vms));
        }

        else if (ch == NM_KEY_A && vm_list.n_memb > 0)
        {
            nm_add_drive(nm_vect_item_name_cur(vms));
        }

        else if (ch == NM_KEY_V && vm_list.n_memb > 0)
        {
            nm_del_drive(nm_vect_item_name_cur(vms));
        }

        else if (ch == NM_KEY_B && vm_list.n_memb > 0)
        {
            nm_edit_boot(nm_vect_item_name_cur(vms));
        }

        else if (ch == NM_KEY_U)
        {
            nm_vmctl_clear_tap();
        }

#if defined (NM_OS_LINUX)
        else if (ch == NM_KEY_H && vm_list.n_memb > 0)
        {
            nm_9p_share(nm_vect_item_name_cur(vms));
        }
#endif

        /* {{{ Edit network settings */
        else if (ch == NM_KEY_I && vm_list.n_memb > 0)
        {
            const nm_str_t *vm = nm_vect_item_name_cur(vms);
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
        else if (ch == NM_KEY_R && vm_list.n_memb > 0)
        {
            nm_print_vm_menu(vm_window, &vms);
            const nm_str_t *vm = nm_vect_item_name_cur(vms);
            int vm_status = nm_vect_item_status_cur(vms);

            if (vm_status)
                nm_print_warn(3, 6, _("already running"));
            else
                nm_vmctl_start(vm, 0);
        } /* }}} start VM */

        /* {{{ Start VM in temporary mode */
        else if (ch == NM_KEY_T && vm_list.n_memb > 0)
        {
            nm_print_vm_menu(vm_window, &vms);
            const nm_str_t *vm = nm_vect_item_name_cur(vms);
            int vm_status = nm_vect_item_status_cur(vms);

            if (vm_status)
                nm_print_warn(3, 6, _("already running"));
            else
                nm_vmctl_start(vm, NM_VMCTL_TEMP);
        } /* }}} start VM (temporary) */

        /* {{{ Create drive snapshot */
        else if (ch == NM_KEY_S && vm_list.n_memb > 0)
        {
            nm_print_vm_menu(vm_window, &vms);
            const nm_str_t *vm = nm_vect_item_name_cur(vms);
            int vm_status = nm_vect_item_status_cur(vms);

            if (vm_status)
                nm_snapshot_create(vm);
            else
                nm_print_warn(3, 6, _("VM must be running"));
        } /* }}} drive snapshot */

        /* {{{ Revert to snapshot */
        else if (ch == NM_KEY_X && vm_list.n_memb > 0)
        {
            nm_print_vm_menu(vm_window, &vms);
            const nm_str_t *vm = nm_vect_item_name_cur(vms);
            int vm_status = nm_vect_item_status_cur(vms);

            if (!vm_status)
                nm_snapshot_revert(vm);
            else
                nm_print_warn(3, 6, _("VM must be stopped"));
        } /* }}} revert to snapshot */

        /* {{{ Poweroff VM */
        else if (ch == NM_KEY_P && vm_list.n_memb > 0)
        {
            nm_print_vm_menu(vm_window, &vms);
            const nm_str_t *vm = nm_vect_item_name_cur(vms);
            int vm_status = nm_vect_item_status_cur(vms);

            if (vm_status)
                nm_qmp_vm_shut(vm);
        } /* }}} Poweroff VM */

        /* {{{ stop VM */
        else if (ch == NM_KEY_F && vm_list.n_memb > 0)
        {
            nm_print_vm_menu(vm_window, &vms);
            const nm_str_t *vm = nm_vect_item_name_cur(vms);
            int vm_status = nm_vect_item_status_cur(vms);

            if (vm_status)
                nm_qmp_vm_stop(vm);
        } /* }}} stop VM */

        /* {{{ reset VM */
        else if (ch == NM_KEY_Z && vm_list.n_memb > 0)
        {
            nm_print_vm_menu(vm_window, &vms);
            const nm_str_t *vm = nm_vect_item_name_cur(vms);
            int vm_status = nm_vect_item_status_cur(vms);

            if (vm_status)
                nm_qmp_vm_reset(vm);
        } /* }}} reset VM */

        /* {{{ kill VM */
        else if (ch == NM_KEY_K && vm_list.n_memb > 0)
        {
            nm_print_vm_menu(vm_window, &vms);
            const nm_str_t *vm = nm_vect_item_name_cur(vms);
            int vm_status = nm_vect_item_status_cur(vms);

            if (vm_status)
                nm_vmctl_kill(vm);
        } /* }}} kill VM */

        /* {{{ Delete VM */
        else if (ch == NM_KEY_D && vm_list.n_memb > 0)
        {
            nm_print_vm_menu(vm_window, &vms);
            const nm_str_t *vm = nm_vect_item_name_cur(vms);
            int vm_status = nm_vect_item_status_cur(vms);

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
                    regen_data = 1;
                    old_hl = vms.highlight;
                }
            }
        } /*}}} delete VM */

        /* {{{ Clone VM */
        else if (ch == NM_KEY_L && vm_list.n_memb > 0)
        {
            nm_print_vm_menu(vm_window, &vms);
            const nm_str_t *vm = nm_vect_item_name_cur(vms);
            int vm_status = nm_vect_item_status_cur(vms);

            if (vm_status)
            {
                nm_print_warn(3, 6, _("VM must be stopped"));
            }
            else
            {
                nm_clone_vm(vm);
                regen_data = 1;
                old_hl = vms.highlight;
            }
        } /*}}} clone VM */

#ifdef NM_WITH_VNC_CLIENT
        /* {{{ Connect to VM */
        else if (ch == NM_KEY_C && vm_list.n_memb > 0)
        {
            const nm_str_t *vm = nm_vect_item_name_cur(vms);
            int vm_status = nm_vect_item_status_cur(vms);

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
        else if (ch == NM_KEY_ESC)
        {
            if (vm_window)
            {
                delwin(vm_window);
                vm_window = NULL;
            }

            break;
        } /* }}} */

        if (redraw_window)
        {
            if (vm_window)
            {
                delwin(vm_window);
                vm_window = NULL;
            }
            endwin();
            refresh();
            clear();
            redraw_window = 0;
            redraw_title = 1;
        }
    }

    nm_vect_free(&vms_v, NULL);
    nm_vect_free(&vm_list, nm_str_vect_free_cb);
}
/* vim:set ts=4 sw=4 fdm=marker: */
