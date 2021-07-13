#include <nm_core.h>
#include <nm_form.h>
#include <nm_utils.h>
#include <nm_string.h>
#include <nm_vector.h>
#include <nm_window.h>
#include <nm_menu.h>
#include <nm_machine.h>
#include <nm_hw_info.h>
#include <nm_network.h>
#include <nm_database.h>
#include <nm_cfg_file.h>
#include <nm_vm_control.h>
#include <nm_qmp_control.h>
#include <nm_edit_vm.h>

static const char NM_VM_FORM_CPU[]       = "CPU count";
static const char NM_VM_FORM_MEM_BEGIN[] = "Memory [4-";
static const char NM_VM_FORM_MEM_END[]   = "]Mb";
static const char NM_VM_FORM_KVM[]       = "KVM [yes/no]";
static const char NM_VM_FORM_HCPU[]      = "Host CPU [yes/no]";
static const char NM_VM_FORM_NET_IFS[]   = "Network interfaces";
static const char NM_VM_FORM_DRV_IF[]    = "Disk interface";
static const char NM_VM_FORM_DRV_DIS[]   = "Discard mode";
static const char NM_VM_FORM_USB[]       = "USB [yes/no]";
static const char NM_VM_FORM_USBT[]      = "USB version";
static const char NM_VM_FORM_MACH[]      = "Machine type";
static const char NM_VM_FORM_ARGS[]      = "Extra QEMU args";
static const char NM_VM_FORM_GROUP[]     = "Group";

static void nm_edit_vm_init_windows(nm_form_t *form);
static void nm_edit_vm_fields_setup(const nm_vmctl_data_t *cur);
static size_t nm_edit_vm_labels_setup();
static int nm_edit_vm_get_data(nm_vm_t *vm, const nm_vmctl_data_t *cur);
static void nm_edit_vm_update_db(nm_vm_t *vm, const nm_vmctl_data_t *cur, uint64_t mac);

enum {
    NM_LBL_CPUNUM = 0, NM_FLD_CPUNUM,
    NM_LBL_RAMTOT, NM_FLD_RAMTOT,
    NM_LBL_KVMFLG, NM_FLD_KVMFLG,
    NM_LBL_HOSCPU, NM_FLD_HOSCPU,
    NM_LBL_IFSCNT, NM_FLD_IFSCNT,
    NM_LBL_DISKIN, NM_FLD_DISKIN,
    NM_LBL_DISCARD, NM_FLD_DISCARD,
    NM_LBL_USBUSE, NM_FLD_USBUSE,
    NM_LBL_USBTYP, NM_FLD_USBTYP,
    NM_LBL_MACH, NM_FLD_MACH,
    NM_LBL_ARGS, NM_FLD_ARGS,
    NM_LBL_GROUP, NM_FLD_GROUP,
    NM_FLD_COUNT
};

static nm_field_t *fields[NM_FLD_COUNT + 1];

static void nm_edit_vm_init_windows(nm_form_t *form)
{
    nm_form_window_init();
    if(form) {
        nm_form_data_t *form_data = (nm_form_data_t *)form_userptr(form);
        if(form_data)
            form_data->parent_window = action_window;
    }

    nm_init_side();
    nm_init_action(_(NM_MSG_EDIT_VM));
    nm_init_help_edit();

    nm_print_vm_menu(NULL);
}

void nm_edit_vm(const nm_str_t *name)
{
    nm_form_data_t *form_data = NULL;
    nm_form_t *form = NULL;
    nm_vm_t vm = NM_INIT_VM;
    nm_vmctl_data_t cur_settings = NM_VMCTL_INIT_DATA;
    uint64_t last_mac;
    size_t msg_len;

    nm_edit_vm_init_windows(NULL);

    nm_vmctl_get_data(name, &cur_settings);

    msg_len = nm_edit_vm_labels_setup();

    form_data = nm_form_data_new(
        action_window, nm_edit_vm_init_windows, msg_len, NM_FLD_COUNT / 2, NM_TRUE);

    if (nm_form_data_update(form_data, 0, 0) != NM_OK)
        goto out;

    for (size_t n = 0; n < NM_FLD_COUNT; n += 2) {
        fields[n] = nm_field_new(NM_FIELD_LABEL, n / 2, form_data);
        fields[n + 1] = nm_field_new(NM_FIELD_EDIT, n / 2, form_data);
    }
    fields[NM_FLD_COUNT] = NULL;

    nm_edit_vm_labels_setup();
    nm_edit_vm_fields_setup(&cur_settings);
    nm_fields_unset_status(fields);

    form = nm_form_new(form_data, fields);
    nm_form_post(form);

    if (nm_form_draw(&form) != NM_OK)
        goto out;

    last_mac = nm_form_get_last_mac();

    if (nm_edit_vm_get_data(&vm, &cur_settings) != NM_OK)
        goto out;

    nm_edit_vm_update_db(&vm, &cur_settings, last_mac);

out:
    NM_FORM_EXIT();
    nm_vm_free(&vm);
    nm_form_free(form);
    nm_form_data_free(form_data);
    nm_fields_free(fields);
    nm_vmctl_free_data(&cur_settings);
}

