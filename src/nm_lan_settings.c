#include <nm_core.h>
#include <nm_form.h>
#include <nm_menu.h>
#include <nm_utils.h>
#include <nm_string.h>
#include <nm_vector.h>
#include <nm_window.h>
#include <nm_network.h>
#include <nm_svg_map.h>
#include <nm_database.h>
#include <nm_cfg_file.h>
#include <nm_lan_settings.h>

#if defined (NM_OS_LINUX)

const char NM_LAN_FORM_LNAME[] = "Name";
const char NM_LAN_FORM_RNAME[] = "Peer name";

static void nm_lan_init_main_windows(bool redraw);
static void nm_lan_init_add_windows(nm_form_t *form);
static size_t nm_lan_labels_setup();
static void nm_lan_add_veth(void);
static void nm_lan_del_veth(const nm_str_t *name);
static void nm_lan_up_veth(const nm_str_t *name);
static void nm_lan_down_veth(const nm_str_t *name);
static void nm_lan_veth_info(const nm_str_t *name);
static int nm_lan_add_get_data(nm_str_t *ln, nm_str_t *rn);

enum {
    NM_LAN_LBL_LNAME = 0, NM_LAN_FLD_LNAME,
    NM_LAN_LBL_RNAME, NM_LAN_FLD_RNAME,
    NM_LAN_FLD_COUNT
};

static nm_field_t *fields_lan[NM_LAN_FLD_COUNT + 1];

#if defined (NM_WITH_NETWORK_MAP)
const char *nm_form_svg_state[] = {
    "UP",
    "DOWN",
    "ALL",
    NULL
};

const char *nm_form_svg_layout[] = {
    "neato",
    "dot",
    NULL
};

const char NM_SVG_FORM_PATH[] = "Export path";
const char NM_SVG_FORM_TYPE[] = "State";
const char NM_SVG_FORM_LAYT[] = "Layout";
const char NM_SVG_FORM_GROUP[] = "Group";

static void nm_svg_init_windows(nm_form_t *form);
static void nm_svg_fields_setup();
static size_t nm_svg_labels_setup();
static void nm_lan_export_svg(const nm_vect_t *veths);

enum {
    NM_SVG_LBL_PATH = 0, NM_SVG_FLD_PATH,
    NM_SVG_LBL_TYPE, NM_SVG_FLD_TYPE,
    NM_SVG_LBL_LAYT, NM_SVG_FLD_LAYT,
    NM_SVG_LBL_GROUP, NM_SVG_FLD_GROUP,
    NM_SVG_FLD_COUNT
};

static nm_field_t *fields_svg[NM_SVG_FLD_COUNT + 1];
#endif /* NM_WITH_NETWORK_MAP */

static void nm_lan_init_main_windows(bool redraw)
{
    if (redraw) {
        nm_form_window_init();
    } else {
        werase(action_window);
        werase(help_window);
        werase(side_window);
    }

    nm_init_help_lan();
    nm_init_action(_(NM_MSG_LAN));
    nm_init_side_lan();

    nm_print_veth_menu(NULL, 1);
}

static void nm_lan_init_add_windows(nm_form_t *form)
{
    if (form) {
        nm_form_window_init();
        nm_form_data_t *form_data = (nm_form_data_t *)form_userptr(form);
        if(form_data)
            form_data->parent_window = action_window;
    } else {
        werase(action_window);
        werase(help_window);
    }

    nm_init_help_edit();
    nm_init_action(_(NM_MSG_ADD_VETH));
    nm_init_side_lan();

    nm_print_veth_menu(NULL, 1);
}

