#include <nm_core.h>
#include <nm_form.h>
#include <nm_utils.h>
#include <nm_string.h>
#include <nm_vector.h>
#include <nm_window.h>
#include <nm_machine.h>
#include <nm_hw_info.h>
#include <nm_network.h>
#include <nm_database.h>
#include <nm_cfg_file.h>
#include <nm_vm_control.h>
#include <nm_qmp_control.h>
#include <nm_edit_vm.h>

static const char NM_VM_FORM_CPU_BEGIN[] = "CPU cores [1-";
static const char NM_VM_FORM_CPU_END[]   = "]";
static const char NM_VM_FORM_MEM_BEGIN[] = "Memory [4-";
static const char NM_VM_FORM_MEM_END[]   = "]Mb";
static const char NM_VM_FORM_KVM[]       = "KVM [yes/no]";
static const char NM_VM_FORM_HCPU[]      = "Host CPU [yes/no]";
static const char NM_VM_FORM_NET_IFS[]   = "Network interfaces";
static const char NM_VM_FORM_DRV_IF[]    = "Disk interface";
static const char NM_VM_FORM_USB[]       = "USB [yes/no]";
static const char NM_VM_FORM_USBT[]      = "USB version";
static const char NM_VM_FORM_MACH[]      = "Machine type";
static const char NM_VM_FORM_ARGS[]      = "Extra QEMU args";

static void nm_edit_vm_field_setup(const nm_vmctl_data_t *cur);
static void nm_edit_vm_field_names(nm_vect_t *msg);
static int nm_edit_vm_get_data(nm_vm_t *vm, const nm_vmctl_data_t *cur);
static void nm_edit_vm_update_db(nm_vm_t *vm, const nm_vmctl_data_t *cur, uint64_t mac);

enum {
    NM_FLD_CPUNUM = 0,
    NM_FLD_RAMTOT,
    NM_FLD_KVMFLG,
    NM_FLD_HOSCPU,
    NM_FLD_IFSCNT,
    NM_FLD_DISKIN,
    NM_FLD_USBUSE,
    NM_FLD_USBTYP,
    NM_FLD_MACH,
    NM_FLD_ARGS,
    NM_FLD_COUNT
};

static nm_form_t *form = NULL;
static nm_field_t *fields[NM_FLD_COUNT + 1];

void nm_edit_vm(const nm_str_t *name)
{
    nm_vm_t vm = NM_INIT_VM;
    nm_vect_t msg_fields = NM_INIT_VECT;
    nm_vmctl_data_t cur_settings = NM_VMCTL_INIT_DATA;
    nm_form_data_t form_data = NM_INIT_FORM_DATA;
    uint64_t last_mac;
    size_t msg_len;

    nm_edit_vm_field_names(&msg_fields);
    msg_len = nm_max_msg_len((const char **) msg_fields.data);

    if (nm_form_calc_size(msg_len, NM_FLD_COUNT, &form_data) != NM_OK)
        return;

    werase(action_window);
    werase(help_window);
    nm_init_action(_(NM_MSG_EDIT_VM));
    nm_init_help_edit();

    nm_vmctl_get_data(name, &cur_settings);

    for (size_t n = 0; n < NM_FLD_COUNT; ++n)
        fields[n] = new_field(1, form_data.form_len, n * 2, 0, 0, 0);

    fields[NM_FLD_COUNT] = NULL;

    nm_edit_vm_field_setup(&cur_settings);
    for (size_t n = 0, y = 1, x = 2; n < NM_FLD_COUNT; n++)
    {
        mvwaddstr(form_data.form_window, y, x, msg_fields.data[n]);
        y += 2;
    }

    form = nm_post_form(form_data.form_window, fields, msg_len + 4, NM_TRUE);

    if (nm_draw_form(action_window, form) != NM_OK)
        goto out;

    last_mac = nm_form_get_last_mac();

    if (nm_edit_vm_get_data(&vm, &cur_settings) != NM_OK)
        goto out;

    nm_edit_vm_update_db(&vm, &cur_settings, last_mac);

out:
    NM_FORM_EXIT();
    nm_vm_free(&vm);
    nm_vect_free(&msg_fields, NULL);
    nm_form_free(form, fields);
    nm_vmctl_free_data(&cur_settings);
}

