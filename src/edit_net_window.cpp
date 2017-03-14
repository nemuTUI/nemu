#include <libintl.h>

#include "edit_net_window.h"

namespace QManager {

EditNetWindow::EditNetWindow(
    const std::string &dbf, const std::string &vmdir, const std::string &vm_name,
    int height, int width, int starty
) : QMFormWindow(height, width, starty)
{
    dbf_ = dbf;
    vmdir_ = vmdir;
    vm_name_ = vm_name;

    field.resize(5);
}

void EditNetWindow::Create_fields()
{
    for (size_t i = 0; i < field.size() - 1; ++i)
        field[i] = new_field(1, 22, i*2, 2, 0, 0);

    field[field.size() - 1] = NULL;
}

void EditNetWindow::Get_data_from_db()
{
    std::unique_ptr<QemuDb> db(new QemuDb(dbf_));

    sql_query = "select mac from vms where name='" + vm_name_ + "'";
    db->SelectQuery(sql_query, &guest_old_ints);

    sql_query = "select mac from vms";
    db->SelectQuery(sql_query, &all_ints);
}

void EditNetWindow::Config_fields_type()
{
    ifs = Read_ifaces_from_json(guest_old_ints[0]);

    for (auto i : ifs)
        iflist.push_back(i.first);

    IfaceList = new char *[iflist.size() + 1];

    for (size_t i = 0; i < iflist.size(); ++i)
    {
        IfaceList[i] = new char[iflist[i].size() + 1];
        memcpy(IfaceList[i], iflist[i].c_str(), iflist[i].size() + 1);
    }

    IfaceList[iflist.size()] = NULL;

    set_field_type(field[0], TYPE_ENUM, (char **)IfaceList, false, false);
    set_field_type(field[1], TYPE_ENUM, (char **)NetDrv, false, false);
    set_field_type(field[2], TYPE_REGEXP, ".*");
    set_field_type(field[3], TYPE_REGEXP, ".*");
    field_opts_off(field[2], O_STATIC);
    field_opts_off(field[3], O_STATIC);

    for (size_t i = 0; i < iflist.size(); ++i)
        delete [] IfaceList[i];

    delete [] IfaceList;

    for (size_t i = 0; i < field.size() - 1; ++i)
        set_field_status(field[i], false);
}

void EditNetWindow::Print_fields_names()
{
    mvwaddstr(window, 2, 2, _("Interface"));
    mvwaddstr(window, 4, 2, _("Net driver"));
    mvwaddstr(window, 6, 2, _("Mac address"));
    mvwaddstr(window, 8, 2, _("IPv4 address"));
}

void EditNetWindow::Get_data_from_form()
{
    guest_new.name.assign(trim_field_buffer(field_buffer(field[0], 0), false));
    guest_new.ndrv.assign(trim_field_buffer(field_buffer(field[1], 0), false));
    guest_new.imac.assign(trim_field_buffer(field_buffer(field[2], 0), false));
    guest_new.ipv4.assign(trim_field_buffer(field_buffer(field[3], 0), false));
}

void EditNetWindow::Gen_iface_json()
{
    guest_new.ints.clear();

    for (auto &i : ifs)
    {
        guest_new.ints += "{\"name\":\"" + i.first + "\",\"mac\":\"" +
            i.second[IFS_MAC] + "\",\"drv\":\"" + i.second[IFS_DRV] +
            "\",\"ip4\":\"" + i.second[IFS_IPV4] + "\"},";
    }

    guest_new.ints.erase(guest_new.ints.find_last_not_of(",") + 1);
    guest_new.ints = "{\"ifaces\":[" + guest_new.ints + "]}";
}

void EditNetWindow::Update_db_data()
{
    std::unique_ptr<QemuDb> db(new QemuDb(dbf_));

    if (field_status(field[1]))
    {
        if (!field_status(field[0]))
            throw QMException(_("Null network interface"));

        auto it = ifs.find(guest_new.name);

        if (it == ifs.end())
            throw QMException(_("Something goes wrong"));

        it->second[IFS_DRV] = guest_new.ndrv;

        Gen_iface_json();

        sql_query = "update vms set mac='" + guest_new.ints +
            "' where name='" + vm_name_ + "'";
        db->ActionQuery(sql_query);
    }

    if (field_status(field[2]))
    {
        if (!field_status(field[0]))
            throw QMException(_("Null network interface"));

        if (!verify_mac(guest_new.imac))
            throw QMException(_("Wrong mac address"));

        auto it = ifs.find(guest_new.name);

        if (it == ifs.end())
            throw QMException(_("Something goes wrong"));

        std::unordered_set<std::string> mac_db;

        for (size_t i = 0; i < all_ints.size(); ++i)
        {
            MapStringVector ints = Read_ifaces_from_json(all_ints[i]);
    
            for (auto i : ints)
                mac_db.insert(i.second[IFS_MAC]);
        }

        auto mac_it = mac_db.find(guest_new.imac);

        if (mac_it != mac_db.end())
            throw QMException(_("This mac is already used!"));

        it->second[IFS_MAC] = guest_new.imac;

        Gen_iface_json();

        sql_query = "update vms set mac='" + guest_new.ints +
            "' where name='" + vm_name_ + "'";
        db->ActionQuery(sql_query);
    }

    if (field_status(field[3]))
    {
        if (!field_status(field[0]))
            throw QMException(_("Null network interface"));

        auto it = ifs.find(guest_new.name);

        if (it == ifs.end())
            throw QMException(_("Something goes wrong"));

        it->second[IFS_IPV4] = guest_new.ipv4;

        Gen_iface_json();

        sql_query = "update vms set mac='" + guest_new.ints +
            "' where name='" + vm_name_ + "'";
        db->ActionQuery(sql_query);
    }
}

void EditNetWindow::Print()
{
    finish.store(false);

    try
    {
        Draw_title();
        Create_fields();
        Enable_color();
        Get_data_from_db();
        Config_fields_type();
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

        try {
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
