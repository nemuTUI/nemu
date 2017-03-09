#include <libintl.h>

#include "edit_window.h"

namespace QManager {

EditVmWindow::EditVmWindow(
    const std::string &dbf, const std::string &vmdir, const std::string &vm_name,
    int height, int width, int starty
) : QMFormWindow(height, width, starty)
{
    dbf_ = dbf;
    vmdir_ = vmdir;
    vm_name_ = vm_name;

    field.resize(10);
}

void EditVmWindow::Create_fields()
{
    for (size_t i = 0; i < field.size() - 1; ++i)
    {
        field[i] = new_field(1, 38, (i+1)*2, 5, 0, 0);
        set_field_back(field[i], A_UNDERLINE);
    }

    field[field.size() - 1] = NULL;
}

void EditVmWindow::Config_fields_type()
{
    UdevList = NULL;

    // Convert MapString to *char
    if (u_dev)
    {
        int i = 0;
        UdevList = new char *[u_dev->size() + 1];
        UdevList[u_dev->size()] = NULL;

        for (auto &usb_dev : *u_dev)
        {
            UdevList[i] = new char[usb_dev.first.size() +1];
            memcpy(UdevList[i], usb_dev.first.c_str(), usb_dev.first.size() + 1);
            i++;
        }
    }

    set_field_type(field[0], TYPE_INTEGER, 0, 1, cpu_count());
    set_field_type(field[1], TYPE_INTEGER, 0, 64, total_memory());
    set_field_type(field[2], TYPE_ENUM, (char **)YesNo, false, false);
    set_field_type(field[3], TYPE_ENUM, (char **)YesNo, false, false);
    set_field_type(field[4], TYPE_INTEGER, 1, 0, 64);
    set_field_type(field[5], TYPE_ENUM, (char **)drive_ints, false, false);
    set_field_type(field[6], TYPE_ENUM, (char **)YesNo, false, false);
    set_field_type(field[7], TYPE_ENUM, UdevList, false, false);
    set_field_type(field[8], TYPE_ENUM, (char **)YesNo, false, false);

    if (!u_dev)
    {
        field_opts_off(field[6], O_ACTIVE);
        field_opts_off(field[7], O_ACTIVE);
    }

    if (u_dev)
    {
        for (size_t i = 0; i < u_dev->size(); ++i)
            delete [] UdevList[i];

        delete [] UdevList;
    }
}

void EditVmWindow::Config_fields_buffer()
{
    MapStringVector old_ifaces = Read_ifaces_from_json(guest_old[SQL_IDX_MAC]);
    ints_count = old_ifaces.size();

    char cints[64] = {0};
    snprintf(cints, sizeof(cints), "%u", ints_count);

    set_field_buffer(field[0], 0, guest_old[SQL_IDX_SMP].c_str());
    set_field_buffer(field[1], 0, guest_old[SQL_IDX_MEM].c_str());
    set_field_buffer(field[5], 0, guest_old[SQL_IDX_DINT].c_str());
    set_field_buffer(field[4], 0, cints);

    if (guest_old[SQL_IDX_KVM] == "1")
        set_field_buffer(field[2], 0, YesNo[0]);
    else
        set_field_buffer(field[2], 0, YesNo[1]);

    if (guest_old[SQL_IDX_HCPU] == "1")
        set_field_buffer(field[3], 0, YesNo[0]);
    else
        set_field_buffer(field[3], 0, YesNo[1]);

    if (guest_old[SQL_IDX_USBF] == "1")
        set_field_buffer(field[6], 0, YesNo[0]);
    else
        set_field_buffer(field[6], 0, YesNo[1]);

    if (guest_old[SQL_IDX_OVER] == "1")
        set_field_buffer(field[8], 0, YesNo[0]);
    else
        set_field_buffer(field[8], 0, YesNo[1]);

    field_opts_off(field[7], O_STATIC);

    for (size_t i = 0; i < field.size() - 1; ++i)
        set_field_status(field[i], false);
}

void EditVmWindow::Get_data_from_form()
{
    try
    {
        guest_new.cpus.assign(trim_field_buffer(field_buffer(field[0], 0), true));
        guest_new.memo.assign(trim_field_buffer(field_buffer(field[1], 0), true));
        guest_new.kvmf.assign(trim_field_buffer(field_buffer(field[2], 0), true));
        guest_new.hcpu.assign(trim_field_buffer(field_buffer(field[3], 0), true));
        guest_new.ints.assign(trim_field_buffer(field_buffer(field[4], 0), true));
        guest_new.drvint.assign(trim_field_buffer(field_buffer(field[5], 0), true));
        guest_new.usbp.assign(trim_field_buffer(field_buffer(field[6], 0), true));
        if (guest_new.usbp == "yes")
            guest_new.usbd.assign(trim_field_buffer(field_buffer(field[7], 0), false));
        guest_new.mouse.assign(trim_field_buffer(field_buffer(field[8], 0), true));
    }
    catch (const std::runtime_error &e)
    {
        throw QMException(_(e.what()));
    }
}

void EditVmWindow::Print_fields_names()
{
    char ccpu[128] = {0};
    char cmem[128] = {0};

    snprintf(ccpu, sizeof(ccpu), "%s%u%s", _("CPU cores [1-"), cpu_count(), "]");
    snprintf(cmem, sizeof(cmem), "%s%u%s", _("Memory [64-"), total_memory(), "]Mb");

    mvwaddstr(window, 2, 26, (vm_name_ + _(" settings:")).c_str());
    mvwaddstr(window, 4, 2, ccpu);
    mvwaddstr(window, 6, 2, cmem);
    mvwaddstr(window, 8, 2, _("KVM [yes/no]"));
    mvwaddstr(window, 10, 2, _("Host CPU [yes/no]"));
    mvwaddstr(window, 12, 2, _("Interfaces"));
    mvwaddstr(window, 14, 2, _("Disk interface"));
    mvwaddstr(window, 16, 2, _("USB [yes/no]"));
    mvwaddstr(window, 18, 2, _("USB device"));
    mvwaddstr(window, 20, 2, _("Sync mouse position"));
}

void EditVmWindow::Get_data_from_db()
{
    std::unique_ptr<QemuDb> db(new QemuDb(dbf_));

    sql_query = "SELECT * FROM vms WHERE name='" + vm_name_ + "'";
    db->SelectQuery(sql_query, &guest_old);

    sql_query = "SELECT mac FROM lastval";
    db->SelectQuery(sql_query, &v_last_mac);
}

void EditVmWindow::Gen_iface_json()
{
    MapStringVector old_ifaces = Read_ifaces_from_json(guest_old[SQL_IDX_MAC]);
    VectorString ndrv;
    size_t old_if_count = old_ifaces.size();

    if (old_if_count > ui_vm_ints)
    {
        size_t n = 0;
        for (auto it : old_ifaces)
        {
            ndrv.push_back(it.second[1]);
            ++n;
            if (n == ui_vm_ints)
                break;
        }
    }
    else if (old_if_count < ui_vm_ints)
    {
        size_t count_diff = ui_vm_ints - old_if_count;

        for (auto it : old_ifaces)
            ndrv.push_back(it.second[1]);

        for (size_t i = 0; i < count_diff; ++i)
            ndrv.push_back(DEFAULT_NETDRV);
    }

    guest_new.ints.clear();
    size_t i = 0;
    for (auto &ifs : ifaces)
    {
        guest_new.ints += "{\"name\":\"" + ifs.first + "\",\"mac\":\"" +
            ifs.second + "\",\"drv\":\"" + ndrv[i] + "\"},";

        i++;
    }

    guest_new.ints.erase(guest_new.ints.find_last_not_of(",") + 1);
    guest_new.ints = "{\"ifaces\":[" + guest_new.ints + "]}";
}

void EditVmWindow::Update_db_data()
{
    std::unique_ptr<QemuDb> db(new QemuDb(dbf_));

    if (field_status(field[0]))
    {
        sql_query = "update vms set smp='" + guest_new.cpus +
            "' where name='" + vm_name_ + "'";
        db->ActionQuery(sql_query);
    }

    if (field_status(field[1]))
    {
        sql_query = "update vms set mem='" + guest_new.memo +
            "' where name='" + vm_name_ + "'";
        db->ActionQuery(sql_query);
    }

    if (field_status(field[2]))
    {
        if (guest_new.kvmf == "yes")
            guest_new.kvmf = "1";
        else
            guest_new.kvmf = "0";

        sql_query = "update vms set kvm='" + guest_new.kvmf +
            "' where name='" + vm_name_ + "'";
        db->ActionQuery(sql_query);
    }

    if (field_status(field[3]))
    {
        if (guest_new.hcpu == "yes")
            guest_new.hcpu = "1";
        else
            guest_new.hcpu = "0";

        sql_query = "update vms set hcpu='" + guest_new.hcpu +
            "' where name='" + vm_name_ + "'";
        db->ActionQuery(sql_query);
    }

    if (field_status(field[4]))
    {
        ui_vm_ints = std::stoi(guest_new.ints);

        if (ui_vm_ints != ints_count)
        {
            Gen_mac_address(ui_vm_ints, vm_name_);
            Gen_iface_json();

            if (ui_vm_ints != 0)
            {
                sql_query = "update lastval set mac='" + std::to_string(last_mac) + "'";
                db->ActionQuery(sql_query);
            }

            sql_query = "update vms set mac='" + guest_new.ints +
                "' where name='" + vm_name_ + "'";
            db->ActionQuery(sql_query);
        }
    }

    if (field_status(field[5]))
    {
        sql_query = "update vms set drive_interface='" + guest_new.drvint +
            "' where name='" + vm_name_ + "'";
        db->ActionQuery(sql_query);
    }

    if (field_status(field[6]))
    {
        if (guest_new.usbp == "yes")
        {
            if (!field_status(field[7]))
            {
                Delete_form();
                throw QMException(_("Usb device was not selected."));
            }
            guest_new.usbp = "1";
            guest_new.usbd = u_dev->at(guest_new.usbd);
        }
        else
        {
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

    if (field_status(field[8]))
    {
        if (guest_new.mouse == "yes")
            guest_new.mouse = "1";
        else
            guest_new.mouse = "0";

        sql_query = "update vms set mouse_override='" + guest_new.mouse +
            "' where name='" + vm_name_ + "'";
        db->ActionQuery(sql_query);
    }
}

void EditVmWindow::Print()
{
    finish.store(false);

    try
    {
        u_dev = get_usb_list();

        Enable_color();

        Draw_title();
        Get_data_from_db();
        Create_fields();
        Config_fields_type();
        Config_fields_buffer();
        Post_form(21);
        Print_fields_names();
        Draw_form();

        if (!action_ok)
        {
            Delete_form();
            curs_set(0);
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
