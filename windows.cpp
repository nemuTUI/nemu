#include <cstring>
#include <algorithm>
#include <memory>
#include <array>
#include <thread>
#include <unordered_set>

#include "qemu_manage.h"

static const char *YesNo[3] = {
  "yes","no", NULL
};

static const char *NetDrv[4] = {
  "virtio", "rtl8139", "e1000", NULL
};

static const QManager::VectorString q_arch = QManager::list_arch();

QManager::TemplateWindow::TemplateWindow(int height, int width, int starty) {
  getmaxyx(stdscr, row, col);

  height_ = height;
  width_ = width;
  starty_ = starty;
  getmaxyx(stdscr, row, col);
  startx_ = (col - width) / 2;
}

void QManager::TemplateWindow::Init() {
  start_color();
  use_default_colors();
  window = newwin(height_, width_, starty_, startx_);
  keypad(window, TRUE);
}

void QManager::MainWindow::Print() {
  help_msg = _("Use arrow keys to go up and down, Press enter to select a choice, F10 - exit");
  str_size = mbstowcs(NULL, help_msg.c_str(), help_msg.size());

  clear();
  border(0,0,0,0,0,0,0,0);
  mvprintw(1, (col - str_size) / 2, help_msg.c_str());
  refresh();
}

void QManager::VmWindow::Print() {
  help_msg = _("F1 - help, F10 - main menu ");
  str_size = mbstowcs(NULL, help_msg.c_str(), help_msg.size());

  border(0,0,0,0,0,0,0,0);
  mvprintw(1, (col - str_size) / 2, help_msg.c_str());
  refresh();
}

QManager::VmInfoWindow::VmInfoWindow(const std::string &guest, const std::string &dbf, int height, int width,
  int starty) : TemplateWindow(height, width, starty) {
    guest_ = guest;
    dbf_ = dbf;
    title_ = guest_ + _(" info");
}

void QManager::VmInfoWindow::Print() {
  clear();
  border(0,0,0,0,0,0,0,0);
  mvprintw(1, (col - title_.size())/2, title_.c_str());
  // TODO Draw guest info in window.
  std::unique_ptr<QemuDb> db(new QemuDb(dbf_));

  sql_query = "select arch from vms where name='" + guest_ + "'";
  guest.arch = db->SelectQuery(sql_query);
  mvprintw(4, col/4, "%-12s%s", "arch: ", guest.arch[0].c_str());

  sql_query = "select smp from vms where name='" + guest_ + "'";
  guest.cpus = db->SelectQuery(sql_query);
  mvprintw(5, col/4, "%-12s%s", "cores: ", guest.cpus[0].c_str());

  sql_query = "select mem from vms where name='" + guest_ + "'";
  guest.memo = db->SelectQuery(sql_query);
  mvprintw(6, col/4, "%-12s%s %s", "memory: ", guest.memo[0].c_str(), "Mb");

  sql_query = "select kvm from vms where name='" + guest_ + "'";
  guest.kvmf = db->SelectQuery(sql_query);
  guest.kvmf[0] == "1" ? guest.kvmf[0] = "enabled" : guest.kvmf[0] = "disabled";
  mvprintw(7, col/4, "%-12s%s", "kvm: ", guest.kvmf[0].c_str());

  sql_query = "select usb from vms where name='" + guest_ + "'";
  guest.usbp = db->SelectQuery(sql_query);
  guest.usbp[0] == "1" ? guest.usbp[0] = "enabled" : guest.usbp[0] = "disabled";
  mvprintw(8, col/4, "%-12s%s", "usb: ", guest.usbp[0].c_str());

  sql_query = "select vnc from vms where name='" + guest_ + "'";
  guest.vncp = db->SelectQuery(sql_query);
  mvprintw(
    9, col/4, "%-12s%s [%u]", "vnc port: ",
    guest.vncp[0].c_str(), std::stoi(guest.vncp[0]) + 5900
  );

  sql_query = "select mac from vms where name='" + guest_ + "'";
  guest.ints = db->SelectQuery(sql_query);
  MapStringVector ints = Read_ifaces_from_json(guest.ints[0]);

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
  sql_query = "select hdd from vms where name='" + guest_ + "'";
  guest.disk = db->SelectQuery(sql_query);
  MapString disk = Gen_map_from_str(guest.disk[0]);

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
}

