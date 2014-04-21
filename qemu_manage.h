#ifndef QEMU_MANAGE_H_
#define QEMU_MANAGE_H_

#include <string>
#include <cstdint>
#include <vector>
#include <map>
#include <atomic>
#include <form.h>
#include <ncursesw/ncurses.h>
#include <sqlite3.h>

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/ini_parser.hpp>

#define _(str) gettext(str)
#define VERSION "0.1.3"
#define DEFAULT_NETDRV "virtio"

namespace QManager {
  extern std::atomic<bool> finish;

  typedef std::vector<std::string> VectorString;
  typedef std::map<std::string, std::string> MapString;
  typedef std::map<std::string, std::vector<std::string>> MapStringVector;

  enum input_choices {
    MenuVmlist = 1, MenuAddVm = 2,
#ifdef ENABLE_OPENVSWITCH
    MenuOvsMap = 3,
#endif
    MenuKeyEnter = 10,
    MenuKeyR = 114, MenuKeyC = 99,
    MenuKeyF = 102, MenuKeyD = 100,
    MenuKeyE = 101, MenuKeyL = 108,
    MenuKeyY = 121, MenuKeyA = 97,
    MenuKeyI = 105,
  };

  template <typename T>
  struct guest_t {
    T name, arch, cpus, memo, disk, vncp, imac,
    kvmf, path, ints, usbp, usbd, ndrv, install;
  };

  void err_exit(const char *msg, const std::string &err = "");

  template <typename T>
  T read_cfg(const std::string &cfg, const char *param) {
    T value;
    try {
      boost::property_tree::ptree ptr;
      boost::property_tree::ini_parser::read_ini(cfg, ptr);
      value = ptr.get<T>(param);
    }
    catch(boost::property_tree::ptree_bad_path &err) {
      err_exit("Error parsing config file! missing parameter.", (std::string) err.what());
    }
    catch(boost::property_tree::ptree_bad_data &err) {
      err_exit("Error parsing config file! bad value.", (std::string) err.what());
    }
    catch(boost::property_tree::ini_parser::ini_parser_error &err) {
      err_exit("Config file not found!.", (std::string) err.what());
    }

    return value;
  }

  MapString list_usb();
  VectorString list_arch();
  uint32_t total_memory();
  uint32_t disk_free(const std::string &vmdir);
  uint32_t cpu_count();

  std::string trim_field_buffer(char *buff);
  MapString gen_mac_addr(uint64_t &mac, uint32_t &int_num, std::string &vm_name);
  MapString Gen_map_from_str(const std::string &str);
  MapStringVector Read_ifaces_from_json(const std::string &str);
  bool check_root();
  bool verify_mac(const std::string &mac);
  void spinner(uint32_t, uint32_t);

  void start_guest(
    const std::string &vm_name, const std::string &dbf,
    const std::string &vmdir, const std::string &cfg
  );
  void delete_guest(
    const std::string &vm_name, const std::string &dbf, const std::string &vmdir
  );
  void connect_guest(const std::string &vm_name, const std::string &dbf);
  void kill_guest(const std::string &vm_name);

  class QMException : public std::exception {
    public:
      QMException(const std::string &m) : msg(m) {}
      ~QMException() throw() {}
      const char* what() const throw() { return msg.c_str(); }

    private:
      std::string msg;
  };

  class TemplateWindow {
    public:
      TemplateWindow(int height, int width, int starty = 7);
      void Init();
      WINDOW *window;
      ~TemplateWindow() { delwin(window); }

    protected:
      int row, col;
      int height_, width_;
      int startx_, starty_;
      std::string help_msg;
      int str_size;
  };

  class MainWindow : public TemplateWindow {
    public:
      MainWindow(int height, int width, int starty = 7)
        : TemplateWindow(height, width, starty) {}
      void Print();
  };

  class VmWindow : public TemplateWindow {
    public:
      VmWindow(int height, int width, int starty = 7)
        : TemplateWindow(height, width, starty) {}
      void Print();
  };

  class VmInfoWindow : public TemplateWindow {
    public:
      VmInfoWindow(
        const std::string &guest, const std::string &dbf,
        int height, int width, int starty = 7
      );
      void Print();

    private:
      std::string guest_, title_, dbf_, sql_query;
      guest_t<VectorString> guest;
  };

  class HelpWindow : public TemplateWindow {
    public:
      HelpWindow(int height, int width, int starty = 1)
        : TemplateWindow(height, width, starty) {}
      void Print();

    private:
      WINDOW *window_;
      std::vector<std::string> msg_;
      uint32_t line;
  };

  class AddVmWindow : public TemplateWindow {
    public:
      AddVmWindow(const std::string &dbf, const std::string &vmdir,
        int height, int width, int starty = 3);
      void Print();

    protected:
      void Delete_form();
      void Draw_form();
      void Draw_title();
      void Enable_color();
      void Post_form(uint32_t size);
      void ExeptionExit(QMException &err);
      void Gen_mac_address(
        struct guest_t<std::string> &guest, uint32_t int_count, std::string vm_name
      );
      void Gen_iface_json();

      std::string sql_query, s_last_mac,
      dbf_, vmdir_, guest_dir, create_guest_dir_cmd, create_img_cmd;
      uint32_t last_vnc, ui_vm_ints;
      uint64_t last_mac;
      VectorString v_last_vnc, v_last_mac, v_name;
      MapString ifaces;
      MapString::iterator itm;
      std::string::iterator its;
      FORM *form;
      std::vector<FIELD*> field;
      int ch, cmd_exit_status;
      MapString u_dev;
      char **UdevList, **ArchList;

