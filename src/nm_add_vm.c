#include <nm_core.h>
#include <nm_form.h>
#include <nm_utils.h>
#include <nm_string.h>
#include <nm_window.h>
#include <nm_add_vm.h>
#include <nm_hw_info.h>
#include <nm_network.h>
#include <nm_database.h>
#include <nm_cfg_file.h>
#include <nm_usb_devices.h>

#define NM_ADD_VM_FIELDS_NUM 11
#define NM_INSTALL_VM 0
#define NM_IMPORT_VM  1

static nm_window_t *window = NULL;
static nm_form_t *form = NULL;
static nm_field_t *fields[NM_ADD_VM_FIELDS_NUM + 1];

static void nm_add_vm_field_setup(const nm_vect_t *usb_names, int import);
static void nm_add_vm_field_names(nm_window_t *w, int import);
static int nm_add_vm_get_data(nm_vm_t *vm, const nm_vect_t *usb_devs, int import);
static void nm_add_vm_to_db(nm_vm_t *vm, uint64_t mac, int import);
static void nm_add_vm_to_fs(nm_vm_t *vm, int import);
static void nm_add_vm_main(int import);

enum {
    NM_FLD_VMNAME = 0,
    NM_FLD_VMARCH,
    NM_FLD_CPUNUM,
    NM_FLD_RAMTOT,
    NM_FLD_DISKSZ,
    NM_FLD_DISKIN,
    NM_FLD_SOURCE,
    NM_FLD_IFSCNT,
    NM_FLD_IFSDRV,
    NM_FLD_USBUSE,
    NM_FLD_USBDEV
};

void nm_import_vm(void)
{
    nm_add_vm_main(NM_IMPORT_VM);
}

void nm_add_vm(void)
{
    nm_add_vm_main(NM_INSTALL_VM);
}

static void nm_add_vm_main(int import)
{
    nm_vm_t vm = NM_INIT_VM;
    nm_vect_t usb_devs = NM_INIT_VECT;
    nm_vect_t usb_names = NM_INIT_VECT;
    nm_spinner_data_t sp_data = NM_INIT_SPINNER;
    uint64_t last_mac;
    uint32_t last_vnc;
    size_t msg_len;
    pthread_t spin_th;
    int done = 0, mult = 2;

    nm_vm_get_usb(&usb_devs, &usb_names);

    nm_print_title(_(NM_EDIT_TITLE));
    if (getmaxy(stdscr) <= 28)
        mult = 1;

    window = nm_init_window((mult == 2) ? 25 : 14, 67, 3);

    init_pair(1, COLOR_BLACK, COLOR_WHITE);
    wbkgd(window, COLOR_PAIR(1));

    for (size_t n = 0; n < NM_ADD_VM_FIELDS_NUM; ++n)
    {
        fields[n] = new_field(1, 38, n * mult, 5, 0, 0);
        set_field_back(fields[n], A_UNDERLINE);
    }

    fields[NM_ADD_VM_FIELDS_NUM] = NULL;

    nm_add_vm_field_setup(&usb_names, import);
    nm_add_vm_field_names(window, import);

    form = nm_post_form(window, fields, 21);
    if (nm_draw_form(window, form) != NM_OK)
        goto out;

    nm_form_get_last(&last_mac, &last_vnc);
    nm_str_format(&vm.vncp, "%u", last_vnc);

    if (nm_add_vm_get_data(&vm, &usb_devs, import) != NM_OK)
        goto out;

    msg_len = mbstowcs(NULL, _(NM_EDIT_TITLE), strlen(_(NM_EDIT_TITLE)));
    sp_data.stop = &done;
    sp_data.x = (getmaxx(stdscr) + msg_len + 2) / 2;

    if (pthread_create(&spin_th, NULL, nm_spinner, (void *) &sp_data) != 0)
        nm_bug(_("%s: cannot create thread"), __func__);

    nm_add_vm_to_fs(&vm, import);
    nm_add_vm_to_db(&vm, last_mac, import);

    done = 1;
    if (pthread_join(spin_th, NULL) != 0)
        nm_bug(_("%s: cannot join thread"), __func__);

out:
    nm_vm_free(&vm);
    nm_form_free(form, fields);
    nm_vect_free(&usb_names, NULL);
    nm_vect_free(&usb_devs, nm_usb_vect_free_cb);
}

void nm_vm_get_usb(nm_vect_t *devs, nm_vect_t *names)
{
    nm_usb_get_devs(devs);

    for (size_t n = 0; n < devs->n_memb; n++)
    {
        nm_vect_insert(names,
                       nm_usb_name(devs->data[n]).data,
                       nm_usb_name(devs->data[n]).len + 1,
                       NULL);
#if (NM_DEBUG)
        nm_debug("%s %s\n", nm_usb_id(devs->data[n]).data,
                            nm_usb_name(devs->data[n]).data);
#endif
    }

    nm_vect_end_zero(names);
}

