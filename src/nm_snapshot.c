#include <nm_core.h>
#include <nm_form.h>
#include <nm_utils.h>
#include <nm_string.h>
#include <nm_window.h>
#include <nm_database.h>

#define NM_SNAP_FIELDS_NUM 2

enum {
    NM_FLD_SNAPDISK = 0,
    NM_FLD_SNAPNAME,
};

typedef struct {
    nm_str_t drive;
    nm_str_t snap_name;
} nm_snap_data_t;

#define NM_INIT_SNAP { NM_INIT_STR, NM_INIT_STR }

static void nm_snapshot_get_drives(const nm_str_t *name, nm_vect_t *v);
static int nm_snapshot_get_data(nm_snap_data_t *data);

static nm_field_t *fields[NM_SNAP_FIELDS_NUM + 1];

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
    int done = 0;

    nm_print_title(_(NM_EDIT_TITLE));
    window = nm_init_window(9, 45, 3);

    init_pair(1, COLOR_BLACK, COLOR_WHITE);
    wbkgd(window, COLOR_PAIR(1));

    for (size_t n = 0; n < NM_SNAP_FIELDS_NUM; ++n)
    {
        fields[n] = new_field(1, 19, (n + 1) * 2, 1, 0, 0);
        set_field_back(fields[n], A_UNDERLINE);
    }

    fields[NM_SNAP_FIELDS_NUM] = NULL;
    field_opts_off(fields[NM_FLD_SNAPNAME], O_STATIC);

    nm_snapshot_get_drives(name, &drives);
    set_field_type(fields[NM_FLD_SNAPDISK], TYPE_ENUM, drives.data, false, false);
    set_field_buffer(fields[NM_FLD_SNAPDISK], 0, *drives.data);

    nm_str_alloc_text(&buf, _("Snapshot "));
    nm_str_add_str(&buf, name);
    mvwaddstr(window, 1, 2, buf.data);
    mvwaddstr(window, 4, 2, _("Drive"));
    mvwaddstr(window, 6, 2, _("Snapshot name"));

    form = nm_post_form(window, fields, 22);
    if (nm_draw_form(window, form) != NM_OK)
        goto out;

    if (nm_snapshot_get_data(&data) != NM_OK)
        goto out;

    msg_len = mbstowcs(NULL, _(NM_EDIT_TITLE), strlen(_(NM_EDIT_TITLE)));
    sp_data.stop = &done;
    sp_data.x = (getmaxx(stdscr) + msg_len + 2) / 2;

    if (pthread_create(&spin_th, NULL, nm_spinner, (void *) &sp_data) != 0)
        nm_bug(_("%s: cannot create thread"), __func__);

    /*XXX action here */

    done = 1;
    if (pthread_join(spin_th, NULL) != 0)
        nm_bug(_("%s: cannot join thread"), __func__);

out:
    nm_vect_free(&drives, NULL);
    nm_form_free(form, fields);
    nm_str_free(&data.drive);
    nm_str_free(&data.snap_name);
    nm_str_free(&buf);
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

static int nm_snapshot_get_data(nm_snap_data_t *data)
{
    int rc = NM_OK;
    nm_vect_t err = NM_INIT_VECT;

    nm_get_field_buf(fields[NM_FLD_SNAPDISK], &data->drive);
    nm_get_field_buf(fields[NM_FLD_SNAPNAME], &data->snap_name);

    nm_form_check_data(_("Drive"), data->drive, err);
    nm_form_check_data(_("Snapshot name"), data->snap_name, err);

    rc = nm_print_empty_fields(&err);

    nm_vect_free(&err, NULL);

    return rc;
}

/* vim:set ts=4 sw=4 fdm=marker: */
