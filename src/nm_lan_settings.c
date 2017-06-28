#include <nm_core.h>
#include <nm_string.h>
#include <nm_vector.h>
#include <nm_window.h>
#include <nm_network.h>
#include <nm_database.h>

static void nm_lan_help(void);
static void nm_lan_map(void);
static void nm_lan_add_veth(void);
static void nm_lan_del_veth(void);

void nm_lan_settings(void)
{
    int ch = 0;

    keypad(stdscr, 1);

    for (;;)
    {
        nm_print_vm_window();
        nm_lan_map();

        switch (ch = getch()) {
        case NM_KEY_A:
            nm_lan_add_veth();
            break;

        case NM_KEY_D:
            nm_lan_del_veth();
            break;

        case KEY_F(1):
            nm_lan_help();
            break;

        case NM_KEY_ESC:
            keypad(stdscr, 0);
            return;
        }
    }
}

static void nm_lan_help(void)
{
    nm_window_t *w = NULL;

    const char *msg[] = {
          "             nEMU v" NM_VERSION,
          "",
        _(" a - add veth interface"),
        _(" d - delete veth interface"),
    };

    w = nm_init_window(7, 40, 1);
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

static void nm_lan_map(void)
{
    nm_vect_t veths =  NM_INIT_VECT;
    nm_str_t query = NM_INIT_STR;

    nm_vect_free(&veths, nm_str_vect_free_cb);
    nm_str_free(&query);
}

/* vim:set ts=4 sw=4 fdm=marker: */
