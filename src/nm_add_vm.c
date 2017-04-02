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

static nm_window_t *window = NULL;
static nm_form_t *form = NULL;
static nm_field_t *fields[NM_ADD_VM_FIELDS_NUM + 1];

static void nm_add_vm_field_setup(const nm_vect_t *usb_names);
static void nm_add_vm_field_names(nm_window_t *w);
static void nm_add_vm_get_usb(nm_vect_t *devs, nm_vect_t *names);
static void nm_add_vm_get_last(uint64_t *mac, uint32_t *vnc);
static void nm_add_vm_update_last(uint64_t mac, const nm_str_t *vnc);
static int nm_add_vm_get_data(nm_vm_t *vm, const nm_vect_t *usb_devs);
static void nm_add_vm_to_db(nm_vm_t *vm, uint64_t mac);
static void nm_add_vm_to_fs(nm_vm_t *vm);

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

void nm_add_vm(void)
{
    nm_vm_t vm = NM_INIT_VM;
    nm_vect_t usb_devs = NM_INIT_VECT;
    nm_vect_t usb_names = NM_INIT_VECT;
    nm_spinner_data_t sp_data = NM_INIT_SPINNER;
    uint64_t last_mac;
    uint32_t last_vnc;
    size_t msg_len;
    pthread_t spin_th;
    int done = 0;

    nm_add_vm_get_usb(&usb_devs, &usb_names);

    nm_print_title(_(NM_EDIT_TITLE));
    window = nm_init_window(25, 67, 3);

    init_pair(1, COLOR_BLACK, COLOR_WHITE);
    wbkgd(window, COLOR_PAIR(1));

    for (size_t n = 0; n < NM_ADD_VM_FIELDS_NUM; ++n)
    {
        fields[n] = new_field(1, 38, n * 2, 5, 0, 0);
        set_field_back(fields[n], A_UNDERLINE);
    }

    fields[NM_ADD_VM_FIELDS_NUM] = NULL;

    nm_add_vm_field_setup(&usb_names);
    nm_add_vm_field_names(window);

    form = nm_post_form(window, fields, 21);
    if (nm_draw_form(window, form) != NM_OK)
        goto out;

    nm_add_vm_get_last(&last_mac, &last_vnc);
    last_vnc++;

    nm_str_format(&vm.vncp, "%u", last_vnc);

    if (nm_add_vm_get_data(&vm, &usb_devs) != NM_OK)
        goto out;

    msg_len = mbstowcs(NULL, _(NM_EDIT_TITLE), strlen(_(NM_EDIT_TITLE)));
    sp_data.stop = &done;
    sp_data.x = (getmaxx(stdscr) + msg_len + 2) / 2;

    if (pthread_create(&spin_th, NULL, nm_spinner, (void *) &sp_data) != 0)
        nm_bug(_("%s: cannot create thread"), __func__);

    nm_add_vm_to_fs(&vm);
    nm_add_vm_to_db(&vm, last_mac);

    done = 1;
    if (pthread_join(spin_th, NULL) != 0)
        nm_bug(_("%s: cannot join thread"), __func__);

#if (NM_DEBUG) /*XXX remove this later */
    nm_debug("name: %s\n", vm.name.data);
    nm_debug("arch: %s\n", vm.arch.data);
    nm_debug("cpus: %s\n", vm.cpus.data);
    nm_debug("memo: %s\n", vm.memo.data);
    nm_debug("iso: %s\n", vm.srcp.data);
    nm_debug("disk_sz: %s\n", vm.drive.size.data);
    nm_debug("disk_drv: %s\n", vm.drive.driver.data);
    nm_debug("usb: %u\n", vm.usb.enable);
    nm_debug("usbid: %s\n", vm.usb.device.data);
    nm_debug("ifs_sz: %u\n", vm.ifs.count);
    nm_debug("ifs_drv: %s\n", vm.ifs.driver.data);
#endif

out:
    nm_vm_free(&vm);
    nm_form_free(form, fields);
    nm_vect_free(&usb_names, NULL);
    nm_vect_free(&usb_devs, nm_usb_vect_free_cb);
}

