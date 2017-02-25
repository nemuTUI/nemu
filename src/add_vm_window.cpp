#include <libintl.h>

#include "add_vm_window.h"

namespace QManager {

AddVmWindow::AddVmWindow(const std::string &dbf, const std::string &vmdir,
                         int height, int width, int starty) :
                         QMFormWindow(height, width, starty)
{
    dbf_ = dbf;
    vmdir_ = vmdir;

    field.resize(11);
}

void AddVmWindow::Gen_iface_json()
{
    guest.ints.clear();

    for (auto &ifs : ifaces)
    {
        guest.ints += "{\"name\":\"" + ifs.first + "\",\"mac\":\"" +
            ifs.second + "\",\"drv\":\"" + guest.ndrv + "\"},";
    }

    guest.ints.erase(guest.ints.find_last_not_of(",") + 1);
    guest.ints = "{\"ifaces\":[" + guest.ints + "]}";
}

void AddVmWindow::Create_fields() 
{
    for (size_t i = 0; i < field.size() - 1; ++i)
    {
        field[i] = new_field(1, 35, i*2, 1, 0, 0);
        set_field_back(field[i], A_UNDERLINE);
    }

    field[field.size() - 1] = NULL;
}

void AddVmWindow::Config_fields_type()
{
    UdevList = NULL;
    const VectorString *q_arch = &get_cfg()->qemu_targets;
    ArchList = new char *[q_arch->size() + 1];

    // Convert VectorString to *char
    for (size_t i = 0; i < q_arch->size(); ++i)
    {
        ArchList[i] = new char[q_arch[i].size() + 1];
        memcpy(ArchList[i], (*q_arch)[i].c_str(), (*q_arch)[i].size() + 1);
    }

    // Convert MapString to *char
    if (u_dev)
    {
        int i = 0;
        UdevList = new char *[u_dev->size() + 1];
        UdevList[u_dev->size()] = NULL;

        for (auto &usb_dev : *u_dev)
        {
            UdevList[i] = new char[usb_dev.first.size() + 1];
            memcpy(UdevList[i], usb_dev.first.c_str(), usb_dev.first.size() + 1);
            i++;
        }

    }

    ArchList[q_arch->size()] = NULL;

    set_field_type(field[0], TYPE_ALNUM, 0);
    set_field_type(field[1], TYPE_ENUM, ArchList, false, false);
    set_field_type(field[2], TYPE_INTEGER, 0, 1, cpu_count());
    set_field_type(field[3], TYPE_INTEGER, 0, 64, total_memory());
    set_field_type(field[4], TYPE_INTEGER, 0, 1, disk_free(vmdir_));
    set_field_type(field[5], TYPE_REGEXP, "^/.*");
    set_field_type(field[6], TYPE_INTEGER, 0, 0, 64);
    set_field_type(field[7], TYPE_ENUM, (char **)NetDrv, false, false);
    set_field_type(field[8], TYPE_ENUM, (char **)YesNo, false, false);
    set_field_type(field[9], TYPE_ENUM, UdevList, false, false);

    if (!u_dev)
    {
        field_opts_off(field[8], O_ACTIVE);
        field_opts_off(field[9], O_ACTIVE);
    }

    for (size_t i = 0; i < q_arch->size(); ++i)
        delete [] ArchList[i];

    delete [] ArchList;

    if (u_dev)
    {
        for (size_t i = 0; i < u_dev->size(); ++i)
            delete [] UdevList[i];

        delete [] UdevList;
    }
}

void AddVmWindow::Config_fields_buffer()
{
    set_field_buffer(field[2], 0, "1");
    set_field_buffer(field[6], 0, "1");
    set_field_buffer(field[7], 0, DEFAULT_NETDRV);
    set_field_buffer(field[8], 0, "no");
    field_opts_off(field[0], O_STATIC);
    field_opts_off(field[5], O_STATIC);
    field_opts_off(field[9], O_STATIC);
    set_max_field(field[0], 30);
}

void AddVmWindow::Print_fields_names()
{
    char ccpu[128], cmem[128], cfree[128];
    snprintf(ccpu, sizeof(ccpu), "%s%u%s", _("CPU cores [1-"), cpu_count(), "]");
    snprintf(cmem, sizeof(cmem), "%s%u%s", _("Memory [64-"), total_memory(), "]Mb");
    snprintf(cfree, sizeof(cfree), "%s%u%s", _("Disk [1-"), disk_free(vmdir_), "]Gb");

    mvwaddstr(window, 2, 2, _("Name"));
    mvwaddstr(window, 4, 2, _("Architecture"));
    mvwaddstr(window, 6, 2, ccpu);
    mvwaddstr(window, 8, 2, cmem);
    mvwaddstr(window, 10, 2, cfree);
    mvwaddstr(window, 12, 2, _("Path to ISO/IMG"));
    mvwaddstr(window, 14, 2, _("Interfaces"));
    mvwaddstr(window, 16, 2, _("Net driver"));
    mvwaddstr(window, 18, 2, _("USB [yes/no]"));
    mvwaddstr(window, 20, 2, _("USB device"));
}

void AddVmWindow::Get_data_from_form()
{
    guest.name.assign(trim_field_buffer(field_buffer(field[0], 0)));
    guest.arch.assign(trim_field_buffer(field_buffer(field[1], 0)));
    guest.cpus.assign(trim_field_buffer(field_buffer(field[2], 0)));
    guest.memo.assign(trim_field_buffer(field_buffer(field[3], 0)));
    guest.disk.assign(trim_field_buffer(field_buffer(field[4], 0)));
    guest.vncp.assign(v_last_vnc[0]);
    guest.path.assign(trim_field_buffer(field_buffer(field[5], 0)));
    guest.ints.assign(trim_field_buffer(field_buffer(field[6], 0)));
    guest.ndrv.assign(trim_field_buffer(field_buffer(field[7], 0)));
    guest.usbp.assign(trim_field_buffer(field_buffer(field[8], 0)));
    guest.usbd.assign(trim_field_buffer(field_buffer(field[9], 0)));
}

void AddVmWindow::Get_data_from_db()
{
    std::unique_ptr<QemuDb> db(new QemuDb(dbf_));
    sql_query = "select vnc from lastval";
    v_last_vnc = db->SelectQuery(sql_query); // TODO: add check if null exeption

    sql_query = "select mac from lastval";
    v_last_mac = db->SelectQuery(sql_query); // TODO: add check if null exeption

    last_mac = std::stoull(v_last_mac[0]);
    last_vnc = std::stoi(v_last_vnc[0]);
    last_vnc++;
}

void AddVmWindow::Update_db_data()
{
    if (guest.usbp == "yes")
    {
        guest.usbp = "1";
        guest.usbd = u_dev->at(guest.usbd);
    }
    else
    {
        guest.usbp = "0";
        guest.usbd = "none";
    }

    guest.disk = guest.name + "_a.img=" + guest.disk + ";";

    guest.kvmf = "1"; //Enable KVM by default
    guest.hcpu = "0"; //Disable Host CPU by default

    std::unique_ptr<QemuDb> db(new QemuDb(dbf_));

    sql_query = "update lastval set mac='" + std::to_string(last_mac) + "'";
    db->ActionQuery(sql_query);

    sql_query = "update lastval set vnc='" + std::to_string(last_vnc) + "'";
    db->ActionQuery(sql_query);

    // Add guest to database
    sql_query = "insert into vms("
        "name, mem, smp, hdd, kvm, hcpu, vnc, mac, arch, iso, install, usb, usbid"
        ") values('"
        + guest.name + "', '" + guest.memo + "', '" + guest.cpus + "', '"
        + guest.disk + "', '" + guest.kvmf + "', '" + guest.hcpu + "', '"
        + guest.vncp + "', '" + guest.ints + "', '" + guest.arch + "', '"
        + guest.path + "', '1', '" + guest.usbp + "', '" + guest.usbd + "')";

    db->ActionQuery(sql_query);
}

void AddVmWindow::Gen_hdd()
{
    guest_dir = vmdir_ + "/" + guest.name;
    create_guest_dir_cmd = "mkdir " + guest_dir + " >/dev/null 2>&1";
    create_img_cmd = "qemu-img create -f qcow2 " + guest_dir +
        "/" + guest.name + "_a.img " + guest.disk + "G > /dev/null 2>&1";

    cmd_exit_status = system(create_guest_dir_cmd.c_str());

    if (cmd_exit_status != 0)
    {
        Delete_form();
        throw QMException(_("Can't create guest dir"));
    }

    cmd_exit_status = system(create_img_cmd.c_str());

    if (cmd_exit_status != 0)
    {
        Delete_form();
        throw QMException(_("Can't create img file"));
    }
}

void AddVmWindow::Check_input_data()
{
    std::unique_ptr<QemuDb> db(new QemuDb(dbf_));
    sql_query = "select id from vms where name='" + guest.name + "'";
    v_name = db->SelectQuery(sql_query);

    if (!v_name.empty())
    {
        Delete_form();
        throw QMException(_("This name is already used"));
    }

    if (guest.usbp == "yes")
    {
        for (size_t i = 0; i < 10; ++i)
        {
            if (!field_status(field[i]))
            {
                Delete_form();
                throw QMException(_("Must fill all params"));
            }
        }
    }
    else
    {
        for (size_t i = 0; i < 8; ++i)
        {
            if (!field_status(field[i]))
            {
                Delete_form();
                throw QMException(_("Must fill all params"));
            }
        }
    }
}

void AddVmWindow::Print()
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

        Get_data_from_form();
        Check_input_data();

        getmaxyx(stdscr, row, col);
        std::thread spin_thr(spinner, 1, (col + str_size + 2) / 2);

        try
        {
            Gen_hdd();
            Gen_mac_address(guest, std::stoi(guest.ints), guest.name);
            Gen_iface_json();
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
        ExeptionExit(err);
    }

    curs_set(0);
}

} // namespace QManager
