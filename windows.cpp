#include <fstream> // debug
#include <iostream> // debug
#include <cstring>
#include <memory>
#include "qemu_manage.h"

QManager::TemplateWindow::TemplateWindow(int height, int width, int starty, int startx) {
  height_ = height;
  width_ = width;
  starty_ = starty;
  startx_ = startx;

  getmaxyx(stdscr, row, col);
}

void QManager::TemplateWindow::Init() {
  start_color();
  use_default_colors();
  window = newwin(height_, width_, starty_, startx_);
  keypad(window, TRUE);
}

void QManager::MainWindow::Print() {
  clear();
  border(0,0,0,0,0,0,0,0);
  mvprintw(1, 1, "Use arrow keys to go up and down, Press enter to select a choice, F10 - exit");
  refresh();
}

void QManager::VmWindow::Print() {
  border(0,0,0,0,0,0,0,0);
  mvprintw(1, 1, "F1 - help, F10 - main menu");
  refresh();
}

QManager::VmInfoWindow::VmInfoWindow(const std::string &guest, int height, int width,
  int starty, int startx) : TemplateWindow(height, width, starty, startx) {
    guest_ = guest;
    title_ = guest_ + " info";
}

void QManager::VmInfoWindow::Print() {
  clear();
  border(0,0,0,0,0,0,0,0);
  mvprintw(1, (col - title_.size())/2, title_.c_str());
  getch();
  refresh();
  clear();
}

QManager::AddVmWindow::AddVmWindow(const std::string &dbf, const std::string &vmdir,
  int height, int width, int starty, int startx) : TemplateWindow(height, width, starty, startx) {
    dbf_ = dbf;
    vmdir_ = vmdir;
}

