#include <nm_core.h>
#include <nm_form.h>
#include <nm_utils.h>
#include <nm_string.h>
#include <nm_window.h>
#include <nm_menu.h>
#include <nm_database.h>
#include <nm_cfg_file.h>
#include <nm_vm_control.h>
#include <nm_9p_share.h>

typedef struct {
    nm_str_t mode;
    nm_str_t path;
    nm_str_t name;
} nm_9p_data_t;

#define NM_INIT_9P_DATA (nm_9p_data_t) { NM_INIT_STR, NM_INIT_STR, NM_INIT_STR }

static const char NM_9P_SET_MODE_SQL[] =
    "UPDATE vms SET fs9p_enable='%s' WHERE name='%s'";
static const char NM_9P_SET_PATH_SQL[] =
    "UPDATE vms SET fs9p_path='%s' WHERE name='%s'";
static const char NM_9P_SET_NAME_SQL[] =
    "UPDATE vms SET fs9p_name='%s' WHERE name='%s'";

static const char NM_9P_FORM_MODE[] = "Enable sharing";
static const char NM_9P_FORM_PATH[] = "Path to directory";
static const char NM_9P_FORM_NAME[] = "Name of the share";

static void nm_9p_init_windows(nm_form_t *form);
static void nm_9p_fields_setup(const nm_vmctl_data_t *cur);
static size_t nm_9p_labels_setup();
static int nm_9p_get_data(nm_9p_data_t *data, const nm_vmctl_data_t *cur);
static void nm_9p_update_db(const nm_str_t *name, const nm_9p_data_t *data);

enum {
    NM_LBL_9PMODE = 0, NM_FLD_9PMODE,
    NM_LBL_9PPATH, NM_FLD_9PPATH,
    NM_LBL_9PNAME, NM_FLD_9PNAME,
    NM_FLD_COUNT
};

static nm_field_t *fields[NM_FLD_COUNT + 1];

static void nm_9p_init_windows(nm_form_t *form)
{
    if (form) {
        nm_form_window_init();
        nm_form_data_t *form_data = (nm_form_data_t *)form_userptr(form);
        if (form_data)
            form_data->parent_window = action_window;
    } else {
        werase(action_window);
        werase(help_window);
    }

    nm_init_side();
    nm_init_action(_(NM_MSG_9P_SHARE));
    nm_init_help_edit();

    nm_print_vm_menu(NULL);
}

void nm_9p_share(const nm_str_t *name)
{
    nm_form_data_t *form_data = NULL;
    nm_form_t *form = NULL;
    nm_vmctl_data_t vm = NM_VMCTL_INIT_DATA;
    nm_9p_data_t data = NM_INIT_9P_DATA;
    size_t msg_len;

    nm_9p_init_windows(NULL);

    nm_vmctl_get_data(name, &vm);

    msg_len = nm_9p_labels_setup();

    form_data = nm_form_data_new(
        action_window, nm_9p_init_windows, msg_len, NM_FLD_COUNT / 2, NM_TRUE);

    if (nm_form_data_update(form_data, 0, 0) != NM_OK)
        goto out;

    for (size_t n = 0; n < NM_FLD_COUNT; n++) {
        switch (n) {
            case NM_FLD_9PMODE:
                fields[n] = nm_field_enum_new(
                    n / 2, form_data, nm_form_yes_no, false, false);
                break;
            case NM_FLD_9PPATH:
                fields[n] = nm_field_regexp_new(n / 2, form_data, "^/.*");
                break;
            case NM_FLD_9PNAME:
                fields[n] = nm_field_regexp_new(n / 2, form_data, ".*");
                break;
            default:
                fields[n] = nm_field_label_new(n / 2, form_data);
                break;
        }
    }
    fields[NM_FLD_COUNT] = NULL;

    nm_9p_labels_setup();
    nm_9p_fields_setup(&vm);
    nm_fields_unset_status(fields);

    form = nm_form_new(form_data, fields);
    nm_form_post(form);

    if (nm_form_draw(&form) != NM_OK)
        goto out;

    if (nm_9p_get_data(&data, &vm) != NM_OK)
        goto out;

    nm_9p_update_db(name, &data);

out:
    NM_FORM_EXIT();
    nm_str_free(&data.mode);
    nm_str_free(&data.name);
    nm_str_free(&data.path);
    nm_form_free(form);
    nm_form_data_free(form_data);
    nm_fields_free(fields);
    nm_vmctl_free_data(&vm);
}

