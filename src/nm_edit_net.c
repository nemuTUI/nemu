#include "nm_string.h"
#include <nm_core.h>
#include <nm_form.h>
#include <nm_menu.h>
#include <nm_utils.h>
#include <nm_window.h>
#include <nm_network.h>
#include <nm_database.h>
#include <nm_edit_net.h>
#include <nm_vm_control.h>
#include <nm_qmp_control.h>

#if defined (NM_OS_LINUX)
static const size_t NM_NET_MACVTAP_NUM = 2;
#endif

static const char NM_LC_EDIT_NET_FORM_NAME[] = "Interface name";
static const char NM_LC_EDIT_NET_FORM_NDRV[] = "Net driver";
static const char NM_LC_EDIT_NET_FORM_MADR[] = "Mac address";
static const char NM_LC_EDIT_NET_FORM_IPV4[] = "IPv4 address";
#if defined (NM_OS_LINUX)
static const char NM_LC_EDIT_NET_FORM_VHST[] = "Enable vhost";
static const char NM_LC_EDIT_NET_FORM_MTAP[] = "Enable MacVTap";
static const char NM_LC_EDIT_NET_FORM_PETH[] = "MacVTap iface";
#endif
static const char NM_LC_EDIT_NET_FORM_USER[] = "User mode";
static const char NM_LC_EDIT_NET_FORM_BRGE[] = "Bridge mode";
static const char NM_LC_EDIT_NET_FORM_BETH[] = "Bridge iface";
static const char NM_LC_EDIT_NET_FORM_FWD[]  = "Port forwarding";
static const char NM_LC_EDIT_NET_FORM_SMB[]  = "Share folder";

static void nm_edit_net_init_main_windows(bool redraw);
static void nm_edit_net_init_edit_windows(nm_form_t *form);
static void nm_edit_net_fields_setup(const nm_vmctl_data_t *vm,
        size_t if_idx, bool add);
static size_t nm_edit_net_labels_setup(void);
static int nm_edit_net_get_data(const nm_str_t *name,
        nm_iface_t *ifp, bool add);
static void nm_edit_net_update_db(const nm_str_t *name,
        nm_iface_t *ifp, bool add);
static inline void nm_edit_net_iface_free(nm_iface_t *ifp);
static int nm_edit_net_maddr_busy(const nm_str_t *mac);
static bool nm_check_nic_name_busy(const nm_str_t *name);
static int nm_edit_net_action(const nm_str_t *name, const nm_vmctl_data_t *vm,
        size_t if_idx, bool add);
static int nm_edit_net_unplug(const nm_str_t *name, const nm_vmctl_data_t *vm,
        size_t if_idx);
static void nm_edit_net_plug(const nm_str_t *name, nm_iface_t *ifp);
static int nm_verify_portfwd(const nm_str_t *fwd);

enum {
    NM_LBL_NAME, NM_FLD_NAME,
    NM_LBL_NDRV, NM_FLD_NDRV,
    NM_LBL_MADR, NM_FLD_MADR,
    NM_LBL_IPV4, NM_FLD_IPV4,
#if defined (NM_OS_LINUX)
    NM_LBL_VHST, NM_FLD_VHST,
    NM_LBL_MTAP, NM_FLD_MTAP,
    NM_LBL_PETH, NM_FLD_PETH,
#endif
    NM_LBL_BRGE, NM_FLD_BRGE,
    NM_LBL_BETH, NM_FLD_BETH,
    NM_LBL_USER, NM_FLD_USER,
    NM_LBL_FWD, NM_FLD_FWD,
    NM_LBL_SMB, NM_FLD_SMB,
    NM_FLD_COUNT
};

static nm_field_t *fields[NM_FLD_COUNT + 1];

static void nm_edit_net_init_main_windows(bool redraw)
{
    if (redraw) {
        nm_form_window_init();
    } else {
        werase(action_window);
        werase(help_window);
        werase(side_window);
    }

    nm_init_help_iface();
    nm_init_action(_(NM_MSG_IF_PROP));
    nm_init_side_if_list();

    nm_print_base_menu(NULL);
}

static void nm_edit_net_init_edit_windows(nm_form_t *form)
{
    if (form) {
        nm_form_window_init();
        nm_form_data_t *form_data = (nm_form_data_t *)form_userptr(form);

        if (form_data) {
            form_data->parent_window = action_window;
        }
    } else {
        werase(action_window);
        werase(help_window);
    }

    nm_init_help_edit();
    nm_init_action(_(NM_MSG_IF_PROP));
    nm_init_side_if_list();

    nm_print_base_menu(NULL);
}

