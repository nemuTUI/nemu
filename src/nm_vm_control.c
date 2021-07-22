#include <nm_core.h>
#include <nm_form.h>
#include <nm_utils.h>
#include <nm_string.h>
#include <nm_window.h>
#include <nm_network.h>
#include <nm_database.h>
#include <nm_cfg_file.h>
#include <nm_vm_control.h>
#include <nm_usb_devices.h>
#include <nm_qmp_control.h>
#include <nm_stat_usage.h>

#include <time.h>

enum {
    NM_VIEWER_SPICE,
    NM_VIEWER_VNC
};

#if defined(NM_WITH_VNC_CLIENT) || defined(NM_WITH_SPICE)
static void nm_vmctl_gen_viewer(const nm_str_t *name, uint32_t port, nm_str_t *cmd, int type);
#endif
static int nm_vmctl_clear_tap_vect(const nm_vect_t *vms);

void nm_vmctl_get_data(const nm_str_t *name, nm_vmctl_data_t *vm)
{
    nm_str_t query = NM_INIT_STR;

    nm_str_format(&query, NM_VM_GET_LIST_SQL, name->data);
    nm_db_select(query.data, &vm->main);

    nm_str_format(&query, NM_VM_GET_IFACES_SQL, name->data);
    nm_db_select(query.data, &vm->ifs);

    nm_str_format(&query, NM_VM_GET_DRIVES_SQL, name->data);
    nm_db_select(query.data, &vm->drives);

    nm_str_format(&query, NM_USB_GET_SQL, name->data);
    nm_db_select(query.data, &vm->usb);

    nm_str_free(&query);
}

void nm_vmctl_start(const nm_str_t *name, int flags)
{
    nm_str_t buf = NM_INIT_STR;
    nm_str_t snap = NM_INIT_STR;
    nm_vect_t argv = NM_INIT_VECT;
    nm_vmctl_data_t vm = NM_VMCTL_INIT_DATA;
    nm_vect_t tfds = NM_INIT_VECT;

    nm_vmctl_get_data(name, &vm);

    /* check if VM is already installed */
    if (nm_str_cmp_st(nm_vect_str(&vm.main, NM_SQL_INST), NM_ENABLE) == NM_OK) {
        int ch = nm_notify(_(NM_MSG_INST_CONF));

        if (ch == 'y') {
            flags &= ~NM_VMCTL_TEMP;
            nm_str_t query = NM_INIT_STR;

            nm_str_trunc(nm_vect_str(&vm.main, NM_SQL_INST), 0);
            nm_str_add_text(nm_vect_str(&vm.main, NM_SQL_INST), NM_DISABLE);

            nm_str_alloc_text(&query, "UPDATE vms SET install='0' WHERE name='");
            nm_str_add_str(&query, name);
            nm_str_add_char(&query, '\'');
            nm_db_edit(query.data);

            nm_str_free(&query);
        }
    }

    nm_vmctl_gen_cmd(&argv, &vm, name, &flags, &tfds, &snap);
    if (argv.n_memb > 0) {
        if (nm_spawn_process(&argv, NULL) != NM_OK) {
            nm_str_t qmp_path = NM_INIT_STR;
            struct stat qmp_info;

            nm_str_format(&qmp_path, "%s/%s/%s",
                nm_cfg_get()->vm_dir.data, name->data, NM_VM_QMP_FILE);

            /* must delete qmp sock file if exists */
            if (stat(qmp_path.data, &qmp_info) != -1)
                unlink(qmp_path.data);

            nm_str_free(&qmp_path);

            nm_warn(_(NM_MSG_START_ERR));
        } else {
            nm_cmd_str(&buf, &argv);
            nm_debug("cmd=%s\n", buf.data);
            nm_vmctl_log_last(&buf);

            /* close all tap file descriptors */
            for (size_t n = 0; n < tfds.n_memb; n++)
                close(*((int *) tfds.data[n]));

            /* load snapshot and resume vm after suspend */
            if (flags & NM_VMCTL_CONT) {
                nm_qmp_loadvm(name, &snap);
                nm_qmp_vm_resume(name);
            }
        }
    }

    nm_str_free(&buf);
    nm_str_free(&snap);
    nm_vect_free(&argv, NULL);
    nm_vect_free(&tfds, NULL);
    nm_vmctl_free_data(&vm);
}

void nm_vmctl_delete(const nm_str_t *name)
{
    nm_str_t vmdir = NM_INIT_STR;
    nm_str_t query = NM_INIT_STR;
    nm_vect_t drives = NM_INIT_VECT;
    nm_vect_t snaps = NM_INIT_VECT;
    int delete_ok = NM_TRUE;

    nm_str_format(&vmdir, "%s/%s/", nm_cfg_get()->vm_dir.data, name->data);

    nm_str_format(&query, NM_SELECT_DRIVE_NAMES_SQL, name->data);
    nm_db_select(query.data, &drives);

    for (size_t n = 0; n < drives.n_memb; n++) {
        nm_str_t img_path = NM_INIT_STR;
        nm_str_format(&img_path, "%s%s", vmdir.data, nm_vect_str(&drives, n)->data);

        if (unlink(img_path.data) == -1)
            delete_ok = NM_FALSE;

        nm_str_free(&img_path);
    }

    { /* delete pid and QMP socket if exists */
        nm_str_t path = NM_INIT_STR;

        nm_str_format(&path, "%s%s", vmdir.data, NM_VM_PID_FILE);

        if (unlink(path.data) == -1 && errno != ENOENT)
            delete_ok = NM_FALSE;

        nm_str_trunc(&path, vmdir.len);
        nm_str_add_text(&path, NM_VM_QMP_FILE);
        if (unlink(path.data) == -1 && errno != ENOENT)
            delete_ok = NM_FALSE;

        nm_str_free(&path);
    }

    if (delete_ok) {
        if (rmdir(vmdir.data) == -1)
            delete_ok = NM_FALSE;
    }

    nm_vmctl_clear_tap(name);

    nm_str_format(&query, NM_DEL_DRIVES_SQL, name->data);
    nm_db_edit(query.data);

    nm_str_format(&query, NM_DEL_VMSNAP_SQL, name->data);
    nm_db_edit(query.data);

    nm_str_format(&query, NM_DEL_IFS_SQL, name->data);
    nm_db_edit(query.data);

    nm_str_format(&query, NM_DEL_USB_SQL, name->data);
    nm_db_edit(query.data);

    nm_str_format(&query, NM_DEL_VM_SQL, name->data);
    nm_db_edit(query.data);

    if (!delete_ok)
        nm_warn(_(NM_MSG_INC_DEL));

    nm_str_free(&vmdir);
    nm_str_free(&query);
    nm_vect_free(&drives, nm_str_vect_free_cb);
    nm_vect_free(&snaps, nm_str_vect_free_cb);
}

