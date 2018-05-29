#include <nm_core.h>
#include <nm_form.h>
#include <nm_utils.h>
#include <nm_string.h>
#include <nm_vector.h>
#include <nm_window.h>
#include <nm_hw_info.h>
#include <nm_network.h>
#include <nm_database.h>
#include <nm_cfg_file.h>
#include <nm_vm_control.h>
#include <nm_qmp_control.h>

#define NM_EDIT_VM_FIELDS_NUM 8

static nm_window_t *window = NULL;
static nm_form_t *form = NULL;
static nm_field_t *fields[NM_EDIT_VM_FIELDS_NUM + 1];

static const char *nm_form_msg[] = {
    "KVM [yes/no]",
    "Host CPU [yes/no]",
    "Network ",
    "Disk interface",
    "USB [yes/no]",
    "Sync mouse position",
    NULL
};

static void nm_edit_vm_field_setup(const nm_vmctl_data_t *cur);
static void nm_edit_vm_field_names(const nm_str_t *name, nm_window_t *w);
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
    NM_FLD_MOUSES
};

void nm_edit_vm(const nm_str_t *name)
{
    nm_vm_t vm = NM_INIT_VM;
    nm_vmctl_data_t cur_settings = NM_VMCTL_INIT_DATA;
    nm_spinner_data_t sp_data = NM_INIT_SPINNER;
    nm_form_data_t form_data = NM_INIT_FORM_DATA;
    uint64_t last_mac;
    pthread_t spin_th;
    int done = 0;
    size_t msg_len;

    msg_len = nm_max_msg_len(nm_form_msg);

    if (nm_form_calc_size(msg_len, NM_EDIT_VM_FIELDS_NUM, &form_data) != NM_OK)
        return;

    nm_vmctl_get_data(name, &cur_settings);

    for (size_t n = 0; n < NM_EDIT_VM_FIELDS_NUM; ++n)
        fields[n] = new_field(1, form_data.form_len, n * 2, 0, 0, 0);

    fields[NM_EDIT_VM_FIELDS_NUM] = NULL;

    nm_edit_vm_field_setup(&cur_settings);
    nm_edit_vm_field_names(name, form_data.form_window);

    form = nm_post_form__(form_data.form_window, fields, msg_len + 4, NM_TRUE);

    if (nm_draw_form(action_window, form) != NM_OK)
        goto out;
    
    nm_form_get_last(&last_mac, NULL);

    if (nm_edit_vm_get_data(&vm, &cur_settings) != NM_OK)
        goto out;

    msg_len = mbstowcs(NULL, _(NM_EDIT_TITLE), strlen(_(NM_EDIT_TITLE)));
    sp_data.stop = &done;
    sp_data.x = (getmaxx(stdscr) + msg_len + 2) / 2;

    //if (pthread_create(&spin_th, NULL, nm_spinner, (void *) &sp_data) != 0)
    //    nm_bug(_("%s: cannot create thread"), __func__);

    nm_edit_vm_update_db(&vm, &cur_settings, last_mac);

    //done = 1;
    //if (pthread_join(spin_th, NULL) != 0)
     //   nm_bug(_("%s: cannot join thread"), __func__);

out:
    nm_vm_free(&vm);
    nm_form_free(form, fields);
    delwin(form_data.form_window);
    nm_vmctl_free_data(&cur_settings);
}

static void nm_edit_vm_field_setup(const nm_vmctl_data_t *cur)
{
    nm_str_t buf = NM_INIT_STR;

    set_field_type(fields[NM_FLD_CPUNUM], TYPE_INTEGER, 0, 1, nm_hw_ncpus());
    set_field_type(fields[NM_FLD_RAMTOT], TYPE_INTEGER, 0, 4, nm_hw_total_ram());
    set_field_type(fields[NM_FLD_KVMFLG], TYPE_ENUM, nm_form_yes_no, false, false);
    set_field_type(fields[NM_FLD_HOSCPU], TYPE_ENUM, nm_form_yes_no, false, false);
    set_field_type(fields[NM_FLD_IFSCNT], TYPE_INTEGER, 1, 0, 64);
    set_field_type(fields[NM_FLD_DISKIN], TYPE_ENUM, nm_form_drive_drv, false, false);
    set_field_type(fields[NM_FLD_USBUSE], TYPE_ENUM, nm_form_yes_no, false, false);
    set_field_type(fields[NM_FLD_MOUSES], TYPE_ENUM, nm_form_yes_no, false, false);

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
    
    if (nm_str_cmp_st(nm_vect_str(&cur->main, NM_SQL_OVER), NM_ENABLE) == NM_OK)
        set_field_buffer(fields[NM_FLD_MOUSES], 0, nm_form_yes_no[0]);
    else
        set_field_buffer(fields[NM_FLD_MOUSES], 0, nm_form_yes_no[1]);

    for (size_t n = 0; n < NM_EDIT_VM_FIELDS_NUM; n++)
        set_field_status(fields[n], 0);

#if defined (NM_OS_FREEBSD)
    field_opts_off(fields[NM_FLD_USBUSE], O_ACTIVE);
#endif

    nm_str_free(&buf);
}

