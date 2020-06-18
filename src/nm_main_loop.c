#include <nm_core.h>
#include <nm_main_loop.h>
#include <nm_menu.h>
#include <nm_form.h>
#include <nm_utils.h>
#include <nm_string.h>
#include <nm_window.h>
#include <nm_add_vm.h>
#include <nm_viewer.h>
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
#include <nm_mon_daemon.h>
#include <nm_vm_snapshot.h>
#include <nm_qmp_control.h>
#include <nm_lan_settings.h>

static const char NM_SEARCH_STR[] = "Search:";

typedef struct nm_filter {
    nm_str_t query;
    int type;
    int flags;
} nm_filter_t;

enum {
    NM_FILTER_NONE = -1,
    NM_FILTER_GROUP
};

enum {
    NM_FILTER_RESET =  (1 << 0),
    NM_FILTER_UPDATE = (1 << 1)
};

#define NM_INIT_FILTER (nm_filter_t) { NM_INIT_STR, NM_FILTER_NONE, 0 }

static size_t nm_search_vm(const nm_vect_t *list, int *err, nm_filter_t *filter);
static int nm_filter_check(const nm_str_t *input, nm_filter_t *filter);
static int nm_search_cmp_cb(const void *s1, const void *s2);

static inline void nm_filter_clean(nm_filter_t *filter)
{
    nm_str_free(&filter->query);
    filter->type = NM_FILTER_NONE;
    filter->flags = 0;
}

