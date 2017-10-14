#include <nm_core.h>
#include <nm_form.h>
#include <nm_utils.h>
#include <nm_string.h>
#include <nm_window.h>
#include <nm_database.h>
#include <nm_cfg_file.h>
#include <nm_qmp_control.h>

#define NM_SNAP_FIELDS_NUM 2

enum {
    NM_FLD_SNAPDISK = 0,
    NM_FLD_SNAPNAME,
};

enum {
    NM_SQL_SNAP_ID = 0,
    NM_SQL_SNAP_VM,
    NM_SQL_SNAP_NAME,
    NM_SQL_SNAP_BACK,
    NM_SQL_SNAP_NUM,
    NM_SQL_SNAP_TIME = 6
};

typedef struct {
    nm_str_t drive;
    nm_str_t snap_name;
} nm_snap_data_t;

#define NM_INIT_SNAP { NM_INIT_STR, NM_INIT_STR }

static void nm_snapshot_get_drives(const nm_str_t *name, nm_vect_t *v);
static int nm_snapshot_get_data(const nm_str_t *name, nm_snap_data_t *data);
static int nm_snapshot_to_fs(const nm_str_t *name, const nm_snap_data_t *data);
static void nm_snapshot_to_db(const nm_str_t *name, const nm_snap_data_t *data, int idx);
static void nm_snapshot_revert_data(const nm_str_t *name, const char *drive,
                                    const nm_str_t *snapshot, const nm_vect_t *snaps);

static nm_field_t *fields_c[NM_SNAP_FIELDS_NUM + 1];

void nm_snapshot_create(const nm_str_t *name)
{
    nm_form_t *form = NULL;
    nm_window_t *window = NULL;
    nm_spinner_data_t sp_data = NM_INIT_SPINNER;
    nm_str_t buf = NM_INIT_STR;
    nm_vect_t drives = NM_INIT_VECT;
    nm_snap_data_t data = NM_INIT_SNAP;
    size_t msg_len;
    pthread_t spin_th;
    int done = 0, snap_idx = 0;

    nm_print_title(_(NM_EDIT_TITLE));
    window = nm_init_window(9, 45, 3);

    init_pair(1, COLOR_BLACK, COLOR_WHITE);
    wbkgd(window, COLOR_PAIR(1));

    for (size_t n = 0; n < NM_SNAP_FIELDS_NUM; ++n)
    {
        fields_c[n] = new_field(1, 19, (n + 1) * 2, 1, 0, 0);
        set_field_back(fields_c[n], A_UNDERLINE);
    }

    fields_c[NM_SNAP_FIELDS_NUM] = NULL;
    field_opts_off(fields_c[NM_FLD_SNAPNAME], O_STATIC);

    nm_snapshot_get_drives(name, &drives);
    set_field_type(fields_c[NM_FLD_SNAPDISK], TYPE_ENUM, drives.data, false, false);
    set_field_buffer(fields_c[NM_FLD_SNAPDISK], 0, *drives.data);

    nm_str_alloc_text(&buf, _("Snapshot "));
    nm_str_add_str(&buf, name);
    mvwaddstr(window, 1, 2, buf.data);
    mvwaddstr(window, 4, 2, _("Drive"));
    mvwaddstr(window, 6, 2, _("Snapshot name"));
    nm_str_trunc(&buf, 0);

    form = nm_post_form(window, fields_c, 22);
    if (nm_draw_form(window, form) != NM_OK)
        goto out;

    if (nm_snapshot_get_data(name, &data) != NM_OK)
        goto out;

    msg_len = mbstowcs(NULL, _(NM_EDIT_TITLE), strlen(_(NM_EDIT_TITLE)));
    sp_data.stop = &done;
    sp_data.x = (getmaxx(stdscr) + msg_len + 2) / 2;

    if (pthread_create(&spin_th, NULL, nm_spinner, (void *) &sp_data) != 0)
        nm_bug(_("%s: cannot create thread"), __func__);

    if ((snap_idx = nm_snapshot_to_fs(name, &data)) != NM_ERR)
        nm_snapshot_to_db(name, &data, snap_idx);

    done = 1;
    if (pthread_join(spin_th, NULL) != 0)
        nm_bug(_("%s: cannot join thread"), __func__);

out:
    nm_vect_free(&drives, NULL);
    nm_form_free(form, fields_c);
    nm_str_free(&data.drive);
    nm_str_free(&data.snap_name);
    nm_str_free(&buf);
}

