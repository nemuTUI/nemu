#include <nm_core.h>
#include <nm_form.h>
#include <nm_menu.h>
#include <nm_utils.h>
#include <nm_string.h>
#include <nm_vector.h>
#include <nm_window.h>
#include <nm_network.h>
#include <nm_database.h>
#include <nm_cfg_file.h>
#include <nm_lan_settings.h>

#if defined (NM_OS_LINUX)

#define NM_LAN_FIELDS_NUM 2

#define NM_LAN_GET_VETH_SQL \
    "SELECT (l_name || '<->' || r_name) FROM veth ORDER by l_name ASC"
#define NM_LAN_ADD_VETH_SQL \
    "INSERT INTO veth(l_name, r_name) VALUES ('%s', '%s')"
#define NM_LAN_CHECK_NAME_SQL \
    "SELECT id FROM veth WHERE l_name='%s' OR r_name='%s'"
#define NM_LAN_DEL_VETH_SQL \
    "DELETE FROM veth WHERE l_name='%s'"
#define NM_LAN_VETH_INF_SQL \
    "SELECT if_name FROM ifaces WHERE parent_eth='%s'"
#define NM_LAN_VETH_DEP_SQL \
    "UPDATE ifaces SET macvtap='0', parent_eth='' WHERE parent_eth='%s' OR parent_eth='%s'"

extern sig_atomic_t redraw_window;

static nm_field_t *fields[NM_LAN_FIELDS_NUM + 1];

enum {
    NM_FLD_LNAME = 0,
    NM_FLD_RNAME
};

static const char *nm_form_add_msg[] = {
    "Name", "Peer name", NULL
};

static void nm_lan_add_veth(void);
static void nm_lan_del_veth(const nm_str_t *name);
static void nm_lan_up_veth(const nm_str_t *name);
static void nm_lan_down_veth(const nm_str_t *name);
static void nm_lan_veth_info(const nm_str_t *name);
static int nm_lan_add_get_data(nm_str_t *ln, nm_str_t *rn);

void nm_lan_settings(void)
{
    int ch = 0, regen_data = 1, renew_status = 0;
    nm_str_t query = NM_INIT_STR;
    nm_vect_t veths = NM_INIT_VECT;
    nm_vect_t veths_list = NM_INIT_VECT;
    nm_menu_data_t veths_data = NM_INIT_MENU_DATA;
    size_t veth_list_len, old_hl = 0;

    werase(side_window);
    werase(action_window);
    werase(help_window);
    nm_init_help_lan();
    nm_init_action(_(NM_MSG_LAN));
    nm_init_side_lan();

    do {
        if (veths.n_memb > 0)
            nm_menu_scroll(&veths_data, veth_list_len, ch);

        if (ch == NM_KEY_QUESTION)
            nm_lan_help();

        else if (ch == NM_KEY_A)
        {
            werase(action_window);
            nm_init_action(_(NM_MSG_ADD_VETH));
            nm_lan_add_veth();
            regen_data = 1;
            old_hl = veths_data.highlight;
        }

        else if (ch == NM_KEY_R)
        {
            if (veths.n_memb > 0)
            {
                nm_lan_del_veth(nm_vect_item_name_cur(veths_data));
                regen_data = 1;
                old_hl = veths_data.highlight;
                if (veths_data.item_first != 0)
                {
                    veths_data.item_first--;
                    veths_data.item_last--;
                }
                werase(side_window);
                nm_init_side_lan();
            }
        }

        else if (ch == NM_KEY_D)
        {
            if (veths.n_memb > 0)
            {
                nm_lan_down_veth(nm_vect_item_name_cur(veths_data));
                renew_status = 1;
            }
        }

        else if (ch == NM_KEY_U)
        {
            if (veths.n_memb > 0)
            {
                nm_lan_up_veth(nm_vect_item_name_cur(veths_data));
                renew_status = 1;
            }
        }

        if (regen_data)
        {
            nm_vect_free(&veths_list, NULL);
            nm_vect_free(&veths, nm_str_vect_free_cb);
            nm_db_select(NM_LAN_GET_VETH_SQL, &veths);
            veth_list_len = (getmaxy(side_window) - 4);

            veths_data.highlight = 1;

            if (old_hl > 1)
            {
                if (veths.n_memb < old_hl)
                    veths_data.highlight = (old_hl - 1);
                else
                    veths_data.highlight = old_hl;
                old_hl = 0;
            }

            if (veth_list_len < veths.n_memb)
                veths_data.item_last = veth_list_len;
            else
                veths_data.item_last = veth_list_len = veths.n_memb;

            for (size_t n = 0; n < veths.n_memb; n++)
            {
                nm_menu_item_t veth = NM_INIT_MENU_ITEM;
                veth.name = (nm_str_t *) nm_vect_at(&veths, n);
                nm_vect_insert(&veths_list, &veth, sizeof(veth), NULL);
            }

            veths_data.v = &veths_list;

            regen_data = 0;
            renew_status = 1;
        }

        if (veths.n_memb > 0)
        {
            werase(action_window);
            nm_init_action(_(NM_MSG_LAN));
            nm_print_veth_menu(&veths_data, renew_status);
            nm_lan_veth_info(nm_vect_item_name_cur(veths_data));

            if (renew_status)
                renew_status = 0;
        }

        if (redraw_window)
        {
            nm_destroy_windows();
            endwin();
            refresh();
            nm_create_windows();
            nm_init_help_lan();
            nm_init_action(_(NM_MSG_LAN));
            nm_init_side_lan();

            veth_list_len = (getmaxy(side_window) - 4);
            /* TODO save last pos */
            if (veth_list_len < veths.n_memb)
            {
                veths_data.item_last = veth_list_len;
                veths_data.item_first = 0;
                veths_data.highlight = 1;
            }
            else
                veths_data.item_last = veth_list_len = veths.n_memb;

            redraw_window = 0;
        }
    } while ((ch = wgetch(action_window)) != NM_KEY_Q);

    werase(side_window);
    werase(help_window);
    nm_init_side();
    nm_init_help_main();
    nm_vect_free(&veths, nm_str_vect_free_cb);
    nm_vect_free(&veths_list, NULL);
    nm_str_free(&query);
}

