#include <nm_core.h>
#include <nm_form.h>
#include <nm_menu.h>
#include <nm_utils.h>
#include <nm_window.h>
#include <nm_network.h>
#include <nm_database.h>
#include <nm_vm_control.h>

#if defined (NM_OS_LINUX)
#define NM_NET_FIELDS_NUM 6
#else
#define NM_NET_FIELDS_NUM 3
#endif

#if defined (NM_OS_LINUX)
#define NM_INIT_NET_IF { NM_INIT_STR, NM_INIT_STR, \
                         NM_INIT_STR, NM_INIT_STR, \
                         NM_INIT_STR, NM_INIT_STR, \
                         NM_INIT_STR }
#else
#define NM_INIT_NET_IF { NM_INIT_STR, NM_INIT_STR, \
                         NM_INIT_STR, NM_INIT_STR }
#endif

#define NM_INIT_SEL_IF { NULL, 0 }
#define NM_NET_MACVTAP_NUM 2

typedef struct {
    nm_str_t name;
    nm_str_t drv;
    nm_str_t maddr;
    nm_str_t ipv4;
#if defined (NM_OS_LINUX)
    nm_str_t vhost;
    nm_str_t macvtap;
    nm_str_t parent_eth;
#endif
} nm_iface_t;

typedef struct {
    char *name;
    size_t if_idx;
} nm_sel_iface_t;

static nm_field_t *fields[NM_NET_FIELDS_NUM + 1];

static void nm_edit_net_field_names(nm_window_t *w);
static void nm_edit_net_field_setup(const nm_vmctl_data_t *vm,
                                    size_t if_idx);
static int nm_edit_net_get_data(const nm_str_t *name, nm_iface_t *ifp);
static void nm_edit_net_update_db(const nm_str_t *name, nm_iface_t *ifp);
static inline void nm_edit_net_iface_free(nm_iface_t *ifp);
static int nm_edit_net_maddr_busy(const nm_str_t *mac);
static int nm_edit_net_action(const nm_str_t *name,
                              const nm_vmctl_data_t *vm, size_t if_idx);

static const char *nm_form_msg[] = {
    "Net driver",
    "Mac address",
    "IPv4 address",
#if defined (NM_OS_LINUX)
    "Enable vhost",
    "Enable MacVTap",
    "MacVTap iface",
#endif
    NULL
};

static nm_form_t *form = NULL;

enum {
    NM_FLD_NDRV = 0,
    NM_FLD_MADR,
    NM_FLD_IPV4,
    NM_FLD_VHST,
    NM_FLD_MTAP,
    NM_FLD_PETH
};

