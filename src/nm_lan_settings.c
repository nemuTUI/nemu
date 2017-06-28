#include <nm_core.h>
#include <nm_menu.h>
#include <nm_string.h>
#include <nm_vector.h>
#include <nm_window.h>
#include <nm_network.h>
#include <nm_database.h>
#include <nm_cfg_file.h>

#define NM_LAN_GET_VETH_SQL \
    "SELECT (l_name || '<->' || r_name) FROM veth"

static void nm_lan_help(void);
static void nm_lan_add_veth(void);
static void nm_lan_del_veth(void);

void nm_lan_settings(void)
{
    int ch = 0;
    nm_str_t query = NM_INIT_STR;
    nm_window_t *veth_window = NULL;

    nm_str_alloc_text(&query, NM_LAN_GET_VETH_SQL);

    keypad(stdscr, 1);

    for (;;)
    {
        nm_vect_t veths =  NM_INIT_VECT;
        nm_db_select(query.data, &veths);
        nm_print_vm_window();

        if (veths.n_memb == 0)
        {
            int col;
            size_t msg_len;
            const char *msg = _("No VETH interfaces configured");

            col = getmaxx(stdscr);
            msg_len = mbstowcs(NULL, msg, strlen(msg));
            mvprintw(4, (col - msg_len) / 2, msg);
        }
        else
        {
            uint32_t list_max = nm_cfg_get()->list_max;
            nm_menu_data_t veths_data = NM_INIT_MENU_DATA;
            nm_vect_t veths_list = NM_INIT_VECT;

            veths_data.highlight = 1;

            if (list_max > veths.n_memb)
                list_max = veths.n_memb;

            veths_data.item_last = list_max;

            for (size_t n = 0; n < veths.n_memb; n++)
            {
                nm_menu_item_t veth = NM_INIT_MENU_ITEM;
                veth.name = (nm_str_t *) nm_vect_at(&veths, n);
                nm_vect_insert(&veths_list, &veth, sizeof(veth), NULL);
            }

            veths_data.v = &veths_list;

            if (veth_window)
            {
                delwin(veth_window);
                veth_window = NULL;
            }
            veth_window = nm_init_window(list_max + 4, 32, 7);
            nm_print_veth_menu(veth_window, &veths_data);
        }

        switch (ch = getch()) {
        case NM_KEY_A:
            nm_lan_add_veth();
            break;

        case NM_KEY_R:
            nm_lan_del_veth();
            break;

        case KEY_F(1):
            nm_lan_help();
            break;

        case NM_KEY_ESC:
            nm_vect_free(&veths, nm_str_vect_free_cb);
            goto out;
        }

        nm_vect_free(&veths, nm_str_vect_free_cb);
    }

out:
    keypad(stdscr, 0);
    nm_str_free(&query);
}

static void nm_lan_help(void)
{
    nm_window_t *w = NULL;

    const char *msg[] = {
          "             nEMU v" NM_VERSION,
          "",
        _(" a - add veth interface"),
        _(" r - remove veth interface"),
        _(" u - up veth interface"),
        _(" d - down veth interface")
    };

    w = nm_init_window(9, 40, 1);
    box(w, 0, 0);

    for (size_t n = 0, y = 1; n < nm_arr_len(msg); n++, y++)
        mvwprintw(w, y, 1, "%s", msg[n]);

    wgetch(w);
    delwin(w);
}

static void nm_lan_add_veth(void)
{
    nm_str_t l_name = NM_INIT_STR;
    nm_str_t r_name = NM_INIT_STR;
    nm_str_t query = NM_INIT_STR;

    nm_str_alloc_text(&l_name, "vm1");
    nm_str_alloc_text(&r_name, "vm2");

    nm_net_add_veth(&l_name, &r_name);
    nm_net_link_up(&l_name);
    nm_net_link_up(&r_name);

    nm_str_format(&query, "INSERT INTO veth(l_name, r_name) VALUES ('%s', '%s')",
        l_name.data, r_name.data);
    nm_db_edit(query.data);

    nm_str_free(&l_name);
    nm_str_free(&r_name);
    nm_str_free(&query);
}

static void nm_lan_del_veth(void)
{
    nm_str_t name = NM_INIT_STR;
    nm_str_t query = NM_INIT_STR;

    nm_str_alloc_text(&name, "vm1");
    nm_net_del_iface(&name);

    nm_str_format(&query, "DELETE FROM veth WHERE l_name='%s'",
        name.data);
    nm_db_edit(query.data);

    nm_str_free(&name);
    nm_str_free(&query);
}

/* vim:set ts=4 sw=4 fdm=marker: */
