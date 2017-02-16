#include <libintl.h>

#include "guest.h"

namespace QManager {

void start_guest(const std::string &vm_name,
                 const std::string &dbf,
                 const std::string &vmdir)
{
    guest_t<VectorString> guest;

    std::string lock_file = vmdir + "/" + vm_name + "/" + vm_name + ".lock";
    std::string guest_dir = vmdir + "/" + vm_name + "/";

    std::unique_ptr<QemuDb> db(new QemuDb(dbf));

    // Check if system is already installed
    std::string sql_query = "select install from vms where name='" + vm_name + "'";
    guest.install = db->SelectQuery(sql_query);

    if (guest.install[0] == "1")
    {
        uint32_t ch;
        std::unique_ptr<PopupWarning> Warn(new PopupWarning(_("Already installed (y/n)"), 3, 25, 7));
        
        Warn->Init();
        ch = Warn->Print(Warn->window);

        if (ch == MenuKeyY)
        {
            std::string sql_query = "update vms set install='0' where name='" + vm_name + "'";
            db->ActionQuery(sql_query);
        }
    }

    // Get guest parameters from database
    sql_query = "select arch from vms where name='" + vm_name + "'";
    guest.arch = db->SelectQuery(sql_query);
    sql_query = "select mem from vms where name='" + vm_name + "'";
    guest.memo = db->SelectQuery(sql_query);
    sql_query = "select smp from vms where name='" + vm_name + "'";
    guest.cpus = db->SelectQuery(sql_query);
    sql_query = "select hdd from vms where name='" + vm_name + "'";
    guest.disk = db->SelectQuery(sql_query);
    sql_query = "select kvm from vms where name='" + vm_name + "'";
    guest.kvmf = db->SelectQuery(sql_query);
    sql_query = "select hcpu from vms where name='" + vm_name + "'";
    guest.hcpu = db->SelectQuery(sql_query);
    sql_query = "select vnc from vms where name='" + vm_name + "'";
    guest.vncp = db->SelectQuery(sql_query);
    sql_query = "select mac from vms where name='" + vm_name + "'";
    guest.ints = db->SelectQuery(sql_query);
    sql_query = "select iso from vms where name='" + vm_name + "'";
    guest.path = db->SelectQuery(sql_query);
    sql_query = "select install from vms where name='" + vm_name + "'";
    guest.install = db->SelectQuery(sql_query);
    sql_query = "select usb from vms where name='" + vm_name + "'";
    guest.usbp = db->SelectQuery(sql_query);
    sql_query = "select usbid from vms where name='" + vm_name + "'";
    guest.usbd = db->SelectQuery(sql_query);

    // Generate strings for system shell commands
    std::string create_lock = "( touch " + lock_file + "; ";
    std::string delete_lock = " > /dev/null 2>&1; rm " + lock_file + " )&";
    std::string qemu_bin = "qemu-system-" + guest.arch[0];

    MapStringVector ints = Read_ifaces_from_json(guest.ints[0]);
    MapString disk = Gen_map_from_str(guest.disk[0]);

    std::string hdx_arg, ints_arg;
    char hdx_char= 'a';

    for (auto &hdx : disk)
    {
        std::string hdd(1, hdx_char++);
        hdx_arg += " -hd" + hdd + " " + guest_dir + hdx.first;
    }

    for (auto &ifs : ints)
    {
        ints_arg += " -net nic,macaddr=" + ifs.second[0] + ",model=" + ifs.second[1];
        ints_arg += " -net tap,ifname=" + ifs.first + ",script=no";
    }

    std::string cpu_arg, kvm_arg, hcpu_arg, install_arg, usb_arg, mem_arg, vnc_arg;
    std::stoi(guest.cpus[0]) > 1 ? cpu_arg = " -smp " + guest.cpus[0] : cpu_arg = "";
    guest.kvmf[0] == "1" ?  kvm_arg = " -enable-kvm" : kvm_arg = "";
    (guest.kvmf[0] == "1" && guest.hcpu[0] == "1") ?  hcpu_arg = " -cpu host" : hcpu_arg = "";
    guest.install[0] == "1" ?  install_arg = " -boot d -cdrom " + guest.path[0] : install_arg = "";
    guest.usbp[0] == "1" ?  usb_arg = " -usb -usbdevice host:" + guest.usbd[0] : usb_arg = "";
    mem_arg = " -m " + guest.memo[0];

    if (!get_cfg()->vnc_listen_any)
        vnc_arg = " -vnc 127.0.0.1:" + guest.vncp[0];
    else
        vnc_arg = " -vnc :" + guest.vncp[0];

    // Generate and execute complete command
    std::string guest_cmd =
        create_lock + qemu_bin + usb_arg +
        install_arg + hdx_arg + cpu_arg +
        mem_arg + kvm_arg + hcpu_arg +
        ints_arg + vnc_arg + delete_lock;

    system(guest_cmd.c_str());

    std::ofstream debug;
    debug.open("/tmp/qemu_manage.log");
    debug << guest_cmd << std::endl;
    debug.close();
}

void connect_guest(const std::string &vm_name, const std::string &dbf)
{
    guest_t<VectorString> guest;
    uint16_t port;

    std::unique_ptr<QemuDb> db(new QemuDb(dbf));
    std::string sql_query = "select vnc from vms where name='" + vm_name + "'";
    guest.vncp = db->SelectQuery(sql_query);

    port = 5900 + std::stoi(guest.vncp[0]);

    std::string connect_cmd = "vncviewer :" + std::to_string(port) + " > /dev/null 2>&1 &";

    system(connect_cmd.c_str());
}

void delete_guest(const std::string &vm_name,
                  const std::string &dbf,
                  const std::string &vmdir)
{
    std::string guest_dir = vmdir + "/" + vm_name;
    std::string guest_dir_rm_cmd = "rm -rf " + guest_dir;

    char path[PATH_MAX + 1] = {0};

    if (realpath(guest_dir.c_str(), path) == NULL)
        err_exit(_("Error get realpath."));

    /* Just in case */
    if (strcmp(path, "/") == 0)
        err_exit(_("Something goes wrong. Delete root partition prevented."));

    std::unique_ptr<QemuDb> db(new QemuDb(dbf));
    std::string sql_query = "delete from vms where name='" + vm_name + "'";

    db->ActionQuery(sql_query);
    system(guest_dir_rm_cmd.c_str());
}

void kill_guest(const std::string &vm_name)
{
    std::string stop_cmd = "pgrep -nf \"[q]emu.*/" + vm_name +
        "/" + vm_name + "_a.img\" | xargs kill";

    system(stop_cmd.c_str());
}

} // namespace QManager
