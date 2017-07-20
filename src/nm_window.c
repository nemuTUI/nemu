#include <nm_core.h>
#include <nm_utils.h>
#include <nm_string.h>
#include <nm_window.h>
#include <nm_ncurses.h>
#include <nm_database.h>
#include <nm_cfg_file.h>
#include <nm_vm_control.h>

#define NM_VM_MSG   "F1 - help, ESC - main menu "
#define NM_MAIN_MSG "Enter - select a choice, ESC - exit"

void nm_print_main_window(void)
{
    nm_print_title(_(NM_MAIN_MSG));
}

void nm_print_vm_window(void)
{
    nm_print_title(_(NM_VM_MSG));
}

int nm_print_warn(int nlines, int begin_x, const char *msg)
{
    size_t msg_len;
    nm_window_t *w;
    int ch;

    msg_len = mbstowcs(NULL, msg, strlen(msg));
    if (msg_len % 2 != 0)
        msg_len--;
    w = nm_init_window(nlines, msg_len + 6, begin_x);
    curs_set(0);
    box(w, 0, 0);
    mvwprintw(w, 1, 2, " %s ", msg);
    wrefresh(w);
    ch = wgetch(w);
    delwin(w);

    return ch;
}

void nm_print_cmd(const nm_str_t *name)
{
    nm_vmctl_data_t vm = NM_VMCTL_INIT_DATA;
    nm_str_t cmd = NM_INIT_STR;
    int col = getmaxx(stdscr);

    nm_vmctl_get_data(name, &vm);
    nm_vmctl_gen_cmd(&cmd, &vm, name, NM_VMCTL_INFO, NULL);

    nm_clear_screen();
    mvprintw(1, (col - name->len) / 2, "%s", name->data);
    mvprintw(3, 0, "%s", cmd.data);

    nm_vmctl_free_data(&vm);
    nm_str_free(&cmd);

    refresh();
    getch();
}

