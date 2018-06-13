#include <nm_core.h>
#include <nm_utils.h>
#include <nm_string.h>
#include <nm_window.h>
#include <nm_ncurses.h>
#include <nm_database.h>
#include <nm_cfg_file.h>

#define NM_VM_MSG   "F1 - help, ESC - main menu "
#define NM_MAIN_MSG "Enter - select a choice, ESC - exit"

/* Fit VM properties in action window */
#define NM_PR_VM_INFO()                                   \
    do {                                                  \
        if (y > (rows - 3)) {                             \
            mvwprintw(action_window, y, x, "...");        \
            return;                                       \
        }                                                 \
        nm_align2line(&buf, cols);                        \
        mvwprintw(action_window, y++, x, "%s", buf.data); \
        nm_str_trunc(&buf, 0);                            \
    } while (0)

static void nm_init_window__(nm_window_t *w, const char *msg);

void nm_create_windows(void)
{
    int action_cols, screen_x, screen_y; 
    nm_cord_t help_size = NM_INIT_POS;
    nm_cord_t side_size = NM_INIT_POS;
    nm_cord_t action_size = NM_INIT_POS;

    getmaxyx(stdscr, screen_y, screen_x); 
    /* TODO read param [0.7] from cfg file */
    action_cols = screen_x * 0.7;

    help_size = NM_SET_POS(1, screen_x, 0, 0);
    side_size = NM_SET_POS(screen_y - 1, screen_x - action_cols, 0, 1);
    action_size = NM_SET_POS(screen_y - 1, action_cols, screen_x - action_cols, 1);

    help_window = nm_init_window(&help_size);
    side_window = nm_init_window(&side_size);
    action_window = nm_init_window(&action_size);
}

void nm_init_help(const char *msg, int err)
{
    wbkgd(help_window, COLOR_PAIR(err ? 4 : 1));
    mvwprintw(help_window, 0, 1, msg ? msg : _("q:Quit I:Install O:Import ?:Help"));
    wrefresh(help_window);
}

void nm_init_side(void)
{
    nm_init_window__(side_window, _("VM list"));
    wtimeout(side_window, 500);
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
    //w = nm_init_window(nlines, msg_len + 6, begin_x);
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

void nm_print_vm_info(const nm_str_t *name, const nm_vmctl_data_t *vm)
{
    nm_str_t buf = NM_INIT_STR;
    size_t y = 3, x = 2;
    size_t cols, rows;
    size_t ifs_count, drives_count;

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
        nm_str_format(&buf, "%-12s%s", "usb: ", "enabled");
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
                //y++;
                pid[nread - 1] = '\0';
                pid_num = atoi(pid);
                nm_str_format(&buf, "%-12s%d", "pid: ", pid_num);
                NM_PR_VM_INFO();
            }
            close(fd);
        }
        else /* clear PID file info */
        {
            mvwhline(action_window, y, 1, ' ', cols - 4);
        }

        nm_str_free(&pid_path);
        nm_str_free(&qmp_path);
    } /* }}} PID */

    nm_str_free(&buf);
}

void nm_print_help(void)
{
    size_t cols, rows;

    const char *keys[] = {
        "r", "t",
#if (NM_WITH_VNC_CLIENT)
        "c",
#endif
        "p", "z", "f", "d", "e", "i",
        "a", "l", "b", "s", "x", "h",
        "m", "v", "u", "P", "R",
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
#if (NM_WITH_VNC_CLIENT)
        "connect to vm via vnc",
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
        "take drive snapshot",
        "revert drive snapshot",
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
    size_t n = 0, last;
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
        nm_str_format(&help_title, _("Help [all]"));
    else
        nm_str_format(&help_title, _("Help [%d%%]") , perc);

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
                nm_str_trunc(&help_title, 0);
                if (last < hotkey_num)
                {
                    perc = 100 * last / hotkey_num;
                    nm_str_format(&help_title, _("Help [%d%%]") , perc);
                }
                else
                    nm_str_format(&help_title, _("Help [end]"));

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
        size_t mb_len = mbstowcs(NULL, *msg, strlen(*msg));
        len = (mb_len > len) ? mb_len : len;
        msg++;
    }

    return len;
}

void nm_warn(const char *msg)
{
    assert(msg != NULL);
    werase(help_window);
    nm_init_help(msg, NM_TRUE);
    wgetch(action_window);
    werase(help_window);
    nm_init_help(NULL, 0);
}

/* vim:set ts=4 sw=4 fdm=marker: */
