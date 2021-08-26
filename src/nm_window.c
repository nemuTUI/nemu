#include <nm_core.h>
#include <nm_form.h>
#include <nm_utils.h>
#include <nm_string.h>
#include <nm_window.h>
#include <nm_ncurses.h>
#include <nm_database.h>
#include <nm_cfg_file.h>
#include <nm_usb_plug.h>
#include <nm_stat_usage.h>

static float nm_window_scale = 0.7;

static void nm_init_window__(nm_window_t *w, const char *msg);
static void nm_print_help_lines(const char **msg, size_t objs, int err);
static void nm_print_help__(const char **keys, const char **values,
                            size_t hotkey_num, size_t maxlen);

#if defined (NM_OS_LINUX)
  #if defined (NM_WITH_OVF_SUPPORT)
    #define NM_HELP_MSG \
      "q:Quit", "I:Install VM", "O:Import OVA", "A:Import image", "N:Network", "?:Help"
  #else
    #define NM_HELP_MSG \
      "q:Quit", "I:Install VM", "A:Import image", "N:Network", "?:Help"
  #endif
#else
  #if defined (NM_WITH_OVF_SUPPORT)
    #define NM_HELP_MSG \
      "q:Quit", "I:Install VM", "O:Import OVA", "A:Import image", "?:Help"
  #else
    #define NM_HELP_MSG \
      "q:Quit", "I:Install VM", "A:Import image", "?:Help"
  #endif
#endif

#if defined (NM_WITH_NETWORK_MAP)
  #define NM_LAN_MSG \
    "q:Back", "e:Export SVG map", "?:Help"
#else
  #define NM_LAN_MSG \
    "q:Back", "?:Help"
#endif

#define X_NM_HELP_GEN                         \
    X(main, NM_HELP_MSG)                      \
    X(lan, NM_LAN_MSG)                        \
    X(edit, "esc:Cancel", "enter:Save")       \
    X(import, "esc:Cancel", "enter:Import")   \
    X(install, "esc:Cancel", "enter:Install") \
    X(iface, "q:Back", "enter:Edit")          \
    X(clone, "esc:Cancel", "enter:Clone")     \
    X(export, "esc:Cancel", "enter:Export")   \
    X(delete, "q:Back", "enter:Delete")

#define X(name, ...)                                         \
    void nm_init_help_ ## name(void) {                       \
        const char *msg[] = { __VA_ARGS__ };                 \
        nm_print_help_lines(msg, nm_arr_len(msg), NM_FALSE); \
    }
X_NM_HELP_GEN
#undef X

void nm_create_windows(void)
{
    int action_cols, screen_x, screen_y;
    nm_cord_t help_size;
    nm_cord_t side_size;
    nm_cord_t action_size;

    getmaxyx(stdscr, screen_y, screen_x);
    action_cols = screen_x * nm_window_scale;

    help_size = NM_SET_POS(1, screen_x, 0, 0);
    side_size = NM_SET_POS(screen_y - 1, screen_x - action_cols, 0, 1);
    action_size = NM_SET_POS(screen_y - 1, action_cols, screen_x - action_cols, 1);

    help_window = nm_init_window(&help_size);
    side_window = nm_init_window(&side_size);
    action_window = nm_init_window(&action_size);

    help_panel = new_panel(help_window);
    side_panel = new_panel(side_window);
    action_panel = new_panel(action_window);
}

void nm_init_help(const char *msg, int err)
{
    nm_print_help_lines(&msg, 1, err);
}

static void nm_print_help_lines(const char **msg, size_t objs, int err)
{
    if (!msg)
        return;

    int x = 1, y = 0;

    wbkgd(help_window, COLOR_PAIR(err ? NM_COLOR_RED : NM_COLOR_BLACK));

    for (size_t n = 0; n < objs; n++) {
        if (n > 0) {
            x++;
            mvwaddch(help_window, y, x, ACS_VLINE);
            x+=2;
        }

        mvwprintw(help_window, y, x, _(msg[n]));
        x += mbstowcs(NULL, _(msg[n]), strlen(_(msg[n])));
    }

    wrefresh(help_window);
}

void nm_init_side(void)
{
    nm_str_t title = NM_INIT_STR;

    wattroff(side_window, COLOR_PAIR(NM_COLOR_HIGHLIGHT));

    if (nm_filter.type == NM_FILTER_GROUP) {
        nm_str_format(&title, "VM list [%s]", nm_filter.query.data);
        nm_init_window__(side_window, _(title.data));
        nm_str_free(&title);
    } else {
        nm_init_window__(side_window, _("VM list"));
    }

    wtimeout(side_window, 500);
}

void nm_init_side_lan(void)
{
    wattroff(side_window, COLOR_PAIR(NM_COLOR_HIGHLIGHT));
    nm_init_window__(side_window, _("veth list"));
    wtimeout(side_window, -1);
}

void nm_init_side_if_list(void)
{
    wattroff(side_window, COLOR_PAIR(NM_COLOR_HIGHLIGHT));
    nm_init_window__(side_window, _("Iface list"));
    wtimeout(side_window, -1);
}

void nm_init_side_drives(void)
{
    wattroff(side_window, COLOR_PAIR(NM_COLOR_HIGHLIGHT));
    nm_init_window__(side_window, _("Drive list"));
    wtimeout(side_window, -1);
}

void nm_init_action(const char *msg)
{
    nm_init_window__(action_window, msg ? msg : _("Properties"));
}

static void nm_init_window__(nm_window_t *w, const char *msg)
{
    int cols = getmaxx(w);
    size_t mb_len = mbstowcs(NULL, msg, strlen(msg));

    box(w, 0, 0);
    mvwprintw(w, 1, (cols - mb_len) / 2, msg);
    mvwaddch(w, 2, 0, ACS_LTEE);
    mvwhline(w, 2, 1, ACS_HLINE, cols - 2);
    mvwaddch(w, 2, cols - 1, ACS_RTEE);
    wrefresh(w);
}

