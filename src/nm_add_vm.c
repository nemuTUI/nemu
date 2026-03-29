#include <nm_core.h>
#include <nm_form.h>
#include <nm_utils.h>
#include <nm_string.h>
#include <nm_window.h>
#include <nm_menu.h>
#include <nm_machine.h>
#include <nm_add_drive.h>
#include <nm_add_vm.h>
#include <nm_hw_info.h>
#include <nm_network.h>
#include <nm_database.h>
#include <nm_cfg_file.h>
#include <nm_ovf_import.h>

static const char NM_LC_VM_FORM_NAME[]       = "Name";
static const char NM_LC_VM_FORM_ARCH[]       = "Architecture";
static const char NM_LC_VM_FORM_CPU[]        = "CPU count";
static const char NM_LC_VM_FORM_MEM_BEGIN[]  = "Memory [4-";
static const char NM_LC_VM_FORM_MEM_END[]    = "]Mb";
static const char NM_LC_VM_FORM_DRV_BEGIN[]  = "Disk [1-";
static const char NM_LC_VM_FORM_DRV_END[]    = "]Gb";
static const char NM_LC_VM_FORM_DRV_IF[]     = "Disk interface";
static const char NM_LC_VM_FORM_DRV_FORMAT[] = "Disk image format";
static const char NM_LC_VM_FORM_DRV_DIS[]    = "Discard mode";
static const char NM_LC_VM_FORM_IMP_PATH[]   = "Path to disk image";
static const char NM_LC_VM_FORM_INS_PATH[]   = "Path to ISO/IMG";
static const char NM_LC_VM_FORM_NET_IFS[]    = "Network interfaces";
static const char NM_LC_VM_FORM_NET_DRV[]    = "Net driver";

static void nm_add_vm_init_windows(nm_form_t *form);
static void nm_add_vm_fields_setup(void);
static size_t nm_add_vm_labels_setup(void);
static int nm_add_vm_get_data(nm_vm_t *vm);
static void nm_add_vm_to_fs(nm_vm_t *vm);
static void nm_add_vm_main(void);

enum {
    NM_LBL_VMNAME = 0, NM_FLD_VMNAME,
    NM_LBL_VMARCH, NM_FLD_VMARCH,
    NM_LBL_CPUNUM, NM_FLD_CPUNUM,
    NM_LBL_RAMTOT, NM_FLD_RAMTOT,
    NM_LBL_DISKSZ, NM_FLD_DISKSZ,
    NM_LBL_DISKIN, NM_FLD_DISKIN,
    NM_LBL_DISKFMT, NM_FLD_DISKFMT,
    NM_LBL_DISCARD, NM_FLD_DISCARD,
    NM_LBL_SOURCE, NM_FLD_SOURCE,
    NM_LBL_IFSCNT, NM_FLD_IFSCNT,
    NM_LBL_IFSDRV, NM_FLD_IFSDRV,
    NM_FLD_COUNT
};

static int import;
static nm_field_t *fields[NM_FLD_COUNT + 1];

void nm_import_vm(void)
{
    import = NM_IMPORT_VM;
    nm_add_vm_main();
}

void nm_add_vm(void)
{
    import = NM_INSTALL_VM;
    nm_add_vm_main();
}

static void nm_add_vm_init_windows(nm_form_t *form)
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

    nm_init_side();
    if (import == NM_INSTALL_VM) {
        nm_init_action(_(NM_MSG_INSTALL));
        nm_init_help_install();
    } else {
        nm_init_action(_(NM_MSG_IMPORT));
        nm_init_help_import();
    }

    nm_print_vm_menu(NULL);
}

