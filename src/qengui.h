#ifndef QENGUI_H_
#define QENGUI_H_

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
#define VERSION "0.3.0-dev"
#define DEFAULT_NETDRV "virtio"

namespace QManager {

extern const char *YesNo[3];
extern const char *NetDrv[4];
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
};

template <typename T>
struct guest_t {
    T name, arch, cpus, memo,
    disk, vncp, imac, kvmf,
    hcpu, path, ints, usbp,
    usbd, ndrv, install, bios,
    mouse;
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

#endif /* QENGUI_H_ */
