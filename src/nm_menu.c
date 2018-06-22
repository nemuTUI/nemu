#include <nm_core.h>
#include <nm_menu.h>
#include <nm_utils.h>
#include <nm_string.h>
#include <nm_window.h>
#include <nm_network.h>
#include <nm_cfg_file.h>
#include <nm_lan_settings.h>

void nm_print_main_menu(nm_window_t *w, uint32_t highlight)
{
    const char *nm_main_chioces[] = {
        _("Manage guests"),
        _("Install guest"),
        _("Import image"),
#if defined (NM_WITH_OVF_SUPPORT)
        _("Import OVA"),
#endif
#if defined (NM_OS_LINUX)
        _("Local network"),
#endif
        _("Quit"),
    };

    box(w, 0, 0);

    for (uint32_t x = 2, y = 2, n = 0; n < NM_MAIN_CHOICES; n++, y++)
    {
        if (highlight ==  n + 1)
        {
            wattron(w, A_REVERSE);
            mvwprintw(w, y, x, "%s", nm_main_chioces[n]);
            wattroff(w, A_REVERSE);
        } 
        else
            mvwprintw(w, y, x, "%s", nm_main_chioces[n]);
    }
}

void nm_print_base_menu(nm_menu_data_t *ifs)
{
    int x = 2, y = 3;
    size_t screen_x;

    wattroff(side_window, COLOR_PAIR(2));
    wattroff(side_window, COLOR_PAIR(3));

    screen_x = getmaxx(side_window);
    if (screen_x < 20) /* window to small */
    {
        mvwaddstr(side_window, 3, 1, "...");
        wrefresh(side_window);
        return;
    }

    for (size_t n = ifs->item_first, i = 0; n < ifs->item_last; n++, i++)
    {
        nm_str_t if_name = NM_INIT_STR;
        int space_num;

        if (n >= ifs->v->n_memb)
            nm_bug(_("%s: invalid index: %zu"), __func__, n);

        nm_str_alloc_text(&if_name, (char *) ifs->v->data[n]);
        nm_align2line(&if_name, screen_x);

        space_num = (screen_x - if_name.len - 4);
        if (space_num > 0)
        {
            for (int n = 0; n < space_num; n++)
                nm_str_add_char_opt(&if_name, ' ');
        }

        if (ifs->highlight == i + 1)
        {
            wattron(side_window, A_REVERSE);
            mvwprintw(side_window, y, x, "%s", if_name.data);
            wattroff(side_window, A_REVERSE);
        }
        else
        {
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
    struct stat file_info;
    nm_str_t lock_path = NM_INIT_STR;

    screen_x = getmaxx(side_window);
    if (screen_x < 20) /* window to small */
    {
        mvwaddstr(side_window, 3, 1, "...");
        wrefresh(side_window);
        return;
    }

    memset(&file_info, 0, sizeof(file_info));

    /* TODO support 256 colors */
    init_pair(3, COLOR_GREEN, -1);
    wattroff(side_window, COLOR_PAIR(3));

    for (size_t n = vm->item_first, i = 0; n < vm->item_last; n++, i++)
    {
        nm_str_t vm_name = NM_INIT_STR;
        int space_num;

        if (n >= vm->v->n_memb)
            nm_bug(_("%s: invalid index: %zu"), __func__, n);

        nm_str_alloc_text(&vm_name, nm_vect_item_name(vm->v, n));
        nm_align2line(&vm_name, screen_x);

        space_num = (screen_x - vm_name.len - 4);
        if (space_num > 0)
        {
            for (int n = 0; n < space_num; n++)
                nm_str_add_char_opt(&vm_name, ' ');
        }

        nm_str_alloc_str(&lock_path, &nm_cfg_get()->vm_dir);
        nm_str_add_char(&lock_path, '/');
        nm_str_add_text(&lock_path, nm_vect_item_name(vm->v, n));
        nm_str_add_text(&lock_path, "/" NM_VM_QMP_FILE);

        if (stat(lock_path.data, &file_info) != -1)
        {
            nm_vect_set_item_status(vm->v, n, 1);
            wattron(side_window, COLOR_PAIR(3));
        }
        else
        {
            nm_vect_set_item_status(vm->v, n, 0);
            wattroff(side_window, COLOR_PAIR(3));
        }

        if (vm->highlight == i + 1)
        {
            wattron(side_window, A_REVERSE);
            mvwprintw(side_window, y, x, "%s", vm_name.data);
            wattroff(side_window, A_REVERSE);
        }
        else
        {
            mvwprintw(side_window, y, x, "%s", vm_name.data);
        }

        y++;
        wrefresh(side_window);
        nm_str_free(&vm_name);
    }

    nm_str_free(&lock_path);
}

void nm_menu_scroll(nm_menu_data_t *menu, size_t list_len, int ch)
{
    if ((ch == KEY_UP) && (menu->highlight == 1) && (menu->item_first == 0) &&
            (list_len < menu->v->n_memb))
    {
        menu->highlight = list_len;
        menu->item_first = menu->v->n_memb - list_len;
        menu->item_last = menu->v->n_memb;
    }

    else if (ch == KEY_UP)
    {
        if ((menu->highlight == 1) && (menu->v->n_memb <= list_len))
            menu->highlight = menu->v->n_memb;
        else if ((menu->highlight == 1) && (menu->item_first != 0))
        {
            menu->item_first--;
            menu->item_last--;
        }
        else
        {
            menu->highlight--;
        }
    }

    else if ((ch == KEY_DOWN) && (menu->highlight == list_len) &&
            (menu->item_last == menu->v->n_memb))
    {
        menu->highlight = 1;
        menu->item_first = 0;
        menu->item_last = list_len;
    }

    else if (ch == KEY_DOWN)
    {
        if ((menu->highlight == menu->v->n_memb) && (menu->v->n_memb <= list_len))
            menu->highlight = 1;
        else if ((menu->highlight == list_len) && (menu->item_last < menu->v->n_memb))
        {
            menu->item_first++;
            menu->item_last++;
        }
        else
        {
            menu->highlight++;
        }
    }

    else if (ch == KEY_HOME)
    {
        menu->highlight = 1;
        menu->item_first = 0;
        menu->item_last = list_len;
    }

    else if (ch == KEY_END)
    {
        menu->highlight = list_len;
        menu->item_first = menu->v->n_memb - list_len;
        menu->item_last = menu->v->n_memb;
    }
}

#if defined (NM_OS_LINUX)
void nm_print_veth_menu(nm_window_t *w, nm_menu_data_t *veth, int get_status)
{
    int x = 2, y = 2;
    nm_str_t veth_name = NM_INIT_STR;
    nm_str_t veth_copy = NM_INIT_STR;
    nm_str_t veth_lname = NM_INIT_STR;
    nm_str_t veth_rname = NM_INIT_STR;

    init_pair(1, COLOR_WHITE, COLOR_BLACK);
    init_pair(2, COLOR_GREEN, COLOR_BLACK);
    wattroff(w, COLOR_PAIR(1));
    wattroff(w, COLOR_PAIR(2));
    box(w, 0, 0);

    for (size_t n = veth->item_first, i = 0; n < veth->item_last; n++, i++)
    {
        if (n >= veth->v->n_memb)
            nm_bug(_("%s: invalid index: %zu"), __func__, n);

        nm_str_alloc_text(&veth_name, nm_vect_item_name(veth->v, n));
        nm_str_copy(&veth_copy, &veth_name);
        if (veth_name.len > 25)
        {
            nm_str_trunc(&veth_name, 25);
            nm_str_add_text(&veth_name, "...");
        }

        nm_lan_parse_name(&veth_copy, &veth_lname, &veth_rname);

        if (get_status)
        {
            /* If VETH interface does not exists, create it */
            if (nm_net_iface_exists(&veth_lname) != NM_OK)
            {
                nm_net_add_veth(&veth_lname, &veth_rname);
                nm_net_link_up(&veth_lname);
                nm_net_link_up(&veth_rname);
            }

            if (nm_net_link_status(&veth_lname) == NM_OK)
                nm_vect_set_item_status(veth->v, n, 1);
            else
                nm_vect_set_item_status(veth->v, n, 0);
        }

        if (nm_vect_item_status(veth->v, n))
            wattron(w, COLOR_PAIR(2));
        else
            wattron(w, COLOR_PAIR(1));

        if (veth->highlight == i + 1)
        {
            wattron(w, A_REVERSE);
            mvwprintw(w, y, x, "%-31s%s", veth_name.data,
                nm_vect_item_status(veth->v, n) ? NM_VETH_UP : NM_VETH_DOWN);
            wattroff(w, A_REVERSE);
        }
        else
        {
            mvwprintw(w, y, x, "%-31s%s", veth_name.data,
                nm_vect_item_status(veth->v, n) ? NM_VETH_UP : NM_VETH_DOWN);
        }

        y++;
        wrefresh(w);

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

/* vim:set ts=4 sw=4 fdm=marker: */