void nm_print_vm_info(const nm_str_t *name)
{
    nm_vmctl_data_t vm = NM_VMCTL_INIT_DATA;
    int y = 1, x = 3;
    int col = getmaxx(stdscr);
    size_t ifs_count, drives_count;

    nm_vmctl_get_data(name, &vm);

    nm_clear_screen();
    border(0,0,0,0,0,0,0,0);

    mvprintw(y++, (col - name->len) / 2, "%s", name->data);
    mvaddch(y, 0, ACS_LTEE);
    mvhline(y, 1, ACS_HLINE, col - 2);
    mvaddch(y++, col - 1, ACS_RTEE);
    mvprintw(y++, x, "%-12s%s", "arch: ",
        nm_vect_str_ctx(&vm.main, NM_SQL_ARCH));
    mvprintw(y++, x, "%-12s%s", "cores: ",
        nm_vect_str_ctx(&vm.main, NM_SQL_SMP));
    mvprintw(y++, x, "%-12s%s %s", "memory: ",
        nm_vect_str_ctx(&vm.main, NM_SQL_MEM), "Mb");

    if (nm_str_cmp_st(nm_vect_str(&vm.main, NM_SQL_KVM), NM_ENABLE) == NM_OK)
    {
        if (nm_str_cmp_st(nm_vect_str(&vm.main, NM_SQL_HCPU), NM_ENABLE) == NM_OK)
            mvprintw(y++, x, "%-12s%s", "kvm: ", "enabled [+hostcpu]");
        else
            mvprintw(y++, x, "%-12s%s", "kvm: ", "enabled");
    }
    else
    {
        mvprintw(y++, x, "%-12s%s", "kvm: ", "disabled");
    }

    if (nm_str_cmp_st(nm_vect_str(&vm.main, NM_SQL_USBF), "1") == NM_OK)
    {
        mvprintw(y++, x, "%-12s%s [%s]", "usb: ", "enabled",
                 nm_vect_str_ctx(&vm.main, NM_SQL_USBD));
    }
    else
    {
        mvprintw(y++, x, "%-12s%s", "usb: ", "disabled");
    }

    mvprintw(y++, x, "%-12s%s [%u]", "vnc port: ",
             nm_vect_str_ctx(&vm.main, NM_SQL_VNC),
             nm_str_stoui(nm_vect_str(&vm.main, NM_SQL_VNC), 10) + 5900);

    /* {{{ print network interfaces info */
    ifs_count = vm.ifs.n_memb / NM_IFS_IDX_COUNT;

    for (size_t n = 0; n < ifs_count; n++)
    {
        size_t idx_shift = NM_IFS_IDX_COUNT * n;

        mvprintw(y++, x, "eth%zu%-8s%s [%s %s%s]",
                 n, ":",
                 nm_vect_str_ctx(&vm.ifs, NM_SQL_IF_NAME + idx_shift),
                 nm_vect_str_ctx(&vm.ifs, NM_SQL_IF_MAC + idx_shift),
                 nm_vect_str_ctx(&vm.ifs, NM_SQL_IF_DRV + idx_shift),
                 (nm_str_cmp_st(nm_vect_str(&vm.ifs, NM_SQL_IF_VHO + idx_shift),
                    NM_ENABLE) == NM_OK) ? "+vhost" : "");
    }
    /* }}} network */

    /* {{{ print drives info */
    drives_count = vm.drives.n_memb / NM_DRV_IDX_COUNT;

    for (size_t n = 0; n < drives_count; n++)
    {
        size_t idx_shift = NM_DRV_IDX_COUNT * n;
        int boot = 0;

        if (nm_str_cmp_st(nm_vect_str(&vm.drives, NM_SQL_DRV_BOOT + idx_shift),
                NM_ENABLE) == NM_OK)
        {
            boot = 1;
        }

        mvprintw(y++, x, "disk%zu%-7s%s [%sGb %s] %s", n, ":",
                 nm_vect_str_ctx(&vm.drives, NM_SQL_DRV_NAME + idx_shift),
                 nm_vect_str_ctx(&vm.drives, NM_SQL_DRV_SIZE + idx_shift),
                 nm_vect_str_ctx(&vm.drives, NM_SQL_DRV_TYPE + idx_shift),
                 boot ? "*" : "");
    }
    /* }}} drives */

    /* {{{ print 9pfs info */
    if (nm_str_cmp_st(nm_vect_str(&vm.main, NM_SQL_9FLG), "1") == NM_OK)
    {
        mvprintw(y++, x, "%-12s%s [%s]", "9pfs: ",
                 nm_vect_str_ctx(&vm.main, NM_SQL_9PTH),
                 nm_vect_str_ctx(&vm.main, NM_SQL_9ID));
    }
    /* }}} 9pfs */

    /* {{{ Generate guest boot settings info */
    if (nm_vect_str_len(&vm.main, NM_SQL_MACH))
        mvprintw(y++, x, "%-12s%s", "machine: ", nm_vect_str_ctx(&vm.main, NM_SQL_MACH));
    if (nm_vect_str_len(&vm.main, NM_SQL_BIOS))
        mvprintw(y++, x, "%-12s%s", "bios: ", nm_vect_str_ctx(&vm.main, NM_SQL_BIOS));
    if (nm_vect_str_len(&vm.main, NM_SQL_KERN))
        mvprintw(y++, x, "%-12s%s", "kernel: ", nm_vect_str_ctx(&vm.main, NM_SQL_KERN));
    if (nm_vect_str_len(&vm.main, NM_SQL_KAPP))
        mvprintw(y++, x, "%-12s%s", "cmdline: ", nm_vect_str_ctx(&vm.main, NM_SQL_KAPP));
    if (nm_vect_str_len(&vm.main, NM_SQL_INIT))
        mvprintw(y++, x, "%-12s%s", "initrd: ", nm_vect_str_ctx(&vm.main, NM_SQL_INIT));
    if (nm_vect_str_len(&vm.main, NM_SQL_TTY))
        mvprintw(y++, x, "%-12s%s", "tty: ", nm_vect_str_ctx(&vm.main, NM_SQL_TTY));
    if (nm_vect_str_len(&vm.main, NM_SQL_SOCK))
        mvprintw(y++, x, "%-12s%s", "socket: ", nm_vect_str_ctx(&vm.main, NM_SQL_SOCK));
    /* }}} boot settings */

    /* {{{ Print PID */
    {
        int fd;
        nm_str_t pid_path = NM_INIT_STR;
        nm_str_t qmp_path = NM_INIT_STR;
        ssize_t nread;
        struct stat qmp_info;
        char pid[10];
        int pid_num = 0;

        nm_str_copy(&pid_path, &nm_cfg_get()->vm_dir);
        nm_str_add_char(&pid_path, '/');
        nm_str_add_str(&pid_path, name);
        nm_str_copy(&qmp_path, &pid_path);
        nm_str_add_text(&pid_path, "/" NM_VM_PID_FILE);
        nm_str_add_text(&qmp_path, "/" NM_VM_QMP_FILE);

        if ((stat(qmp_path.data, &qmp_info) != -1) &&
            (fd = open(pid_path.data, O_RDONLY)) != -1)
        {
            if ((nread = read(fd, pid, sizeof(pid))) > 0)
            {
                y++;
                pid[nread - 1] = '\0';
                pid_num = atoi(pid);
                mvprintw(y++, x, "%-12s%d", "pid: ", pid_num);
            }
            close(fd);
        }

        nm_str_free(&pid_path);
        nm_str_free(&qmp_path);
    } /* }}} PID */

    /* {{{ Print host IP addresses for TAP ints */
    y++;
    for (size_t n = 0; n < ifs_count; n++)
    {
        size_t idx_shift = NM_IFS_IDX_COUNT * n;

        if (!nm_vect_str_len(&vm.ifs, NM_SQL_IF_IP4 + idx_shift))
            continue;

        mvprintw(y++, x, "%-12s%s [%s]", "Host IP: ",
            nm_vect_str_ctx(&vm.ifs, NM_SQL_IF_NAME + idx_shift),
            nm_vect_str_ctx(&vm.ifs, NM_SQL_IF_IP4 + idx_shift));

    } /* }}} host IP addr */

    nm_vmctl_free_data(&vm);

    refresh();
    getch();
}

