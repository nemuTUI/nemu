#include <libintl.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/stat.h>

#include "guest.h"

namespace QManager {

void start_guest(const std::string &vm_name,
                 const std::string &dbf,
                 const std::string &vmdir,
                 struct start_data *data)
{
    VectorString guest;
    const struct config *cfg = get_cfg();
    bool iso_install = false;
    int unused __attribute__((unused));

    const std::string guest_dir = vmdir + "/" + vm_name + "/";
    std::string qemu_cmd = "( ";

    std::unique_ptr<QemuDb> db(new QemuDb(dbf));

    // Check if system is already installed
    const std::string sql_query = "SELECT * FROM vms WHERE name='" + vm_name + "'";
    db->SelectQuery(sql_query, &guest);

    if (guest[SQL_IDX_INST] == "1")
    {
        uint32_t ch;
        std::unique_ptr<PopupWarning> Warn(new PopupWarning(_("Already installed (y/n)"), 3, 25, 7));
        
        Warn->Init();
        ch = Warn->Print(Warn->window);

        if (ch == MenuKeyY)
        {
            std::string sql_query = "UPDATE vms SET install='0' WHERE name='" + vm_name + "'";
            db->ActionQuery(sql_query);
        }

        /* Clear temporary flag while install */
        if (data)
            data->flags &= ~START_TEMP;
    }

    // Get guest parameters from database
    if (!guest[SQL_IDX_ISO].empty() && guest[SQL_IDX_ISO].length() > 4)
    {
        if ((guest[SQL_IDX_ISO].compare(guest[SQL_IDX_ISO].size() - 4, 4, ".iso")) == 0)
            iso_install = true;
    }

    // Generate strings for system shell commands
    qemu_cmd += cfg->qemu_system_path + "-" + guest[SQL_IDX_ARCH];

    MapStringVector ints = Read_ifaces_from_json(guest[SQL_IDX_MAC]);
    MapString disk = Gen_map_from_str(guest[SQL_IDX_HDD]);

    if (guest[SQL_IDX_INST] == "1")
    {
        if (iso_install)
            qemu_cmd += " -boot d -drive file=" + guest[SQL_IDX_ISO] + ",media=cdrom,if=ide";
        else
            qemu_cmd += " -drive file=" + guest[SQL_IDX_ISO] + ",media=disk,if=ide";
    }

    for (auto &hdx : disk)
    {
        qemu_cmd += " -drive file=" + guest_dir + hdx.first +
            ",media=disk,if=" + guest[SQL_IDX_DINT];
    }

    for (auto &ifs : ints)
    {
        qemu_cmd += " -net nic,macaddr=" + ifs.second[0] + ",model=" + ifs.second[1];
        qemu_cmd += " -net tap,ifname=" + ifs.first + ",script=no,downscript=no";
    }

    if (guest[SQL_IDX_OVER] == "1")
         qemu_cmd += " -usbdevice tablet";