static void nm_add_vm_field_setup(const nm_vect_t *usb_names, int import)
{
    set_field_type(fields[NM_FLD_VMNAME], TYPE_REGEXP, "^[a-zA-Z0-9_-]{1,30} *$");
    set_field_type(fields[NM_FLD_VMARCH], TYPE_ENUM, nm_cfg_get_arch(), false, false);
    set_field_type(fields[NM_FLD_CPUNUM], TYPE_INTEGER, 0, 1, nm_hw_ncpus());
    set_field_type(fields[NM_FLD_RAMTOT], TYPE_INTEGER, 0, 4, nm_hw_total_ram());
    set_field_type(fields[NM_FLD_DISKSZ], TYPE_INTEGER, 0, 1, nm_hw_disk_free());
    set_field_type(fields[NM_FLD_DISKIN], TYPE_ENUM, nm_form_drive_drv, false, false);
    set_field_type(fields[NM_FLD_SOURCE], TYPE_REGEXP, "^/.*");
    set_field_type(fields[NM_FLD_IFSCNT], TYPE_INTEGER, 1, 0, 64);
    set_field_type(fields[NM_FLD_IFSDRV], TYPE_ENUM, nm_form_net_drv, false, false);
    set_field_type(fields[NM_FLD_USBUSE], TYPE_ENUM, nm_form_yes_no, false, false);
    set_field_type(fields[NM_FLD_USBDEV], TYPE_ENUM, usb_names->data, false, false);

    if (usb_names->n_memb == 0)
    {
        field_opts_off(fields[NM_FLD_USBUSE], O_ACTIVE);
        field_opts_off(fields[NM_FLD_USBDEV], O_ACTIVE);
    }
    if (import)
        field_opts_off(fields[NM_FLD_DISKSZ], O_ACTIVE);

    set_field_buffer(fields[NM_FLD_VMARCH], 0, *nm_cfg_get()->qemu_targets.data);
    set_field_buffer(fields[NM_FLD_CPUNUM], 0, "1");
    set_field_buffer(fields[NM_FLD_DISKIN], 0, NM_DEFAULT_DRVINT);
    set_field_buffer(fields[NM_FLD_IFSCNT], 0, "1");
    if (import)
        set_field_buffer(fields[NM_FLD_DISKSZ], 0, "unused");
    set_field_buffer(fields[NM_FLD_IFSDRV], 0, NM_DEFAULT_NETDRV);
    set_field_buffer(fields[NM_FLD_USBUSE], 0, "no");
    field_opts_off(fields[NM_FLD_VMNAME], O_STATIC);
    field_opts_off(fields[NM_FLD_SOURCE], O_STATIC);
    field_opts_off(fields[NM_FLD_USBDEV], O_STATIC);
}

static void nm_add_vm_field_names(nm_window_t *w, int import)
{
    int y = 2, mult = 2;
    nm_str_t buf = NM_INIT_STR;

    if (getmaxy(stdscr) <= 28)
        mult = 1;

    mvwaddstr(w, y, 2, _("Name"));
    mvwaddstr(w, y += mult, 2, _("Architecture"));

    nm_str_alloc_text(&buf, _("CPU cores [1-"));
    nm_str_format(&buf, "%u", nm_hw_ncpus());
    nm_str_add_char(&buf, ']');
    mvwaddstr(w, y += mult, 2, buf.data);
    nm_str_trunc(&buf, 0);

    nm_str_add_text(&buf, _("Memory [4-"));
    nm_str_format(&buf, "%u", nm_hw_total_ram());
    nm_str_add_text(&buf, "]Mb");
    mvwaddstr(w, y += mult, 2, buf.data);
    nm_str_trunc(&buf, 0);

    nm_str_add_text(&buf, _("Disk [1-"));
    nm_str_format(&buf, "%u", nm_hw_disk_free());
    nm_str_add_text(&buf, "]Gb");
    mvwaddstr(w, y += mult, 2, buf.data);

    mvwaddstr(w, y += mult, 2, _("Disk interface"));
    if (import)
        mvwaddstr(w, y += mult, 2, _("Path to disk image"));
    else
        mvwaddstr(w, y += mult, 2, _("Path to ISO/IMG"));
    mvwaddstr(w, y += mult, 2, _("Network interfaces"));
    mvwaddstr(w, y += mult, 2, _("Net driver"));
    mvwaddstr(w, y += mult, 2, _("USB [yes/no]"));
    mvwaddstr(w, y += mult, 2, _("USB device"));

    nm_str_free(&buf);
}