static void nm_edit_vm_field_setup(const nm_vmctl_data_t *cur)
{
    nm_str_t buf = NM_INIT_STR;
    const char **machs = NULL;

    machs = nm_mach_get(nm_vect_str(&cur->main, NM_SQL_ARCH));

    set_field_type(fields[NM_FLD_CPUNUM], TYPE_INTEGER, 0, 1, nm_hw_ncpus());
    set_field_type(fields[NM_FLD_RAMTOT], TYPE_INTEGER, 0, 4, nm_hw_total_ram());
    set_field_type(fields[NM_FLD_KVMFLG], TYPE_ENUM, nm_form_yes_no, false, false);
    set_field_type(fields[NM_FLD_HOSCPU], TYPE_ENUM, nm_form_yes_no, false, false);
    set_field_type(fields[NM_FLD_IFSCNT], TYPE_INTEGER, 1, 0, 64);
    set_field_type(fields[NM_FLD_DISKIN], TYPE_ENUM, nm_form_drive_drv, false, false);
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

    if (nm_str_cmp_st(nm_vect_str(&cur->main, NM_SQL_USBF), NM_ENABLE) == NM_OK)
        set_field_buffer(fields[NM_FLD_USBUSE], 0, nm_form_yes_no[0]);
    else
        set_field_buffer(fields[NM_FLD_USBUSE], 0, nm_form_yes_no[1]);

    if (nm_str_cmp_st(nm_vect_str(&cur->main, NM_SQL_USBT), NM_DEFAULT_USBVER) == NM_OK)
        set_field_buffer(fields[NM_FLD_USBTYP], 0, nm_form_usbtype[1]);
    else
        set_field_buffer(fields[NM_FLD_USBTYP], 0, nm_form_usbtype[0]);

    set_field_buffer(fields[NM_FLD_MACH], 0, nm_vect_str_ctx(&cur->main, NM_SQL_MACH));
    set_field_buffer(fields[NM_FLD_ARGS], 0, nm_vect_str_ctx(&cur->main, NM_SQL_ARGS));

    for (size_t n = 0; n < NM_FLD_COUNT; n++)
        set_field_status(fields[n], 0);

#if defined (NM_OS_FREEBSD)
    field_opts_off(fields[NM_FLD_USBUSE], O_ACTIVE);
#endif

    nm_str_free(&buf);
}

static void nm_edit_vm_field_names(nm_vect_t *msg)
{
    nm_str_t buf = NM_INIT_STR;

    nm_str_format(&buf, "%s%u%s",
        _(NM_VM_FORM_CPU_BEGIN), nm_hw_ncpus(), _(NM_VM_FORM_CPU_END));
    nm_vect_insert(msg, buf.data, buf.len + 1, NULL);

    nm_str_format(&buf, "%s%u%s",
        _(NM_VM_FORM_MEM_BEGIN), nm_hw_total_ram(), _(NM_VM_FORM_MEM_END));
    nm_vect_insert(msg, buf.data, buf.len + 1, NULL);

    nm_vect_insert(msg, _(NM_VM_FORM_KVM), strlen(_(NM_VM_FORM_KVM)) + 1, NULL);
    nm_vect_insert(msg, _(NM_VM_FORM_HCPU), strlen(_(NM_VM_FORM_HCPU)) + 1, NULL);
    nm_vect_insert(msg, _(NM_VM_FORM_NET_IFS), strlen(_(NM_VM_FORM_NET_IFS)) + 1, NULL);
    nm_vect_insert(msg, _(NM_VM_FORM_DRV_IF), strlen(_(NM_VM_FORM_DRV_IF)) + 1, NULL);
    nm_vect_insert(msg, _(NM_VM_FORM_USB), strlen(_(NM_VM_FORM_USB)) + 1, NULL);
    nm_vect_insert(msg, _(NM_VM_FORM_USBT), strlen(_(NM_VM_FORM_USBT)) + 1, NULL);
    nm_vect_insert(msg, _(NM_VM_FORM_MACH), strlen(_(NM_VM_FORM_MACH)) + 1, NULL);
    nm_vect_insert(msg, _(NM_VM_FORM_ARGS), strlen(_(NM_VM_FORM_ARGS)) + 1, NULL);
    nm_vect_end_zero(msg);

    nm_str_free(&buf);
}