void nm_edit_net(const nm_str_t *name)
{
    int ch = 0;
    bool regen_data = true;
    nm_menu_data_t ifs = NM_INIT_MENU_DATA;
    nm_vect_t ifaces = NM_INIT_VECT;
    nm_vmctl_data_t vm = NM_VMCTL_INIT_DATA;
    size_t nic_list_len, old_hl;

    nic_list_len = old_hl = 0;
    nm_edit_net_init_main_windows(false);

    do {
        switch (ch) {
        case NM_KEY_ENTER:
            if (vm.ifs.n_memb) {
                regen_data = true;
                if (nm_edit_net_action(name, &vm, ifs.highlight, false)
                        == NM_OK) {
                    nm_vmctl_free_data(&vm);
                    nm_vmctl_get_data(name, &vm);
                }
                nm_init_help_iface();
            }
            break;
        case NM_KEY_A:
            regen_data = true;
            old_hl = ifs.highlight;
            if (nm_edit_net_action(name, &vm, ifs.highlight, true)
                    == NM_OK) {
                nm_vmctl_free_data(&vm);
                nm_vmctl_get_data(name, &vm);
            }
            nm_init_help_iface();
            break;
        case NM_KEY_R:
            {
                int ans = nm_notify(_(NM_MSG_DELETE));

                if (ans == 'y') {
                    if (nm_edit_net_unplug(name, &vm, ifs.highlight) == NM_OK) {
                        nm_vmctl_free_data(&vm);
                        nm_vmctl_get_data(name, &vm);
                        regen_data = true;
                        old_hl = ifs.highlight;
                        if (ifs.item_first != 0) {
                            ifs.item_first--;
                            ifs.item_last--;
                        }
                        werase(side_window);
                        nm_init_side_if_list();
                    }
                }
            }
            break;
        }

        if (regen_data) {
            size_t iface_count = 0;

            nm_vect_free(&ifaces, NULL);
            nm_vmctl_free_data(&vm);
            nm_vmctl_get_data(name, &vm);
            iface_count = vm.ifs.n_memb / NM_IFS_IDX_COUNT;
            nic_list_len = (getmaxy(side_window) - 4);
            ifs.highlight = 1;

            if (old_hl > 1) {
                if (iface_count < old_hl) {
                    ifs.highlight = (old_hl - 1);
                } else {
                    ifs.highlight = old_hl;
                }
            }

            if (nic_list_len < iface_count) {
                ifs.item_last = nic_list_len;
            } else {
                ifs.item_last = nic_list_len = iface_count;
            }

            for (size_t n = 0; n < iface_count; n++) {
                size_t idx_shift = NM_IFS_IDX_COUNT * n;

                nm_vect_insert(&ifaces,
                               nm_vect_str_ctx(&vm.ifs,
                                   NM_SQL_IF_NAME + idx_shift),
                               nm_vect_str_len(&vm.ifs,
                                   NM_SQL_IF_NAME + idx_shift) + 1,
                               NULL);
            }

            ifs.v = &ifaces;
            regen_data = false;
        }

        if (ifs.v->n_memb > 0) {
            nm_menu_scroll(&ifs, nic_list_len, ch);
            werase(action_window);
            nm_init_action(_(NM_MSG_IF_PROP));
            nm_print_base_menu(&ifs);
            nm_print_iface_info(&vm, ifs.highlight);
        } else {
            werase(action_window);
            nm_init_action(_(NM_MSG_IF_PROP));
        }

        if (redraw_window) {
            size_t iface_count = vm.ifs.n_memb / NM_IFS_IDX_COUNT;

            nm_edit_net_init_main_windows(true);

            nic_list_len = (getmaxy(side_window) - 4);
            /* TODO save last pos */
            if (nic_list_len < iface_count) {
                ifs.item_last = nic_list_len;
                ifs.item_first = 0;
                ifs.highlight = 1;
            } else {
                ifs.item_last = nic_list_len = iface_count;
            }

            redraw_window = 0;
        }
    } while ((ch = wgetch(action_window)) != NM_KEY_Q);

    werase(side_window);
    werase(help_window);
    nm_init_help_main();

    nm_vect_free(&ifaces, NULL);
    nm_vmctl_free_data(&vm);
}