void nm_destroy_windows(void)
{
    del_panel(help_panel);
    del_panel(side_panel);
    del_panel(action_panel);

    delwin(help_window);
    delwin(side_window);
    delwin(action_window);

    help_window = NULL;
    side_window = NULL;
    action_window = NULL;
}

void nm_print_cmd(const nm_str_t *name)
{
    nm_str_t buf = NM_INIT_STR;
    nm_vect_t argv = NM_INIT_VECT;
    nm_vect_t res = NM_INIT_VECT;
    nm_vmctl_data_t vm = NM_VMCTL_INIT_DATA;
    int off, start = 3, flags = 0;

    int col = getmaxx(stdscr);
    flags |= NM_VMCTL_INFO;

    nm_vmctl_get_data(name, &vm);

    nm_vmctl_gen_cmd(&argv, &vm, name, &flags, NULL, NULL);

    /* pre checking */
    for (size_t n = 0; n < argv.n_memb; n++) {
        off = strlen((char *) nm_vect_at(&argv, n));
        off += 2; /* count ' \' */

        if (off >= col) {
            nm_str_format(&buf, "%s", _("window to small"));
            nm_clear_screen();
            mvprintw(1, (col - name->len) / 2, "%s", name->data);
            mvprintw(3, 0, "%s", buf.data);
            goto out;
        }
    }

    for (size_t n = 0; n < argv.n_memb; n++) {
        const char *arg = nm_vect_at(&argv, n);
        off = buf.len;
        off += strlen(arg);
        off += 2;

        if (off > col) {
            nm_str_t tmp = NM_INIT_STR;
            nm_str_append_format(&tmp, "%s\\", buf.data);
            nm_vect_insert_cstr(&res, tmp.data);
            nm_str_trunc(&buf, 0);
            if (n + 1 != argv.n_memb) {
                nm_str_append_format(&buf, "%s ", arg);
            } else {
                nm_vect_insert_cstr(&res, arg);
            }
            nm_str_free(&tmp);
        } else if (off <= col && n + 1 < argv.n_memb) {
            nm_str_append_format(&buf, "%s ", arg);
        } else {
            nm_str_append_format(&buf, "%s ", arg);
            nm_vect_insert_cstr(&res, buf.data);
        }
    }

    nm_clear_screen();
    mvprintw(1, (col - name->len) / 2, "%s", name->data);

    for (size_t n = 0; n < res.n_memb; n++) {
        mvprintw(start + n, 0, "%s", (char *) nm_vect_at(&res, n));
    }

out:
    nm_str_free(&buf);
    nm_vect_free(&argv, NULL);
    nm_vect_free(&res, NULL);
    nm_vmctl_free_data(&vm);

    refresh();
    getch();
}

void nm_print_snapshots(const nm_vect_t *v)
{
    nm_str_t buf = NM_INIT_STR;
    size_t count = v->n_memb / 5;
    size_t y = 7, x = 2;
    size_t cols, rows;
    chtype ch1, ch2;
    ch1 = ch2 = 0;

    getmaxyx(action_window, rows, cols);

    enum {
        NM_SQL_VMSNAP_NAME = 2,
        NM_SQL_VMSNAP_TIME = 4
    };

    for (size_t n = 0; n < count; n++) {
        size_t idx_shift = 5 * n;

        if (n && n < count) {
            ch1 = (n != (count - 1)) ? ACS_LTEE : ACS_LLCORNER;
            ch2 = ACS_HLINE;
        }

        nm_str_format(&buf, "%s (%s)",
                nm_vect_str_ctx(v, NM_SQL_VMSNAP_NAME + idx_shift),
                nm_vect_str_ctx(v, NM_SQL_VMSNAP_TIME + idx_shift));
        NM_PR_VM_INFO();
    }

    nm_str_free(&buf);
}

void nm_print_drive_info(const nm_vect_t *v, size_t idx)
{
    if (!idx)
        return;

    nm_str_t buf = NM_INIT_STR;
    size_t y = 3, x = 2;
    size_t cols, rows;
    size_t idx_shift;
    chtype ch1, ch2;
    ch1 = ch2 = 0;

    idx_shift = 2 * (--idx);

    getmaxyx(action_window, rows, cols);

    nm_str_format(&buf, "%-12s%sGb", "capacity: ",
            nm_vect_str_ctx(v, 1 + idx_shift));
    NM_PR_VM_INFO();

    nm_str_free(&buf);
}