void nm_snapshot_revert(const nm_str_t *name)
{
    nm_vect_t drives = NM_INIT_VECT;
    nm_vect_t snaps = NM_INIT_VECT;
    nm_vect_t choices = NM_INIT_VECT;
    nm_vect_t err = NM_INIT_VECT;
    nm_form_t *snap_form = NULL;
    nm_field_t *fields_snap[2] = {NULL};
    const char *drive = NULL;
    nm_window_t *revert_window = NULL;
    nm_str_t query = NM_INIT_STR;
    nm_str_t buf = NM_INIT_STR;
    size_t snaps_count = 0, msg_len;
    int pos_y = 11, pos_x = 2, done = 0;
    nm_spinner_data_t sp_data = NM_INIT_SPINNER;
    pthread_t spin_th;

    nm_snapshot_get_drives(name, &drives);

    if (drives.n_memb > 1)
    {
        drive = nm_form_select_drive(&drives);
        if (drive == NULL)
            goto out;
    }
    else
    {
        drive = (const char *) drives.data[0];
    }

    nm_str_format(&query,
        "SELECT * FROM snapshots WHERE vm_name='%s' AND backing_drive='%s'"
        " ORDER BY snap_idx ASC",
        name->data, drive);
    nm_db_select(query.data, &snaps);

    if (snaps.n_memb == 0)
    {
        nm_print_warn(3, 6, _("There are no snapshots"));
        goto out;
    }

    nm_print_title(_(NM_EDIT_TITLE));

    snaps_count = snaps.n_memb / 7;
    for (size_t n = 0; n < snaps_count; n++)
    {
        size_t idx_shift = 7 * n;

        if (pos_x >= (getmaxx(stdscr) - 20))
        {
            pos_y++;
            pos_x = 2;
        }

        mvprintw(pos_y, pos_x, nm_vect_str_ctx(&snaps, NM_SQL_SNAP_NAME + idx_shift));
        pos_x += nm_vect_str_len(&snaps, NM_SQL_SNAP_NAME + idx_shift);
        mvprintw(pos_y, pos_x, "(");
        pos_x++;
        mvprintw(pos_y, pos_x, nm_vect_str_ctx(&snaps, NM_SQL_SNAP_TIME + idx_shift));
        pos_x += nm_vect_str_len(&snaps, NM_SQL_SNAP_TIME + idx_shift);
        mvprintw(pos_y, pos_x, ") -> ");
        pos_x += 5;

        nm_vect_insert(&choices,
            nm_vect_str_ctx(&snaps, NM_SQL_SNAP_NAME + idx_shift),
            nm_vect_str_len(&snaps, NM_SQL_SNAP_NAME + idx_shift) + 1,
            NULL);
    }

    mvprintw(pos_y, pos_x, "current");

    nm_vect_end_zero(&choices);

    revert_window = nm_init_window(7, 45, 3);
    init_pair(1, COLOR_BLACK, COLOR_WHITE);
    wbkgd(revert_window, COLOR_PAIR(1));

    fields_snap[0] = new_field(1, 30, 2, 1, 0, 0);
    set_field_back(fields_snap[0], A_UNDERLINE);
    fields_snap[1] = NULL;

    set_field_type(fields_snap[0], TYPE_ENUM, choices.data, false, false);
    set_field_buffer(fields_snap[0], 0, *choices.data);

    nm_str_add_text(&buf, _("Revert "));
    nm_str_add_str(&buf, name);
    mvwaddstr(revert_window, 1, 2, buf.data);
    mvwaddstr(revert_window, 4, 2, _("Snapshot"));
    nm_str_trunc(&buf, 0);

    snap_form = nm_post_form(revert_window, fields_snap, 11);
    if (nm_draw_form(revert_window, snap_form) != NM_OK)
        goto out;

    nm_get_field_buf(fields_snap[0], &buf);
    nm_form_check_data(_("Snapshot"), buf, err);

    if (nm_print_empty_fields(&err) == NM_ERR)
    {
        nm_vect_free(&err, NULL);
        goto out;
    }

    msg_len = mbstowcs(NULL, _(NM_EDIT_TITLE), strlen(_(NM_EDIT_TITLE)));
    sp_data.stop = &done;
    sp_data.x = (getmaxx(stdscr) + msg_len + 2) / 2;

    if (pthread_create(&spin_th, NULL, nm_spinner, (void *) &sp_data) != 0)
        nm_bug(_("%s: cannot create thread"), __func__);

    nm_snapshot_revert_data(name, drive, &buf, &choices);

    done = 1;
    if (pthread_join(spin_th, NULL) != 0)
        nm_bug(_("%s: cannot join thread"), __func__);

out:
    nm_form_free(snap_form, fields_snap);
    nm_vect_free(&drives, NULL);
    nm_vect_free(&choices, NULL);
    nm_vect_free(&snaps, nm_str_vect_free_cb);
    nm_str_free(&query);
    nm_str_free(&buf);
}