static void
nm_edit_net_plug(const nm_str_t *name, nm_iface_t *ifp)
{
#if defined(NM_OS_LINUX)
    /*
     * If we need to setup IPv4 address or altname we must create
     * the tap interface yourself.
     */
    if (ifp->ipv4.len && (nm_str_cmp_st(&ifp->macvtap, "no") == NM_OK)) {
        nm_net_add_tap(&ifp->name);
        nm_net_set_ipaddr(&ifp->name, &ifp->ipv4);
    }

    if ((nm_str_cmp_st(&ifp->macvtap, "no") != NM_OK) && ifp->parent_eth.len) {
        nm_str_t tap_path = NM_INIT_STR;
        bool wait_perm = false;
        uint32_t tap_idx = 0;

        if (nm_net_iface_exists(&ifp->name) != NM_OK) {
            wait_perm = true;
            nm_net_add_macvtap(&ifp->name, &ifp->parent_eth, &ifp->maddr,
                    (nm_str_cmp_st(&ifp->macvtap, "bridge") == NM_OK) ?
                    NM_MACVTAP_BRIDGE_MODE : NM_MACVTAP_PRIVATE_MODE);
        }
        tap_idx = nm_net_iface_idx(&ifp->name);
        if (tap_idx == 0) {
            nm_bug("%s: MacVTap interface not found", __func__);
        }

        nm_str_format(&tap_path, "/dev/tap%u", tap_idx);
        /* wait for udev fixes permissions on /dev/tapN */
        if ((getuid() != 0) && wait_perm) {
            struct timespec ts;
            int tap_rw_ok = 0;

            memset(&ts, 0, sizeof(ts));
            ts.tv_nsec = 5e+7; /* 0.05sec */

            for (int m = 0; m < 40; m++) {
                if (access(tap_path.data, R_OK | W_OK) == 0) {
                    tap_rw_ok = 1;
                    break;
                }
                nanosleep(&ts, NULL);
            }
            if (!tap_rw_ok) {
                nm_warn(_(NM_MSG_TAP_EACC));
                return;
            }
        }

        ifp->tap_fd = open(tap_path.data, O_RDWR);
        if (ifp->tap_fd == -1) {
            nm_bug("%s: open failed: %s", __func__, strerror(errno));
        }
    }
#endif /* NM_OS_LINUX */

    nm_qmp_nic_attach(name, ifp);
}

static int
nm_edit_net_unplug(const nm_str_t *name, const nm_vmctl_data_t *vm,
        size_t if_idx)
{
    nm_iface_t iface_data = NM_INIT_NET_IF;
    nm_str_t query = NM_INIT_STR;
    nm_str_t *if_name;
    size_t idx_shift;
    int rc = NM_OK;

    if (!if_idx) {
        rc = NM_ERR;
        goto out;
    }

    idx_shift = NM_IFS_IDX_COUNT * (--if_idx);

    if (nm_qmp_test_socket(name) == NM_OK) {
        nm_str_format(&iface_data.maddr, "%s",
                nm_vect_str_ctx(&vm->ifs, NM_SQL_IF_MAC + idx_shift));
        nm_qmp_nic_detach(name, &iface_data);
    }

    if_name = nm_vect_str(&vm->ifs, NM_SQL_IF_NAME + idx_shift);
    nm_str_format(&query, NM_SQL_IFACES_DELETE, name->data, if_name->data);
    nm_db_edit(query.data);
#if defined(NM_OS_LINUX)
    uint32_t tap_idx = nm_net_iface_idx(if_name);

    if (tap_idx != 0) { /* iface exist */
        /* detect iface type */
        struct stat tap_info;
        nm_str_t tap_path = NM_INIT_STR;

        nm_str_format(&tap_path, "/dev/tap%u", tap_idx);
        if (stat(tap_path.data, &tap_info) == 0) {
            /* iface is macvtap, delete it */
            nm_net_del_iface(if_name);
        } else {
            /* iface is simple tap, delete it */
            nm_net_del_tap(if_name);
        }
        nm_str_free(&tap_path);
    }
#endif

out:
    nm_edit_net_iface_free(&iface_data);
    nm_str_free(&query);
    return rc;
}