void nm_lan_settings(void)
{
    int ch = 0, regen_data = 1, renew_status = 0;
    nm_str_t query = NM_INIT_STR;
    nm_vect_t veths = NM_INIT_VECT;
    nm_vect_t veths_list = NM_INIT_VECT;
    nm_menu_data_t veths_data = NM_INIT_MENU_DATA;
    size_t veth_list_len, old_hl;

    veth_list_len = old_hl = 0;

    nm_lan_create_veth(NM_FALSE);

    nm_lan_init_main_windows(false);

    do {
        if (ch == NM_KEY_QUESTION) {
            nm_lan_help();
        } else if (ch == NM_KEY_A) {
            nm_lan_add_veth();
            regen_data = 1;
            old_hl = veths_data.highlight;
        } else if (ch == NM_KEY_R) {
            if (veths.n_memb > 0) {
                nm_lan_del_veth(nm_vect_item_name_cur(&veths_data));
                regen_data = 1;
                old_hl = veths_data.highlight;

                if (veths_data.item_first != 0) {
                    veths_data.item_first--;
                    veths_data.item_last--;
                }
                werase(side_window);
                nm_init_side_lan();
            }
        } else if (ch == NM_KEY_D) {
            if (veths.n_memb > 0) {
                nm_lan_down_veth(nm_vect_item_name_cur(&veths_data));
                renew_status = 1;
            }
        } else if (ch == NM_KEY_U) {
            if (veths.n_memb > 0) {
                nm_lan_up_veth(nm_vect_item_name_cur(&veths_data));
                renew_status = 1;
            }
        }
#if defined (NM_WITH_NETWORK_MAP)
        else if (ch == NM_KEY_E) {
            if (veths.n_memb > 0)
                nm_lan_export_svg(&veths);
        }
#endif

        if (regen_data) {
            nm_vect_free(&veths_list, NULL);
            nm_vect_free(&veths, nm_str_vect_free_cb);
            nm_db_select(NM_LAN_GET_VETH_SQL, &veths);
            veth_list_len = (getmaxy(side_window) - 4);

            veths_data.highlight = 1;

            if (old_hl > 1) {
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

            for (size_t n = 0; n < veths.n_memb; n++) {
                nm_menu_item_t veth = NM_INIT_MENU_ITEM;
                veth.name = (nm_str_t *) nm_vect_at(&veths, n);
                nm_vect_insert(&veths_list, &veth, sizeof(veth), NULL);
            }

            veths_data.v = &veths_list;

            regen_data = 0;
            renew_status = 1;
        }

        if (veths.n_memb > 0) {
            nm_menu_scroll(&veths_data, veth_list_len, ch);
            werase(action_window);
            nm_init_action(_(NM_MSG_LAN));
            nm_print_veth_menu(&veths_data, renew_status);
            nm_lan_veth_info(nm_vect_item_name_cur(&veths_data));

            if (renew_status)
                renew_status = 0;
        } else {
            werase(action_window);
            nm_init_action(_(NM_MSG_LAN));
        }

        if (redraw_window) {
            nm_lan_init_main_windows(true);

            veth_list_len = (getmaxy(side_window) - 4);
            /* TODO save last pos */
            if (veth_list_len < veths.n_memb) {
                veths_data.item_last = veth_list_len;
                veths_data.item_first = 0;
                veths_data.highlight = 1;
            } else {
                veths_data.item_last = veth_list_len = veths.n_memb;
            }

            redraw_window = 0;
        }
    } while ((ch = wgetch(action_window)) != NM_KEY_Q);

    werase(side_window);
    werase(help_window);
    nm_init_help_main();
    nm_vect_free(&veths, nm_str_vect_free_cb);
    nm_vect_free(&veths_list, NULL);
    nm_str_free(&query);
}

static void nm_lan_add_veth(void)
{
    nm_form_data_t *form_data = NULL;
    nm_form_t *form = NULL;
    nm_str_t l_name = NM_INIT_STR;
    nm_str_t r_name = NM_INIT_STR;
    nm_str_t query = NM_INIT_STR;
    size_t msg_len;

    nm_lan_init_add_windows(NULL);

    msg_len = nm_lan_labels_setup();

    form_data = nm_form_data_new(
        action_window, nm_lan_init_add_windows, msg_len, NM_LAN_FLD_COUNT / 2, NM_TRUE);

    if (nm_form_data_update(form_data, 0, 0) != NM_OK)
        goto out;

    for (size_t n = 0; n < NM_LAN_FLD_COUNT; n++) {
        switch (n) {
            case NM_LAN_FLD_LNAME:
                fields_lan[n] = nm_field_regexp_new(
                    n / 2, form_data, "^[a-zA-Z0-9_-]{1,15} *$");
                break;
            case NM_LAN_FLD_RNAME:
                fields_lan[n] = nm_field_regexp_new(
                    n / 2, form_data, "^[a-zA-Z0-9_-]{1,15} *$");
                break;
            default:
                fields_lan[n] = nm_field_label_new(n / 2, form_data);
                break;
        }
    }
    fields_lan[NM_LAN_FLD_COUNT] = NULL;

    nm_lan_labels_setup();
    nm_fields_unset_status(fields_lan);

    form = nm_form_new(form_data, fields_lan);
    nm_form_post(form);

    if (nm_form_draw(&form) != NM_OK)
        goto out;

    if (nm_lan_add_get_data(&l_name, &r_name) != NM_OK)
        goto out;

    nm_net_add_veth(&l_name, &r_name);
    nm_net_link_up(&l_name);
    nm_net_link_up(&r_name);

    nm_str_format(&query, NM_LAN_ADD_VETH_SQL, l_name.data, r_name.data);
    nm_db_edit(query.data);

out:
    wtimeout(action_window, -1);
    werase(help_window);
    nm_init_help_lan();
    nm_form_free(form);
    nm_form_data_free(form_data);
    nm_fields_free(fields_lan);
    nm_str_free(&l_name);
    nm_str_free(&r_name);
    nm_str_free(&query);
}

static size_t nm_lan_labels_setup()
{
    nm_str_t buf = NM_INIT_STR;
    size_t max_label_len = 0;
    size_t msg_len = 0;

    for (size_t n = 0; n < NM_LAN_FLD_COUNT; n++) {
        switch (n) {
        case NM_LAN_LBL_LNAME:
            nm_str_format(&buf, "%s", _(NM_LAN_FORM_LNAME));
            break;
        case NM_LAN_LBL_RNAME:
            nm_str_format(&buf, "%s", _(NM_LAN_FORM_RNAME));
            break;
        default:
            continue;
        }

        msg_len = mbstowcs(NULL, buf.data, buf.len);
        if (msg_len > max_label_len)
            max_label_len = msg_len;

        if (fields_lan[n])
            set_field_buffer(fields_lan[n], 0, buf.data);
    }

    nm_str_free(&buf);
    return max_label_len;
}

static int nm_lan_add_get_data(nm_str_t *ln, nm_str_t *rn)
{
    int rc;
    nm_str_t query = NM_INIT_STR;
    nm_vect_t err = NM_INIT_VECT;
    nm_vect_t names = NM_INIT_VECT;

    nm_get_field_buf(fields_lan[NM_LAN_FLD_LNAME], ln);
    nm_get_field_buf(fields_lan[NM_LAN_FLD_RNAME], rn);

    nm_form_check_datap(_("Name"), ln, err);
    nm_form_check_datap(_("Peer name"), rn, err);

    if ((rc = nm_print_empty_fields(&err)) == NM_ERR) {
        nm_vect_free(&err, NULL);
        goto out;
    }

    nm_str_format(&query, NM_LAN_CHECK_NAME_SQL, ln->data, ln->data);
    nm_db_select(query.data, &names);
    if (names.n_memb > 0) {
        nm_warn(_(NM_MSG_NAME_BUSY));
        rc = NM_ERR;
        goto out;
    }

    nm_str_format(&query, NM_LAN_CHECK_NAME_SQL, rn->data, rn->data);
    nm_db_select(query.data, &names);
    if (names.n_memb > 0) {
        nm_warn(_(NM_MSG_NAME_BUSY));
        rc = NM_ERR;
        goto out;
    }

    if (nm_str_cmp_ss(ln, rn) == NM_OK) {
        nm_warn(_(NM_MSG_NAME_DIFF));
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
            nm_str_append_format(&buf, " %s", nm_vect_str_ctx(&ifs, n));
    else
        nm_str_add_text(&buf, _(" [none]"));

    NM_PR_VM_INFO();
    nm_str_trunc(&buf, 0);

    nm_vect_free(&ifs, nm_str_vect_free_cb);
    nm_str_copy(&buf, &rname);

    nm_str_format(&query, NM_LAN_VETH_INF_SQL, buf.data);
    nm_db_select(query.data, &ifs);

    nm_str_add_char(&buf, ':');
    if (ifs.n_memb > 0)
        for (size_t n = 0; n < ifs.n_memb; n++)
            nm_str_append_format(&buf, " %s", nm_vect_str_ctx(&ifs, n));
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

    if (rn != NULL) {
        cp = strchr(name_copy.data, '>');
        nm_str_alloc_text(rn, ++cp);
        cp = NULL;
    }

    cp = strchr(name_copy.data, '<');
    if (cp) {
        *cp = '\0';
        nm_str_alloc_text(ln, name_copy.data);
    }

    nm_str_free(&name_copy);
}

void nm_lan_create_veth(int info)
{
    nm_vect_t veths = NM_INIT_VECT;
    size_t veth_count, veth_created = 0;

    nm_db_select(NM_GET_VETH_SQL, &veths);
    veth_count = veths.n_memb / 2;

    for (size_t n = 0; n < veth_count; n++) {
        size_t idx_shift = n * 2;
        const nm_str_t *l_name = nm_vect_str(&veths, idx_shift);
        const nm_str_t *r_name = nm_vect_str(&veths, idx_shift + 1);

        if (info)
            printf("Checking \"%s <-> %s\"...", l_name->data, r_name->data);

        if (nm_net_iface_exists(l_name) != NM_OK) {
            if (info)
                printf("\t[not found]\n");

            nm_net_add_veth(l_name, r_name);
            nm_net_link_up(l_name);
            nm_net_link_up(r_name);

            veth_created++;
        } else {
            if (info)
                printf("\t[found]\n");
        }
    }

    if (info && !veth_created)
        printf("Nothing to do.\n");
    else if (info && veth_created)
        printf("%zu VETH interface[s] was created.\n", veth_created);

    nm_vect_free(&veths, nm_str_vect_free_cb);
}

#if defined (NM_WITH_NETWORK_MAP)
static void nm_svg_init_windows(nm_form_t *form)
{
    if (form) {
        nm_form_window_init();
        nm_form_data_t *form_data = (nm_form_data_t *)form_userptr(form);
        if (form_data)
            form_data->parent_window = action_window;
    } else {
        werase(action_window);
        werase(help_window);
    }

    nm_init_help_export();
    nm_init_action(_(NM_MSG_EXPORT_MAP));
    nm_init_side_lan();

    nm_print_veth_menu(NULL, 1);
}

static void nm_lan_export_svg(const nm_vect_t *veths)
{
    nm_form_data_t *form_data = NULL;
    nm_form_t *form = NULL;
    nm_vect_t err = NM_INIT_VECT;
    nm_str_t path = NM_INIT_STR;
    nm_str_t type = NM_INIT_STR;
    nm_str_t layout = NM_INIT_STR;
    nm_str_t group = NM_INIT_STR;
    int state = NM_SVG_STATE_ALL;
    char **states = (char **) nm_form_svg_state;
    size_t msg_len;

    nm_svg_init_windows(NULL);

    msg_len = nm_svg_labels_setup();

    form_data = nm_form_data_new(
        action_window, nm_svg_init_windows, msg_len, NM_SVG_FLD_COUNT / 2, NM_TRUE);

    if (nm_form_data_update(form_data, 0, 0) != NM_OK)
        goto out;

    for (size_t n = 0; n < NM_SVG_FLD_COUNT; n++) {
        switch (n) {
            case NM_SVG_FLD_PATH:
                fields_svg[n] = nm_field_regexp_new(n / 2, form_data, "^/.*");
                break;
            case NM_SVG_FLD_TYPE:
                fields_svg[n] = nm_field_enum_new(
                    n / 2, form_data, nm_form_svg_state, false, false);
                break;
            case NM_SVG_FLD_LAYT:
                fields_svg[n] = nm_field_enum_new(
                    n / 2, form_data, nm_form_svg_layout, false, false);
                break;
            case NM_SVG_FLD_GROUP:
                fields_svg[n] = nm_field_default_new(n / 2, form_data);
                break;
            default:
                fields_svg[n] = nm_field_label_new(n / 2, form_data);
                break;
        }
    }
    fields_svg[NM_SVG_FLD_COUNT] = NULL;

    nm_svg_labels_setup();
    nm_svg_fields_setup();
    nm_fields_unset_status(fields_svg);

    form = nm_form_new(form_data, fields_svg);
    nm_form_post(form);

    if (nm_form_draw(&form) != NM_OK)
        goto out;

    nm_get_field_buf(fields_svg[NM_SVG_FLD_PATH], &path);
    nm_get_field_buf(fields_svg[NM_SVG_FLD_TYPE], &type);
    nm_get_field_buf(fields_svg[NM_SVG_FLD_LAYT], &layout);
    nm_get_field_buf(fields_svg[NM_SVG_FLD_GROUP], &group);
    nm_form_check_data(_(NM_SVG_FORM_PATH), path, err);
    nm_form_check_data(_(NM_SVG_FORM_TYPE), type, err);
    nm_form_check_data(_(NM_SVG_FORM_LAYT), layout, err);

    if (nm_print_empty_fields(&err) == NM_ERR) {
        nm_vect_free(&err, NULL);
        goto out;
    }

    for (size_t n = 0; *states; n++, states++) {
        if (nm_str_cmp_st(&type, *states) == NM_OK) {
            state = n;
            break;
        }
    }

    nm_svg_map(path.data, veths, state, &layout, &group);

out:
    wtimeout(action_window, -1);
    werase(help_window);
    nm_init_help_lan();
    nm_init_action(_(NM_MSG_LAN));
    nm_str_free(&path);
    nm_str_free(&type);
    nm_str_free(&layout);
    nm_str_free(&group);
    nm_form_free(form);
    nm_form_data_free(form_data);
    nm_fields_free(fields_svg);
}

static void nm_svg_fields_setup()
{
    nm_vect_t vgroup = NM_INIT_VECT;
    const char **groups;
    nm_field_type_args_t groups_args = {0};

    nm_db_select(NM_GET_GROUPS_SQL, &vgroup);
    groups = nm_calloc(vgroup.n_memb + 1, sizeof(char *));
    for (size_t n = 0; n < vgroup.n_memb; n++) {
       groups[n] = nm_vect_str_ctx(&vgroup, n);
    }
    groups[vgroup.n_memb] = NULL;

    groups_args.enum_arg.strings = groups;
    groups_args.enum_arg.case_sens = false;
    groups_args.enum_arg.uniq_match = false;

    nm_set_field_type(fields_svg[NM_SVG_FLD_GROUP], NM_FIELD_ENUM, groups_args);

    set_field_buffer(fields_svg[NM_SVG_FLD_TYPE], 0, nm_form_svg_state[0]);
    set_field_buffer(fields_svg[NM_SVG_FLD_LAYT], 0, nm_form_svg_layout[0]);

    if (vgroup.n_memb == 1) {
        field_opts_off(fields_svg[NM_SVG_FLD_GROUP], O_ACTIVE);
    }

    free(groups);
    nm_vect_free(&vgroup, nm_str_vect_free_cb);
}

static size_t nm_svg_labels_setup()
{
    nm_str_t buf = NM_INIT_STR;
    size_t max_label_len = 0;
    size_t msg_len = 0;

    for (size_t n = 0; n < NM_SVG_FLD_COUNT; n++) {
        switch (n) {
        case NM_SVG_LBL_PATH:
            nm_str_format(&buf, "%s", _(NM_SVG_FORM_PATH));
            break;
        case NM_SVG_LBL_TYPE:
            nm_str_format(&buf, "%s", _(NM_SVG_FORM_TYPE));
            break;
        case NM_SVG_LBL_LAYT:
            nm_str_format(&buf, "%s", _(NM_SVG_FORM_LAYT));
            break;
        case NM_SVG_LBL_GROUP:
            nm_str_format(&buf, "%s", _(NM_SVG_FORM_GROUP));
            break;
        default:
            continue;
        }

        msg_len = mbstowcs(NULL, buf.data, buf.len);
        if (msg_len > max_label_len)
            max_label_len = msg_len;

        if (fields_svg[n])
            set_field_buffer(fields_svg[n], 0, buf.data);
    }

    nm_str_free(&buf);
    return max_label_len;
}

#endif /* NM_WITH_NETWORK_MAP */
#endif /* NM_OS_LINUX */
/* vim:set ts=4 sw=4: */
