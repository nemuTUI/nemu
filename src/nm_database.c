#include <nm_core.h>
#include <nm_utils.h>
#include <nm_string.h>
#include <nm_vector.h>
#include <nm_cfg_file.h>
#include <nm_database.h>

#include <pthread.h>

static const char db_script[] = NM_FULL_DATAROOTDIR
    "/nemu/scripts/upgrade_db.sh";
static pthread_key_t db_conn_key;
static pthread_once_t key_once = PTHREAD_ONCE_INIT;

static void nm_db_check_version(void);
static int nm_db_update(void);
static int nm_db_select_cb(void *v, int argc, char **argv,
                           char **unused NM_UNUSED);
static int nm_db_select_value_cb(void *s, int argc, char **argv,
                           char **unused NM_UNUSED);

static void nm_db_init_key(void)
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
        NM_SQL_PRAGMA_USER_VERSION_SET,
        NM_SQL_VMS_CREATE,
        NM_SQL_IFACES_CREATE,
        NM_SQL_DRIVES_CREATE,
        NM_SQL_SNAPS_CREATE,
        NM_SQL_VETH_CREATE,
        NM_SQL_USB_CREATE,
        NM_SQL_VETH_CREATE_TRIGGER
    };

    if (stat(cfg->db_path.data, &file_info) == -1) {
        need_create_db = 1;
    }

    db_conn = nm_calloc(1, sizeof(*db_conn));
    db_conn->in_transaction = false;

    if ((rc = sqlite3_open(cfg->db_path.data,
                    &db_conn->handler)) != SQLITE_OK) {
        nm_bug(_("%s: database error: %s"), __func__, sqlite3_errstr(rc));
    } else {
        nm_debug("%s: db handler %p\n", __func__, (void *) db_conn->handler);
    }

    if (pthread_once(&key_once, nm_db_init_key) != 0) {
        nm_bug(_("%s: pthread_once error: %s"), __func__, strerror(errno));
    }

    if (pthread_getspecific(db_conn_key) == NULL) {
        if (pthread_setspecific(db_conn_key, db_conn) != 0) {
            nm_bug(_("%s: pthread_setspecific error: %s"),
                    __func__, strerror(errno));
        }
    }

    /*
     * Foreign key constraints are disabled by default
     * (for backwards compatibility), so must be enabled separately
     * for each database connection.
     */
    if (sqlite3_exec(db_conn->handler, NM_SQL_PRAGMA_FOREIGN_ON,
                NULL, NULL, &db_errmsg) != SQLITE_OK) {
        nm_bug(_("%s: database error: %s"), __func__, db_errmsg);
    }

    if (!need_create_db) {
        nm_db_check_version();
        return;
    }

    for (size_t n = 0; n < nm_arr_len(query); n++) {
        nm_debug("%s: \"%s\"\n", __func__, query[n]);

        if (db_conn->in_transaction) {
            nm_bug(_("%s: database in transaction"), __func__);
        }

        if (sqlite3_exec(db_conn->handler, query[n],
                    NULL, NULL, &db_errmsg) != SQLITE_OK) {
            nm_bug(_("%s: database error: %s"), __func__, db_errmsg);
        }
    }
}

static void nm_db_select_common(const char *query, void *res,
        int (*cb)(void *, int, char **, char **))
{
    char *db_errmsg;
    db_conn_t *db;

    if ((db = pthread_getspecific(db_conn_key)) == NULL) {
        nm_bug(_("%s: got NULL db_conn"), __func__);
    }

    if (db->in_transaction) {
        nm_bug(_("%s: database in transaction"), __func__);
    }

    nm_debug("%s: \"%s\"\n", __func__, query);

    if (sqlite3_exec(db->handler, query, cb, res, &db_errmsg) != SQLITE_OK) {
        nm_bug(_("%s: database error: %s"), __func__, db_errmsg);
    }
}

void nm_db_select(const char *query, nm_vect_t *res)
{
    nm_db_select_common(query, res, nm_db_select_cb);
}

void nm_db_select_value(const char *query, nm_str_t *res)
{
    nm_db_select_common(query, res, nm_db_select_value_cb);
}