static void nm_add_vm_field_setup(const nm_vect_t *usb_names)
{
    set_field_type(fields[NM_FLD_VMNAME], TYPE_REGEXP, "^[a-zA-Z0-9_-]* *$");
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

    set_field_buffer(fields[NM_FLD_VMARCH], 0, *nm_cfg_get()->qemu_targets.data);
    set_field_buffer(fields[NM_FLD_CPUNUM], 0, "1");
    set_field_buffer(fields[NM_FLD_DISKIN], 0, NM_DEFAULT_DRVINT);
    set_field_buffer(fields[NM_FLD_IFSCNT], 0, "1");
    set_field_buffer(fields[NM_FLD_IFSDRV], 0, NM_DEFAULT_NETDRV);
    set_field_buffer(fields[NM_FLD_USBUSE], 0, "no");
    field_opts_off(fields[NM_FLD_VMNAME], O_STATIC);
    field_opts_off(fields[NM_FLD_SOURCE], O_STATIC);
    field_opts_off(fields[NM_FLD_USBDEV], O_STATIC);
    set_max_field(fields[NM_FLD_VMNAME], 30);
}

static void nm_add_vm_field_names(nm_window_t *w)
{
    nm_str_t buf = NM_INIT_STR;

    mvwaddstr(w, 2, 2, _("Name"));
    mvwaddstr(w, 4, 2, _("Architecture"));

    nm_str_alloc_text(&buf, _("CPU cores [1-"));
    nm_str_format(&buf, "%u", nm_hw_ncpus());
    nm_str_add_char(&buf, ']');
    mvwaddstr(w, 6, 2, buf.data);
    nm_str_trunc(&buf, 0);

    nm_str_add_text(&buf, _("Memory [4-"));
    nm_str_format(&buf, "%u", nm_hw_total_ram());
    nm_str_add_text(&buf, "]Mb");
    mvwaddstr(w, 8, 2, buf.data);
    nm_str_trunc(&buf, 0);

    nm_str_add_text(&buf, _("Disk [1-"));
    nm_str_format(&buf, "%u", nm_hw_disk_free());
    nm_str_add_text(&buf, "]Gb");
    mvwaddstr(w, 10, 2, buf.data);

    mvwaddstr(w, 12, 2, _("Disk interface"));
    mvwaddstr(w, 14, 2, _("Path to ISO/IMG"));
    mvwaddstr(w, 16, 2, _("Network interfaces"));
    mvwaddstr(w, 18, 2, _("Net driver"));
    mvwaddstr(w, 20, 2, _("USB [yes/no]"));
    mvwaddstr(w, 22, 2, _("USB device"));

    nm_str_free(&buf);
}

static void nm_add_vm_get_usb(nm_vect_t *devs, nm_vect_t *names)
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

static void nm_add_vm_get_last(uint64_t *mac, uint32_t *vnc)
{
    nm_vect_t res = NM_INIT_VECT;

    nm_db_select("SELECT mac,vnc FROM lastval", &res);
    *mac = nm_str_stoul(res.data[0]);
    *vnc = nm_str_stoui(res.data[1]);

    nm_vect_free(&res, nm_str_vect_free_cb);
}

static void nm_add_vm_update_last(uint64_t mac, const nm_str_t *vnc)
{
    nm_str_t query = NM_INIT_STR;

    nm_str_alloc_text(&query, "UPDATE lastval SET mac='");
    nm_str_format(&query, "%" PRIu64 "'", mac);

    nm_db_edit(query.data);
    nm_str_trunc(&query, 0);

    nm_str_alloc_text(&query, "UPDATE lastval SET vnc='");
    nm_str_add_str(&query, vnc);
    nm_str_add_char(&query, '\'');
    nm_db_edit(query.data);

    nm_str_free(&query);
}

