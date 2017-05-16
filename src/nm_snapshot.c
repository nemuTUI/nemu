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

void nm_snapshot_create(const nm_str_t *name)
{
    nm_form_t *form = NULL;
    nm_window_t *window = NULL;
    nm_field_t *fields[NM_SNAP_FIELDS_NUM + 1];
    nm_spinner_data_t sp_data = NM_INIT_SPINNER;
    size_t msg_len;
    pthread_t spin_th;
    int done = 0;
}

/* vim:set ts=4 sw=4 fdm=marker: */