void nm_vmctl_kill(const nm_str_t *name)
{
    pid_t pid;
    int fd;
    char buf[10];
    nm_str_t pid_file = NM_INIT_STR;

    nm_str_format(&pid_file, "%s/%s/%s",
        nm_cfg_get()->vm_dir.data, name->data, NM_VM_PID_FILE);

    if ((fd = open(pid_file.data, O_RDONLY)) == -1)
        return;

    if (read(fd, buf, sizeof(buf)) <= 0) {
         close(fd);
         return;
    }

    pid = atoi(buf);
    kill(pid, SIGTERM);

    close(fd);

    nm_str_free(&pid_file);
}

#if defined(NM_WITH_VNC_CLIENT) || defined(NM_WITH_SPICE)
void nm_vmctl_connect(const nm_str_t *name)
{
    nm_str_t cmd = NM_INIT_STR;
    nm_str_t query = NM_INIT_STR;
    nm_vect_t res = NM_INIT_VECT;
    uint32_t port;
    int unused __attribute__((unused));

    nm_str_format(&query, NM_VMCTL_GET_VNC_PORT_SQL, name->data);
    nm_db_select(query.data, &res);
    port = nm_str_stoui(nm_vect_str(&res, 0), 10) + NM_STARTING_VNC_PORT;
#if defined(NM_WITH_SPICE)
    if (nm_str_cmp_st(nm_vect_str(&res, 1), NM_ENABLE) == NM_OK) {
        nm_vmctl_gen_viewer(name, port, &cmd, NM_VIEWER_SPICE);
    } else {
#endif
        nm_vmctl_gen_viewer(name, port, &cmd, NM_VIEWER_VNC);
#if defined(NM_WITH_SPICE)
    }
#endif
    unused = system(cmd.data);

    nm_vect_free(&res, nm_str_vect_free_cb);
    nm_str_free(&query);
    nm_str_free(&cmd);
}
#endif