void nm_edit_net(const nm_str_t *name, nm_vmctl_data_t *vm)
{
    int ch;
    nm_menu_data_t ifs = NM_INIT_MENU_DATA;
    nm_vect_t ifaces = NM_INIT_VECT;
    size_t vm_list_len = (getmaxy(side_window) - 4);
    size_t iface_count = vm->ifs.n_memb / NM_IFS_IDX_COUNT;
    
    ifs.highlight = 1;
    if (vm_list_len < iface_count)
        ifs.item_last = vm_list_len;
    else
        ifs.item_last = vm_list_len = iface_count;

    for (size_t n = 0; n < iface_count; n++)
    {
        size_t idx_shift = NM_IFS_IDX_COUNT * n;
        nm_vect_insert(&ifaces,
                       nm_vect_str_ctx(&vm->ifs, NM_SQL_IF_NAME + idx_shift),
                       nm_vect_str_len(&vm->ifs, NM_SQL_IF_NAME + idx_shift) + 1,
                       NULL);
    }

    ifs.v = &ifaces;
    do {
        if ((ch == KEY_UP) && (ifs.highlight == 1) && (ifs.item_first == 0) &&
            (vm_list_len < ifs.v->n_memb))
        {
            ifs.highlight = vm_list_len;
            ifs.item_first = ifs.v->n_memb - vm_list_len;
            ifs.item_last = ifs.v->n_memb;
        }

        else if (ch == KEY_UP)
        {
            if ((ifs.highlight == 1) && (ifs.v->n_memb <= vm_list_len))
                ifs.highlight = ifs.v->n_memb;
            else if ((ifs.highlight == 1) && (ifs.item_first != 0))
            {
                ifs.item_first--;
                ifs.item_last--;
            }
            else
            {
                ifs.highlight--;
            }
        }

        else if ((ch == KEY_DOWN) && (ifs.highlight == vm_list_len) &&
                 (ifs.item_last == ifs.v->n_memb))
        {
            ifs.highlight = 1;
            ifs.item_first = 0;
            ifs.item_last = vm_list_len;
        }

        else if (ch == KEY_DOWN)
        {
            if ((ifs.highlight == ifs.v->n_memb) && (ifs.v->n_memb <= vm_list_len))
                ifs.highlight = 1;
            else if ((ifs.highlight == vm_list_len) && (ifs.item_last < ifs.v->n_memb))
            {
                ifs.item_first++;
                ifs.item_last++;
            }
            else
            {
                ifs.highlight++;
            }
        }

        else if (ch == KEY_HOME)
        {
            ifs.highlight = 1;
            ifs.item_first = 0;
            ifs.item_last = vm_list_len;
        }

        else if (ch == KEY_END)
        {
            ifs.highlight = vm_list_len;
            ifs.item_first = ifs.v->n_memb - vm_list_len;
            ifs.item_last = ifs.v->n_memb;
        }

        else if (ch == NM_KEY_ENTER)
        {
            werase(action_window);
            nm_init_action(_(NM_MSG_IF_PROP));

            if (nm_edit_net_action(name, vm, ifs.highlight) == NM_OK)
            {
                nm_vmctl_free_data(vm);
                nm_vmctl_get_data(name, vm);
            }
        }

        nm_print_iface_menu(&ifs);
        werase(action_window);
        nm_init_action(_(NM_MSG_IF_PROP));
        nm_print_iface_info(vm, ifs.highlight);

        if (redraw_window)
        {
            nm_destroy_windows();
            endwin();
            refresh();
            nm_create_windows();
            nm_init_help_iface();
            nm_init_side_if_list();
            nm_init_action(_(NM_MSG_IF_PROP));

            vm_list_len = (getmaxy(side_window) - 4);
            /* TODO save last pos */
            if (vm_list_len < iface_count)
            {
                ifs.item_last = vm_list_len;
                ifs.item_first = 0;
                ifs.highlight = 1;
            }
            else
                ifs.item_last = vm_list_len = iface_count;

            redraw_window = 0;
        }
    } while ((ch = wgetch(action_window)) != NM_KEY_Q);

    nm_vect_free(&ifaces, NULL);
}

static int
nm_edit_net_action(const nm_str_t *name, const nm_vmctl_data_t *vm, size_t if_idx)
{
    int rc = NM_OK;
    size_t msg_len = nm_max_msg_len(nm_form_msg);
    nm_iface_t iface_data = NM_INIT_NET_IF;
    nm_form_data_t form_data = NM_INIT_FORM_DATA;
    size_t idx_shift;

    assert(if_idx > 0);
    idx_shift = NM_IFS_IDX_COUNT * (--if_idx);
    if_idx++; /* restore idx */

    nm_str_alloc_str(&iface_data.name,
            nm_vect_str(&vm->ifs, NM_SQL_IF_NAME + idx_shift));

    assert(iface_data.name.len > 0);

    werase(help_window);
    nm_init_help_edit();

    if (nm_form_calc_size(msg_len, NM_NET_FIELDS_NUM, &form_data) != NM_OK)
        return NM_ERR;

    for (size_t n = 0; n < NM_NET_FIELDS_NUM; ++n)
        fields[n] = new_field(1, form_data.form_len, n * 2, 0, 0, 0);

    fields[NM_NET_FIELDS_NUM] = NULL;

    nm_edit_net_field_setup(vm, if_idx);
    nm_edit_net_field_names(form_data.form_window);

    form = nm_post_form__(form_data.form_window, fields, msg_len + 4, NM_TRUE);

    if (nm_draw_form(action_window, form) != NM_OK)
    {
        rc = NM_ERR;
        goto out;
    }

    if (nm_edit_net_get_data(name, &iface_data) != NM_OK)
        goto out;

    nm_edit_net_update_db(name, &iface_data);

out:
    nm_edit_net_iface_free(&iface_data);
    nm_form_free(form, fields);
    delwin(form_data.form_window);
    werase(help_window);
    nm_init_help_iface();

    return rc;
}

