#include <nm_core.h>
#include <nm_form.h>
#include <nm_utils.h>
#include <nm_string.h>
#include <nm_window.h>
#include <nm_menu.h>
#include <nm_database.h>

typedef struct {
#if defined(NM_WITH_SPICE)
    nm_str_t spice;
#endif
    nm_str_t port;
    nm_str_t tty;
    nm_str_t sock;
    nm_str_t sync;
    nm_str_t display;
} nm_view_data_t;

#if defined(NM_WITH_SPICE)
#define NM_INIT_VIEW_DATA (nm_view_data_t) { NM_INIT_STR, NM_INIT_STR, \
    NM_INIT_STR,  NM_INIT_STR, NM_INIT_STR, NM_INIT_STR }
#else
#define NM_INIT_VIEW_DATA (nm_view_data_t) { NM_INIT_STR, NM_INIT_STR, \
    NM_INIT_STR, NM_INIT_STR }
#endif

#if defined(NM_WITH_SPICE)
static const char NM_LC_VIEWER_FORM_SPICE[] = "Spice server";
static const char NM_LC_VIEWER_FORM_PORT[]  = "VNC/Spice port";
#else
static const char NM_LC_VIEWER_FORM_PORT[]  = "VNC port";
#endif
static const char NM_LC_VIEWER_FORM_TTYP[]  = "Serial TTY";
static const char NM_LC_VIEWER_FORM_SOCK[]  = "Serial socket";
static const char NM_LC_VIEWER_FORM_SYNC[]  = "Sync mouse position";
static const char NM_LC_VIEWER_FORM_DSP[]   = "Display type";

static void nm_viewer_init_windows(nm_form_t *form);
static void nm_viewer_fields_setup(const nm_vmctl_data_t *vm);
static size_t nm_viewer_labels_setup();
static inline void nm_viewer_free(nm_view_data_t *data);
static int nm_viewer_get_data(nm_view_data_t *vm);
static void nm_viewer_update_db(const nm_str_t *name, nm_view_data_t *vm);

enum {
#if defined(NM_WITH_SPICE)
    NM_LBL_SPICE = 0, NM_FLD_SPICE,
    NM_LBL_PORT, NM_FLD_PORT,
#else
    NM_LBL_PORT = 0, NM_FLD_PORT,
#endif
    NM_LBL_TTYP, NM_FLD_TTYP,
    NM_LBL_SOCK, NM_FLD_SOCK,
    NM_LBL_SYNC, NM_FLD_SYNC,
    NM_LBL_DSP, NM_FLD_DSP,
    NM_FLD_COUNT,
};

static nm_field_t *fields[NM_FLD_COUNT + 1];

static void nm_viewer_init_windows(nm_form_t *form)
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
    nm_init_action(_(NM_MSG_EDIT_VIEW));
    nm_init_help_edit();

    nm_print_vm_menu(NULL);
}

void nm_viewer(const nm_str_t *name)
{
    nm_form_data_t *form_data = NULL;
    nm_form_t *form = NULL;
    nm_vmctl_data_t vm = NM_VMCTL_INIT_DATA;
    nm_view_data_t vm_new = NM_INIT_VIEW_DATA;
    size_t msg_len;

    nm_viewer_init_windows(NULL);

    nm_vmctl_get_data(name, &vm);

    msg_len = nm_viewer_labels_setup();

    form_data = nm_form_data_new(
        action_window, nm_viewer_init_windows, msg_len, NM_FLD_COUNT / 2, NM_TRUE);

    if (nm_form_data_update(form_data, 0, 0) != NM_OK)
        goto out;

    for (size_t n = 0; n < NM_FLD_COUNT; n++) {
        switch (n) {
#if defined(NM_WITH_SPICE)
            case NM_FLD_SPICE:
                fields[n] = nm_field_enum_new(
                    n / 2, form_data, nm_form_yes_no, false, false);
                break;
#endif
            case NM_FLD_PORT:
                fields[n] = nm_field_integer_new(n / 2, form_data,
                    1, NM_STARTING_VNC_PORT, 0xffff);
                break;
            case NM_FLD_TTYP:
                fields[n] = nm_field_regexp_new(
                    n / 2, form_data, "^/.*");
                break;
            case NM_FLD_SOCK:
                fields[n] = nm_field_regexp_new(
                    n / 2, form_data, "^/.*");
                break;
            case NM_FLD_SYNC:
                fields[n] = nm_field_enum_new(
                    n / 2, form_data, nm_form_yes_no, false, false);
                break;
            case NM_FLD_DSP:
                fields[n] = nm_field_enum_new(
                    n / 2, form_data, nm_form_displaytype, false, false);
                break;
            default:
                fields[n] = nm_field_label_new(n / 2, form_data);
                break;
        }
    }
    fields[NM_FLD_COUNT] = NULL;

    nm_viewer_labels_setup();
    nm_viewer_fields_setup(&vm);
    nm_fields_unset_status(fields);

    form = nm_form_new(form_data, fields);
    nm_form_post(form);

    if (nm_form_draw(&form) != NM_OK)
        goto out;

    if (nm_viewer_get_data(&vm_new) != NM_OK)
        goto out;

    nm_viewer_update_db(name, &vm_new);

out:
    NM_FORM_EXIT();
    nm_viewer_free(&vm_new);
    nm_vmctl_free_data(&vm);
    nm_form_free(form);
    nm_form_data_free(form_data);
    nm_fields_free(fields);
}