void nm_print_iface_info(const nm_vmctl_data_t *vm, size_t idx)
{
    if (!idx)
        return;

    nm_str_t buf = NM_INIT_STR;
    size_t y = 3, x = 2;
    size_t cols, rows;
    size_t idx_shift, mvtap_idx = 0;
    chtype ch1, ch2;
    ch1 = ch2 = 0;

    idx_shift = NM_IFS_IDX_COUNT * (--idx);

    getmaxyx(action_window, rows, cols);

    if (nm_str_cmp_st(nm_vect_str(&vm->ifs, NM_SQL_IF_USR + idx_shift),
                NM_ENABLE) == NM_OK) {
        nm_str_format(&buf, "%-12s%s", "User mode: ", "enabled");
        NM_PR_VM_INFO();

        if (nm_vect_str_len(&vm->ifs, NM_SQL_IF_FWD + idx_shift) != 0) {
            nm_str_format(&buf, "%-12s%s", "hostfwd: ",
                    nm_vect_str_ctx(&vm->ifs, NM_SQL_IF_FWD + idx_shift));
            NM_PR_VM_INFO();
        }
        if (nm_vect_str_len(&vm->ifs, NM_SQL_IF_SMB + idx_shift) != 0) {
            nm_str_format(&buf, "%-12s%s", "smb: ",
                    nm_vect_str_ctx(&vm->ifs, NM_SQL_IF_SMB + idx_shift));
            NM_PR_VM_INFO();
        }

        goto out;
    }

    nm_str_format(&buf, "%-12s%s", "hwaddr: ",
            nm_vect_str_ctx(&vm->ifs, NM_SQL_IF_MAC + idx_shift));
    NM_PR_VM_INFO();

    nm_str_format(&buf, "%-12s%s", "driver: ",
            nm_vect_str_ctx(&vm->ifs, NM_SQL_IF_DRV + idx_shift));
    NM_PR_VM_INFO();

    if (nm_vect_str_len(&vm->ifs, NM_SQL_IF_IP4 + idx_shift) > 0) {
        nm_str_format(&buf, "%-12s%s", "host addr: ",
                nm_vect_str_ctx(&vm->ifs, NM_SQL_IF_IP4 + idx_shift));
        NM_PR_VM_INFO();
    }

    nm_str_format(&buf, "%-12s%s", "vhost: ",
            (nm_str_cmp_st(nm_vect_str(&vm->ifs, NM_SQL_IF_VHO + idx_shift),
                           NM_ENABLE) == NM_OK) ? "yes" : "no");
    NM_PR_VM_INFO();

    mvtap_idx = nm_str_stoui(nm_vect_str(&vm->ifs, NM_SQL_IF_MVT + idx_shift), 10);
    if (!mvtap_idx)
        nm_str_format(&buf, "%-12s%s", "MacVTap: ", nm_form_macvtap[mvtap_idx]);
    else
        nm_str_format(&buf, "%-12s%s [iface: %s]", "MacVTap: ", nm_form_macvtap[mvtap_idx],
            nm_vect_str_ctx(&vm->ifs, NM_SQL_IF_PET + idx_shift));
    NM_PR_VM_INFO();
out:
    nm_str_free(&buf);
}