void nm_vmctl_gen_cmd(nm_vect_t *argv, const nm_vmctl_data_t *vm,
    const nm_str_t *name, int *flags, nm_vect_t *tfds, nm_str_t *snap)
{
    nm_str_t vmdir = NM_INIT_STR;
    const nm_cfg_t *cfg = nm_cfg_get();
    size_t drives_count = vm->drives.n_memb / NM_DRV_IDX_COUNT;
    size_t ifs_count = vm->ifs.n_memb / NM_IFS_IDX_COUNT;
    int scsi_added = NM_FALSE;
    nm_cpu_t cpu = NM_INIT_CPU;
    nm_str_t buf = NM_INIT_STR;

    nm_str_format(&vmdir, "%s/%s/", cfg->vm_dir.data, name->data);

    nm_str_format(&buf, "%s/%s%s",
        cfg->qemu_bin_path.data, "qemu-system-",
        nm_vect_str(&vm->main, NM_SQL_ARCH)->data);
    nm_vect_insert(argv, buf.data, buf.len + 1, NULL);

    nm_vect_insert_cstr(argv, "-daemonize");

    if (nm_str_cmp_st(nm_vect_str(&vm->main, NM_SQL_USBF), NM_ENABLE) == NM_OK) {
        size_t usb_count = vm->usb.n_memb / NM_USB_IDX_COUNT;
        nm_vect_t usb_list = NM_INIT_VECT;
        nm_vect_t serial_cache = NM_INIT_VECT;
        nm_str_t serial = NM_INIT_STR;

        nm_vect_insert_cstr(argv, "-usb");
        nm_vect_insert_cstr(argv, "-device");

        if (nm_str_cmp_st(nm_vect_str(&vm->main, NM_SQL_USBT), NM_DEFAULT_USBVER) == NM_OK)
            nm_vect_insert_cstr(argv, "qemu-xhci,id=usbbus");
        else if (nm_str_cmp_st(nm_vect_str(&vm->main, NM_SQL_USBT), *nm_form_usbtype) == NM_OK)
            nm_vect_insert_cstr(argv, "usb-ehci,id=usbbus");
        else
            nm_vect_insert_cstr(argv, "nec-usb-xhci,id=usbbus");

        if (usb_count > 0)
            nm_usb_get_devs(&usb_list);

        for (size_t n = 0; n < usb_count; n++) {
            int found_in_cache = 0;
            int found_in_devs = 0;
            size_t idx_shift = NM_USB_IDX_COUNT * n;
            nm_usb_dev_t *usb = NULL;

            const char *vid = nm_vect_str_ctx(&vm->usb, NM_SQL_USB_VID + idx_shift);
            const char *pid = nm_vect_str_ctx(&vm->usb, NM_SQL_USB_PID + idx_shift);
            const char *ser = nm_vect_str_ctx(&vm->usb, NM_SQL_USB_SERIAL + idx_shift);

            /* look for cached data first, libusb_open() is very expensive */
            for (size_t m = 0; m < serial_cache.n_memb; m++) {
                usb = *nm_usb_data_dev(serial_cache.data[m]);
                nm_str_t *ser_str = nm_usb_data_serial(serial_cache.data[m]);

                if ((nm_str_cmp_st(nm_usb_vendor_id(usb), vid) == NM_OK) &&
                    (nm_str_cmp_st(nm_usb_product_id(usb), pid) == NM_OK) &&
                    (nm_str_cmp_st(ser_str, ser) == NM_OK)) {
                    found_in_cache = 1;
                    break;
                }
            }

            if (found_in_cache && usb) {
                nm_vect_insert_cstr(argv, "-device");
                nm_str_format(&buf, "usb-host,hostbus=%d,hostaddr=%d,id=usb-%s-%s-%s,bus=usbbus.0",
                    *nm_usb_bus_num(usb), *nm_usb_dev_addr(usb),
                    nm_usb_vendor_id(usb)->data, nm_usb_product_id(usb)->data, ser);
                nm_vect_insert(argv, buf.data, buf.len + 1, NULL);

                continue;
            }

            for (size_t m = 0; m < usb_list.n_memb; m++) {
                usb = (nm_usb_dev_t *) usb_list.data[m];
                if ((nm_str_cmp_st(nm_usb_vendor_id(usb), vid) == NM_OK) &&
                    (nm_str_cmp_st(nm_usb_product_id(usb), pid) == NM_OK)) {
                    if (serial.len)
                        nm_str_trunc(&serial, 0);

                    nm_usb_get_serial(usb, &serial);
                    if (nm_str_cmp_st(&serial, ser) == NM_OK) {
                        found_in_devs = 1;
                        break;
                    } else {
                        /* save result in cache */
                        nm_usb_data_t usb_data = NM_INIT_USB_DATA;
                        nm_str_copy(&usb_data.serial, &serial);
                        usb_data.dev = usb;
                        nm_vect_insert(&serial_cache, &usb_data,
                                sizeof(usb_data), nm_usb_data_vect_ins_cb);
                        nm_str_free(&usb_data.serial);
                    }
                }
            }

            if (found_in_devs && usb) {
                nm_vect_insert_cstr(argv, "-device");
                nm_str_format(&buf, "usb-host,hostbus=%d,hostaddr=%d,id=usb-%s-%s-%s,bus=usbbus.0",
                    *nm_usb_bus_num(usb), *nm_usb_dev_addr(usb),
                    nm_usb_vendor_id(usb)->data, nm_usb_product_id(usb)->data, ser);
                nm_vect_insert(argv, buf.data, buf.len + 1, NULL);

                continue;
            }
        }

        nm_vect_free(&serial_cache, nm_usb_data_vect_free_cb);
        nm_vect_free(&usb_list, nm_usb_vect_free_cb);
        nm_str_free(&serial);
    }

    /* setup install source */
    if (nm_str_cmp_st(nm_vect_str(&vm->main, NM_SQL_INST), NM_ENABLE) == NM_OK) {
        const char *iso = nm_vect_str_ctx(&vm->main, NM_SQL_ISO);
        size_t srcp_len = strlen(iso);

        if ((srcp_len == 0) && (!(*flags & NM_VMCTL_INFO))) {
            nm_warn(_(NM_MSG_ISO_MISS));
            nm_vect_free(argv, NULL);
            goto out;
        }
        if ((srcp_len > 4) &&
            (nm_str_case_cmp_tt(iso + (srcp_len - 4), ".iso") == NM_OK)) {
            nm_vect_insert_cstr(argv, "-boot");
            nm_vect_insert_cstr(argv, "d");
            nm_vect_insert_cstr(argv, "-cdrom");
            nm_vect_insert_cstr(argv, iso);
        } else {
            nm_vect_insert_cstr(argv, "-drive");
            nm_str_format(&buf, "id=usb0,if=none,file=%s", iso);
            nm_vect_insert(argv, buf.data, buf.len + 1, NULL);

            nm_vect_insert_cstr(argv, "-device");
            nm_vect_insert_cstr(argv, "usb-storage,drive=usb0,bus=usbbus.0,bootindex=1");
        }
    } else { /* just mount cdrom */
        const char *iso = nm_vect_str_ctx(&vm->main, NM_SQL_ISO);
        size_t srcp_len = strlen(iso);
        struct stat info;
        int rc = -1;

        if (srcp_len > 0) {
            memset(&info, 0x0, sizeof(info));
            rc = stat(iso, &info);

            if ((rc == -1) && (!(*flags & NM_VMCTL_INFO))) {
                nm_warn(_(NM_MSG_ISO_NF));
                nm_vect_free(argv, NULL);
                goto out;
            }
            if (rc != -1) {
                nm_vect_insert_cstr(argv, "-cdrom");
                nm_vect_insert_cstr(argv, iso);
            }
        }
    }

    for (size_t n = 0; n < drives_count; n++) {
        int nvme_drv = NM_FALSE;
        int scsi_drv = NM_FALSE;
        size_t idx_shift = NM_DRV_IDX_COUNT * n;
        const nm_str_t *drive_img = nm_vect_str(&vm->drives, NM_SQL_DRV_NAME + idx_shift);
        const nm_str_t *blk_drv = nm_vect_str(&vm->drives, NM_SQL_DRV_TYPE + idx_shift);
        const nm_str_t *discard = nm_vect_str(&vm->drives, NM_SQL_DRV_DISC + idx_shift);
        const char *blk_drv_type = blk_drv->data;

        if (nm_str_cmp_st(blk_drv, "nvme") == NM_OK) {
            nvme_drv = NM_TRUE;
            blk_drv_type = "none";
        } else if (nm_str_cmp_st(blk_drv, "scsi") == NM_OK) {
            scsi_drv = NM_TRUE;
            blk_drv_type = "none";
            if (!scsi_added) {
                nm_vect_insert_cstr(argv, "-device");
                nm_vect_insert_cstr(argv, "virtio-scsi-pci,id=scsi");
                scsi_added = NM_TRUE;
            }
        }

        nm_vect_insert_cstr(argv, "-drive");

        nm_str_format(&buf, "node-name=hd%zu,media=disk,if=%s,file=%s%s",
            n, blk_drv_type, vmdir.data, drive_img->data);
        if (scsi_added && (nm_str_cmp_st(discard, NM_ENABLE) == NM_OK)) {
            nm_str_append_format(&buf, "%s", ",discard=unmap,detect-zeroes=unmap");
        }

        nm_vect_insert(argv, buf.data, buf.len + 1, NULL);

        if (nvme_drv) {
            long int host_id = labs(gethostid());
            nm_vect_insert_cstr(argv, "-device");
            nm_str_format(&buf, "nvme,drive=hd%zu,serial=%lX%zX", n, host_id, n);
            nm_vect_insert(argv, buf.data, buf.len + 1, NULL);
        } else if (scsi_drv) {
            nm_vect_insert_cstr(argv, "-device");
            nm_str_format(&buf, "scsi-hd,drive=hd%zu", n);
            nm_vect_insert(argv, buf.data, buf.len + 1, NULL);
        }
    }

    /* load vm snapshot if exists */
    {
        nm_str_t query = NM_INIT_STR;
        nm_vect_t snap_res = NM_INIT_VECT;

        nm_str_format(&query, NM_GET_VMSNAP_LOAD_SQL, name->data);
        nm_db_select(query.data, &snap_res);

        if (snap_res.n_memb > 0) {
            nm_vect_insert_cstr(argv, "-S");
            nm_str_format(snap, "%s", nm_vect_str_ctx(&snap_res, 0));

            /* reset load flag */
            if (!(*flags & NM_VMCTL_INFO)) {
                nm_str_format(&query, NM_RESET_LOAD_SQL, name->data);
                nm_db_edit(query.data);
            }

            *flags |= NM_VMCTL_CONT;
        }

        nm_str_free(&query);
        nm_vect_free(&snap_res, nm_str_vect_free_cb);
    }

    nm_vect_insert_cstr(argv, "-m");
    nm_vect_insert(argv,
        nm_vect_str(&vm->main, NM_SQL_MEM)->data,
        nm_vect_str(&vm->main, NM_SQL_MEM)->len + 1, NULL);

    nm_parse_smp(&cpu, nm_vect_str_ctx(&vm->main, NM_SQL_SMP));
    if (cpu.smp > 1) {
        nm_str_trunc(&buf, 0);
        nm_vect_insert_cstr(argv, "-smp");

        if (!cpu.sockets) {
            nm_str_format(&buf, "%zu", cpu.smp);
        } else if (cpu.threads) {
            nm_str_format(&buf, "%zu,sockets=%zu,cores=%zu,threads=%zu",
                    cpu.smp, cpu.sockets, cpu.cores, cpu.threads);
        } else {
            nm_str_format(&buf, "%zu,sockets=%zu,cores=%zu",
                    cpu.smp, cpu.sockets, cpu.cores);
        }
        nm_vect_insert(argv, buf.data, buf.len + 1, NULL);
    }

    /* 9p sharing.
     *
     * guest mount example:
     * mount -t 9p -o trans=virtio,version=9p2000.L hostshare /mnt/host
     */
    if (nm_str_cmp_st(nm_vect_str(&vm->main, NM_SQL_9FLG), NM_ENABLE) == NM_OK) {
        nm_vect_insert_cstr(argv, "-fsdev");
        nm_str_format(&buf, "local,security_model=none,id=fsdev0,path=%s",
            nm_vect_str(&vm->main, NM_SQL_9PTH)->data);
        nm_vect_insert(argv, buf.data, buf.len + 1, NULL);

        nm_vect_insert_cstr(argv, "-device");
        nm_str_format(&buf, "virtio-9p-pci,fsdev=fsdev0,mount_tag=%s",
            nm_vect_str(&vm->main, NM_SQL_9ID)->data);
        nm_vect_insert(argv, buf.data, buf.len + 1, NULL);
    }

    if (nm_str_cmp_st(nm_vect_str(&vm->main, NM_SQL_KVM), NM_ENABLE) == NM_OK) {
        nm_vect_insert_cstr(argv, "-enable-kvm");
        if (nm_str_cmp_st(nm_vect_str(&vm->main, NM_SQL_HCPU), NM_ENABLE) == NM_OK) {
            nm_vect_insert_cstr(argv, "-cpu");
            nm_vect_insert_cstr(argv, "host");
        }
    }

    /* Save info about usb subsystem status at boot time.
     * Needed for USB hotplug feature. */
    if (!(*flags & NM_VMCTL_INFO)) {
        nm_str_t query = NM_INIT_STR;
        nm_str_format(&query,
                      NM_USB_UPDATE_STATE_SQL,
                      nm_vect_str_ctx(&vm->main, NM_SQL_USBF),
                      name->data);
        nm_db_edit(query.data);
        nm_str_free(&query);
    }

    if (nm_vect_str_len(&vm->main, NM_SQL_BIOS)) {
        nm_vect_insert_cstr(argv, "-bios");
        nm_vect_insert(argv,
            nm_vect_str(&vm->main, NM_SQL_BIOS)->data,
            nm_vect_str(&vm->main, NM_SQL_BIOS)->len + 1, NULL);
    }

    if (nm_vect_str_len(&vm->main, NM_SQL_MACH)) {
        nm_vect_insert_cstr(argv, "-M");
        nm_vect_insert(argv,
            nm_vect_str(&vm->main, NM_SQL_MACH)->data,
            nm_vect_str(&vm->main, NM_SQL_MACH)->len + 1, NULL);
    }

    if (nm_vect_str_len(&vm->main, NM_SQL_KERN)) {
        nm_vect_insert_cstr(argv, "-kernel");
        nm_vect_insert(argv,
            nm_vect_str(&vm->main, NM_SQL_KERN)->data,
            nm_vect_str(&vm->main, NM_SQL_KERN)->len + 1, NULL);

        if (nm_vect_str_len(&vm->main, NM_SQL_KAPP)) {
            nm_vect_insert_cstr(argv, "-append");
            nm_vect_insert(argv,
                nm_vect_str(&vm->main, NM_SQL_KAPP)->data,
                nm_vect_str(&vm->main, NM_SQL_KAPP)->len + 1, NULL);
        }
    }

    if (nm_vect_str_len(&vm->main, NM_SQL_INIT)) {
        nm_vect_insert_cstr(argv, "-initrd");
        nm_vect_insert(argv,
            nm_vect_str(&vm->main, NM_SQL_INIT)->data,
            nm_vect_str(&vm->main, NM_SQL_INIT)->len + 1, NULL);
    }

    if (nm_str_cmp_st(nm_vect_str(&vm->main, NM_SQL_OVER), NM_ENABLE) == NM_OK) {
        nm_vect_insert_cstr(argv, "-device");
        nm_vect_insert_cstr(argv, "usb-tablet,bus=usbbus.0");
    }

    /* setup serial socket */
    if (nm_vect_str_len(&vm->main, NM_SQL_SOCK)) {
        if (!(*flags & NM_VMCTL_INFO)) {
            struct stat info;

            if (stat(nm_vect_str_ctx(&vm->main, NM_SQL_SOCK), &info) != -1) {
                nm_warn(_(NM_MSG_SOCK_USED));
                nm_vect_free(argv, NULL);
                goto out;
            }
        }

        nm_vect_insert_cstr(argv, "-chardev");
        nm_str_format(&buf, "socket,path=%s,server,nowait,id=socket_%s",
            nm_vect_str(&vm->main, NM_SQL_SOCK)->data, name->data);
        nm_vect_insert(argv, buf.data, buf.len + 1, NULL);

        nm_vect_insert_cstr(argv, "-device");
        nm_str_format(&buf, "isa-serial,chardev=socket_%s", name->data);
        nm_vect_insert(argv, buf.data, buf.len + 1, NULL);
    }

    /* setup debug port for GDB */
    if (nm_vect_str_len(&vm->main, NM_SQL_DEBP)) {
        nm_vect_insert_cstr(argv, "-gdb");
        nm_str_format(&buf, "tcp::%s",
            nm_vect_str(&vm->main, NM_SQL_DEBP)->data);
        nm_vect_insert(argv, buf.data, buf.len + 1, NULL);
    }
    if (nm_str_cmp_st(nm_vect_str(&vm->main, NM_SQL_DEBF), NM_ENABLE) == NM_OK)
        nm_vect_insert_cstr(argv, "-S");

    /* setup serial TTY */
    if (nm_vect_str_len(&vm->main, NM_SQL_TTY)) {
        if (!(*flags & NM_VMCTL_INFO)) {
            int fd;

            if ((fd = open(nm_vect_str_ctx(&vm->main, NM_SQL_TTY), O_RDONLY)) == -1) {
                nm_warn(_(NM_MSG_TTY_MISS));
                nm_vect_free(argv, NULL);
                goto out;
            }

            if (!isatty(fd)) {
                close(fd);
                nm_warn(_(NM_MSG_TTY_INVAL));
                nm_vect_free(argv, NULL);
                goto out;
            }
        }

        nm_vect_insert_cstr(argv, "-chardev");
        nm_str_format(&buf, "tty,path=%s,id=tty_%s",
            nm_vect_str(&vm->main, NM_SQL_TTY)->data,
            name->data);
        nm_vect_insert(argv, buf.data, buf.len + 1, NULL);

        nm_vect_insert_cstr(argv, "-device");
        nm_str_format(&buf, "isa-serial,chardev=tty_%s",
            name->data);
        nm_vect_insert(argv, buf.data, buf.len + 1, NULL);
    }

    /* setup network interfaces */
    for (size_t n = 0; n < ifs_count; n++) {
        size_t idx_shift = NM_IFS_IDX_COUNT * n;

        nm_vect_insert_cstr(argv, "-device");
        nm_str_format(&buf, "%s,mac=%s,netdev=netdev%zu",
            nm_vect_str(&vm->ifs, NM_SQL_IF_DRV + idx_shift)->data,
            nm_vect_str(&vm->ifs, NM_SQL_IF_MAC + idx_shift)->data,
            n);
        nm_vect_insert(argv, buf.data, buf.len + 1, NULL);

        if (nm_str_cmp_st(nm_vect_str(&vm->ifs, NM_SQL_IF_USR + idx_shift),
            NM_ENABLE) == NM_OK) {
#if defined (NM_OS_LINUX)
            if (!(*flags & NM_VMCTL_INFO)) {
                /* Delete iface if exists, we are in user mode */
                uint32_t tap_idx = 0;
                tap_idx = nm_net_iface_idx(nm_vect_str(&vm->ifs,
                            NM_SQL_IF_NAME + idx_shift));

                if (tap_idx != 0) { /* iface exist */
                    /* detect iface type */
                    struct stat tap_info;
                    nm_str_t tap_path = NM_INIT_STR;

                    nm_str_format(&tap_path, "/dev/tap%u", tap_idx);
                    if (stat(tap_path.data, &tap_info) == 0) {
                        /* iface is macvtap, delete it */
                        nm_net_del_iface(nm_vect_str(&vm->ifs,
                                    NM_SQL_IF_NAME + idx_shift));
                    } else {
                        /* iface is simple tap, delete it */
                        nm_net_del_tap(nm_vect_str(&vm->ifs,
                                    NM_SQL_IF_NAME + idx_shift));
                    }
                    nm_str_free(&tap_path);
                }
            }
#endif /* NM_OS_LINUX */

            nm_vect_insert_cstr(argv, "-netdev");
            nm_str_format(&buf, "user,id=netdev%zu", n);

            if (nm_vect_str_len(&vm->ifs, NM_SQL_IF_FWD + idx_shift) != 0) {
                nm_str_append_format(&buf, ",hostfwd=%s",
                        nm_vect_str_ctx(&vm->ifs, NM_SQL_IF_FWD + idx_shift));
            }
            if (nm_vect_str_len(&vm->ifs, NM_SQL_IF_SMB + idx_shift) != 0) {
                nm_str_append_format(&buf, ",smb=%s",
                        nm_vect_str_ctx(&vm->ifs, NM_SQL_IF_SMB + idx_shift));
            }

        } else if (nm_str_cmp_st(nm_vect_str(&vm->ifs, NM_SQL_IF_MVT + idx_shift),
            NM_DISABLE) == NM_OK) {
            nm_vect_insert_cstr(argv, "-netdev");
            nm_str_format(&buf, "tap,ifname=%s,script=no,downscript=no,id=netdev%zu",
                nm_vect_str(&vm->ifs, NM_SQL_IF_NAME + idx_shift)->data, n);

#if defined (NM_OS_LINUX)
            /* Delete macvtap iface if exists, we using simple tap iface now.
             * We do not need to create tap interface: QEMU will create it itself. */
            if (!(*flags & NM_VMCTL_INFO)) {
                uint32_t tap_idx = 0;
                tap_idx = nm_net_iface_idx(nm_vect_str(&vm->ifs,
                    NM_SQL_IF_NAME + idx_shift));

                if (tap_idx != 0) {
                    /* is this iface macvtap? */
                    struct stat tap_info;
                    nm_str_t tap_path = NM_INIT_STR;

                    nm_str_format(&tap_path, "/dev/tap%u", tap_idx);
                    if (stat(tap_path.data, &tap_info) == 0) {
                        /* iface is macvtap, delete it */
                        nm_net_del_iface(nm_vect_str(&vm->ifs,
                            NM_SQL_IF_NAME + idx_shift));
                    }
                    nm_str_free(&tap_path);
                }
            }
#endif /* NM_OS_LINUX */
        } else {
#if defined (NM_OS_LINUX)
            int tap_fd = 0, wait_perm = 0;

            if (!(*flags & NM_VMCTL_INFO)) {
                nm_str_t tap_path = NM_INIT_STR;
                uint32_t tap_idx = 0;

                /* Delete simple tap iface if exists, we using macvtap iface now */
                if ((tap_idx = nm_net_iface_idx(nm_vect_str(&vm->ifs,
                        NM_SQL_IF_NAME + idx_shift))) != 0) {
                    /* is this iface simple tap? */
                    struct stat tap_info;
                    nm_str_format(&tap_path, "/dev/tap%u", tap_idx);
                    if (stat(tap_path.data, &tap_info) != 0) {
                        /* iface is simple tap, delete it */
                        nm_net_del_tap(nm_vect_str(&vm->ifs,
                                                   NM_SQL_IF_NAME + idx_shift));
                    }

                    tap_idx = 0;
                }

                if (nm_net_iface_exists(nm_vect_str(&vm->ifs, NM_SQL_IF_NAME + idx_shift)) != NM_OK) {
                    wait_perm = 1;
                    int macvtap_type = nm_str_stoui(nm_vect_str(&vm->ifs,
                                                                NM_SQL_IF_MVT + idx_shift), 10);

                    /* check for lower iface (parent) exists */
                    if (nm_vect_str_len(&vm->ifs, NM_SQL_IF_PET + idx_shift) == 0) {
                        nm_warn(_(NM_MSG_MTAP_NSET));
                        nm_vect_free(argv, NULL);
                        goto out;
                    }

                    nm_net_add_macvtap(nm_vect_str(&vm->ifs, NM_SQL_IF_NAME + idx_shift),
                                       nm_vect_str(&vm->ifs, NM_SQL_IF_PET + idx_shift),
                                       nm_vect_str(&vm->ifs, NM_SQL_IF_MAC + idx_shift),
                                       macvtap_type);

                    if (nm_vect_str_len(&vm->ifs, NM_SQL_IF_ALT + idx_shift) != 0) {
                        nm_net_set_altname(nm_vect_str(&vm->ifs, NM_SQL_IF_NAME + idx_shift),
                                nm_vect_str(&vm->ifs, NM_SQL_IF_ALT + idx_shift));
                    }
                }

                tap_idx = nm_net_iface_idx(nm_vect_str(&vm->ifs,
                    NM_SQL_IF_NAME + idx_shift));
                if (tap_idx == 0)
                    nm_bug("%s: MacVTap interface not found", __func__);

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
                        nm_vect_free(argv, NULL);
                        goto out;
                    }
                }

                tap_fd = open(tap_path.data, O_RDWR);
                if (tap_fd == -1)
                    nm_bug("%s: open failed: %s", __func__, strerror(errno));
                if (tfds == NULL)
                    nm_bug("%s: tfds is NULL", __func__);
                nm_vect_insert(tfds, &tap_fd, sizeof(int), NULL);
                nm_str_free(&tap_path);
            }

            nm_vect_insert_cstr(argv, "-netdev");
            nm_str_format(&buf, "tap,id=netdev%zu,fd=%d",
                n, (*flags & NM_VMCTL_INFO) ? -1 : tap_fd);
#endif /* NM_OS_LINUX */
        }
        if ((nm_str_cmp_st(nm_vect_str(&vm->ifs, NM_SQL_IF_VHO + idx_shift), NM_ENABLE) == NM_OK) &&
            (nm_str_cmp_st(nm_vect_str(&vm->ifs, NM_SQL_IF_USR + idx_shift), NM_DISABLE) == NM_OK))
            nm_str_add_text(&buf, ",vhost=on");
        nm_vect_insert(argv, buf.data, buf.len + 1, NULL);

