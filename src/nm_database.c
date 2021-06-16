#include <nm_core.h>
#include <nm_utils.h>
#include <nm_string.h>
#include <nm_vector.h>
#include <nm_cfg_file.h>
#include <nm_database.h>

#include <pthread.h>

static const char db_script[] = NM_FULL_DATAROOTDIR "/nemu/scripts/upgrade_db.sh";
static pthread_key_t db_conn_key;
static pthread_once_t key_once = PTHREAD_ONCE_INIT;

static void nm_db_check_version(void);
static int nm_db_update(void);
static int nm_db_select_cb(void *v, int argc, char **argv,
                           char **unused NM_UNUSED);

static void nm_db_init_key()
{
    if (pthread_key_create(&db_conn_key, NULL) != 0) {
        nm_bug(_("%s: pthread_key_create error: %s"),
                __func__, strerror(errno));
    }
}

void nm_db_init(void)
{
    const nm_cfg_t *cfg = nm_cfg_get();
    int need_create_db = 0;
    struct stat file_info;
    char *db_errmsg;
    db_conn_t *db_conn;
    int rc;

    const char *query[] = {
        "PRAGMA user_version=" NM_DB_VERSION,
        "CREATE TABLE vms(id integer PRIMARY KEY AUTOINCREMENT, "
            "name char(31), mem integer, smp char, kvm integer, "
            "hcpu integer, vnc integer, arch char(32), iso char, "
            "install integer, usb integer, usbid char, bios char, kernel char, "
            "mouse_override integer, kernel_append char, tty_path char, "
            "socket_path char, initrd char, machine char, fs9p_enable integer, "
            "fs9p_path char, fs9p_name char, usb_type char, spice integer, "
            "debug_port integer, debug_freeze integer, cmdappend char, team char, display_type char)",
        "CREATE TABLE ifaces(id integer primary key autoincrement, "
            "vm_name char, if_name char, mac_addr char, ipv4_addr char, "
            "if_drv char, vhost integer, macvtap integer, parent_eth char, altname char, "
            "netuser integer, hostfwd char, smb char)",
        "CREATE TABLE drives(id integer primary key autoincrement, vm_name char, "
            "drive_name char, drive_drv char, capacity integer, boot integer, discard integer)",
        "CREATE TABLE vmsnapshots(id integer primary key autoincrement, "
            "vm_name char, snap_name char, load integer, timestamp char)",
        "CREATE TABLE veth(id integer primary key autoincrement, l_name char, r_name char)",
        "CREATE TABLE usb(id integer primary key autoincrement, "
            "vm_name char, dev_name char, vendor_id char, product_id char, serial char)"
    };

    if (stat(cfg->db_path.data, &file_info) == -1)
        need_create_db = 1;

    db_conn = nm_calloc(1, sizeof(*db_conn));
    db_conn->in_transaction = false;

    if ((rc = sqlite3_open(cfg->db_path.data, &db_conn->handler)) != SQLITE_OK)
        nm_bug(_("%s: database error: %s"), __func__, sqlite3_errstr(rc));
    else
        nm_debug("%s: db handler %p\n", __func__, (void *) db_conn->handler);

    if (pthread_once(&key_once, nm_db_init_key) != 0) {
        nm_bug(_("%s: pthread_once error: %s"), __func__, strerror(errno));
    }

    if (pthread_getspecific(db_conn_key) == NULL) {
        if (pthread_setspecific(db_conn_key, db_conn) != 0) {
            nm_bug(_("%s: pthread_setspecific error: %s"),
                    __func__, strerror(errno));
        }
    }

    if (!need_create_db) {
        nm_db_check_version();
        return;
    }

    for (size_t n = 0; n < nm_arr_len(query); n++) {
        nm_debug("%s: \"%s\"\n", __func__, query[n]);

        if (db_conn->in_transaction)
            nm_bug(_("%s: database in transaction"), __func__);

        if (sqlite3_exec(db_conn->handler, query[n],
                    NULL, NULL, &db_errmsg) != SQLITE_OK)
            nm_bug(_("%s: database error: %s"), __func__, db_errmsg);
    }
}

void nm_db_select(const char *query, nm_vect_t *v)
{
    char *db_errmsg;
    db_conn_t *db;

    if ((db = pthread_getspecific(db_conn_key)) == NULL) {
        nm_bug(_("%s: got NULL db_conn"), __func__);
    }

    if (db->in_transaction)
        nm_bug(_("%s: database in transaction"), __func__);

    nm_debug("%s: \"%s\"\n", __func__, query);

    if (sqlite3_exec(db->handler, query, nm_db_select_cb,
                    (void *) v, &db_errmsg) != SQLITE_OK) {
        nm_bug(_("%s: database error: %s"), __func__, db_errmsg);
    }
}

void nm_db_edit(const char *query)
{
    char *db_errmsg;
    db_conn_t *db;

    if ((db = pthread_getspecific(db_conn_key)) == NULL) {
        nm_bug(_("%s: got NULL db_conn"), __func__);
    }

    if (db->in_transaction)
        nm_bug(_("%s: database in transaction"), __func__);

    nm_debug("%s: \"%s\"\n", __func__, query);

    if (sqlite3_exec(db->handler, query, NULL, NULL, &db_errmsg) != SQLITE_OK)
        nm_bug(_("%s: database error: %s"), __func__, db_errmsg);
}

bool nm_db_in_transaction()
{
    db_conn_t *db;

    db = pthread_getspecific(db_conn_key);

    return (db) ? db->in_transaction : false;
}