    if (std::stoi(guest[SQL_IDX_SMP]) > 1)
        qemu_cmd += " -smp " + guest[SQL_IDX_SMP];
    if (guest[SQL_IDX_KVM] == "1")
        qemu_cmd += " -enable-kvm";
    if (guest[SQL_IDX_KVM] == "1" && guest[SQL_IDX_HCPU] == "1")
        qemu_cmd += " -cpu host";
    if (guest[SQL_IDX_USBF] == "1")
        qemu_cmd += " -usb -usbdevice host:" + guest[SQL_IDX_USBD];
    if (!guest[SQL_IDX_BIOS].empty())
        qemu_cmd += " -bios " + guest[SQL_IDX_BIOS];
    if (!guest[SQL_IDX_KERN].empty())
    {
        qemu_cmd += " -kernel " + guest[SQL_IDX_KERN];
        if (!guest[SQL_IDX_KAPP].empty())
            qemu_cmd += " -append \"" + guest[SQL_IDX_KAPP] + "\"";
    }
    if (!guest[SQL_IDX_SOCK].empty())
    {
        struct stat info;

        if (stat(guest[SQL_IDX_SOCK].c_str(), &info) != -1)
        {
            std::unique_ptr<PopupWarning> Warn(new PopupWarning(_("Socket is already used!"), 3, 30, 7));
            Warn->Init();
            Warn->Print(Warn->window);
            return;
        }

        qemu_cmd += " -chardev socket,path=" + guest[SQL_IDX_SOCK] +
            ",server,nowait,id=socket_" + vm_name +
            " -device isa-serial,chardev=socket_" + vm_name;
    }
    if (!guest[SQL_IDX_TTY].empty())
    {
        int fd;

        if ((fd = open(guest[SQL_IDX_TTY].c_str(), O_RDONLY)) == -1)
        {
            std::unique_ptr<PopupWarning> Warn(new PopupWarning(_("TTY is missing!"), 3, 30, 7));
            Warn->Init();
            Warn->Print(Warn->window);
            return;
        }
        if (!isatty(fd))
        {
            close(fd);
            std::unique_ptr<PopupWarning> Warn(new PopupWarning(_("TTY is not terminal!"), 3, 30, 7));
            Warn->Init();
            Warn->Print(Warn->window);
            return;
        }
        close(fd);
        qemu_cmd += " -chardev tty,path=" + guest[SQL_IDX_TTY] +
            ",id=tty_" + vm_name +
            " -device isa-serial,chardev=tty_" + vm_name;
    }

    if (data)
    {
        if (data->flags & START_TEMP)
            qemu_cmd += " -snapshot";
    }

    qemu_cmd += " -pidfile " + guest_dir + "qemu.pid";
    qemu_cmd += " -m " + guest[SQL_IDX_MEM];

    if (!cfg->vnc_listen_any)
        qemu_cmd += " -vnc 127.0.0.1:" + guest[SQL_IDX_VNC];
    else
        qemu_cmd += " -vnc :" + guest[SQL_IDX_VNC];

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
    VectorString vncp;
    uint16_t port;
    int unused __attribute__((unused));

    std::unique_ptr<QemuDb> db(new QemuDb(dbf));
    const std::string sql_query = "select vnc from vms where name='" + vm_name + "'";
    db->SelectQuery(sql_query, &vncp);

    port = 5900 + std::stoi(vncp[0]);

    std::string connect_cmd = get_cfg()->vnc_bin + " :" +
        std::to_string(port) + " > /dev/null 2>&1 &";

    unused = system(connect_cmd.c_str());
}

void delete_guest(const std::string &vm_name,
                  const std::string &dbf,
                  const std::string &vmdir)
{
    std::string guest_dir = vmdir + "/" + vm_name;
    VectorString guest;
    bool ok = true;

    char path[PATH_MAX + 1] = {0};

    if (realpath(guest_dir.c_str(), path) == NULL)
        err_exit(_("Error get realpath."));

    std::unique_ptr<QemuDb> db(new QemuDb(dbf));
    std::string sql_query = "SELECT hdd FROM vms WHERE name='" + vm_name + "'";
    db->SelectQuery(sql_query, &guest);

    MapString disk = Gen_map_from_str(guest[0]);
    std::string disk_path;

    for (auto &hd : disk)
    {
        disk_path = std::string(path) + "/" + hd.first;
        if (unlink(disk_path.c_str()) == -1)
            ok = false;
    }

    if (ok)
    {
        if (rmdir(path) == -1)
            ok = false;
    }

    sql_query = "DELETE FROM vms WHERE name='" + vm_name + "'";
    db->ActionQuery(sql_query);

    if (!ok)
    {
        std::unique_ptr<PopupWarning> Warn(new PopupWarning(_("Some files was not deleted!"), 3, 30, 7));
        Warn->Init();
        Warn->Print(Warn->window);
    }
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