#if defined (NM_OS_LINUX)
        /* Simple tap interface additional setup:
         * If we need to setup IPv4 address or altname we must create
         * the tap interface yourself. */
        if ((nm_str_cmp_st(nm_vect_str(&vm->ifs, NM_SQL_IF_USR + idx_shift), NM_DISABLE) == NM_OK)) {
            if ((!(*flags & NM_VMCTL_INFO)) &&
                    (nm_net_iface_exists(nm_vect_str(&vm->ifs, NM_SQL_IF_NAME + idx_shift)) != NM_OK) &&
                    (nm_str_cmp_st(nm_vect_str(&vm->ifs, NM_SQL_IF_MVT + idx_shift), NM_DISABLE) == NM_OK)) {
                nm_net_add_tap(nm_vect_str(&vm->ifs, NM_SQL_IF_NAME + idx_shift));

                if (nm_vect_str_len(&vm->ifs, NM_SQL_IF_IP4 + idx_shift) != 0) {
                    nm_net_set_ipaddr(nm_vect_str(&vm->ifs, NM_SQL_IF_NAME + idx_shift),
                            nm_vect_str(&vm->ifs, NM_SQL_IF_IP4 + idx_shift));
                }
                if (nm_vect_str_len(&vm->ifs, NM_SQL_IF_ALT + idx_shift) != 0) {
                    nm_net_set_altname(nm_vect_str(&vm->ifs, NM_SQL_IF_NAME + idx_shift),
                            nm_vect_str(&vm->ifs, NM_SQL_IF_ALT + idx_shift));
                }
            }
        }
