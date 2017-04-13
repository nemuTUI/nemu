#include <nm_core.h>
#include <nm_form.h>
#include <nm_utils.h>
#include <nm_window.h>
#include <nm_network.h>
#include <nm_database.h>
#include <nm_vm_control.h>

#define NM_NET_FIELDS_NUM 4
#define NM_INIT_NET_IF { NM_INIT_STR, NM_INIT_STR, \
                         NM_INIT_STR, NM_INIT_STR }

typedef struct {
    nm_str_t name;
    nm_str_t drv;
    nm_str_t maddr;
    nm_str_t ipv4;
} nm_iface_t;

static nm_field_t *fields[NM_NET_FIELDS_NUM + 1];

static void nm_edit_net_field_setup(const nm_vmctl_data_t *vm);
static void nm_edit_net_field_names(const nm_str_t *name, nm_window_t *w);
static int nm_edit_net_get_data(nm_iface_t *ifp);
static inline void nm_edit_net_iface_free(nm_iface_t *ifp);

enum {
    NM_FLD_INTN = 0,
    NM_FLD_NDRV,
    NM_FLD_MADR,
    NM_FLD_IPV4
};

void nm_edit_net(const nm_str_t *name, const nm_vmctl_data_t *vm)
{
    nm_form_t *form = NULL;
    nm_window_t *window = NULL;
    nm_spinner_data_t sp_data = NM_INIT_SPINNER;
    nm_iface_t iface = NM_INIT_NET_IF;
    size_t msg_len;
    pthread_t spin_th;
    int done = 0;

    nm_print_title(_(NM_EDIT_TITLE));
    window = nm_init_window(13, 51, 3);

    init_pair(1, COLOR_BLACK, COLOR_WHITE);
    wbkgd(window, COLOR_PAIR(1));

    for (size_t n = 0; n < NM_NET_FIELDS_NUM; ++n)
    {
        fields[n] = new_field(1, 27, (n + 1) * 2, 3, 0, 0);
        set_field_back(fields[n], A_UNDERLINE);
    }

    fields[NM_NET_FIELDS_NUM] = NULL;

    nm_edit_net_field_setup(vm);
    nm_edit_net_field_names(name, window);

    form = nm_post_form(window, fields, 18);
    if (nm_draw_form(window, form) != NM_OK)
        goto out;

    if (nm_edit_net_get_data(&iface) != NM_OK)
        goto out;
out:
    nm_form_free(form, fields);
    nm_edit_net_iface_free(&iface);
}

static void nm_edit_net_field_setup(const nm_vmctl_data_t *vm)
{
    nm_vect_t ifaces = NM_INIT_VECT;
    size_t iface_count = vm->ifs.n_memb / 4;

    for (size_t n = 0; n < iface_count; n++)
    {
        size_t idx_shift = 4 * n;
        nm_vect_insert(&ifaces,
                       nm_vect_str_ctx(&vm->ifs, NM_SQL_IF_NAME + idx_shift),
                       nm_vect_str_len(&vm->ifs, NM_SQL_IF_NAME + idx_shift) + 1,
                       NULL);
    }

    nm_vect_end_zero(&ifaces);

    set_field_type(fields[NM_FLD_INTN], TYPE_ENUM, ifaces.data, false, false);
    set_field_type(fields[NM_FLD_NDRV], TYPE_ENUM, nm_form_net_drv, false, false);
    set_field_type(fields[NM_FLD_MADR], TYPE_REGEXP, ".*");
    set_field_type(fields[NM_FLD_IPV4], TYPE_REGEXP, ".*");

    field_opts_off(fields[NM_FLD_MADR], O_STATIC);
    field_opts_off(fields[NM_FLD_IPV4], O_STATIC);

    set_field_buffer(fields[NM_FLD_INTN], 0, *ifaces.data);

    nm_vect_free(&ifaces, NULL);
}

static void nm_edit_net_field_names(const nm_str_t *name, nm_window_t *w)
{
    nm_str_t buf = NM_INIT_STR;

    nm_str_alloc_str(&buf, name);
    nm_str_add_text(&buf, _(" network settings"));

    mvwaddstr(w, 1,  2,  buf.data);
    mvwaddstr(w, 4,  2, _("Interface"));
    mvwaddstr(w, 6,  2, _("Net driver"));
    mvwaddstr(w, 8,  2, _("Mac address"));
    mvwaddstr(w, 10, 2, _("IPv4 address"));

    nm_str_free(&buf);
}

static int nm_edit_net_get_data(nm_iface_t *ifp)
{
    int rc = NM_OK;
    nm_vect_t err = NM_INIT_VECT;

    nm_get_field_buf(fields[NM_FLD_INTN], &ifp->name);
    nm_get_field_buf(fields[NM_FLD_NDRV], &ifp->drv);
    nm_get_field_buf(fields[NM_FLD_MADR], &ifp->maddr);
    nm_get_field_buf(fields[NM_FLD_IPV4], &ifp->ipv4);

    if (field_status(fields[NM_FLD_INTN]))
        nm_form_check_data(_("Interface"), ifp->name, err);
    if (field_status(fields[NM_FLD_NDRV]))
        nm_form_check_data(_("Net driver"), ifp->drv, err);
    if (field_status(fields[NM_FLD_MADR]))
        nm_form_check_data(_("Mac address"), ifp->maddr, err);

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
    }

    if (field_status(fields[NM_FLD_IPV4]))
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

out:
    nm_vect_free(&err, NULL);

    return rc;
}

static inline void nm_edit_net_iface_free(nm_iface_t *ifp)
{
    nm_str_free(&ifp->name);
    nm_str_free(&ifp->drv);
    nm_str_free(&ifp->maddr);
    nm_str_free(&ifp->ipv4);
}

/* vim:set ts=4 sw=4 fdm=marker: */