static int
nm_edit_net_action(const nm_str_t *name, const nm_vmctl_data_t *vm,
        size_t if_idx, bool add)
{
    nm_form_data_t *form_data = NULL;
    nm_form_t *form = NULL;
    int rc = NM_OK;
    nm_iface_t iface_data = NM_INIT_NET_IF;
    size_t idx_shift;
    size_t msg_len;

    if (!if_idx) {
        rc = NM_ERR;
        goto out;
    }

    idx_shift = NM_IFS_IDX_COUNT * (--if_idx);
    if_idx++; /* restore idx */

    if (!add) {
        nm_str_format(&iface_data.name, "%s",
            nm_vect_str(&vm->ifs, NM_SQL_IF_NAME + idx_shift)->data);

        if (!iface_data.name.len) {
            rc = NM_ERR;
            goto out;
        }
    }

    nm_edit_net_init_edit_windows(NULL);

    msg_len = nm_edit_net_labels_setup();

    form_data = nm_form_data_new(
        action_window, nm_edit_net_init_edit_windows, msg_len,
        NM_FLD_COUNT / 2, NM_TRUE);

    if (nm_form_data_update(form_data, 0, 0) != NM_OK) {
        rc = NM_ERR;
        goto out;
    }

    for (size_t n = 0; n < NM_FLD_COUNT; n++) {
        switch (n) {
        case NM_FLD_NAME:
            fields[n] = nm_field_regexp_new(n / 2, form_data,
                    "^[a-zA-Z0-9_]{1,15} *$");
            break;
        case NM_FLD_NDRV:
            fields[n] = nm_field_enum_new(
                n / 2, form_data, nm_form_net_drv, false, false);
            break;
        case NM_FLD_MADR:
            fields[n] = nm_field_regexp_new(n / 2, form_data, ".*");
            break;
        case NM_FLD_IPV4:
            fields[n] = nm_field_regexp_new(n / 2, form_data, ".*");
            break;
#if defined (NM_OS_LINUX)
        case NM_FLD_VHST:
            fields[n] = nm_field_enum_new(
                n / 2, form_data, nm_form_yes_no, false, false);
            break;
        case NM_FLD_MTAP:
            fields[n] = nm_field_enum_new(
                n / 2, form_data, nm_form_macvtap, false, false);
            break;
        case NM_FLD_PETH:
            fields[n] = nm_field_regexp_new(n / 2, form_data, ".*");
            break;
#endif
        case NM_FLD_BRGE:
            fields[n] = nm_field_enum_new(
                n / 2, form_data, nm_form_yes_no, false, false);
            break;
        case NM_FLD_BETH:
            fields[n] = nm_field_regexp_new(n / 2, form_data,
                    "^[a-zA-Z0-9_]{1,15} *$");
            break;
        case NM_FLD_USER:
            fields[n] = nm_field_enum_new(
                n / 2, form_data, nm_form_yes_no, false, false);
            break;
        case NM_FLD_FWD:
            fields[n] = nm_field_regexp_new(n / 2, form_data, ".*");
            break;
        case NM_FLD_SMB:
            fields[n] = nm_field_regexp_new(n / 2, form_data, "^/.*");
            break;
        default:
            fields[n] = nm_field_label_new(n / 2, form_data);
            break;
        }
    }
    fields[NM_FLD_COUNT] = NULL;

    nm_edit_net_labels_setup();
    nm_edit_net_fields_setup(vm, if_idx, add);
    nm_fields_unset_status(fields);

    form = nm_form_new(form_data, fields);
    nm_form_add_hline(form, NM_LBL_BRGE);
    nm_form_add_hline(form, NM_LBL_USER);
    nm_form_post(form);

    if (nm_form_draw(&form) != NM_OK) {
        rc = NM_ERR;
        goto out;
    }

    if (nm_edit_net_get_data(name, &iface_data, add) != NM_OK) {
        rc = NM_ERR;
        goto out;
    }

    nm_edit_net_update_db(name, &iface_data, add);

    if (add && (nm_qmp_test_socket(name) == NM_OK)) {
        nm_edit_net_plug(name, &iface_data);
    }

out:
    wtimeout(action_window, -1);
    delwin(form_data->form_window);
    werase(help_window);
    nm_edit_net_iface_free(&iface_data);
    nm_form_free(form);
    nm_form_data_free(form_data);
    nm_fields_free(fields);

    return rc;
}

static void
nm_edit_net_fields_setup(const nm_vmctl_data_t *vm, size_t if_idx, bool add)
{
    size_t mvtap_idx = 0;
    size_t idx_shift;

    if (!if_idx) {
        return;
    }
    idx_shift = NM_IFS_IDX_COUNT * (--if_idx);

    field_opts_off(fields[NM_FLD_NAME], O_STATIC);
    field_opts_off(fields[NM_FLD_MADR], O_STATIC);
    field_opts_off(fields[NM_FLD_IPV4], O_STATIC);
    field_opts_off(fields[NM_FLD_SMB], O_STATIC);
#if defined (NM_OS_LINUX)
    field_opts_off(fields[NM_FLD_PETH], O_STATIC);
#endif
    if (add) {
        uint64_t last_mac = nm_form_get_last_mac();
        nm_str_t maddr = NM_INIT_STR;

        last_mac++;
        nm_net_mac_n2s(last_mac, &maddr);
        set_field_buffer(fields[NM_FLD_MADR], 0, maddr.data);
        nm_str_free(&maddr);

        set_field_buffer(fields[NM_FLD_NDRV], 0, nm_form_net_drv[0]);
        set_field_buffer(fields[NM_FLD_BRGE], 0, nm_form_yes_no[0]);
        field_opts_off(fields[NM_FLD_BETH], O_STATIC);
        set_field_buffer(fields[NM_FLD_USER], 0, nm_form_yes_no[1]);
#if defined (NM_OS_LINUX)
        set_field_buffer(fields[NM_FLD_VHST], 0, nm_form_yes_no[0]);
        set_field_buffer(fields[NM_FLD_MTAP], 0, nm_form_macvtap[0]);
#endif
        return;
    }

    set_field_buffer(fields[NM_FLD_NAME], 0,
        nm_vect_str_ctx(&vm->ifs, NM_SQL_IF_NAME + idx_shift));
    set_field_buffer(fields[NM_FLD_NDRV], 0,
        nm_vect_str_ctx(&vm->ifs, NM_SQL_IF_DRV + idx_shift));
    set_field_buffer(fields[NM_FLD_MADR], 0,
        nm_vect_str_ctx(&vm->ifs, NM_SQL_IF_MAC + idx_shift));
    if (nm_vect_str_len(&vm->ifs, NM_SQL_IF_IP4 + idx_shift) > 0) {
        set_field_buffer(fields[NM_FLD_IPV4], 0,
                nm_vect_str_ctx(&vm->ifs, NM_SQL_IF_IP4 + idx_shift));
    }
#if defined (NM_OS_LINUX)
    set_field_buffer(fields[NM_FLD_VHST], 0,
        (nm_str_cmp_st(nm_vect_str(&vm->ifs, NM_SQL_IF_VHO + idx_shift),
            NM_ENABLE) == NM_OK) ? "yes" : "no");

    mvtap_idx = nm_str_stoui(nm_vect_str(&vm->ifs,
                NM_SQL_IF_MVT + idx_shift), 10);
    if (mvtap_idx > NM_NET_MACVTAP_NUM) {
        nm_bug("%s: invalid macvtap array index: %zu", __func__, mvtap_idx);
    }
    set_field_buffer(fields[NM_FLD_MTAP], 0, nm_form_macvtap[mvtap_idx]);
    if (nm_vect_str_len(&vm->ifs, NM_SQL_IF_PET + idx_shift) > 0) {
        set_field_buffer(fields[NM_FLD_PETH], 0,
            nm_vect_str_ctx(&vm->ifs, NM_SQL_IF_PET + idx_shift));
    }
#else
    (void) mvtap_idx;
#endif
    set_field_buffer(fields[NM_FLD_BRGE], 0,
        (nm_str_cmp_st(nm_vect_str(&vm->ifs, NM_SQL_IF_BGE + idx_shift),
                       NM_ENABLE) == NM_OK) ?
        nm_form_yes_no[0] : nm_form_yes_no[1]);
    set_field_buffer(fields[NM_FLD_BETH], 0,
        nm_vect_str_ctx(&vm->ifs, NM_SQL_IF_BETH + idx_shift));
    set_field_buffer(fields[NM_FLD_USER], 0,
        (nm_str_cmp_st(nm_vect_str(&vm->ifs, NM_SQL_IF_USR + idx_shift),
                       NM_ENABLE) == NM_OK) ?
        nm_form_yes_no[0] : nm_form_yes_no[1]);
    if (nm_vect_str_len(&vm->ifs, NM_SQL_IF_FWD + idx_shift) > 0) {
        set_field_buffer(fields[NM_FLD_FWD], 0,
            nm_vect_str_ctx(&vm->ifs, NM_SQL_IF_FWD + idx_shift));
    }
    if (nm_vect_str_len(&vm->ifs, NM_SQL_IF_SMB + idx_shift) > 0) {
        set_field_buffer(fields[NM_FLD_SMB], 0,
            nm_vect_str_ctx(&vm->ifs, NM_SQL_IF_SMB + idx_shift));
    }
}