#elif defined (NM_OS_FREEBSD)
        if (nm_net_iface_exists(nm_vect_str(&vm->ifs, NM_SQL_IF_NAME + idx_shift)) == NM_OK) {
            nm_net_del_tap(nm_vect_str(&vm->ifs, NM_SQL_IF_NAME + idx_shift));
        }
        (void) tfds;
#endif /* NM_OS_LINUX */
    }

    if (*flags & NM_VMCTL_TEMP)
        nm_vect_insert_cstr(argv, "-snapshot");

    nm_vect_insert_cstr(argv, "-pidfile");
    nm_str_format(&buf, "%s%s",
        vmdir.data, NM_VM_PID_FILE);
    nm_vect_insert(argv, buf.data, buf.len + 1, NULL);

    nm_vect_insert_cstr(argv, "-qmp");
    nm_str_format(&buf, "unix:%s%s,server,nowait",
        vmdir.data, NM_VM_QMP_FILE);
    nm_vect_insert(argv, buf.data, buf.len + 1, NULL);

    /* Check if vnc/spice port is available, generate new one if not */
    if (!(*flags & NM_VMCTL_INFO)) {
        uint32_t in_addr = cfg->listen_any ? INADDR_ANY : INADDR_LOOPBACK;
        uint32_t curr_port = nm_str_stoui(nm_vect_str(&vm->main, NM_SQL_VNC), 10) + NM_STARTING_VNC_PORT;
        if (curr_port > 0xffff)
            nm_bug("%s: port number overflow", __func__);

        if (!nm_net_check_port((uint16_t)curr_port, SOCK_STREAM, in_addr)) {
            nm_vect_t vms_ports = NM_INIT_VECT;
            uint32_t *occupied_ports;

            nm_db_select("SELECT vnc FROM vms ORDER BY vnc ASC", &vms_ports);

            occupied_ports = (uint32_t *)calloc(vms_ports.n_memb, sizeof(uint32_t));
            if (vms_ports.n_memb > 0 && !occupied_ports)
                nm_bug("%s: couldn't allocate memory for occupied ports", __func__);

            for (size_t i = 0; i < vms_ports.n_memb; ++i)
                occupied_ports[i] = nm_str_stoui(vms_ports.data[i], 10) + NM_STARTING_VNC_PORT;

            for (curr_port = NM_STARTING_VNC_PORT; curr_port <= 0xffff; ++curr_port) {
                if (bsearch(&curr_port, occupied_ports, vms_ports.n_memb, sizeof(uint32_t), compar_uint32_t))
                    continue;

                if (nm_net_check_port((uint16_t)curr_port, SOCK_STREAM, in_addr))
                    break;
            }

            nm_str_format(nm_vect_str(&vm->main, NM_SQL_VNC), "%u", curr_port - NM_STARTING_VNC_PORT);
            nm_str_t query = NM_INIT_STR;
            nm_str_format(&query, "UPDATE vms SET vnc='%u' WHERE name='%s'",
                curr_port - NM_STARTING_VNC_PORT, nm_vect_str_ctx(&vm->main, NM_SQL_NAME));
            nm_db_edit(query.data);

            if (occupied_ports)
                free(occupied_ports);
            nm_str_free(&query);
            nm_vect_free(&vms_ports, nm_str_vect_free_cb);
        }
    }

