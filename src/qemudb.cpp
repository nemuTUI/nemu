#include "qemudb.h"

namespace QManager {

static VectorString result;

static int query_cb(void *NotUsed, int argc, char **argv, char **azColName)
{
    (void) NotUsed;
    (void) azColName;
    int i;

    for (i = 0; i < argc; i++)
        result.push_back(argv[i] ? argv[i] : "NULL");

    return 0;
}

QemuDb::QemuDb(const std::string &dbf)
{
    dbf_ = dbf;
    zErrMsg = 0;
    std::ifstream db(dbf_);
    dbexec = sqlite3_open(dbf_.c_str(), &qdb);

    try
    {
        if (!db)
        {
            sql.push_back("create table vms(id integer primary key autoincrement, " \
                "name char(30), mem integer, smp integer, hdd char, kvm integer, " \
                "hcpu integer, vnc integer, mac char, arch char(32), iso char, "\
                "install integer, usb integer, usbid char)");
            sql.push_back("create table lastval(id integer, mac integer, vnc integer)");
            sql.push_back("insert into lastval(id, mac, vnc) values ('1', '244837814042624', '0')");

            if (dbexec)
                throw QMException(sqlite3_errmsg(qdb));
        }

        for (auto &query : sql)
        {
            dbexec = sqlite3_exec(qdb, query.c_str(), NULL, 0, &zErrMsg);

            if (dbexec != SQLITE_OK)
                throw QMException(zErrMsg);

            sql.clear();
        }
    }
    catch (QMException &err)
    {
        PopupWarning Warn(err.what(), 3, 30, 4);
        Warn.Init();
        Warn.Print(Warn.window);
        refresh();
        endwin();
        exit(10);
    }
}

VectorString QemuDb::SelectQuery(const std::string &query)
{
    query_ = query;

    result.clear();

    try
    {
        dbexec = sqlite3_exec(qdb, query_.c_str(), query_cb, 0, &zErrMsg);

        if (dbexec != SQLITE_OK)
            throw QMException(zErrMsg);
    }
    catch (QMException &err)
    {
        PopupWarning Warn(err.what(), 3, 30, 4);
        Warn.Init();
        Warn.Print(Warn.window);
        refresh();
        endwin();
        exit(10);
    }

    return result;
}

void QemuDb::ActionQuery(const std::string &query)
{
    query_ = query;

    try
    {
        dbexec = sqlite3_exec(qdb, query_.c_str(), NULL, 0, &zErrMsg);

        if (dbexec != SQLITE_OK)
            throw QMException(zErrMsg);
    }
    catch (QMException &err)
    {
        PopupWarning Warn(err.what(), 3, 30, 4);
        Warn.Init();
        Warn.Print(Warn.window);
        refresh();
        endwin();
        exit(10);
    }
}

} // namespace QManager