static void nm_snapshot_revert_data(const nm_str_t *name, const char *drive,
                                    const nm_str_t *snapshot, const nm_vect_t *snaps)
{
    int found = 0;
    size_t cur_snap = 0;
    nm_str_t query = NM_INIT_STR;
    nm_str_t vmdir = NM_INIT_STR;

    for (size_t n = 0; n < snaps->n_memb; n++)
    {
        if (nm_str_cmp_st(snapshot, snaps->data[n]) == NM_OK)
        {
            found = 1;
            cur_snap = n;
            break;
        }
    }

    if (!found)
    {
        nm_print_warn(3, 2, _("Invalid snapshot name"));
        goto out;
    }

    nm_str_format(&query,
        "UPDATE snapshots SET active='0' WHERE vm_name='%s' and backing_drive='%s'",
        name->data, drive);
    nm_db_edit(query.data);

    nm_str_trunc(&query, 0);

    if (cur_snap > 0)
    {
        nm_str_format(&query,
            "UPDATE snapshots SET active='1' WHERE vm_name='%s' and snap_idx='%zu'",
            name->data, cur_snap - 1);
        nm_db_edit(query.data);
        nm_str_trunc(&query, 0);

        nm_str_format(&query,
            "DELETE FROM snapshots WHERE vm_name='%s' "
            "AND backing_drive='%s' AND snap_idx > %zu",
            name->data, drive, cur_snap - 1);
        nm_db_edit(query.data);
        nm_str_trunc(&query, 0);
    }
    else
    {
        nm_str_format(&query,
            "DELETE FROM snapshots WHERE vm_name='%s' "
            "AND backing_drive='%s'",
            name->data, drive);
        nm_db_edit(query.data);
        nm_str_trunc(&query, 0);
    }

    nm_str_alloc_str(&vmdir, &nm_cfg_get()->vm_dir);
    nm_str_add_char(&vmdir, '/');
    nm_str_add_str(&vmdir, name);

    for (; cur_snap < snaps->n_memb; cur_snap++)
    {
        nm_str_t snap_path = NM_INIT_STR;

        nm_str_format(&snap_path, "%s/%s.snap%zu",
            vmdir.data, drive, cur_snap);

        unlink(snap_path.data);

        nm_str_free(&snap_path);
    }

out:
    nm_str_free(&query);
    nm_str_free(&vmdir);
    return;
}

static void nm_snapshot_get_drives(const nm_str_t *name, nm_vect_t *v)
{
    nm_str_t query = NM_INIT_STR;
    nm_vect_t drives = NM_INIT_VECT;

    nm_str_format(&query, "SELECT drive_name FROM drives WHERE vm_name='%s'",
        name->data);

    nm_db_select(query.data, &drives);

    if (drives.n_memb == 0)
        nm_bug("%s: cannot find any drives", __func__);

    for (size_t n = 0; n < drives.n_memb; n++)
    {
        nm_str_t *drive = (nm_str_t *) drives.data[n];
        nm_vect_insert(v, drive->data, drive->len + 1, NULL);
    }

    nm_vect_end_zero(v);

    nm_str_free(&query);
    nm_vect_free(&drives, nm_str_vect_free_cb);
}

