#include <fstream> // debug
#include <iostream> // debug
#include <cstring>
#include <algorithm>
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

QManager::VmInfoWindow::VmInfoWindow(const std::string &guest, const std::string &dbf, int height, int width,
  int starty, int startx) : TemplateWindow(height, width, starty, startx) {
    guest_ = guest;
    dbf_ = dbf;
    title_ = guest_ + " info";
}

void QManager::VmInfoWindow::Print() {
  clear();
  border(0,0,0,0,0,0,0,0);
  mvprintw(1, (col - title_.size())/2, title_.c_str());

  std::unique_ptr<QemuDb> db(new QemuDb(dbf_));

  sql_query = "select arch from vms where name='" + guest_ + "'";
  guest.arch = db->SelectQuery(sql_query);
  mvprintw(4, col/6, "%-12s%s", "arch: ", guest.arch[0].c_str());

  sql_query = "select smp from vms where name='" + guest_ + "'";
  guest.cpus = db->SelectQuery(sql_query);
  mvprintw(5, col/6, "%-12s%s", "cores: ", guest.cpus[0].c_str());

  sql_query = "select mem from vms where name='" + guest_ + "'";
  guest.memo = db->SelectQuery(sql_query);
  mvprintw(6, col/6, "%-12s%s %s", "memory: ", guest.memo[0].c_str(), "Mb");

  sql_query = "select kvm from vms where name='" + guest_ + "'";
  guest.kvmf = db->SelectQuery(sql_query);
  guest.kvmf[0] == "1" ? guest.kvmf[0] = "enabled" : guest.kvmf[0] = "disabled";
  mvprintw(7, col/6, "%-12s%s", "kvm: ", guest.kvmf[0].c_str());

  sql_query = "select usb from vms where name='" + guest_ + "'";
  guest.usbp = db->SelectQuery(sql_query);
  guest.usbp[0] == "1" ? guest.usbp[0] = "enabled" : guest.usbp[0] = "disabled";
  mvprintw(8, col/6, "%-12s%s", "usb: ", guest.usbp[0].c_str());

  sql_query = "select vnc from vms where name='" + guest_ + "'";
  guest.vncp = db->SelectQuery(sql_query);
  mvprintw(9, col/6, "%-12s%s", "vnc port: ", guest.vncp[0].c_str());

  sql_query = "select mac from vms where name='" + guest_ + "'";
  guest.ints = db->SelectQuery(sql_query);
  MapString ints = Gen_map_from_str(guest.ints[0]);

  // Generate guest inerfaces info
  uint32_t i = 0;
  uint32_t y = 9;
  for(auto &ifs : ints) {
    mvprintw(
      ++y, col/6, "%s%u%-8s%s %s%s%s", "eth", i++, ":", ifs.first.c_str(),
      "[", ifs.second.c_str(), "]"
    );
  }

  // Generate guest hd images info
  sql_query = "select hdd from vms where name='" + guest_ + "'";
  guest.disk = db->SelectQuery(sql_query);
  MapString disk = Gen_map_from_str(guest.disk[0]);

  char hdx = 'a';
  for(auto &hd : disk) {
    mvprintw(
      ++y, col/6, "%s%c%-9s%s %s%s%s", "hd", hdx++, ":", hd.first.c_str(),
      "[", hd.second.c_str(), "Gb]"
    );
  }

  getch();
  refresh();
  clear();
}

QManager::AddVmWindow::AddVmWindow(const std::string &dbf, const std::string &vmdir,
  int height, int width, int starty, int startx) : TemplateWindow(height, width, starty, startx) {
    dbf_ = dbf;
    vmdir_ = vmdir;

    field.resize(12);
}

void QManager::AddVmWindow::Delete_form() {
  unpost_form(form);
  free_form(form);
  for(size_t i = 0; i < field.size() - 1; ++i) {
    free_field(field[i]);
  }
}

void QManager::AddVmWindow::Draw_form() {
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
}