static void nm_edit_vm_field_names(const nm_str_t *name, nm_window_t *w)
{
    int y = 1, x = 2, mult = 2;
    nm_str_t buf = NM_INIT_STR;

    nm_str_add_text(&buf, _("CPU cores [1-"));
    nm_str_format(&buf, "%u", nm_hw_ncpus());
    nm_str_add_char(&buf, ']');
    mvwaddstr(w, y, x, buf.data);
    nm_str_trunc(&buf, 0);

    nm_str_add_text(&buf, _("Memory [4-"));
    nm_str_format(&buf, "%u", nm_hw_total_ram());
    nm_str_add_text(&buf, "]Mb");
    mvwaddstr(w, y += mult, x, buf.data);

    for (size_t n = 0; n < 6; n++)
        mvwaddstr(w, y += mult, x, _(nm_form_msg[n]));

    nm_str_free(&buf);
}

static int nm_edit_vm_get_data(nm_vm_t *vm, const nm_vmctl_data_t *cur)
{
    int rc = NM_OK;
    nm_vect_t err = NM_INIT_VECT;

    nm_str_t ifs = NM_INIT_STR;
    nm_str_t usb = NM_INIT_STR;
    nm_str_t kvm = NM_INIT_STR;
    nm_str_t hcpu = NM_INIT_STR;
    nm_str_t sync = NM_INIT_STR;

    nm_get_field_buf(fields[NM_FLD_CPUNUM], &vm->cpus);
    nm_get_field_buf(fields[NM_FLD_RAMTOT], &vm->memo);
    nm_get_field_buf(fields[NM_FLD_KVMFLG], &kvm);
    nm_get_field_buf(fields[NM_FLD_HOSCPU], &hcpu);
    nm_get_field_buf(fields[NM_FLD_IFSCNT], &ifs);
    nm_get_field_buf(fields[NM_FLD_DISKIN], &vm->drive.driver);
    nm_get_field_buf(fields[NM_FLD_USBUSE], &usb);
    nm_get_field_buf(fields[NM_FLD_MOUSES], &sync);

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
    if (field_status(fields[NM_FLD_MOUSES]))
        nm_form_check_data(_("Sync mouse position"), sync, err);

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
                nm_print_warn(3, 6, _("Host CPU requires KVM enabled"));
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
                nm_print_warn(3, 6, _("Host CPU requires KVM enabled"));
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

    if (nm_str_cmp_st(&sync, "yes") == NM_OK)
        vm->mouse_sync = 1;

out:
    nm_str_free(&ifs);
    nm_str_free(&usb);
    nm_str_free(&kvm);
    nm_str_free(&hcpu);
    nm_str_free(&sync);
    nm_vect_free(&err, NULL);

    return rc;
}

