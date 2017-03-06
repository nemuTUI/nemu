#include <libintl.h>
#include <fcntl.h>
#include <signal.h>

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

    const std::string guest_dir = vmdir + "/" + vm_name + "/";
    std::string qemu_cmd = "( ";

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
    // TODO optimize queries
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
    sql_query = "select mouse_override from vms where name='" + vm_name + "'";
    guest.mouse = db->SelectQuery(sql_query);
    sql_query = "select drive_interface from vms where name='" + vm_name + "'";
    guest.drvint = db->SelectQuery(sql_query);

    if (!guest.path[0].empty() && guest.path[0].length() > 4)
    {
        if ((guest.path[0].compare(guest.path[0].size() - 4, 4, ".iso")) == 0)
            iso_install = true;
    }

    // Generate strings for system shell commands
    qemu_cmd += cfg->qemu_system_path + "-" + guest.arch[0];

    MapStringVector ints = Read_ifaces_from_json(guest.ints[0]);
    MapString disk = Gen_map_from_str(guest.disk[0]);

    if (guest.install[0] == "1")
    {
        if (iso_install)
            qemu_cmd += " -boot d -drive file=" + guest.path[0] + ",media=cdrom,if=ide";
        else
            qemu_cmd += " -drive file=" + guest.path[0] + ",media=disk,if=ide";
    }

    for (auto &hdx : disk)
    {
        qemu_cmd += " -drive file=" + guest_dir + hdx.first +
            ",media=disk,if=" + guest.drvint[0];
    }

    for (auto &ifs : ints)
    {
        qemu_cmd += " -net nic,macaddr=" + ifs.second[0] + ",model=" + ifs.second[1];
        qemu_cmd += " -net tap,ifname=" + ifs.first + ",script=no,downscript=no";
    }

    if (guest.mouse[0] == "1")
         qemu_cmd += " -usbdevice tablet";

    if (std::stoi(guest.cpus[0]) > 1)
        qemu_cmd += " -smp " + guest.cpus[0];
    if (guest.kvmf[0] == "1")
        qemu_cmd += " -enable-kvm";
    if (guest.kvmf[0] == "1" && guest.hcpu[0] == "1")
        qemu_cmd += " -cpu host";
    if (guest.usbp[0] == "1")
        qemu_cmd += " -usb -usbdevice host:" + guest.usbd[0];
    if (!guest.bios[0].empty())
        qemu_cmd += " -bios " + guest.bios[0];

    qemu_cmd += " -pidfile " + guest_dir + "qemu.pid";
    qemu_cmd += " -m " + guest.memo[0];

    if (!cfg->vnc_listen_any)
        qemu_cmd += " -vnc 127.0.0.1:" + guest.vncp[0];
    else
        qemu_cmd += " -vnc :" + guest.vncp[0];

    qemu_cmd += " > /dev/null 2>&1; rm -f " + guest_dir + "qemu.pid )&";

    unused = system(qemu_cmd.c_str());

    if (!cfg->log_path.empty())
    {
        std::ofstream debug;
        debug.open(cfg->log_path);
        debug << qemu_cmd << std::endl;
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
    pid_t pid;
    int fd;
    const std::string pid_path = get_cfg()->vmdir + "/" + vm_name + "/qemu.pid";
    char buf[10];

    if ((fd = open(pid_path.c_str(), O_RDONLY)) == -1)
        return;

    if (read(fd, buf, sizeof(buf)) <= 0)
    {
         close(fd);
         return;
    }

    pid = std::stoi(buf);
    kill(pid, SIGTERM);

    close(fd);
}

} // namespace QManager