static size_t nm_edit_net_labels_setup(void)
{
    nm_str_t buf = NM_INIT_STR;
    size_t max_label_len = 0;
    size_t msg_len = 0;

    for (size_t n = 0; n < NM_FLD_COUNT; n++) {
        switch (n) {
        case NM_LBL_NAME:
            nm_str_format(&buf, "%s", _(NM_LC_EDIT_NET_FORM_NAME));
            break;
        case NM_LBL_NDRV:
            nm_str_format(&buf, "%s", _(NM_LC_EDIT_NET_FORM_NDRV));
            break;
        case NM_LBL_MADR:
            nm_str_format(&buf, "%s", _(NM_LC_EDIT_NET_FORM_MADR));
            break;
        case NM_LBL_IPV4:
            nm_str_format(&buf, "%s", _(NM_LC_EDIT_NET_FORM_IPV4));
            break;
#if defined (NM_OS_LINUX)
        case NM_LBL_VHST:
            nm_str_format(&buf, "%s", _(NM_LC_EDIT_NET_FORM_VHST));
            break;
        case NM_LBL_MTAP:
            nm_str_format(&buf, "%s", _(NM_LC_EDIT_NET_FORM_MTAP));
            break;
        case NM_LBL_PETH:
            nm_str_format(&buf, "%s", _(NM_LC_EDIT_NET_FORM_PETH));
            break;
#endif
        case NM_LBL_BRGE:
            nm_str_format(&buf, "%s", _(NM_LC_EDIT_NET_FORM_BRGE));
            break;
        case NM_LBL_BETH:
            nm_str_format(&buf, "%s", _(NM_LC_EDIT_NET_FORM_BETH));
            break;
        case NM_LBL_USER:
            nm_str_format(&buf, "%s", _(NM_LC_EDIT_NET_FORM_USER));
            break;
        case NM_LBL_FWD:
            nm_str_format(&buf, "%s", _(NM_LC_EDIT_NET_FORM_FWD));
            break;
        case NM_LBL_SMB:
            nm_str_format(&buf, "%s", _(NM_LC_EDIT_NET_FORM_SMB));
            break;
        default:
            continue;
        }

        msg_len = mbstowcs(NULL, buf.data, buf.len);
        if (msg_len > max_label_len) {
            max_label_len = msg_len;
        }

        if (fields[n]) {
            set_field_buffer(fields[n], 0, buf.data);
        }
    }

    nm_str_free(&buf);
    return max_label_len;
}

