#ifndef NEMU_H_
#define NEMU_H_

#include <string>
#include <thread>
#include <cstdint>
#include <vector>
#include <map>
#include <atomic>
#include <unordered_set>
#include <memory>
#include <limits.h>
#include <form.h>
#include <ncurses.h>
#include <sqlite3.h>

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/ini_parser.hpp>

#ifndef USR_PREFIX
#define USR_PREFIX "/usr"
#endif

#define _(str) gettext(str)
#define STRING_LITERAL(x) # x
#define STRING(x) STRING_LITERAL(x)
#define VERSION "0.4.0-dev"
#define DEFAULT_NETDRV "virtio"
#define DEFAULT_DRVINT "virtio"

namespace QManager {

extern const char *YesNo[3];
extern const char *NetDrv[4];
extern const char *drive_ints[4];
extern std::atomic<bool> finish;

typedef std::vector<std::string> VectorString;
typedef std::map<std::string, std::string> MapString;
typedef std::map<std::string, std::vector<std::string>> MapStringVector;

struct config {
    std::string vmdir;
    std::string db;
    std::string vnc_bin;
    std::string qemu_system_path;
    std::string log_path;
    VectorString qemu_targets;
    uint32_t list_max;
    bool vnc_listen_any;
};

enum input_choices {
    MenuVmlist = 1, MenuAddVm = 2,
    MenuQuit = 3,   MenuKeyEnter = 10,
    MenuKeyR = 114, MenuKeyC = 99,
    MenuKeyF = 102, MenuKeyD = 100,
    MenuKeyE = 101, MenuKeyL = 108,
    MenuKeyY = 121, MenuKeyA = 97,
    MenuKeyI = 105, MenuKeyS = 115,
    MenuKeyT = 116, MenuKeyM = 109
};

enum select_idx {
    SQL_IDX_ID = 0,
    SQL_IDX_NAME,
    SQL_IDX_MEM,
    SQL_IDX_SMP,
    SQL_IDX_HDD,
    SQL_IDX_KVM,
    SQL_IDX_HCPU,
    SQL_IDX_VNC,
    SQL_IDX_MAC,
    SQL_IDX_ARCH,
    SQL_IDX_ISO,
    SQL_IDX_INST,
    SQL_IDX_USBF,
    SQL_IDX_USBD,
    SQL_IDX_BIOS,
    SQL_IDX_KERN,
    SQL_IDX_OVER,
    SQL_IDX_DINT,
    SQL_IDX_KAPP,
    SQL_IDX_TTY,
    SQL_IDX_SOCK
};

enum ifs_idx {
    IFS_MAC = 0,
    IFS_DRV,
    IFS_IPV4
};

enum start_flags {
    START_TEMP = (1 << 0),
    START_FAKE = (1 << 1)
};

struct start_data {
    uint32_t flags;
    std::string cmd;
    start_data() : flags(0), cmd({}) {}
};

struct guest_t {
    std::string name, arch, cpus, memo,
    disk, vncp, imac, kvmf,
    hcpu, path, ints, usbp,
    usbd, ndrv, install, bios,
    mouse, drvint, kappend, kpath, tty,
    socket, ipv4;
};

class QMException : public std::exception {
public:
    QMException(const std::string &m) : msg(m) {}
    ~QMException() throw() {}
    const char* what() const throw() { return msg.c_str(); }

private:
    std::string msg;
};

} // namespace QManager

#endif /* NEMU_H_ */
/* vim:set ts=4 sw=4 fdm=marker: */
