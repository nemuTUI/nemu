#include <nm_core.h>
#include <nm_utils.h>
#include <nm_string.h>
#include <nm_vector.h>
#include <nm_cfg_file.h>

#include <sqlite3.h>

typedef sqlite3 nm_sqlite_t;

static nm_sqlite_t *db_handler = NULL;

static int nm_db_select_cb(void *v, int argc, char **argv,
                           __attribute__((unused)) char **unused);

void nm_db_init(void)
{
    struct stat file_info;
    const nm_cfg_t *cfg = nm_cfg_get();
    int need_create_db = 0;
    char *db_errmsg;
    int rc;

    const char *query[] = {
        "PRAGMA user_version=2",
        "CREATE TABLE vms(id integer PRIMARY KEY AUTOINCREMENT, "
            "name char(30), mem integer, smp integer, hdd char, kvm integer, "
            "hcpu integer, vnc integer, mac char, arch char(32), iso char, "
            "install integer, usb integer, usbid char, bios char, kernel char, "
            "mouse_override integer, drive_interface char, kernel_append char, "
            "tty_path char, socket_path char)",
        "CREATE TABLE lastval(id integer, mac integer, vnc integer)",
        "INSERT INTO lastval(id, mac, vnc) VALUES ('1', '244837814042624', '0')"
    };

    if (stat(cfg->db_path.data, &file_info) == -1)
        need_create_db = 1;

    if ((rc = sqlite3_open(cfg->db_path.data, &db_handler)) != SQLITE_OK)
        nm_bug(_("%s: database error: %s"), __func__, sqlite3_errstr(rc));

    if (!need_create_db)
        return;
    
    for (size_t n = 0; n < nm_arr_len(query); n++)
    {
#if (NM_DEBUG)
        printf("%s: \"%s\"\n", __func__, query[n]);
#endif
        if (sqlite3_exec(db_handler, query[n], NULL, NULL, &db_errmsg) != SQLITE_OK)
            nm_bug(_("%s: database error: %s"), __func__, db_errmsg);
    }
}

void nm_db_select(const char *query, nm_vect_t *v)
{
    char *db_errmsg;
#if (NM_DEBUG)
    printf("%s: \"%s\"\n", __func__, query);
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
#if (NM_DEBUG)
    printf("%s: \"%s\"\n", __func__, query);
#endif

    if (sqlite3_exec(db_handler, query, NULL, NULL, &db_errmsg) != SQLITE_OK)
        nm_bug(_("%s: database error: %s"), __func__, db_errmsg);
}

void nm_db_close(void)
{
    sqlite3_close(db_handler);
}

static int nm_db_select_cb(void *v, int argc, char **argv,
                           __attribute__((unused)) char **unused)
{
    nm_vect_t *res = (nm_vect_t *) v;
    nm_str_t value = NM_INIT_STR;

    for (int n = 0; n < argc; n++)
    {
        nm_str_alloc_text(&value, argv[n] ? argv[n] : "");
        nm_vect_insert(res, &value, sizeof(nm_str_t), nm_vect_ins_str_cb);
    }

    nm_str_free(&value);

    return 0;
}

/* vim:set ts=4 sw=4 fdm=marker: */