void QManager::AddVmWindow::Print() {
  sql_last_vnc = "select vnc from lastval";
  sql_last_mac = "select mac from lastval";

  char clvnc[128], ccpu[128], cmem[128], cfree[128];

  const char *YesNo[] = {
    "yes","no", NULL
  };

  VectorString q_arch(list_arch());
  MapString u_dev(list_usb());

  clear();
  border(0,0,0,0,0,0,0,0);
  mvprintw(1, 1, "F10 - finish, F2 - save");
  refresh();
  curs_set(1);

  std::unique_ptr<QemuDb> db(new QemuDb(dbf_));
  v_last_vnc = db->SelectQuery(sql_last_vnc);
  v_last_mac = db->SelectQuery(sql_last_mac);

  last_vnc = std::stoi(v_last_vnc[0]);
  last_vnc++;

  for(int i(0); i<11; ++i) {
    field[i] = new_field(1, 35, i*2, 1, 0, 0);
    set_field_back(field[i], A_UNDERLINE);
  }

  field[11] = NULL;

  init_pair(1, COLOR_BLACK, COLOR_WHITE);
  wbkgd(window, COLOR_PAIR(1));

  char **ArchList = new char *[q_arch.size() + 1];
  char **UdevList = new char *[u_dev.size() + 1];

  for(uint32_t i(0); i < q_arch.size(); ++i) {
    ArchList[i] = new char[q_arch[i].size() + 1];
    memcpy(ArchList[i], q_arch[i].c_str(), q_arch[i].size() + 1);
  }

  {
    int i(0);
    for(auto &UList : u_dev) {
      UdevList[i] = new char[UList.second.size() +1];
      memcpy(UdevList[i], UList.second.c_str(), UList.second.size() + 1);
      i++;
    }
  }

  ArchList[q_arch.size()] = NULL;
  UdevList[u_dev.size()] = NULL;

  set_field_type(field[0], TYPE_ALNUM, 0);
  set_field_type(field[1], TYPE_ENUM, ArchList, false, false);
  set_field_type(field[2], TYPE_INTEGER, 0, 1, cpu_count());
  set_field_type(field[3], TYPE_INTEGER, 0, 64, total_memory());
  set_field_type(field[4], TYPE_INTEGER, 0, 1, disk_free(vmdir_));
  set_field_type(field[5], TYPE_INTEGER, 0, 0, 65535);
  set_field_type(field[6], TYPE_ENUM, (char **)YesNo, false, false);
  set_field_type(field[7], TYPE_REGEXP, "^/.*");
  set_field_type(field[8], TYPE_INTEGER, 0, 0, 64);
  set_field_type(field[9], TYPE_ENUM, (char **)YesNo, false, false);
  set_field_type(field[10], TYPE_ENUM, UdevList, false, false);

  for(uint32_t i(0); i < q_arch.size(); ++i) {
    delete [] ArchList[i];
  }

  for(uint32_t i(0); i < u_dev.size(); ++i) {
    delete [] UdevList[i];
  }

  delete [] ArchList;
  delete [] UdevList;

  snprintf(clvnc, sizeof(clvnc), "%u", last_vnc);
  snprintf(ccpu, sizeof(ccpu), "%s%u%s", "CPU cores [1-", cpu_count(), "]");
  snprintf(cmem, sizeof(cmem), "%s%u%s", "Memory [64-", total_memory(), "]Mb");
  snprintf(cfree, sizeof(cfree), "%s%u%s", "Disk [1-", disk_free(vmdir_), "]Gb");

  set_field_buffer(field[2], 0, "1");
  set_field_buffer(field[5], 0, clvnc);
  set_field_buffer(field[8], 0, "1");
  field_opts_off(field[0], O_STATIC);
  field_opts_off(field[7], O_STATIC);
  field_opts_off(field[10], O_STATIC);
  field_opts_off(field[5], O_EDIT);
  set_max_field(field[0], 30);

  form = new_form(field);
  scale_form(form, &row, &col);
  set_form_win(form, window);
  set_form_sub(form, derwin(window, row, col, 2, 21));
  box(window, 0, 0);
  post_form(form);

  mvwaddstr(window, 2, 2, "Name");
  mvwaddstr(window, 4, 2, "Architecture");
  mvwaddstr(window, 6, 2, ccpu);
  mvwaddstr(window, 8, 2, cmem);
  mvwaddstr(window, 10, 2, cfree);
  mvwaddstr(window, 12, 2, "VNC port [ro]");
  mvwaddstr(window, 14, 2, "KVM [yes/no]");
  mvwaddstr(window, 16, 2, "Path to ISO");
  mvwaddstr(window, 18, 2, "Interfaces");
  mvwaddstr(window, 20, 2, "USB [yes/no]");
  mvwaddstr(window, 22, 2, "USB device");

  while((ch = wgetch(window)) != KEY_F(10)) {
    switch(ch) {
      case KEY_DOWN:
        form_driver(form, REQ_VALIDATION);
        form_driver(form, REQ_NEXT_FIELD);
        form_driver(form, REQ_END_LINE);
        break;

      case KEY_UP:
        form_driver(form, REQ_VALIDATION);
        form_driver(form, REQ_PREV_FIELD);
        form_driver(form, REQ_END_LINE);
        break;

      case KEY_LEFT:
        if(field_type(current_field(form)) == TYPE_ENUM)
          form_driver(form, REQ_PREV_CHOICE);
        else
          form_driver(form, REQ_PREV_CHAR);
        break;

      case KEY_RIGHT:
        if(field_type(current_field(form)) == TYPE_ENUM)
          form_driver(form, REQ_NEXT_CHOICE);
        else
          form_driver(form, REQ_NEXT_CHAR);
        break;

      case KEY_BACKSPACE:
        form_driver(form, REQ_DEL_PREV);
        break;

      case KEY_F(2):
        form_driver(form, REQ_VALIDATION);
        break;

      default:
        form_driver(form, ch);
        break;
    }
  }

  vm_name.assign(trim_field_buffer(field_buffer(field[0], 0)));
  vm_arch.assign(trim_field_buffer(field_buffer(field[1], 0)));
  vm_cpus.assign(trim_field_buffer(field_buffer(field[2], 0)));
  vm_memo.assign(trim_field_buffer(field_buffer(field[3], 0)));
  vm_disk.assign(trim_field_buffer(field_buffer(field[4], 0)));
  vm_vncp.assign(trim_field_buffer(field_buffer(field[5], 0)));
  vm_kvmf.assign(trim_field_buffer(field_buffer(field[6], 0)));
  vm_path.assign(trim_field_buffer(field_buffer(field[7], 0)));
  vm_ints.assign(trim_field_buffer(field_buffer(field[8], 0)));
  vm_usbp.assign(trim_field_buffer(field_buffer(field[9], 0)));
  vm_usbd.assign(trim_field_buffer(field_buffer(field[10], 0)));

  if(vm_usbp == "yes") {
    exit(12);
  }
  
  // debug
  std::ofstream debug;
  debug.open("/tmp/ddd");
  debug << vm_name << std::endl;
  debug << vm_arch << std::endl;
  debug << vm_cpus << std::endl;
  debug << vm_memo << std::endl;
  debug << vm_disk << std::endl;
  debug << vm_vncp << std::endl;
  debug << vm_kvmf << std::endl;
  debug << vm_path << std::endl;
  debug << vm_usbp << std::endl;
  debug << vm_usbd << std::endl;
  debug.close();
  // debug end

  unpost_form(form);
  free_form(form);
  for(int i(0); i<11; ++i) {
    free_field(field[i]);
  }

 // endwin();
  curs_set(0);
}

void QManager::HelpWindow::Print() {
  line = 1;
  window_ = newwin(height_, width_, starty_, startx_);
  keypad(window_, TRUE);
  box(window_, 0, 0);

  msg_.push_back("\"r\" - start guest");
  msg_.push_back("\"c\" - connect to guest via vnc");
  msg_.push_back("\"f\" - force stop guest");
  msg_.push_back("\"d\" - delete guest");
  msg_.push_back("\"e\" - edit guest settings");
  msg_.push_back("\"l\" - clone guest");

  for(auto &msg : msg_) {
    mvwprintw(window_, line, 1, "%s", msg.c_str());
    line++;
  }

  wrefresh(window_);
  wgetch(window_);
}

QManager::PopupWarning::PopupWarning(const std::string &msg, int height,
 int width, int starty, int startx) : TemplateWindow(height, width, starty, startx) {
    msg_ = msg;
}

int QManager::PopupWarning::Print(WINDOW *window) {
  window_ = window;
  box(window_, 0, 0);
  mvwprintw(window_, 1, 1, "%s", msg_.c_str());
  wrefresh(window_);
  ch_ = wgetch(window_);

  return ch_;
}

QManager::MenuList::MenuList(WINDOW *window, uint32_t &highlight) {
  window_ = window;
  highlight_ = highlight;
}

QManager::VmList::VmList(WINDOW *menu_window, uint32_t &highlight, const std::string &vmdir)
        : MenuList(menu_window, highlight) {
  vmdir_ = vmdir;
  init_pair(1, COLOR_WHITE, COLOR_BLACK);
  init_pair(2, COLOR_GREEN, COLOR_BLACK);
}
