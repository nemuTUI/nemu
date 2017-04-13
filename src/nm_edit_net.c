#include <nm_core.h>
#include <nm_form.h>
#include <nm_utils.h>
#include <nm_window.h>
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
    size_t msg_len;
    pthread_t spin_th;
    int done = 0;

    nm_print_title(_(NM_EDIT_TITLE));
    window = nm_init_window(13, 45, 3);

    init_pair(1, COLOR_BLACK, COLOR_WHITE);
    wbkgd(window, COLOR_PAIR(1));

    for (size_t n = 0; n < NM_NET_FIELDS_NUM; ++n)
    {
        fields[n] = new_field(1, 22, (n + 1) * 2, 2, 0, 0);
        set_field_back(fields[n], A_UNDERLINE);
    }

    fields[NM_NET_FIELDS_NUM] = NULL;

    nm_edit_net_field_setup(vm);
    nm_edit_net_field_names(name, window);

    form = nm_post_form(window, fields, 18);
    if (nm_draw_form(window, form) != NM_OK)
        goto out;
out:
    nm_form_free(form, fields);
}

static void nm_edit_net_field_setup(const nm_vmctl_data_t *vm)
{
    //...
}

static void nm_edit_net_field_names(const nm_str_t *name, nm_window_t *w)
{
    //...
}

static inline void nm_edit_net_iface_free(nm_iface_t *ifp)
{
    nm_str_free(&ifp->name);
    nm_str_free(&ifp->drv);
    nm_str_free(&ifp->maddr);
    nm_str_free(&ifp->ipv4);
}

/* vim:set ts=4 sw=4 fdm=marker: */
