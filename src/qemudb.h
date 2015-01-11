#ifndef QEMUDB_H_
#define QEMUDB_H_

#include <fstream>

#include "window.h"

namespace QManager
{
  class QemuDb
  {
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
}// namespace QManager

#endif
