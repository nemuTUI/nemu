#include <nm_core.h>
#include <nm_form.h>
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
                         NM_INIT_STR, NM_INIT_STR }
#else
#define NM_INIT_NET_IF { NM_INIT_STR, NM_INIT_STR, \
                         NM_INIT_STR }
#endif

#define NM_INIT_SEL_IF { NULL, 0 }
#define NM_NET_MACVTAP_NUM 2

typedef struct {
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

static void nm_edit_net_field_setup(const nm_vmctl_data_t *vm,
                                    const nm_sel_iface_t *iface);
static void nm_edit_net_field_names(const nm_str_t *name, nm_window_t *w,
                                    const nm_sel_iface_t *ifname);
static int nm_edit_net_get_data(const nm_str_t *name, nm_iface_t *ifp,
                                const nm_sel_iface_t *ifname);
static void nm_edit_net_update_db(const nm_str_t *name, nm_iface_t *ifp,
                                  const nm_sel_iface_t *ifname);
static inline void nm_edit_net_iface_free(nm_iface_t *ifp);
static int nm_edit_net_maddr_busy(const nm_str_t *mac);
static void nm_edit_net_select_iface(const nm_vmctl_data_t *vm, size_t ifcnt,
                                     nm_sel_iface_t *ifp);

enum {
    NM_FLD_NDRV = 0,
    NM_FLD_MADR,
    NM_FLD_IPV4,
    NM_FLD_VHST,
    NM_FLD_MTAP,
    NM_FLD_PETH
};

void nm_edit_net(const nm_str_t *name, const nm_vmctl_data_t *vm)
{
    nm_form_t *form = NULL;
    nm_window_t *window = NULL;
    nm_spinner_data_t sp_data = NM_INIT_SPINNER;
    nm_iface_t iface = NM_INIT_NET_IF;
    nm_sel_iface_t sel_iface = NM_INIT_SEL_IF;
    size_t msg_len;
    pthread_t spin_th;
    int done = 0, mult = 2;
    size_t iface_count = vm->ifs.n_memb / NM_IFS_IDX_COUNT;

    if (iface_count > 1)
    {
        nm_edit_net_select_iface(vm, iface_count, &sel_iface);
        if (sel_iface.name == NULL)
            goto out;
    }
    else
    {
        sel_iface.name = nm_vect_str_ctx(&vm->ifs, NM_SQL_IF_NAME);
    }

    nm_print_title(_(NM_EDIT_TITLE));
    if (getmaxy(stdscr) <= 28)
        mult = 1;

#if defined (NM_OS_LINUX)
    window = nm_init_window((mult == 2) ? 18 : 11, 51, 3);
#else
    window = nm_init_window((mult == 2) ? 12 : 8, 51, 3);
#endif

    init_pair(1, COLOR_BLACK, COLOR_WHITE);
    wbkgd(window, COLOR_PAIR(1));

    for (size_t n = 0; n < NM_NET_FIELDS_NUM; ++n)
    {
        fields[n] = new_field(1, 27, ((n + 1) * mult) + 1, 3, 0, 0);
        set_field_back(fields[n], A_UNDERLINE);
    }

    fields[NM_NET_FIELDS_NUM] = NULL;

    nm_edit_net_field_setup(vm, &sel_iface);
    nm_edit_net_field_names(name, window, &sel_iface);

    form = nm_post_form(window, fields, 18);
    if (nm_draw_form(window, form) != NM_OK)
        goto out;

    if (nm_edit_net_get_data(name, &iface, &sel_iface) != NM_OK)
        goto out;

    msg_len = mbstowcs(NULL, _(NM_EDIT_TITLE), strlen(_(NM_EDIT_TITLE)));
    sp_data.stop = &done;
    sp_data.x = (getmaxx(stdscr) + msg_len + 2) / 2;

    if (pthread_create(&spin_th, NULL, nm_spinner, (void *) &sp_data) != 0)
        nm_bug(_("%s: cannot create thread"), __func__);

    nm_edit_net_update_db(name, &iface, &sel_iface);

    done = 1;
    if (pthread_join(spin_th, NULL) != 0)
        nm_bug(_("%s: cannot join thread"), __func__);
out:
    nm_form_free(form, fields);
    nm_edit_net_iface_free(&iface);
}

static void nm_edit_net_select_iface(const nm_vmctl_data_t *vm, size_t ifcnt,
                                     nm_sel_iface_t *ifp)
{
    nm_vect_t ifaces = NM_INIT_VECT;
    nm_form_t *form = NULL;
    nm_window_t *window = NULL;
    nm_field_t *if_fields[2] = {NULL};
    nm_vect_t err = NM_INIT_VECT;
    nm_str_t buf = NM_INIT_STR;

    for (size_t n = 0; n < ifcnt; n++)
    {
        size_t idx_shift = NM_IFS_IDX_COUNT * n;
        nm_vect_insert(&ifaces,
                       nm_vect_str_ctx(&vm->ifs, NM_SQL_IF_NAME + idx_shift),
                       nm_vect_str_len(&vm->ifs, NM_SQL_IF_NAME + idx_shift) + 1,
                       NULL);
    }

    nm_vect_end_zero(&ifaces);

    nm_print_title(_(NM_EDIT_TITLE));
    window = nm_init_window(7, 45, 3);
    init_pair(1, COLOR_BLACK, COLOR_WHITE);
    wbkgd(window, COLOR_PAIR(1));

    if_fields[0] = new_field(1, 30, 2, 1, 0, 0);
    set_field_back(if_fields[0], A_UNDERLINE);
    if_fields[1] = NULL;

    set_field_type(if_fields[0], TYPE_ENUM, ifaces.data, false, false);
    set_field_buffer(if_fields[0], 0, *ifaces.data);

    mvwaddstr(window, 1, 2, _("Select interface"));
    mvwaddstr(window, 4, 2, _("Interface"));

    form = nm_post_form(window, if_fields, 11);
    if (nm_draw_form(window, form) != NM_OK)
        goto out;

    nm_get_field_buf(if_fields[0], &buf);
    nm_form_check_data(_("Interface"), buf, err);

    if (nm_print_empty_fields(&err) == NM_ERR)
    {
        nm_vect_free(&err, NULL);
        goto out;
    }

    for (size_t n = 0; n < ifcnt; n++)
    {
        size_t idx_shift = NM_IFS_IDX_COUNT * n;
        char *i = nm_vect_str_ctx(&vm->ifs, NM_SQL_IF_NAME + idx_shift);
        if (nm_str_cmp_st(&buf, i) == NM_OK)
        {
            ifp->name = i;
            ifp->if_idx = idx_shift;
            break;
        }
    }

out:
    nm_vect_free(&ifaces, NULL);
    nm_form_free(form, if_fields);
    nm_str_free(&buf);
}

static void nm_edit_net_field_setup(const nm_vmctl_data_t *vm, const nm_sel_iface_t *iface)
{
    size_t mvtap_idx = 0;

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
        nm_vect_str_ctx(&vm->ifs, NM_SQL_IF_DRV + iface->if_idx));
    set_field_buffer(fields[NM_FLD_MADR], 0,
        nm_vect_str_ctx(&vm->ifs, NM_SQL_IF_MAC + iface->if_idx));
    if (nm_vect_str_len(&vm->ifs, NM_SQL_IF_IP4 + iface->if_idx) > 0)
    {
        set_field_buffer(fields[NM_FLD_IPV4], 0,
            nm_vect_str_ctx(&vm->ifs, NM_SQL_IF_IP4 + iface->if_idx));
    }
#if defined (NM_OS_LINUX)
    set_field_buffer(fields[NM_FLD_VHST], 0,
        (nm_str_cmp_st(nm_vect_str(&vm->ifs, NM_SQL_IF_VHO + iface->if_idx),
            NM_ENABLE) == NM_OK) ? "yes" : "no");

    mvtap_idx = nm_str_stoui(nm_vect_str(&vm->ifs, NM_SQL_IF_MVT + iface->if_idx), 10);
    if (mvtap_idx > NM_NET_MACVTAP_NUM)
        nm_bug("%s: invalid macvtap array index: %zu", __func__, mvtap_idx);
    set_field_buffer(fields[NM_FLD_MTAP], 0, nm_form_macvtap[mvtap_idx]);
    if (nm_vect_str_len(&vm->ifs, NM_SQL_IF_PET + iface->if_idx) > 0)
    {
        set_field_buffer(fields[NM_FLD_PETH], 0,
            nm_vect_str_ctx(&vm->ifs, NM_SQL_IF_PET + iface->if_idx));
    }
#else
    (void) mvtap_idx;
#endif

    for (size_t n = 0; n < NM_NET_FIELDS_NUM; n++)
        set_field_status(fields[n], 0);
}