QManager::AddVmWindow::AddVmWindow(const std::string &dbf, const std::string &vmdir,
  int height, int width, int starty) : TemplateWindow(height, width, starty) {
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

void QManager::AddVmWindow::Draw_title() {
  help_msg = _("F10 - finish, F2 - save");
  str_size = mbstowcs(NULL, help_msg.c_str(), help_msg.size());

  clear();
  border(0,0,0,0,0,0,0,0);
  mvprintw(1, (col - str_size) / 2, help_msg.c_str());
  refresh();
  curs_set(1);
}

void QManager::AddVmWindow::Post_form(uint32_t size) {
  form = new_form(&field[0]);
  scale_form(form, &row, &col);
  set_form_win(form, window);
  set_form_sub(form, derwin(window, row, col, 2, size));
  box(window, 0, 0);
  post_form(form);
}

void QManager::AddVmWindow::ExeptionExit(QMException &err) {
  curs_set(0);
  PopupWarning Warn(err.what(), 3, 30, 4);
  Warn.Init();
  Warn.Print(Warn.window);
  refresh();
}

void QManager::AddVmWindow::Enable_color() {
  init_pair(1, COLOR_BLACK, COLOR_WHITE);
  wbkgd(window, COLOR_PAIR(1));
}

void QManager::AddVmWindow::Gen_mac_address(
  struct guest_t<std::string> &guest, uint32_t int_count, std::string vm_name
) {
  last_mac = std::stol(v_last_mac[0]);
  ifaces = gen_mac_addr(last_mac, int_count, vm_name);

  itm = ifaces.end();
  --itm;
  s_last_mac = itm->second;

  its = std::remove(s_last_mac.begin(), s_last_mac.end(), ':');
  s_last_mac.erase(its, s_last_mac.end());

  last_mac = std::stol(s_last_mac, 0, 16);
}

void QManager::AddVmWindow::Gen_iface_json() {
  guest.ints.clear();
  for(auto &ifs : ifaces) {
    guest.ints += "{\"name\":\"" + ifs.first + "\",\"mac\":\"" +
      ifs.second + "\",\"drv\":\"" + guest.ndrv + "\"},";
  }

  guest.ints.erase(guest.ints.find_last_not_of(",") + 1);
  guest.ints = "{\"ifaces\":[" + guest.ints + "]}";
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

void QManager::AddVmWindow::Create_fields() {
  for(size_t i = 0; i < field.size() - 1; ++i) {
    field[i] = new_field(1, 35, i*2, 1, 0, 0);
    set_field_back(field[i], A_UNDERLINE);
  }

  field[field.size() - 1] = NULL;
}

void QManager::AddVmWindow::Config_fields_type() {
  ArchList = new char *[q_arch.size() + 1];
  UdevList = new char *[u_dev.size() + 1];

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
  set_field_type(field[5], TYPE_ENUM, (char **)YesNo, false, false);
  set_field_type(field[6], TYPE_REGEXP, "^/.*");
  set_field_type(field[7], TYPE_INTEGER, 0, 0, 64);
  set_field_type(field[8], TYPE_ENUM, (char **)NetDrv, false, false);
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
}

void QManager::AddVmWindow::Config_fields_buffer() {
  set_field_buffer(field[2], 0, "1");
  set_field_buffer(field[5], 0, "yes");
  set_field_buffer(field[7], 0, "1");
  set_field_buffer(field[8], 0, DEFAULT_NETDRV);
  set_field_buffer(field[9], 0, "no");
  field_opts_off(field[0], O_STATIC);
  field_opts_off(field[6], O_STATIC);
  field_opts_off(field[10], O_STATIC);
  set_max_field(field[0], 30);
}

void QManager::AddVmWindow::Print_fields_names() {
  char ccpu[128], cmem[128], cfree[128];
  snprintf(ccpu, sizeof(ccpu), "%s%u%s", _("CPU cores [1-"), cpu_count(), "]");
  snprintf(cmem, sizeof(cmem), "%s%u%s", _("Memory [64-"), total_memory(), "]Mb");
  snprintf(cfree, sizeof(cfree), "%s%u%s", _("Disk [1-"), disk_free(vmdir_), "]Gb");

  mvwaddstr(window, 2, 2, _("Name"));
  mvwaddstr(window, 4, 2, _("Architecture"));
  mvwaddstr(window, 6, 2, ccpu);
  mvwaddstr(window, 8, 2, cmem);
  mvwaddstr(window, 10, 2, cfree);
  mvwaddstr(window, 12, 2, _("KVM [yes/no]"));
  mvwaddstr(window, 14, 2, _("Path to ISO"));
  mvwaddstr(window, 16, 2, _("Interfaces"));
  mvwaddstr(window, 18, 2, _("Net driver"));
  mvwaddstr(window, 20, 2, _("USB [yes/no]"));
  mvwaddstr(window, 22, 2, _("USB device"));
}

void QManager::AddVmWindow::Get_data_from_form() {
  guest.name.assign(trim_field_buffer(field_buffer(field[0], 0)));
  guest.arch.assign(trim_field_buffer(field_buffer(field[1], 0)));
  guest.cpus.assign(trim_field_buffer(field_buffer(field[2], 0)));
  guest.memo.assign(trim_field_buffer(field_buffer(field[3], 0)));
  guest.disk.assign(trim_field_buffer(field_buffer(field[4], 0)));
  guest.vncp.assign(v_last_vnc[0]);
  guest.kvmf.assign(trim_field_buffer(field_buffer(field[5], 0)));
  guest.path.assign(trim_field_buffer(field_buffer(field[6], 0)));
  guest.ints.assign(trim_field_buffer(field_buffer(field[7], 0)));
  guest.ndrv.assign(trim_field_buffer(field_buffer(field[8], 0)));
  guest.usbp.assign(trim_field_buffer(field_buffer(field[9], 0)));
  guest.usbd.assign(trim_field_buffer(field_buffer(field[10], 0)));
}

void QManager::AddVmWindow::Get_data_from_db() {
  std::unique_ptr<QemuDb> db(new QemuDb(dbf_));
  sql_query = "select vnc from lastval";
  v_last_vnc = db->SelectQuery(sql_query); // TODO: add check if null exeption

  sql_query = "select mac from lastval";
  v_last_mac = db->SelectQuery(sql_query); // TODO: add check if null exeption

  sql_query = "select id from vms where name='" + guest.name + "'";
  v_name = db->SelectQuery(sql_query);

  last_mac = std::stol(v_last_mac[0]);
  last_vnc = std::stoi(v_last_vnc[0]);
  last_vnc++;
}

void QManager::AddVmWindow::Update_db_data() {
  if(guest.usbp == "yes") {
    guest.usbp = "1";
    guest.usbd = u_dev.at(guest.usbd);
  }
  else {
    guest.usbp = "0";
    guest.usbd = "none";
  }

  guest.disk = guest.name + "_a.img=" + guest.disk + ";";
  guest.kvmf == "yes" ? guest.kvmf = "1" : guest.kvmf = "0";

  std::unique_ptr<QemuDb> db(new QemuDb(dbf_));

  sql_query = "update lastval set mac='" + std::to_string(last_mac) + "'";
  db->ActionQuery(sql_query);

  sql_query = "update lastval set vnc='" + std::to_string(last_vnc) + "'";
  db->ActionQuery(sql_query);

  // Add guest to database
  sql_query = "insert into vms("
  "name, mem, smp, hdd, kvm, vnc, mac, arch, iso, install, usb, usbid"
  ") values('"
  + guest.name + "', '" + guest.memo + "', '" + guest.cpus + "', '"
  + guest.disk + "', '" + guest.kvmf + "', '" + guest.vncp + "', '"
  + guest.ints + "', '" + guest.arch + "', '" + guest.path + "', '1', '"
  + guest.usbp + "', '" + guest.usbd + "')";

  db->ActionQuery(sql_query);
}

void QManager::AddVmWindow::Gen_hdd() {
  guest_dir = vmdir_ + "/" + guest.name;
  create_guest_dir_cmd = "mkdir " + guest_dir + " >/dev/null 2>&1";
  create_img_cmd = "qemu-img create -f qcow2 " + guest_dir
  + "/" + guest.name + "_a.img " + guest.disk + "G > /dev/null 2>&1";

  cmd_exit_status = system(create_guest_dir_cmd.c_str());

  if(cmd_exit_status != 0) {
    Delete_form();
    throw QMException(_("Can't create guest dir"));
  }

  cmd_exit_status = system(create_img_cmd.c_str());

  if(cmd_exit_status != 0) {
    Delete_form();
    throw QMException(_("Can't create img file"));
  }
}

void QManager::AddVmWindow::Check_input_data() {
  if(guest.usbp == "yes") {
    for(size_t i = 0; i < 11; ++i) {
      if(! field_status(field[i])) {
        Delete_form();
        throw QMException(_("Must fill all params"));
      }
    }
  }
  else {
    for(size_t i = 0; i < 9; ++i) {
      if(! field_status(field[i])) {
        Delete_form();
        throw QMException(_("Must fill all params"));
      }
    }
  }
}

void QManager::AddVmWindow::Print() {
  finish.store(false);

  try {
    u_dev = list_usb();

    Enable_color();
    Draw_title();
    Get_data_from_db();
    Create_fields();
    Config_fields_type();
    Config_fields_buffer();
    Post_form(21);
    Print_fields_names();
    Draw_form();

    Get_data_from_form();
    Check_input_data();

    if(! v_name.empty()) {
      Delete_form();
      throw QMException(_("This name is already used"));
    }

    getmaxyx(stdscr, row, col);
    std::thread spin_thr(spinner, 1, (col + str_size + 2) / 2);

    try {
      Gen_hdd();
      Gen_mac_address(guest, std::stoi(guest.ints), guest.name);
      Gen_iface_json();
      Update_db_data();
    }
    catch (...) {
      finish.store(true);
      spin_thr.join();
      throw;
    }

    finish.store(true);
    spin_thr.join();
    Delete_form();
  }
  catch (QMException &err) {
    ExeptionExit(err);
  }
  curs_set(0);
}

QManager::EditVmWindow::EditVmWindow(
  const std::string &dbf, const std::string &vmdir, const std::string &vm_name,
  int height, int width, int starty
) : AddVmWindow(dbf, vmdir, height, width, starty) {
    vm_name_ = vm_name;

    field.resize(7);
}

void QManager::EditVmWindow::Create_fields() {
  for(size_t i = 0; i < field.size() - 1; ++i) {
    field[i] = new_field(1, 35, (i+1)*2, 1, 0, 0);
    set_field_back(field[i], A_UNDERLINE);
  }

  field[field.size() - 1] = NULL;
}

void QManager::EditVmWindow::Config_fields_type() {
  UdevList = new char *[u_dev.size() + 1];

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
}

void QManager::EditVmWindow::Config_fields_buffer() {
  MapStringVector old_ifaces = Read_ifaces_from_json(guest_old.ints[0]);
  ints_count = old_ifaces.size();

  char cints[64];
  snprintf(cints, sizeof(cints), "%u", ints_count);

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

  for(size_t i = 0; i < 5; ++i)
    set_field_status(field[i], false);
}

void QManager::EditVmWindow::Get_data_from_form() {
  guest_new.cpus.assign(trim_field_buffer(field_buffer(field[0], 0)));
  guest_new.memo.assign(trim_field_buffer(field_buffer(field[1], 0)));
  guest_new.kvmf.assign(trim_field_buffer(field_buffer(field[2], 0)));
  guest_new.ints.assign(trim_field_buffer(field_buffer(field[3], 0)));
  guest_new.usbp.assign(trim_field_buffer(field_buffer(field[4], 0)));
  guest_new.usbd.assign(trim_field_buffer(field_buffer(field[5], 0)));
}

void QManager::EditVmWindow::Print_fields_names() {
  char ccpu[128], cmem[128];

  snprintf(ccpu, sizeof(ccpu), "%s%u%s", _("CPU cores [1-"), cpu_count(), "]");
  snprintf(cmem, sizeof(cmem), "%s%u%s", _("Memory [64-"), total_memory(), "]Mb");

  mvwaddstr(window, 2, 22, (vm_name_ + _(" settings:")).c_str());
  mvwaddstr(window, 4, 2, ccpu);
  mvwaddstr(window, 6, 2, cmem);
  mvwaddstr(window, 8, 2, _("KVM [yes/no]"));
  mvwaddstr(window, 10, 2, _("Interfaces"));
  mvwaddstr(window, 12, 2, _("USB [yes/no]"));
  mvwaddstr(window, 14, 2, _("USB device"));
}

void QManager::EditVmWindow::Get_data_from_db() {
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

  sql_query = "select mac from lastval";
  v_last_mac = db->SelectQuery(sql_query);
}

void QManager::EditVmWindow::Gen_iface_json(uint32_t new_if_count) {
  MapStringVector old_ifaces = Read_ifaces_from_json(guest_old.ints[0]);
  size_t old_if_count = old_ifaces.size();

  if(old_if_count > new_if_count) {
    size_t n = 0;
    for(auto it : old_ifaces) {
      guest_old.ndrv.push_back(it.second[1]);
      ++n;
      if(n == new_if_count)
        break;
    }
  }
  else if(old_if_count < new_if_count) {
    for(auto it : old_ifaces)
      guest_old.ndrv.push_back(it.second[1]);

    size_t count_diff = new_if_count - old_if_count;
    for(size_t i = 0; i < count_diff; ++i)
      guest_old.ndrv.push_back(DEFAULT_NETDRV);
  }

  guest_new.ints.clear();
  size_t i = 0;
  for(auto &ifs : ifaces) {
    guest_new.ints += "{\"name\":\"" + ifs.first + "\",\"mac\":\"" +
      ifs.second + "\",\"drv\":\"" + guest_old.ndrv[i] + "\"},";

    i++;
  }

  guest_new.ints.erase(guest_new.ints.find_last_not_of(",") + 1);
  guest_new.ints = "{\"ifaces\":[" + guest_new.ints + "]}";
}

void QManager::EditVmWindow::Update_db_cpu_data() {
  std::unique_ptr<QemuDb> db(new QemuDb(dbf_));

  if(field_status(field[0])) {
    sql_query = "update vms set smp='" + guest_new.cpus +
      "' where name='" + vm_name_ + "'";
    db->ActionQuery(sql_query);
  }
}

void QManager::EditVmWindow::Update_db_mem_data() {
  std::unique_ptr<QemuDb> db(new QemuDb(dbf_));

  if(field_status(field[1])) {
    sql_query = "update vms set mem='" + guest_new.memo +
      "' where name='" + vm_name_ + "'";
    db->ActionQuery(sql_query);
  }
}

void QManager::EditVmWindow::Update_db_kvm_data() {
  std::unique_ptr<QemuDb> db(new QemuDb(dbf_));

  if(field_status(field[2])) {
    if(guest_new.kvmf == "yes")
      guest_new.kvmf = "1";
    else
      guest_new.kvmf = "0";

    sql_query = "update vms set kvm='" + guest_new.kvmf +
      "' where name='" + vm_name_ + "'";
    db->ActionQuery(sql_query);
  }
}

void QManager::EditVmWindow::Update_db_usb_data() {
  std::unique_ptr<QemuDb> db(new QemuDb(dbf_));

  if(field_status(field[4])) {
    if(guest_new.usbp == "yes") {
      if(! field_status(field[5])) {
        Delete_form();
        throw QMException(_("Usb device was not selected."));
      }

      guest_new.usbp = "1";
      guest_new.usbd = u_dev.at(guest_new.usbd);
    }
    else {
      guest_new.usbp = "0";
      guest_new.usbd = "none";
    }

    sql_query = "update vms set usbid='" + guest_new.usbd +
      "' where name='" + vm_name_ + "'";
    db->ActionQuery(sql_query);

    sql_query = "update vms set usb='" + guest_new.usbp +
      "' where name='" + vm_name_ + "'";
    db->ActionQuery(sql_query);
  }
}

void QManager::EditVmWindow::Update_db_eth_data() {
  std::unique_ptr<QemuDb> db(new QemuDb(dbf_));

  if(field_status(field[3])) {
    ui_vm_ints = std::stoi(guest_new.ints);

    if(ui_vm_ints != ints_count) {
      Gen_mac_address(guest_new, ui_vm_ints, vm_name_);
      Gen_iface_json(ui_vm_ints);

      sql_query = "update lastval set mac='" + std::to_string(last_mac) + "'";
      db->ActionQuery(sql_query);

      sql_query = "update vms set mac='" + guest_new.ints +
        "' where name='" + vm_name_ + "'";
      db->ActionQuery(sql_query);
    }
  }
}

void QManager::EditVmWindow::Print() {
  finish.store(false);

  try {
    u_dev = list_usb();

    Enable_color();

    Draw_title();
    Get_data_from_db();
    Create_fields();
    Config_fields_type();
    Config_fields_buffer();
    Post_form(21);
    Print_fields_names();
    Draw_form();
    Get_data_from_form();

    getmaxyx(stdscr, row, col);
    std::thread spin_thr(spinner, 1, (col + str_size + 2) / 2);

    try {
      Update_db_cpu_data();
      Update_db_mem_data();
      Update_db_kvm_data();
      Update_db_usb_data();
      Update_db_eth_data();
    }
    catch (...) {
      finish.store(true);
      spin_thr.join();
      throw;
    }

    finish.store(true);
    spin_thr.join();

    Delete_form();
  }
  catch (QMException &err) {
    ExeptionExit(err);
  }
}

QManager::CloneVmWindow::CloneVmWindow(
  const std::string &dbf, const std::string &vmdir, const std::string &vm_name,
  int height, int width, int starty
) : AddVmWindow(dbf, vmdir, height, width, starty) {
    vm_name_ = vm_name;

    field.resize(2);
}

void QManager::CloneVmWindow::Create_fields() {
  field[0] = new_field(1, 20, 2, 1, 0, 0);
  field[field.size() - 1] = NULL;
}

void QManager::CloneVmWindow::Config_fields() {
  char cname[64];
  snprintf(cname, sizeof(cname), "%s%s", vm_name_.c_str(), "_");
  set_field_type(field[0], TYPE_ALNUM, 0);
  set_field_buffer(field[0], 0, cname);
  set_field_status(field[0], false);
}

void QManager::CloneVmWindow::Print_fields_names() {
  mvwaddstr(window, 2, 12, (_("Clone ") + vm_name_).c_str());
  mvwaddstr(window, 4, 2, _("Name"));
}

void QManager::CloneVmWindow::Get_data_from_form() {
  guest_new.name.assign(trim_field_buffer(field_buffer(field[0], 0)));
}

void QManager::CloneVmWindow::Get_data_from_db() {
  std::unique_ptr<QemuDb> db(new QemuDb(dbf_));

  sql_query = "select mac from vms where name='" + vm_name_ + "'";
  guest_old.ints = db->SelectQuery(sql_query);

  sql_query = "select mac from lastval";
  v_last_mac = db->SelectQuery(sql_query);

  sql_query = "select vnc from lastval";
  v_last_vnc = db->SelectQuery(sql_query);

  sql_query = "select id from vms where name='" + guest_new.name + "'";
  v_name = db->SelectQuery(sql_query);

  sql_query = "select hdd from vms where name='" + vm_name_ + "'";
  guest_old.disk = db->SelectQuery(sql_query);

  last_vnc = std::stoi(v_last_vnc[0]);
  last_vnc++;
}

void QManager::CloneVmWindow::Gen_iface_json() {
  MapStringVector old_ifaces = Read_ifaces_from_json(guest_old.ints[0]);

  for(auto &old_ifs : old_ifaces)
    guest_old.ndrv.push_back(old_ifs.second[1]);

  guest_new.ints.clear();
  size_t i = 0;
  for(auto &ifs : ifaces) {
    guest_new.ints += "{\"name\":\"" + ifs.first + "\",\"mac\":\"" +
      ifs.second + "\",\"drv\":\"" + guest_old.ndrv[i] + "\"},";

    i++;
  }

  guest_new.ints.erase(guest_new.ints.find_last_not_of(",") + 1);
  guest_new.ints = "{\"ifaces\":[" + guest_new.ints + "]}";
}

void QManager::CloneVmWindow::Update_db_data() {
  const std::array<std::string, 8> columns = {
    "mem", "smp", "kvm",
    "arch", "iso", "install",
    "usb", "usbid"
  };

  std::unique_ptr<QemuDb> db(new QemuDb(dbf_));

  sql_query = "insert into vms(name) values('" + guest_new.name + "')";
  db->ActionQuery(sql_query);

  sql_query = "update lastval set mac='" + std::to_string(last_mac) + "'";
  db->ActionQuery(sql_query);

  sql_query = "update lastval set vnc='" + std::to_string(last_vnc) + "'";
  db->ActionQuery(sql_query);

  for(auto &c : columns) {
    sql_query = "update vms set " + c + "=(select " + c + " from vms where name='" +
    vm_name_ + "') where name='" + guest_new.name + "'";
    db->ActionQuery(sql_query);
  }

  sql_query = "update vms set mac='" + guest_new.ints +
    "' where name='" + guest_new.name + "'";
  db->ActionQuery(sql_query);

  sql_query = "update vms set vnc='" + std::to_string(last_vnc) +
    "' where name='" + guest_new.name + "'";
  db->ActionQuery(sql_query);

  sql_query = "update vms set hdd='" + guest_new.disk +
    "' where name='" + guest_new.name + "'";
  db->ActionQuery(sql_query);
}

void QManager::CloneVmWindow::Gen_hdd() {
  hdd_ch = 'a';
  MapString disk = Gen_map_from_str(guest_old.disk[0]);

  guest_dir = vmdir_ + "/" + guest_new.name;
  create_guest_dir_cmd = "mkdir " + guest_dir;

  system(create_guest_dir_cmd.c_str());

  for(auto &hd : disk) {
    guest_new.disk += guest_new.name + "_" + hdd_ch +
      ".img=" + hd.second + ";";

    create_img_cmd = "cp " + vmdir_ + "/" + vm_name_ +
      "/" + hd.first + " " + vmdir_ + "/" + guest_new.name +
      "/" + guest_new.name + "_" + hdd_ch + ".img";

    system(create_img_cmd.c_str());
    hdd_ch++;
  }
}

void QManager::CloneVmWindow::Print() {
  finish.store(false);

  try {
    Draw_title();
    Create_fields();
    Enable_color();
    Config_fields();
    Post_form(10);
    Print_fields_names();
    Draw_form();

    Get_data_from_form();
    Get_data_from_db();

    if(guest_new.name.empty())
      throw QMException(_("Null guest name"));

    if(! v_name.empty())
      throw QMException(_("This name is already used"));

    getmaxyx(stdscr, row, col);
    std::thread spin_thr(spinner, 1, (col + str_size + 2) / 2);

    Gen_mac_address(
      guest_new, Read_ifaces_from_json(guest_old.ints[0]).size(),
      guest_new.name
    );
    Gen_iface_json();

    Gen_hdd();
    Update_db_data();
    finish.store(true);
    spin_thr.join();

    Delete_form();
  }
  catch (QMException &err) {
    ExeptionExit(err);
  }
}

QManager::AddDiskWindow::AddDiskWindow(
  const std::string &dbf, const std::string &vmdir, const std::string &vm_name,
  int height, int width, int starty
) : AddVmWindow(dbf, vmdir, height, width, starty) {
    vm_name_ = vm_name;

    field.resize(2);
}

void QManager::AddDiskWindow::Create_fields() {
  field[0] = new_field(1, 8, 2, 1, 0, 0);
  field[field.size() - 1] = NULL;
}

void QManager::AddDiskWindow::Config_fields() {
  set_field_type(field[0], TYPE_INTEGER, 0, 1, disk_free(vmdir_));
}

void QManager::AddDiskWindow::Print_fields_names() {
  char cfree[128];
  snprintf(cfree, sizeof(cfree), "%s%u%s", _("Size [1-"), disk_free(vmdir_), "]Gb");

  mvwaddstr(window, 2, 8, (_("Add disk to ") + vm_name_).c_str());
  mvwaddstr(window, 4, 2, cfree);
}

void QManager::AddDiskWindow::Get_data_from_form() {
  guest_new.disk.assign(trim_field_buffer(field_buffer(field[0], 0)));
}

void QManager::AddDiskWindow::Get_data_from_db() {
  std::unique_ptr<QemuDb> db(new QemuDb(dbf_));

  sql_query = "select hdd from vms where name='" + vm_name_ + "'";
  guest_old.disk = db->SelectQuery(sql_query);
}

void QManager::AddDiskWindow::Update_db_data() {
  std::unique_ptr<QemuDb> db(new QemuDb(dbf_));

  sql_query = "update vms set hdd='" + guest_old.disk[0] +
    "' where name='" + vm_name_ + "'";
  db->ActionQuery(sql_query);
}

void QManager::AddDiskWindow::Gen_hdd() {
  hdd_ch = 'a';
  MapString disk = Gen_map_from_str(guest_old.disk[0]);

  if(disk.size() == 26)
    throw QMException(_("26 disks limit reached :("));

  hdd_ch += disk.size();

  guest_dir = vmdir_ + "/" + vm_name_;
  create_img_cmd = "qemu-img create -f qcow2 " + guest_dir
  + "/" + vm_name_ + "_" + hdd_ch + ".img " + guest_new.disk + "G > /dev/null 2>&1";

  cmd_exit_status = system(create_img_cmd.c_str());

  if(cmd_exit_status != 0) {
    Delete_form();
    throw QMException(_("Can't create img file"));
  }

  guest_old.disk[0] += vm_name_ + "_" + hdd_ch + ".img=" + guest_new.disk + ";";
}

void QManager::AddDiskWindow::Print() {
  finish.store(false);

  try {
    Draw_title();
    Create_fields();
    Enable_color();
    Config_fields();
    Post_form(22);
    Print_fields_names();
    Draw_form();

    Get_data_from_form();
    Get_data_from_db();

    if(guest_new.disk.empty())
      throw QMException(_("Null disk size"));

    if(std::stol(guest_new.disk) <= 0 || std::stoul(guest_new.disk) >= disk_free(vmdir_))
      throw QMException(_("Wrong disk size"));

    getmaxyx(stdscr, row, col);
    std::thread spin_thr(spinner, 1, (col + str_size + 2) / 2);

    try {
      Gen_hdd();
      Update_db_data();
    }
    catch (...) {
      finish.store(true);
      spin_thr.join();
      throw;
    }

    finish.store(true);
    spin_thr.join();
    Delete_form();
  }
  catch (QMException &err) {
    ExeptionExit(err);
  }
}

void QManager::HelpWindow::Print() {
  line = 1;
  window_ = newwin(height_, width_, starty_, startx_);
  keypad(window_, TRUE);
  box(window_, 0, 0);

  msg_.push_back("Qemu Manage v" + std::string(VERSION));
  msg_.push_back("");
  msg_.push_back(_("\"r\" - start guest"));
  msg_.push_back(_("\"c\" - connect to guest via vnc"));
  msg_.push_back(_("\"f\" - force stop guest"));
  msg_.push_back(_("\"d\" - delete guest"));
  msg_.push_back(_("\"e\" - edit guest settings"));
  msg_.push_back(_("\"i\" - edit network settings"));
  msg_.push_back(_("\"a\" - add virtual disk"));
  msg_.push_back(_("\"l\" - clone guest"));

  for(auto &msg : msg_) {
    mvwprintw(window_, line, 1, "%s", msg.c_str());
    line++;
  }

  wrefresh(window_);
  wgetch(window_);
}

QManager::EditNetWindow::EditNetWindow(
  const std::string &dbf, const std::string &vmdir, const std::string &vm_name,
  int height, int width, int starty
) : AddVmWindow(dbf, vmdir, height, width, starty) {
    vm_name_ = vm_name;

    field.resize(4);
}

void QManager::EditNetWindow::Create_fields() {
  for(size_t i = 0; i < field.size() - 1; ++i)
    field[i] = new_field(1, 17, i*2, 1, 0, 0);

  field[field.size() - 1] = NULL;
}

void QManager::EditNetWindow::Get_data_from_db() {
  std::unique_ptr<QemuDb> db(new QemuDb(dbf_));

  sql_query = "select mac from vms where name='" + vm_name_ + "'";
  guest_old.ints = db->SelectQuery(sql_query);

  sql_query = "select mac from vms";
  all_ints = db->SelectQuery(sql_query);
}

void QManager::EditNetWindow::Config_fields() {
  ifs = Read_ifaces_from_json(guest_old.ints[0]);

  for(auto i : ifs)
    iflist.push_back(i.first);

  IfaceList = new char *[iflist.size() + 1];

  for(size_t i = 0; i < iflist.size(); ++i) {
    IfaceList[i] = new char[iflist[i].size() + 1];
    memcpy(IfaceList[i], iflist[i].c_str(), iflist[i].size() + 1);
  }

  IfaceList[iflist.size()] = NULL;

  set_field_type(field[0], TYPE_ENUM, (char **)IfaceList, false, false);
  set_field_type(field[1], TYPE_ENUM, (char **)NetDrv, false, false);
  set_field_type(field[2], TYPE_REGEXP, "([0-9a-fA-F]{2}:){5}[0-9a-fA-F]{2}");

  for(size_t i = 0; i < iflist.size(); ++i) {
    delete [] IfaceList[i];
  }

  delete [] IfaceList;

  for(size_t i = 0; i < field.size() - 1; ++i)
    set_field_status(field[i], false);
}

void QManager::EditNetWindow::Print_fields_names() {
  mvwaddstr(window, 2, 2, _("Interface"));
  mvwaddstr(window, 4, 2, _("Net driver"));
  mvwaddstr(window, 6, 2, _("Mac address"));
}

void QManager::EditNetWindow::Get_data_from_form() {
  guest_new.name.assign(trim_field_buffer(field_buffer(field[0], 0)));
  guest_new.ndrv.assign(trim_field_buffer(field_buffer(field[1], 0)));
  guest_new.imac.assign(trim_field_buffer(field_buffer(field[2], 0)));
}

void QManager::EditNetWindow::Gen_iface_json() {
  guest_new.ints.clear();
  for(auto &i : ifs) {
    guest_new.ints += "{\"name\":\"" + i.first + "\",\"mac\":\"" +
      i.second[0] + "\",\"drv\":\"" + i.second[1] + "\"},";
  }

  guest_new.ints.erase(guest_new.ints.find_last_not_of(",") + 1);
  guest_new.ints = "{\"ifaces\":[" + guest_new.ints + "]}";
}

void QManager::EditNetWindow::Update_db_eth_drv_data() {
  std::unique_ptr<QemuDb> db(new QemuDb(dbf_));

  if(field_status(field[1])) {
    if(! field_status(field[0]))
      throw QMException(_("Null network interface"));

    auto it = ifs.find(guest_new.name);

    if(it == ifs.end())
      throw QMException(_("Something goes wrong"));

    it->second[1] = guest_new.ndrv;

    Gen_iface_json();

    sql_query = "update vms set mac='" + guest_new.ints +
      "' where name='" + vm_name_ + "'";
    db->ActionQuery(sql_query);
  }
}

void QManager::EditNetWindow::Update_db_eth_mac_data() {
  std::unique_ptr<QemuDb> db(new QemuDb(dbf_));

  if(field_status(field[2])) {
    if(! field_status(field[0]))
      throw QMException(_("Null network interface"));

    if(! verify_mac(guest_new.imac))
      throw QMException(_("Wrong mac address"));

    auto it = ifs.find(guest_new.name);

    if(it == ifs.end())
      throw QMException(_("Something goes wrong"));

    std::unordered_set<std::string> mac_db;

    for(size_t i = 0; i < all_ints.size(); ++i) {
      MapStringVector ints = Read_ifaces_from_json(all_ints[i]);
      for(auto i : ints)
        mac_db.insert(i.second[0]);
    }

    auto mac_it = mac_db.find(guest_new.imac);

    if(mac_it != mac_db.end())
      throw QMException(_("This mac is already used!"));

    it->second[0] = guest_new.imac;

    Gen_iface_json();

    sql_query = "update vms set mac='" + guest_new.ints +
      "' where name='" + vm_name_ + "'";
    db->ActionQuery(sql_query);
  }
}

void QManager::EditNetWindow::Print() {
  finish.store(false);

  try {
    Draw_title();
    Create_fields();
    Enable_color();
    Get_data_from_db();
    Config_fields();
    Post_form(18);
    Print_fields_names();
    Draw_form();

    Get_data_from_form();

    getmaxyx(stdscr, row, col);
    std::thread spin_thr(spinner, 1, (col + str_size + 2) / 2);
    
    try {
      Update_db_eth_drv_data();
      Update_db_eth_mac_data();
    }
    catch (...) {
      finish.store(true);
      spin_thr.join();
      throw;
    }

    finish.store(true);
    spin_thr.join();
    Delete_form();
  }
  catch (QMException &err) {
    ExeptionExit(err);
  }
}

QManager::PopupWarning::PopupWarning(const std::string &msg, int height,
 int width, int starty) : TemplateWindow(height, width, starty) {
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