void nm_print_vm_info(const nm_str_t *name, const nm_vmctl_data_t *vm, int status)
{
    nm_str_t buf = NM_INIT_STR;
    size_t y = 3, x = 2;
    size_t cols, rows;
    size_t ifs_count, drives_count;
    chtype ch1, ch2;
    nm_cpu_t cpu = NM_INIT_CPU;
    ch1 = ch2 = 0;

    static const nm_str_t *name_ = NULL;
    static const nm_vmctl_data_t *vm_ = NULL;
    static int status_ = 0;

    if (name && vm) {
        name_ = name;
        vm_ = vm;
        status_ = status;
    }

    if (!name_ || !vm_)
        return;

    getmaxyx(action_window, rows, cols);

    nm_str_format(&buf, "%-12s%s", "arch: ",
        nm_vect_str_ctx(&vm_->main, NM_SQL_ARCH));
    NM_PR_VM_INFO();

    nm_parse_smp(&cpu, nm_vect_str_ctx(&vm_->main, NM_SQL_SMP));
    nm_str_format(&buf, "%-12s%zu %s (%zu %s), threads %zu", "cpu: ",
            (cpu.sockets) ? cpu.sockets : cpu.smp,
            (cpu.sockets > 1) ? "cpus" : "cpu",
            (cpu.cores) ? cpu.cores : 1,
            (cpu.cores > 1) ? "cores" : "core",
            cpu.smp);
    NM_PR_VM_INFO();

    nm_str_format(&buf, "%-12s%s %s", "memory: ",
        nm_vect_str_ctx(&vm_->main, NM_SQL_MEM), "Mb");
    NM_PR_VM_INFO();

    if (nm_str_cmp_st(nm_vect_str(&vm_->main, NM_SQL_KVM), NM_ENABLE) == NM_OK) {
        if (nm_str_cmp_st(nm_vect_str(&vm_->main, NM_SQL_HCPU), NM_ENABLE) == NM_OK)
            nm_str_format(&buf, "%-12s%s", "kvm: ", "enabled [+hostcpu]");
        else
            nm_str_format(&buf, "%-12s%s", "kvm: ", "enabled");
    } else {
        nm_str_format(&buf, "%-12s%s", "kvm: ", "disabled");
    }
    NM_PR_VM_INFO();

    if (nm_str_cmp_st(nm_vect_str(&vm_->main, NM_SQL_USBF), "1") == NM_OK) {
        nm_str_format(&buf, "%-12s%s [%s]", "usb: ", "enabled",
                nm_vect_str_ctx(&vm_->main, NM_SQL_USBT));
    } else {
        nm_str_format(&buf, "%-12s%s", "usb: ", "disabled");
    }
    NM_PR_VM_INFO();

    {
        nm_vect_t usb_names = NM_INIT_VECT;

        nm_usb_unplug_list(&vm_->usb, &usb_names, false);

        for (size_t n = 0; n < usb_names.n_memb; n++) {
            ch1 = (n != (usb_names.n_memb - 1)) ? ACS_LTEE : ACS_LLCORNER;
            ch2 = ACS_HLINE;
            nm_str_format(&buf, "%s", (char *) usb_names.data[n]);
            NM_PR_VM_INFO();
        }

        nm_vect_free(&usb_names, NULL);
        ch1 = ch2 = 0;
    }

    nm_str_format(&buf, "%-12s%s [%u]", "vnc port: ",
             nm_vect_str_ctx(&vm_->main, NM_SQL_VNC),
             nm_str_stoui(nm_vect_str(&vm_->main, NM_SQL_VNC), 10) + NM_STARTING_VNC_PORT);
    NM_PR_VM_INFO();

    /* print network interfaces info */
    ifs_count = vm_->ifs.n_memb / NM_IFS_IDX_COUNT;

    for (size_t n = 0; n < ifs_count; n++) {
        size_t idx_shift = NM_IFS_IDX_COUNT * n;
        if (nm_str_cmp_st(nm_vect_str(&vm_->ifs, NM_SQL_IF_USR + idx_shift),
                    NM_ENABLE) == NM_OK) {
            nm_str_format(&buf, "eth%zu%-8s%s [user mode]",
                    n, ":",
                    nm_vect_str_ctx(&vm_->ifs, NM_SQL_IF_NAME + idx_shift));
        } else {
            nm_str_format(&buf, "eth%zu%-8s%s [%s %s%s]",
                    n, ":",
                    nm_vect_str_ctx(&vm_->ifs, NM_SQL_IF_NAME + idx_shift),
                    nm_vect_str_ctx(&vm_->ifs, NM_SQL_IF_MAC + idx_shift),
                    nm_vect_str_ctx(&vm_->ifs, NM_SQL_IF_DRV + idx_shift),
                    (nm_str_cmp_st(nm_vect_str(&vm_->ifs, NM_SQL_IF_VHO + idx_shift),
                                   NM_ENABLE) == NM_OK) ? "+vhost" : "");
        }

        NM_PR_VM_INFO();
    }

    /* print drives info */
    drives_count = vm_->drives.n_memb / NM_DRV_IDX_COUNT;

    for (size_t n = 0; n < drives_count; n++) {
        size_t idx_shift = NM_DRV_IDX_COUNT * n;
        int boot = 0;

        if (nm_str_cmp_st(nm_vect_str(&vm_->drives, NM_SQL_DRV_BOOT + idx_shift),
                NM_ENABLE) == NM_OK) {
            boot = 1;
        }

        nm_str_format(&buf, "disk%zu%-7s%s [%sGb %s discard=%s] %s", n, ":",
                 nm_vect_str_ctx(&vm_->drives, NM_SQL_DRV_NAME + idx_shift),
                 nm_vect_str_ctx(&vm_->drives, NM_SQL_DRV_SIZE + idx_shift),
                 nm_vect_str_ctx(&vm_->drives, NM_SQL_DRV_TYPE + idx_shift),
                 (nm_str_cmp_st(nm_vect_str(&vm_->drives, NM_SQL_DRV_DISC + idx_shift),
                                NM_ENABLE) == NM_OK) ? "on" : "off",
                 boot ? "*" : "");
        NM_PR_VM_INFO();
    }

    /* print 9pfs info */
    if (nm_str_cmp_st(nm_vect_str(&vm_->main, NM_SQL_9FLG), "1") == NM_OK) {
        nm_str_format(&buf, "%-12s%s [%s]", "9pfs: ",
                 nm_vect_str_ctx(&vm_->main, NM_SQL_9PTH),
                 nm_vect_str_ctx(&vm_->main, NM_SQL_9ID));
        NM_PR_VM_INFO();
    }

    /* generate guest boot settings info */
    if (nm_vect_str_len(&vm_->main, NM_SQL_MACH)) {
        nm_str_format(&buf, "%-12s%s", "machine: ",
                nm_vect_str_ctx(&vm_->main, NM_SQL_MACH));
        NM_PR_VM_INFO();
    }
    if (nm_vect_str_len(&vm_->main, NM_SQL_BIOS)) {
        nm_str_format(&buf,"%-12s%s", "bios: ",
                nm_vect_str_ctx(&vm_->main, NM_SQL_BIOS));
        NM_PR_VM_INFO();
    }
    if (nm_vect_str_len(&vm_->main, NM_SQL_KERN)) {
        nm_str_format(&buf,"%-12s%s", "kernel: ",
                nm_vect_str_ctx(&vm_->main, NM_SQL_KERN));
        NM_PR_VM_INFO();
    }
    if (nm_vect_str_len(&vm_->main, NM_SQL_KAPP)) {
        nm_str_format(&buf, "%-12s%s", "cmdline: ",
                nm_vect_str_ctx(&vm_->main, NM_SQL_KAPP));
        NM_PR_VM_INFO();
    }
    if (nm_vect_str_len(&vm_->main, NM_SQL_INIT)) {
        nm_str_format(&buf, "%-12s%s", "initrd: ",
                nm_vect_str_ctx(&vm_->main, NM_SQL_INIT));
        NM_PR_VM_INFO();
    }
    if (nm_vect_str_len(&vm_->main, NM_SQL_TTY)) {
        nm_str_format(&buf, "%-12s%s", "tty: ",
                nm_vect_str_ctx(&vm_->main, NM_SQL_TTY));
        NM_PR_VM_INFO();
    }
    if (nm_vect_str_len(&vm_->main, NM_SQL_SOCK)) {
        nm_str_format(&buf, "%-12s%s", "socket: ",
                nm_vect_str_ctx(&vm_->main, NM_SQL_SOCK));
        NM_PR_VM_INFO();
    }
    if (nm_vect_str_len(&vm_->main, NM_SQL_DEBP)) {
        nm_str_format(&buf, "%-12s%s", "gdb port: ",
                nm_vect_str_ctx(&vm_->main, NM_SQL_DEBP));
        NM_PR_VM_INFO();
    }
    if (nm_str_cmp_st(nm_vect_str(&vm_->main, NM_SQL_DEBF), NM_ENABLE) == NM_OK) {
        nm_str_format(&buf, "%-12s", "freeze cpu: yes");
        NM_PR_VM_INFO();
    }
    if (nm_vect_str_len(&vm_->main, NM_SQL_ARGS)) {
        nm_str_format(&buf,"%-12s%s", "extra args: ",
                nm_vect_str_ctx(&vm_->main, NM_SQL_ARGS));
        NM_PR_VM_INFO();
    }
    if (nm_vect_str_len(&vm_->main, NM_SQL_GROUP)) {
        nm_str_format(&buf,"%-12s%s", "group: ",
                nm_vect_str_ctx(&vm_->main, NM_SQL_GROUP));
        NM_PR_VM_INFO();
    }

    /* print host IP addresses for TAP ints */
    for (size_t n = 0; n < ifs_count; n++) {
        size_t idx_shift = NM_IFS_IDX_COUNT * n;

        if (!nm_vect_str_len(&vm_->ifs, NM_SQL_IF_IP4 + idx_shift))
            continue;

        nm_str_format(&buf, "%-12s%s [%s]", "host IP: ",
            nm_vect_str_ctx(&vm_->ifs, NM_SQL_IF_NAME + idx_shift),
            nm_vect_str_ctx(&vm_->ifs, NM_SQL_IF_IP4 + idx_shift));
        NM_PR_VM_INFO();

    }

    /* print PID */
    {
        int fd;
        nm_str_t pid_path = NM_INIT_STR;

        nm_str_format(&pid_path, "%s/%s/%s",
            nm_cfg_get()->vm_dir.data, name_->data, NM_VM_PID_FILE);

        if ((status_ && (fd = open(pid_path.data, O_RDONLY)) != -1)) {
            char pid[10];
            ssize_t nread;
            int pid_num = 0;

            if ((nread = read(fd, pid, sizeof(pid))) > 0) {
                pid[nread - 1] = '\0';
                pid_num = atoi(pid);

                nm_str_format(&buf, "%-12s%d", "pid: ", pid_num);
                NM_PR_VM_INFO();
            }
            close(fd);

#if defined (NM_OS_LINUX)
            if (pid_num) {
                double usage = nm_stat_get_usage(pid_num);
                nm_str_format(&buf, "%-12s%0.1f%%", "cpu usage: ", usage);
                mvwhline(action_window, y, 1, ' ', cols - 4);
                NM_PR_VM_INFO();
            }
#else
            (void) pid_num;
#endif
        } else { /* clear PID file info and cpu usage data */
            if (y < (rows - 2)) {
                mvwhline(action_window, y, 1, ' ', cols - 4);
                mvwhline(action_window, y + 1, 1, ' ', cols - 4);
            }
            NM_STAT_CLEAN();
        }

        nm_str_free(&pid_path);
    }

    nm_str_free(&buf);
}