void QManager::AddVmWindow::Print() {
  char clvnc[128], ccpu[128], cmem[128], cfree[128];

  const char *YesNo[] = {
    "yes","no", NULL
  };

  try {
    VectorString q_arch(list_arch());
    MapString u_dev(list_usb());

    clear();
    border(0,0,0,0,0,0,0,0);
    mvprintw(1, 1, "F10 - finish, F2 - save");
    refresh();
    curs_set(1);

    std::unique_ptr<QemuDb> db(new QemuDb(dbf_));
    sql_query = "select vnc from lastval";
    v_last_vnc = db->SelectQuery(sql_query); // TODO: add check if null exeption
    sql_query = "select mac from lastval";
    v_last_mac = db->SelectQuery(sql_query); // TODO: add check if null exeption

    last_mac = std::stol(v_last_mac[0]);
    last_vnc = std::stoi(v_last_vnc[0]);
    last_vnc++;

    // Create fields
    for(size_t i = 0; i < field.size() - 1; ++i) {
      field[i] = new_field(1, 35, i*2, 1, 0, 0);
      set_field_back(field[i], A_UNDERLINE);
    }

    field[field.size() - 1] = NULL;

    init_pair(1, COLOR_BLACK, COLOR_WHITE);
    wbkgd(window, COLOR_PAIR(1));

    char **ArchList = new char *[q_arch.size() + 1];
    char **UdevList = new char *[u_dev.size() + 1];

    // Convert VectorString to *char
    for(size_t i = 0; i < q_arch.size(); ++i) {
      ArchList[i] = new char[q_arch[i].size() + 1];
      memcpy(ArchList[i], q_arch[i].c_str(), q_arch[i].size() + 1);
    }

    // Convert MapString to *char
    {
      int i = 0;
      for(auto &UList : u_dev) {
        UdevList[i] = new char[UList.first.size() +1];
        memcpy(UdevList[i], UList.first.c_str(), UList.first.size() + 1);
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

    for(size_t i = 0; i < q_arch.size(); ++i) {
      delete [] ArchList[i];
    }

    for(size_t i = 0; i < u_dev.size(); ++i) {
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

    form = new_form(&field[0]);
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

    Draw_form();

    // Get variables from form buffer
    guest.name.assign(trim_field_buffer(field_buffer(field[0], 0)));
    guest.arch.assign(trim_field_buffer(field_buffer(field[1], 0)));
    guest.cpus.assign(trim_field_buffer(field_buffer(field[2], 0)));
    guest.memo.assign(trim_field_buffer(field_buffer(field[3], 0)));
    guest.disk.assign(trim_field_buffer(field_buffer(field[4], 0)));
    guest.vncp.assign(trim_field_buffer(field_buffer(field[5], 0)));
    guest.kvmf.assign(trim_field_buffer(field_buffer(field[6], 0)));
    guest.path.assign(trim_field_buffer(field_buffer(field[7], 0)));
    guest.ints.assign(trim_field_buffer(field_buffer(field[8], 0)));
    guest.usbp.assign(trim_field_buffer(field_buffer(field[9], 0)));
    guest.usbd.assign(trim_field_buffer(field_buffer(field[10], 0)));

    // Check all the necessary parametrs are filled.
    if(guest.usbp == "yes") {
      for(size_t i = 0; i < 11; ++i) {
        if(! field_status(field[i])) {
          Delete_form();
          throw QMException("Must fill all params");
        }
      }
    }
    else {
      for(size_t i = 0; i < 9; ++i) {
        if(! field_status(field[i])) {
          Delete_form();
          throw QMException("Must fill all params");
        }
      }
    }

    // Check if guest name is already taken
    sql_query = "select id from vms where name='" + guest.name + "'";
    v_name = db->SelectQuery(sql_query);
    if(! v_name.empty()) {
      Delete_form();
      throw QMException("This name is already used");
    }

    // Create img file for guest
    guest_dir = vmdir_ + "/" + guest.name;
    create_guest_dir_cmd = "mkdir " + guest_dir + " >/dev/null 2>&1";
    create_img_cmd = "qemu-img create -f qcow2 " + guest_dir
    + "/" + guest.name + ".img " + guest.disk + "G > /dev/null 2>&1";

    cmd_exit_status = system(create_guest_dir_cmd.c_str());

    if(cmd_exit_status != 0) {
      Delete_form();
      throw QMException("Can't create guest dir");
    }

    cmd_exit_status = system(create_img_cmd.c_str());

    if(cmd_exit_status != 0) {
      Delete_form();
      throw QMException("Can't create img file");
    }

    // Generate mac address for interfaces
    ui_vm_ints = std::stoi(guest.ints);
    ifaces = gen_mac_addr(last_mac, ui_vm_ints, guest.name);

    //Get last mac address and put it into database
    itm = ifaces.end();
    --itm;
    s_last_mac = itm->second;

    its = std::remove(s_last_mac.begin(), s_last_mac.end(), ':');
    s_last_mac.erase(its, s_last_mac.end());

    last_mac = std::stol(s_last_mac, 0, 16);

    sql_query = "update lastval set mac='" + std::to_string(last_mac) + "'";
    db->ActionQuery(sql_query);

    // Update last vnc port in database
    sql_query = "update lastval set vnc='" + std::to_string(last_vnc) + "'";
    db->ActionQuery(sql_query);

    // Get usb dev ID from user choice
    if(guest.usbp == "yes") {
      guest.usbp = "1";
      guest.usbd = u_dev.at(guest.usbd);
    }
    else {
      guest.usbp = "0";
      guest.usbd = "none";
    }

    // Gen qemu img name and kvm status
    guest.disk = guest.name + ".img=" + guest.disk + ";";
    guest.kvmf == "yes" ? guest.kvmf = "1" : guest.kvmf = "0";

    // Convert interfaces from MapString to string
    guest.ints.clear();
    for(auto &ifs : ifaces) {
      guest.ints += ifs.first + "=" + ifs.second + ";";
    }

    // Add guest to database
    sql_query = "insert into vms("
    "name, mem, smp, hdd, kvm, vnc, mac, arch, iso, install, usb, usbid"
    ") values('"
    + guest.name + "', '" + guest.memo + "', '" + guest.cpus + "', '"
    + guest.disk + "', '" + guest.kvmf + "', '" + guest.vncp + "', '"
    + guest.ints + "', '" + guest.arch + "', '" + guest.path + "', '1', '"
    + guest.usbp + "', '" + guest.usbd + "')";

    db->ActionQuery(sql_query);

    // End
    Delete_form();
  }
  catch (QMException &err) {
  curs_set(0);
    PopupWarning Warn(err.what(), 3, 30, 4, 20);
    Warn.Init();
    Warn.Print(Warn.window);
    refresh();
  }
  curs_set(0);
}

QManager::EditVmWindow::EditVmWindow(
  const std::string &dbf, const std::string &vmdir, const std::string &vm_name,
  int height, int width, int starty, int startx
) : AddVmWindow(dbf, vmdir, height, width, starty, startx) {
    vm_name_ = vm_name;

    field.resize(7);
}

void QManager::EditVmWindow::Print() {
  char ccpu[128], cmem[128], cints[64]; 

  const char *YesNo[] = {
    "yes","no", NULL
  };

  try {
    MapString u_dev(list_usb());

    clear();
    border(0,0,0,0,0,0,0,0);
    mvprintw(1, 1, "F10 - finish, F2 - save");
    refresh();
    curs_set(1);

    // Get guest parametrs
    std::unique_ptr<QemuDb> db(new QemuDb(dbf_));
   
    sql_query = "select smp from vms where name='" + vm_name_ + "'";
    guest_old.cpus = db->SelectQuery(sql_query);

    sql_query = "select mem from vms where name='" + vm_name_ + "'";
    guest_old.memo = db->SelectQuery(sql_query);

    sql_query = "select kvm from vms where name='" + vm_name_ + "'";
    guest_old.kvmf = db->SelectQuery(sql_query);

    sql_query = "select usb from vms where name='" + vm_name_ + "'";
    guest_old.usbp = db->SelectQuery(sql_query);

    sql_query = "select mac from vms where name='" + vm_name_ + "'";
    guest_old.ints = db->SelectQuery(sql_query);
    
    // Get interface count
    ifaces = Gen_map_from_str(guest_old.ints[0]);
    ints_count = ifaces.size();

    // Create fields
    for(size_t i = 0; i < field.size() - 1; ++i) {
      field[i] = new_field(1, 35, (i+1)*2, 1, 0, 0);
      set_field_back(field[i], A_UNDERLINE);
    }

    field[field.size() - 1] = NULL;

    init_pair(1, COLOR_BLACK, COLOR_WHITE);
    wbkgd(window, COLOR_PAIR(1));

    char **UdevList = new char *[u_dev.size() + 1];

    // Convert MapString to *char
    {
      int i = 0;
      for(auto &UList : u_dev) {
        UdevList[i] = new char[UList.first.size() +1];
        memcpy(UdevList[i], UList.first.c_str(), UList.first.size() + 1);
        i++;
      }
    }

    UdevList[u_dev.size()] = NULL;

    set_field_type(field[0], TYPE_INTEGER, 0, 1, cpu_count());
    set_field_type(field[1], TYPE_INTEGER, 0, 64, total_memory());
    set_field_type(field[2], TYPE_ENUM, (char **)YesNo, false, false);
    set_field_type(field[3], TYPE_INTEGER, 0, 0, 64);
    set_field_type(field[4], TYPE_ENUM, (char **)YesNo, false, false);
    set_field_type(field[5], TYPE_ENUM, UdevList, false, false);

    for(size_t i = 0; i < u_dev.size(); ++i) {
      delete [] UdevList[i];
    }

    delete [] UdevList;

    snprintf(cints, sizeof(cints), "%u", ints_count);
    snprintf(ccpu, sizeof(ccpu), "%s%u%s", "CPU cores [1-", cpu_count(), "]");
    snprintf(cmem, sizeof(cmem), "%s%u%s", "Memory [64-", total_memory(), "]Mb");

    set_field_buffer(field[0], 0, guest_old.cpus[0].c_str());
    set_field_buffer(field[1], 0, guest_old.memo[0].c_str());
    set_field_buffer(field[3], 0, cints);

    if(guest_old.kvmf[0] == "1")
      set_field_buffer(field[2], 0, YesNo[0]);
    else
      set_field_buffer(field[2], 0, YesNo[1]);

    if(guest_old.usbp[0] == "1")
      set_field_buffer(field[4], 0, YesNo[0]);
    else
      set_field_buffer(field[4], 0, YesNo[1]);
       
    field_opts_off(field[5], O_STATIC);

    form = new_form(&field[0]);
    scale_form(form, &row, &col);
    set_form_win(form, window);
    set_form_sub(form, derwin(window, row, col, 2, 21));
    box(window, 0, 0);
    post_form(form);

    mvwaddstr(window, 2, 22, (vm_name_ + " settings:").c_str());
    mvwaddstr(window, 4, 2, ccpu);
    mvwaddstr(window, 6, 2, cmem);
    mvwaddstr(window, 8, 2, "KVM [yes/no]");
    mvwaddstr(window, 10, 2, "Interfaces");
    mvwaddstr(window, 12, 2, "USB [yes/no]");
    mvwaddstr(window, 14, 2, "USB device");

    Draw_form();

    // Get variables from form buffer
    guest_new.cpus.assign(trim_field_buffer(field_buffer(field[0], 0)));
    guest_new.memo.assign(trim_field_buffer(field_buffer(field[1], 0)));
    guest_new.kvmf.assign(trim_field_buffer(field_buffer(field[2], 0)));
    guest_new.ints.assign(trim_field_buffer(field_buffer(field[3], 0)));
    guest_new.usbp.assign(trim_field_buffer(field_buffer(field[4], 0)));
    guest_new.usbd.assign(trim_field_buffer(field_buffer(field[5], 0)));


    // Check all the necessary parametrs are filled.
    if(guest_new.usbp == "yes") {
      if(! field_status(field[5])) {
        Delete_form();
        throw QMException(_("Usb device was not selected."));
      }
    }
/*

    // Generate mac address for interfaces
    ui_vm_ints = std::stoi(guest.ints);
    ifaces = gen_mac_addr(last_mac, ui_vm_ints, guest.name);

    //Get last mac address and put it into database
    itm = ifaces.end();
    --itm;
    s_last_mac = itm->second;

    its = std::remove(s_last_mac.begin(), s_last_mac.end(), ':');
    s_last_mac.erase(its, s_last_mac.end());

    last_mac = std::stol(s_last_mac, 0, 16);

    sql_query = "update lastval set mac='" + std::to_string(last_mac) + "'";
    db->ActionQuery(sql_query);

    // Update last vnc port in database
    sql_query = "update lastval set vnc='" + std::to_string(last_vnc) + "'";
    db->ActionQuery(sql_query);

    // Get usb dev ID from user choice
    if(guest.usbp == "yes") {
      guest.usbp = "1";
      guest.usbd = u_dev.at(guest.usbd);
    }
    else {
      guest.usbp = "0";
      guest.usbd = "none";
    }

    // Gen qemu img name and kvm status
    guest.disk = guest.name + ".img=" + guest.disk + ";";
    guest.kvmf == "yes" ? guest.kvmf = "1" : guest.kvmf = "0";

    // Convert interfaces from MapString to string
    guest.ints.clear();
    for(auto &ifs : ifaces) {
      guest.ints += ifs.first + "=" + ifs.second + ";";
    }

    // Add guest to database
    sql_query = "insert into vms("
    "name, mem, smp, hdd, kvm, vnc, mac, arch, iso, install, usb, usbid"
    ") values('"
    + guest.name + "', '" + guest.memo + "', '" + guest.cpus + "', '"
    + guest.disk + "', '" + guest.kvmf + "', '" + guest.vncp + "', '"
    + guest.ints + "', '" + guest.arch + "', '" + guest.path + "', '1', '"
    + guest.usbp + "', '" + guest.usbd + "')";

    db->ActionQuery(sql_query);
*/
    // End
    Delete_form();
  }
  catch (QMException &err) {
  curs_set(0);
    PopupWarning Warn(err.what(), 3, 30, 4, 20);
    Warn.Init();
    Warn.Print(Warn.window);
    refresh();
  }
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