static void nm_edit_net_field_setup(const nm_vmctl_data_t *vm, size_t if_idx)
{
    size_t mvtap_idx = 0;
    size_t idx_shift;

    assert(if_idx > 0);
    idx_shift = NM_IFS_IDX_COUNT * (--if_idx);

    set_field_type(fields[NM_FLD_NDRV], TYPE_ENUM, nm_form_net_drv, false, false);
    set_field_type(fields[NM_FLD_MADR], TYPE_REGEXP, ".*");
    set_field_type(fields[NM_FLD_IPV4], TYPE_REGEXP, ".*");
#if defined (NM_OS_LINUX)
    set_field_type(fields[NM_FLD_VHST], TYPE_ENUM, nm_form_yes_no, false, false);
    set_field_type(fields[NM_FLD_MTAP], TYPE_ENUM, nm_form_macvtap, false, false);
    set_field_type(fields[NM_FLD_PETH], TYPE_REGEXP, ".*");
#endif

    field_opts_off(fields[NM_FLD_MADR], O_STATIC);
    field_opts_off(fields[NM_FLD_IPV4], O_STATIC);
#if defined (NM_OS_LINUX)
    field_opts_off(fields[NM_FLD_PETH], O_STATIC);
#endif

    set_field_buffer(fields[NM_FLD_NDRV], 0,
        nm_vect_str_ctx(&vm->ifs, NM_SQL_IF_DRV + idx_shift));
    set_field_buffer(fields[NM_FLD_MADR], 0,
        nm_vect_str_ctx(&vm->ifs, NM_SQL_IF_MAC + idx_shift));
    if (nm_vect_str_len(&vm->ifs, NM_SQL_IF_IP4 + idx_shift) > 0)
    {
        set_field_buffer(fields[NM_FLD_IPV4], 0,
            nm_vect_str_ctx(&vm->ifs, NM_SQL_IF_IP4 + idx_shift));
    }
#if defined (NM_OS_LINUX)
    set_field_buffer(fields[NM_FLD_VHST], 0,
        (nm_str_cmp_st(nm_vect_str(&vm->ifs, NM_SQL_IF_VHO + idx_shift),
            NM_ENABLE) == NM_OK) ? "yes" : "no");

    mvtap_idx = nm_str_stoui(nm_vect_str(&vm->ifs, NM_SQL_IF_MVT + idx_shift), 10);
    if (mvtap_idx > NM_NET_MACVTAP_NUM)
        nm_bug("%s: invalid macvtap array index: %zu", __func__, mvtap_idx);
    set_field_buffer(fields[NM_FLD_MTAP], 0, nm_form_macvtap[mvtap_idx]);
    if (nm_vect_str_len(&vm->ifs, NM_SQL_IF_PET + idx_shift) > 0)
    {
        set_field_buffer(fields[NM_FLD_PETH], 0,
            nm_vect_str_ctx(&vm->ifs, NM_SQL_IF_PET + idx_shift));
    }
#else
    (void) mvtap_idx;
#endif

    for (size_t n = 0; n < NM_NET_FIELDS_NUM; n++)
        set_field_status(fields[n], 0);
}

static void nm_edit_net_field_names(nm_window_t *w)
{
    int y = 1, x = 2, mult = 2;

    for (size_t n = 0; n < NM_NET_FIELDS_NUM; n++)
    {
        mvwaddstr(w, y, x, _(nm_form_msg[n]));
        y += mult;
    }
}