static void nm_edit_net_field_names(const nm_str_t *name, nm_window_t *w,
                                    const nm_sel_iface_t *ifname)
{
    int y = 3, mult = 2;
    nm_str_t buf = NM_INIT_STR;

    if (getmaxy(stdscr) <= 28)
        mult = 1;

    nm_str_alloc_str(&buf, name);
    nm_str_add_text(&buf, _(" network settings:"));

    mvwaddstr(w, 1,         2,  buf.data);
    mvwaddstr(w, 2,         2,  ifname->name);
    mvwaddstr(w, y += mult, 2, _("Net driver"));
    mvwaddstr(w, y += mult, 2, _("Mac address"));
    mvwaddstr(w, y += mult, 2, _("IPv4 address"));
#if defined (NM_OS_LINUX)
    mvwaddstr(w, y += mult, 2, _("Enable vhost"));
    mvwaddstr(w, y += mult, 2, _("Enable MacVTap"));
    mvwaddstr(w, y += mult, 2, _("MacVTap iface"));
#endif

    nm_str_free(&buf);
}

static int nm_edit_net_get_data(const nm_str_t *name, nm_iface_t *ifp,
                                const nm_sel_iface_t *ifname)
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
                name->data, ifname->name, NM_DEFAULT_NETDRV);
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

