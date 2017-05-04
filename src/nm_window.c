#include <nm_core.h>
#include <nm_string.h>
#include <nm_window.h>
#include <nm_ncurses.h>
#include <nm_database.h>
#include <nm_cfg_file.h>
#include <nm_vm_control.h>

#define NM_VM_MSG   "F1 - help, F10 - main menu "
#define NM_MAIN_MSG "Enter - select a choice, F10 - exit"

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
    nm_vmctl_gen_cmd(&cmd, &vm, name, NM_VMCTL_INFO);

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
    int y = 1;
    int col = getmaxx(stdscr);
    size_t ifs_count, drives_count;

    nm_vmctl_get_data(name, &vm);

    nm_clear_screen();
    border(0,0,0,0,0,0,0,0);

    mvprintw(y, (col - name->len) / 2, "%s", name->data);
    y += 3;
    mvprintw(y++, col / 6, "%-12s%s", "arch: ", nm_vect_str_ctx(&vm.main, NM_SQL_ARCH));
    mvprintw(y++, col / 6, "%-12s%s", "cores: ", nm_vect_str_ctx(&vm.main, NM_SQL_SMP));
    mvprintw(y++, col / 6, "%-12s%s %s", "memory: ", nm_vect_str_ctx(&vm.main, NM_SQL_MEM), "Mb");

    if (nm_str_cmp_st(nm_vect_str(&vm.main, NM_SQL_KVM), NM_ENABLE) == NM_OK)
    {
        if (nm_str_cmp_st(nm_vect_str(&vm.main, NM_SQL_HCPU), NM_ENABLE) == NM_OK)
            mvprintw(y++, col / 6, "%-12s%s", "kvm: ", "enabled [+hostcpu]");
        else
            mvprintw(y++, col / 6, "%-12s%s", "kvm: ", "enabled");
    }
    else
    {
        mvprintw(y++, col / 6, "%-12s%s", "kvm: ", "disabled");
    }

    if (nm_str_cmp_st(nm_vect_str(&vm.main, NM_SQL_USBF), "1") == NM_OK)
    {
        mvprintw(y++, col / 6, "%-12s%s [%s]", "usb: ", "enabled",
                 nm_vect_str_ctx(&vm.main, NM_SQL_USBD));
    }
    else
    {
        mvprintw(y++, col / 6, "%-12s%s", "usb: ", "disabled");
    }

    mvprintw(y++, col / 6, "%-12s%s [%u]", "vnc port: ",
             nm_vect_str_ctx(&vm.main, NM_SQL_VNC),
             nm_str_stoui(nm_vect_str(&vm.main, NM_SQL_VNC)) + 5900);

    /* {{{ print network interfaces info */
    ifs_count = vm.ifs.n_memb / 4;

    for (size_t n = 0; n < ifs_count; n++)
    {
        size_t idx_shift = 4 * n;

        mvprintw(y++, col / 6 , "eth%zu%-8s%s [%s] [%s]",
                 n, ":",
                 nm_vect_str_ctx(&vm.ifs, NM_SQL_IF_NAME + idx_shift),
                 nm_vect_str_ctx(&vm.ifs, NM_SQL_IF_MAC + idx_shift),
                 nm_vect_str_ctx(&vm.ifs, NM_SQL_IF_DRV + idx_shift));
    }
    /* }}} network */

    /* {{{ print drives info */
    drives_count = vm.drives.n_memb / 4;

    for (size_t n = 0; n < drives_count; n++)
    {
        size_t idx_shift = 4 * n;
        int boot = 0;

        if (nm_str_cmp_st(nm_vect_str(&vm.drives, NM_SQL_DRV_BOOT + idx_shift),
                NM_ENABLE) == NM_OK)
        {
            boot = 1;
        }

        mvprintw(y++, col / 6, "disk%zu%-7s%s [%sGb] [%s] %s", n, ":",
                 nm_vect_str_ctx(&vm.drives, NM_SQL_DRV_NAME + idx_shift),
                 nm_vect_str_ctx(&vm.drives, NM_SQL_DRV_SIZE + idx_shift),
                 nm_vect_str_ctx(&vm.drives, NM_SQL_DRV_TYPE + idx_shift),
                 boot ? "*" : "");
    }
    /* }}} drives */

    /* {{{ Generate guest boot settings info */
    if (nm_vect_str_len(&vm.main, NM_SQL_MACH))
        mvprintw(y++, col / 6, "%-12s%s", "machine: ", nm_vect_str_ctx(&vm.main, NM_SQL_MACH));
    if (nm_vect_str_len(&vm.main, NM_SQL_BIOS))
        mvprintw(y++, col / 6, "%-12s%s", "bios: ", nm_vect_str_ctx(&vm.main, NM_SQL_BIOS));
    if (nm_vect_str_len(&vm.main, NM_SQL_KERN))
        mvprintw(y++, col / 6, "%-12s%s", "kernel: ", nm_vect_str_ctx(&vm.main, NM_SQL_KERN));
    if (nm_vect_str_len(&vm.main, NM_SQL_KAPP))
        mvprintw(y++, col / 6, "%-12s%s", "cmdline: ", nm_vect_str_ctx(&vm.main, NM_SQL_KAPP));
    if (nm_vect_str_len(&vm.main, NM_SQL_INIT))
        mvprintw(y++, col / 6, "%-12s%s", "initrd: ", nm_vect_str_ctx(&vm.main, NM_SQL_INIT));
    if (nm_vect_str_len(&vm.main, NM_SQL_TTY))
        mvprintw(y++, col / 6, "%-12s%s", "tty: ", nm_vect_str_ctx(&vm.main, NM_SQL_TTY));
    if (nm_vect_str_len(&vm.main, NM_SQL_SOCK))
        mvprintw(y++, col / 6, "%-12s%s", "socket: ", nm_vect_str_ctx(&vm.main, NM_SQL_SOCK));
    /* }}} boot settings */

    /* {{{ Print PID */
    {
        int fd;
        nm_str_t pid_path = NM_INIT_STR;
        ssize_t nread;
        char pid[10];

        nm_str_copy(&pid_path, &nm_cfg_get()->vm_dir);
        nm_str_add_char(&pid_path, '/');
        nm_str_add_str(&pid_path, name);
        nm_str_add_text(&pid_path, "/" NM_VM_PID_FILE);

        if ((fd = open(pid_path.data, O_RDONLY)) != -1)
        {
            if ((nread = read(fd, pid, sizeof(pid))) > 0)
            {
                y++;
                pid[nread - 1] = '\0';
                mvprintw(y++, col / 6, "%-12s%s", "pid: ", pid);
            }
            close(fd);
        }

        nm_str_free(&pid_path);
    } /* }}} PID */

    /* {{{ Print host IP addresses for TAP ints */
    y++;
    for (size_t n = 0; n < ifs_count; n++)
    {
        size_t idx_shift = 4 * n;

        if (!nm_vect_str_len(&vm.ifs, NM_SQL_IF_IP4 + idx_shift))
            continue;

        mvprintw(y++, col / 6, "%-12s%s [%s]", "Host IP: ",
            nm_vect_str_ctx(&vm.ifs, NM_SQL_IF_NAME + idx_shift),
            nm_vect_str_ctx(&vm.ifs, NM_SQL_IF_IP4 + idx_shift));

    } /* }}} host IP addr */

    nm_vmctl_free_data(&vm);

    refresh();
    getch();
}

void nm_print_help(nm_window_t *w)
{
    const char *msg[] = {
          "             nEMU v" NM_VERSION,
          "",
        _(" r - start guest"),
        _(" t - start guest in temporary mode"),
#if (NM_WITH_VNC_CLIENT)
        _(" c - connect to guest via vnc"),
#endif
        _(" f - force stop guest"),
        _(" d - delete guest"),
        _(" e - edit guest settings"),
        _(" i - edit network settings"),
        _(" a - add virtual disk"),
        _(" l - clone guest"),
        _(" b - edit boot settings"),
        _(" m - show command"),
        _(" s - take snapshot"),
        _(" x - manage snapshots"),
        _(" u - delete unused tap interfaces")
    };

    box(w, 0, 0);

    for (size_t n = 0, y = 1; n < nm_arr_len(msg); n++, y++)
        mvwprintw(w, y, 1, "%s", msg[n]);

    wgetch(w);
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