static int nm_edit_net_get_data(const nm_str_t *name, nm_iface_t *ifp)
{
    int rc = NM_OK;
    nm_vect_t err = NM_INIT_VECT;

    nm_get_field_buf(fields[NM_FLD_NDRV], &ifp->drv);
    nm_get_field_buf(fields[NM_FLD_MADR], &ifp->maddr);
    nm_get_field_buf(fields[NM_FLD_IPV4], &ifp->ipv4);
#if defined (NM_OS_LINUX)
    nm_get_field_buf(fields[NM_FLD_VHST], &ifp->vhost);
    nm_get_field_buf(fields[NM_FLD_MTAP], &ifp->macvtap);
    nm_get_field_buf(fields[NM_FLD_PETH], &ifp->parent_eth);
#endif

    if (field_status(fields[NM_FLD_NDRV]))
        nm_form_check_data(_("Net driver"), ifp->drv, err);
    if (field_status(fields[NM_FLD_MADR]))
        nm_form_check_data(_("Mac address"), ifp->maddr, err);
#if defined (NM_OS_LINUX)
    if (field_status(fields[NM_FLD_VHST]))
        nm_form_check_data(_("Enable vhost"), ifp->vhost, err);
    if (field_status(fields[NM_FLD_MTAP]))
        nm_form_check_data(_("Enable MacVTap"), ifp->macvtap, err);
#endif

    if ((rc = nm_print_empty_fields(&err)) == NM_ERR)
        goto out;

    if (field_status(fields[NM_FLD_MADR]))
    {
        if (nm_net_verify_mac(&ifp->maddr) != NM_OK)
        {
            nm_print_warn(3, 2, _("Invalid mac address"));
            rc = NM_ERR;
            goto out;
        }

        if (nm_edit_net_maddr_busy(&ifp->maddr) != NM_OK)
        {
            nm_print_warn(3, 2, _("This mac is already used"));
            rc = NM_ERR;
            goto out;
        }
    }

    if ((field_status(fields[NM_FLD_IPV4])) && (ifp->ipv4.len > 0))
    {
        nm_str_t err_msg = NM_INIT_STR;
        if (nm_net_verify_ipaddr4(&ifp->ipv4, NULL, &err_msg) != NM_OK)
        {
            nm_print_warn(3, 2, err_msg.data);
            rc = NM_ERR;
            goto out;
        }
        nm_str_free(&err_msg);
    }

#if defined (NM_OS_LINUX)
    /* Do not allow to enable vhost on non virtio net device */
    if ((field_status(fields[NM_FLD_VHST])) &&
        (nm_str_cmp_st(&ifp->vhost, "yes") == NM_OK))
    {
        int vhost_ok = 1;

        if (field_status(fields[NM_FLD_NDRV]))
        {
            if (nm_str_cmp_st(&ifp->drv, NM_DEFAULT_NETDRV) != NM_OK)
                vhost_ok = 0;
        }
        else
        {
            nm_str_t query = NM_INIT_STR;
            nm_vect_t netv = NM_INIT_VECT;

            nm_str_format(&query,
                "SELECT id FROM ifaces WHERE vm_name='%s' AND if_name='%s' AND if_drv='%s'",
                name->data, ifp->name.data, NM_DEFAULT_NETDRV);
            nm_db_select(query.data, &netv);

            if (netv.n_memb == 0)
                vhost_ok = 0;

            nm_vect_free(&netv, nm_str_vect_free_cb);
            nm_str_free(&query);
        }

        if (!vhost_ok)
        {
            rc = NM_ERR;
            nm_print_warn(3, 2, _("vhost can be enabled only on virtio-net"));
        }
    }

    /* Check for MacVTap parent interface exists */
    if (field_status(fields[NM_FLD_PETH]) && (ifp->parent_eth.len > 0))
    {
        if (nm_net_iface_exists(&ifp->parent_eth) != NM_OK)
        {
            rc = NM_ERR;
            nm_print_warn(3, 2, _("MacVTap parent interface does not exists"));
        }
    }
#else
    (void) name;
    (void) ifname;
#endif /* NM_OS_LINUX */

out:
    nm_vect_free(&err, NULL);

    return rc;
}