static void nm_edit_vm_fields_setup(const nm_vmctl_data_t *cur)
{
    nm_str_t buf = NM_INIT_STR;

    const char **machs = nm_mach_get(nm_vect_str(&cur->main, NM_SQL_ARCH));
    field_opts_off(fields[NM_FLD_ARGS], O_STATIC);

    set_field_type(fields[NM_FLD_CPUNUM], TYPE_REGEXP, "^[0-9]{1}(:[0-9]{1})?(:[0-9]{1})? *$");
    set_field_type(fields[NM_FLD_RAMTOT], TYPE_INTEGER, 0, 4, nm_hw_total_ram());
    set_field_type(fields[NM_FLD_KVMFLG], TYPE_ENUM, nm_form_yes_no, false, false);
    set_field_type(fields[NM_FLD_HOSCPU], TYPE_ENUM, nm_form_yes_no, false, false);
    set_field_type(fields[NM_FLD_IFSCNT], TYPE_INTEGER, 1, 0, 64);
    set_field_type(fields[NM_FLD_DISKIN], TYPE_ENUM, nm_form_drive_drv, false, false);
    set_field_type(fields[NM_FLD_DISCARD], TYPE_ENUM, nm_form_yes_no, false, false);
    set_field_type(fields[NM_FLD_USBUSE], TYPE_ENUM, nm_form_yes_no, false, false);
    set_field_type(fields[NM_FLD_USBTYP], TYPE_ENUM, nm_form_usbtype, false, false);
    set_field_type(fields[NM_FLD_MACH], TYPE_ENUM, machs, false, false);
    if (machs == NULL)
        field_opts_off(fields[NM_FLD_MACH], O_ACTIVE);


    set_field_buffer(fields[NM_FLD_CPUNUM], 0, nm_vect_str_ctx(&cur->main, NM_SQL_SMP));
    set_field_buffer(fields[NM_FLD_RAMTOT], 0, nm_vect_str_ctx(&cur->main, NM_SQL_MEM));

    if (nm_str_cmp_st(nm_vect_str(&cur->main, NM_SQL_KVM), NM_ENABLE) == NM_OK)
        set_field_buffer(fields[NM_FLD_KVMFLG], 0, nm_form_yes_no[0]);
    else
        set_field_buffer(fields[NM_FLD_KVMFLG], 0, nm_form_yes_no[1]);

    if (nm_str_cmp_st(nm_vect_str(&cur->main, NM_SQL_HCPU), NM_ENABLE) == NM_OK)
        set_field_buffer(fields[NM_FLD_HOSCPU], 0, nm_form_yes_no[0]);
    else
        set_field_buffer(fields[NM_FLD_HOSCPU], 0, nm_form_yes_no[1]);

    nm_str_format(&buf, "%zu", cur->ifs.n_memb / NM_IFS_IDX_COUNT);
    set_field_buffer(fields[NM_FLD_IFSCNT], 0, buf.data);
    set_field_buffer(fields[NM_FLD_DISKIN], 0, nm_vect_str_ctx(&cur->drives, NM_SQL_DRV_TYPE));
    if (nm_str_cmp_st(nm_vect_str(&cur->drives, NM_SQL_DRV_DISC), NM_ENABLE) == NM_OK)
        set_field_buffer(fields[NM_FLD_DISCARD], 0, nm_form_yes_no[0]);
    else
        set_field_buffer(fields[NM_FLD_DISCARD], 0, nm_form_yes_no[1]);

    if (nm_str_cmp_st(nm_vect_str(&cur->main, NM_SQL_USBF), NM_ENABLE) == NM_OK)
        set_field_buffer(fields[NM_FLD_USBUSE], 0, nm_form_yes_no[0]);
    else
        set_field_buffer(fields[NM_FLD_USBUSE], 0, nm_form_yes_no[1]);

    set_field_buffer(fields[NM_FLD_USBTYP], 0, nm_vect_str_ctx(&cur->main, NM_SQL_USBT));
    set_field_buffer(fields[NM_FLD_MACH], 0, nm_vect_str_ctx(&cur->main, NM_SQL_MACH));
    set_field_buffer(fields[NM_FLD_ARGS], 0, nm_vect_str_ctx(&cur->main, NM_SQL_ARGS));
    set_field_buffer(fields[NM_FLD_GROUP], 0, nm_vect_str_ctx(&cur->main, NM_SQL_GROUP));

#if defined (NM_OS_FREEBSD)
    field_opts_off(fields[NM_FLD_USBUSE], O_ACTIVE);
#endif

    nm_str_free(&buf);
}