void nm_lan_help(void)
{
    const char *keys[] = {
        "a", "r", "u", "d"
    };

    const char *values[] = {
        "add veth interface",
        "remove veth interface",
        "up veth interface",
        "down veth interface",
        NULL
    };

    size_t hotkey_num = nm_arr_len(keys);
    size_t maxlen = nm_max_msg_len(values);

    nm_print_help__(keys, values, hotkey_num, maxlen);
}

void nm_print_help(void)
{
    const char *keys[] = {
        "r", "t",
#if defined(NM_WITH_VNC_CLIENT) || defined(NM_WITH_SPICE)
        "c",
#endif
        "p", "z", "f", "d", "y", "e",
        "i", "C", "a", "l", "b", "h",
        "m", "v", "u", "P", "R", "S",
        "X", "D",
#if defined (NM_OS_LINUX)
        "+", "-",
#endif
        "k", "/"
};

    const char *values[] = {
        "start vm",
        "start vm in temporary mode",
#if defined(NM_WITH_VNC_CLIENT) || defined(NM_WITH_SPICE)
        "connect to vm",
#endif
        "powerdown vm",
        "reset vm",
        "force stop vm",
        "delete vm",
        "rename vm",
        "edit vm settings",
        "edit network settings",
        "edit viewer settings",
        "add virtual disk",
        "clone vm",
        "edit boot settings",
        "share host filesystem",
        "show command",
        "delete virtual disk",
        "delete unused tap interfaces",
        "pause vm",
        "resume vm",
        "take vm snapshot",
        "revert vm snapshot",
        "delete vm snapshot",
#if defined (NM_OS_LINUX)
        "attach usb device",
        "detach usb device",
#endif
        "kill vm process",
        "search vm, filters",
        NULL
    };

    size_t hotkey_num = nm_arr_len(keys);
    size_t maxlen = nm_max_msg_len(values);

    nm_print_help__(keys, values, hotkey_num, maxlen);
}

static void
nm_print_help__(const char **keys, const char **values,
                size_t hotkey_num, size_t maxlen)
{
    size_t cols, rows;
    size_t n = 0;
    int perc;
    nm_str_t help_title = NM_INIT_STR;

    getmaxyx(action_window, rows, cols);
    rows -= 4;
    cols -= 2;

    if (maxlen + 10 > cols) {
        nm_warn(_(NM_MSG_SMALL_WIN));
        return;
    }

    perc = (hotkey_num > rows) ? ((rows * 100) / hotkey_num) : 100;
    if (perc == 100)
        nm_str_format(&help_title, _("nEMU %s - help [all]"), NM_VERSION);
    else
        nm_str_format(&help_title, _("nEMU %s - help [%d%%]") , NM_VERSION, perc);

    werase(action_window);
    nm_init_action(help_title.data);

    for (size_t y = 3; n < rows && n < hotkey_num; n++, y++)
        mvwprintw(action_window, y, 2, "%-10s%s", keys[n], values[n]);

    if (perc != 100) {
        size_t shift = 0;

        for (;;) {
            int ch = wgetch(action_window);

            if (ch == NM_KEY_ENTER) {
                size_t last = 0;

                shift++;
                n = shift;
                werase(action_window);

                for (size_t y = 3, l = 0; l < rows && n < hotkey_num; n++, y++, l++)
                    mvwprintw(action_window, y, 2, "%-10s%s", keys[n], values[n]);

                last = n;
                if (last < hotkey_num) {
                    perc = 100 * last / hotkey_num;
                    nm_str_format(&help_title, _("nEMU %s - help [%d%%]") , NM_VERSION, perc);
                } else {
                    nm_str_format(&help_title, _("nEMU %s - help [end]"), NM_VERSION);
                }

                nm_init_action(help_title.data);

                if (last >= hotkey_num) {
                    wgetch(action_window);
                    break;
                }
            } else {
                break;
            }
        }
    } else {
        wgetch(action_window);
    }

    wrefresh(action_window);
    nm_init_action(NULL);
    nm_str_free(&help_title);
}