void nm_start_main_loop(void)
{
    int nemu = 0, regen_data = 1;
    int clear_action = 1;
    size_t vm_list_len, old_hl = 0;
    nm_menu_data_t vms = NM_INIT_MENU_DATA;
    nm_vmctl_data_t vm_props = NM_VMCTL_INIT_DATA;
    nm_vect_t vms_v = NM_INIT_VECT;
    nm_vect_t vm_list = NM_INIT_VECT;
    const nm_cfg_t *cfg = nm_cfg_get();
    nm_filter_t filter = NM_INIT_FILTER;

    init_pair(NM_COLOR_BLACK, COLOR_BLACK, COLOR_WHITE);
    init_pair(NM_COLOR_RED, COLOR_RED, COLOR_WHITE);
    if (cfg->hl_is_set && can_change_color()) {
        init_color(COLOR_WHITE + 1,
                cfg->hl_color.r, cfg->hl_color.g, cfg->hl_color.b);
        init_pair(NM_COLOR_HIGHLIGHT, COLOR_WHITE + 1, -1);
    } else {
        init_pair(NM_COLOR_HIGHLIGHT, COLOR_GREEN, -1);
    }

    nm_create_windows();
    nm_init_help_main();
    nm_init_side();
    nm_init_action(NULL);

    for (;;) {
        int ch;

        if (regen_data) {
            nm_str_t query = NM_INIT_STR;

            nm_vect_free(&vm_list, nm_str_vect_free_cb);
            nm_vect_free(&vms_v, NULL);

            switch (filter.type) {
            case NM_FILTER_NONE:
                nm_str_format(&query, "%s", NM_GET_VMS_SQL);
                break;
            case NM_FILTER_GROUP:
                nm_str_format(&query, NM_GET_VMS_FILTER_GROUP_SQL, filter.query.data);
                break;
            }

            nm_db_select(query.data, &vm_list);
            nm_str_free(&query);

            vm_list_len = (getmaxy(side_window) - 4);

            vms.highlight = 1;

            if (old_hl > 1) {
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

            for (size_t n = 0; n < vm_list.n_memb; n++) {
                nm_menu_item_t vm = NM_INIT_MENU_ITEM;
                vm.name = (nm_str_t *) nm_vect_at(&vm_list, n);
                nm_vect_insert(&vms_v, &vm, sizeof(vm), NULL);
            }

            vms.v = &vms_v;

            regen_data = 0;
        }

        if (vm_list.n_memb > 0) {
            const nm_str_t *name = nm_vect_item_name_cur(&vms);
            int status = nm_vect_item_status_cur(&vms);

            if (clear_action) {
                nm_vmctl_free_data(&vm_props);
                nm_vmctl_get_data(name, &vm_props);
                werase(action_window);
                nm_init_action(NULL);
                clear_action = 0;
            }

            nm_print_vm_menu(&vms);
            nm_print_vm_info(name, &vm_props, status);
            wrefresh(side_window);
            wrefresh(action_window);
        } else {
            if (clear_action) {
                werase(action_window);
                nm_init_action(NULL);
                clear_action = 0;
            }
        }

        ch = wgetch(side_window);

        /* Clear action window only if key pressed.
         * Otherwise text will be flicker in tty. */
        if (ch != ERR)
            clear_action = 1;

        if (vm_list.n_memb > 0)
            nm_menu_scroll(&vms, vm_list_len, ch);

        if (ch == NM_KEY_Q) {
            nm_destroy_windows();
            nm_curses_deinit();
            nm_db_close();
            nm_cfg_free();
            nm_mach_free();
            break;
        }

        if (vm_list.n_memb > 0) {
            const nm_str_t *name = nm_vect_item_name_cur(&vms);
            int vm_status = nm_vect_item_status_cur(&vms);

            switch (ch) {
            case NM_KEY_R:
                if (vm_status) {
                    nm_warn(_(NM_MSG_RUNNING));
                    break;
                }
                nm_vmctl_start(name, 0);
                break;

            case NM_KEY_T:
                if (vm_status) {
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

#if defined(NM_WITH_VNC_CLIENT) || defined(NM_WITH_SPICE)
            case NM_KEY_C:
                if (vm_status)
                    nm_vmctl_connect(name);
                break;
#endif

            case NM_KEY_M:
                nm_print_cmd(name);
                nm_init_side();
                nm_init_help_main();
                break;

            case NM_KEY_A:
                nm_add_drive(name);
                break;

            case NM_KEY_V:
                if (vm_status) {
                    nm_warn(_(NM_MSG_MUST_STOP));
                    break;
                }
                nm_del_drive(name);
                break;

#if defined (NM_OS_LINUX)
            case NM_KEY_PLUS:
                nm_usb_plug(name, vm_status);
                break;

            case NM_KEY_MINUS:
                nm_usb_unplug(name, vm_status);
                break;

            case NM_KEY_H:
                nm_9p_share(name);
                break;
#endif /* NM_OS_LINUX */

            case NM_KEY_E:
                nm_edit_vm(name);
                break;

            case NM_KEY_B:
                nm_edit_boot(name);
                break;

            case NM_KEY_C_UP:
                nm_viewer(name);
                break;

#ifdef NM_SAVEVM_SNAPSHOTS
            case NM_KEY_S_UP:
                if (!vm_status) {
                    nm_warn(NM_MSG_MUST_RUN);
                    break;
                }
                if (nm_usb_check_plugged(name) != NM_OK) {
                    nm_warn(NM_MSG_SNAP_USB);
                    break;
                }
                nm_vm_snapshot_create(name);
                break;

            case NM_KEY_X_UP:
                nm_vm_snapshot_load(name, vm_status);
                break;

            case NM_KEY_D_UP:
                nm_vm_snapshot_delete(name, vm_status);
                break;
#endif /* NM_SAVEVM_SNAPSHOTS */

            case NM_KEY_L:
                if (vm_status) {
                    nm_warn(_(NM_MSG_MUST_STOP));
                    break;
                }

                nm_clone_vm(name);
                regen_data = 1;
                old_hl = vms.highlight;
                nm_mon_ping();
                break;

            case NM_KEY_D:
                if (vm_status) {
                    nm_warn(_(NM_MSG_MUST_STOP));
                    break;
                }
                {
                    int ans = nm_notify(_(NM_MSG_DELETE));
                    if (ans == 'y') {
                        nm_vmctl_delete(name);
                        regen_data = 1;
                        old_hl = vms.highlight;
                        if (vms.item_first != 0) {
                            vms.item_first--;
                            vms.item_last--;
                        }
                        nm_mon_ping();
                    }
                    werase(side_window);
                    werase(action_window);
                    nm_init_side();
                    nm_init_action(NULL);
                }
                break;

            case NM_KEY_I:
                nm_edit_net(name);
                if (filter.type == NM_FILTER_GROUP)
                    nm_init_side_group(&filter.query);
                else
                    nm_init_side();
                break;

#if defined (NM_OS_LINUX)
            case NM_KEY_N_UP:
                nm_lan_settings();
                break;
#endif
            }
        }

        if (ch == NM_KEY_SLASH) {
            int err = NM_FALSE;
            size_t pos = nm_search_vm(&vm_list, &err, &filter);
            int cols = getmaxx(side_window);

            if (err == NM_TRUE) {
                nm_warn(_(NM_MSG_SMALL_WIN));
                break;
            }

            if (pos > vm_list_len) {
                vms.highlight = vm_list_len;
                vms.item_first = pos - vm_list_len;
                vms.item_last = pos;
            } else if (pos != 0) {
                vms.item_first = 0;
                vms.item_last = vm_list_len;
                vms.highlight = pos;
            }

            if (filter.flags & NM_FILTER_UPDATE) {
                regen_data = 1;
                werase(side_window);
                werase(action_window);
                nm_init_action(NULL);
                filter.flags &= ~NM_FILTER_UPDATE;
            } else if (filter.flags & NM_FILTER_RESET) {
                nm_filter_clean(&filter);
                regen_data = 1;
                werase(side_window);
                werase(action_window);
                nm_init_action(NULL);
                filter.flags &= ~NM_FILTER_RESET;
            }

            NM_ERASE_TITLE(side, cols);
            if (filter.type == NM_FILTER_GROUP)
                nm_init_side_group(&filter.query);
            else
                nm_init_side();
        }

        if (ch == NM_KEY_I_UP) {
            nm_add_vm();
            regen_data = 1;
            nm_mon_ping();
        }

        if (ch == NM_KEY_A_UP) {
            nm_import_vm();
            regen_data = 1;
            nm_mon_ping();
        }

        if (ch == NM_KEY_U) {
            nm_vmctl_clear_all_tap();
        }
#if defined (NM_WITH_OVF_SUPPORT)
        if (ch == NM_KEY_O_UP) {
            nm_ovf_import();
            regen_data = 1;
        }
#endif

        if (ch == NM_KEY_QUESTION) {
            nm_print_help();
        }

        if (ch == KEY_LEFT) {
            if (nm_window_scale_inc() == NM_OK)
                redraw_window = 1;
        }

        if (ch == KEY_RIGHT) {
            if (nm_window_scale_dec() == NM_OK)
                redraw_window = 1;
        }

        if (ch == 0x6e || ch == 0x45 || ch == 0x4d || ch == 0x55) {
            if (ch == 0x6e && !nemu)
                 nemu++;
            if (ch == 0x45 && nemu == 1)
                 nemu++;
            if (ch == 0x4d && nemu == 2)
                 nemu++;
            if (ch == 0x55 && nemu == 3) {
                werase(action_window);
                nm_init_action("Nemu Kurotsuchi");
                nm_print_nemu();
                nemu = 0;
            }
        }

        if (redraw_window) {
            nm_destroy_windows();
            endwin();
            refresh();
            nm_create_windows();
            nm_init_help_main();
            if (filter.type == NM_FILTER_GROUP)
                nm_init_side_group(&filter.query);
            else
                nm_init_side();
            nm_init_action(NULL);

            vm_list_len = (getmaxy(side_window) - 4);
            /* TODO save last pos */
            if (vm_list_len < vm_list.n_memb) {
                vms.item_last = vm_list_len;
                vms.item_first = 0;
                vms.highlight = 1;
            }
            else
                vms.item_last = vm_list_len = vm_list.n_memb;

            redraw_window = 0;
        }
    }

    nm_filter_clean(&filter);
    nm_vmctl_free_data(&vm_props);
    nm_vect_free(&vms_v, NULL);
    nm_vect_free(&vm_list, nm_str_vect_free_cb);
}

static size_t nm_search_vm(const nm_vect_t *list, int *err, nm_filter_t *filter)
{
    if (!err)
        return 0;

    size_t pos = 0;
    void *match = NULL;
    nm_form_t *form = NULL;
    nm_field_t *fields[2];
    nm_str_t input = NM_INIT_STR;
    int cols = getmaxx(side_window);
    size_t msg_len = mbstowcs(NULL, _(NM_SEARCH_STR), strlen(NM_SEARCH_STR));
    int req_len = msg_len + 9;
    nm_form_data_t form_data = NM_INIT_FORM_DATA;

    if (req_len > cols) {
        *err = NM_TRUE;
        return 0;
    }

    wattroff(side_window, COLOR_PAIR(NM_COLOR_HIGHLIGHT));

    form_data.form_len = cols - (5 + msg_len);
    form_data.w_start_x = msg_len + 2;

    fields[0] = new_field(1, form_data.form_len, 0, 1, 0, 0);
    fields[1] = NULL;
    set_field_back(fields[0], A_UNDERLINE);
    field_opts_off(fields[0], O_STATIC);
    NM_ERASE_TITLE(side, cols);
    mvwaddstr(side_window, 1, 2, _(NM_SEARCH_STR));
    wrefresh(side_window);

    form = nm_post_form(side_window, fields, form_data.w_start_x, NM_FALSE);
    if (nm_draw_form(side_window, form) != NM_OK)
        goto out;

    nm_get_field_buf(fields[0], &input);
    if (!input.len)
        goto out;

    if (nm_filter_check(&input, filter) == NM_OK)
        goto out;

    match = bsearch(&input, list->data, list->n_memb, sizeof(void *), nm_search_cmp_cb);

    if (match != NULL) {
        pos = (((unsigned char *)match - (unsigned char *)list->data) / sizeof(void *));
        pos++;
    }

    /* An incomplete match can happen not at the
     * beginning of the list with the same prefixes
     * in nm_search_cmp_cb.
     * So use backward linear search to fix it */
    if (pos <= 1)
        goto out;

    for (uint32_t n = pos - 1; n != 0; n--) {
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

static int nm_filter_check(const nm_str_t *input, nm_filter_t *filter)
{
    if (input->len <= 2)
        return NM_ERR;

    if (input->data[1] != ':')
        return NM_ERR;

    if (input->data[0] == 'g') {
        nm_str_format(&filter->query, "%s", input->data + 2);
        if (nm_str_cmp_st(&filter->query, "all") == NM_OK) {
            filter->flags |= NM_FILTER_RESET;
            return NM_OK;
        }

        filter->type = NM_FILTER_GROUP;
    }

    filter->flags |= NM_FILTER_UPDATE;

    return NM_OK;
}

static int nm_search_cmp_cb(const void *s1, const void *s2)
{
    int rc;
    const nm_str_t *str1 = s1;
    const nm_str_t **str2 = (const nm_str_t **) s2;

    rc = strcmp(str1->data, (*str2)->data);

    if (rc != 0) {
        char *fo = strstr((*str2)->data, str1->data);
        if (fo != NULL && fo == (*str2)->data)
            rc = 0;
    }

    return rc;
}
/* vim:set ts=4 sw=4: */