static size_t nm_edit_vm_labels_setup()
{
    nm_str_t buf = NM_INIT_STR;
    size_t max_label_len = 0;
    size_t msg_len = 0;

    for (size_t n = 0; n < NM_FLD_COUNT; n++) {
        switch (n) {
        case NM_LBL_CPUNUM:
            nm_str_format(&buf, "%s", _(NM_VM_FORM_CPU));
            break;
        case NM_LBL_RAMTOT:
            nm_str_format(&buf, "%s%u%s",
                _(NM_VM_FORM_MEM_BEGIN), nm_hw_total_ram(), _(NM_VM_FORM_MEM_END));
            break;
        case NM_LBL_KVMFLG:
            nm_str_format(&buf, "%s", _(NM_VM_FORM_KVM));
            break;
        case NM_LBL_HOSCPU:
            nm_str_format(&buf, "%s", _(NM_VM_FORM_HCPU));
            break;
        case NM_LBL_IFSCNT:
            nm_str_format(&buf, "%s", _(NM_VM_FORM_NET_IFS));
            break;
        case NM_LBL_DISKIN:
            nm_str_format(&buf, "%s", _(NM_VM_FORM_DRV_IF));
            break;
        case NM_LBL_DISCARD:
            nm_str_format(&buf, "%s", _(NM_VM_FORM_DRV_DIS));
            break;
        case NM_LBL_USBUSE:
            nm_str_format(&buf, "%s", _(NM_VM_FORM_USB));
            break;
        case NM_LBL_USBTYP:
            nm_str_format(&buf, "%s", _(NM_VM_FORM_USBT));
            break;
        case NM_LBL_MACH:
            nm_str_format(&buf, "%s", _(NM_VM_FORM_MACH));
            break;
        case NM_LBL_ARGS:
            nm_str_format(&buf, "%s", _(NM_VM_FORM_ARGS));
            break;
        case NM_LBL_GROUP:
            nm_str_format(&buf, "%s", _(NM_VM_FORM_GROUP));
            break;
        default:
            continue;
        }

        msg_len = mbstowcs(NULL, buf.data, buf.len);
        if (msg_len > max_label_len)
            max_label_len = msg_len;

        if (fields[n])
            set_field_buffer(fields[n], 0, buf.data);
    }

    nm_str_free(&buf);
    return max_label_len;
}

