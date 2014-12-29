#include <thread>

#include "install_window.h"

namespace QManager
{
  EditInstallWindow::EditInstallWindow(
    const std::string &dbf, const std::string &vmdir, const std::string &vm_name,
    int height, int width, int starty
  ) : AddVmWindow(dbf, vmdir, height, width, starty) {
      vm_name_ = vm_name;

      field.resize(3);
  }

  void EditInstallWindow::Create_fields()
  {
    for(size_t i = 0; i < field.size() - 1; ++i) {
      field[i] = new_field(1, 35, i*2, 1, 0, 0);
      set_field_back(field[i], A_UNDERLINE);
    }

    field[field.size() - 1] = NULL;
  }

  void EditInstallWindow::Config_fields()
  {
    field_opts_off(field[1], O_STATIC);
    set_field_type(field[0], TYPE_ENUM, (char **)YesNo, false, false);
    set_field_type(field[1], TYPE_REGEXP, "^/.*");

    if(guest_old.install[0] == "1")
      set_field_buffer(field[0], 0, YesNo[1]);
    else
      set_field_buffer(field[0], 0, YesNo[0]);

    set_field_buffer(field[1], 0, guest_old.path[0].c_str());

    for(size_t i = 0; i < field.size() - 1; ++i)
      set_field_status(field[i], false);
  }

  void EditInstallWindow::Print_fields_names()
  {
    mvwaddstr(window, 2, 2, _("Installed [yes/no]"));
    mvwaddstr(window, 4, 2, _("Path to ISO"));
  }

  void EditInstallWindow::Get_data_from_form()
  {
    guest_new.install.assign(trim_field_buffer(field_buffer(field[0], 0)));
    guest_new.path.assign(trim_field_buffer(field_buffer(field[1], 0)));
  }

  void EditInstallWindow::Get_data_from_db()
  {
    std::unique_ptr<QemuDb> db(new QemuDb(dbf_));

    sql_query = "select install from vms where name='" + vm_name_ + "'";
    guest_old.install = db->SelectQuery(sql_query);
    sql_query = "select iso from vms where name='" + vm_name_ + "'";
    guest_old.path = db->SelectQuery(sql_query);
  }

  void EditInstallWindow::Update_db_data()
  {
    std::unique_ptr<QemuDb> db(new QemuDb(dbf_));

    if(field_status(field[0]))
    {
      if(guest_new.install == "yes")
        guest_new.install = "0";
      else
        guest_new.install = "1";

      sql_query = "update vms set install='" + guest_new.install +
        "' where name='" + vm_name_ + "'";
      db->ActionQuery(sql_query);
    }

    if(field_status(field[1]))
    {
      sql_query = "update vms set iso='" + guest_new.path +
        "' where name='" + vm_name_ + "'";
      db->ActionQuery(sql_query);
    }
  }

  void EditInstallWindow::Print()
  {
    finish.store(false);

    try {
      Draw_title();
      Create_fields();
      Enable_color();
      Get_data_from_db();
      Config_fields();
      Post_form(22);
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
} // namespace QManager