#if defined (NM_WITH_SPICE)
    if (nm_str_cmp_st(nm_vect_str(&vm->main, NM_SQL_SPICE), NM_ENABLE) == NM_OK) {
        nm_vect_insert_cstr(argv, "-vga");
        nm_vect_insert_cstr(argv, (nm_vect_str(&vm->main, NM_SQL_DISPLAY))->data);
        nm_vect_insert_cstr(argv, "-spice");
        nm_str_format(&buf, "port=%u,disable-ticketing",
            nm_str_stoui(nm_vect_str(&vm->main, NM_SQL_VNC), 10) + NM_STARTING_VNC_PORT);
        if (!cfg->listen_any)
            nm_str_append_format(&buf, ",addr=127.0.0.1");
        nm_vect_insert(argv, buf.data, buf.len + 1, NULL);
    } else {
#endif
    nm_vect_insert_cstr(argv, "-vnc");
    if (cfg->listen_any)
        nm_str_format(&buf, ":");
    else
        nm_str_format(&buf, "127.0.0.1:");
    nm_str_add_str(&buf, nm_vect_str(&vm->main, NM_SQL_VNC));
    nm_vect_insert(argv, buf.data, buf.len + 1, NULL);
#if defined (NM_WITH_SPICE)
    }
#endif

    if (nm_vect_str_len(&vm->main, NM_SQL_ARGS)) {
        nm_vect_t args = NM_INIT_VECT;

        nm_str_append_to_vect(nm_vect_str(&vm->main, NM_SQL_ARGS), &args, " ");

        for (size_t n = 0; n < args.n_memb; n++)
            nm_vect_insert_cstr(argv, args.data[n]);

        nm_vect_free(&args, NULL);
    }

    nm_vect_end_zero(argv);

    nm_cmd_str(&buf, argv);
    nm_debug("cmd=%s\n", buf.data);

out:
    nm_str_free(&vmdir);
    nm_str_free(&buf);
}