void nm_db_edit(const char *query)
{
    char *db_errmsg;
    db_conn_t *db;

    if ((db = pthread_getspecific(db_conn_key)) == NULL) {
        nm_bug(_("%s: got NULL db_conn"), __func__);
    }

    if (db->in_transaction) {
        nm_bug(_("%s: database in transaction"), __func__);
    }

    nm_debug("%s: \"%s\"\n", __func__, query);

    if (sqlite3_exec(db->handler, query, NULL, NULL, &db_errmsg) != SQLITE_OK) {
        nm_bug(_("%s: database error: %s"), __func__, db_errmsg);
    }
}

bool nm_db_in_transaction(void)
{
    db_conn_t *db;

    pthread_once(&key_once, nm_db_init_key);
    db = pthread_getspecific(db_conn_key);

    return (db) ? db->in_transaction : false;
}

void nm_db_begin_transaction(void)
{
    char *db_errmsg;
    db_conn_t *db;

    if ((db = pthread_getspecific(db_conn_key)) == NULL) {
        nm_bug(_("%s: got NULL db_conn"), __func__);
    }

    if (db->in_transaction) {
        nm_bug(_("%s: database already in transaction"), __func__);
    }

    nm_debug("%s: BEGIN TRANSACTION\n", __func__);

    if (sqlite3_exec(db->handler, "BEGIN TRANSACTION",
                NULL, NULL, &db_errmsg) != SQLITE_OK) {
        nm_bug(_("%s: database error: %s"), __func__, db_errmsg);
    }

    db->in_transaction = true;
}

void nm_db_atomic(const char *query)
{
    char *db_errmsg;
    db_conn_t *db;

    if ((db = pthread_getspecific(db_conn_key)) == NULL) {
        nm_bug(_("%s: got NULL db_conn"), __func__);
    }

    if (!db->in_transaction) {
        nm_bug(_("%s: database not in transaction"), __func__);
    }

    nm_debug("%s: \"%s\"\n", __func__, query);

    if (sqlite3_exec(db->handler, query,
                NULL, NULL, &db_errmsg) != SQLITE_OK) {
        nm_bug(_("%s: database error: %s"), __func__, db_errmsg);
    }
}

void nm_db_commit(void)
{
    char *db_errmsg;
    db_conn_t *db;

    if ((db = pthread_getspecific(db_conn_key)) == NULL) {
        nm_bug(_("%s: got NULL db_conn"), __func__);
    }

    if (!db->in_transaction) {
        nm_bug(_("%s: database not in transaction"), __func__);
    }

    nm_debug("%s: COMMIT\n", __func__);

    if (sqlite3_exec(db->handler, "COMMIT",
                NULL, NULL, &db_errmsg) != SQLITE_OK)
        nm_bug(_("%s: database error: %s"), __func__, db_errmsg);

    db->in_transaction = false;
}

void nm_db_rollback(void)
{
    char *db_errmsg;
    db_conn_t *db;

    if ((db = pthread_getspecific(db_conn_key)) == NULL) {
        nm_bug(_("%s: got NULL db_conn"), __func__);
    }

    if (!db->in_transaction) {
        nm_bug(_("%s: database not in transaction"), __func__);
    }

    nm_debug("%s: ROLLBACK\n", __func__);

    if (sqlite3_exec(db->handler, "ROLLBACK",
                NULL, NULL, &db_errmsg) != SQLITE_OK) {
        nm_bug(_("%s: database error: %s"), __func__, db_errmsg);
    }

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

    nm_str_alloc_text(&query, NM_SQL_PRAGMA_USER_VERSION_GET);
    nm_db_select(query.data, &res);

    if (!res.n_memb) {
        fprintf(stderr, _("%s: cannot get database version"), __func__);
        nm_exit(NM_ERR);
    }

    if (nm_str_cmp_st(nm_vect_at(&res, 0), NM_DB_VERSION) != NM_OK) {
        printf(_("Database version is not up do date."
                    " I will try to update it.\n"));
        if (nm_db_update() != NM_OK) {
            fprintf(stderr, _("Cannot update database\n"));
            nm_exit(NM_ERR);
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

    if (db->in_transaction) {
        nm_bug(_("%s: database in transaction"), __func__);
    }

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

    if (answer.len) {
        printf("%s\n", answer.data);
    }

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

static int nm_db_select_value_cb(void *s, int argc, char **argv,
                           char **unused NM_UNUSED)
{
    nm_str_t *res = s;

    if (argc != 1) {
        return 1;
    }

    nm_str_format(res, "%s", argv[0] ? argv[0] : "");

    return 0;
}

/* vim:set ts=4 sw=4: */