static int nm_snapshot_get_data(const nm_str_t *name, nm_snap_data_t *data)
{
    int rc = NM_OK;
    nm_str_t query = NM_INIT_STR;
    nm_vect_t names = NM_INIT_VECT;
    nm_vect_t err = NM_INIT_VECT;

    nm_get_field_buf(fields_c[NM_FLD_SNAPDISK], &data->drive);
    nm_get_field_buf(fields_c[NM_FLD_SNAPNAME], &data->snap_name);

    nm_form_check_data(_("Drive"), data->drive, err);
    nm_form_check_data(_("Snapshot name"), data->snap_name, err);

    if ((rc = nm_print_empty_fields(&err)) == NM_ERR)
    {
        nm_vect_free(&err, NULL);
        goto out;
    }

    nm_str_format(&query,
        "SELECT * FROM snapshots WHERE vm_name='%s'"
        " AND backing_drive='%s' AND snap_name='%s'",
        name->data, data->drive.data, data->snap_name.data);
    nm_db_select(query.data, &names);

    if (names.n_memb != 0)
    {
        nm_print_warn(3, 6, _("This name is already taken"));
        rc = NM_ERR;
    }

out:
    nm_vect_free(&names, nm_str_vect_free_cb);
    nm_str_free(&query);
    return rc;
}

static int nm_snapshot_to_fs(const nm_str_t *name, const nm_snap_data_t *data)
{
    int rc = NM_OK;
    nm_str_t query = NM_INIT_STR;
    nm_str_t snap_path = NM_INIT_STR;
    nm_str_t interface = NM_INIT_STR;
    nm_vect_t drive = NM_INIT_VECT;
    nm_vect_t snaps = NM_INIT_VECT;
    char drive_idx = 0, snap_idx = 0;

    nm_str_format(&query,
        "SELECT drive_drv FROM drives WHERE vm_name='%s' AND drive_name='%s'",
        name->data, data->drive.data);

    nm_db_select(query.data, &drive);

    if (drive.n_memb == 0)
        nm_bug("%s: cannot drive info", __func__);

    /* FIXME this is bad but it works.
     * Take disk interface from qmp query
     * "query-block" and parse it with json-c */
    if (nm_str_cmp_st(drive.data[0], nm_form_drive_drv[0]) == NM_OK)
        nm_str_add_text(&interface, "ide0-hd");
    else if (nm_str_cmp_st(drive.data[0], nm_form_drive_drv[1]) == NM_OK)
        nm_str_add_text(&interface, "scsi0-hd");
    else if (nm_str_cmp_st(drive.data[0], nm_form_drive_drv[2]) == NM_OK)
        nm_str_add_text(&interface, "virtio");
    else
        nm_bug("%s: something goes wrong", __func__);

    drive_idx = data->drive.data[data->drive.len - 5];
    nm_str_format(&interface, "%d", drive_idx - 97);

    nm_str_trunc(&query, 0);
    nm_str_format(&query,
        "SELECT id FROM snapshots WHERE vm_name='%s' AND backing_drive='%s'",
        name->data, data->drive.data);

    nm_db_select(query.data, &snaps);

    snap_idx = snaps.n_memb;

    nm_str_alloc_str(&snap_path, &nm_cfg_get()->vm_dir);
    nm_str_add_char(&snap_path, '/');
    nm_str_add_str(&snap_path, name);
    nm_str_add_char(&snap_path, '/');
    nm_str_add_str(&snap_path, &data->drive);
    nm_str_format(&snap_path, ".snap%d", snap_idx);

    rc = snap_idx;

    if (nm_qmp_drive_snapshot(name, &interface, &snap_path) == NM_ERR)
        rc = NM_ERR;

    nm_str_free(&query);
    nm_str_free(&snap_path);
    nm_str_free(&interface);
    nm_vect_free(&drive, nm_str_vect_free_cb);
    nm_vect_free(&snaps, nm_str_vect_free_cb);

    return rc;
}

static void nm_snapshot_to_db(const nm_str_t *name, const nm_snap_data_t *data, int idx)
{
    nm_str_t query = NM_INIT_STR;

    nm_str_format(&query,
        "UPDATE snapshots SET active='0' WHERE vm_name='%s' and backing_drive='%s'",
        name->data, data->drive.data);
    nm_db_edit(query.data);

    nm_str_trunc(&query, 0);
    nm_str_add_text(&query, "INSERT INTO snapshots("
        "vm_name, snap_name, backing_drive, snap_idx, active) VALUES('");
    nm_str_format(&query, "%s', '%s', '%s', '%d', '1')",
        name->data, data->snap_name.data, data->drive.data, idx);

    nm_db_edit(query.data);

    nm_str_free(&query);
}

/* vim:set ts=4 sw=4 fdm=marker: */
