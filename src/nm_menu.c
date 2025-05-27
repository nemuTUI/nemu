#include <nm_core.h>
#include <nm_menu.h>
#include <nm_dbus.h>
#include <nm_utils.h>
#include <nm_string.h>
#include <nm_window.h>
#include <nm_network.h>
#include <nm_cfg_file.h>
#include <nm_stat_usage.h>
#include <nm_qmp_control.h>
#include <nm_lan_settings.h>

void nm_print_dropdown_menu(nm_menu_data_t *values, nm_window_t *w)
{
    int x = 2, y = 1;

    box(w, 0, 0);
    curs_set(0);
    wattroff(w, COLOR_PAIR(NM_COLOR_HIGHLIGHT));

    for (size_t n = values->item_first, i = 0; n < values->item_last;
            n++, i++) {
        if (values->highlight == i + 1) {
            wattron(w, A_REVERSE);
            mvwprintw(w, y, x, "%s", (char *) values->v->data[n]);
            wattroff(w, A_REVERSE);
        } else {
            mvwprintw(w, y, x, "%s", (char *) values->v->data[n]);
        }

        y++;
    }
    wrefresh(w);
}

void nm_print_base_menu(nm_menu_data_t *ifs)
{
    int x = 2, y = 3;
    size_t screen_x;
    static nm_menu_data_t *ifs_;

    if (ifs) {
        ifs_ = ifs;
    }

    if (!ifs_) {
        return;
    }

    wattroff(side_window, COLOR_PAIR(NM_COLOR_HIGHLIGHT));

    screen_x = getmaxx(side_window);
    if (screen_x < 20) { /* window to small */
        mvwaddstr(side_window, 3, 1, "...");
        wrefresh(side_window);
        return;
    }

    for (size_t n = ifs_->item_first, i = 0; n < ifs_->item_last; n++, i++) {
        nm_str_t if_name = NM_INIT_STR;
        int space_num;

        if (n >= ifs_->v->n_memb) {
            nm_bug(_("%s: invalid index: %zu"), __func__, n);
        }

        nm_str_alloc_text(&if_name, (char *) ifs_->v->data[n]);
        nm_align2line(&if_name, screen_x);

        space_num = (screen_x - if_name.len - 4);
        if (space_num > 0) {
            for (int s = 0; s < space_num; s++) {
                nm_str_add_char_opt(&if_name, ' ');
            }
        }

        if (ifs_->highlight == i + 1) {
            wattron(side_window, A_REVERSE);
            mvwprintw(side_window, y, x, "%s", if_name.data);
            wattroff(side_window, A_REVERSE);
        } else {
            mvwprintw(side_window, y, x, "%s", if_name.data);
        }

        y++;
        wrefresh(side_window);
        nm_str_free(&if_name);
    }
}

void nm_print_vm_menu(nm_menu_data_t *vm)
{
    int x = 2, y = 3;
    size_t screen_x;
    static nm_menu_data_t *vm_;

    if (vm) {
        vm_ = vm;
    }

    if (!vm_) {
        return;
    }

    screen_x = getmaxx(side_window);
    if (screen_x < 20) { /* window to small */
        mvwaddstr(side_window, 3, 1, "...");
        wrefresh(side_window);
        return;
    }

    wattroff(side_window, COLOR_PAIR(NM_COLOR_HIGHLIGHT));

    for (size_t n = vm_->item_first, i = 0; n < vm_->item_last; n++, i++) {
        int space_num;
        nm_str_t vm_name = NM_INIT_STR;

        if (n >= vm_->v->n_memb) {
            nm_bug(_("%s: invalid index: %zu"), __func__, n);
        }

        nm_str_alloc_text(&vm_name, nm_vect_item_name_ctx(vm_->v, n));
        nm_align2line(&vm_name, screen_x);

        space_num = (screen_x - vm_name.len - 4);
        if (space_num > 0) {
            for (int s = 0; s < space_num; s++) {
                nm_str_add_char_opt(&vm_name, ' ');
            }
        }

        if (nm_qmp_test_socket(nm_vect_item_name(vm_->v, n)) == NM_OK) {
            nm_vect_set_item_status(vm_->v, n, 1);
            wattron(side_window, COLOR_PAIR(NM_COLOR_HIGHLIGHT));
        } else {
            nm_vect_set_item_status(vm_->v, n, 0);
            wattroff(side_window, COLOR_PAIR(NM_COLOR_HIGHLIGHT));
        }

        if (vm_->highlight == i + 1) {
            wattron(side_window, A_REVERSE);
            mvwprintw(side_window, y, x, "%s", vm_name.data);
            wattroff(side_window, A_REVERSE);
        } else {
            mvwprintw(side_window, y, x, "%s", vm_name.data);
        }

        y++;
        wrefresh(side_window);
        nm_str_free(&vm_name);
    }
}

