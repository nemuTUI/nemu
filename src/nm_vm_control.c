#include <nm_core.h>
#include <nm_utils.h>
#include <nm_string.h>
#include <nm_window.h>
#include <nm_network.h>
#include <nm_database.h>
#include <nm_cfg_file.h>
#include <nm_vm_control.h>
#include <nm_usb_devices.h>

#include <time.h>

#define NM_GET_VMSNAP_LOAD_SQL \
    "SELECT snap_name FROM vmsnapshots WHERE vm_name='%s' " \
    "AND load='1'"

#define NM_USB_UPDATE_STATE_SQL \
    "UPDATE vms SET usbid='%s' WHERE name='%s'"

void nm_vmctl_get_data(const nm_str_t *name, nm_vmctl_data_t *vm)
{
    nm_str_t query = NM_INIT_STR;

    nm_str_format(&query, NM_VM_GET_LIST_SQL, name->data);
    nm_db_select(query.data, &vm->main);

    nm_str_trunc(&query, 0);
    nm_str_format(&query, NM_VM_GET_IFACES_SQL, name->data);
    nm_db_select(query.data, &vm->ifs);

    nm_str_trunc(&query, 0);
    nm_str_format(&query, NM_VM_GET_DRIVES_SQL, name->data);
    nm_db_select(query.data, &vm->drives);

    nm_str_trunc(&query, 0);
    nm_str_format(&query, NM_USB_GET_SQL, name->data);
    nm_db_select(query.data, &vm->usb);

    nm_str_free(&query);
}

