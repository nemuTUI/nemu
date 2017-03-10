#include <libintl.h>

#include "add_disk_window.h"

namespace QManager {

AddDiskWindow::AddDiskWindow(const std::string &dbf, const std::string &vmdir,
                             const std::string &vm_name,
                             int height, int width, int starty) :
                             QMFormWindow(height, width, starty)
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
    char cfree[128] = {0};
    snprintf(cfree, sizeof(cfree), "%s%u%s", _("Size [1-"), disk_free(vmdir_), "]Gb");

    mvwaddstr(window, 2, 8, (_("Add disk to ") + vm_name_).c_str());
    mvwaddstr(window, 4, 2, cfree);
}

void AddDiskWindow::Get_data_from_form()
{
    try
    {
        guest_new.disk.assign(trim_field_buffer(field_buffer(field[0], 0), true));
    }
    catch (const std::runtime_error &e)
    {
        throw QMException(_(e.what()));
    }
}

void AddDiskWindow::Get_data_from_db()
{
    std::unique_ptr<QemuDb> db(new QemuDb(dbf_));

    sql_query = "select hdd from vms where name='" + vm_name_ + "'";
    db->SelectQuery(sql_query, &guest_old_disk);
}

void AddDiskWindow::Update_db_data()
{
    std::unique_ptr<QemuDb> db(new QemuDb(dbf_));

    sql_query = "update vms set hdd='" + guest_old_disk[0] +
      "' where name='" + vm_name_ + "'";
    db->ActionQuery(sql_query);
}

void AddDiskWindow::Gen_hdd()
{
    hdd_ch = 'a';
    MapString disk = Gen_map_from_str(guest_old_disk[0]);

    if (disk.size() == 3)
        throw QMException(_("3 disks limit reached :("));

    hdd_ch += disk.size();

    guest_dir = vmdir_ + "/" + vm_name_;
    create_img_cmd = "qemu-img create -f qcow2 " + guest_dir +
        "/" + vm_name_ + "_" + hdd_ch + ".img " + guest_new.disk + "G > /dev/null 2>&1";

    cmd_exit_status = system(create_img_cmd.c_str());

    if (cmd_exit_status != 0)
    {
        Delete_form();
        throw QMException(_("Can't create img file"));
    }

    guest_old_disk[0] += vm_name_ + "_" + hdd_ch + ".img=" + guest_new.disk + ";";
}

void AddDiskWindow::Print()
{
    finish.store(false);

    try
    {
        Draw_title();
        Create_fields();
        Enable_color();
        Config_fields_type();
        Post_form(22);
        Print_fields_names();
        Draw_form();

        if (!action_ok)
        {
            Delete_form();
            return;
        }

        Get_data_from_form();
        Get_data_from_db();

        if (guest_new.disk.empty())
            throw QMException(_("Null disk size"));

        try
        {
            if (std::stol(guest_new.disk) <= 0 || std::stoul(guest_new.disk) >= disk_free(vmdir_))
                throw QMException(_("Wrong disk size"));
        }
        catch (const std::logic_error)
        {
            throw QMException(_("Invalid data"));
        }

        getmaxyx(stdscr, row, col);
        std::thread spin_thr(spinner, 1, (col + str_size + 2) / 2);

        try
        {
            Gen_hdd();
            Update_db_data();
        }
        catch (...)
        {
            finish.store(true);
            spin_thr.join();
            throw;
        }

        finish.store(true);
        spin_thr.join();
        Delete_form();
    }
    catch (QMException &err)
    {
        ExceptionExit(err);
    }
}

} // namespace QManager
/* vim:set ts=4 sw=4 fdm=marker: */