#define NM_SI_ENEMIE_COUNT  60
#define NM_SI_ENEMIE_INROW  10
#define NM_SI_EMEMIE_ROTATE 10
#define NM_SI_DELAY         20000
#define NM_SI_BULLETS       200
#define NM_SI_BONIS_DELAY   1000
static void nm_si_game(void)
{
    typedef enum {
        NM_SI_SHIP_1,
        NM_SI_SHIP_2,
        NM_SI_SHIP_3,
        NM_SI_BULLET,
        NM_SI_PLAYER,
        NM_SI_BONUS
    } nm_si_type_t;

    typedef struct {
        int pos_x;
        int pos_y;
        nm_si_type_t type;
        size_t health;
        bool hit;
    } nm_si_t;

    static bool si_player_init = false;
    static size_t bullets_cnt;
    static nm_si_t player;
    static size_t score;
    static size_t level;
    static size_t speed;

    bool play = true, change_dir = false, next = false;
    int max_x, max_y, start_x, start_y;
    nm_vect_t pl_bullets = NM_INIT_VECT;
    nm_vect_t en_bullets = NM_INIT_VECT;
    nm_vect_t enemies = NM_INIT_VECT;
    nm_str_t info = NM_INIT_STR;
    bool bonus_active = false;
    int direction = 1, mch;
    int b_direction = 1;
    uint64_t iter = 0;
    nm_si_t bonus;

    const char *shape_player = "<^>";
    const char *shape_ship0  = "#v#";
    const char *shape_ship1  = ")v(";
    const char *shape_ship2  = "]v[";
    const char *shape_dead   = "x-x";
    const char *shape_hit    = ">-<";
    const char *shape_win    = "^_^";
    const char *shape_bonus  = "(?)";

    nodelay(action_window, TRUE);
    getmaxyx(action_window, max_y, max_x);

    bonus = (nm_si_t) { 1, 3, NM_SI_BONUS, 1, false };

    if (!si_player_init) {
        player = (nm_si_t) { max_x / 2, max_y - 2, NM_SI_PLAYER, 5, false };
        score = 0, bullets_cnt = NM_SI_BULLETS;
        si_player_init = true;
        speed = NM_SI_EMEMIE_ROTATE;
        level = 0;
    } else {
        level++;
        bullets_cnt += 200;
        if (speed > 1) {
            speed--;
        }
    }
    start_x = (max_x / 5);
    start_y = 5;

    for (size_t n = 0; n < NM_SI_ENEMIE_COUNT; n++) {
        nm_si_type_t type = (n < 10) ? NM_SI_SHIP_3 :
                            (n >= 10 && n < 30) ? NM_SI_SHIP_2 : NM_SI_SHIP_1;
        size_t health = (n < 10) ? 3 :
                            (n >= 10 && n < 30) ? 2 : 1;
        nm_si_t ship = (nm_si_t) { start_x, start_y, type, health, false };

        nm_vect_insert(&enemies, &ship, sizeof(nm_si_t), NULL);
        start_x += 5;
        if (!((n + 1) % NM_SI_ENEMIE_INROW)) {
            start_y++;
            start_x = (max_x / 5);
        }
    }

    while (play) {
        int ch = wgetch(action_window);

        werase(action_window);
        nm_str_format(&info, "Level %zu [score: %zu bullets: %zu hp: %zu]",
                level, score, bullets_cnt, player.health);
        nm_init_action(info.data);

        switch (ch) {
        case KEY_LEFT:
        case NM_KEY_A:
            if (player.pos_x > 1) {
                player.pos_x -= 1;
            }
            break;
        case KEY_RIGHT:
        case NM_KEY_D:
            if (player.pos_x < max_x - 4) {
                player.pos_x += 1;
            }
            break;
        case ' ':
            {
                nm_si_t bullet = (nm_si_t) { player.pos_x + 1,
                    max_y - 3, NM_SI_BULLET, 0, false };
                if (bullets_cnt) {
                    nm_vect_insert(&pl_bullets, &bullet, sizeof(nm_si_t), NULL);
                    bullets_cnt--;
                }
            }
            break;
        case ERR:
            break;
        default:
            play = false;
            break;
        }

        if (!enemies.n_memb) {
            play = false;
            next = true;
            mvwaddstr(action_window, player.pos_y, player.pos_x, shape_win);
            break;
        }

        for (size_t n = 0; n < enemies.n_memb; n++) {
            nm_si_t *e = nm_vect_at(&enemies, n);

            for (size_t nb = 0; nb < pl_bullets.n_memb; nb++) {
                nm_si_t *b = nm_vect_at(&pl_bullets, nb);

                if (((b->pos_x >= e->pos_x && b->pos_x <= e->pos_x + 2)) &&
                        (e->pos_y == b->pos_y)) {
                    e->health--;
                    if (!e->health) {
                        switch (e->type) {
                            case NM_SI_SHIP_1:
                                score += 10;
                                break;
                            case NM_SI_SHIP_2:
                                score += 20;
                                break;
                            case NM_SI_SHIP_3:
                                score += 30;
                                break;
                            default:
                                break;
                        }
                    } else {
                        e->hit = true;
                        switch (e->type) {
                            case NM_SI_SHIP_1:
                                score += 1;
                                break;
                            case NM_SI_SHIP_2:
                                score += 2;
                                break;
                            case NM_SI_SHIP_3:
                                score += 3;
                                break;
                            default:
                                break;
                        }
                    }
                    nm_vect_delete(&pl_bullets, nb, NULL);
                    nb--;
                    break;
                }
            }

            if (e->health) {
                bool can_shoot = true;
                const char *shape;
                switch (e->type) {
                case NM_SI_SHIP_1:
                    shape = shape_ship0;
                    break;
                case NM_SI_SHIP_2:
                    shape = shape_ship1;
                    break;
                case NM_SI_SHIP_3:
                    shape = shape_ship2;
                    break;
                default:
                    break;
                }

                if (e->hit) {
                    shape = shape_hit;
                    e->hit = false;
                }

                mvwaddstr(action_window, e->pos_y, e->pos_x, shape);
                if (iter % (speed + 1) == speed) {
                    direction ? e->pos_x++ : e->pos_x--;
                    if (!direction && e->pos_x == 1) {
                        change_dir = true;
                    } else if (direction && (e->pos_x == max_x - 4)) {
                        change_dir = true;
                    }
                }

                for (size_t no = n; no < enemies.n_memb; no++) {
                    nm_si_t *eo = nm_vect_at(&enemies, no);
                    if (((eo->pos_y - 1 >= e->pos_y &&
                          eo->pos_y - 1 <= e->pos_y + 5)) &&
                          (e->pos_x == eo->pos_x)) {
                        can_shoot = false;
                        break;
                    }
                }

                if (can_shoot) {
                    struct timespec ts;
                    int r;

                    clock_gettime(CLOCK_MONOTONIC, &ts);
                    srand((time_t) ts.tv_nsec);
                    r = rand() % 1000;
                    if (r > 498 && r < 500) { /* fire! */
                        nm_si_t bullet = (nm_si_t) {
                            e->pos_x + 1, e->pos_y + 1,
                            NM_SI_BULLET, 0, false
                        };
                        nm_vect_insert(&en_bullets, &bullet,
                                sizeof(nm_si_t), NULL);
                    }
                }

            } else {
                mvwaddstr(action_window, e->pos_y, e->pos_x, shape_dead);
                nm_vect_delete(&enemies, n, NULL);
                n--;
            }
        }

        if (enemies.n_memb && change_dir) {
            for (size_t n = 0; n < enemies.n_memb; n++) {
                nm_si_t *e = nm_vect_at(&enemies, n);
                e->pos_y += 1;
                if (e->pos_y == max_y - 2) {
                    play = false;
                }
            }

            if (!direction) {
                direction = 1;
            } else {
                direction = 0;
            }
            change_dir = false;
        }

        for (size_t n = 0; n < pl_bullets.n_memb; n++) {
            nm_si_t *b = nm_vect_at(&pl_bullets, n);
            mvwaddch(action_window, b->pos_y, b->pos_x, '*');
            b->pos_y--;

            if (b->pos_y == 1) {
                nm_vect_delete(&pl_bullets, n, NULL);
                n--;
            }
        }

        if (iter % (NM_SI_BONIS_DELAY + 1) == NM_SI_BONIS_DELAY &&
                !bonus_active) {
            bonus_active = true;
        }

        if (bonus_active) {
            for (size_t n = 0; n < pl_bullets.n_memb; n++) {
                nm_si_t *b = nm_vect_at(&pl_bullets, n);

                if (((b->pos_x >= bonus.pos_x && b->pos_x <= bonus.pos_x + 2)) &&
                        (bonus.pos_y == b->pos_y)) {
                    bonus.hit = true;
                    nm_vect_delete(&pl_bullets, n, NULL);
                    break;
                }
            }

            if (bonus.hit) {
                mvwaddstr(action_window, bonus.pos_y, bonus.pos_x, shape_dead);
                bonus.hit = false;
                bonus_active = false;
                if (b_direction) {
                    b_direction = 0;
                    bonus.pos_x = max_x - 4;
                } else {
                    b_direction = 1;
                    bonus.pos_x = 1;
                }
                score += 100;
                (iter % 2) ? bullets_cnt += 50 : player.health++;
            } else {
                mvwaddstr(action_window, bonus.pos_y, bonus.pos_x, shape_bonus);
                if (iter % (speed + 1) == speed) {
                    (b_direction) ? bonus.pos_x++ : bonus.pos_x--;
                    if (!b_direction && bonus.pos_x == 1) {
                        bonus_active = false;
                        b_direction = 1;
                    } else if (b_direction && (bonus.pos_x == max_x - 4)) {
                        bonus_active = false;
                        b_direction = 0;
                    }
                }
            }
        }

        for (size_t n = 0; n < en_bullets.n_memb; n++) {
            nm_si_t *b = nm_vect_at(&en_bullets, n);
            mvwaddch(action_window, b->pos_y, b->pos_x, '|');

            if (((b->pos_x >= player.pos_x && b->pos_x <= player.pos_x + 2)) &&
                    (player.pos_y == b->pos_y)) {
                player.health--;
                player.hit = true;
            }

            b->pos_y++;

            if (b->pos_y == max_y - 1) {
                nm_vect_delete(&en_bullets, n, NULL);
                n--;
            }
        }

        if (player.health) {
            if (player.hit) {
                mvwaddstr(action_window, player.pos_y, player.pos_x, shape_hit);
                player.hit = false;
            } else {
                mvwaddstr(action_window, player.pos_y, player.pos_x, shape_player);
            }
        } else {
            mvwaddstr(action_window, player.pos_y, player.pos_x, shape_dead);
            play = false;
        }

        wrefresh(action_window);

        usleep(NM_SI_DELAY);
        iter++;
    }

    nodelay(action_window, FALSE);
    NM_ERASE_TITLE(action, getmaxx(action_window));
    nm_str_format(&info, "%s [score: %zu bullets: %zu hp: %zu]",
            next ? "(n)ext level, (q)uit" : "Game over, (q)uit, (r)eplay",
            score, bullets_cnt, player.health);
    nm_init_action(info.data);

    nm_str_free(&info);
    nm_vect_free(&pl_bullets, NULL);
    nm_vect_free(&en_bullets, NULL);
    nm_vect_free(&enemies, NULL);

    do {
        mch = wgetch(action_window);
    } while ((mch != 'n') && (mch != 'q') && (mch != 'r'));

    if (mch == 'n' && next) {
        nm_si_game();
    } else if (mch == 'r') {
        si_player_init = false;
        nm_si_game();
    } else {
        si_player_init = false;
    }
}