void nm_vmctl_start(const nm_str_t *name, int flags)
{
    nm_vmctl_data_t vm = NM_VMCTL_INIT_DATA;
    nm_vect_t tfds = NM_INIT_VECT;
    nm_str_t cmd = NM_INIT_STR;

    nm_vmctl_get_data(name, &vm);

    /* {{{ Check if VM is already installed */
    if (nm_str_cmp_st(nm_vect_str(&vm.main, NM_SQL_INST), NM_ENABLE) == NM_OK)
    {
        int ch = nm_notify(_(NM_MSG_INST_CONF));
        if (ch == 'y')
        {
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
    /* }}} VM is already installed */

    nm_vmctl_gen_cmd(&cmd, &vm, name, flags, &tfds);
    if (cmd.len > 0)
    {
        if (nm_spawn_process(&cmd, NULL) != NM_OK)
        {
            nm_str_t qmp_path = NM_INIT_STR;
            struct stat qmp_info;

            nm_str_format(&qmp_path, "%s/%s/%s",
                nm_cfg_get()->vm_dir.data, name->data, NM_VM_QMP_FILE);

            /* must delete qmp sock file if exists */
            if (stat(qmp_path.data, &qmp_info) != -1)
                unlink(qmp_path.data);

            nm_str_free(&qmp_path);

            nm_warn(_(NM_MSG_START_ERR));
        }
        else
        {
            nm_vmctl_log_last(&cmd);

            /* close all tap file descriptors */
            for (size_t n = 0; n < tfds.n_memb; n++)
                close(*((int *) tfds.data[n]));
        }
    }

    nm_str_free(&cmd);
    nm_vect_free(&tfds, NULL);
    nm_vmctl_free_data(&vm);
}

void nm_vmctl_delete(const nm_str_t *name)
{
    nm_str_t vmdir = NM_INIT_STR;
    nm_str_t query = NM_INIT_STR;
    nm_vect_t drives = NM_INIT_VECT;
    nm_vect_t snaps = NM_INIT_VECT;
    int delete_ok = 1;

    nm_str_alloc_str(&vmdir, &nm_cfg_get()->vm_dir);
    nm_str_add_char(&vmdir, '/');
    nm_str_add_str(&vmdir, name);
    nm_str_add_char(&vmdir, '/');

    nm_str_format(&query, NM_SELECT_DRIVE_NAMES_SQL, name->data);
    nm_db_select(query.data, &drives);
    nm_str_trunc(&query, 0);

    for (size_t n = 0; n < drives.n_memb; n++)
    {
        nm_str_t img_path = NM_INIT_STR;
        nm_str_alloc_str(&img_path, &vmdir);
        nm_str_add_str(&img_path, nm_vect_str(&drives, n));

        if (unlink(img_path.data) == -1)
            delete_ok = 0;

        nm_str_free(&img_path);
    }

    { /* delete pid file if exists */
        nm_str_t qmp_path = NM_INIT_STR;
        struct stat qmp_info;

        nm_str_copy(&qmp_path, &vmdir);
        nm_str_add_text(&qmp_path, NM_VM_PID_FILE);

        if (stat(qmp_path.data, &qmp_info) != -1)
        {
            if (unlink(qmp_path.data) == -1)
                delete_ok = 0;
        }

        nm_str_free(&qmp_path);
    }

    if (delete_ok)
    {
        if (rmdir(vmdir.data) == -1)
            delete_ok = 0;
    }

    nm_str_format(&query, NM_DEL_DRIVES_SQL, name->data);
    nm_db_edit(query.data);
    nm_str_trunc(&query, 0);

    nm_str_format(&query, NM_DEL_VMSNAP_SQL, name->data);
    nm_db_edit(query.data);
    nm_str_trunc(&query, 0);

    nm_str_format(&query, NM_DEL_IFS_SQL, name->data);
    nm_db_edit(query.data);
    nm_str_trunc(&query, 0);

    nm_str_format(&query, NM_DEL_USB_SQL, name->data);
    nm_db_edit(query.data);
    nm_str_trunc(&query, 0);

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

    nm_str_alloc_str(&pid_file, &nm_cfg_get()->vm_dir);
    nm_str_add_char(&pid_file, '/');
    nm_str_add_str(&pid_file, name);
    nm_str_add_text(&pid_file, "/" NM_VM_PID_FILE);

    if ((fd = open(pid_file.data, O_RDONLY)) == -1)
        return;

    if (read(fd, buf, sizeof(buf)) <= 0)
    {
         close(fd);
         return;
    }

    pid = atoi(buf);
    kill(pid, SIGTERM);

    close(fd);

    nm_str_free(&pid_file);
}

#if (NM_WITH_VNC_CLIENT)
void nm_vmctl_connect(const nm_str_t *name)
{
    nm_str_t cmd = NM_INIT_STR;
    nm_str_t query = NM_INIT_STR;
    nm_vect_t vm = NM_INIT_VECT;
    int unused __attribute__((unused));

    nm_str_add_text(&query, "SELECT vnc FROM vms WHERE name='");
    nm_str_add_str(&query, name);
    nm_str_add_char(&query, '\'');
    nm_db_select(query.data, &vm);

    nm_str_format(&cmd, "%s :%u > /dev/null 2>&1 &",
                  nm_cfg_get()->vnc_bin.data,
                  nm_str_stoui(nm_vect_str(&vm, 0), 10) + 5900);

    unused = system(cmd.data);

    nm_vect_free(&vm, nm_str_vect_free_cb);
    nm_str_free(&query);
    nm_str_free(&cmd);
}
#endif

void nm_vmctl_gen_cmd(nm_str_t *res, const nm_vmctl_data_t *vm,
                      const nm_str_t *name, int flags, nm_vect_t *tfds)
{
    nm_str_t vmdir = NM_INIT_STR;
    const nm_cfg_t *cfg = nm_cfg_get();
    size_t drives_count = vm->drives.n_memb / NM_DRV_IDX_COUNT;
    size_t ifs_count = vm->ifs.n_memb / NM_IFS_IDX_COUNT;

    nm_str_alloc_str(&vmdir, &cfg->vm_dir);
    nm_str_add_char(&vmdir, '/');
    nm_str_add_str(&vmdir, name);
    nm_str_add_char(&vmdir, '/');

    nm_str_add_text(res, NM_STRING(NM_USR_PREFIX) "/bin/qemu-system-");
    nm_str_add_str(res, nm_vect_str(&vm->main, NM_SQL_ARCH));

    nm_str_add_text(res,  " -daemonize");

    /* {{{ Setup install source */
    if (nm_str_cmp_st(nm_vect_str(&vm->main, NM_SQL_INST), NM_ENABLE) == NM_OK)
    {
        /* TODO use --blockdev */
        size_t srcp_len = nm_vect_str_len(&vm->main, NM_SQL_ISO);

        if ((srcp_len == 0) && (!(flags & NM_VMCTL_INFO)))
        {
            nm_warn(_(NM_MSG_ISO_MISS));
            nm_str_trunc(res, 0);
            goto out;
        }
        if ((srcp_len > 4) &&
            (nm_str_cmp_tt(nm_vect_str_ctx(&vm->main, NM_SQL_ISO) + (srcp_len - 4),
                ".iso") == NM_OK))
        {
            nm_str_add_text(res, " -boot d -cdrom ");
            nm_str_add_str(res, nm_vect_str(&vm->main, NM_SQL_ISO));
        }
        else
        {
            nm_str_add_text(res, " -drive file=");
            nm_str_add_str(res, nm_vect_str(&vm->main, NM_SQL_ISO));
            nm_str_add_text(res, ",media=disk,if=ide");
        }
    } /* }}} install */

    for (size_t n = 0; n < drives_count; n++)
    {
        size_t idx_shift = NM_DRV_IDX_COUNT * n;
        const nm_str_t *drive_img = nm_vect_str(&vm->drives, NM_SQL_DRV_NAME + idx_shift);
        const nm_str_t *blk_drv = nm_vect_str(&vm->drives, NM_SQL_DRV_TYPE + idx_shift);

        if (nm_str_cmp_st(blk_drv, "scsi-hd") == NM_OK)
            nm_str_format(res, " -device virtio-scsi-pci,id=scsi%zu", n);
        nm_str_format(res, " --blockdev file,node-name=f%zu,filename=", n);
        nm_str_add_str(res, &vmdir);
        nm_str_add_str(res, drive_img);
        nm_str_format(res, " --blockdev qcow2,node-name=q%zu,file=f%zu --device %s,drive=q%zu", n, n, blk_drv->data, n);
        if (nm_str_cmp_st(blk_drv, "scsi-hd") == NM_OK)
            nm_str_format(res, ",bus=scsi%zu.0", n);
    }

#ifdef NM_SAVEVM_SNAPSHOTS
    /* load vm snapshot if exists */
    {
        nm_str_t query = NM_INIT_STR;
        nm_vect_t snap_res = NM_INIT_VECT;

        nm_str_format(&query, NM_GET_VMSNAP_LOAD_SQL, name->data);
        nm_db_select(query.data, &snap_res);

        if (snap_res.n_memb > 0)
        {
            nm_str_format(res, " -loadvm %s", nm_vect_str_ctx(&snap_res, 0));

            /* reset load flag */
            nm_str_trunc(&query, 0);
            nm_str_format(&query, NM_RESET_LOAD_SQL, name->data);
            nm_db_edit(query.data);
        }

        nm_str_free(&query);
        nm_vect_free(&snap_res, nm_str_vect_free_cb);
    }
#endif /* NM_SAVEVM_SNAPSHOTS */

    nm_str_add_text(res, " -m ");
    nm_str_add_str(res, nm_vect_str(&vm->main, NM_SQL_MEM));

    if (nm_str_stoui(nm_vect_str(&vm->main, NM_SQL_SMP), 10) > 1)
    {
        nm_str_add_text(res, " -smp ");
        nm_str_add_str(res, nm_vect_str(&vm->main, NM_SQL_SMP));
    }

    /* 9p sharing.
     *
     * guest mount example:
     * mount -t 9p -o trans=virtio,version=9p2000.L hostshare /mnt/host
     */
    if (nm_str_cmp_st(nm_vect_str(&vm->main, NM_SQL_9FLG), NM_ENABLE) == NM_OK)
    {
        nm_str_add_text(res, " -fsdev local,security_model=none,id=fsdev0,path=");
        nm_str_add_str(res, nm_vect_str(&vm->main, NM_SQL_9PTH));
        nm_str_add_text(res, " -device virtio-9p-pci,fsdev=fsdev0,mount_tag=");
        nm_str_add_str(res, nm_vect_str(&vm->main, NM_SQL_9ID));
    }

    if (nm_str_cmp_st(nm_vect_str(&vm->main, NM_SQL_KVM), NM_ENABLE) == NM_OK)
    {
        nm_str_add_text(res, " -enable-kvm");
        if (nm_str_cmp_st(nm_vect_str(&vm->main, NM_SQL_HCPU), NM_ENABLE) == NM_OK)
            nm_str_add_text(res, " -cpu host");
    }

    /* Save info about usb subsystem status at boot time.
     * Needed for USB hotplug feature. */
    if (!(flags & NM_VMCTL_INFO))
    {
        nm_str_t query = NM_INIT_STR;
        nm_str_format(&query,
                      NM_USB_UPDATE_STATE_SQL,
                      nm_vect_str_ctx(&vm->main, NM_SQL_USBF),
                      name->data);
        nm_db_edit(query.data);
        nm_str_free(&query);
    }

    if (nm_str_cmp_st(nm_vect_str(&vm->main, NM_SQL_USBF), NM_ENABLE) == NM_OK)
    {
        size_t usb_count = vm->usb.n_memb / NM_USB_IDX_COUNT;
        nm_vect_t usb_list = NM_INIT_VECT;
        nm_vect_t serial_cache = NM_INIT_VECT;
        nm_str_t serial = NM_INIT_STR;

        if (nm_str_cmp_st(nm_vect_str(&vm->main, NM_SQL_USBT), NM_DEFAULT_USBVER) == NM_OK)
            nm_str_add_text(res, " -usb -device qemu-xhci");
        else
            nm_str_add_text(res, " -usb -device usb-ehci");

        if (usb_count > 0)
            nm_usb_get_devs(&usb_list);

        for (size_t n = 0; n < usb_count; n++)
        {
            int found_in_cache = 0;
            int found_in_devs = 0;
            size_t idx_shift = NM_USB_IDX_COUNT * n;
            nm_usb_dev_t *usb = NULL;

            const char *vid = nm_vect_str_ctx(&vm->usb, NM_SQL_USB_VID + idx_shift);
            const char *pid = nm_vect_str_ctx(&vm->usb, NM_SQL_USB_PID + idx_shift);
            const char *ser = nm_vect_str_ctx(&vm->usb, NM_SQL_USB_SERIAL + idx_shift);

            /* look for cached data first, libusb_open() is very expensive */
            for (size_t n = 0; n < serial_cache.n_memb; n++)
            {
                usb = nm_usb_data_dev(serial_cache.data[n]);
                nm_str_t *ser_str = &nm_usb_data_serial(serial_cache.data[n]);

                if ((nm_str_cmp_st(&nm_usb_vendor_id(usb), vid) == NM_OK) &&
                    (nm_str_cmp_st(&nm_usb_product_id(usb), pid) == NM_OK) &&
                    (nm_str_cmp_st(ser_str, ser) == NM_OK))
                {
                    found_in_cache = 1;
                    break;
                }
            }

            if (found_in_cache)
            {
                assert(usb != NULL);
                nm_str_format(res, " -device usb-host,hostbus=%d,hostaddr=%d,id=usb-%s-%s-%s",
                        nm_usb_bus_num(usb), nm_usb_dev_addr(usb),
                        nm_usb_vendor_id(usb).data, nm_usb_product_id(usb).data, ser);
                continue;
            }

            for (size_t n = 0; n < usb_list.n_memb; n++)
            {
                usb = (nm_usb_dev_t *) usb_list.data[n];
                if ((nm_str_cmp_st(&nm_usb_vendor_id(usb), vid) == NM_OK) &&
                    (nm_str_cmp_st(&nm_usb_product_id(usb), pid) == NM_OK))
                {
                    if (serial.len)
                        nm_str_trunc(&serial, 0);

                    nm_usb_get_serial(usb, &serial);
                    if (nm_str_cmp_st(&serial, ser) == NM_OK)
                    {
                        found_in_devs = 1;
                        break;
                    }
                    else
                    {
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

            if (found_in_devs)
            {
                assert(usb != NULL);
                nm_str_format(res, " -device usb-host,hostbus=%d,hostaddr=%d,id=usb-%s-%s-%s",
                        nm_usb_bus_num(usb), nm_usb_dev_addr(usb),
                        nm_usb_vendor_id(usb).data, nm_usb_product_id(usb).data, ser);
                continue;
            }
        }

        nm_vect_free(&serial_cache, nm_usb_data_vect_free_cb);
        nm_vect_free(&usb_list, nm_usb_vect_free_cb);
        nm_str_free(&serial);
    }

    if (nm_vect_str_len(&vm->main, NM_SQL_BIOS))
    {
        nm_str_add_text(res, " -bios ");
        nm_str_add_str(res, nm_vect_str(&vm->main, NM_SQL_BIOS));
    }

    if (nm_vect_str_len(&vm->main, NM_SQL_MACH))
    {
        nm_str_add_text(res, " -M ");
        nm_str_add_str(res, nm_vect_str(&vm->main, NM_SQL_MACH));
    }

    if (nm_vect_str_len(&vm->main, NM_SQL_KERN))
    {
        nm_str_add_text(res, " -kernel ");
        nm_str_add_str(res, nm_vect_str(&vm->main, NM_SQL_KERN));

        if (nm_vect_str_len(&vm->main, NM_SQL_KAPP))
        {
            nm_str_add_text(res, " -append \"");
            nm_str_add_str(res, nm_vect_str(&vm->main, NM_SQL_KAPP));
            nm_str_add_char(res, '"');
        }
    }

    if (nm_vect_str_len(&vm->main, NM_SQL_INIT))
    {
        nm_str_add_text(res, " -initrd ");
        nm_str_add_str(res, nm_vect_str(&vm->main, NM_SQL_INIT));
    }

    if (nm_str_cmp_st(nm_vect_str(&vm->main, NM_SQL_OVER), NM_ENABLE) == NM_OK)
        nm_str_add_text(res, " -usbdevice tablet");

    /* {{{ Setup serial socket */
    if (nm_vect_str_len(&vm->main, NM_SQL_SOCK))
    {
        if (!(flags & NM_VMCTL_INFO))
        {
            struct stat info;

            if (stat(nm_vect_str_ctx(&vm->main, NM_SQL_SOCK), &info) != -1)
            {
                nm_warn(_(NM_MSG_SOCK_USED));
                nm_str_trunc(res, 0);
                goto out;
            }
        }

        nm_str_add_text(res, " -chardev socket,path=");
        nm_str_add_str(res, nm_vect_str(&vm->main, NM_SQL_SOCK));
        nm_str_add_text(res, ",server,nowait,id=socket_");
        nm_str_add_str(res, name);
        nm_str_add_text(res, " -device isa-serial,chardev=socket_");
        nm_str_add_str(res, name);
    } /* }}} socket */

    /* {{{ Setup serial TTY */
    if (nm_vect_str_len(&vm->main, NM_SQL_TTY))
    {
        if (!(flags & NM_VMCTL_INFO))
        {
            int fd;

            if ((fd = open(nm_vect_str_ctx(&vm->main, NM_SQL_TTY), O_RDONLY)) == -1)
            {
                nm_warn(_(NM_MSG_TTY_MISS));
                nm_str_trunc(res, 0);
                goto out;
            }

            if (!isatty(fd))
            {
                close(fd);
                nm_warn(_(NM_MSG_TTY_INVAL));
                nm_str_trunc(res, 0);
                goto out;
            }
        }

        nm_str_add_text(res, " -chardev tty,path=");
        nm_str_add_str(res, nm_vect_str(&vm->main, NM_SQL_TTY));
        nm_str_add_text(res, ",id=tty_");
        nm_str_add_str(res, name);
        nm_str_add_text(res, " -device isa-serial,chardev=tty_");
        nm_str_add_str(res, name);
    } /* }}} TTY */

    /* {{{ setup network interfaces */
    for (size_t n = 0; n < ifs_count; n++)
    {
        size_t idx_shift = NM_IFS_IDX_COUNT * n;

        nm_str_add_text(res, " -device ");
        nm_str_add_str(res, nm_vect_str(&vm->ifs, NM_SQL_IF_DRV + idx_shift));
        nm_str_add_text(res, ",mac=");
        nm_str_add_str(res, nm_vect_str(&vm->ifs, NM_SQL_IF_MAC + idx_shift));
        if (nm_str_cmp_st(nm_vect_str(&vm->ifs, NM_SQL_IF_MVT + idx_shift),
            NM_DISABLE) == NM_OK)
        {
            nm_str_format(res, ",netdev=netdev%zu -netdev tap,ifname=", n);
            nm_str_add_str(res, nm_vect_str(&vm->ifs, NM_SQL_IF_NAME + idx_shift));
            nm_str_format(res, ",script=no,downscript=no,id=netdev%zu", n);
#if defined (NM_OS_LINUX)
            /* Delete macvtap iface if exists, we using simple tap iface now */
            if (!(flags & NM_VMCTL_INFO))
            {
                uint32_t tap_idx = 0;
                tap_idx = nm_net_iface_idx(nm_vect_str(&vm->ifs,
                    NM_SQL_IF_NAME + idx_shift));

                if (tap_idx != 0)
                {
                    /* is this iface macvtap? */
                    struct stat tap_info;
                    nm_str_t tap_path = NM_INIT_STR;

                    nm_str_format(&tap_path, "/dev/tap%u", tap_idx);
                    if (stat(tap_path.data, &tap_info) == 0)
                    {
                        /* iface is macvtap, delete it */
                        nm_net_del_iface(nm_vect_str(&vm->ifs,
                                                     NM_SQL_IF_NAME + idx_shift));
                    }
                    nm_str_free(&tap_path);
                }
            }
#endif /* NM_OS_LINUX */
        }
        else
        {
#if defined (NM_OS_LINUX)
            int tap_fd = 0, wait_perm = 0;

            if (!(flags & NM_VMCTL_INFO))
            {
                nm_str_t tap_path = NM_INIT_STR;
                uint32_t tap_idx = 0;

                /* Delete simple tap iface if exists, we using macvtap iface now */
                if ((tap_idx = nm_net_iface_idx(nm_vect_str(&vm->ifs,
                        NM_SQL_IF_NAME + idx_shift))) != 0)
                {
                    /* is this iface simple tap? */
                    struct stat tap_info;
                    nm_str_format(&tap_path, "/dev/tap%u", tap_idx);
                    if (stat(tap_path.data, &tap_info) != 0)
                    {
                        /* iface is simple tap, delete it */
                        nm_net_del_tap(nm_vect_str(&vm->ifs,
                                                   NM_SQL_IF_NAME + idx_shift));
                    }

                    nm_str_trunc(&tap_path, 0);
                    tap_idx = 0;
                }

                if (nm_net_iface_exists(nm_vect_str(&vm->ifs, NM_SQL_IF_NAME + idx_shift)) != NM_OK)
                {
                    wait_perm = 1;
                    int macvtap_type = nm_str_stoui(nm_vect_str(&vm->ifs,
                                                                NM_SQL_IF_MVT + idx_shift), 10);

                    /* check for lower iface (parent) exists */
                    if (nm_vect_str_len(&vm->ifs, NM_SQL_IF_PET + idx_shift) == 0)
                    {
                        nm_warn(_(NM_MSG_MTAP_NSET));
                        nm_str_trunc(res, 0);
                        goto out;
                    }

                    nm_net_add_macvtap(nm_vect_str(&vm->ifs, NM_SQL_IF_NAME + idx_shift),
                                       nm_vect_str(&vm->ifs, NM_SQL_IF_PET + idx_shift),
                                       nm_vect_str(&vm->ifs, NM_SQL_IF_MAC + idx_shift),
                                       macvtap_type);
                }

                tap_idx = nm_net_iface_idx(nm_vect_str(&vm->ifs,
                    NM_SQL_IF_NAME + idx_shift));
                if (tap_idx == 0)
                    nm_bug("%s: MacVTap interface not found", __func__);

                nm_str_format(&tap_path, "/dev/tap%u", tap_idx);

                /* wait for udev fixes permitions on /dev/tapN */
                if ((getuid() != 0) && wait_perm)
                {
                    struct timespec ts;
                    int tap_rw_ok = 0;

                    memset(&ts, 0, sizeof(ts));
                    ts.tv_nsec = 5e+7; /* 0.05sec */

                    for (int n = 0; n < 40; n++)
                    {
                        if (access(tap_path.data, R_OK | W_OK) == 0)
                        {
                            tap_rw_ok = 1;
                            break;
                        }
                        nanosleep(&ts, NULL);
                    }
                    if (!tap_rw_ok)
                    {
                        nm_warn(_(NM_MSG_TAP_EACC));
                        nm_str_trunc(res, 0);
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

            nm_str_format(res, ",netdev=netdev%zu -netdev tap,", n);
            nm_str_format(res, "id=netdev%zu,fd=%d", n, (flags & NM_VMCTL_INFO) ? -1 : tap_fd);
#endif /* NM_OS_LINUX */
        }
        if (nm_str_cmp_st(nm_vect_str(&vm->ifs, NM_SQL_IF_VHO + idx_shift), NM_ENABLE) == NM_OK)
            nm_str_add_text(res, ",vhost=on");

#if defined (NM_OS_LINUX)
        if ((!(flags & NM_VMCTL_INFO)) &&
            (nm_vect_str_len(&vm->ifs, NM_SQL_IF_IP4 + idx_shift) != 0) &&
            (nm_net_iface_exists(nm_vect_str(&vm->ifs, NM_SQL_IF_NAME + idx_shift)) != NM_OK) &&
            (nm_str_cmp_st(nm_vect_str(&vm->ifs, NM_SQL_IF_MVT + idx_shift), NM_DISABLE) == NM_OK))
        {
            nm_net_add_tap(nm_vect_str(&vm->ifs, NM_SQL_IF_NAME + idx_shift));
            nm_net_set_ipaddr(nm_vect_str(&vm->ifs, NM_SQL_IF_NAME + idx_shift),
                              nm_vect_str(&vm->ifs, NM_SQL_IF_IP4 + idx_shift));
        }
#elif defined (NM_OS_FREEBSD)
        if (nm_net_iface_exists(nm_vect_str(&vm->ifs, NM_SQL_IF_NAME + idx_shift)) == NM_OK)
        {
            nm_net_del_tap(nm_vect_str(&vm->ifs, NM_SQL_IF_NAME + idx_shift));
        }
        (void) tfds;
#endif /* NM_OS_LINUX */
    } /* }}} ifaces */

    if (flags & NM_VMCTL_TEMP)
        nm_str_add_text(res, " -snapshot");

    nm_str_add_text(res, " -pidfile ");
    nm_str_add_str(res, &vmdir);
    nm_str_add_text(res, NM_VM_PID_FILE);

    nm_str_format(res, " -qmp unix:%s%s,server,nowait", vmdir.data, NM_VM_QMP_FILE);

    if (cfg->vnc_listen_any)
        nm_str_add_text(res, " -vnc :");
    else
        nm_str_add_text(res, " -vnc 127.0.0.1:");

    nm_str_add_str(res, nm_vect_str(&vm->main, NM_SQL_VNC));

    nm_debug("cmd=%s\n", res->data);
out:
    nm_str_free(&vmdir);
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

    if ((fp = fopen(cfg->log_path.data, "w+")) == NULL)
    {
        nm_bug(_("%s: cannot open file %s:%s"),
            __func__, cfg->log_path.data, strerror(errno));
    }

    fprintf(fp, "%s\n", msg->data);
    fclose(fp);
}

void nm_vmctl_clear_tap(void)
{
    nm_vect_t vms = NM_INIT_VECT;
    nm_str_t query = NM_INIT_STR;
    nm_str_t lock_path = NM_INIT_STR;
    int clear_done = 0;

    nm_db_select("SELECT name FROM vms", &vms);

    for (size_t n = 0; n < vms.n_memb; n++)
    {
        struct stat file_info;
        size_t ifs_count;
        nm_vect_t ifaces = NM_INIT_VECT;

        nm_str_alloc_str(&lock_path, &nm_cfg_get()->vm_dir);
        nm_str_add_char(&lock_path, '/');
        nm_str_add_str(&lock_path, nm_vect_str(&vms, n));
        nm_str_add_text(&lock_path, "/" NM_VM_QMP_FILE);

        if (stat(lock_path.data, &file_info) == 0)
            continue;

        nm_str_add_text(&query, "SELECT if_name, mac_addr, if_drv, ipv4_addr, vhost, "
            "macvtap, parent_eth FROM ifaces WHERE vm_name='");
        nm_str_add_str(&query, nm_vect_str(&vms, n));
        nm_str_add_char(&query, '\'');

        nm_db_select(query.data, &ifaces);
        ifs_count = ifaces.n_memb / NM_IFS_IDX_COUNT;

        for (size_t ifn = 0; ifn < ifs_count; ifn++)
        {
            size_t idx_shift = NM_IFS_IDX_COUNT * ifn;
            if (nm_net_iface_exists(nm_vect_str(&ifaces, NM_SQL_IF_NAME + idx_shift)) == NM_OK)
            {
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

    nm_notify(clear_done ? _(NM_MSG_IFCLR_DONE) : _(NM_MSG_IFCLR_NONE));

    nm_str_free(&query);
    nm_str_free(&lock_path);
    nm_vect_free(&vms, nm_str_vect_free_cb);
}

/* vim:set ts=4 sw=4 fdm=marker: */