static int
nm_edit_net_get_data(const nm_str_t *name, nm_iface_t *ifp, bool add)
{
    int rc;
    nm_vect_t err = NM_INIT_VECT;

    if (!add) {
        nm_str_copy(&ifp->oldname, &ifp->name);
        nm_str_trunc(&ifp->name, 0);
    }

    nm_get_field_buf(fields[NM_FLD_NAME], &ifp->name);
    nm_get_field_buf(fields[NM_FLD_NDRV], &ifp->drv);
    nm_get_field_buf(fields[NM_FLD_MADR], &ifp->maddr);
    nm_get_field_buf(fields[NM_FLD_IPV4], &ifp->ipv4);
#if defined (NM_OS_LINUX)
    nm_get_field_buf(fields[NM_FLD_VHST], &ifp->vhost);
    nm_get_field_buf(fields[NM_FLD_MTAP], &ifp->macvtap);
    nm_get_field_buf(fields[NM_FLD_PETH], &ifp->parent_eth);
#endif
    nm_get_field_buf(fields[NM_FLD_BRGE], &ifp->bridge);
    nm_get_field_buf(fields[NM_FLD_BETH], &ifp->bridge_eth);
    nm_get_field_buf(fields[NM_FLD_USER], &ifp->netuser);
    nm_get_field_buf(fields[NM_FLD_FWD], &ifp->hostfwd);
    nm_get_field_buf(fields[NM_FLD_SMB], &ifp->smb);

    if (field_status(fields[NM_FLD_NAME]) || add) {
        nm_form_check_data(_("Interface name"), ifp->name, err);
    }
    if (field_status(fields[NM_FLD_NDRV])) {
        nm_form_check_data(_("Net driver"), ifp->drv, err);
    }
    if (field_status(fields[NM_FLD_MADR])) {
        nm_form_check_data(_("Mac address"), ifp->maddr, err);
    }
#if defined (NM_OS_LINUX)
    if (field_status(fields[NM_FLD_VHST])) {
        nm_form_check_data(_("Enable vhost"), ifp->vhost, err);
    }
    if (field_status(fields[NM_FLD_MTAP])) {
        nm_form_check_data(_("Enable MacVTap"), ifp->macvtap, err);
    }
#endif
    if (field_status(fields[NM_FLD_BRGE])) {
        nm_form_check_data(_("Bridge mode"), ifp->bridge, err);
    }
    if (field_status(fields[NM_FLD_BETH])) {
        nm_form_check_data(_("Bridge iface"), ifp->bridge_eth, err);
    }
    if (field_status(fields[NM_FLD_USER])) {
        nm_form_check_data(_("User mode"), ifp->netuser, err);
    }

    if ((rc = nm_print_empty_fields(&err)) == NM_ERR) {
        goto out;
    }

    if (field_status(fields[NM_FLD_MADR])) {
        if (nm_net_verify_mac(&ifp->maddr) != NM_OK) {
            nm_warn(_(NM_MSG_MAC_INVAL));
            rc = NM_ERR;
            goto out;
        }

        if (nm_edit_net_maddr_busy(&ifp->maddr) != NM_OK) {
            nm_warn(_(NM_MSG_MAC_USED));
            rc = NM_ERR;
            goto out;
        }
    }

    if (field_status(fields[NM_FLD_NAME])) {
        if (!add && nm_str_cmp_ss(&ifp->name, &ifp->oldname) != NM_OK &&
                nm_check_nic_name_busy(&ifp->name)) {
            nm_warn(_(NM_MSG_NIC_USED));
            rc = NM_ERR;
            goto out;
        } else if (add && nm_check_nic_name_busy(&ifp->name)) {
            nm_warn(_(NM_MSG_NIC_USED));
            rc = NM_ERR;
            goto out;
        }
    }

    if (field_status(fields[NM_FLD_FWD])) {
        if (nm_verify_portfwd(&ifp->hostfwd) != NM_OK) {
            nm_warn(_(NM_MSF_FWD_INVAL));
            rc = NM_ERR;
            goto out;
        }
    }

    if ((field_status(fields[NM_FLD_IPV4])) && (ifp->ipv4.len > 0)) {
        nm_str_t err_msg = NM_INIT_STR;

        if (nm_net_verify_ipaddr4(&ifp->ipv4, NULL, &err_msg) != NM_OK) {
            nm_warn(err_msg.data);
            rc = NM_ERR;
            goto out;
        }
        nm_str_free(&err_msg);
    }

#if defined (NM_OS_LINUX)
    /* Do not allow to enable vhost on non virtio net device */
    if ((field_status(fields[NM_FLD_VHST])) &&
        (nm_str_cmp_st(&ifp->vhost, "yes") == NM_OK)) {
        int vhost_ok = 1;

        if (field_status(fields[NM_FLD_NDRV])) {
            if (nm_str_cmp_st(&ifp->drv, NM_DEFAULT_NETDRV) != NM_OK) {
                vhost_ok = 0;
            }
        } else {
            nm_str_t query = NM_INIT_STR;
            nm_vect_t netv = NM_INIT_VECT;

            nm_str_format(&query, NM_SQL_IFACES_SELECT_ID,
                name->data, ifp->name.data, NM_DEFAULT_NETDRV);
            nm_db_select(query.data, &netv);

            if (netv.n_memb == 0) {
                vhost_ok = 0;
            }

            nm_vect_free(&netv, nm_str_vect_free_cb);
            nm_str_free(&query);
        }

        if (!vhost_ok) {
            rc = NM_ERR;
            nm_warn(_(NM_MSG_VHOST_ERR));
        }
    }

    /* Check for MacVTap parent interface exists */
    if (field_status(fields[NM_FLD_PETH]) && (ifp->parent_eth.len > 0)) {
        if (nm_net_iface_exists(&ifp->parent_eth) != NM_OK) {
            rc = NM_ERR;
            nm_warn(_(NM_MSG_VTAP_NOP));
        }
    }
#else
    (void) name;
    (void) ifp;
#endif /* NM_OS_LINUX */

out:
    nm_vect_free(&err, NULL);

    return rc;
}