static void nm_edit_net_update_db(const nm_str_t *name, nm_iface_t *ifp)
{
    nm_str_t query = NM_INIT_STR;

    if (field_status(fields[NM_FLD_NDRV]))
    {
        nm_str_alloc_text(&query, "UPDATE ifaces SET if_drv='");
        nm_str_format(&query, "%s' WHERE vm_name='%s' AND if_name='%s'",
            ifp->drv.data, name->data, ifp->name.data);
        nm_db_edit(query.data);
        nm_str_trunc(&query, 0);

#if defined (NM_OS_LINUX)
        /* disable vhost if driver is not virtio-net */
        if (nm_str_cmp_st(&ifp->drv, NM_DEFAULT_NETDRV) != NM_OK)
        {
            nm_str_alloc_text(&query, "UPDATE ifaces SET vhost='0' ");
            nm_str_format(&query, "WHERE vm_name='%s' AND if_name='%s'",
                name->data, ifp->name.data);
            nm_db_edit(query.data);
            nm_str_trunc(&query, 0);
        }
#endif
    }

    if (field_status(fields[NM_FLD_MADR]))
    {
        nm_str_add_text(&query, "UPDATE ifaces SET mac_addr='");
        nm_str_format(&query, "%s' WHERE vm_name='%s' AND if_name='%s'",
            ifp->maddr.data, name->data, ifp->name.data);
        nm_db_edit(query.data);
        nm_str_trunc(&query, 0);
    }

    if (field_status(fields[NM_FLD_IPV4]))
    {
        nm_str_add_text(&query, "UPDATE ifaces SET ipv4_addr='");
        nm_str_format(&query, "%s' WHERE vm_name='%s' AND if_name='%s'",
            ifp->ipv4.data, name->data, ifp->name.data);
        nm_db_edit(query.data);
        nm_str_trunc(&query, 0);
    }

#if defined (NM_OS_LINUX)
    if (field_status(fields[NM_FLD_VHST]))
    {
        nm_str_alloc_text(&query, "UPDATE ifaces SET vhost='");
        nm_str_format(&query, "%s' WHERE vm_name='%s' AND if_name='%s'",
            (nm_str_cmp_st(&ifp->vhost, "yes") == NM_OK) ? NM_ENABLE : NM_DISABLE,
            name->data, ifp->name.data);
        nm_db_edit(query.data);
        nm_str_trunc(&query, 0);
    }

    if (field_status(fields[NM_FLD_MTAP]))
    {
        ssize_t macvtap_idx = -1;
        const char **p = nm_form_macvtap;

        for (ssize_t n = 0; *p; p++, n++)
        {
            if (nm_str_cmp_st(&ifp->macvtap, *p) == NM_OK)
            {
                macvtap_idx = n;
                break;
            }
        }

        if (macvtap_idx == -1)
            nm_bug("%s: macvtap_idx is not found", __func__);

        nm_str_alloc_text(&query, "UPDATE ifaces SET macvtap='");
        nm_str_format(&query, "%zd' WHERE vm_name='%s' AND if_name='%s'",
            macvtap_idx, name->data, ifp->name.data);
        nm_db_edit(query.data);
        nm_str_trunc(&query, 0);
    }

    if (field_status(fields[NM_FLD_PETH]))
    {
        nm_str_alloc_text(&query, "UPDATE ifaces SET parent_eth='");
        nm_str_format(&query, "%s' WHERE vm_name='%s' AND if_name='%s'",
            ifp->parent_eth.data, name->data, ifp->name.data);
        nm_db_edit(query.data);
    }
#endif

    nm_str_free(&query);
}

static inline void nm_edit_net_iface_free(nm_iface_t *ifp)
{
    nm_str_free(&ifp->name);
    nm_str_free(&ifp->drv);
    nm_str_free(&ifp->maddr);
    nm_str_free(&ifp->ipv4);
#if defined (NM_OS_LINUX)
    nm_str_free(&ifp->vhost);
    nm_str_free(&ifp->macvtap);
    nm_str_free(&ifp->parent_eth);
#endif
}

/* TODO add this check in all genmaddr points */
static int nm_edit_net_maddr_busy(const nm_str_t *mac)
{
    int rc = NM_OK;
    nm_vect_t maddrs = NM_INIT_VECT;

    nm_db_select("SELECT mac_addr FROM ifaces", &maddrs);

    for (size_t n = 0; n < maddrs.n_memb; n++)
    {
        if (nm_str_cmp_ss(mac, nm_vect_str(&maddrs, n)) == NM_OK)
        {
            rc = NM_ERR;
            break;
        }
    }

    nm_vect_free(&maddrs, nm_str_vect_free_cb);

    return rc;
}

/* vim:set ts=4 sw=4 fdm=marker: */