static void nm_lan_add_veth(void)
{
    nm_str_t l_name = NM_INIT_STR;
    nm_str_t r_name = NM_INIT_STR;
    nm_str_t query = NM_INIT_STR;
    nm_form_data_t form_data = NM_INIT_FORM_DATA;
    nm_form_t *form = NULL;
    size_t msg_len = nm_max_msg_len(nm_form_add_msg);

    if (nm_form_calc_size(msg_len, NM_LAN_FIELDS_NUM, &form_data) != NM_OK)
        return;

    for (size_t n = 0; n < NM_LAN_FIELDS_NUM; ++n)
        fields[n] = new_field(1, form_data.form_len, n * 2, 0, 0, 0);

    fields[NM_LAN_FIELDS_NUM] = NULL;

    set_field_type(fields[NM_FLD_LNAME], TYPE_REGEXP, "^[a-zA-Z0-9_-]{1,15} *$");
    set_field_type(fields[NM_FLD_RNAME], TYPE_REGEXP, "^[a-zA-Z0-9_-]{1,15} *$");

    for (size_t n = 0, y = 1, x = 2; n < NM_LAN_FIELDS_NUM; n++)
    {
        mvwaddstr(form_data.form_window, y, x, nm_form_add_msg[n]);
        y += 2;
    }

    form = nm_post_form__(form_data.form_window, fields, msg_len + 4, NM_TRUE);
    if (nm_draw_form(action_window, form) != NM_OK)
        goto out;

    if (nm_lan_add_get_data(&l_name, &r_name) != NM_OK)
        goto out;

    nm_net_add_veth(&l_name, &r_name);
    nm_net_link_up(&l_name);
    nm_net_link_up(&r_name);

    nm_str_format(&query, NM_LAN_ADD_VETH_SQL, l_name.data, r_name.data);
    nm_db_edit(query.data);

out:
    nm_form_free(form, fields);
    nm_str_free(&l_name);
    nm_str_free(&r_name);
    nm_str_free(&query);
}

static int nm_lan_add_get_data(nm_str_t *ln, nm_str_t *rn)
{
    int rc = NM_OK;
    nm_str_t query = NM_INIT_STR;
    nm_vect_t err = NM_INIT_VECT;
    nm_vect_t names = NM_INIT_VECT;

    nm_get_field_buf(fields[NM_FLD_LNAME], ln);
    nm_get_field_buf(fields[NM_FLD_RNAME], rn);

    nm_form_check_datap(_("Name"), ln, err);
    nm_form_check_datap(_("Peer name"), rn, err);

    if ((rc = nm_print_empty_fields(&err)) == NM_ERR)
    {
        nm_vect_free(&err, NULL);
        goto out;
    }

    nm_str_format(&query, NM_LAN_CHECK_NAME_SQL, ln->data, ln->data);
    nm_db_select(query.data, &names);
    if (names.n_memb > 0)
    {
        nm_print_warn(3, 2, _("This name is already taken"));
        rc = NM_ERR;
        goto out;
    }

    nm_str_trunc(&query, 0);
    nm_str_format(&query, NM_LAN_CHECK_NAME_SQL, rn->data, rn->data);
    nm_db_select(query.data, &names);
    if (names.n_memb > 0)
    {
        nm_print_warn(3, 2, _("This name is already taken"));
        rc = NM_ERR;
        goto out;
    }

    if (nm_str_cmp_ss(ln, rn) == NM_OK)
    {
        nm_print_warn(3, 2, _("Names must be different"));
        rc = NM_ERR;
    }

out:
    nm_vect_free(&names, nm_str_vect_free_cb);
    nm_str_free(&query);
    return rc;
}