static void nm_9p_fields_setup(const nm_vmctl_data_t *cur)
{
    field_opts_off(fields[NM_FLD_9PPATH], O_STATIC);
    field_opts_off(fields[NM_FLD_9PNAME], O_STATIC);

    set_field_buffer(fields[NM_FLD_9PMODE], 0,
        (nm_str_cmp_st(nm_vect_str(&cur->main, NM_SQL_9FLG),
            NM_ENABLE) == NM_OK) ? "yes" : "no");
    set_field_buffer(fields[NM_FLD_9PPATH], 0, nm_vect_str_ctx(&cur->main, NM_SQL_9PTH));
    set_field_buffer(fields[NM_FLD_9PNAME], 0, nm_vect_str_ctx(&cur->main, NM_SQL_9ID));
}

static size_t nm_9p_labels_setup()
{
    nm_str_t buf = NM_INIT_STR;
    size_t max_label_len = 0;
    size_t msg_len = 0;

    for (size_t n = 0; n < NM_FLD_COUNT; n++) {
        switch (n) {
        case NM_LBL_9PMODE:
            nm_str_format(&buf, "%s", _(NM_9P_FORM_MODE));
            break;
        case NM_LBL_9PPATH:
            nm_str_format(&buf, "%s", _(NM_9P_FORM_PATH));
            break;
        case NM_LBL_9PNAME:
            nm_str_format(&buf, "%s", _(NM_9P_FORM_NAME));
            break;
        default:
            continue;
        }

        msg_len = mbstowcs(NULL, buf.data, buf.len);
        if (msg_len > max_label_len)
            max_label_len = msg_len;

        if (fields[n])
            set_field_buffer(fields[n], 0, buf.data);
    }

    nm_str_free(&buf);
    return max_label_len;
}

static int nm_9p_get_data(nm_9p_data_t *data, const nm_vmctl_data_t *cur)
{
    int rc = NM_OK;
    nm_vect_t err = NM_INIT_VECT;

    nm_get_field_buf(fields[NM_FLD_9PMODE], &data->mode);
    nm_get_field_buf(fields[NM_FLD_9PPATH], &data->path);
    nm_get_field_buf(fields[NM_FLD_9PNAME], &data->name);

    if (field_status(fields[NM_FLD_9PMODE])) {
        if (nm_str_cmp_st(&data->mode, "no") == NM_OK)
            goto out;
    }
    else if (nm_str_cmp_st(nm_vect_str(&cur->main, NM_SQL_9FLG), NM_ENABLE) != NM_OK)
            goto out;

    nm_form_check_data(_(NM_9P_FORM_PATH), data->path, err);
    nm_form_check_data(_(NM_9P_FORM_NAME), data->name, err);

    if ((rc = nm_print_empty_fields(&err)) == NM_ERR)
        nm_vect_free(&err, NULL);

out:
    return rc;
}

static void nm_9p_update_db(const nm_str_t *name, const nm_9p_data_t *data)
{
    nm_str_t query = NM_INIT_STR;
    nm_str_free(&query);

    if (field_status(fields[NM_FLD_9PMODE])) {
        nm_str_format(&query, NM_9P_SET_MODE_SQL,
            (nm_str_cmp_st(&data->mode, "yes") == NM_OK) ? "1" : "0", name->data);
        nm_db_edit(query.data);
        nm_str_trunc(&query, 0);
    }

    if (field_status(fields[NM_FLD_9PPATH])) {
        nm_str_format(&query, NM_9P_SET_PATH_SQL, data->path.data, name->data);
        nm_db_edit(query.data);
        nm_str_trunc(&query, 0);
    }

    if (field_status(fields[NM_FLD_9PNAME])) {
        nm_str_format(&query, NM_9P_SET_NAME_SQL, data->name.data, name->data);
        nm_db_edit(query.data);
    }

    nm_str_free(&query);
}

/* vim:set ts=4 sw=4: */