void nm_print_nemu(void)
{
    int ch;
    size_t max_y = getmaxy(action_window);
    const char *nemu[] = {
        "            .x@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@o.           ",
        "          .x@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@x.          ",
        "         o@@@@@@@@@@@@@x@@@@@@@@@@@@@@@@@@@@@@@@@o         ",
        "       .x@@@@@@@@x@@@@xx@@@@@@@@@@@@@@@@@@@@@@@@@@o        ",
        "      .x@@@@@@@x. .x@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@.       ",
        "      x@@@@@ox@x..o@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@.      ",
        "     o@@@@@@xx@@@@@@@@@@@@@@@@@@@@@@@@x@@@@@@@@@@@@@o      ",
        "    .x@@@@@@@@@@x@@@@@@@@@@@@@@@@@@@@@o@@@@@@@@@@@@@@.     ",
        "    .@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@x.@@@@@@@@@@@@@@o     ",
        "    o@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@xx..o@@@@@@@@@@@@@x     ",
        "    x@@@@@@@@@@@@@@@@@@xxoooooooo........oxxx@@@@@@@@x.    ",
        "    x@@@@@@@@@@x@@@@@@x. .oxxx@@xo.       .oxo.o@@@@@x     ",
        "    o@@@@@@@@x..o@@@@@o .xxx@@@o ..      .x@ooox@@@@@o     ",
        "    .x@@@@@@@oo..x@@@@.  ...oo..         .oo. o@@@@@o.     ",
        "     .x@@@@@@xoooo@@@@.                       x@@@x..      ",
        "       o@@@@@@ooxx@@@@.                 .    .@@@x..       ",
        "        .o@@@@@@o.x@@@.                 ..   o@@x.         ",
        "         .x@@@@@@xx@@@.                 ..  .x@o           ",
        "         o@@@@@@@o.x@@.                    .x@x.           ",
        "         o@@@@@@@o o@@@xo.         .....  .x@x.            ",
        "          .@@@@@x. .x@o.x@x..           .o@@@.             ",
        "         .x@@@@@o.. o@o  .o@@xoo..    .o..@@o              ",
        "          o@@@@oo@@xx@x.   .x@@@ooooooo. o@x.              ",
        "         .o@@o..xx@@@@@xo.. o@x.         xx.               ",
        "          .oo. ....ox@@@@@@xx@o          xo.               ",
        "          ....       .o@@@@@@@@o        .@..               ",
        "           ...         ox.ox@@@.        .x..               ",
        "            ...        .x. .@@x.        .o                 ",
        "              .         .o .@@o.         o                 ",
        "                         o. x@@o         .                 ",
        "                         .o o@@..        .                 ",
        "                          ...@o..        .                 ",
        "                           .x@xo.                          ",
        "                            .x..                           ",
        "                           .x....                          ",
        "                           o@..xo                          ",
        "                           o@o oo                          ",
        "                           ..                              "
    };

    for (size_t l = 3, n = 0; n < (max_y - 4) && n < nm_arr_len(nemu); n++, l++)
        mvwprintw(action_window, l, 1, "%s", nemu[n]);

    ch = wgetch(action_window);
    if (ch == NM_KEY_S) {
        nm_si_game();
    }
}

