#include <nm_core.h>
#include <nm_utils.h>
#include <nm_string.h>
#include <nm_window.h>
#include <nm_network.h>
#include <nm_database.h>
#include <nm_cfg_file.h>
#include <nm_vm_control.h>

static void nm_vmctl_log_last(const nm_str_t *cmd);

void nm_vmctl_get_data(const nm_str_t *name, nm_vmctl_data_t *vm)
{
    nm_str_t query = NM_INIT_STR;

    nm_str_add_text(&query, "SELECT * FROM vms WHERE name='");
    nm_str_add_str(&query, name);
    nm_str_add_char(&query, '\'');
    nm_db_select(query.data, &vm->main);

    nm_str_trunc(&query, 0);
    nm_str_add_text(&query, "SELECT if_name, mac_addr, if_drv, ipv4_addr, vhost "
        "FROM ifaces WHERE vm_name='");
    nm_str_add_str(&query, name);
    nm_str_add_text(&query, "' ORDER BY if_name ASC");
    nm_db_select(query.data, &vm->ifs);

    nm_str_trunc(&query, 0);
    nm_str_add_text(&query, "SELECT drive_name, drive_drv, capacity, boot "
        "FROM drives WHERE vm_name='");
    nm_str_add_str(&query, name);
    nm_str_add_text(&query, "' ORDER BY drive_name ASC");
    nm_db_select(query.data, &vm->drives);

    nm_str_free(&query);
}

