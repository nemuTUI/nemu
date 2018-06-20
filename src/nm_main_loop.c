#include <nm_core.h>
#include <nm_menu.h>
#include <nm_form.h>
#include <nm_utils.h>
#include <nm_string.h>
#include <nm_window.h>
#include <nm_add_vm.h>
#include <nm_machine.h>
#include <nm_edit_vm.h>
#include <nm_clone_vm.h>
#include <nm_database.h>
#include <nm_cfg_file.h>
#include <nm_edit_net.h>
#include <nm_9p_share.h>
#include <nm_usb_plug.h>
#include <nm_add_drive.h>
#include <nm_edit_boot.h>
#include <nm_ovf_import.h>
#include <nm_vm_control.h>
#include <nm_vm_snapshot.h>
#include <nm_qmp_control.h>

#define NM_SQL_GET_VM "SELECT name FROM vms ORDER BY name ASC"
#define NM_SEARCH_STR "Search:"

static void nm_action_menu_s(const nm_str_t *name);
static void nm_action_menu_r(const nm_str_t *name);
static size_t nm_search_vm(const nm_vect_t *list, int *err);
static int nm_search_cmp_cb(const void *s1, const void *s2);

void nm_start_main_loop(void)
{
    int ch, nemu = 0, regen_data = 1;
    int clear_action = 1;
    size_t vm_list_len, old_hl = 0;
    nm_menu_data_t vms = NM_INIT_MENU_DATA;
    nm_vmctl_data_t vm_props = NM_VMCTL_INIT_DATA;
    nm_vect_t vms_v = NM_INIT_VECT;
    nm_vect_t vm_list = NM_INIT_VECT;

    /* TODO use 256 color schema if ncurses and terminal support it */
    init_pair(1, COLOR_BLACK, COLOR_WHITE);
    init_pair(4, COLOR_RED, COLOR_WHITE);

    nm_create_windows();
    nm_init_help_main();
    nm_init_side();
    nm_init_action(NULL);

    for (;;)
    {
        if (regen_data)
        {
            nm_vect_free(&vm_list, nm_str_vect_free_cb);
            nm_vect_free(&vms_v, NULL);
            nm_db_select(NM_SQL_GET_VM, &vm_list);
            vm_list_len = (getmaxy(side_window) - 4);

            vms.highlight = 1;

            if (old_hl > 1)
            {
                if (vm_list.n_memb < old_hl)
                    vms.highlight = (old_hl - 1);
                else
                    vms.highlight = old_hl;
                old_hl = 0;
            }

            if (vm_list_len < vm_list.n_memb)
                vms.item_last = vm_list_len;
            else
                vms.item_last = vm_list_len = vm_list.n_memb;

            for (size_t n = 0; n < vm_list.n_memb; n++)
            {
                nm_menu_item_t vm = NM_INIT_MENU_ITEM;
                vm.name = (nm_str_t *) nm_vect_at(&vm_list, n);
                nm_vect_insert(&vms_v, &vm, sizeof(vm), NULL);
            }

            vms.v = &vms_v;

            regen_data = 0;
        }

        if (vm_list.n_memb > 0)
        {
            const nm_str_t *name = nm_vect_item_name_cur(vms);

            if (clear_action)
            {
                nm_vmctl_free_data(&vm_props);
                nm_vmctl_get_data(name, &vm_props);
                werase(action_window);
                nm_init_action(NULL);
                clear_action = 0;
            }

            nm_print_vm_menu(&vms);
            nm_print_vm_info(name, &vm_props);
            wrefresh(side_window);
            wrefresh(action_window);
        }

        ch = wgetch(side_window);

        /* Clear action window only if key pressed.
         * Otherwise text will be flicker in tty. */
        if (ch != ERR)
            clear_action = 1;

        if ((ch == KEY_UP) && (vms.highlight == 1) && (vms.item_first == 0) &&
            (vm_list_len < vms.v->n_memb) && (vm_list.n_memb > 0))
        {
            vms.highlight = vm_list_len;
            vms.item_first = vms.v->n_memb - vm_list_len;
            vms.item_last = vms.v->n_memb;
        }

        else if ((ch == KEY_UP) && (vm_list.n_memb > 0))
        {
            if ((vms.highlight == 1) && (vms.v->n_memb <= vm_list_len))
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

        else if ((ch == KEY_DOWN) && (vms.highlight == vm_list_len) &&
                 (vms.item_last == vms.v->n_memb) && (vm_list.n_memb > 0))
        {
            vms.highlight = 1;
            vms.item_first = 0;
            vms.item_last = vm_list_len;
        }

        else if (ch == KEY_DOWN && vm_list.n_memb > 0)
        {
            if ((vms.highlight == vms.v->n_memb) && (vms.v->n_memb <= vm_list_len))
                vms.highlight = 1;
            else if ((vms.highlight == vm_list_len) && (vms.item_last < vms.v->n_memb))
            {
                vms.item_first++;
                vms.item_last++;
            }
            else
            {
                vms.highlight++;
            }
        }

        else if (ch == KEY_HOME && vm_list.n_memb > 0)
        {
            vms.highlight = 1;
            vms.item_first = 0;
            vms.item_last = vm_list_len;
        }

        else if (ch == KEY_END && vm_list.n_memb > 0)
        {
            vms.highlight = vm_list_len;
            vms.item_first = vms.v->n_memb - vm_list_len;
            vms.item_last = vms.v->n_memb;
        }

        if (ch == NM_KEY_Q)
        {
            nm_destroy_windows();
            nm_curses_deinit();
            nm_db_close();
            nm_cfg_free();
            nm_mach_free();
            break;
        }

        if (vm_list.n_memb > 0)
        {
            const nm_str_t *name = nm_vect_item_name_cur(vms);
            int vm_status = nm_vect_item_status_cur(vms);

            switch (ch) {
            case NM_KEY_R:
                if (vm_status)
                {
                    nm_warn(_(NM_MSG_RUNNING));
                    break;
                }
                nm_vmctl_start(name, 0);
                break;

            case NM_KEY_T:
                if (vm_status)
                {
                    nm_warn(_(NM_MSG_RUNNING));
                    break;
                }
                nm_vmctl_start(name, NM_VMCTL_TEMP);
                break;

            case NM_KEY_P:
                if (vm_status)
                    nm_qmp_vm_shut(name);
                break;

            case NM_KEY_F:
                if (vm_status)
                    nm_qmp_vm_stop(name);
                break;

            case NM_KEY_Z:
                if (vm_status)
                    nm_qmp_vm_reset(name);
                break;

            case NM_KEY_P_UP:
                if (vm_status)
                    nm_qmp_vm_pause(name);
                break;

            case NM_KEY_R_UP:
                if (vm_status)
                    nm_qmp_vm_resume(name);
                break;

            case NM_KEY_K:
                if (vm_status)
                    nm_vmctl_kill(name);
                break;

            case NM_KEY_C:
                if (vm_status)
                    nm_vmctl_connect(name);
                break;

#if defined (NM_OS_LINUX)
            case NM_KEY_PLUS:
                werase(action_window);
                werase(help_window);
                nm_init_action(_(NM_MSG_USB_ATTACH));
                nm_init_help_edit();
                nm_usb_plug(name);
                werase(help_window);
                nm_init_help_main();
                break;

            case NM_KEY_MINUS:
                werase(action_window);
                werase(help_window);
                nm_init_action(_(NM_MSG_USB_DETACH));
                nm_init_help_edit();
                nm_usb_unplug(name);
                werase(help_window);
                nm_init_help_main();
                break;

            case NM_KEY_H:
                werase(action_window);
                werase(help_window);
                nm_init_action(_(NM_MSG_9P_SHARE));
                nm_init_help_edit();
                nm_9p_share(name);
                werase(help_window);
                nm_init_help_main();
                break;
#endif /* NM_OS_LINUX */

            case NM_KEY_E:
                werase(action_window);
                werase(help_window);
                nm_init_action(_(NM_MSG_EDIT_VM));
                nm_init_help_edit();
                nm_edit_vm(name);
                werase(help_window);
                nm_init_help_main();
                break;

            case NM_KEY_B:
                werase(action_window);
                werase(help_window);
                nm_init_action(_(NM_MSG_EDIT_BOOT));
                nm_init_help_edit();
                nm_edit_boot(name);
                werase(help_window);
                nm_init_help_main();
                break;

#ifdef NM_SAVEVM_SNAPSHOTS
            case NM_KEY_S_UP:
                if (!vm_status)
                {
                    nm_warn(NM_MSG_MUST_RUN);
                    break;
                }
                werase(action_window);
                werase(help_window);
                nm_init_action(_(NM_MSG_SNAP_CRT));
                nm_init_help_edit();
                nm_vm_snapshot_create(name);
                werase(help_window);
                nm_init_help_main();
                break;

            case NM_KEY_X_UP:
                werase(action_window);
                werase(help_window);
                nm_init_action(_(NM_MSG_SNAP_REV));
                nm_init_help_edit();
                nm_vm_snapshot_load(name, vm_status);
                werase(help_window);
                nm_init_help_main();
                break;

            case NM_KEY_D_UP:
                werase(action_window);
                werase(help_window);
                nm_init_action(_(NM_MSG_SNAP_DEL));
                nm_init_help_edit();
                nm_vm_snapshot_delete(name, vm_status);
                werase(help_window);
                nm_init_help_main();
                break;
#endif /* NM_SAVEVM_SNAPSHOTS */

            case NM_KEY_L:
                if (vm_status)
                {
                    nm_warn(_(NM_MSG_MUST_STOP));
                    break;
                }

                werase(action_window);
                werase(help_window);
                nm_init_action(_(NM_MSG_CLONE));
                nm_init_help_clone();
                nm_clone_vm(name);
                werase(help_window);
                nm_init_help_main();
                regen_data = 1;
                old_hl = vms.highlight;
                break;

            case NM_KEY_D:
                if (vm_status)
                {
                    nm_warn(_(NM_MSG_MUST_STOP));
                    break;
                }
                {
                    int ch = nm_notify(_(NM_MSG_DELETE));
                    if (ch == 'y')
                    {
                        nm_vmctl_delete(name);
                        regen_data = 1;
                        old_hl = vms.highlight;
                        if (vms.item_first != 0)
                        {
                            vms.item_first--;
                            vms.item_last--;
                        }
                    }
                    werase(side_window);
                    nm_init_side();
                }
                break;

            case NM_KEY_I:
                {
                    nm_vmctl_data_t vm_data = NM_VMCTL_INIT_DATA;
                    nm_vmctl_get_data(name, &vm_data);

                    if (vm_data.ifs.n_memb == 0)
                    {
                        nm_warn(_(NM_MSG_NO_IFACES));
                    }
                    else
                    {
                        werase(side_window);
                        werase(action_window);
                        werase(help_window);
                        nm_init_help_iface();
                        nm_init_action(_(NM_MSG_IF_PROP));
                        nm_init_side_if_list();
                        nm_edit_net(name, &vm_data);
                        werase(side_window);
                        werase(help_window);
                        nm_init_side();
                        nm_init_help_main();
                    }
                    nm_vmctl_free_data(&vm_data);
                }
                break;

            case NM_KEY_SLASH:
                {
                    int err = NM_FALSE;
                    size_t pos = nm_search_vm(&vm_list, &err);
                    int cols = getmaxx(side_window);

                    if (err == NM_TRUE)
                    {
                        nm_warn(_(NM_MSG_SMALL_WIN));
                        break;
                    }

                    if (pos > vm_list_len)
                    {
                        vms.highlight = vm_list_len;
                        vms.item_first = pos - vm_list_len;
                        vms.item_last = pos;
                    }
                    else if (pos != 0)
                    {
                        vms.item_first = 0;
                        vms.item_last = vm_list_len;
                        vms.highlight = pos;
                    }
                    NM_ERASE_TITLE(side, cols);
                    nm_init_side();
                }
                break;
            }
        }

        if (ch == NM_KEY_I_UP)
        {
            werase(action_window);
            werase(help_window);
            nm_init_action(_(NM_MSG_INSTALL));
            nm_init_help_install();
            nm_add_vm();
            werase(action_window);
            werase(help_window);
            nm_init_help_main();
            regen_data = 1;
        }

        if (ch == NM_KEY_A_UP)
        {
            werase(action_window);
            werase(help_window);
            nm_init_action(_(NM_MSG_IMPORT));
            nm_init_help_import();
            nm_import_vm();
            werase(action_window);
            werase(help_window);
            nm_init_help_main();
            regen_data = 1;
        }

        if (ch == NM_KEY_U)
        {
            nm_vmctl_clear_tap();
        }
#if defined (NM_WITH_OVF_SUPPORT)
        if (ch == NM_KEY_O_UP)
        {
            werase(action_window);
            werase(help_window);
            nm_init_action(_(NM_MSG_OVA_HEADER));
            nm_init_help_import();
            nm_ovf_import();
            werase(action_window);
            werase(help_window);
            nm_init_help_main();
            regen_data = 1;
        }
#endif

        if (ch == NM_KEY_QUESTION)
        {
            nm_print_help();
        }

        if (ch == 0x6e || ch == 0x45 || ch == 0x4d || ch == 0x55)
        {
            if (ch == 0x6e && !nemu)
                 nemu++;
            if (ch == 0x45 && nemu == 1)
                 nemu++;
            if (ch == 0x4d && nemu == 2)
                 nemu++;
            if (ch == 0x55 && nemu == 3)
            {
                werase(action_window);
                nm_init_action("Nemu Kurotsuchi");
                nm_print_nemu();
                werase(action_window);
                nm_init_action(NULL);
                wrefresh(action_window);
                nemu = 0;
            }
        }

        if (redraw_window)
        {
            nm_destroy_windows();
            endwin();
            refresh();
            nm_create_windows();
            nm_init_help_main();
            nm_init_side();
            nm_init_action(NULL);

            vm_list_len = (getmaxy(side_window) - 4);
            /* TODO save last pos */
            if (vm_list_len < vm_list.n_memb)
            {
                vms.item_last = vm_list_len;
                vms.item_first = 0;
                vms.highlight = 1;
            }
            else
                vms.item_last = vm_list_len = vm_list.n_memb;

            redraw_window = 0;
        }
    }

    nm_vmctl_free_data(&vm_props);
    nm_vect_free(&vms_v, NULL);
    nm_vect_free(&vm_list, nm_str_vect_free_cb);
}

void nm_print_vm_list(void)
{
    int ch, nemu = 0;
    int regen_data = 1, redraw_title = 0;
    uint32_t list_max = 0, old_hl = 0;
    nm_window_t *vm_window = NULL;
    nm_menu_data_t vms = NM_INIT_MENU_DATA;
    nm_vect_t vms_v = NM_INIT_VECT;
    nm_vect_t vm_list = NM_INIT_VECT;

    vms.item_last = nm_cfg_get()->list_max;

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

            if (list_max >= vm_list.n_memb)
            {
                vms.item_last = vm_list.n_memb;
                list_max = vm_list.n_memb;
            }

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

            //vm_window = nm_init_window(list_max + 4, 32, 7);

            wtimeout(vm_window, 500);
            regen_data = 0;
        }

        if (vm_list.n_memb > 0)
        {
            if (!vm_window)
            {
                //vm_window = nm_init_window(list_max + 4, 32, 7);
                wtimeout(vm_window, 500);
            }

            nm_print_vm_menu(&vms);
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

        else if (ch == KEY_HOME && vm_list.n_memb > 0)
        {
            vms.highlight = 1;
            vms.item_first = 0;
            vms.item_last = list_max;
        }

        else if (ch == KEY_END && vm_list.n_memb > 0)
        {
            vms.highlight = list_max;
            vms.item_first = vms.v->n_memb - list_max;
            vms.item_last = vms.v->n_memb;
        }

        else if (ch == NM_KEY_ENTER && vm_list.n_memb > 0)
        {
            if (nm_vect_item_status_cur(vms))
                nm_action_menu_r(nm_vect_item_name_cur(vms));
            else
                nm_action_menu_s(nm_vect_item_name_cur(vms));
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

        else if (ch == NM_KEY_QUESTION && vm_list.n_memb > 0)
        {
            //nm_print_vm_info(nm_vect_item_name_cur(vms));
        }

#if defined (NM_OS_LINUX)
        else if (ch == NM_KEY_PLUS && vm_list.n_memb > 0)
        {
            nm_usb_plug(nm_vect_item_name_cur(vms));
        }

        else if (ch == NM_KEY_MINUS && vm_list.n_memb > 0)
        {
            nm_usb_unplug(nm_vect_item_name_cur(vms));
        }
#endif

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
            nm_print_vm_menu(&vms);
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
            nm_print_vm_menu(&vms);
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
            nm_print_vm_menu(&vms);
            const nm_str_t *vm = nm_vect_item_name_cur(vms);
            int vm_status = nm_vect_item_status_cur(vms);

            /*if (vm_status)
                nm_snapshot_create(vm);
            else
                nm_print_warn(3, 6, _("VM must be running"));*/
        } /* }}} drive snapshot */

        /* {{{ Revert to snapshot */
        else if (ch == NM_KEY_X && vm_list.n_memb > 0)
        {
            nm_print_vm_menu(&vms);
            const nm_str_t *vm = nm_vect_item_name_cur(vms);
            int vm_status = nm_vect_item_status_cur(vms);

            /*if (!vm_status)
                nm_snapshot_revert(vm);
            else
                nm_print_warn(3, 6, _("VM must be stopped"));*/
        } /* }}} revert to snapshot */

#ifdef NM_SAVEVM_SNAPSHOTS
        /* {{{ Create vm snapshot */
        else if (ch == NM_KEY_S_UP && vm_list.n_memb > 0)
        {
            nm_print_vm_menu(&vms);
            const nm_str_t *vm = nm_vect_item_name_cur(vms);
            int vm_status = nm_vect_item_status_cur(vms);

            if (vm_status)
                nm_vm_snapshot_create(vm);
            else
                nm_print_warn(3, 6, _("VM must be running"));
        } /* }}} create vm snapshot */

        /* {{{ Load vm snapshot */
        else if (ch == NM_KEY_X_UP && vm_list.n_memb > 0)
        {
            nm_print_vm_menu(&vms);
            const nm_str_t *vm = nm_vect_item_name_cur(vms);
            int vm_status = nm_vect_item_status_cur(vms);

            nm_vm_snapshot_load(vm, vm_status);
        } /* }}} load vm snapshot */

        /* {{{ Delete vm snapshot */
        else if (ch == NM_KEY_D_UP && vm_list.n_memb > 0)
        {
            nm_print_vm_menu(&vms);
            const nm_str_t *vm = nm_vect_item_name_cur(vms);
            int vm_status = nm_vect_item_status_cur(vms);

            nm_vm_snapshot_delete(vm, vm_status);
        } /* }}} delete vm snapshot */
#endif /* NM_SAVEVM_SNAPSHOTS */

        /* {{{ Poweroff VM */
        else if (ch == NM_KEY_P && vm_list.n_memb > 0)
        {
            nm_print_vm_menu(&vms);
            const nm_str_t *vm = nm_vect_item_name_cur(vms);
            int vm_status = nm_vect_item_status_cur(vms);

            if (vm_status)
                nm_qmp_vm_shut(vm);
        } /* }}} Poweroff VM */

        /* {{{ stop VM */
        else if (ch == NM_KEY_F && vm_list.n_memb > 0)
        {
            nm_print_vm_menu(&vms);
            const nm_str_t *vm = nm_vect_item_name_cur(vms);
            int vm_status = nm_vect_item_status_cur(vms);

            if (vm_status)
                nm_qmp_vm_stop(vm);
        } /* }}} stop VM */

        /* {{{ reset VM */
        else if (ch == NM_KEY_Z && vm_list.n_memb > 0)
        {
            nm_print_vm_menu(&vms);
            const nm_str_t *vm = nm_vect_item_name_cur(vms);
            int vm_status = nm_vect_item_status_cur(vms);

            if (vm_status)
                nm_qmp_vm_reset(vm);
        } /* }}} reset VM */

        /* {{{ pause VM */
        else if (ch == NM_KEY_P_UP && vm_list.n_memb > 0)
        {
            nm_print_vm_menu(&vms);
            const nm_str_t *vm = nm_vect_item_name_cur(vms);
            int vm_status = nm_vect_item_status_cur(vms);

            if (vm_status)
                nm_qmp_vm_pause(vm);
        } /* }}} pause VM */

        /* {{{ resume VM */
        else if (ch == NM_KEY_R_UP && vm_list.n_memb > 0)
        {
            nm_print_vm_menu(&vms);
            const nm_str_t *vm = nm_vect_item_name_cur(vms);
            int vm_status = nm_vect_item_status_cur(vms);

            if (vm_status)
                nm_qmp_vm_resume(vm);
        } /* }}} resume VM */

        /* {{{ kill VM */
        else if (ch == NM_KEY_K && vm_list.n_memb > 0)
        {
            nm_print_vm_menu(&vms);
            const nm_str_t *vm = nm_vect_item_name_cur(vms);
            int vm_status = nm_vect_item_status_cur(vms);

            if (vm_status)
                nm_vmctl_kill(vm);
        } /* }}} kill VM */

        /* {{{ Delete VM */
        else if (ch == NM_KEY_D && vm_list.n_memb > 0)
        {
            nm_print_vm_menu(&vms);
            const nm_str_t *vm = nm_vect_item_name_cur(vms);
            int vm_status = nm_vect_item_status_cur(vms);

            if (vm_status)
            {
                nm_print_warn(3, 6, _("VM must be stopped"));
            }
            else
            {
                int ch = nm_print_warn(3, 6, _("Confirm deletion? (y/n)"));
                if (ch == 'y')
                {
                    nm_vmctl_delete(vm);
                    regen_data = 1;
                    old_hl = vms.highlight;
                    if (vms.item_first != 0)
                    {
                        vms.item_first--;
                        vms.item_last--;
                    }
                }
            }
        } /*}}} delete VM */

        /* {{{ Clone VM */
        else if (ch == NM_KEY_L && vm_list.n_memb > 0)
        {
            nm_print_vm_menu(&vms);
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

        /* {{{ Search in VM list */
        else if (ch == NM_KEY_SLASH && vm_list.n_memb > 0)
        {
            //size_t pos = nm_search_vm(&vm_list);
            size_t pos=0;

            if (pos > list_max)
            {
                vms.highlight = list_max;
                vms.item_first = pos - list_max;
                vms.item_last = pos;
            }
            else if (pos != 0)
            {
                vms.item_first = 0;
                vms.item_last = list_max;
                vms.highlight = pos;
            }
        } /* }}} */

        /* {{{ Print help */
        else if (ch == KEY_F(1))
        {
#if 0
            nm_window_t *help_window = nm_init_window(18, 40, 1);
            nm_print_help(help_window);
            delwin(help_window);
#endif
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

static void nm_action_menu_s(const nm_str_t *name)
{
    size_t ch = 0, hl = 1, act_len;
    nm_window_t *w = NULL;

    const char *actions[] = {
        _("start        [r]"),
        _("edit         [e]"),
        _("info         [?]")
    };

    enum {
        ACT_START = 1,
        ACT_EDIT,
        ACT_INFO
    };

    act_len = nm_arr_len(actions);

    for (;;)
    {
        if (w)
        {
            delwin(w);
            w = NULL;
        }

        //w = nm_init_window(5, 20, 6);
        box(w, 0, 0);

        for (size_t x = 2, y = 1, n = 0; n < act_len; n++, y++)
        {
            if (hl == n + 1)
            {
                wattron(w, A_REVERSE);
                mvwprintw(w, y, x, "%s", actions[n]);
                wattroff(w, A_REVERSE);
            }
            else
                mvwprintw(w, y, x, "%s", actions[n]);
        }

        ch = wgetch(w);

        if (ch == KEY_UP)
            (hl == ACT_START) ? hl = act_len : hl--;

        else if (ch == KEY_DOWN)
            (hl == act_len) ? hl = ACT_START : hl++;

        else if (ch == NM_KEY_ENTER)
        {
            if (hl == ACT_START)
                nm_vmctl_start(name, 0);

            if (hl == ACT_EDIT)
                nm_edit_vm(name);

            //if (hl == ACT_INFO)
                //nm_print_vm_info(name);

            break;
        }

        else
            break;
    }

    delwin(w);
}

static void nm_action_menu_r(const nm_str_t *name)
{
    size_t ch = 0, hl = 1, act_len;
    nm_window_t *w = NULL;

    const char *actions[] = {
#if defined (NM_WITH_VNC_CLIENT)
        _("connect      [c]"),
#endif
        _("stop         [p]"),
        _("reset        [z]"),
        _("edit         [e]"),
#if defined (NM_OS_LINUX)
        _("attach usb   [+]"),
        _("detach usb   [-]"),
#endif
        _("info         [?]")
    };

    enum {
#if defined (NM_WITH_VNC_CLIENT)
        ACT_CONNECT = 1,
        ACT_STOP,
#else
		ACT_STOP = 1,
#endif /* NM_WITH_VNC_CLIENT */
        ACT_RESET,
        ACT_EDIT,
#if defined (NM_OS_LINUX)
        ACT_ATTACH,
        ACT_DETACH,
#endif
        ACT_INFO
    };

    act_len = nm_arr_len(actions);

    for (;;)
    {
        if (w)
        {
            delwin(w);
            w = NULL;
        }

        //w = nm_init_window(ACT_INFO + 2, 20, 5);
        box(w, 0, 0);

        for (size_t x = 2, y = 1, n = 0; n < act_len; n++, y++)
        {
            if (hl == n + 1)
            {
                wattron(w, A_REVERSE);
                mvwprintw(w, y, x, "%s", actions[n]);
                wattroff(w, A_REVERSE);
            }
            else
                mvwprintw(w, y, x, "%s", actions[n]);
        }

        ch = wgetch(w);

        if (ch == KEY_UP)
            (hl == 1) ? hl = act_len : hl--;

        else if (ch == KEY_DOWN)
            (hl == act_len) ? hl = 1 : hl++;

        else if (ch == NM_KEY_ENTER)
        {
#if defined (NM_WITH_VNC_CLIENT)
            if (hl == ACT_CONNECT)
                nm_vmctl_connect(name);
#endif

            if (hl == ACT_STOP)
                nm_qmp_vm_shut(name);

            if (hl == ACT_RESET)
                nm_qmp_vm_reset(name);

            if (hl == ACT_EDIT)
                nm_edit_vm(name);

#if defined (NM_OS_LINUX)
            if (hl == ACT_ATTACH)
                nm_usb_plug(name);

            if (hl == ACT_DETACH)
                nm_usb_unplug(name);
#endif

            //if (hl == ACT_INFO)
             //   nm_print_vm_info(name);

            break;
        }

        else
            break;
    }

    delwin(w);
}

static size_t nm_search_vm(const nm_vect_t *list, int *err)
{
    size_t pos = 0;
    void *match = NULL;
    nm_form_t *form = NULL;
    nm_field_t *fields[2];
    nm_window_t *window = NULL;
    nm_str_t input = NM_INIT_STR;
    int cols = getmaxx(side_window);
    size_t msg_len = mbstowcs(NULL, _(NM_SEARCH_STR), strlen(NM_SEARCH_STR));
    int req_len = msg_len + 9;
    nm_form_data_t form_data = NM_INIT_FORM_DATA;

    assert(err != NULL);

    if (req_len > cols)
    {
        *err = NM_TRUE;
        return 0;
    }

    form_data.form_len = cols - (5 + msg_len);
    form_data.w_start_x = msg_len + 2;

    fields[0] = new_field(1, form_data.form_len, 0, 1, 0, 0);
    fields[1] = NULL;
    set_field_back(fields[0], A_UNDERLINE);
    field_opts_off(fields[0], O_STATIC);
    wattroff(side_window, COLOR_PAIR(2));
    NM_ERASE_TITLE(side, cols);
    mvwaddstr(side_window, 1, 2, _(NM_SEARCH_STR));
    wrefresh(side_window);

    form = nm_post_form__(side_window, fields, form_data.w_start_x, NM_FALSE);
    if (nm_draw_form(side_window, form) != NM_OK)
        goto out;

    nm_get_field_buf(fields[0], &input);
    if (!input.len)
        goto out;

    match = bsearch(&input, list->data, list->n_memb, sizeof(void *), nm_search_cmp_cb);

    if (match != NULL)
    {
        pos = (((unsigned char *)match - (unsigned char *)list->data) / sizeof(void *));
        pos++;
    }

    /* An incomplete match can happen not at the
     * beginning of the list with the same prefixes
     * in nm_search_cmp_cb.
     * So use backward linear search to fix it */
    if (pos <= 1)
        goto out;

    for (uint32_t n = pos - 1; n != 0; n--)
    {
        char *fo = strstr(nm_vect_str_ctx(list, n - 1), input.data);

        if (fo != NULL && fo == nm_vect_str_ctx(list, n - 1))
            pos--;
        else
            break;
    }

out:
    nm_form_free(form, fields);
    nm_str_free(&input);

    return pos;
}

static int nm_search_cmp_cb(const void *s1, const void *s2)
{
    int rc;
    const nm_str_t *str1 = s1;
    const nm_str_t **str2 = (const nm_str_t **) s2;

    rc = strcmp(str1->data, (*str2)->data);

    if (rc != 0)
    {
        char *fo = strstr((*str2)->data, str1->data);
        if (fo != NULL && fo == (*str2)->data)
            rc = 0;
    }

    return rc;
}
/* vim:set ts=4 sw=4 fdm=marker: */