    private:
      void Create_fields();
      void Config_fields_buffer();
      void Config_fields_type();
      void Print_fields_names();
      void Get_data_from_form();
      void Get_data_from_db();
      void Update_db_data();
      void Gen_hdd();
      void Check_input_data();

      guest_t<std::string> guest;
  };

  class EditVmWindow : public AddVmWindow {
    public:
      EditVmWindow(
        const std::string &dbf, const std::string &vmdir, const std::string &vm_name,
        int height, int width, int starty = 3
      );
      void Print();

    private:
      void Create_fields();
      void Config_fields_type();
      void Config_fields_buffer();
      void Print_fields_names();
      void Get_data_from_form();
      void Get_data_from_db();
      void Update_db_cpu_data();
      void Update_db_mem_data();
      void Update_db_kvm_data();
      void Update_db_usb_data();
      void Update_db_eth_data();
      void Gen_hdd();
      void Check_input_data();
      void Gen_iface_json(uint32_t);

      guest_t<VectorString> guest_old;
      guest_t<std::string> guest_new;
      std::string vm_name_;
      uint32_t ints_count;
  };

  class PopupWarning : public TemplateWindow {
    public:
      PopupWarning(const std::string &msg, int height,
        int width, int starty = 7);
        int Print(WINDOW *window);

    private:
      std::string msg_;
      WINDOW *window_;
      int ch_;
  };

  class CloneVmWindow : public AddVmWindow {
    public:
      CloneVmWindow(
        const std::string &dbf, const std::string &vmdir, const std::string &vm_name,
        int height, int width, int starty = 3
      );
      void Print();

    private:
      void Create_fields();
      void Config_fields();
      void Print_fields_names();
      void Get_data_from_form();
      void Get_data_from_db();
      void Gen_hdd();
      void Update_db_data();
      void Gen_iface_json();

      std::string vm_name_;
      guest_t<VectorString> guest_old;
      guest_t<std::string> guest_new;
      char hdd_ch;
  };

  class AddDiskWindow : public AddVmWindow {
    public:
      AddDiskWindow(
        const std::string &dbf, const std::string &vmdir, const std::string &vm_name,
        int height, int width, int starty = 3
      );
      void Print();

    private:
      void Create_fields();
      void Config_fields();
      void Print_fields_names();
      void Get_data_from_form();
      void Get_data_from_db();
      void Gen_hdd();
      void Update_db_data();

      std::string vm_name_;
      guest_t<VectorString> guest_old;
      guest_t<std::string> guest_new;
      char hdd_ch;
  };

  class EditNetWindow : public AddVmWindow {
    public:
      EditNetWindow(
        const std::string &dbf, const std::string &vmdir, const std::string &vm_name,
        int height, int width, int starty = 3
      );
      void Print();

    private:
      void Create_fields();
      void Config_fields();
      void Print_fields_names();
      void Get_data_from_form();
      void Get_data_from_db();
      void Gen_hdd();
      void Gen_iface_json();
      void Update_db_eth_drv_data();
      void Update_db_eth_mac_data();

      std::string vm_name_;
      guest_t<VectorString> guest_old;
      guest_t<std::string> guest_new;
      MapStringVector ifs;
      VectorString iflist, all_ints;
      char **IfaceList;
  };

  class MenuList {
    public:
      MenuList(WINDOW *menu_window, uint32_t &highlight);
      template <typename Iterator>
      void Print(Iterator begin, Iterator end) {
        x = 2; y = 2; i = 0;
        box(window_, 0, 0);

        for(Iterator list = begin; list != end; ++list) {
          if (highlight_ ==  i + 1) {
            wattron(window_, A_REVERSE);
            mvwprintw(window_, y, x, "%s", list->c_str());
            wattroff(window_, A_REVERSE);
          } else {
            mvwprintw(window_, y, x, "%s", list->c_str());
          }
          y++; i++;
        }

        wrefresh(window_);
      }

    protected:
      uint32_t highlight_;
      WINDOW *window_;
      uint32_t x, y, i;
  };

  class VmList : public MenuList {
    public:
      VmList(WINDOW *menu_window, uint32_t &highlight, const std::string &vmdir);
      template <typename Iterator>
      void Print(Iterator begin, Iterator end) {
        wattroff(window_, COLOR_PAIR(1));
        wattroff(window_, COLOR_PAIR(2));
        x = 2; y = 2; i = 0;
        box(window_, 0, 0);

        for(Iterator list = begin; list != end; ++list) {
          lock_ = vmdir_ + "/" + *list + "/" + *list + ".lock";
          std::ifstream lock_f(lock_);

          if (lock_f) {
            vm_status.insert(std::make_pair(*list, "running"));
            wattron(window_, COLOR_PAIR(2));
          }
          else {
            vm_status.insert(std::make_pair(*list, "stopped"));
            wattron(window_, COLOR_PAIR(1));
          }

          if (highlight_ ==  i + 1) {
            wattron(window_, A_REVERSE);
            mvwprintw(window_, y, x, "%-20s%s", list->c_str(), vm_status.at(*list).c_str());
            wattroff(window_, A_REVERSE);
          }
          else {
            mvwprintw(window_, y, x, "%-20s%s", list->c_str(), vm_status.at(*list).c_str());
          }
          y++; i++;
          wrefresh(window_);
        }
      }
      MapString vm_status;

    private:
      std::string vmdir_, lock_;
  };

  class QemuDb {
    public:
      QemuDb(const std::string &dbf);
      VectorString SelectQuery(const std::string &query);
      void ActionQuery(const std::string &query);
      ~QemuDb() { sqlite3_close(qdb); }

    private:
      sqlite3 *qdb;
      std::string dbf_, query_;
      VectorString sql;
      int dbexec;
      char *zErrMsg;
  };

} // namespace QManager

#endif