static void nm_edit_vm_update_db(nm_vm_t *vm, const nm_vmctl_data_t *cur, uint64_t mac)
{
    nm_str_t query = NM_INIT_STR;

    if (field_status(fields[NM_FLD_CPUNUM]))
    {
        nm_str_add_text(&query, "UPDATE vms SET smp='");
        nm_str_add_str(&query, &vm->cpus);
        nm_str_add_text(&query, "' WHERE name='");
        nm_str_add_str(&query, nm_vect_str(&cur->main, NM_SQL_NAME));
        nm_str_add_char(&query, '\'');
        nm_db_edit(query.data);
        nm_str_trunc(&query, 0);
    }

    if (field_status(fields[NM_FLD_RAMTOT]))
    {
        nm_str_add_text(&query, "UPDATE vms SET mem='");
        nm_str_add_str(&query, &vm->memo);
        nm_str_add_text(&query, "' WHERE name='");
        nm_str_add_str(&query, nm_vect_str(&cur->main, NM_SQL_NAME));
        nm_str_add_char(&query, '\'');
        nm_db_edit(query.data);
        nm_str_trunc(&query, 0);
    }

    if (field_status(fields[NM_FLD_KVMFLG]))
    {
        nm_str_add_text(&query, "UPDATE vms SET kvm='");
        nm_str_add_text(&query, vm->kvm.enable ? NM_ENABLE : NM_DISABLE);
        nm_str_add_text(&query, "' WHERE name='");
        nm_str_add_str(&query, nm_vect_str(&cur->main, NM_SQL_NAME));
        nm_str_add_char(&query, '\'');
        nm_db_edit(query.data);
        nm_str_trunc(&query, 0);
    }

    if (field_status(fields[NM_FLD_HOSCPU]))
    {
        nm_str_add_text(&query, "UPDATE vms SET hcpu='");
        nm_str_add_text(&query, vm->kvm.hostcpu_enable ? NM_ENABLE : NM_DISABLE);
        nm_str_add_text(&query, "' WHERE name='");
        nm_str_add_str(&query, nm_vect_str(&cur->main, NM_SQL_NAME));
        nm_str_add_char(&query, '\'');
        nm_db_edit(query.data);
        nm_str_trunc(&query, 0);
    }

    if (field_status(fields[NM_FLD_IFSCNT]))
    {
        size_t cur_count = cur->ifs.n_memb / NM_IFS_IDX_COUNT;

        if (vm->ifs.count < cur_count)
        {

            for (; cur_count > vm->ifs.count; cur_count--)
            {
                size_t idx_shift = NM_IFS_IDX_COUNT * (cur_count - 1);

                nm_str_add_text(&query, "DELETE FROM ifaces WHERE vm_name='");
                nm_str_add_str(&query, nm_vect_str(&cur->main, NM_SQL_NAME));
                nm_str_add_text(&query, "' AND if_name='");
                nm_str_add_str(&query, nm_vect_str(&cur->ifs, NM_SQL_IF_NAME + idx_shift));
                nm_str_add_char(&query, '\'');
                nm_db_edit(query.data);

                nm_str_trunc(&query, 0);
            }
        }

        if (vm->ifs.count > cur_count)
        {
            for (size_t n = cur_count; n < vm->ifs.count; n++)
            {
                nm_str_t if_name = NM_INIT_STR;
                nm_str_t maddr = NM_INIT_STR;
                mac++;

                nm_net_mac_n2a(mac, &maddr);
                nm_str_format(&if_name, "%s_eth%zu",
                    nm_vect_str_ctx(&cur->main, NM_SQL_NAME), n);

                if (if_name.len > 15) /* Linux tap iface max name len */
                {
                    nm_str_trunc(&if_name, 14);
                    nm_str_format(&if_name, "%zu", n);
                }

                nm_str_add_text(&query, "INSERT INTO ifaces("
                    "vm_name, if_name, mac_addr, if_drv, vhost, macvtap) VALUES('");
                nm_str_add_str(&query, nm_vect_str(&cur->main, NM_SQL_NAME));
                nm_str_add_text(&query, "', '");
                nm_str_add_str(&query, &if_name);
                nm_str_add_text(&query, "', '");
                nm_str_add_str(&query, &maddr);
#if defined (NM_OS_LINUX)
                nm_str_add_text(&query, "', '" NM_DEFAULT_NETDRV "', '1', '0')");
#else
                nm_str_add_text(&query, "', '" NM_DEFAULT_NETDRV "', '0', '0')");
#endif

                nm_db_edit(query.data);
                
                nm_str_free(&if_name);
                nm_str_free(&maddr);
                nm_str_trunc(&query, 0);
            }

            nm_form_update_last_mac(mac);
        }
    }

    if (field_status(fields[NM_FLD_DISKIN]))
    {
        nm_str_add_text(&query, "UPDATE drives SET drive_drv='");
        nm_str_add_str(&query, &vm->drive.driver);
        nm_str_add_text(&query, "' WHERE vm_name='");
        nm_str_add_str(&query, nm_vect_str(&cur->main, NM_SQL_NAME));
        nm_str_add_char(&query, '\'');

        nm_db_edit(query.data);

        nm_str_trunc(&query, 0);
    }

    if (field_status(fields[NM_FLD_USBUSE]))
    {
        nm_str_add_text(&query, "UPDATE vms SET usb='");
        nm_str_add_text(&query, vm->usb_enable ? NM_ENABLE : NM_DISABLE);
        nm_str_add_text(&query, "' WHERE name='");
        nm_str_add_str(&query, nm_vect_str(&cur->main, NM_SQL_NAME));
        nm_str_add_char(&query, '\'');

        nm_db_edit(query.data);
        
        nm_str_trunc(&query, 0);
    }

    if (field_status(fields[NM_FLD_MOUSES]))
    {
        nm_str_add_text(&query, "UPDATE vms SET mouse_override='");
        nm_str_add_text(&query, vm->mouse_sync ? NM_ENABLE : NM_DISABLE);
        nm_str_add_text(&query, "' WHERE name='");
        nm_str_add_str(&query, nm_vect_str(&cur->main, NM_SQL_NAME));
        nm_str_add_char(&query, '\'');

        nm_db_edit(query.data);
    }

    nm_str_free(&query);
}

/* vim:set ts=4 sw=4 fdm=marker: */