static void nm_add_vm_main(void)
{
    nm_form_data_t *form_data = NULL;
    nm_form_t *form = NULL;
    nm_vm_t vm = NM_INIT_VM;
    nm_spinner_data_t sp_data = NM_INIT_SPINNER;
    uint64_t last_mac;
    uint32_t last_vnc;
    size_t msg_len;
    pthread_t spin_th = pthread_self();
    int done = 0;

    nm_add_vm_init_windows(NULL);

    msg_len = nm_add_vm_labels_setup();

    form_data = nm_form_data_new(
        action_window, nm_add_vm_init_windows, msg_len,
        NM_FLD_COUNT / 2, NM_TRUE);

    if (nm_form_data_update(form_data, 0, 0) != NM_OK) {
        goto out;
    }

    for (size_t n = 0; n < NM_FLD_COUNT; n++) {
        switch (n) {
        case NM_FLD_VMNAME:
            fields[n] = nm_field_regexp_new(
                n / 2, form_data, "^[a-zA-Z0-9_-]{1,30} *$");
            break;
        case NM_FLD_VMARCH:
            fields[n] = nm_field_enum_new(
                n / 2, form_data, nm_cfg_get_arch(), false, false);
            break;
        case NM_FLD_CPUNUM:
            fields[n] = nm_field_regexp_new(
                n / 2, form_data, "^[0-9]{1}(:[0-9]{1})?(:[0-9]{1})? *$");
            break;
        case NM_FLD_RAMTOT:
            fields[n] = nm_field_integer_new(n / 2, form_data,
                0, 4, nm_hw_total_ram());
            break;
        case NM_FLD_DISKSZ:
            fields[n] = nm_field_integer_new(n / 2, form_data,
                0, 1, nm_hw_disk_free());
            break;
        case NM_FLD_DISKIN:
            fields[n] = nm_field_enum_new(
                n / 2, form_data, nm_form_drive_drv, false, false);
            break;
        case NM_FLD_DISKFMT:
            fields[n] = nm_field_enum_new(
                n / 2, form_data, nm_form_drive_fmt, false, false);
            break;
        case NM_FLD_DISCARD:
            fields[n] = nm_field_enum_new(
                n / 2, form_data, nm_form_yes_no, false, false);
            break;
        case NM_FLD_SOURCE:
            fields[n] = nm_field_regexp_new(
                n / 2, form_data, "^/.*");
            break;
        case NM_FLD_IFSCNT:
            fields[n] = nm_field_integer_new(n / 2, form_data, 1, 0, 64);
            break;
        case NM_FLD_IFSDRV:
            fields[n] = nm_field_enum_new(
                n / 2, form_data, nm_form_net_drv, false, false);
            break;
        default:
            fields[n] = nm_field_label_new(n / 2, form_data);
            break;
        }
    }
    fields[NM_FLD_COUNT] = NULL;

    nm_add_vm_labels_setup();
    nm_add_vm_fields_setup();
    nm_fields_unset_status(fields);

    form = nm_form_new(form_data, fields);
    nm_form_post(form);

    if (nm_form_draw(&form) != NM_OK) {
        goto out;
    }

    last_mac = nm_form_get_last_mac();
    last_vnc = nm_form_get_free_vnc();

    nm_str_format(&vm.vncp, "%u", last_vnc);

    if (nm_add_vm_get_data(&vm) != NM_OK) {
        goto out;
    }

    if (import) {
        sp_data.stop = &done;
        sp_data.ctx = &vm;

        if (pthread_create(&spin_th, NULL, nm_file_progress,
                    (void *) &sp_data) != 0) {
            nm_bug(_("%s: cannot create thread"), __func__);
        }
    }

    nm_add_vm_to_fs(&vm);
    nm_add_vm_to_db(&vm, last_mac, import, NULL);

    if (import) {
        done = 1;
        if (pthread_join(spin_th, NULL) != 0) {
            nm_bug(_("%s: cannot join thread"), __func__);
        }
    }

out:
    NM_FORM_EXIT();
    nm_vm_free(&vm);
    nm_form_free(form);
    nm_form_data_free(form_data);
    nm_fields_free(fields);
}