static int nm_edit_vm_get_data(nm_vm_t *vm, const nm_vmctl_data_t *cur)
{
    int rc = NM_OK;
    nm_vect_t err = NM_INIT_VECT;

    nm_str_t ifs = NM_INIT_STR;
    nm_str_t usb = NM_INIT_STR;
    nm_str_t usbv = NM_INIT_STR;
    nm_str_t kvm = NM_INIT_STR;
    nm_str_t hcpu = NM_INIT_STR;

    nm_get_field_buf(fields[NM_FLD_CPUNUM], &vm->cpus);
    nm_get_field_buf(fields[NM_FLD_RAMTOT], &vm->memo);
    nm_get_field_buf(fields[NM_FLD_KVMFLG], &kvm);
    nm_get_field_buf(fields[NM_FLD_HOSCPU], &hcpu);
    nm_get_field_buf(fields[NM_FLD_IFSCNT], &ifs);
    nm_get_field_buf(fields[NM_FLD_DISKIN], &vm->drive.driver);
    nm_get_field_buf(fields[NM_FLD_USBUSE], &usb);
    nm_get_field_buf(fields[NM_FLD_USBTYP], &usbv);
    nm_get_field_buf(fields[NM_FLD_MACH], &vm->mach);
    nm_get_field_buf(fields[NM_FLD_ARGS], &vm->cmdappend);

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
    if (field_status(fields[NM_FLD_USBUSE]))
        nm_form_check_data(_("USB"), usb, err);
    if (field_status(fields[NM_FLD_USBTYP]))
        nm_form_check_data(_("USB version"), usbv, err);

    if ((rc = nm_print_empty_fields(&err)) == NM_ERR)
        goto out;

    if (field_status(fields[NM_FLD_KVMFLG]))
    {
        if (nm_str_cmp_st(&kvm, "yes") == NM_OK)
            vm->kvm.enable = 1;
        else
        {
            if (!field_status(fields[NM_FLD_HOSCPU]) &&
                (nm_str_cmp_st(nm_vect_str(&cur->main, NM_SQL_HCPU), NM_ENABLE) == NM_OK))
            {
                rc = NM_ERR;
                NM_FORM_RESET();
                nm_warn(_(NM_MSG_HCPU_KVM));
                goto out;
            }
        }
    }

    if (field_status(fields[NM_FLD_HOSCPU]))
    {
        if (nm_str_cmp_st(&hcpu, "yes") == NM_OK)
        {
            if (((!vm->kvm.enable) && (field_status(fields[NM_FLD_KVMFLG]))) ||
                ((nm_str_cmp_st(nm_vect_str(&cur->main, NM_SQL_KVM), NM_DISABLE) == NM_OK) &&
                 !field_status(fields[NM_FLD_KVMFLG])))
            {
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

    if (field_status(fields[NM_FLD_USBUSE]))
    {
        if (nm_str_cmp_st(&usb, "yes") == NM_OK)
            vm->usb_enable = 1;
    }

    if (field_status(fields[NM_FLD_USBTYP]))
    {
        if (nm_str_cmp_st(&usbv, NM_DEFAULT_USBVER) == NM_OK)
            vm->usb_xhci = 1;
    }

out:
    nm_str_free(&ifs);
    nm_str_free(&usb);
    nm_str_free(&usbv);
    nm_str_free(&kvm);
    nm_str_free(&hcpu);
    nm_vect_free(&err, NULL);

    return rc;
}

static void nm_edit_vm_update_db(nm_vm_t *vm, const nm_vmctl_data_t *cur, uint64_t mac)
{
    nm_str_t query = NM_INIT_STR;

    if (field_status(fields[NM_FLD_CPUNUM]))
    {
        nm_str_format(&query, "UPDATE vms SET smp='%s' WHERE name='%s'",
            vm->cpus.data, nm_vect_str_ctx(&cur->main, NM_SQL_NAME));
        nm_db_edit(query.data);
    }

    if (field_status(fields[NM_FLD_RAMTOT]))
    {
        nm_str_format(&query, "UPDATE vms SET mem='%s' WHERE name='%s'",
            vm->memo.data, nm_vect_str_ctx(&cur->main, NM_SQL_NAME));
        nm_db_edit(query.data);
    }

    if (field_status(fields[NM_FLD_KVMFLG]))
    {
        nm_str_format(&query, "UPDATE vms SET kvm='%s' WHERE name='%s'",
            vm->kvm.enable ? NM_ENABLE : NM_DISABLE,
            nm_vect_str_ctx(&cur->main, NM_SQL_NAME));
        nm_db_edit(query.data);
    }

    if (field_status(fields[NM_FLD_HOSCPU]))
    {
        nm_str_format(&query, "UPDATE vms SET hcpu='%s' WHERE name='%s'",
            vm->kvm.hostcpu_enable ? NM_ENABLE : NM_DISABLE,
            nm_vect_str_ctx(&cur->main, NM_SQL_NAME));
        nm_db_edit(query.data);
    }

    if (field_status(fields[NM_FLD_IFSCNT]))
    {
        size_t cur_count = cur->ifs.n_memb / NM_IFS_IDX_COUNT;
        int altname;

        if (vm->ifs.count < cur_count)
        {

            for (; cur_count > vm->ifs.count; cur_count--)
            {
                size_t idx_shift = NM_IFS_IDX_COUNT * (cur_count - 1);

                nm_str_format(&query, NM_DEL_IFACE_SQL,
                    nm_vect_str_ctx(&cur->main, NM_SQL_NAME),
                    nm_vect_str_ctx(&cur->ifs, NM_SQL_IF_NAME + idx_shift));
                nm_db_edit(query.data);
            }
        }

        if (vm->ifs.count > cur_count)
        {
            for (size_t n = cur_count; n < vm->ifs.count; n++)
            {
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

    if (field_status(fields[NM_FLD_DISKIN]))
    {
        nm_str_format(&query, "UPDATE drives SET drive_drv='%s' WHERE vm_name='%s'",
            vm->drive.driver.data, nm_vect_str_ctx(&cur->main, NM_SQL_NAME));
        nm_db_edit(query.data);
    }

    if (field_status(fields[NM_FLD_USBUSE]))
    {
        nm_str_format(&query, "UPDATE vms SET usb='%s' WHERE name='%s'",
            vm->usb_enable ? NM_ENABLE : NM_DISABLE,
            nm_vect_str_ctx(&cur->main, NM_SQL_NAME));
        nm_db_edit(query.data);
    }

    if (field_status(fields[NM_FLD_USBTYP]))
    {
        nm_str_format(&query, "UPDATE vms SET usb_type='%s' WHERE name='%s'",
            vm->usb_xhci ? nm_form_usbtype[1] : nm_form_usbtype[0],
            nm_vect_str_ctx(&cur->main, NM_SQL_NAME));
        nm_db_edit(query.data);
    }

    if (field_status(fields[NM_FLD_MACH]))
    {
        nm_str_format(&query, "UPDATE vms SET machine='%s' WHERE name='%s'",
            vm->mach.data, nm_vect_str_ctx(&cur->main, NM_SQL_NAME));
        nm_db_edit(query.data);
    }

    if (field_status(fields[NM_FLD_ARGS]))
    {
        nm_str_format(&query, "UPDATE vms SET cmdappend='%s' WHERE name='%s'",
            vm->cmdappend.data, nm_vect_str_ctx(&cur->main, NM_SQL_NAME));
        nm_db_edit(query.data);
    }


    nm_str_free(&query);
}

/* vim:set ts=4 sw=4: */