static void nm_lan_del_veth(const nm_str_t *name)
{
    nm_str_t lname = NM_INIT_STR;
    nm_str_t rname = NM_INIT_STR;
    nm_str_t query = NM_INIT_STR;

    nm_lan_parse_name(name, &lname, &rname);
    nm_net_del_iface(&lname);

    nm_str_format(&query, NM_LAN_DEL_VETH_SQL, lname.data);
    nm_db_edit(query.data);
    nm_str_trunc(&query, 0);
    nm_str_format(&query, NM_LAN_VETH_DEP_SQL, lname.data, rname.data);
    nm_db_edit(query.data);

    nm_str_free(&lname);
    nm_str_free(&rname);
    nm_str_free(&query);
}

static void nm_lan_up_veth(const nm_str_t *name)
{
    nm_str_t lname = NM_INIT_STR;
    nm_str_t rname = NM_INIT_STR;

    nm_lan_parse_name(name, &lname, &rname);

    nm_net_link_up(&lname);
    nm_net_link_up(&rname);

    nm_str_free(&lname);
    nm_str_free(&rname);
}

static void nm_lan_down_veth(const nm_str_t *name)
{
    nm_str_t lname = NM_INIT_STR;
    nm_str_t rname = NM_INIT_STR;

    nm_lan_parse_name(name, &lname, &rname);

    nm_net_link_down(&lname);
    nm_net_link_down(&rname);

    nm_str_free(&lname);
    nm_str_free(&rname);
}

static void nm_lan_veth_info(const nm_str_t *name)
{
    chtype ch1, ch2;
    size_t y = 3, x = 2;
    size_t cols, rows;
    nm_str_t rname = NM_INIT_STR;
    nm_str_t query = NM_INIT_STR;
    nm_str_t buf = NM_INIT_STR;
    nm_vect_t ifs = NM_INIT_VECT;

    ch1 = ch2 = 0;
    getmaxyx(action_window, rows, cols);
    nm_lan_parse_name(name, &buf, &rname);

    nm_str_format(&query, NM_LAN_VETH_INF_SQL, buf.data);
    nm_db_select(query.data, &ifs);
    nm_str_add_char(&buf, ':');

    if (ifs.n_memb > 0)
        for (size_t n = 0; n < ifs.n_memb; n++)
            nm_str_format(&buf, " %s", nm_vect_str_ctx(&ifs, n));
    else
        nm_str_add_text(&buf, _(" [none]"));

    NM_PR_VM_INFO();
    nm_str_trunc(&buf, 0);

    nm_vect_free(&ifs, nm_str_vect_free_cb);
    nm_str_trunc(&query, 0);
    nm_str_copy(&buf, &rname);

    nm_str_format(&query, NM_LAN_VETH_INF_SQL, buf.data);
    nm_db_select(query.data, &ifs);
    nm_str_add_char(&buf, ':');

    if (ifs.n_memb > 0)
        for (size_t n = 0; n < ifs.n_memb; n++)
            nm_str_format(&buf, " %s", nm_vect_str_ctx(&ifs, n));
    else
        nm_str_add_text(&buf, _(" [none]"));
    NM_PR_VM_INFO();

    nm_vect_free(&ifs, nm_str_vect_free_cb);
    nm_str_free(&rname);
    nm_str_free(&query);

    nm_str_free(&buf);
}

void nm_lan_parse_name(const nm_str_t *name, nm_str_t *ln, nm_str_t *rn)
{
    nm_str_t name_copy = NM_INIT_STR;
    char *cp = NULL;

    nm_str_copy(&name_copy, name);

    if (rn != NULL)
    {
        cp = strchr(name_copy.data, '>');
        nm_str_alloc_text(rn, ++cp);
        cp = NULL;
    }

    cp = strchr(name_copy.data, '<');
    if (cp)
    {
        *cp = '\0';
        nm_str_alloc_text(ln, name_copy.data);
    }

    nm_str_free(&name_copy);
}
#endif /* NM_OS_LINUX */

/* vim:set ts=4 sw=4 fdm=marker: */
