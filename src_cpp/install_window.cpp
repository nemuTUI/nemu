#include <libintl.h>

#include "install_window.h"

namespace QManager {

EditInstallWindow::EditInstallWindow(const std::string &dbf,
     const std::string &vmdir, 
     const std::string &vm_name,
     int height, int width,
     int starty) :
     QMFormWindow(height, width, starty)
{
    dbf_= dbf;
    vmdir_ = vmdir;
    vm_name_ = vm_name;

    field.resize(8);
}

void EditInstallWindow::Create_fields()
{
    for (size_t i = 0; i < field.size() - 1; ++i)
    {
        field[i] = new_field(1, 41, i*2, 5, 0, 0);
        set_field_back(field[i], A_UNDERLINE);
    }

    field[field.size() - 1] = NULL;
}

void EditInstallWindow::Config_fields_type()
{
    field_opts_off(field[1], O_STATIC);
    set_field_type(field[0], TYPE_ENUM, (char **)YesNo, false, false);
    set_field_type(field[1], TYPE_REGEXP, "^/.*");
    set_field_type(field[2], TYPE_REGEXP, "^/.*");
    set_field_type(field[3], TYPE_REGEXP, "^/.*");
    set_field_type(field[4], TYPE_REGEXP, ".*");
    set_field_type(field[5], TYPE_REGEXP, "^/.*");
    set_field_type(field[6], TYPE_REGEXP, "^/.*");
}

void EditInstallWindow::Config_fields_buffer()
{
    if (guest_old[SQL_IDX_INST] == "1")
        set_field_buffer(field[0], 0, YesNo[1]);
    else
        set_field_buffer(field[0], 0, YesNo[0]);

    set_field_buffer(field[1], 0, guest_old[SQL_IDX_ISO].c_str());
    set_field_buffer(field[2], 0, guest_old[SQL_IDX_BIOS].c_str());
    set_field_buffer(field[3], 0, guest_old[SQL_IDX_KERN].c_str());
    set_field_buffer(field[4], 0, guest_old[SQL_IDX_KAPP].c_str());
    set_field_buffer(field[5], 0, guest_old[SQL_IDX_TTY].c_str());
    set_field_buffer(field[6], 0, guest_old[SQL_IDX_SOCK].c_str());

    for (size_t i = 0; i < field.size() - 1; ++i)
        set_field_status(field[i], false);
    for (size_t i = 1; i < field.size() - 1; ++i)
        field_opts_off(field[i], O_STATIC);
}

void EditInstallWindow::Print_fields_names()
{
    mvwaddstr(window, 2, 2, _("OS Installed"));
    mvwaddstr(window, 4, 2, _("Path to ISO/IMG"));
    mvwaddstr(window, 6, 2, _("Path to BIOS"));
    mvwaddstr(window, 8, 2, _("Path to kernel"));
    mvwaddstr(window, 10, 2, _("Kernel cmdline"));
    mvwaddstr(window, 12, 2, _("Serial TTY"));
    mvwaddstr(window, 14, 2, _("Serial socket"));
}

void EditInstallWindow::Get_data_from_form()
{
    guest_new.install.assign(trim_field_buffer(field_buffer(field[0], 0), false));
    guest_new.path.assign(trim_field_buffer(field_buffer(field[1], 0), false));
    guest_new.bios.assign(trim_field_buffer(field_buffer(field[2], 0), false));
    guest_new.kpath.assign(trim_field_buffer(field_buffer(field[3], 0), false));
    guest_new.kappend.assign(trim_field_buffer(field_buffer(field[4], 0), false));
    guest_new.tty.assign(trim_field_buffer(field_buffer(field[5], 0), false));
    guest_new.socket.assign(trim_field_buffer(field_buffer(field[6], 0), false));
}

void EditInstallWindow::Get_data_from_db()
{
    std::unique_ptr<QemuDb> db(new QemuDb(dbf_));

    sql_query = "SELECT * FROM vms WHERE name='" + vm_name_ + "'";
    db->SelectQuery(sql_query, &guest_old);
}

void EditInstallWindow::Update_db_data()
{
    std::unique_ptr<QemuDb> db(new QemuDb(dbf_));

    if (field_status(field[0]))
    {
        if (guest_new.install == "yes")
            guest_new.install = "0";
        else
            guest_new.install = "1";

        sql_query = "UPDATE vms SET install='" + guest_new.install +
            "' WHERE name='" + vm_name_ + "'";
        db->ActionQuery(sql_query);
    }

    if (field_status(field[1]))
    {
        sql_query = "UPDATE vms SET iso='" + guest_new.path +
            "' WHERE name='" + vm_name_ + "'";
        db->ActionQuery(sql_query);
    }

    if (field_status(field[2]))
    {
        sql_query = "UPDATE vms SET bios='" + guest_new.bios +
            "' WHERE name='" + vm_name_ + "'";
        db->ActionQuery(sql_query);
    }

    if (field_status(field[3]))
    {
        sql_query = "UPDATE vms SET kernel='" + guest_new.kpath +
            "' WHERE name='" + vm_name_ + "'";
        db->ActionQuery(sql_query);
    }

    if (field_status(field[4]))
    {
        sql_query = "UPDATE vms SET kernel_append='" + guest_new.kappend +
            "' WHERE name='" + vm_name_ + "'";
        db->ActionQuery(sql_query);
    }

    if (field_status(field[5]))
    {
        sql_query = "UPDATE vms SET tty_path='" + guest_new.tty +
            "' WHERE name='" + vm_name_ + "'";
        db->ActionQuery(sql_query);
    }

    if (field_status(field[6]))
    {
        sql_query = "UPDATE vms SET socket_path='" + guest_new.socket +
            "' WHERE name='" + vm_name_ + "'";
        db->ActionQuery(sql_query);
    }
}

void EditInstallWindow::Print()
{
    finish.store(false);

    try
    {
        Draw_title();
        Create_fields();
        Enable_color();
        Get_data_from_db();
        Config_fields_type();
        Config_fields_buffer();
        Post_form(18);
        Print_fields_names();
        Draw_form();

        if (!action_ok)
        {
            Delete_form();
            return;
        }

        Get_data_from_form();

        getmaxyx(stdscr, row, col);
        std::thread spin_thr(spinner, 1, (col + str_size + 2) / 2);

        try
        {
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
