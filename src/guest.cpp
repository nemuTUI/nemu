#include <libintl.h>

#include "guest.h"

namespace QManager {

void start_guest(const std::string &vm_name,
                 const std::string &dbf,
                 const std::string &vmdir)
{
    guest_t<VectorString> guest;
    const struct config *cfg = get_cfg();
    bool iso_install = false;
    int unused __attribute__((unused));

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
    sql_query = "select bios from vms where name='" + vm_name + "'";
    guest.bios = db->SelectQuery(sql_query);

    if (!guest.path[0].empty())
    {
        if ((guest.path[0].compare(guest.path[0].size() - 4, 4, ".iso")) == 0)
            iso_install = true;
    }

    // Generate strings for system shell commands
    std::string create_lock = "( touch " + lock_file + "; ";
    std::string delete_lock = " > /dev/null 2>&1; rm " + lock_file + " )&";
    std::string qemu_bin = "qemu-system-" + guest.arch[0];

    MapStringVector ints = Read_ifaces_from_json(guest.ints[0]);
    MapString disk = Gen_map_from_str(guest.disk[0]);

    std::string hdx_arg, ints_arg;
    char hdx_char = 'a';
    if (!iso_install && (guest.install[0] == "1"))
        hdx_char = 'b';

    for (auto &hdx : disk)
    {
        std::string hdd(1, hdx_char++);
        hdx_arg += " -hd" + hdd + " " + guest_dir + hdx.first;
    }

    for (auto &ifs : ints)
    {
        ints_arg += " -net nic,macaddr=" + ifs.second[0] + ",model=" + ifs.second[1];
        ints_arg += " -net tap,ifname=" + ifs.first + ",script=no,downscript=no";
    }

    std::string cpu_arg, kvm_arg, hcpu_arg, install_arg,
        usb_arg, mem_arg, vnc_arg, bios_arg;
    install_arg = "";
    if (guest.install[0] == "1")
    {
        if (iso_install)
            install_arg = " -boot d -cdrom " + guest.path[0];
        else
            install_arg = " -hda " + guest.path[0];
    }

    std::stoi(guest.cpus[0]) > 1 ? cpu_arg = " -smp " + guest.cpus[0] : cpu_arg = "";
    guest.kvmf[0] == "1" ?  kvm_arg = " -enable-kvm" : kvm_arg = "";
    (guest.kvmf[0] == "1" && guest.hcpu[0] == "1") ?  hcpu_arg = " -cpu host" : hcpu_arg = "";
    guest.usbp[0] == "1" ?  usb_arg = " -usb -usbdevice host:" + guest.usbd[0] : usb_arg = "";
    mem_arg = " -m " + guest.memo[0];
    guest.bios[0].empty() ? bios_arg = "" : bios_arg = " -bios " + guest.bios[0];

    if (!cfg->vnc_listen_any)
        vnc_arg = " -vnc 127.0.0.1:" + guest.vncp[0];
    else
        vnc_arg = " -vnc :" + guest.vncp[0];

    // Generate and execute complete command
    std::string guest_cmd =
        create_lock + qemu_bin + usb_arg +
        install_arg + hdx_arg + cpu_arg +
        mem_arg + kvm_arg + hcpu_arg +
        ints_arg + vnc_arg + bios_arg + delete_lock;

    unused = system(guest_cmd.c_str());

    if (!cfg->log_path.empty())
    {
        std::ofstream debug;
        debug.open(cfg->log_path);
        debug << guest_cmd << std::endl;
        debug.close();
    }
}

void connect_guest(const std::string &vm_name, const std::string &dbf)
{
    guest_t<VectorString> guest;
    uint16_t port;
    int unused __attribute__((unused));

    std::unique_ptr<QemuDb> db(new QemuDb(dbf));
    std::string sql_query = "select vnc from vms where name='" + vm_name + "'";
    guest.vncp = db->SelectQuery(sql_query);

    port = 5900 + std::stoi(guest.vncp[0]);

    std::string connect_cmd = get_cfg()->vnc_bin + " :" +
        std::to_string(port) + " > /dev/null 2>&1 &";

    unused = system(connect_cmd.c_str());
}

void delete_guest(const std::string &vm_name,
                  const std::string &dbf,
                  const std::string &vmdir)
{
    std::string guest_dir = vmdir + "/" + vm_name;
    std::string guest_dir_rm_cmd = "rm -rf " + guest_dir;
    int unused __attribute__((unused));

    char path[PATH_MAX + 1] = {0};

    if (realpath(guest_dir.c_str(), path) == NULL)
        err_exit(_("Error get realpath."));

    /* Just in case */
    if (strcmp(path, "/") == 0)
        err_exit(_("Something goes wrong. Delete root partition prevented."));

    std::unique_ptr<QemuDb> db(new QemuDb(dbf));
    std::string sql_query = "delete from vms where name='" + vm_name + "'";

    db->ActionQuery(sql_query);
    unused = system(guest_dir_rm_cmd.c_str());
}

void kill_guest(const std::string &vm_name)
{
    int unused __attribute__((unused));
    std::string stop_cmd = "pgrep -nf \"[q]emu.*/" + vm_name +
        "/" + vm_name + "_a.img\" | xargs kill";

    unused = system(stop_cmd.c_str());
}

} // namespace QManager