void nm_print_help(nm_window_t *w)
{
    int curr_p = 1;
    const char *msg_p1[] = {
          "             nEMU v" NM_VERSION,
          "",
        _(" r - start vm"),
        _(" t - start vm in temporary mode"),
#if (NM_WITH_VNC_CLIENT)
        _(" c - connect to vm via vnc"),
#endif
        _(" p - powerdown vm"),
        _(" z - reset vm"),
        _(" f - force stop vm"),
        _(" d - delete vm"),
        _(" e - edit vm settings"),
        _(" i - edit network settings"),
        _(" a - add virtual disk"),
        _(" l - clone vm"),
        _(" b - edit boot settings"),
        "",
        _(" Page 1. \"->\" - next, \"<-\" - prev")
    };

    const char *msg_p2[] = {
          "             nEMU v" NM_VERSION,
          "",
        _(" s - take snapshot"),
        _(" x - revert snapshot"),
        _(" h - share host filesystem"),
        _(" k - kill vm process"),
        _(" m - show command"),
        _(" v - delete virtual disk"),
        _(" u - delete unused tap interfaces"),
          "",
          "",
          "",
          "",
          "",
#if (NM_WITH_VNC_CLIENT)
          "",
#endif
        _(" Page 2. \"->\" - next, \"<-\" - prev")
    };

    for (;;)
    {
        int ch;
        int x, y;
        const char **curr_page = NULL;
        size_t lines = 0;

        switch (curr_p) {
        case 1:
            curr_page = msg_p1;
            lines = nm_arr_len(msg_p1);
            break;
        case 2:
            curr_page = msg_p2;
            lines = nm_arr_len(msg_p2);
            break;
        default:
            nm_bug("%s, no such page: %d", __func__, curr_p);
        }

        box(w, 0, 0);
        getmaxyx(w, y, x);
        mvwaddch(w, 2, 0, ACS_LTEE);
        mvwhline(w, 2, 1, ACS_HLINE, x - 2);
        mvwaddch(w, 2, x - 1, ACS_RTEE);
        for (size_t n = 0, y = 1; n < lines; n++, y++)
            mvwprintw(w, y, 1, "%s", curr_page[n]);

        ch = wgetch(w);
        if (ch != KEY_LEFT && ch != KEY_RIGHT)
            break;

        switch (ch) {
        case KEY_LEFT:
            curr_p--;
            break;
        case KEY_RIGHT:
            curr_p++;
            break;
        }

        if (curr_p == 0)
            curr_p = 2;
        if (curr_p == 3)
            curr_p = 1;

        for (int i = 0; i <= y; i++)
            mvwhline(w, i, 0, ' ', x);
    }
}

void nm_print_nemu(void)
{
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

    nm_clear_screen();

    for (size_t n = 0; n < nm_arr_len(nemu); n++)
        mvprintw(n, 0, "%s", nemu[n]);

    getch();
}

void nm_print_title(const char *msg)
{
    int col;
    size_t msg_len;

    col = getmaxx(stdscr);
    msg_len = mbstowcs(NULL, msg, strlen(msg));

    nm_clear_screen();
    border(0,0,0,0,0,0,0,0);
    mvprintw(1, (col - msg_len) / 2, msg);
    refresh();
}

/* vim:set ts=4 sw=4 fdm=marker: */
