#include <nm_core.h>
#include <nm_form.h>
#include <nm_utils.h>
#include <nm_string.h>
#include <nm_window.h>
#include <nm_ncurses.h>
#include <nm_database.h>
#include <nm_cfg_file.h>
#include <nm_stat_usage.h>

#define NM_HELP_GEN(name)                                    \
    void nm_init_help_ ## name(void) {                       \
        nm_print_help_lines(nm_help_ ## name ## _msg,        \
            nm_arr_len(nm_help_ ## name ## _msg), NM_FALSE); \
    }

static float nm_window_scale = 0.7;

static void nm_init_window__(nm_window_t *w, const char *msg);
static void nm_print_help_lines(const char **msg, size_t objs, int err);
static void nm_print_help__(const char **keys, const char **values,
                            size_t hotkey_num, size_t maxlen);

static const char *nm_help_main_msg[] = {
    "q:Quit", "I:Install VM",
#if defined (NM_WITH_OVF_SUPPORT)
    "O:Import OVA",
#endif
    "A:Import image", "N:Network", "?:Help"
};

static const char *nm_help_lan_msg[] = {
    "q:Back",
#if defined (NM_WITH_NETWORK_MAP)
    "e:Export SVG map",
#endif
    "?:Help"
};

static const char *nm_help_iface_msg[] = {
    "q:Back", "enter:Edit"
};

static const char *nm_help_edit_msg[] = {
    "esc:Cancel", "enter:Save"
};

static const char *nm_help_import_msg[] = {
    "esc:Cancel", "enter:Import"
};

static const char *nm_help_install_msg[] = {
    "esc:Cancel", "enter:Install"
};

static const char *nm_help_clone_msg[] = {
    "esc:Cancel", "enter:Clone"
};

static const char *nm_help_export_msg[] = {
    "esc:Cancel", "enter:Export"
};

static const char *nm_help_delete_msg[] = {
    "q:Back", "enter:Delete"
};

NM_HELP_GEN(main)
NM_HELP_GEN(lan)
NM_HELP_GEN(edit)
NM_HELP_GEN(import)
NM_HELP_GEN(install)
NM_HELP_GEN(iface)
NM_HELP_GEN(clone)
NM_HELP_GEN(export)
NM_HELP_GEN(delete)

void nm_create_windows(void)
{
    int action_cols, screen_x, screen_y;
    nm_cord_t help_size = NM_INIT_POS;
    nm_cord_t side_size = NM_INIT_POS;
    nm_cord_t action_size = NM_INIT_POS;

    getmaxyx(stdscr, screen_y, screen_x);
    action_cols = screen_x * nm_window_scale;

    help_size = NM_SET_POS(1, screen_x, 0, 0);
    side_size = NM_SET_POS(screen_y - 1, screen_x - action_cols, 0, 1);
    action_size = NM_SET_POS(screen_y - 1, action_cols, screen_x - action_cols, 1);

    help_window = nm_init_window(&help_size);
    side_window = nm_init_window(&side_size);
    action_window = nm_init_window(&action_size);
}

void nm_init_help(const char *msg, int err)
{
    nm_print_help_lines(&msg, 1, err);
}

static void nm_print_help_lines(const char **msg, size_t objs, int err)
{
    int x = 1, y = 0;

    assert(msg);
    wbkgd(help_window, COLOR_PAIR(err ? NM_COLOR_RED : NM_COLOR_BLACK));

    for (size_t n = 0; n < objs; n++)
    {
        if (n > 0)
        {
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
    wattroff(side_window, COLOR_PAIR(NM_COLOR_HIGHLIGHT));
    nm_init_window__(side_window, _("VM list"));
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
    nm_vmctl_data_t vm = NM_VMCTL_INIT_DATA;

    int col = getmaxx(stdscr);

    nm_vmctl_get_data(name, &vm);

    nm_vmctl_gen_cmd(&argv, &vm, name, NM_VMCTL_INFO, NULL);

    nm_cmd_str(&buf, &argv);

    nm_clear_screen();
    mvprintw(1, (col - name->len) / 2, "%s", name->data);
    mvprintw(3, 0, "%s", buf.data);

    nm_str_free(&buf);
    nm_vect_free(&argv, NULL);
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

    for (size_t n = 0; n < count; n++)
    {
        size_t idx_shift = 5 * n;
        if (n && n < count)
        {

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
    nm_str_t buf = NM_INIT_STR;
    size_t y = 3, x = 2;
    size_t cols, rows;
    size_t idx_shift;
    chtype ch1, ch2;
    ch1 = ch2 = 0;

    assert(idx > 0);
    idx_shift = 2 * (--idx);

    getmaxyx(action_window, rows, cols);

    nm_str_format(&buf, "%-12s%sGb", "capacity: ",
            nm_vect_str_ctx(v, 1 + idx_shift));
    NM_PR_VM_INFO();

    nm_str_free(&buf);
}

void nm_print_iface_info(const nm_vmctl_data_t *vm, size_t idx)
{
    nm_str_t buf = NM_INIT_STR;
    size_t y = 3, x = 2;
    size_t cols, rows;
    size_t idx_shift, mvtap_idx = 0;
    chtype ch1, ch2;
    ch1 = ch2 = 0;

    assert(idx > 0);
    idx_shift = NM_IFS_IDX_COUNT * (--idx);

    getmaxyx(action_window, rows, cols);

    nm_str_format(&buf, "%-12s%s", "hwaddr: ",
            nm_vect_str_ctx(&vm->ifs, NM_SQL_IF_MAC + idx_shift));
    NM_PR_VM_INFO();

    nm_str_format(&buf, "%-12s%s", "driver: ",
            nm_vect_str_ctx(&vm->ifs, NM_SQL_IF_DRV + idx_shift));
    NM_PR_VM_INFO();

    if (nm_vect_str_len(&vm->ifs, NM_SQL_IF_IP4 + idx_shift) > 0)
    {
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

    nm_str_free(&buf);
}

void nm_print_vm_info(const nm_str_t *name, const nm_vmctl_data_t *vm, int status)
{
    nm_str_t buf = NM_INIT_STR;
    size_t y = 3, x = 2;
    size_t cols, rows;
    size_t ifs_count, drives_count;
    chtype ch1, ch2;
    ch1 = ch2 = 0;

    getmaxyx(action_window, rows, cols);

    nm_str_format(&buf, "%-12s%s", "arch: ",
        nm_vect_str_ctx(&vm->main, NM_SQL_ARCH));
    NM_PR_VM_INFO();

    nm_str_format(&buf, "%-12s%s", "cores: ",
        nm_vect_str_ctx(&vm->main, NM_SQL_SMP));
    NM_PR_VM_INFO();

    nm_str_format(&buf, "%-12s%s %s", "memory: ",
        nm_vect_str_ctx(&vm->main, NM_SQL_MEM), "Mb");
    NM_PR_VM_INFO();

    if (nm_str_cmp_st(nm_vect_str(&vm->main, NM_SQL_KVM), NM_ENABLE) == NM_OK)
    {
        if (nm_str_cmp_st(nm_vect_str(&vm->main, NM_SQL_HCPU), NM_ENABLE) == NM_OK)
            nm_str_format(&buf, "%-12s%s", "kvm: ", "enabled [+hostcpu]");
        else
            nm_str_format(&buf, "%-12s%s", "kvm: ", "enabled");
    }
    else
    {
        nm_str_format(&buf, "%-12s%s", "kvm: ", "disabled");
    }
    NM_PR_VM_INFO();

    if (nm_str_cmp_st(nm_vect_str(&vm->main, NM_SQL_USBF), "1") == NM_OK)
    {
        nm_str_format(&buf, "%-12s%s [%s]", "usb: ", "enabled",
                (nm_str_cmp_st(nm_vect_str(&vm->main, NM_SQL_USBT), NM_DEFAULT_USBVER) == NM_OK) ?
                "XHCI" : "EHCI");
    }
    else
    {
        nm_str_format(&buf, "%-12s%s", "usb: ", "disabled");
    }
    NM_PR_VM_INFO();

    nm_str_format(&buf, "%-12s%s [%u]", "vnc port: ",
             nm_vect_str_ctx(&vm->main, NM_SQL_VNC),
             nm_str_stoui(nm_vect_str(&vm->main, NM_SQL_VNC), 10) + 5900);
    NM_PR_VM_INFO();

    /* {{{ print network interfaces info */
    ifs_count = vm->ifs.n_memb / NM_IFS_IDX_COUNT;

    for (size_t n = 0; n < ifs_count; n++)
    {
        size_t idx_shift = NM_IFS_IDX_COUNT * n;

        nm_str_format(&buf, "eth%zu%-8s%s [%s %s%s]",
                 n, ":",
                 nm_vect_str_ctx(&vm->ifs, NM_SQL_IF_NAME + idx_shift),
                 nm_vect_str_ctx(&vm->ifs, NM_SQL_IF_MAC + idx_shift),
                 nm_vect_str_ctx(&vm->ifs, NM_SQL_IF_DRV + idx_shift),
                 (nm_str_cmp_st(nm_vect_str(&vm->ifs, NM_SQL_IF_VHO + idx_shift),
                    NM_ENABLE) == NM_OK) ? "+vhost" : "");

        NM_PR_VM_INFO();
    }
    /* }}} network */

    /* {{{ print drives info */
    drives_count = vm->drives.n_memb / NM_DRV_IDX_COUNT;

    for (size_t n = 0; n < drives_count; n++)
    {
        size_t idx_shift = NM_DRV_IDX_COUNT * n;
        int boot = 0;

        if (nm_str_cmp_st(nm_vect_str(&vm->drives, NM_SQL_DRV_BOOT + idx_shift),
                NM_ENABLE) == NM_OK)
        {
            boot = 1;
        }

        nm_str_format(&buf, "disk%zu%-7s%s [%sGb %s] %s", n, ":",
                 nm_vect_str_ctx(&vm->drives, NM_SQL_DRV_NAME + idx_shift),
                 nm_vect_str_ctx(&vm->drives, NM_SQL_DRV_SIZE + idx_shift),
                 nm_vect_str_ctx(&vm->drives, NM_SQL_DRV_TYPE + idx_shift),
                 boot ? "*" : "");
        NM_PR_VM_INFO();
    }
    /* }}} drives */

    /* {{{ print 9pfs info */
    if (nm_str_cmp_st(nm_vect_str(&vm->main, NM_SQL_9FLG), "1") == NM_OK)
    {
        nm_str_format(&buf, "%-12s%s [%s]", "9pfs: ",
                 nm_vect_str_ctx(&vm->main, NM_SQL_9PTH),
                 nm_vect_str_ctx(&vm->main, NM_SQL_9ID));
        NM_PR_VM_INFO();
    }
    /* }}} 9pfs */

    /* {{{ Generate guest boot settings info */
    if (nm_vect_str_len(&vm->main, NM_SQL_MACH))
    {
        nm_str_format(&buf, "%-12s%s", "machine: ",
                nm_vect_str_ctx(&vm->main, NM_SQL_MACH));
        NM_PR_VM_INFO();
    }
    if (nm_vect_str_len(&vm->main, NM_SQL_BIOS))
    {
        nm_str_format(&buf,"%-12s%s", "bios: ",
                nm_vect_str_ctx(&vm->main, NM_SQL_BIOS));
        NM_PR_VM_INFO();
    }
    if (nm_vect_str_len(&vm->main, NM_SQL_KERN))
    {
        nm_str_format(&buf,"%-12s%s", "kernel: ",
                nm_vect_str_ctx(&vm->main, NM_SQL_KERN));
        NM_PR_VM_INFO();
    }
    if (nm_vect_str_len(&vm->main, NM_SQL_KAPP))
    {
        nm_str_format(&buf, "%-12s%s", "cmdline: ",
                nm_vect_str_ctx(&vm->main, NM_SQL_KAPP));
        NM_PR_VM_INFO();
    }
    if (nm_vect_str_len(&vm->main, NM_SQL_INIT))
    {
        nm_str_format(&buf, "%-12s%s", "initrd: ",
                nm_vect_str_ctx(&vm->main, NM_SQL_INIT));
        NM_PR_VM_INFO();
    }
    if (nm_vect_str_len(&vm->main, NM_SQL_TTY))
    {
        nm_str_format(&buf, "%-12s%s", "tty: ",
                nm_vect_str_ctx(&vm->main, NM_SQL_TTY));
        NM_PR_VM_INFO();
    }
    if (nm_vect_str_len(&vm->main, NM_SQL_SOCK))
    {
        nm_str_format(&buf, "%-12s%s", "socket: ",
                nm_vect_str_ctx(&vm->main, NM_SQL_SOCK));
        NM_PR_VM_INFO();
    }
    if (nm_vect_str_len(&vm->main, NM_SQL_DEBP))
    {
        nm_str_format(&buf, "%-12s%s", "gdb port: ",
                nm_vect_str_ctx(&vm->main, NM_SQL_DEBP));
        NM_PR_VM_INFO();
    }
    if (nm_str_cmp_st(nm_vect_str(&vm->main, NM_SQL_DEBF), NM_ENABLE) == NM_OK)
    {
        nm_str_format(&buf, "%-12s", "freeze cpu: yes");
        NM_PR_VM_INFO();
    }

    /* }}} boot settings */

    /* {{{ Print host IP addresses for TAP ints */
    //y++;
    for (size_t n = 0; n < ifs_count; n++)
    {
        size_t idx_shift = NM_IFS_IDX_COUNT * n;

        if (!nm_vect_str_len(&vm->ifs, NM_SQL_IF_IP4 + idx_shift))
            continue;

        nm_str_format(&buf, "%-12s%s [%s]", "host IP: ",
            nm_vect_str_ctx(&vm->ifs, NM_SQL_IF_NAME + idx_shift),
            nm_vect_str_ctx(&vm->ifs, NM_SQL_IF_IP4 + idx_shift));
        NM_PR_VM_INFO();

    } /* }}} host IP addr */

    /* {{{ Print PID */
    {
        int fd;
        float usage;
        nm_str_t pid_path = NM_INIT_STR;
        nm_str_t qmp_path = NM_INIT_STR;
        ssize_t nread;
        char pid[10];
        int pid_num = 0;

        nm_str_copy(&pid_path, &nm_cfg_get()->vm_dir);
        nm_str_add_char(&pid_path, '/');
        nm_str_add_str(&pid_path, name);
        nm_str_copy(&qmp_path, &pid_path);
        nm_str_add_text(&pid_path, "/" NM_VM_PID_FILE);
        nm_str_add_text(&qmp_path, "/" NM_VM_QMP_FILE);

        if ((status && (fd = open(pid_path.data, O_RDONLY)) != -1))
        {
            if ((nread = read(fd, pid, sizeof(pid))) > 0)
            {
                pid[nread - 1] = '\0';
                pid_num = atoi(pid);
                nm_str_format(&buf, "%-12s%d", "pid: ", pid_num);
                NM_PR_VM_INFO();
            }
            close(fd);

            usage = nm_stat_get_usage(pid_num);
            nm_str_format(&buf, "%-12s%0.1f%%", "cpu usage: ", usage);
            mvwhline(action_window, y, 1, ' ', cols - 4);
            NM_PR_VM_INFO();
        }
        else /* clear PID file info and cpu usage data */
        {
            if (y < (rows - 2))
            {
                mvwhline(action_window, y, 1, ' ', cols - 4);
                mvwhline(action_window, y + 1, 1, ' ', cols - 4);
            }
            NM_STAT_CLEAN();
        }

        nm_str_free(&pid_path);
        nm_str_free(&qmp_path);
    } /* }}} PID */

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
        "p", "z", "f", "d", "e", "i",
        "a", "l", "b", "h", "m", "v",
        "u", "P", "R",
#if (NM_SAVEVM_SNAPSHOTS)
        "S", "X", "D",
#endif
#if defined (NM_OS_LINUX)
        "+", "-",
#endif
        "k"
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
        "edit vm settings",
        "edit network settings",
        "add virtual disk",
        "clone vm",
        "edit boot settings",
        "share host filesystem",
        "show command",
        "delete virtual disk",
        "delete unused tap interfaces",
        "pause vm",
        "resume vm",
#if (NM_SAVEVM_SNAPSHOTS)
        "take vm snapshot",
        "revert vm snapshot",
        "delete vm snapshot",
#endif
#if defined (NM_OS_LINUX)
        "attach usb device",
        "detach usb device",
#endif
        "kill vm process",
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
    size_t n = 0, last = 0;
    int perc;
    nm_str_t help_title = NM_INIT_STR;

    getmaxyx(action_window, rows, cols);
    rows -= 4;
    cols -= 2;

    if (maxlen + 10 > cols)
    {
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

    if (perc != 100)
    {
        int ch;
        size_t shift = 0;

        for (;;)
        {
            ch = wgetch(action_window);

            if (ch == NM_KEY_ENTER)
            {
                shift++;
                n = shift;
                werase(action_window);

                if (last == hotkey_num)
                    break;

                for (size_t y = 3, l = 0; l < rows && n < hotkey_num; n++, y++, l++)
                    mvwprintw(action_window, y, 2, "%-10s%s", keys[n], values[n]);

                last = n;
                if (last < hotkey_num)
                {
                    perc = 100 * last / hotkey_num;
                    nm_str_format(&help_title, _("nEMU %s - help [%d%%]") , NM_VERSION, perc);
                }
                else
                    nm_str_format(&help_title, _("nEMU %s - help [end]"), NM_VERSION);

                nm_init_action(help_title.data);
            }
            else
                break;
        }
    }
    else
        wgetch(action_window);

    wrefresh(action_window);
    nm_init_action(NULL);
    nm_str_free(&help_title);
}

void nm_print_nemu(void)
{
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

    wgetch(action_window);
}

void nm_align2line(nm_str_t *str, size_t line_len)
{
    assert(line_len > 4);

    if (str->len > (line_len - 4))
    {
        nm_str_trunc(str, line_len - 7);
        nm_str_add_text(str, "...");
    }
}

size_t nm_max_msg_len(const char **msg)
{
    size_t len = 0;

    assert(msg != NULL);

    while (*msg)
    {
        size_t mb_len = mbstowcs(NULL, _(*msg), strlen(_(*msg)));
        len = (mb_len > len) ? mb_len : len;
        msg++;
    }

    return len;
}

int nm_warn__(const char *msg, int red)
{
    int ch;

    assert(msg != NULL);
    werase(help_window);
    nm_init_help(msg, red);
    ch = wgetch(help_window);
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

/* vim:set ts=4 sw=4 fdm=marker: */