static void
nm_edit_net_update_db(const nm_str_t *name, nm_iface_t *ifp, bool add)
{
    nm_str_t query = NM_INIT_STR;

    if (add) {
        nm_str_format(&query, NM_SQL_IFACES_INSERT_NEW,
                name->data,
                ifp->name.data,
                "",
                NM_DEFAULT_NETDRV,
#if defined(NM_OS_LINUX)
                NM_ENABLE,
#else
                NM_DISABLE,
#endif
                NM_DISABLE, "", NM_DISABLE, NM_DISABLE, "");
        nm_db_edit(query.data);
    } else {
        if (field_status(fields[NM_FLD_NAME])) {
            nm_str_format(&query, NM_SQL_IFACES_UPDATE_NAME,
                    ifp->name.data, name->data, ifp->oldname.data);
            nm_db_edit(query.data);
        }
    }

    if (field_status(fields[NM_FLD_NDRV]) || add) {
        nm_str_format(&query, NM_SQL_IFACES_UPDATE_DRV,
                ifp->drv.data, name->data, ifp->name.data);
        nm_db_edit(query.data);

#if defined(NM_OS_LINUX)
        /* disable vhost if driver is not virtio-net */
        if (nm_str_cmp_st(&ifp->drv, NM_DEFAULT_NETDRV) != NM_OK) {
            nm_str_format(&query, NM_SQL_IFACES_UPDATE_VHOST,
                    NM_DISABLE, name->data, ifp->name.data);
            nm_db_edit(query.data);
        }
#endif
    }

    if (field_status(fields[NM_FLD_MADR]) || add) {
        nm_str_format(&query, NM_SQL_IFACES_UPDATE_MADDR,
                ifp->maddr.data, name->data, ifp->name.data);
        nm_db_edit(query.data);
    }

    if (field_status(fields[NM_FLD_IPV4]) || add) {
        nm_str_format(&query, NM_SQL_IFACES_UPDATE_IPV4,
                ifp->ipv4.data, name->data, ifp->name.data);
        nm_db_edit(query.data);
    }

#if defined (NM_OS_LINUX)
    if (field_status(fields[NM_FLD_VHST]) || add) {
        nm_str_format(&query, NM_SQL_IFACES_UPDATE_VHOST,
                (nm_str_cmp_st(&ifp->vhost, "yes") == NM_OK) ?
                NM_ENABLE : NM_DISABLE,
                name->data, ifp->name.data);
        nm_db_edit(query.data);
    }

    if (field_status(fields[NM_FLD_MTAP]) || add) {
        ssize_t macvtap_idx = -1;
        const char **p = nm_form_macvtap;

        for (ssize_t n = 0; *p; p++, n++) {
            if (nm_str_cmp_st(&ifp->macvtap, *p) == NM_OK) {
                macvtap_idx = n;
                break;
            }
        }

        if (macvtap_idx == -1) {
            nm_bug("%s: macvtap_idx is not found", __func__);
        }

        nm_str_format(&query, NM_SQL_IFACES_UPDATE_MACVTAP,
            macvtap_idx, name->data, ifp->name.data);
        nm_db_edit(query.data);
    }

    if (field_status(fields[NM_FLD_PETH]) || add) {
        nm_str_format(&query, NM_SQL_IFACES_UPDATE_PARENT,
            ifp->parent_eth.data, name->data, ifp->name.data);
        nm_db_edit(query.data);
    }
#endif

    if (field_status(fields[NM_FLD_USER]) || add) {
        nm_str_format(&query, NM_SQL_IFACES_UPDATE_USERNET,
            (nm_str_cmp_st(&ifp->netuser, "yes") == NM_OK) ?
            NM_ENABLE : NM_DISABLE, name->data, ifp->name.data);
        nm_db_edit(query.data);
    }

    if (field_status(fields[NM_FLD_BRGE]) || add) {
        nm_str_format(&query, NM_SQL_IFACES_UPDATE_BRIDGE,
            (nm_str_cmp_st(&ifp->bridge, "yes") == NM_OK) ?
            NM_ENABLE : NM_DISABLE, name->data, ifp->name.data);
        nm_db_edit(query.data);
    }

    if (field_status(fields[NM_FLD_BETH]) || add) {
        nm_str_format(&query, NM_SQL_IFACES_UPDATE_BETH,
            ifp->bridge_eth.data, name->data, ifp->name.data);
        nm_db_edit(query.data);
    }

    if (field_status(fields[NM_FLD_FWD]) || add) {
        nm_str_format(&query, NM_SQL_IFACES_UPDATE_HOSTFWD,
            ifp->hostfwd.data, name->data, ifp->name.data);
        nm_db_edit(query.data);
    }

    if (field_status(fields[NM_FLD_SMB]) || add) {
        nm_str_format(&query, NM_SQL_IFACES_UPDATE_SMB,
            ifp->smb.data, name->data, ifp->name.data);
        nm_db_edit(query.data);
    }

    nm_str_free(&query);
}