nm_str_t nm_vmctl_info(const nm_str_t *name)
{
    nm_str_t info = NM_INIT_STR;
    nm_vmctl_data_t vm = NM_VMCTL_INIT_DATA;
    int status;
    size_t ifs_count, drives_count;

    nm_vmctl_get_data(name, &vm);

    nm_str_format(&info, "%-12s%s\n", "name: ", name->data);

    status = nm_qmp_test_socket(name);
    nm_str_append_format(&info, "%-12s%s\n", "status: ",
        status == NM_OK ? "running" : "stopped");

    nm_str_append_format(&info, "%-12s%s\n", "arch: ",
        nm_vect_str_ctx(&vm.main, NM_SQL_ARCH));
    nm_str_append_format(&info, "%-12s%s\n", "cores: ",
        nm_vect_str_ctx(&vm.main, NM_SQL_SMP));
    nm_str_append_format(&info, "%-12s%s Mb\n","memory: ",
        nm_vect_str_ctx(&vm.main, NM_SQL_MEM));

    if (nm_str_cmp_st(nm_vect_str(&vm.main, NM_SQL_KVM), NM_ENABLE) == NM_OK) {
        if (nm_str_cmp_st(nm_vect_str(&vm.main, NM_SQL_HCPU), NM_ENABLE) == NM_OK)
            nm_str_append_format(&info, "%-12s%s\n", "kvm: ", "enabled [+hostcpu]");
        else
            nm_str_append_format(&info, "%-12s%s\n", "kvm: ", "enabled");
    } else {
        nm_str_append_format(&info, "%-12s%s\n", "kvm: ", "disabled");
    }

    if (nm_str_cmp_st(nm_vect_str(&vm.main, NM_SQL_USBF), "1") == NM_OK) {
        nm_str_append_format(&info, "%-12s%s [%s]\n", "usb: ", "enabled",
            nm_vect_str_ctx(&vm.main, NM_SQL_USBT));
    } else {
        nm_str_append_format(&info, "%-12s%s\n", "usb: ", "disabled");
    }

    nm_str_append_format(&info, "%-12s%s [%u]\n", "vnc port: ",
        nm_vect_str_ctx(&vm.main, NM_SQL_VNC),
        nm_str_stoui(nm_vect_str(&vm.main, NM_SQL_VNC), 10) + NM_STARTING_VNC_PORT);

    ifs_count = vm.ifs.n_memb / NM_IFS_IDX_COUNT;
    for (size_t n = 0; n < ifs_count; n++) {
        size_t idx_shift = NM_IFS_IDX_COUNT * n;

        nm_str_append_format(&info, "eth%zu%-8s%s [%s %s%s]\n",
            n, ":",
            nm_vect_str_ctx(&vm.ifs, NM_SQL_IF_NAME + idx_shift),
            nm_vect_str_ctx(&vm.ifs, NM_SQL_IF_MAC + idx_shift),
            nm_vect_str_ctx(&vm.ifs, NM_SQL_IF_DRV + idx_shift),
            (nm_str_cmp_st(nm_vect_str(&vm.ifs, NM_SQL_IF_VHO + idx_shift),
                NM_ENABLE) == NM_OK) ? "+vhost" : "");
    }

    drives_count = vm.drives.n_memb / NM_DRV_IDX_COUNT;
    for (size_t n = 0; n < drives_count; n++) {
        size_t idx_shift = NM_DRV_IDX_COUNT * n;
        int boot = 0;

        if (nm_str_cmp_st(nm_vect_str(&vm.drives, NM_SQL_DRV_BOOT + idx_shift),
                NM_ENABLE) == NM_OK)
            boot = 1;

        nm_str_append_format(&info, "disk%zu%-7s%s [%sGb %s] %s\n", n, ":",
            nm_vect_str_ctx(&vm.drives, NM_SQL_DRV_NAME + idx_shift),
            nm_vect_str_ctx(&vm.drives, NM_SQL_DRV_SIZE + idx_shift),
            nm_vect_str_ctx(&vm.drives, NM_SQL_DRV_TYPE + idx_shift),
            boot ? "*" : "");
    }

    if (nm_str_cmp_st(nm_vect_str(&vm.main, NM_SQL_9FLG), "1") == NM_OK) {
        nm_str_append_format(&info, "%-12s%s [%s]\n", "9pfs: ",
            nm_vect_str_ctx(&vm.main, NM_SQL_9PTH),
            nm_vect_str_ctx(&vm.main, NM_SQL_9ID));
    }

    if (nm_vect_str_len(&vm.main, NM_SQL_MACH))
        nm_str_append_format(&info, "%-12s%s\n", "machine: ",
            nm_vect_str_ctx(&vm.main, NM_SQL_MACH));

    if (nm_vect_str_len(&vm.main, NM_SQL_BIOS))
        nm_str_append_format(&info,"%-12s%s\n", "bios: ",
            nm_vect_str_ctx(&vm.main, NM_SQL_BIOS));

    if (nm_vect_str_len(&vm.main, NM_SQL_KERN))
        nm_str_append_format(&info,"%-12s%s\n", "kernel: ",
            nm_vect_str_ctx(&vm.main, NM_SQL_KERN));

    if (nm_vect_str_len(&vm.main, NM_SQL_KAPP))
        nm_str_append_format(&info, "%-12s%s\n", "cmdline: ",
            nm_vect_str_ctx(&vm.main, NM_SQL_KAPP));

    if (nm_vect_str_len(&vm.main, NM_SQL_INIT))
        nm_str_append_format(&info, "%-12s%s\n", "initrd: ",
            nm_vect_str_ctx(&vm.main, NM_SQL_INIT));

    if (nm_vect_str_len(&vm.main, NM_SQL_TTY))
        nm_str_append_format(&info, "%-12s%s\n", "tty: ",
            nm_vect_str_ctx(&vm.main, NM_SQL_TTY));

    if (nm_vect_str_len(&vm.main, NM_SQL_SOCK))
        nm_str_append_format(&info, "%-12s%s\n", "socket: ",
            nm_vect_str_ctx(&vm.main, NM_SQL_SOCK));

    if (nm_vect_str_len(&vm.main, NM_SQL_DEBP))
        nm_str_append_format(&info, "%-12s%s\n", "gdb port: ",
            nm_vect_str_ctx(&vm.main, NM_SQL_DEBP));

    if (nm_str_cmp_st(nm_vect_str(&vm.main, NM_SQL_DEBF), NM_ENABLE) == NM_OK)
        nm_str_append_format(&info, "%-12s\n", "freeze cpu: yes");

    if (nm_vect_str_len(&vm.main, NM_SQL_ARGS))
        nm_str_append_format(&info,"%-12s%s\n", "extra args: ",
            nm_vect_str_ctx(&vm.main, NM_SQL_ARGS));

    for (size_t n = 0; n < ifs_count; n++) {
        size_t idx_shift = NM_IFS_IDX_COUNT * n;

        if (!nm_vect_str_len(&vm.ifs, NM_SQL_IF_IP4 + idx_shift))
            continue;

        nm_str_append_format(&info, "%-12s%s [%s]\n", "host IP: ",
            nm_vect_str_ctx(&vm.ifs, NM_SQL_IF_NAME + idx_shift),
            nm_vect_str_ctx(&vm.ifs, NM_SQL_IF_IP4 + idx_shift));
    }

    if (status == NM_OK) {
        int fd;
        nm_str_t pid_path = NM_INIT_STR;

        nm_str_format(&pid_path, "%s/%s/%s",
            nm_cfg_get()->vm_dir.data, name->data, NM_VM_PID_FILE);

        if ((fd = open(pid_path.data, O_RDONLY)) != -1) {
            char pid[10];
            ssize_t nread;

            if ((nread = read(fd, pid, sizeof(pid))) > 0) {
                pid[nread - 1] = '\0';
                int pid_num = atoi(pid);
                nm_str_append_format(&info, "%-12s%d\n", "pid: ", pid_num);
#if defined (NM_OS_LINUX)
                struct timespec ts;
                memset(&ts, 0, sizeof(ts));
                ts.tv_nsec = 1e+8; // 0.1 s

                nm_stat_get_usage(pid_num);
                nanosleep(&ts, NULL);
                double usage = nm_stat_get_usage(pid_num);

                nm_str_append_format(&info, "%-12s%0.1f%%\n", "cpu usage: ", usage);
#endif
            }
            close(fd);
        }
        nm_str_free(&pid_path);
    }

    nm_vmctl_free_data(&vm);

    return info;
}

