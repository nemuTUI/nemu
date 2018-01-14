#include <nm_core.h>
#include <nm_utils.h>
#include <nm_string.h>
#include <nm_vector.h>
#include <nm_cfg_file.h>

#include <sqlite3.h>

typedef sqlite3 nm_sqlite_t;

static nm_sqlite_t *db_handler = NULL;

static int nm_db_select_cb(void *v, int argc, char **argv,
                           char **unused NM_UNUSED);

void nm_db_init(void)
{
    struct stat file_info;
    const nm_cfg_t *cfg = nm_cfg_get();
    int need_create_db = 0;
    char *db_errmsg;
    int rc;

    const char *query[] = {
        "PRAGMA user_version=7",
        "CREATE TABLE vms(id integer PRIMARY KEY AUTOINCREMENT, "
            "name char(31), mem integer, smp integer, kvm integer, "
            "hcpu integer, vnc integer, arch char(32), iso char, "
            "install integer, usb integer, usbid char, bios char, kernel char, "
            "mouse_override integer, kernel_append char, tty_path char, "
            "socket_path char, initrd char, machine char, "
            "fs9p_enable integer, fs9p_path char, fs9p_name char)",
        "CREATE TABLE lastval(id integer, mac integer, vnc integer)",
        "INSERT INTO lastval(id, mac, vnc) VALUES ('1', '244837814042624', '0')",
        "CREATE TABLE ifaces(id integer primary key autoincrement, "
            "vm_name char, if_name char, mac_addr char, ipv4_addr char, "
            "if_drv char, vhost integer, macvtap integer, parent_eth char)",
        "CREATE TABLE drives(id integer primary key autoincrement, "
            "vm_name char, drive_name char, drive_drv char, capacity integer, boot integer)",
        "CREATE TABLE snapshots(id integer primary key autoincrement, "
            "vm_name char, snap_name char, backing_drive char, snap_idx integer, active integer, "
            "TIMESTAMP DEFAULT CURRENT_TIMESTAMP NOT NULL)",
        "CREATE TABLE vmsnapshots(id integer primary key autoincrement, "
            "vm_name char, snap_name char, load integer, timestamp char)",
        "CREATE TABLE veth(id integer primary key autoincrement, l_name char, r_name char)",
        "CREATE TABLE usb(id integer primary key autoincrement, "
            "vm_name char, vendor_id char, product_id char, serial char)"
    };

    if (stat(cfg->db_path.data, &file_info) == -1)
        need_create_db = 1;

    if ((rc = sqlite3_open(cfg->db_path.data, &db_handler)) != SQLITE_OK)
        nm_bug(_("%s: database error: %s"), __func__, sqlite3_errstr(rc));

    if (!need_create_db)
        return;
    
    for (size_t n = 0; n < nm_arr_len(query); n++)
    {
#ifdef NM_DEBUG
        nm_debug("%s: \"%s\"\n", __func__, query[n]);
#endif
        if (sqlite3_exec(db_handler, query[n], NULL, NULL, &db_errmsg) != SQLITE_OK)
            nm_bug(_("%s: database error: %s"), __func__, db_errmsg);
    }
}

void nm_db_select(const char *query, nm_vect_t *v)
{
    char *db_errmsg;
#ifdef NM_DEBUG
    nm_debug("%s: \"%s\"\n", __func__, query);
#endif

    if (sqlite3_exec(db_handler, query, nm_db_select_cb,
                    (void *) v, &db_errmsg) != SQLITE_OK)
    {
        nm_bug(_("%s: database error: %s"), __func__, db_errmsg);
    }
}

void nm_db_edit(const char *query)
{
    char *db_errmsg;
#ifdef NM_DEBUG
    nm_debug("%s: \"%s\"\n", __func__, query);
#endif

    if (sqlite3_exec(db_handler, query, NULL, NULL, &db_errmsg) != SQLITE_OK)
        nm_bug(_("%s: database error: %s"), __func__, db_errmsg);
}

void nm_db_close(void)
{
    sqlite3_close(db_handler);
}

static int nm_db_select_cb(void *v, int argc, char **argv,
                           char **unused NM_UNUSED)
{
    nm_vect_t *res = (nm_vect_t *) v;
    nm_str_t value = NM_INIT_STR;

    for (int n = 0; n < argc; n++)
    {
        nm_str_alloc_text(&value, argv[n] ? argv[n] : "");
        nm_vect_insert(res, &value, sizeof(nm_str_t), nm_str_vect_ins_cb);
    }

    nm_str_free(&value);

    return 0;
}

/* vim:set ts=4 sw=4 fdm=marker: */