static inline void nm_edit_net_iface_free(nm_iface_t *ifp)
{
    nm_str_free(&ifp->name);
    nm_str_free(&ifp->oldname);
    nm_str_free(&ifp->drv);
    nm_str_free(&ifp->maddr);
    nm_str_free(&ifp->ipv4);
#if defined (NM_OS_LINUX)
    nm_str_free(&ifp->vhost);
    nm_str_free(&ifp->macvtap);
    nm_str_free(&ifp->parent_eth);
#endif
    nm_str_free(&ifp->netuser);
    nm_str_free(&ifp->bridge);
    nm_str_free(&ifp->bridge_eth);
    nm_str_free(&ifp->hostfwd);
    nm_str_free(&ifp->smb);
}

/* TODO add this check in all genmaddr points */
static int nm_edit_net_maddr_busy(const nm_str_t *mac)
{
    int rc = NM_OK;
    nm_vect_t maddrs = NM_INIT_VECT;

    nm_db_select(NM_SQL_IFACES_SELECT_MAC, &maddrs);

    for (size_t n = 0; n < maddrs.n_memb; n++) {
        if (nm_str_cmp_ss(mac, nm_vect_str(&maddrs, n)) == NM_OK) {
            rc = NM_ERR;
            break;
        }
    }

    nm_vect_free(&maddrs, nm_str_vect_free_cb);

    return rc;
}

static bool nm_check_nic_name_busy(const nm_str_t *name)
{
    nm_vect_t names = NM_INIT_VECT;
    nm_str_t query = NM_INIT_STR;
    bool rc = false;

    nm_str_format(&query, NM_SQL_IFACES_SELECT_ID_BY_NAME, name->data);
    nm_db_select(query.data, &names);
    if (names.n_memb > 0) {
        rc = true;
    }

    nm_str_free(&query);
    nm_vect_free(&names, nm_str_vect_free_cb);
    return rc;
}

static int nm_verify_port(const char *port, size_t *len)
{
    size_t num_len = 0;
    nm_str_t buf = NM_INIT_STR;
    uint32_t port_num;
    int rc = NM_OK;
    const char *tmp = port;

    while (*tmp) {
        if (!isdigit(*tmp)) {
            break;
        }
        num_len++;
        tmp++;
    }

    if (!num_len) {
        return NM_ERR;
    }

    *len = num_len;

    nm_str_add_text_part(&buf, port, num_len);
    port_num = nm_str_stoui(&buf, 10);

    if (port_num < 1 || port_num > USHRT_MAX) {
        rc = NM_ERR;
    }

    nm_str_free(&buf);

    return rc;
}

/*
 * Verify portfwd value
 * Format: tcp|udp::[1-65535]-:[1-65535]
 */
static int nm_verify_portfwd(const nm_str_t *fwd)
{
    char proto[4] = {0};
    const char *p = fwd->data;
    size_t num_len = 0;

    if (fwd->len == 0) {
        return NM_OK;
    }

    if (fwd->len < 3) {
        return NM_ERR;
    }

    memcpy(proto, p, 3);
    p += 3;

    if (nm_str_cmp_tt(proto, "tcp") != NM_OK &&
        nm_str_cmp_tt(proto, "udp") != NM_OK) {
        return NM_ERR;
    }

    if (*p != ':' || *(++p) != ':') {
        return NM_ERR;
    }

    p++;
    if (nm_verify_port(p, &num_len) != NM_OK) {
        return NM_ERR;
    }

    p += num_len;
    if (*p != '-' || *(++p) != ':') {
        return NM_ERR;
    }

    p++;
    if (nm_verify_port(p, &num_len) != NM_OK) {
        return NM_ERR;
    }

    p += num_len;
    if (*(p)) {
        return NM_ERR;
    }

    return NM_OK;
}

/* vim:set ts=4 sw=4: */