static int nm_add_vm_get_data(nm_vm_t *vm, const nm_vect_t *usb_devs,
                              int import)
{
    int rc = NM_OK;
    nm_str_t ifs_buf = NM_INIT_STR;
    nm_str_t usb_buf = NM_INIT_STR;
    nm_vect_t err = NM_INIT_VECT;

    nm_get_field_buf(fields[NM_FLD_VMNAME], &vm->name);
    nm_get_field_buf(fields[NM_FLD_VMARCH], &vm->arch);
    nm_get_field_buf(fields[NM_FLD_CPUNUM], &vm->cpus);
    nm_get_field_buf(fields[NM_FLD_RAMTOT], &vm->memo);
    nm_get_field_buf(fields[NM_FLD_SOURCE], &vm->srcp);
    if (!import)
        nm_get_field_buf(fields[NM_FLD_DISKSZ], &vm->drive.size);
    nm_get_field_buf(fields[NM_FLD_DISKIN], &vm->drive.driver);
    nm_get_field_buf(fields[NM_FLD_IFSCNT], &ifs_buf);
    nm_get_field_buf(fields[NM_FLD_IFSDRV], &vm->ifs.driver);
    nm_get_field_buf(fields[NM_FLD_USBUSE], &usb_buf);
    nm_get_field_buf(fields[NM_FLD_USBDEV], &vm->usb.device);

    nm_form_check_data(_("Name"), vm->name, err);
    nm_form_check_data(_("Architecture"), vm->arch, err);
    nm_form_check_data(_("CPU cores"), vm->cpus, err);
    nm_form_check_data(_("Memory"), vm->memo, err);
    if (!import)
        nm_form_check_data(_("Disk"), vm->drive.size, err);
    nm_form_check_data(_("Disk interface"), vm->drive.driver, err);
    nm_form_check_data(_("Path to ISO/IMG"), vm->srcp, err);
    nm_form_check_data(_("Network interfaces"), vm->ifs.driver, err);
    nm_form_check_data(_("Net driver"), vm->ifs.driver, err);
    nm_form_check_data(_("USB"), usb_buf, err);

    if ((rc = nm_print_empty_fields(&err)) == NM_ERR)
        goto out;

    vm->ifs.count = nm_str_stoui(&ifs_buf);

    if (nm_str_cmp_st(&usb_buf, "yes") == NM_OK)
        vm->usb.enable = 1;

    if (vm->usb.enable)
    {
        int found = 0;

        if (vm->usb.device.len == 0)
        {
            rc = NM_ERR;
            nm_print_warn(3, 2, _("usb device is empty"));
            goto out;
        }

        for (size_t n = 0; n < usb_devs->n_memb; n++)
        {
            if (nm_str_cmp_ss(&vm->usb.device,
                              &nm_usb_name(usb_devs->data[n])) == NM_OK)
            {
                nm_str_trunc(&vm->usb.device, 0);
                nm_str_copy(&vm->usb.device, &nm_usb_id(usb_devs->data[n]));
                found = 1;
                break;
            }
        }

        if (!found)
        {
            rc = NM_ERR;
            nm_print_warn(3, 2, _("usb_id not found"));
            goto out;
        }
    }

    rc = nm_form_name_used(&vm->name);

out:
    nm_str_free(&ifs_buf);
    nm_str_free(&usb_buf);
    nm_vect_free(&err, NULL);

    return rc;
}