void nm_vmctl_free_data(nm_vmctl_data_t *vm)
{
    nm_vect_free(&vm->main, nm_str_vect_free_cb);
    nm_vect_free(&vm->ifs, nm_str_vect_free_cb);
    nm_vect_free(&vm->drives, nm_str_vect_free_cb);
    nm_vect_free(&vm->usb, nm_str_vect_free_cb);
}

void nm_vmctl_log_last(const nm_str_t *msg)
{
    FILE *fp;
    const nm_cfg_t *cfg = nm_cfg_get();

    if ((msg->len == 0) || (!cfg->log_enabled))
        return;

    if ((fp = fopen(cfg->log_path.data, "w+")) == NULL) {
        nm_bug(_("%s: cannot open file %s:%s"),
            __func__, cfg->log_path.data, strerror(errno));
    }

    fprintf(fp, "%s\n", msg->data);
    fclose(fp);
}

void nm_vmctl_clear_tap(const nm_str_t *name)
{
    nm_vect_t vm = NM_INIT_VECT;

    nm_vect_insert(&vm, name, sizeof(nm_str_t), nm_str_vect_ins_cb);
    (void) nm_vmctl_clear_tap_vect(&vm);

    nm_vect_free(&vm, nm_str_vect_free_cb);
}

void nm_vmctl_clear_all_tap(void)
{
    nm_vect_t vms = NM_INIT_VECT;
    nm_str_t query = NM_INIT_STR;
    int clear_done;

    nm_db_select("SELECT name FROM vms", &vms);
    clear_done = nm_vmctl_clear_tap_vect(&vms);

    nm_notify(clear_done ? _(NM_MSG_IFCLR_DONE) : _(NM_MSG_IFCLR_NONE));

    nm_str_free(&query);
    nm_vect_free(&vms, nm_str_vect_free_cb);
}

static int nm_vmctl_clear_tap_vect(const nm_vect_t *vms)
{
    nm_str_t lock_path = NM_INIT_STR;
    nm_str_t query = NM_INIT_STR;
    int clear_done = 0;

    for (size_t n = 0; n < vms->n_memb; n++) {
        struct stat file_info;
        size_t ifs_count;
        nm_vect_t ifaces = NM_INIT_VECT;

        nm_str_format(&lock_path, "%s/%s/%s",
            nm_cfg_get()->vm_dir.data, nm_vect_str(vms, n)->data, NM_VM_QMP_FILE);

        if (stat(lock_path.data, &file_info) == 0)
            continue;

        nm_str_format(&query, NM_VM_GET_IFACES_SQL, nm_vect_str_ctx(vms, n));
        nm_db_select(query.data, &ifaces);
        ifs_count = ifaces.n_memb / NM_IFS_IDX_COUNT;

        for (size_t ifn = 0; ifn < ifs_count; ifn++) {
            size_t idx_shift = NM_IFS_IDX_COUNT * ifn;
            if (nm_net_iface_exists(nm_vect_str(&ifaces, NM_SQL_IF_NAME + idx_shift)) == NM_OK) {
#if defined (NM_OS_LINUX)
                nm_net_del_iface(nm_vect_str(&ifaces, NM_SQL_IF_NAME + idx_shift));
#else
                nm_net_del_tap(nm_vect_str(&ifaces, NM_SQL_IF_NAME + idx_shift));
#endif
                clear_done = 1;
            }
        }

        nm_str_trunc(&lock_path, 0);
        nm_str_trunc(&query, 0);
        nm_vect_free(&ifaces, nm_str_vect_free_cb);
    }

    nm_str_free(&query);
    nm_str_free(&lock_path);

    return clear_done;
}

#if defined(NM_WITH_VNC_CLIENT) || defined(NM_WITH_SPICE)
static void nm_vmctl_gen_viewer(const nm_str_t *name, uint32_t port, nm_str_t *cmd, int type)
{
    const nm_cfg_t *cfg = nm_cfg_get();
    const nm_str_t *bin = (type == NM_VIEWER_SPICE) ? &cfg->spice_bin : &cfg->vnc_bin;
    const nm_str_t *args = (type == NM_VIEWER_SPICE) ? &cfg->spice_args : &cfg->vnc_args;
    const nm_view_args_t *pos = (type == NM_VIEWER_SPICE) ? &cfg->spice_view : &cfg->vnc_view;
    char *argsp = args->data;
    size_t total = 0;
    nm_str_t warn_msg = NM_INIT_STR;
    nm_str_t buf = NM_INIT_STR;

    nm_str_format(&warn_msg, _("%s viewer: port is not set"),
            (type == NM_VIEWER_SPICE) ? "spice" : "vnc");

    if (pos->port == -1) {
        nm_warn(warn_msg.data);
        goto out;
    }

    nm_str_append_format(cmd, "%s ", bin->data);

    if (pos->title != -1 && pos->title < pos->port) {
        nm_str_add_str_part(cmd, args, pos->title);
        nm_str_add_text(cmd, name->data);
        argsp += pos->title + 2;
        nm_str_add_text_part(cmd, argsp, pos->port - pos->title - 2);
        nm_str_append_format(cmd, "%u", port);
        total = pos->port;
    } else if (pos->title != -1 && pos->title > pos->port) {
        nm_str_add_str_part(cmd, args, pos->port);
        nm_str_append_format(cmd, "%u", port);
        argsp += pos->port + 2;
        nm_str_add_text_part(cmd, argsp, pos->title - pos->port - 2);
        nm_str_add_text(cmd, name->data);
        total = pos->title;
    } else {
        nm_str_add_str_part(cmd, args, pos->port);
        nm_str_append_format(cmd, "%u", port);
        total = pos->port;
    }

    if (total < (args->len - 2)) {
        argsp += args->len - total;
        nm_str_add_text_part(cmd, argsp, args->len - total - 2);
    }

    nm_str_append_format(cmd, " > /dev/null 2>&1 &");
    nm_debug("viewer cmd:\"%s\"\n", cmd->data);
out:
    nm_str_free(&buf);
    nm_str_free(&warn_msg);
}
#endif

/* vim:set ts=4 sw=4: */