void nm_db_begin_transaction()
{
    char *db_errmsg;
    db_conn_t *db;

    if ((db = pthread_getspecific(db_conn_key)) == NULL) {
        nm_bug(_("%s: got NULL db_conn"), __func__);
    }

    if (db->in_transaction)
        nm_bug(_("%s: database already in transaction"), __func__);

    nm_debug("%s: BEGIN TRANSACTION\n", __func__);

    if (sqlite3_exec(db->handler, "BEGIN TRANSACTION", NULL, NULL, &db_errmsg) != SQLITE_OK)
        nm_bug(_("%s: database error: %s"), __func__, db_errmsg);

    db->in_transaction = true;
}

void nm_db_atomic(const char *query)
{
    char *db_errmsg;
    db_conn_t *db;

    if ((db = pthread_getspecific(db_conn_key)) == NULL) {
        nm_bug(_("%s: got NULL db_conn"), __func__);
    }

    if (!db->in_transaction)
        nm_bug(_("%s: database not in transaction"), __func__);

    nm_debug("%s: \"%s\"\n", __func__, query);

    if (sqlite3_exec(db->handler, query, NULL, NULL, &db_errmsg) != SQLITE_OK)
        nm_bug(_("%s: database error: %s"), __func__, db_errmsg);
}

void nm_db_commit()
{
    char *db_errmsg;
    db_conn_t *db;

    if ((db = pthread_getspecific(db_conn_key)) == NULL) {
        nm_bug(_("%s: got NULL db_conn"), __func__);
    }

    if (!db->in_transaction)
        nm_bug(_("%s: database not in transaction"), __func__);

    nm_debug("%s: COMMIT\n", __func__);

    if (sqlite3_exec(db->handler, "COMMIT", NULL, NULL, &db_errmsg) != SQLITE_OK)
        nm_bug(_("%s: database error: %s"), __func__, db_errmsg);

    db->in_transaction = false;
}

void nm_db_rollback()
{
    char *db_errmsg;
    db_conn_t *db;

    if ((db = pthread_getspecific(db_conn_key)) == NULL) {
        nm_bug(_("%s: got NULL db_conn"), __func__);
    }

    if (!db->in_transaction)
        nm_bug(_("%s: database not in transaction"), __func__);

    nm_debug("%s: ROLLBACK\n", __func__);

    if (sqlite3_exec(db->handler, "ROLLBACK", NULL, NULL, &db_errmsg) != SQLITE_OK)
        nm_bug(_("%s: database error: %s"), __func__, db_errmsg);

    db->in_transaction = false;
}

void nm_db_close(void)
{
    db_conn_t *db;

    db = pthread_getspecific(db_conn_key);

    if (!db) {
        return;
    }

    sqlite3_close(db->handler);
    pthread_key_delete(db_conn_key);
    free(db);
}

static void nm_db_check_version(void)
{
    nm_str_t query = NM_INIT_STR;
    nm_vect_t res = NM_INIT_VECT;

    nm_str_alloc_text(&query, NM_GET_DB_VERSION_SQL);
    nm_db_select(query.data, &res);

    if (!res.n_memb) {
        fprintf(stderr, _("%s: cannot get database version"), __func__);
        exit(NM_ERR);
    }

    if (nm_str_cmp_st(nm_vect_at(&res, 0), NM_DB_VERSION) != NM_OK) {
        printf(_("Database version is not up do date."
                    " I will try to update it.\n"));
        if (nm_db_update() != NM_OK) {
            fprintf(stderr, _("Cannot update database\n"));
            exit(NM_ERR);
        }
    }

    nm_vect_free(&res, nm_str_vect_free_cb);
    nm_str_free(&query);
}

static int nm_db_update(void)
{
    const nm_cfg_t *cfg = nm_cfg_get();
    nm_str_t backup = NM_INIT_STR;
    nm_str_t answer = NM_INIT_STR;
    nm_vect_t argv = NM_INIT_VECT;
    int rc = NM_OK;
    db_conn_t *db;

    if ((db = pthread_getspecific(db_conn_key)) == NULL) {
        nm_bug(_("%s: got NULL db_conn"), __func__);
    }

    if (db->in_transaction)
        nm_bug(_("%s: database in transaction"), __func__);

    nm_str_format(&backup, "%s-backup", cfg->db_path.data);
    nm_copy_file(&cfg->db_path, &backup);

    nm_vect_insert_cstr(&argv, db_script);
    nm_vect_insert_cstr(&argv, cfg->db_path.data);
    nm_vect_insert_cstr(&argv, cfg->qemu_bin_path.data);

    nm_vect_end_zero(&argv);

    if (nm_spawn_process(&argv, &answer) != NM_OK) {
        rc = NM_ERR;
        unlink(cfg->db_path.data);
        nm_copy_file(&backup, &cfg->db_path);
    }

    if (answer.len)
        printf("%s\n", answer.data);

    unlink(backup.data);

    nm_vect_free(&argv, NULL);
    nm_str_free(&backup);
    nm_str_free(&answer);

    return rc;
}

static int nm_db_select_cb(void *v, int argc, char **argv,
                           char **unused NM_UNUSED)
{
    nm_vect_t *res = (nm_vect_t *) v;
    nm_str_t value = NM_INIT_STR;

    for (int n = 0; n < argc; n++) {
        nm_str_alloc_text(&value, argv[n] ? argv[n] : "");
        nm_vect_insert(res, &value, sizeof(nm_str_t), nm_str_vect_ins_cb);
    }

    nm_str_free(&value);

    return 0;
}

/* vim:set ts=4 sw=4: */