static void nm_viewer_fields_setup(const nm_vmctl_data_t *vm)
{
    uint32_t vnc_port;

#if defined(NM_WITH_SPICE)
    if (nm_str_cmp_st(nm_vect_str(&vm->main, NM_SQL_SPICE), NM_ENABLE) == NM_OK)
        set_field_buffer(fields[NM_FLD_SPICE], 0, nm_form_yes_no[0]);
    else
        set_field_buffer(fields[NM_FLD_SPICE], 0, nm_form_yes_no[1]);
#endif

    field_opts_off(fields[NM_FLD_TTYP], O_STATIC);
    field_opts_off(fields[NM_FLD_SOCK], O_STATIC);

    vnc_port = nm_str_stoui(nm_vect_str(&vm->main, NM_SQL_VNC), 10) + NM_STARTING_VNC_PORT;
    nm_str_format(nm_vect_str(&vm->main, NM_SQL_VNC), "%u", vnc_port);
    set_field_buffer(fields[NM_FLD_PORT], 0, nm_vect_str_ctx(&vm->main, NM_SQL_VNC));
    set_field_buffer(fields[NM_FLD_TTYP], 0, nm_vect_str_ctx(&vm->main, NM_SQL_TTY));
    set_field_buffer(fields[NM_FLD_SOCK], 0, nm_vect_str_ctx(&vm->main, NM_SQL_SOCK));
    set_field_buffer(fields[NM_FLD_DSP], 0, nm_vect_str_ctx(&vm->main, NM_SQL_DISPLAY));

    if (nm_str_cmp_st(nm_vect_str(&vm->main, NM_SQL_OVER), NM_ENABLE) == NM_OK)
        set_field_buffer(fields[NM_FLD_SYNC], 0, nm_form_yes_no[0]);
    else
        set_field_buffer(fields[NM_FLD_SYNC], 0, nm_form_yes_no[1]);
}

