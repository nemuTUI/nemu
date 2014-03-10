#include <fstream>

#include "qemu_manage.h"

QemuDb::QemuDb(const std::string &dbf) {
  dbf_ = dbf;
  zErrMsg = 0;
  sql_c1 = "create table vms(id integer primary key autoincrement," \
    "name char(30), mem integer, smp integer, hdd char, kvm integer," \
    "vnc integer, mac char, arch char(32), iso char, install integer, "\
    "usb integer, usbid char)";
  sql_c2 = "create table lastval(id integer, mac integer, vnc integer)";
  sql_c3 = "insert into lastval(id, mac, vnc) values ('1', '244837814042624', '0')"; 
  std::ifstream db(dbf_);

  try {
    if (! db) {
      sqlite3 *qdb;
      dbexec = sqlite3_open(dbf_.c_str(), &qdb);

      if(dbexec) {
        throw QMException(sqlite3_errmsg(qdb));
      }

      dbexec = sqlite3_exec(qdb, sql_c1)
      sqlite3_close(qdb); 
    }
  }
  catch (QMException &err) {
    PopupWarning Warn(err.what(), 3, 30, 4, 30);
    Warn.Init();
    Warn.Print(Warn.window);
    refresh();
    endwin();
    exit(10);
  }
}