static int nm_edit_vm_get_data(nm_vm_t *vm, const nm_vmctl_data_t *cur)
{
    int rc;
    nm_vect_t err = NM_INIT_VECT;

    nm_str_t ifs = NM_INIT_STR;
    nm_str_t usb = NM_INIT_STR;
    nm_str_t kvm = NM_INIT_STR;
    nm_str_t hcpu = NM_INIT_STR;
    nm_str_t discard = NM_INIT_STR;

    nm_get_field_buf(fields[NM_FLD_CPUNUM], &vm->cpus);
    nm_get_field_buf(fields[NM_FLD_RAMTOT], &vm->memo);
    nm_get_field_buf(fields[NM_FLD_KVMFLG], &kvm);
    nm_get_field_buf(fields[NM_FLD_HOSCPU], &hcpu);
    nm_get_field_buf(fields[NM_FLD_IFSCNT], &ifs);
    nm_get_field_buf(fields[NM_FLD_DISKIN], &vm->drive.driver);
    nm_get_field_buf(fields[NM_FLD_DISCARD], &discard);
    nm_get_field_buf(fields[NM_FLD_USBUSE], &usb);
    nm_get_field_buf(fields[NM_FLD_USBTYP], &vm->usb_type);
    nm_get_field_buf(fields[NM_FLD_MACH], &vm->mach);
    nm_get_field_buf(fields[NM_FLD_ARGS], &vm->cmdappend);
    nm_get_field_buf(fields[NM_FLD_GROUP], &vm->group);

    if (field_status(fields[NM_FLD_CPUNUM]))
        nm_form_check_data(_("CPU cores"), vm->cpus, err);
    if (field_status(fields[NM_FLD_RAMTOT]))
        nm_form_check_data(_("Memory"), vm->memo, err);
    if (field_status(fields[NM_FLD_KVMFLG]))
        nm_form_check_data(_("KVM"), kvm, err);
    if (field_status(fields[NM_FLD_HOSCPU]))
        nm_form_check_data(_("Host CPU"), kvm, err);
    if (field_status(fields[NM_FLD_IFSCNT]))
        nm_form_check_data(_("Network interfaces"), ifs, err);
    if (field_status(fields[NM_FLD_DISKIN]))
        nm_form_check_data(_("Disk interface"), vm->drive.driver, err);
    if (field_status(fields[NM_FLD_DISCARD]))
        nm_form_check_data(_("Discard mode"), discard, err);
    if (field_status(fields[NM_FLD_USBUSE]))
        nm_form_check_data(_("USB"), usb, err);
    if (field_status(fields[NM_FLD_USBTYP]))
        nm_form_check_data(_("USB version"), vm->usb_type, err);

    if ((rc = nm_print_empty_fields(&err)) == NM_ERR)
        goto out;

    if (field_status(fields[NM_FLD_KVMFLG])) {
        if (nm_str_cmp_st(&kvm, "yes") == NM_OK) {
            vm->kvm.enable = 1;
        } else {
            if (!field_status(fields[NM_FLD_HOSCPU]) &&
                    (nm_str_cmp_st(nm_vect_str(&cur->main, NM_SQL_HCPU), NM_ENABLE) == NM_OK)) {
                rc = NM_ERR;
                NM_FORM_RESET();
                nm_warn(_(NM_MSG_HCPU_KVM));
                goto out;
            }
        }
    }

    if (field_status(fields[NM_FLD_DISCARD])) {
        if (nm_str_cmp_st(&discard, "yes") == NM_OK) {
            vm->drive.discard = 1;
        }
    }

    if (field_status(fields[NM_FLD_HOSCPU])) {
        if (nm_str_cmp_st(&hcpu, "yes") == NM_OK) {
            if (((!vm->kvm.enable) && (field_status(fields[NM_FLD_KVMFLG]))) ||
                    ((nm_str_cmp_st(nm_vect_str(&cur->main, NM_SQL_KVM), NM_DISABLE) == NM_OK) &&
                     !field_status(fields[NM_FLD_KVMFLG]))) {
                rc = NM_ERR;
                NM_FORM_RESET();
                nm_warn(_(NM_MSG_HCPU_KVM));
                goto out;
            }
            vm->kvm.hostcpu_enable = 1;
        }
    }

    if (field_status(fields[NM_FLD_IFSCNT]))
        vm->ifs.count = nm_str_stoui(&ifs, 10);

    if (field_status(fields[NM_FLD_USBUSE])) {
        if (nm_str_cmp_st(&usb, "yes") == NM_OK)
            vm->usb_enable = 1;
    }

out:
    nm_str_free(&ifs);
    nm_str_free(&usb);
    nm_str_free(&kvm);
    nm_str_free(&hcpu);
    nm_str_free(&discard);
    nm_vect_free(&err, NULL);

    return rc;
}