void nm_vmctl_start(const nm_str_t *name, int flags)
{
    nm_vmctl_data_t vm = NM_VMCTL_INIT_DATA;
    nm_str_t cmd = NM_INIT_STR;
    int unused __attribute__((unused));

    nm_vmctl_get_data(name, &vm);

    /* {{{ Check if VM is already installed */
    if (nm_str_cmp_st(nm_vect_str(&vm.main, NM_SQL_INST), NM_ENABLE) == NM_OK)
    {
        int ch = nm_print_warn(3, 6, _("Already installed (y/n)"));
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

    nm_vmctl_gen_cmd(&cmd, &vm, name, flags);
    if (cmd.len > 0)
        unused = system(cmd.data);

    nm_vmctl_log_last(&cmd);

    nm_str_free(&cmd);
    nm_vmctl_free_data(&vm);
}

void nm_vmctl_delete(const nm_str_t *name)
{
    nm_str_t vmdir = NM_INIT_STR;
    nm_str_t query = NM_INIT_STR;
    nm_vect_t drives = NM_INIT_VECT;
    nm_vect_t snaps = NM_INIT_VECT;
    size_t snaps_count;
    int delete_ok = 1;

    nm_str_alloc_str(&vmdir, &nm_cfg_get()->vm_dir);
    nm_str_add_char(&vmdir, '/');
    nm_str_add_str(&vmdir, name);
    nm_str_add_char(&vmdir, '/');

    nm_str_add_text(&query, "SELECT drive_name FROM drives WHERE vm_name='");
    nm_str_add_str(&query, name);
    nm_str_add_char(&query, '\'');
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

    /* delete snapshots */
    nm_str_add_text(&query, "SELECT backing_drive, snap_idx FROM snapshots WHERE vm_name='");
    nm_str_add_str(&query, name);
    nm_str_add_char(&query, '\'');
    nm_db_select(query.data, &snaps);
    nm_str_trunc(&query, 0);

    snaps_count = snaps.n_memb / 2;
    for (size_t n = 0; n < snaps_count; n++)
    {
        size_t idx_shift = 2 * n;
        nm_str_t snap_path = NM_INIT_STR;

        nm_str_alloc_str(&snap_path, &vmdir);
        nm_str_add_str(&snap_path, nm_vect_str(&snaps, 0 + idx_shift));
        nm_str_format(&snap_path, ".snap%s", nm_vect_str_ctx(&snaps, 1 + idx_shift));

        if (unlink(snap_path.data) == -1)
            delete_ok = 0;

        nm_str_free(&snap_path);
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

    nm_str_add_text(&query, "DELETE FROM drives WHERE vm_name='");
    nm_str_add_str(&query, name);
    nm_str_add_char(&query, '\'');
    nm_db_edit(query.data);
    nm_str_trunc(&query, 0);

    nm_str_add_text(&query, "DELETE FROM snapshots WHERE vm_name='");
    nm_str_add_str(&query, name);
    nm_str_add_char(&query, '\'');
    nm_db_edit(query.data);
    nm_str_trunc(&query, 0);

    nm_str_add_text(&query, "DELETE FROM ifaces WHERE vm_name='");
    nm_str_add_str(&query, name);
    nm_str_add_char(&query, '\'');
    nm_db_edit(query.data);
    nm_str_trunc(&query, 0);

    nm_str_add_text(&query, "DELETE FROM vms WHERE name='");
    nm_str_add_str(&query, name);
    nm_str_add_char(&query, '\'');
    nm_db_edit(query.data);

    if (!delete_ok)
        nm_print_warn(3, 6, _("Some files was not deleted!"));

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

    nm_str_alloc_str(&cmd, &nm_cfg_get()->vnc_bin);
    nm_str_add_text(&cmd, " :");
    nm_str_add_str(&cmd, nm_vect_str(&vm, 0));
    nm_str_add_text(&cmd, " > /dev/null 2>&1 &");

    unused = system(cmd.data);

    nm_vect_free(&vm, nm_str_vect_free_cb);
    nm_str_free(&query);
    nm_str_free(&cmd);
}
#endif

void nm_vmctl_gen_cmd(nm_str_t *res, const nm_vmctl_data_t *vm,
                      const nm_str_t *name, int flags)
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
        size_t srcp_len = nm_vect_str_len(&vm->main, NM_SQL_ISO);

        if ((srcp_len == 0) && (!(flags & NM_VMCTL_INFO)))
        {
            nm_print_warn(3, 6, _("ISO/IMG path is not set"));
            nm_str_trunc(res, 0);
            goto out;
        }
        if ((srcp_len > 4) &&
            (nm_str_cmp_tt(nm_vect_str_ctx(&vm->main, NM_SQL_ISO) + (srcp_len - 4),
                ".iso") == NM_OK))
        {
            if (flags & NM_VMCTL_LGCD) /* Use legacy cdrom command */
            {
                nm_str_add_text(res, " -boot d -cdrom ");
                nm_str_add_str(res, nm_vect_str(&vm->main, NM_SQL_ISO));
            }
            else
            {
                nm_str_add_text(res, " -boot d -drive file=");
                nm_str_add_str(res, nm_vect_str(&vm->main, NM_SQL_ISO));
                nm_str_add_text(res, ",media=cdrom,if=ide");
            }
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
        nm_str_t snap_name = NM_INIT_STR;
        nm_str_t snap_query = NM_INIT_STR;
        nm_vect_t snap_res = NM_INIT_VECT;
        const nm_str_t *drive_img = nm_vect_str(&vm->drives, NM_SQL_DRV_NAME + idx_shift);

        nm_str_format(&snap_query,
            "SELECT snap_idx FROM snapshots WHERE vm_name='%s' "
            "AND backing_drive='%s' AND active='1'",
            name->data, drive_img->data);

        nm_db_select(snap_query.data, &snap_res);

        nm_str_add_text(res, " -drive file=");
        nm_str_add_str(res, &vmdir);
        if (snap_res.n_memb == 0)
        {
            nm_str_add_str(res, drive_img);
        }
        else
        {
            nm_str_format(&snap_name, "%s.snap%s",
                drive_img->data, ((nm_str_t *)snap_res.data[0])->data);
            nm_str_add_str(res, &snap_name);
        }
        nm_str_add_text(res, ",media=disk,if=");
        nm_str_add_str(res, nm_vect_str(&vm->drives, NM_SQL_DRV_TYPE + idx_shift));

        nm_str_free(&snap_name);
        nm_str_free(&snap_query);
        nm_vect_free(&snap_res, nm_str_vect_free_cb);
    }

    nm_str_add_text(res, " -m ");
    nm_str_add_str(res, nm_vect_str(&vm->main, NM_SQL_MEM));

    if (nm_str_stoui(nm_vect_str(&vm->main, NM_SQL_SMP)) > 1)
    {
        nm_str_add_text(res, " -smp ");
        nm_str_add_str(res, nm_vect_str(&vm->main, NM_SQL_SMP));
    }

    if (nm_str_cmp_st(nm_vect_str(&vm->main, NM_SQL_KVM), NM_ENABLE) == NM_OK)
    {
        nm_str_add_text(res, " -enable-kvm");
        if (nm_str_cmp_st(nm_vect_str(&vm->main, NM_SQL_HCPU), NM_ENABLE) == NM_OK)
            nm_str_add_text(res, " -cpu host");
    }

    if (nm_str_cmp_st(nm_vect_str(&vm->main, NM_SQL_USBF), NM_ENABLE) == NM_OK)
    {
        nm_str_add_text(res, " -usb -usbdevice host:");
        nm_str_add_str(res, nm_vect_str(&vm->main, NM_SQL_USBD));
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
                nm_print_warn(3, 6, _("Socket is already used!"));
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
                nm_print_warn(3, 6, _("TTY is missing!"));
                nm_str_trunc(res, 0);
                goto out;
            }

            if (!isatty(fd))
            {
                close(fd);
                nm_print_warn(3, 6, _("TTY is not terminal!"));
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
        nm_str_format(res, ",netdev=netdev%zu -netdev tap,ifname=", n);
        nm_str_add_str(res, nm_vect_str(&vm->ifs, NM_SQL_IF_NAME + idx_shift));
        nm_str_format(res, ",script=no,downscript=no,id=netdev%zu", n);
        if (nm_str_cmp_st(nm_vect_str(&vm->ifs, NM_SQL_IF_VHO + idx_shift), NM_ENABLE) == NM_OK)
            nm_str_add_text(res, ",vhost=on");

#if defined (NM_OS_LINUX)
        if ((!(flags & NM_VMCTL_INFO)) &&
            (nm_vect_str_len(&vm->ifs, NM_SQL_IF_IP4 + idx_shift) != 0) &&
            (nm_net_iface_exists(nm_vect_str(&vm->ifs, NM_SQL_IF_NAME + idx_shift)) != NM_OK))
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

#if (NM_DEBUG)
    nm_debug("cmd=%s\n", res->data);
#endif
out:
    nm_str_free(&vmdir);
}

void nm_vmctl_free_data(nm_vmctl_data_t *vm)
{
    nm_vect_free(&vm->main, nm_str_vect_free_cb);
    nm_vect_free(&vm->ifs, nm_str_vect_free_cb);
    nm_vect_free(&vm->drives, nm_str_vect_free_cb);
}

static void nm_vmctl_log_last(const nm_str_t *cmd)
{
    FILE *fp;
    const nm_cfg_t *cfg = nm_cfg_get();

    if ((cmd->len == 0) || (!cfg->log_enabled))
        return;

    if ((fp = fopen(cfg->log_path.data, "w+")) == NULL)
    {
        nm_bug(_("%s: cannot open file %s:%s"),
            __func__, cfg->log_path.data, strerror(errno));
    }

    fprintf(fp, "%s\n", cmd->data);
    fclose(fp);
}

void nm_vmctl_clear_tap(void)
{
    nm_vect_t vms = NM_INIT_VECT;
    nm_str_t query = NM_INIT_STR;
    nm_str_t lock_path = NM_INIT_STR;

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

        nm_str_add_text(&query, "SELECT if_name, mac_addr, if_drv, ipv4_addr, vhost "
            "FROM ifaces WHERE vm_name='");
        nm_str_add_str(&query, nm_vect_str(&vms, n));
        nm_str_add_char(&query, '\'');

        nm_db_select(query.data, &ifaces);
        ifs_count = ifaces.n_memb / NM_IFS_IDX_COUNT;

        for (size_t ifn = 0; ifn < ifs_count; ifn++)
        {
            size_t idx_shift = NM_IFS_IDX_COUNT * ifn;
            if ((nm_vect_str_len(&ifaces, NM_SQL_IF_IP4 + idx_shift) != 0) &&
                (nm_net_iface_exists(nm_vect_str(&ifaces, NM_SQL_IF_NAME + idx_shift)) == NM_OK))
            {
                nm_net_del_tap(nm_vect_str(&ifaces, NM_SQL_IF_NAME + idx_shift));
            }
        }

        nm_str_trunc(&lock_path, 0);
        nm_str_trunc(&query, 0);
        nm_vect_free(&ifaces, nm_str_vect_free_cb);
    }

    nm_str_free(&query);
    nm_str_free(&lock_path);
    nm_vect_free(&vms, nm_str_vect_free_cb);
}

/* vim:set ts=4 sw=4 fdm=marker: */