static void nm_add_vm_fields_setup(void)
{
    set_field_buffer(fields[NM_FLD_VMARCH], 0,
            *nm_cfg_get()->qemu_targets.data);
    set_field_buffer(fields[NM_FLD_CPUNUM], 0, "1");
    set_field_buffer(fields[NM_FLD_DISKIN], 0, NM_DEFAULT_DRVINT);
    set_field_buffer(fields[NM_FLD_DISKFMT], 0, NM_DEFAULT_DRVFMT);
    set_field_buffer(fields[NM_FLD_DISCARD], 0, nm_form_yes_no[1]);
    set_field_buffer(fields[NM_FLD_IFSCNT], 0, "1");
    if (import) {
        field_opts_off(fields[NM_FLD_DISKSZ], O_ACTIVE);
        set_field_buffer(fields[NM_FLD_DISKSZ], 0, "unused");
    }
    set_field_buffer(fields[NM_FLD_IFSDRV], 0, NM_DEFAULT_NETDRV);
    field_opts_off(fields[NM_FLD_VMNAME], O_STATIC);
    field_opts_off(fields[NM_FLD_SOURCE], O_STATIC);
}

static size_t nm_add_vm_labels_setup(void)
{
    nm_str_t buf = NM_INIT_STR;
    size_t max_label_len = 0;
    size_t msg_len = 0;

    for (size_t n = 0; n < NM_FLD_COUNT; n++) {
        switch (n) {
        case NM_LBL_VMNAME:
            nm_str_format(&buf, "%s", _(NM_LC_VM_FORM_NAME));
            break;
        case NM_LBL_VMARCH:
            nm_str_format(&buf, "%s", _(NM_LC_VM_FORM_ARCH));
            break;
        case NM_LBL_CPUNUM:
            nm_str_format(&buf, "%s", _(NM_LC_VM_FORM_CPU));
            break;
        case NM_LBL_RAMTOT:
            nm_str_format(&buf, "%s%u%s",
                _(NM_LC_VM_FORM_MEM_BEGIN), nm_hw_total_ram(),
                _(NM_LC_VM_FORM_MEM_END));
            break;
        case NM_LBL_DISKSZ:
            nm_str_format(&buf, "%s%u%s",
                _(NM_LC_VM_FORM_DRV_BEGIN), nm_hw_disk_free(),
                _(NM_LC_VM_FORM_DRV_END));
            break;
        case NM_LBL_DISKIN:
            nm_str_format(&buf, "%s", _(NM_LC_VM_FORM_DRV_IF));
            break;
        case NM_LBL_DISKFMT:
            nm_str_format(&buf, "%s", _(NM_LC_VM_FORM_DRV_FORMAT));
            break;
        case NM_LBL_DISCARD:
            nm_str_format(&buf, "%s", _(NM_LC_VM_FORM_DRV_DIS));
            break;
        case NM_LBL_SOURCE:
            if (import) {
                nm_str_format(&buf, "%s", _(NM_LC_VM_FORM_IMP_PATH));
            } else {
                nm_str_format(&buf, "%s", _(NM_LC_VM_FORM_INS_PATH));
            }
            break;
        case NM_LBL_IFSCNT:
            nm_str_format(&buf, "%s", _(NM_LC_VM_FORM_NET_IFS));
            break;
        case NM_LBL_IFSDRV:
            nm_str_format(&buf, "%s", _(NM_LC_VM_FORM_NET_DRV));
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

static int nm_add_vm_get_data(nm_vm_t *vm)
{
    int rc;
    nm_str_t ifs_buf = NM_INIT_STR;
    nm_vect_t err = NM_INIT_VECT;
    nm_str_t discard = NM_INIT_STR;

    nm_get_field_buf(fields[NM_FLD_VMNAME], &vm->name);
    nm_get_field_buf(fields[NM_FLD_VMARCH], &vm->arch);
    nm_get_field_buf(fields[NM_FLD_CPUNUM], &vm->cpus);
    nm_get_field_buf(fields[NM_FLD_RAMTOT], &vm->memo);
    nm_get_field_buf(fields[NM_FLD_SOURCE], &vm->srcp);
    if (!import) {
        nm_get_field_buf(fields[NM_FLD_DISKSZ], &vm->drive.size);
    }
    nm_get_field_buf(fields[NM_FLD_DISKFMT], &vm->drive.format);
    nm_get_field_buf(fields[NM_FLD_DISCARD], &discard);
    nm_get_field_buf(fields[NM_FLD_DISKIN], &vm->drive.driver);
    nm_get_field_buf(fields[NM_FLD_IFSCNT], &ifs_buf);
    nm_get_field_buf(fields[NM_FLD_IFSDRV], &vm->ifs.driver);

    nm_form_check_data(_("Name"), vm->name, err);
    nm_form_check_data(_("Architecture"), vm->arch, err);
    nm_form_check_data(_("CPU cores"), vm->cpus, err);
    nm_form_check_data(_("Memory"), vm->memo, err);
    if (!import) {
        nm_form_check_data(_("Disk"), vm->drive.size, err);
    }
    nm_form_check_data(_("Disk image format"), vm->drive.format, err);
    nm_form_check_data(_("Disk interface"), vm->drive.driver, err);
    nm_form_check_data(_("Discard mode"), discard, err);
    nm_form_check_data(_("Path to ISO/IMG"), vm->srcp, err);
    nm_form_check_data(_("Network interfaces"), vm->ifs.driver, err);
    nm_form_check_data(_("Net driver"), vm->ifs.driver, err);

    if (nm_str_cmp_st(&discard, "yes") == NM_OK) {
        vm->drive.discard = 1;
    }

    if ((rc = nm_print_empty_fields(&err)) == NM_ERR) {
        goto out;
    }

    /* Check for free space for importing drive image */
    if (import) {
        off_t size_gb = 0;
        off_t virtual_size, actual_size;

        nm_get_drive_size(&vm->srcp, &virtual_size, &actual_size);
        size_gb = actual_size / 1073741824;
        nm_str_format(&vm->drive.size, "%g",
                (double) virtual_size / 1073741824);

        if (size_gb >= nm_hw_disk_free()) {
            curs_set(0);
            nm_warn(_(NM_MSG_NO_SPACE));
            goto out;
        }
    }

    vm->ifs.count = nm_str_stoui(&ifs_buf, 10);
#if defined (NM_WITH_USB)
    vm->usb_enable = 1; /* enable USB by default */
#endif /* NM_WITH_USB */

    rc = nm_form_name_used(&vm->name);

out:
    nm_str_free(&ifs_buf);
    nm_str_free(&discard);
    nm_vect_free(&err, NULL);

    return rc;
}

void nm_add_vm_to_db(nm_vm_t *vm, uint64_t mac,
                     int imported, const nm_vect_t *drives)
{
    nm_str_t query = NM_INIT_STR;

    /* insert main VM data */
    nm_str_format(&query, NM_SQL_VMS_INSERT_NEW,
        vm->name.data, vm->memo.data, vm->cpus.data,
#if defined(NM_OS_LINUX) || defined(NM_OS_DARWIN)
        NM_ENABLE, NM_ENABLE, /* enable KVM and host CPU by default */
#else
        NM_DISABLE, NM_DISABLE, /* disable KVM on non supported platform */
#endif
        vm->vncp.data, vm->arch.data,
        imported ? "" : vm->srcp.data,
        /* if imported, then no need to install */
        imported ? NM_DISABLE : NM_ENABLE,
        nm_mach_get_default(&vm->arch), /* setup default machine type */
        NM_DISABLE, /* mouse override */
        vm->usb_enable ? NM_ENABLE : NM_DISABLE, /* USB enabled */
        NM_DEFAULT_USBVER, /* set USB 3.0 by default */
        NM_DISABLE, /* default usb_status is off */
        NM_DISABLE, /* disable 9pfs by default */
        /* SPICE enabled */
        (nm_cfg_get()->spice_default) ? NM_ENABLE : NM_DISABLE,
        "", /* GDB debug port */
        NM_DISABLE, /* disable GDB debug by default */
        NM_DEFAULT_DISPLAY, /* Display type */
        NM_DISABLE /* disable SPICE agent debug by default */
    );

    nm_db_edit(query.data);

    /* insert drive info */
    if (drives == NULL) {
        nm_str_format(&query, NM_SQL_DRIVES_INSERT_NEW,
                vm->name.data, vm->name.data, vm->drive.driver.data,
                vm->drive.size.data,
                NM_ENABLE, /* boot flag */
                vm->drive.discard ? NM_ENABLE : NM_DISABLE,
                vm->drive.format.data
                );
        nm_db_edit(query.data);
    } else { /* imported from OVF */
        for (size_t n = 0; n < drives->n_memb; n++) {
            nm_str_format(&query, NM_SQL_DRIVES_INSERT_IMPORTED,
                vm->name.data,
                nm_drive_file(drives->data[n])->data, NM_DEFAULT_DRVINT,
                nm_drive_size(drives->data[n])->data,
                n == 0 ? NM_ENABLE : NM_DISABLE, /* boot flag */
                vm->drive.discard ? NM_ENABLE : NM_DISABLE,
                vm->drive.format.data
                );
            nm_db_edit(query.data);
        }
    }

    /* insert network interface info */
    for (size_t n = 0; n < vm->ifs.count; n++) {
        int altname;
        nm_str_t if_name = NM_INIT_STR;
        nm_str_t if_name_copy = NM_INIT_STR;
        nm_str_t maddr = NM_INIT_STR;

        mac++;

        nm_net_mac_n2s(mac, &maddr);
        nm_str_format(&if_name, "%s_eth%zu", vm->name.data, n);
        nm_str_copy(&if_name_copy, &if_name);
        altname = nm_net_fix_tap_name(&if_name, &maddr);

        nm_str_format(&query, NM_SQL_IFACES_INSERT_NEW,
            vm->name.data, if_name.data, maddr.data, vm->ifs.driver.data,
#if defined (NM_OS_LINUX)
            nm_str_cmp_st(&vm->ifs.driver, NM_DEFAULT_NETDRV) == NM_OK ?
            "1" : "0", /* enable vhost by default for virtio-net-pci device */
#else
            "0",
#endif
            "0", /* disable macvtap by default */
            (altname) ? if_name_copy.data : "",
            NM_ENABLE, /* enable user-net */
            NM_DISABLE, /* disable bridge by default */
            "" /* no bridge interface by default */
        );
        nm_db_edit(query.data);

        nm_str_free(&if_name);
        nm_str_free(&if_name_copy);
        nm_str_free(&maddr);
    }

    nm_str_free(&query);
}

static void nm_convert_drives(const nm_str_t *vm_dir, const nm_str_t *src_img,
        const nm_str_t *dst_img, const nm_str_t *format)
{
    nm_str_t buf = NM_INIT_STR;
    nm_vect_t argv = NM_INIT_VECT;

    nm_str_format(&buf, "%s/qemu-img", nm_cfg_get()->qemu_bin_path.data);
    nm_vect_insert(&argv, buf.data, buf.len + 1, NULL);

    nm_vect_insert_cstr(&argv, "convert");
    nm_vect_insert_cstr(&argv, "-O");
    nm_vect_insert_cstr(&argv, format->data);
    nm_vect_insert_cstr(&argv, src_img->data);
    nm_vect_insert_cstr(&argv, dst_img->data);

    nm_cmd_str(&buf, &argv);
    nm_debug("add_vm: exec: %s\n", buf.data);

    nm_vect_end_zero(&argv);
    if (nm_spawn_process(&argv, NULL) != NM_OK) {
        rmdir(vm_dir->data);
        nm_bug(_("%s: cannot convert image file"), __func__);
    }

    nm_vect_free(&argv, NULL);
    nm_str_free(&buf);
}

static void nm_add_vm_to_fs(nm_vm_t *vm)
{
    nm_str_t vm_dir = NM_INIT_STR;
    nm_str_t dst_img = NM_INIT_STR;

    nm_str_format(&vm_dir, "%s/%s", nm_cfg_get()->vm_dir.data, vm->name.data);

    if (mkdir(vm_dir.data, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) != 0) {
        nm_bug(_("%s: cannot create VM directory %s: %s"),
               __func__, vm_dir.data, strerror(errno));
    }

    if (!import) {
        if (nm_add_drive_to_fs(&vm->name, &vm->drive.size,
                    NULL, &vm->drive.format) != NM_OK) {
            nm_bug(_("%s: cannot create image file"), __func__);
        }
    } else {
        nm_str_format(&dst_img, "%s/%s_a.img", vm_dir.data, vm->name.data);
        nm_convert_drives(&vm_dir, &vm->srcp, &dst_img, &vm->drive.format);
    }

    nm_str_free(&vm_dir);
    nm_str_free(&dst_img);
}

/* vim:set ts=4 sw=4: */