static int nm_add_vm_get_data(nm_vm_t *vm, const nm_vect_t *usb_devs)
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
    nm_get_field_buf(fields[NM_FLD_DISKSZ], &vm->drive.size);
    nm_get_field_buf(fields[NM_FLD_DISKIN], &vm->drive.driver);
    nm_get_field_buf(fields[NM_FLD_IFSCNT], &ifs_buf);
    nm_get_field_buf(fields[NM_FLD_IFSDRV], &vm->ifs.driver);
    nm_get_field_buf(fields[NM_FLD_USBUSE], &usb_buf);
    nm_get_field_buf(fields[NM_FLD_USBDEV], &vm->usb.device);

    nm_add_vm_check_data(_("Name"), vm->name, err);
    nm_add_vm_check_data(_("Architecture"), vm->arch, err);
    nm_add_vm_check_data(_("CPU cores"), vm->cpus, err);
    nm_add_vm_check_data(_("Memory"), vm->memo, err);
    nm_add_vm_check_data(_("Disk"), vm->drive.size, err);
    nm_add_vm_check_data(_("Disk interface"), vm->drive.driver, err);
    nm_add_vm_check_data(_("Path to ISO/IMG"), vm->srcp, err);
    nm_add_vm_check_data(_("Network interfaces"), vm->ifs.driver, err);
    nm_add_vm_check_data(_("Net driver"), vm->ifs.driver, err);
    nm_add_vm_check_data(_("USB"), usb_buf, err);

    if (err.n_memb != 0)
    {
        rc = NM_ERR;
        int y = 1;
        size_t msg_len;

        msg_len = mbstowcs(NULL, _(NM_FORM_EMPTY_MSG), strlen(_(NM_FORM_EMPTY_MSG)));

        nm_window_t *err_window = nm_init_window(4 + err.n_memb, msg_len + 2, 2);
        curs_set(0);
        box(err_window, 0, 0);
        mvwprintw(err_window, y++, 1, "%s", _(NM_FORM_EMPTY_MSG));

        for (size_t n = 0; n < err.n_memb; n++)
            mvwprintw(err_window, ++y, 1, "%s", (char *) err.data[n]);

        wrefresh(err_window);
        wgetch(err_window);

        delwin(err_window);
        goto out;
    }

    vm->ifs.count = nm_str_stoui(&ifs_buf);

    if ((nm_str_cmp_st(&usb_buf, "yes") == NM_OK) &&
        (vm->usb.device.len > 0))
    {
        vm->usb.enable = 1;
    }

    if (vm->usb.enable)
    {
        int found = 0;

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
            nm_bug(_("%s: usb_id not found"), __func__);
    }

    { /* {{{ Check that VM name is not taken */
        nm_vect_t res = NM_INIT_VECT;
        nm_str_t query = NM_INIT_STR;

        nm_str_alloc_text(&query, "SELECT id FROM vms WHERE name='");
        nm_str_add_str(&query, &vm->name);
        nm_str_add_char(&query, '\'');

        nm_db_select(query.data, &res);
        if (res.n_memb > 0)
        {
            rc = NM_ERR;
            size_t msg_len;

            msg_len = mbstowcs(NULL, _(NM_FORM_VMNAME_MSG), strlen(_(NM_FORM_VMNAME_MSG)));
            nm_window_t *err_window = nm_init_window(3, msg_len + 2, 2);
            nm_print_warn(err_window, _(NM_FORM_VMNAME_MSG));
            delwin(err_window);
        }

        nm_vect_free(&res, NULL);
        nm_str_free(&query);
    } /* }}} VM name */

out:
    nm_str_free(&ifs_buf);
    nm_str_free(&usb_buf);
    nm_vect_free(&err, NULL);

    return rc;
}

static void nm_add_vm_to_db(nm_vm_t *vm, uint64_t mac)
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
    nm_str_add_str(&query, &vm->srcp);
    nm_str_add_text(&query, "', '" NM_ENABLE); /* not installed */
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
            "vm_name, if_name, mac_addr, if_drv) VALUES('");
        nm_str_add_str(&query, &vm->name);
        nm_str_add_text(&query, "', '");
        nm_str_add_str(&query, &if_name);
        nm_str_add_text(&query, "', '");
        nm_str_add_str(&query, &maddr);
        nm_str_add_text(&query, "', '");
        nm_str_add_str(&query, &vm->ifs.driver);
        nm_str_add_text(&query, "')");

        nm_db_edit(query.data);

        nm_str_free(&if_name);
        nm_str_free(&maddr);
    }
    /* }}} network */

    nm_add_vm_update_last(mac, &vm->vncp);

    nm_str_free(&query);
}

static void nm_add_vm_to_fs(nm_vm_t *vm)
{
    nm_str_t vm_dir = NM_INIT_STR;
    nm_str_t cmd = NM_INIT_STR;

    nm_str_copy(&vm_dir, &nm_cfg_get()->vm_dir);
    nm_str_add_char(&vm_dir, '/');
    nm_str_add_str(&vm_dir, &vm->name);

    if (mkdir(vm_dir.data, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) != 0)
    {
        nm_bug(_("%s: cannot create VM directory %s: %s"),
               __func__, vm_dir.data, strerror(errno));
    }

    nm_str_alloc_text(&cmd, "qemu-img create -f qcow2 ");
    nm_str_add_str(&cmd, &vm_dir);
    nm_str_add_char(&cmd, '/');
    nm_str_add_str(&cmd, &vm->name);
    nm_str_add_text(&cmd, "_a.img ");
    nm_str_add_str(&cmd, &vm->drive.size);
    nm_str_add_text(&cmd, "G > /dev/null 2>&1");

    if (system(cmd.data) != 0)
        nm_bug(_("%s: cannot create image file"), __func__);

    nm_str_free(&vm_dir);
    nm_str_free(&cmd);
}

/* vim:set ts=4 sw=4 fdm=marker: */
