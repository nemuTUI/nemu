#include <cstring>
#include <algorithm>
#include <memory>
#include <array>
#include <thread>
#include <unordered_set>

#include "qemu_manage.h"

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

  void QMFormWindow::Delete_form() 
  {
    unpost_form(form);
    free_form(form);
    for(size_t i = 0; i < field.size() - 1; ++i) {
      free_field(field[i]);
    }
  }

  void QMFormWindow::Draw_title()
  {
    help_msg = _("F10 - finish, F2 - save");
    str_size = mbstowcs(NULL, help_msg.c_str(), help_msg.size());

    clear();
    border(0,0,0,0,0,0,0,0);
    mvprintw(1, (col - str_size) / 2, help_msg.c_str());
    refresh();
    curs_set(1);
  }

  void QMFormWindow::Post_form(uint32_t size)
  {
    form = new_form(&field[0]);
    scale_form(form, &row, &col);
    set_form_win(form, window);
    set_form_sub(form, derwin(window, row, col, 2, size));
    box(window, 0, 0);
    post_form(form);
  }

  void QMFormWindow::ExeptionExit(QMException &err)
  {
    curs_set(0);
    PopupWarning Warn(err.what(), 3, 30, 4);
    Warn.Init();
    Warn.Print(Warn.window);
    refresh();
  }

  void QMFormWindow::Enable_color()
  {
    init_pair(1, COLOR_BLACK, COLOR_WHITE);
    wbkgd(window, COLOR_PAIR(1));
  }

  void QMFormWindow::Gen_mac_address(
    struct guest_t<std::string> &guest, uint32_t int_count, std::string vm_name
  )
  {
    last_mac = std::stol(v_last_mac[0]);
    ifaces = gen_mac_addr(last_mac, int_count, vm_name);

    itm = ifaces.end();
    --itm;
    s_last_mac = itm->second;

    its = std::remove(s_last_mac.begin(), s_last_mac.end(), ':');
    s_last_mac.erase(its, s_last_mac.end());

    last_mac = std::stol(s_last_mac, 0, 16);
  }

  void QMFormWindow::Draw_form()
  {
    while((ch = wgetch(window)) != KEY_F(10))
    {
      switch(ch)
      {
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
  
  CloneVmWindow::CloneVmWindow(
    const std::string &dbf, const std::string &vmdir, const std::string &vm_name,
    int height, int width, int starty
  ) : QMFormWindow(height, width, starty)
  {
      dbf_ = dbf;
      vmdir_ = vmdir;
      vm_name_ = vm_name;

      field.resize(2);
  }

  void CloneVmWindow::Create_fields()
  {
    field[0] = new_field(1, 20, 2, 1, 0, 0);
    field[field.size() - 1] = NULL;
  }

  void CloneVmWindow::Config_fields_type() 
  {
    set_field_type(field[0], TYPE_ALNUM, 0);
  }

  void CloneVmWindow::Config_fields_buffer()
  {
    char cname[64];
    snprintf(cname, sizeof(cname), "%s%s", vm_name_.c_str(), "_");
    set_field_buffer(field[0], 0, cname);
    set_field_status(field[0], false);
  }

  void CloneVmWindow::Print_fields_names()
  {
    mvwaddstr(window, 2, 12, (_("Clone ") + vm_name_).c_str());
    mvwaddstr(window, 4, 2, _("Name"));
  }

  void CloneVmWindow::Get_data_from_form()
  {
    guest_new.name.assign(trim_field_buffer(field_buffer(field[0], 0)));
  }

  void CloneVmWindow::Get_data_from_db()
  {
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

  void CloneVmWindow::Gen_iface_json()
  {
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

  void CloneVmWindow::Update_db_data()
  {
    const std::array<std::string, 9> columns = {
      "mem", "smp", "kvm", "hcpu",
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

  void CloneVmWindow::Gen_hdd()
  {
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

  void CloneVmWindow::Print()
  {
    finish.store(false);

    try {
      Draw_title();
      Create_fields();
      Enable_color();
      Config_fields_type();
      Config_fields_buffer();
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

  AddDiskWindow::AddDiskWindow(
    const std::string &dbf, const std::string &vmdir, const std::string &vm_name,
    int height, int width, int starty
  ) : QMFormWindow(height, width, starty)
  {
      dbf_ = dbf;
      vmdir_ = vmdir;
      vm_name_ = vm_name;

      field.resize(2);
  }

  void AddDiskWindow::Create_fields()
  {
    field[0] = new_field(1, 8, 2, 1, 0, 0);
    field[field.size() - 1] = NULL;
  }

  void AddDiskWindow::Config_fields_type()
  {
    set_field_type(field[0], TYPE_INTEGER, 0, 1, disk_free(vmdir_));
  }

  void AddDiskWindow::Print_fields_names()
  {
    char cfree[128];
    snprintf(cfree, sizeof(cfree), "%s%u%s", _("Size [1-"), disk_free(vmdir_), "]Gb");

    mvwaddstr(window, 2, 8, (_("Add disk to ") + vm_name_).c_str());
    mvwaddstr(window, 4, 2, cfree);
  }

  void AddDiskWindow::Get_data_from_form()
  {
    guest_new.disk.assign(trim_field_buffer(field_buffer(field[0], 0)));
  }

  void AddDiskWindow::Get_data_from_db()
  {
    std::unique_ptr<QemuDb> db(new QemuDb(dbf_));

    sql_query = "select hdd from vms where name='" + vm_name_ + "'";
    guest_old.disk = db->SelectQuery(sql_query);
  }

  void AddDiskWindow::Update_db_data()
  {
    std::unique_ptr<QemuDb> db(new QemuDb(dbf_));

    sql_query = "update vms set hdd='" + guest_old.disk[0] +
      "' where name='" + vm_name_ + "'";
    db->ActionQuery(sql_query);
  }

  void AddDiskWindow::Gen_hdd()
  {
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

  void AddDiskWindow::Print()
  {
    finish.store(false);

    try {
      Draw_title();
      Create_fields();
      Enable_color();
      Config_fields_type();
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

  EditNetWindow::EditNetWindow(
    const std::string &dbf, const std::string &vmdir, const std::string &vm_name,
    int height, int width, int starty
  ) : QMFormWindow(height, width, starty)
  {
      dbf_ = dbf;
      vmdir_ = vmdir;
      vm_name_ = vm_name;

      field.resize(4);
  }

  void EditNetWindow::Create_fields()
  {
    for(size_t i = 0; i < field.size() - 1; ++i)
      field[i] = new_field(1, 17, i*2, 1, 0, 0);

    field[field.size() - 1] = NULL;
  }

  void EditNetWindow::Get_data_from_db()
  {
    std::unique_ptr<QemuDb> db(new QemuDb(dbf_));

    sql_query = "select mac from vms where name='" + vm_name_ + "'";
    guest_old.ints = db->SelectQuery(sql_query);

    sql_query = "select mac from vms";
    all_ints = db->SelectQuery(sql_query);
  }

  void EditNetWindow::Config_fields_type()
  {
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

  void EditNetWindow::Print_fields_names()
  {
    mvwaddstr(window, 2, 2, _("Interface"));
    mvwaddstr(window, 4, 2, _("Net driver"));
    mvwaddstr(window, 6, 2, _("Mac address"));
  }

  void EditNetWindow::Get_data_from_form()
  {
    guest_new.name.assign(trim_field_buffer(field_buffer(field[0], 0)));
    guest_new.ndrv.assign(trim_field_buffer(field_buffer(field[1], 0)));
    guest_new.imac.assign(trim_field_buffer(field_buffer(field[2], 0)));
  }

  void EditNetWindow::Gen_iface_json()
  {
    guest_new.ints.clear();
    for(auto &i : ifs) {
      guest_new.ints += "{\"name\":\"" + i.first + "\",\"mac\":\"" +
        i.second[0] + "\",\"drv\":\"" + i.second[1] + "\"},";
    }

    guest_new.ints.erase(guest_new.ints.find_last_not_of(",") + 1);
    guest_new.ints = "{\"ifaces\":[" + guest_new.ints + "]}";
  }

  void EditNetWindow::Update_db_data() 
  {
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

  void EditNetWindow::Print()
  {
    finish.store(false);

    try {
      Draw_title();
      Create_fields();
      Enable_color();
      Get_data_from_db();
      Config_fields_type();
      Post_form(18);
      Print_fields_names();
      Draw_form();

      Get_data_from_form();

      getmaxyx(stdscr, row, col);
      std::thread spin_thr(spinner, 1, (col + str_size + 2) / 2);
      
      try {
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