static void nm_edit_vm_update_db(nm_vm_t *vm, const nm_vmctl_data_t *cur, uint64_t mac)
{
    nm_str_t query = NM_INIT_STR;

    if (field_status(fields[NM_FLD_CPUNUM])) {
        nm_str_format(&query, "UPDATE vms SET smp='%s' WHERE name='%s'",
            vm->cpus.data, nm_vect_str_ctx(&cur->main, NM_SQL_NAME));
        nm_db_edit(query.data);
    }

    if (field_status(fields[NM_FLD_RAMTOT])) {
        nm_str_format(&query, "UPDATE vms SET mem='%s' WHERE name='%s'",
            vm->memo.data, nm_vect_str_ctx(&cur->main, NM_SQL_NAME));
        nm_db_edit(query.data);
    }

    if (field_status(fields[NM_FLD_KVMFLG])) {
        nm_str_format(&query, "UPDATE vms SET kvm='%s' WHERE name='%s'",
            vm->kvm.enable ? NM_ENABLE : NM_DISABLE,
            nm_vect_str_ctx(&cur->main, NM_SQL_NAME));
        nm_db_edit(query.data);
    }

    if (field_status(fields[NM_FLD_HOSCPU])) {
        nm_str_format(&query, "UPDATE vms SET hcpu='%s' WHERE name='%s'",
            vm->kvm.hostcpu_enable ? NM_ENABLE : NM_DISABLE,
            nm_vect_str_ctx(&cur->main, NM_SQL_NAME));
        nm_db_edit(query.data);
    }

    if (field_status(fields[NM_FLD_IFSCNT])) {
        size_t cur_count = cur->ifs.n_memb / NM_IFS_IDX_COUNT;

        if (vm->ifs.count < cur_count) {
            for (; cur_count > vm->ifs.count; cur_count--) {
                size_t idx_shift = NM_IFS_IDX_COUNT * (cur_count - 1);

                nm_str_format(&query, NM_DEL_IFACE_SQL,
                    nm_vect_str_ctx(&cur->main, NM_SQL_NAME),
                    nm_vect_str_ctx(&cur->ifs, NM_SQL_IF_NAME + idx_shift));
                nm_db_edit(query.data);
            }
        }

        if (vm->ifs.count > cur_count) {
            for (size_t n = cur_count; n < vm->ifs.count; n++) {
                int altname;
                nm_str_t if_name = NM_INIT_STR;
                nm_str_t if_name_copy = NM_INIT_STR;
                nm_str_t maddr = NM_INIT_STR;
                mac++;

                nm_net_mac_n2s(mac, &maddr);
                nm_str_format(&if_name, "%s_eth%zu",
                    nm_vect_str_ctx(&cur->main, NM_SQL_NAME), n);
                nm_str_copy(&if_name_copy, &if_name);

                altname = nm_net_fix_tap_name(&if_name, &maddr);

                nm_str_format(&query,
                    "INSERT INTO ifaces(vm_name, if_name, mac_addr, if_drv, vhost, macvtap, altname) "
                    "VALUES('%s', '%s', '%s', '%s', '%s', '%s', '%s')",
                    nm_vect_str_ctx(&cur->main, NM_SQL_NAME),
                    if_name.data,
                    maddr.data,
                    NM_DEFAULT_NETDRV,
#if defined (NM_OS_LINUX)
                    NM_ENABLE,
#else
                    NM_DISABLE,
#endif
                    NM_DISABLE,
                    (altname) ? if_name_copy.data : "");
                nm_db_edit(query.data);

                nm_str_free(&if_name);
                nm_str_free(&if_name_copy);
                nm_str_free(&maddr);
            }
        }
    }

    if (field_status(fields[NM_FLD_DISKIN])) {
        nm_str_format(&query, "UPDATE drives SET drive_drv='%s' WHERE vm_name='%s'",
            vm->drive.driver.data, nm_vect_str_ctx(&cur->main, NM_SQL_NAME));
        nm_db_edit(query.data);
    }

    if (field_status(fields[NM_FLD_DISCARD])) {
        nm_str_format(&query, "UPDATE drives SET discard='%s' WHERE vm_name='%s'",
            vm->drive.discard ? NM_ENABLE : NM_DISABLE,
            nm_vect_str_ctx(&cur->main, NM_SQL_NAME));
        nm_db_edit(query.data);
    }

    if (field_status(fields[NM_FLD_USBUSE])) {
        nm_str_format(&query, "UPDATE vms SET usb='%s' WHERE name='%s'",
            vm->usb_enable ? NM_ENABLE : NM_DISABLE,
            nm_vect_str_ctx(&cur->main, NM_SQL_NAME));
        nm_db_edit(query.data);
    }

    if (field_status(fields[NM_FLD_USBTYP])) {
        nm_str_format(&query, "UPDATE vms SET usb_type='%s' WHERE name='%s'",
            vm->usb_type.data, nm_vect_str_ctx(&cur->main, NM_SQL_NAME));
        nm_db_edit(query.data);
    }

    if (field_status(fields[NM_FLD_MACH])) {
        nm_str_format(&query, "UPDATE vms SET machine='%s' WHERE name='%s'",
            vm->mach.data, nm_vect_str_ctx(&cur->main, NM_SQL_NAME));
        nm_db_edit(query.data);
    }

    if (field_status(fields[NM_FLD_ARGS])) {
        nm_str_format(&query, "UPDATE vms SET cmdappend='%s' WHERE name='%s'",
            vm->cmdappend.data, nm_vect_str_ctx(&cur->main, NM_SQL_NAME));
        nm_db_edit(query.data);
    }

    if (field_status(fields[NM_FLD_GROUP])) {
        nm_str_format(&query, "UPDATE vms SET team='%s' WHERE name='%s'",
            vm->group.data, nm_vect_str_ctx(&cur->main, NM_SQL_NAME));
        nm_db_edit(query.data);
    }

    nm_str_free(&query);
}

/* vim:set ts=4 sw=4: */