static size_t nm_viewer_labels_setup()
{
    nm_str_t buf = NM_INIT_STR;
    size_t max_label_len = 0;
    size_t msg_len = 0;

    for (size_t n = 0; n < NM_FLD_COUNT; n++) {
        switch (n) {
#if defined(NM_WITH_SPICE)
        case NM_LBL_SPICE:
            nm_str_format(&buf, "%s", _(NM_LC_VIEWER_FORM_SPICE));
            break;
#endif
        case NM_LBL_PORT:
            nm_str_format(&buf, "%s", _(NM_LC_VIEWER_FORM_PORT));
            break;
        case NM_LBL_TTYP:
            nm_str_format(&buf, "%s", _(NM_LC_VIEWER_FORM_TTYP));
            break;
        case NM_LBL_SOCK:
            nm_str_format(&buf, "%s", _(NM_LC_VIEWER_FORM_SOCK));
            break;
        case NM_LBL_SYNC:
            nm_str_format(&buf, "%s", _(NM_LC_VIEWER_FORM_SYNC));
            break;
        case NM_LBL_DSP:
            nm_str_format(&buf, "%s", _(NM_LC_VIEWER_FORM_DSP));
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

static inline void nm_viewer_free(nm_view_data_t *data)
{
#if defined(NM_WITH_SPICE)
    nm_str_free(&data->spice);
#endif
    nm_str_free(&data->port);
    nm_str_free(&data->tty);
    nm_str_free(&data->sock);
    nm_str_free(&data->sync);
    nm_str_free(&data->display);
}

static int nm_viewer_get_data(nm_view_data_t *vm)
{
    int rc;
    nm_vect_t err = NM_INIT_VECT;

#if defined(NM_WITH_SPICE)
    nm_get_field_buf(fields[NM_FLD_SPICE], &vm->spice);
#endif
    nm_get_field_buf(fields[NM_FLD_PORT], &vm->port);
    nm_get_field_buf(fields[NM_FLD_TTYP], &vm->tty);
    nm_get_field_buf(fields[NM_FLD_SOCK], &vm->sock);
    nm_get_field_buf(fields[NM_FLD_SYNC], &vm->sync);
    nm_get_field_buf(fields[NM_FLD_DSP],  &vm->display);

#if defined(NM_WITH_SPICE)
    if (field_status(fields[NM_FLD_SPICE]))
        nm_form_check_data(_(NM_LC_VIEWER_FORM_SPICE), vm->spice, err);
#endif
    if (field_status(fields[NM_FLD_PORT]))
        nm_form_check_data(_(NM_LC_VIEWER_FORM_PORT), vm->port, err);
    if (field_status(fields[NM_FLD_SOCK]))
        nm_form_check_data(_(NM_LC_VIEWER_FORM_SOCK), vm->sock, err);
    if (field_status(fields[NM_FLD_SYNC]))
        nm_form_check_data(_(NM_LC_VIEWER_FORM_SYNC), vm->sync, err);
    if (field_status(fields[NM_FLD_DSP]))
        nm_form_check_data(_(NM_LC_VIEWER_FORM_DSP), vm->display, err);

    rc = nm_print_empty_fields(&err);

    nm_vect_free(&err, NULL);

    return rc;
}

static void nm_viewer_update_db(const nm_str_t *name, nm_view_data_t *vm)
{
    nm_str_t query = NM_INIT_STR;

#if defined(NM_WITH_SPICE)
    if (field_status(fields[NM_FLD_SPICE])) {
        int spice_on = 0;

        if (nm_str_cmp_st(&vm->spice, "yes") == NM_OK)
            spice_on = 1;
        nm_str_format(&query, "UPDATE vms SET spice='%s' WHERE name='%s'",
            spice_on ? NM_ENABLE : NM_DISABLE, name->data);
        nm_db_edit(query.data);
    }
#endif /* NM_WITH_SPICE */

    if (field_status(fields[NM_FLD_PORT])) {
        uint32_t vnc_port = nm_str_stoui(&vm->port, 10) - NM_STARTING_VNC_PORT;
        nm_str_format(&query, "UPDATE vms SET vnc='%u' WHERE name='%s'",
                vnc_port, name->data);
        nm_db_edit(query.data);
    }

    if (field_status(fields[NM_FLD_TTYP])) {
        nm_str_format(&query, "UPDATE vms SET tty_path='%s' WHERE name='%s'",
            vm->tty.data, name->data);
        nm_db_edit(query.data);
    }

    if (field_status(fields[NM_FLD_SOCK])) {
        nm_str_format(&query, "UPDATE vms SET socket_path='%s' WHERE name='%s'",
            vm->sock.data, name->data);
        nm_db_edit(query.data);
    }

    if (field_status(fields[NM_FLD_SYNC])) {
        int sync_on = 0;

        if (nm_str_cmp_st(&vm->sync, "yes") == NM_OK)
            sync_on = 1;
        nm_str_format(&query, "UPDATE vms SET mouse_override='%s' WHERE name='%s'",
            sync_on ? NM_ENABLE : NM_DISABLE, name->data);
        nm_db_edit(query.data);
    }

    if (field_status(fields[NM_FLD_DSP])) {
        nm_str_format(&query, "UPDATE vms SET display_type='%s' WHERE name='%s'",
            vm->display.data, name->data);
        nm_db_edit(query.data);
    }

    nm_str_free(&query);
}
/* vim:set ts=4 sw=4: */