void nm_align2line(nm_str_t *str, size_t line_len)
{
    if (line_len <= 4)
        return;

    if (str->len > (line_len - 4)) {
        nm_str_trunc(str, line_len - 7);
        nm_str_add_text(str, "...");
    }
}

size_t nm_max_msg_len(const char **msg)
{
    if (!msg)
        return 0;

    size_t len = 0;

    while (*msg) {
        size_t mb_len = mbstowcs(NULL, _(*msg), strlen(_(*msg)));
        len = (mb_len > len) ? mb_len : len;
        msg++;
    }

    return len;
}

static int nm_warn__(const char *msg, int red)
{
    if (!msg)
        return ERR;

    int ch;

    werase(help_window);
    nm_init_help(msg, red);
    while ((ch = wgetch(help_window)) == KEY_RESIZE);
    werase(help_window);
    nm_init_help_main();

    return ch;
}

int nm_warn(const char *msg)
{
    return nm_warn__(msg, NM_TRUE);
}

int nm_notify(const char *msg)
{
    return nm_warn__(msg, NM_FALSE);
}

int nm_window_scale_inc(void)
{
    if (nm_window_scale > 0.8)
        return NM_ERR;

    nm_window_scale += 0.02;

    return NM_OK;
}

int nm_window_scale_dec(void)
{
    if (nm_window_scale < 0.2)
        return NM_ERR;

    nm_window_scale -= 0.02;

    return NM_OK;
}

/* vim:set ts=4 sw=4: */
