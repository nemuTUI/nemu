#include <libintl.h>

#include "clone_window.h"

namespace QManager {

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
    char cname[64] = {0};
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
    guest_new.name.assign(trim_field_buffer(field_buffer(field[0], 0), false));
}

void CloneVmWindow::Get_data_from_db()
{
    std::unique_ptr<QemuDb> db(new QemuDb(dbf_));

    sql_query = "SELECT * FROM vms WHERE name='" + vm_name_ + "'";
    db->SelectQuery(sql_query, &guest_old);

    sql_query = "SELECT mac FROM lastval";
    db->SelectQuery(sql_query, &v_last_mac);

    sql_query = "SELECT vnc FROM lastval";
    db->SelectQuery(sql_query, &v_last_vnc);

    sql_query = "SELECT id FROM vms WHERE name='" + guest_new.name + "'";
    db->SelectQuery(sql_query, &v_name);

    last_vnc = std::stoi(v_last_vnc[0]);
    last_vnc++;
}

void CloneVmWindow::Gen_iface_json()
{
    MapStringVector old_ifaces = Read_ifaces_from_json(guest_old[SQL_IDX_MAC]);
    VectorString ndrv;
    size_t i = 0;

    for (auto &old_ifs : old_ifaces)
        ndrv.push_back(old_ifs.second[1]);

    guest_new.ints.clear();

    for (auto &ifs : ifaces)
    {
        guest_new.ints += "{\"name\":\"" + ifs.first + "\",\"mac\":\"" +
            ifs.second + "\",\"drv\":\"" + ndrv[i] + "\"},";

        i++;
    }

    guest_new.ints.erase(guest_new.ints.find_last_not_of(",") + 1);
    guest_new.ints = "{\"ifaces\":[" + guest_new.ints + "]}";
}

void CloneVmWindow::Update_db_data()
{
    const std::array<std::string, 16> columns = {
        "mem", "smp", "kvm", "hcpu", "arch", "iso",
        "install", "usb", "usbid", "bios", "kernel",
        "mouse_override", "drive_interface", "kernel_append",
        "tty_path", "socket_path"
    };

    std::unique_ptr<QemuDb> db(new QemuDb(dbf_));

    sql_query = "INSERT INTO vms(name) VALUES('" + guest_new.name + "')";
    db->ActionQuery(sql_query);

    sql_query = "UPDATE lastval SET mac='" + std::to_string(last_mac) + "'";
    db->ActionQuery(sql_query);

    sql_query = "UPDATE lastval SET vnc='" + std::to_string(last_vnc) + "'";
    db->ActionQuery(sql_query);

    for (auto &c : columns)
    {
        sql_query = "UPDATE vms SET " + c + "=(SELECT " + c + " FROM vms WHERE name='" +
            vm_name_ + "') where name='" + guest_new.name + "'";
        db->ActionQuery(sql_query);
    }

    sql_query = "UPDATE vms SET mac='" + guest_new.ints +
        "' WHERE name='" + guest_new.name + "'";
    db->ActionQuery(sql_query);

    sql_query = "UPDATE vms SET vnc='" + std::to_string(last_vnc) +
        "' WHERE name='" + guest_new.name + "'";
    db->ActionQuery(sql_query);

    sql_query = "UPDATE vms SET hdd='" + guest_new.disk +
        "' WHERE name='" + guest_new.name + "'";
    db->ActionQuery(sql_query);
}

void CloneVmWindow::Gen_hdd()
{
    hdd_ch = 'a';
    MapString disk = Gen_map_from_str(guest_old[SQL_IDX_HDD]);
    int unused __attribute__((unused));

    guest_dir = vmdir_ + "/" + guest_new.name;
    create_guest_dir_cmd = "mkdir " + guest_dir;

    unused = system(create_guest_dir_cmd.c_str());

    for (auto &hd : disk)
    {
        guest_new.disk += guest_new.name + "_" + hdd_ch +
            ".img=" + hd.second + ";";

        create_img_cmd = "cp " + vmdir_ + "/" + vm_name_ +
            "/" + hd.first + " " + vmdir_ + "/" + guest_new.name +
            "/" + guest_new.name + "_" + hdd_ch + ".img";

        unused = system(create_img_cmd.c_str());
        hdd_ch++;
    }
}

void CloneVmWindow::Print()
{
    finish.store(false);

    try
    {
        Draw_title();
        Create_fields();
        Enable_color();
        Config_fields_type();
        Config_fields_buffer();
        Post_form(10);
        Print_fields_names();
        Draw_form();

        if (!action_ok)
        {
            Delete_form();
            return;
        }

        Get_data_from_form();
        Get_data_from_db();

        if (guest_new.name.empty())
            throw QMException(_("Null guest name"));

        if (!v_name.empty())
            throw QMException(_("This name is already used"));

        getmaxyx(stdscr, row, col);
        std::thread spin_thr(spinner, 1, (col + str_size + 2) / 2);

        Gen_mac_address(Read_ifaces_from_json(guest_old[SQL_IDX_MAC]).size(),
                        guest_new.name);
        Gen_iface_json();

        Gen_hdd();
        Update_db_data();
        finish.store(true);
        spin_thr.join();

        Delete_form();
    }
    catch (QMException &err)
    {
        ExceptionExit(err);
    }
}
}// namespace QManager
/* vim:set ts=4 sw=4 fdm=marker: */
