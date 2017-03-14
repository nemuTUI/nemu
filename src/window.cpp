#include <cstring>
#include <algorithm>
#include <memory>
#include <array>
#include <thread>
#include <libintl.h>
#include <fcntl.h>

#include "window.h"
#include "guest.h"

namespace QManager {

const char *YesNo[3] = {
    "yes","no", NULL
};

const char *NetDrv[4] = {
    "virtio", "rtl8139", "e1000", NULL
};

const char *drive_ints[4] = {
    "ide", "scsi", "virtio", NULL
};

QMWindow::QMWindow(int height, int width, int starty)
{
    getmaxyx(stdscr, row, col);

    height_ = height;
    width_ = width;
    starty_ = starty;
    getmaxyx(stdscr, row, col);
    startx_ = (col - width) / 2;
}

void QMWindow::Init()
{
    start_color();
    use_default_colors();
    window = newwin(height_, width_, starty_, startx_);
    keypad(window, TRUE);
}

void MainWindow::Print()
{
    help_msg = _("Use arrow keys to go up and down, Press enter to select a choice, F10 - exit");
    str_size = mbstowcs(NULL, help_msg.c_str(), help_msg.size());

    clear();
    border(0,0,0,0,0,0,0,0);
    mvprintw(1, (col - str_size) / 2, help_msg.c_str());
    refresh();
}

void VmWindow::Print()
{
    help_msg = _("F1 - help, F10 - main menu ");
    str_size = mbstowcs(NULL, help_msg.c_str(), help_msg.size());

    border(0,0,0,0,0,0,0,0);
    mvprintw(1, (col - str_size) / 2, help_msg.c_str());
    refresh();
}

VmInfoWindow::VmInfoWindow(const std::string &guest, const std::string &dbf,
                           int height, int width,
                           int starty) : QMWindow(height, width, starty)
{
    guest_ = guest;
    dbf_ = dbf;
}

CmdInfoWindow::CmdInfoWindow(const std::string &guest,
                  int height, int width, int starty)
: QMWindow(height, width, starty), guest_(guest)
{
    title_ = _("running command for ") + guest_;
}

void CmdInfoWindow::Print()
{
    clear();
    const struct config *cfg = get_cfg();
    struct start_data start;

    start.flags |= START_FAKE;
    start_guest(guest_, cfg->db, cfg->vmdir, &start);
    mvprintw(1, (col - title_.size())/2, title_.c_str());
    mvprintw(3, 0, "%s", start.cmd.c_str());

    getch();
    refresh();
    clear();
}

void VmInfoWindow::Print()
{
    clear();
    border(0,0,0,0,0,0,0,0);
    mvprintw(1, (col - guest_.size())/2, guest_.c_str());
    std::unique_ptr<QemuDb> db(new QemuDb(dbf_));

    sql_query = "SELECT * FROM vms WHERE name='" + guest_ + "'";
    db->SelectQuery(sql_query, &guest_info);

    mvprintw(4, col/4, "%-12s%s", "arch: ", guest_info[SQL_IDX_ARCH].c_str());
    mvprintw(5, col/4, "%-12s%s", "cores: ", guest_info[SQL_IDX_SMP].c_str());
    mvprintw(6, col/4, "%-12s%s %s", "memory: ", guest_info[SQL_IDX_MEM].c_str(), "Mb");

    if (guest_info[SQL_IDX_KVM] == "1")
    {
        if (guest_info[SQL_IDX_HCPU] == "1")
            guest_info[SQL_IDX_KVM] = "enabled [+hostcpu]";
        else
            guest_info[SQL_IDX_KVM] = "enabled";
    }
    else
        guest_info[SQL_IDX_KVM] = "disabled";

    mvprintw(7, col/4, "%-12s%s", "kvm: ", guest_info[SQL_IDX_KVM].c_str());

    if (guest_info[SQL_IDX_USBF] == "1")
    {
        mvprintw(8, col/4, "%-12s%s [%s]", "usb: ", "enabled",
            guest_info[SQL_IDX_USBD].c_str());
    }
    else
        mvprintw(8, col/4, "%-12s%s", "usb: ", "disabled");

    mvprintw(9, col/4, "%-12s%s [%u]", "vnc port: ",
        guest_info[SQL_IDX_VNC].c_str(), std::stoi(guest_info[SQL_IDX_VNC]) + 5900);

    MapStringVector ints = Read_ifaces_from_json(guest_info[SQL_IDX_MAC]);

    // Generate guest inerfaces info
    uint32_t i = 0;
    uint32_t y = 9;
    
    for (auto &ifs : ints)
    {
        mvprintw(++y, col/4, "%s%u%-8s%s [%s] [%s]",
                 "eth", i++, ":", ifs.first.c_str(),
                 ifs.second[IFS_MAC].c_str(), ifs.second[IFS_DRV].c_str());
    }

    // Generate guest hd images info
    MapString disk = Gen_map_from_str(guest_info[SQL_IDX_HDD]);

    i = 0;

    for (auto &hd : disk)
    {
        mvprintw(++y, col/4, "%s%u%-7s%s %s%s%s", "disk",
                 i++, ":", hd.first.c_str(),
                 "[", hd.second.c_str(), "Gb]");
    }

    /* {{{ Generate guest boot settings info */
    if (!guest_info[SQL_IDX_BIOS].empty())
        mvprintw(++y, col/4, "%-12s%s", "bios: ", guest_info[SQL_IDX_BIOS].c_str());
    if (!guest_info[SQL_IDX_KERN].empty())
        mvprintw(++y, col/4, "%-12s%s", "kernel: ", guest_info[SQL_IDX_KERN].c_str());
    if (!guest_info[SQL_IDX_KAPP].empty())
        mvprintw(++y, col/4, "%-12s%s", "cmdline: ", guest_info[SQL_IDX_KAPP].c_str());
    if (!guest_info[SQL_IDX_TTY].empty())
        mvprintw(++y, col/4, "%-12s%s", "tty: ", guest_info[SQL_IDX_TTY].c_str());
    if (!guest_info[SQL_IDX_SOCK].empty())
        mvprintw(++y, col/4, "%-12s%s", "socket: ", guest_info[SQL_IDX_SOCK].c_str());
    /* }}} boot settings */

    /* {{{ Print PID */
    {
        int fd;
        const std::string pid_path = get_cfg()->vmdir + "/" + guest_ + "/qemu.pid";
        ssize_t nread;
        char pid[10];

        if ((fd = open(pid_path.c_str(), O_RDONLY)) != -1)
        {
            if ((nread = read(fd, pid, sizeof(pid))) > 0)
            {
                pid[nread - 1] = '\0';
                y += 2;
                mvprintw(y, col/4, "%-12s%s", "pid: ", pid);
            }
            close(fd);
        }
    } /* }}} PID */

    /* {{{ Print host IP addresses for TAP ints */
    y++;
    for (auto &ifs : ints)
    {
        if (ifs.second[IFS_IPV4].empty())
            continue;
        mvprintw(++y, col/4, "%-12s%s [%s]", "Host IP: ",
            ifs.first.c_str(), ifs.second[IFS_IPV4].c_str());
    } /* }}} */

    getch();
    refresh();
    clear();
}

void HelpWindow::Print()
{
    line = 1;
    window_ = newwin(height_, width_, starty_, startx_);
    keypad(window_, TRUE);
    box(window_, 0, 0);

    std::unique_ptr<VectorString> msg_(new VectorString); 

    msg_->push_back("           nEMU v" + std::string(VERSION));
    msg_->push_back("");
    msg_->push_back(_(" r - start guest"));
    msg_->push_back(_(" t - start guest in temporary mode"));
    msg_->push_back(_(" c - connect to guest via vnc"));
    msg_->push_back(_(" f - force stop guest"));
    msg_->push_back(_(" d - delete guest"));
    msg_->push_back(_(" e - edit guest settings"));
    msg_->push_back(_(" i - edit network settings"));
    msg_->push_back(_(" a - add virtual disk"));
    msg_->push_back(_(" l - clone guest"));
    msg_->push_back(_(" s - edit boot settings"));
    msg_->push_back(_(" m - show command"));
    msg_->push_back(_(" u - delete unused tap interfaces"));

    for (auto &msg : *msg_)
    {
        mvwprintw(window_, line, 1, "%s", msg.c_str());
        line++;
    }

    wrefresh(window_);
    wgetch(window_);
}

PopupWarning::PopupWarning(const std::string &msg, int height,
                           int width, int starty) : 
msg_(msg), height_(height), width_(width), starty_(starty)
{
    getmaxyx(stdscr, row, col);
    startx_ = (col - width) / 2;
}

void PopupWarning::Init()
{
    window = newwin(height_, width_, starty_, startx_);
    keypad(window, TRUE);
}

int PopupWarning::Print(WINDOW *window)
{
    window_ = window;
    box(window_, 0, 0);
    mvwprintw(window_, 1, 1, "%s", msg_.c_str());
    wrefresh(window_);
    ch_ = wgetch(window_);

    return ch_;
}

MenuList::MenuList(WINDOW *window, uint32_t &highlight)
{
    window_ = window;
    highlight_ = highlight;
}

VmList::VmList(WINDOW *menu_window, uint32_t &highlight, const std::string &vmdir)
      : MenuList(menu_window, highlight)
{
    vmdir_ = vmdir;
    init_pair(1, COLOR_WHITE, COLOR_BLACK);
    init_pair(2, COLOR_GREEN, COLOR_BLACK);
}

NemuWindow::NemuWindow(int height, int width, int starty)
: QMWindow(height, width, starty)
{
}

void NemuWindow::Print()
{
    int line = 0;
    VectorString nemu;

    nemu.push_back("            .x@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@o.           ");
    nemu.push_back("          .x@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@x.          ");
    nemu.push_back("         o@@@@@@@@@@@@@x@@@@@@@@@@@@@@@@@@@@@@@@@o         ");
    nemu.push_back("       .x@@@@@@@@x@@@@xx@@@@@@@@@@@@@@@@@@@@@@@@@@o        ");
    nemu.push_back("      .x@@@@@@@x. .x@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@.       ");
    nemu.push_back("      x@@@@@ox@x..o@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@.      ");
    nemu.push_back("     o@@@@@@xx@@@@@@@@@@@@@@@@@@@@@@@@x@@@@@@@@@@@@@o      ");
    nemu.push_back("    .x@@@@@@@@@@x@@@@@@@@@@@@@@@@@@@@@o@@@@@@@@@@@@@@.     ");
    nemu.push_back("    .@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@x.@@@@@@@@@@@@@@o     ");
    nemu.push_back("    o@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@xx..o@@@@@@@@@@@@@x     ");
    nemu.push_back("    x@@@@@@@@@@@@@@@@@@xxoooooooo........oxxx@@@@@@@@x.    ");
    nemu.push_back("    x@@@@@@@@@@x@@@@@@x. .oxxx@@xo.       .oxo.o@@@@@x     ");
    nemu.push_back("    o@@@@@@@@x..o@@@@@o .xxx@@@o ..      .x@ooox@@@@@o     ");
    nemu.push_back("    .x@@@@@@@oo..x@@@@.  ...oo..         .oo. o@@@@@o.     ");
    nemu.push_back("     .x@@@@@@xoooo@@@@.                       x@@@x..      ");
    nemu.push_back("       o@@@@@@ooxx@@@@.                 .    .@@@x..       ");
    nemu.push_back("        .o@@@@@@o.x@@@.                 ..   o@@x.         ");
    nemu.push_back("         .x@@@@@@xx@@@.                 ..  .x@o           ");
    nemu.push_back("         o@@@@@@@o.x@@.                    .x@x.           ");
    nemu.push_back("         o@@@@@@@o o@@@xo.         .....  .x@x.            ");
    nemu.push_back("          .@@@@@x. .x@o.x@x..           .o@@@.             ");
    nemu.push_back("         .x@@@@@o.. o@o  .o@@xoo..    .o..@@o              ");
    nemu.push_back("          o@@@@oo@@xx@x.   .x@@@ooooooo. o@x.              ");
    nemu.push_back("         .o@@o..xx@@@@@xo.. o@x.         xx.               ");
    nemu.push_back("          .oo. ....ox@@@@@@xx@o          xo.               ");
    nemu.push_back("          ....       .o@@@@@@@@o        .@..               ");
    nemu.push_back("           ...         ox.ox@@@.        .x..               ");
    nemu.push_back("            ...        .x. .@@x.        .o                 ");
    nemu.push_back("              .         .o .@@o.         o                 ");
    nemu.push_back("                         o. x@@o         .                 ");
    nemu.push_back("                         .o o@@..        .                 ");
    nemu.push_back("                          ...@o..        .                 ");
    nemu.push_back("                           .x@xo.                          ");
    nemu.push_back("                            .x..                           ");
    nemu.push_back("                           .x....                          ");
    nemu.push_back("                           o@..xo                          ");
    nemu.push_back("                           o@o oo                          ");
    nemu.push_back("                           ..                              ");

    clear();

    for (auto &n : nemu)
        mvprintw(line++, 0, "%s", n.c_str());

    getch();
    refresh();
    clear();
}

} // namespace QManager
/* vim:set ts=4 sw=4 fdm=marker: */
