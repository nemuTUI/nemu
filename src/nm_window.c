#include <nm_core.h>
#include <nm_string.h>
#include <nm_ncurses.h>
#include <nm_database.h>

#define NM_VM_MSG   "F1 - help, F10 - main menu "
#define NM_MAIN_MSG "Enter - select a choice, F10 - exit"

static void nm_print_window(const char *msg);

void nm_print_main_window(void)
{
    nm_print_window(_(NM_MAIN_MSG));
}

void nm_print_vm_window(void)
{
    nm_print_window(_(NM_VM_MSG));
}

void nm_print_warn(nm_window_t *w, const char *msg)
{
    box(w, 0, 0);
    mvwprintw(w, 1, 1, "%s", msg);
    wrefresh(w);
    wgetch(w);
}

void nm_print_vm_info(const nm_str_t *name)
{
    nm_vect_t vm = NM_INIT_VECT;
    nm_str_t sql = NM_INIT_STR;
    int y = 1;
    int col = getmaxx(stdscr);

    nm_str_add_text(&sql, "SELECT * FROM vms WHERE name='");
    nm_str_add_str(&sql, name);
    nm_str_add_char(&sql, '\'');

    nm_db_select(sql.data, &vm);
    nm_str_free(&sql);

    nm_clear_screen();
    border(0,0,0,0,0,0,0,0);

    mvprintw(y, (col - name->len) / 2, "%s", name->data);
    y += 3;
    mvprintw(y++, col / 4, "%-12s%s", "arch: ", nm_vect_str_ctx(&vm, NM_SQL_ARCH));
    mvprintw(y++, col / 4, "%-12s%s", "cores: ", nm_vect_str_ctx(&vm, NM_SQL_SMP));
    mvprintw(y++, col / 4, "%-12s%s %s", "memory: ", nm_vect_str_ctx(&vm, NM_SQL_MEM), "Mb");

    if (nm_str_cmp_st(nm_vect_str(&vm, NM_SQL_KVM), "1") == NM_OK)
    {
        if (nm_str_cmp_st(nm_vect_str(&vm, NM_SQL_HCPU), "1") == NM_OK)
            mvprintw(y++, col / 4, "%-12s%s", "kvm: ", "enabled [+hostcpu]");
        else
            mvprintw(y++, col / 4, "%-12s%s", "kvm: ", "enabled");
    }
    else
    {
        mvprintw(y++, col / 4, "%-12s%s", "kvm: ", "disabled");
    }

    if (nm_str_cmp_st(nm_vect_str(&vm, NM_SQL_USBF), "1") == NM_OK)
    {
        mvprintw(y++, col / 4, "%-12s%s [%s]", "usb: ", "enabled",
                 nm_vect_str_ctx(&vm, NM_SQL_USBD));
    }
    else
    {
        mvprintw(y++, col / 4, "%-12s%s", "usb: ", "disabled");
    }

    mvprintw(y++, col / 4, "%-12s%s [%u]", "vnc port: ",
             nm_vect_str_ctx(&vm, NM_SQL_VNC),
             nm_str_stoui(nm_vect_str(&vm, NM_SQL_VNC)) + 5900);

    nm_vect_free(&vm, nm_vect_free_str_cb);

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
        _(" s - edit boot settings"),
        _(" m - show command"),
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

static void nm_print_window(const char *msg)
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