static void nm_add_vm_to_db(nm_vm_t *vm, uint64_t mac, int import)
{
    nm_str_t query = NM_INIT_STR;

    /* {{{ insert main VM data */
    nm_str_alloc_text(&query, "INSERT INTO vms("
        "name, mem, smp, kvm, hcpu, vnc, arch, iso, install, mouse_override, usb");
    if (vm->usb.enable)
        nm_str_add_text(&query, ", usbid");
    nm_str_add_text(&query, ") VALUES('");
    nm_str_add_str(&query, &vm->name);
    nm_str_add_text(&query, "', '");
    nm_str_add_str(&query, &vm->memo);
    nm_str_add_text(&query, "', '");
    nm_str_add_str(&query, &vm->cpus);
#if (NM_OS_LINUX) /* enable KVM and host CPU by default */
    nm_str_add_text(&query, "', '" NM_ENABLE);
    nm_str_add_text(&query, "', '" NM_ENABLE "', '");
#else /* disable KVM on non Linux platform */
    nm_str_add_text(&query, "', '" NM_DISABLE);
    nm_str_add_text(&query, "', '" NM_DISABLE "', '");
#endif
    nm_str_add_str(&query, &vm->vncp);
    nm_str_add_text(&query, "', '");
    nm_str_add_str(&query, &vm->arch);
    nm_str_add_text(&query, "', '");
    if (!import)
    {
        nm_str_add_str(&query, &vm->srcp);
        nm_str_add_text(&query, "', '" NM_ENABLE); /* not installed */
    }
    else
    {
        nm_str_add_text(&query, "', '" NM_DISABLE); /* no need to install */
    }
    nm_str_add_text(&query, "', '" NM_DISABLE); /* mouse override */
    if (vm->usb.enable)
    {
        nm_str_add_text(&query, "', '" NM_ENABLE "', '");
        nm_str_add_str(&query, &vm->usb.device);
    }
    else
    {
        nm_str_add_text(&query, "', '" NM_DISABLE);
    }
    nm_str_add_text(&query, "')");

    nm_db_edit(query.data);
    /* }}} main VM data */

    nm_str_trunc(&query, 0);

    /* {{{ insert drive info */
    nm_str_add_text(&query, "INSERT INTO drives("
        "vm_name, drive_name, drive_drv, capacity, boot) VALUES('");
    nm_str_add_str(&query, &vm->name);
    nm_str_add_text(&query, "', '");
    nm_str_add_str(&query, &vm->name);
    nm_str_add_text(&query, "_a.img', '");
    nm_str_add_str(&query, &vm->drive.driver);
    nm_str_add_text(&query, "', '");
    nm_str_add_str(&query, &vm->drive.size);
    nm_str_add_text(&query, "', '" NM_ENABLE "')"); /* boot flag */

    nm_db_edit(query.data);
    /* }}} drive */

    /* {{{ insert network interface info */
    for (size_t n = 0; n < vm->ifs.count; n++)
    {
        nm_str_t if_name = NM_INIT_STR;
        nm_str_t maddr = NM_INIT_STR;
        nm_str_trunc(&query, 0);
        mac++;

        nm_net_mac_to_str(mac, &maddr);

        nm_str_format(&if_name, "%s_eth%zu", vm->name.data, n);
        if (if_name.len > 15) /* Linux tap iface max name len */
        {
            nm_str_trunc(&if_name, 14);
            nm_str_format(&if_name, "%zu", n);
        }

        nm_str_add_text(&query, "INSERT INTO ifaces("
            "vm_name, if_name, mac_addr, if_drv, vhost) VALUES('");
        nm_str_add_str(&query, &vm->name);
        nm_str_add_text(&query, "', '");
        nm_str_add_str(&query, &if_name);
        nm_str_add_text(&query, "', '");
        nm_str_add_str(&query, &maddr);
        nm_str_add_text(&query, "', '");
        nm_str_add_str(&query, &vm->ifs.driver);
        /* Enable vhost by default for virtio-net-pci device on Linux */
#if defined (NM_OS_LINUX)
        if (nm_str_cmp_st(&vm->ifs.driver, NM_DEFAULT_NETDRV) == NM_OK)
            nm_str_add_text(&query, "', '1");
        else
            nm_str_add_text(&query, "', '0");
#else
        nm_str_add_text(&query, "', '0");
#endif
        nm_str_add_text(&query, "')");

        nm_db_edit(query.data);

        nm_str_free(&if_name);
        nm_str_free(&maddr);
    }
    /* }}} network */

    nm_form_update_last_mac(mac);
    nm_form_update_last_vnc(nm_str_stoui(&vm->vncp));

    nm_str_free(&query);
}

static void nm_add_vm_to_fs(nm_vm_t *vm, int import)
{
    nm_str_t vm_dir = NM_INIT_STR;
    nm_str_t buf = NM_INIT_STR;

    nm_str_copy(&vm_dir, &nm_cfg_get()->vm_dir);
    nm_str_add_char(&vm_dir, '/');
    nm_str_add_str(&vm_dir, &vm->name);

    if (mkdir(vm_dir.data, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) != 0)
    {
        nm_bug(_("%s: cannot create VM directory %s: %s"),
               __func__, vm_dir.data, strerror(errno));
    }

    if (!import)
    {
        nm_str_alloc_text(&buf, NM_STRING(NM_USR_PREFIX) "/bin/qemu-img create -f qcow2 ");
        nm_str_add_str(&buf, &vm_dir);
        nm_str_add_char(&buf, '/');
        nm_str_add_str(&buf, &vm->name);
        nm_str_add_text(&buf, "_a.img ");
        nm_str_add_str(&buf, &vm->drive.size);
        nm_str_add_text(&buf, "G > /dev/null 2>&1");

        if (system(buf.data) != 0)
            nm_bug(_("%s: cannot create image file"), __func__);
    }
    else
    {
        nm_str_copy(&buf, &vm_dir);
        nm_str_add_char(&buf, '/');
        nm_str_add_str(&buf, &vm->name);
        nm_str_add_text(&buf, "_a.img");
        nm_copy_file(&vm->srcp, &buf);
    }

    nm_str_free(&vm_dir);
    nm_str_free(&buf);
}

/* vim:set ts=4 sw=4 fdm=marker: */