static void nm_edit_net_update_db(const nm_str_t *name, nm_iface_t *ifp,
                                  const nm_sel_iface_t *ifname)
{
    nm_str_t query = NM_INIT_STR;

    if (field_status(fields[NM_FLD_NDRV]))
    {
        nm_str_alloc_text(&query, "UPDATE ifaces SET if_drv='");
        nm_str_format(&query, "%s' WHERE vm_name='%s' AND if_name='%s'",
            ifp->drv.data, name->data, ifname->name);
        nm_db_edit(query.data);
        nm_str_trunc(&query, 0);

#if defined (NM_OS_LINUX)
        /* disable vhost if driver is not virtio-net */
        if (nm_str_cmp_st(&ifp->drv, NM_DEFAULT_NETDRV) != NM_OK)
        {
            nm_str_alloc_text(&query, "UPDATE ifaces SET vhost='0' ");
            nm_str_format(&query, "WHERE vm_name='%s' AND if_name='%s'",
                name->data, ifname->name);
            nm_db_edit(query.data);
            nm_str_trunc(&query, 0);
        }
#endif
    }

    if (field_status(fields[NM_FLD_MADR]))
    {
        nm_str_add_text(&query, "UPDATE ifaces SET mac_addr='");
        nm_str_format(&query, "%s' WHERE vm_name='%s' AND if_name='%s'",
            ifp->maddr.data, name->data, ifname->name);
        nm_db_edit(query.data);
        nm_str_trunc(&query, 0);
    }

    if (field_status(fields[NM_FLD_IPV4]))
    {
        nm_str_add_text(&query, "UPDATE ifaces SET ipv4_addr='");
        nm_str_format(&query, "%s' WHERE vm_name='%s' AND if_name='%s'",
            ifp->ipv4.data, name->data, ifname->name);
        nm_db_edit(query.data);
        nm_str_trunc(&query, 0);
    }

#if defined (NM_OS_LINUX)
    if (field_status(fields[NM_FLD_VHST]))
    {
        nm_str_alloc_text(&query, "UPDATE ifaces SET vhost='");
        nm_str_format(&query, "%s' WHERE vm_name='%s' AND if_name='%s'",
            (nm_str_cmp_st(&ifp->vhost, "yes") == NM_OK) ? NM_ENABLE : NM_DISABLE,
            name->data, ifname->name);
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
            macvtap_idx, name->data, ifname->name);
        nm_db_edit(query.data);
        nm_str_trunc(&query, 0);
    }

    if (field_status(fields[NM_FLD_PETH]))
    {
        nm_str_alloc_text(&query, "UPDATE ifaces SET parent_eth='");
        nm_str_format(&query, "%s' WHERE vm_name='%s' AND if_name='%s'",
            ifp->parent_eth.data, name->data, ifname->name);
        nm_db_edit(query.data);
    }
#endif

    nm_str_free(&query);
}

static inline void nm_edit_net_iface_free(nm_iface_t *ifp)
{
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
