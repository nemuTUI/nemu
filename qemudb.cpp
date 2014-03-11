#include <fstream>

#include "qemu_manage.h"

static int callback_c(void *NotUsed, int argc, char **argv, char **azColName) {
/*  int i;
  for(i=0; i<argc; i++) {
    printf("%s = %s\n", azColName[i], argv[i] ? argv[i] : "NULL");
  }
  printf("\n");
*/
  return 0;
}

QemuDb::QemuDb(const std::string &dbf) {
  dbf_ = dbf;
  zErrMsg = 0;
  sql.push_back("create table vms(id integer primary key autoincrement," \
    "name char(30), mem integer, smp integer, hdd char, kvm integer," \
    "vnc integer, mac char, arch char(32), iso char, install integer, "\
    "usb integer, usbid char)");
  sql.push_back("create table lastval(id integer, mac integer, vnc integer)");
  sql.push_back("insert into lastval(id, mac, vnc) values ('1', '244837814042624', '0')");
   
  //std::string sql[3] = {sql_c1, sql_c2, sql_c3};
  std::ifstream db(dbf_);

  try {
    if (! db) {
      sqlite3 *qdb;
      dbexec = sqlite3_open(dbf_.c_str(), &qdb);

      if(dbexec) {
        throw QMException(sqlite3_errmsg(qdb));
      }

      for(auto &query  : sql) {
        dbexec = sqlite3_exec(qdb, query.c_str(), callback_c, 0, &zErrMsg);

        if(dbexec != SQLITE_OK) {
          throw QMException(zErrMsg);
        }
      }

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