void nm_menu_scroll(nm_menu_data_t *menu, size_t list_len, int ch)
{
    if ((ch == NM_CTRL_KEY_K) && (menu->highlight == 1) && (menu->item_first == 0) &&
            (list_len < menu->v->n_memb)) {
        menu->highlight = list_len;
        menu->item_first = menu->v->n_memb - list_len;
        menu->item_last = menu->v->n_memb;
    } else if (ch == NM_CTRL_KEY_K) {
        if ((menu->highlight == 1) && (menu->v->n_memb <= list_len)) {
            menu->highlight = menu->v->n_memb;
        } else if ((menu->highlight == 1) && (menu->item_first != 0)) {
            menu->item_first--;
            menu->item_last--;
        } else {
            menu->highlight--;
        }
    } else if ((ch == NM_CTRL_KEY_J) && (menu->highlight == list_len) &&
            (menu->item_last == menu->v->n_memb)) {
        menu->highlight = 1;
        menu->item_first = 0;
        menu->item_last = list_len;
    } else if (ch == NM_CTRL_KEY_J) {
        if ((menu->highlight == menu->v->n_memb) &&
                (menu->v->n_memb <= list_len)) {
            menu->highlight = 1;
        } else if ((menu->highlight == list_len) &&
                (menu->item_last < menu->v->n_memb)) {
            menu->item_first++;
            menu->item_last++;
        } else {
            menu->highlight++;
        }
    } else if (ch == KEY_HOME) {
        menu->highlight = 1;
        menu->item_first = 0;
        menu->item_last = list_len;
    } else if (ch == KEY_END) {
        menu->highlight = list_len;
        menu->item_first = menu->v->n_memb - list_len;
        menu->item_last = menu->v->n_memb;
    }

    if (ch == NM_CTRL_KEY_K || ch == NM_CTRL_KEY_J || ch == KEY_HOME || ch == KEY_END) {
        NM_STAT_CLEAN();
    }
}

#if defined (NM_OS_LINUX)
void nm_print_veth_menu(nm_menu_data_t *veth, int get_status)
{
    int x = 2, y = 3;
    size_t screen_x;
    nm_str_t veth_name = NM_INIT_STR;
    nm_str_t veth_copy = NM_INIT_STR;
    nm_str_t veth_lname = NM_INIT_STR;
    nm_str_t veth_rname = NM_INIT_STR;
    static nm_menu_data_t *veth_;

    if (veth) {
        veth_ = veth;
    }

    if (!veth_) {
        return;
    }

    screen_x = getmaxx(side_window);
    if (screen_x < 20) { /* window to small */
        mvwaddstr(side_window, 3, 1, "...");
        wrefresh(side_window);
        return;
    }

    wattroff(side_window, COLOR_PAIR(NM_COLOR_HIGHLIGHT));

    for (size_t n = veth_->item_first, i = 0; n < veth_->item_last; n++, i++) {
        int space_num;

        if (n >= veth_->v->n_memb) {
            nm_bug(_("%s: invalid index: %zu"), __func__, n);
        }

        nm_str_alloc_text(&veth_name, nm_vect_item_name_ctx(veth_->v, n));
        nm_str_copy(&veth_copy, &veth_name);
        nm_align2line(&veth_name, screen_x);
        nm_lan_parse_name(&veth_copy, &veth_lname, &veth_rname);

        space_num = (screen_x - veth_name.len - 4);
        if (space_num > 0) {
            for (int s = 0; s < space_num; s++) {
                nm_str_add_char_opt(&veth_name, ' ');
            }
        }

        if (get_status) {
            if (nm_net_link_status(&veth_lname) == NM_OK) {
                nm_vect_set_item_status(veth_->v, n, 1);
            } else {
                nm_vect_set_item_status(veth_->v, n, 0);
            }
        }

        if (nm_vect_item_status(veth_->v, n)) {
            wattron(side_window, COLOR_PAIR(NM_COLOR_HIGHLIGHT));
        } else {
            wattroff(side_window, COLOR_PAIR(NM_COLOR_HIGHLIGHT));
        }

        if (veth_->highlight == i + 1) {
            wattron(side_window, A_REVERSE);
            mvwprintw(side_window, y, x, "%s", veth_name.data);
            wattroff(side_window, A_REVERSE);
        } else {
            mvwprintw(side_window, y, x, "%s", veth_name.data);
        }

        y++;
        wrefresh(side_window);

        nm_str_trunc(&veth_name, 0);
        nm_str_trunc(&veth_copy, 0);
        nm_str_trunc(&veth_lname, 0);
        nm_str_trunc(&veth_rname, 0);
    }

    nm_str_free(&veth_name);
    nm_str_free(&veth_copy);
    nm_str_free(&veth_lname);
    nm_str_free(&veth_rname);
}
#endif /* NM_OS_LINUX */

/* vim:set ts=4 sw=4: */
