#include <cstring>
#include <algorithm>
#include <memory>
#include <array>
#include <thread>

#include "window.h"

namespace QManager {
  const char *YesNo[3] = {
    "yes","no", NULL
  };

  const char *NetDrv[4] = {
    "virtio", "rtl8139", "e1000", NULL
  };

  QMWindow::QMWindow(int height, int width, int starty) {
    getmaxyx(stdscr, row, col);

    height_ = height;
    width_ = width;
    starty_ = starty;
    getmaxyx(stdscr, row, col);
    startx_ = (col - width) / 2;
  }

  void QMWindow::Init() {
    start_color();
    use_default_colors();
    window = newwin(height_, width_, starty_, startx_);
    keypad(window, TRUE);
  }

  void MainWindow::Print() {
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

  VmInfoWindow::VmInfoWindow(const std::string &guest, const std::string &dbf, int height, int width,
    int starty) : QMWindow(height, width, starty)
  {
      guest_ = new std::string(guest);
      title_ = new std::string(*guest_ + _(" info"));
      dbf_ = new std::string(dbf);
  }

  void VmInfoWindow::Print()
  {
    clear();
    border(0,0,0,0,0,0,0,0);
    mvprintw(1, (col - title_->size())/2, title_->c_str());
    std::unique_ptr<QemuDb> db(new QemuDb(*dbf_));
    std::unique_ptr<guest_t<VectorString>> guest_info(new guest_t<VectorString>);
    std::unique_ptr<std::string> sql_query(new std::string);

    *sql_query = "select arch from vms where name='" + *guest_ + "'";
    guest_info->arch = db->SelectQuery(*sql_query);
    mvprintw(4, col/4, "%-12s%s", "arch: ", guest_info->arch[0].c_str());
    
    *sql_query = "select smp from vms where name='" + *guest_ + "'";
    guest_info->cpus = db->SelectQuery(*sql_query);
    mvprintw(5, col/4, "%-12s%s", "cores: ", guest_info->cpus[0].c_str());

    *sql_query = "select mem from vms where name='" + *guest_ + "'";
    guest_info->memo = db->SelectQuery(*sql_query);
    mvprintw(6, col/4, "%-12s%s %s", "memory: ", guest_info->memo[0].c_str(), "Mb");

    *sql_query = "select kvm from vms where name='" + *guest_ + "'";
    guest_info->kvmf = db->SelectQuery(*sql_query);
    *sql_query = "select hcpu from vms where name='" + *guest_ + "'";
    guest_info->hcpu = db->SelectQuery(*sql_query);

    if(guest_info->kvmf[0] == "1") {
      if(guest_info->hcpu[0] == "1")
        guest_info->kvmf[0] = "enabled [+hostcpu]";
      else
        guest_info->kvmf[0] = "enabled";
    }
    else
      guest_info->kvmf[0] = "disabled";

    mvprintw(7, col/4, "%-12s%s", "kvm: ", guest_info->kvmf[0].c_str());

    *sql_query = "select usb from vms where name='" + *guest_ + "'";
    guest_info->usbp = db->SelectQuery(*sql_query);
    guest_info->usbp[0] == "1" ? guest_info->usbp[0] = "enabled" : guest_info->usbp[0] = "disabled";
    mvprintw(8, col/4, "%-12s%s", "usb: ", guest_info->usbp[0].c_str());

    *sql_query = "select vnc from vms where name='" + *guest_ + "'";
    guest_info->vncp = db->SelectQuery(*sql_query);
    mvprintw(
      9, col/4, "%-12s%s [%u]", "vnc port: ",
      guest_info->vncp[0].c_str(), std::stoi(guest_info->vncp[0]) + 5900
    );

    *sql_query = "select mac from vms where name='" + *guest_ + "'";
    guest_info->ints = db->SelectQuery(*sql_query);
    MapStringVector ints = Read_ifaces_from_json(guest_info->ints[0]);

    // Generate guest inerfaces info
    uint32_t i = 0;
    uint32_t y = 9;
    for(auto &ifs : ints) {
      mvprintw(
        ++y, col/4, "%s%u%-8s%s [%s] [%s]", "eth", i++, ":", ifs.first.c_str(),
          ifs.second[0].c_str(), ifs.second[1].c_str()
      );
    }

    // Generate guest hd images info
    *sql_query = "select hdd from vms where name='" + *guest_ + "'";
    guest_info->disk = db->SelectQuery(*sql_query);
    MapString disk = Gen_map_from_str(guest_info->disk[0]);

    char hdx = 'a';
    for(auto &hd : disk) {
      mvprintw(
        ++y, col/4, "%s%c%-9s%s %s%s%s", "hd", hdx++, ":", hd.first.c_str(),
        "[", hd.second.c_str(), "Gb]"
      );
    }

    getch();
    refresh();
    clear();
    delete title_; delete guest_; delete dbf_;
  }
  
  void HelpWindow::Print()
  {
    line = 1;
    window_ = newwin(height_, width_, starty_, startx_);
    keypad(window_, TRUE);
    box(window_, 0, 0);

    std::unique_ptr<VectorString> msg_(new VectorString); 

    msg_->push_back("Qemu Manage v" + std::string(VERSION));
    msg_->push_back("");
    msg_->push_back(_("\"r\" - start guest"));
    msg_->push_back(_("\"c\" - connect to guest via vnc"));
    msg_->push_back(_("\"f\" - force stop guest"));
    msg_->push_back(_("\"d\" - delete guest"));
    msg_->push_back(_("\"e\" - edit guest settings"));
    msg_->push_back(_("\"i\" - edit network settings"));
    msg_->push_back(_("\"a\" - add virtual disk"));
    msg_->push_back(_("\"l\" - clone guest"));
    msg_->push_back(_("\"s\" - edit boot settings"));

    for(auto &msg : *msg_) {
      mvwprintw(window_, line, 1, "%s", msg.c_str());
      line++;
    }

    wrefresh(window_);
    wgetch(window_);
  }

  PopupWarning::PopupWarning(const std::string &msg, int height,
   int width, int starty) : height_(height), width_(width),
   starty_(starty), msg_(msg), window_(window) {
     getmaxyx(stdscr, row, col);
     startx_ = (col - width) / 2;
  }

  void PopupWarning::Init() {
    window = newwin(height_, width_, starty_, startx_);
    keypad(window, TRUE);
  }

  int PopupWarning::Print(WINDOW *window) {
    window_ = window;
    box(window_, 0, 0);
    mvwprintw(window_, 1, 1, "%s", msg_.c_str());
    wrefresh(window_);
    ch_ = wgetch(window_);

    return ch_;
  }

  MenuList::MenuList(WINDOW *window, uint32_t &highlight) {
    window_ = window;
    highlight_ = highlight;
  }

  VmList::VmList(WINDOW *menu_window, uint32_t &highlight, const std::string &vmdir)
          : MenuList(menu_window, highlight) {
    vmdir_ = vmdir;
    init_pair(1, COLOR_WHITE, COLOR_BLACK);
    init_pair(2, COLOR_GREEN, COLOR_BLACK);
  }

} // namespace QManager
