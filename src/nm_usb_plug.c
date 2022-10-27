#include <nm_core.h>
#include <nm_form.h>
#include <nm_utils.h>
#include <nm_string.h>
#include <nm_window.h>
#include <nm_menu.h>
#include <nm_vector.h>
#include <nm_database.h>
#include <nm_usb_devices.h>
#include <nm_qmp_control.h>
#include <nm_usb_plug.h>

static const char NM_LC_USB_FORM_MSG[] = "Device";

static void nm_usb_attach_init_windows(nm_form_t *form);
static void nm_usb_detach_init_windows(nm_form_t *form);
static void nm_usb_plug_list(nm_vect_t *devs, nm_vect_t *names);
static int nm_usb_plug_get_data(const nm_str_t *name, nm_usb_data_t *usb,
        const nm_vect_t *usb_list);
static int nm_usb_unplug_get_data(nm_usb_data_t *usb, const nm_vect_t *db_list);
static void nm_usb_plug_update_db(const nm_str_t *name,
        const nm_usb_data_t *usb);

enum {
    NM_LBL_USB_DEV = 0, NM_FLD_USB_DEV,
    NM_FLD_COUNT
};

static nm_field_t *fields[NM_FLD_COUNT + 1];

static void nm_usb_attach_init_windows(nm_form_t *form)
{
    if (form) {
        nm_form_window_init();
        nm_form_data_t *form_data = (nm_form_data_t *)form_userptr(form);

        if (form_data) {
            form_data->parent_window = action_window;
        }
    } else {
        werase(action_window);
        werase(help_window);
    }

    nm_init_side();
    nm_init_action(_(NM_MSG_USB_ATTACH));
    nm_init_help_edit();

    nm_print_vm_menu(NULL);
}

static void nm_usb_detach_init_windows(nm_form_t *form)
{
    if (form) {
        nm_form_window_init();
        nm_form_data_t *form_data = (nm_form_data_t *)form_userptr(form);

        if (form_data) {
            form_data->parent_window = action_window;
        }
    } else {
        werase(action_window);
        werase(help_window);
    }

    nm_init_side();
    nm_init_action(_(NM_MSG_USB_DETACH));
    nm_init_help_edit();

    nm_print_vm_menu(NULL);
}

void nm_usb_plug(const nm_str_t *name, int vm_status)
{
    nm_form_data_t *form_data = NULL;
    nm_form_t *form = NULL;
    nm_str_t buf = NM_INIT_STR;
    nm_vect_t usb_devs = NM_INIT_VECT;
    nm_vect_t usb_names = NM_INIT_VECT;
    nm_vect_t db_result = NM_INIT_VECT;
    nm_usb_data_t usb = NM_INIT_USB_DATA;
    size_t msg_len = mbstowcs(NULL,
            _(NM_LC_USB_FORM_MSG), strlen(_(NM_LC_USB_FORM_MSG)));

    /* check for usb enabled first */
    nm_str_format(&buf, NM_USB_CHECK_SQL, name->data);
    nm_db_select(buf.data, &db_result);

    if (vm_status && nm_str_cmp_st(nm_vect_str(&db_result, 0),
                NM_DISABLE) == NM_OK) {
        nm_warn(_(NM_MSG_USB_DIS));
        goto out;
    }

    nm_usb_plug_list(&usb_devs, &usb_names);

    if (usb_names.n_memb == 0) {
        nm_warn(_(NM_MSG_USB_MISS));
        goto out;
    }

    nm_usb_attach_init_windows(NULL);

    form_data = nm_form_data_new(action_window,
            nm_usb_attach_init_windows, msg_len, NM_FLD_COUNT / 2, NM_TRUE);

    if (nm_form_data_update(form_data, 0, 0) != NM_OK) {
        goto out;
    }

    fields[0] = nm_field_label_new(0, form_data);
    fields[1] = nm_field_enum_new(0, form_data,
        (const char **)usb_names.data, false, false);
    fields[2] = NULL;

    set_field_buffer(fields[0], 0, _(NM_LC_USB_FORM_MSG));
    field_opts_off(fields[1], O_STATIC);
    set_field_buffer(fields[1], 0, *usb_names.data);
    nm_fields_unset_status(fields);

    form = nm_form_new(form_data, fields);
    nm_form_post(form);

    if (nm_form_draw(&form) != NM_OK) {
        goto out;
    }

    if ((nm_usb_plug_get_data(name, &usb, &usb_devs)) != NM_OK) {
        goto out;
    }

    if (vm_status) {
        if (nm_qmp_usb_attach(name, &usb) != NM_OK) {
            goto out;
        }
    }

    nm_usb_plug_update_db(name, &usb);

out:
    NM_FORM_EXIT();
    nm_form_free(form);
    nm_form_data_free(form_data);
    nm_fields_free(fields);
    nm_vect_free(&usb_names, NULL);
    nm_vect_free(&usb_devs, nm_usb_vect_free_cb);
    nm_vect_free(&db_result, nm_str_vect_free_cb);
    nm_str_free(&buf);
    nm_str_free(&usb.serial);
}

void nm_usb_unplug(const nm_str_t *name, int vm_status)
{
    nm_form_data_t *form_data = NULL;
    nm_form_t *form = NULL;
    nm_str_t buf = NM_INIT_STR;
    nm_usb_data_t usb_data = NM_INIT_USB_DATA;
    nm_usb_dev_t usb_dev = NM_INIT_USB;
    nm_vect_t usb_names = NM_INIT_VECT;
    nm_vect_t db_result = NM_INIT_VECT;
    size_t msg_len = mbstowcs(NULL,
            _(NM_LC_USB_FORM_MSG), strlen(_(NM_LC_USB_FORM_MSG)));

    usb_data.dev = &usb_dev;

    nm_str_format(&buf, NM_USB_GET_SQL, name->data);
    nm_db_select(buf.data, &db_result);

    if (!db_result.n_memb) {
        nm_warn(_(NM_MSG_USB_NONE));
        goto out;
    }

    nm_usb_unplug_list(&db_result, &usb_names, true);
    nm_usb_detach_init_windows(NULL);

    form_data = nm_form_data_new(action_window,
            nm_usb_detach_init_windows, msg_len, NM_FLD_COUNT / 2, NM_TRUE);

    if (nm_form_data_update(form_data, 0, 0) != NM_OK) {
        goto out;
    }

    fields[0] = nm_field_label_new(0, form_data);
    fields[1] = nm_field_enum_new(0, form_data,
        (const char **)usb_names.data, false, false);
    fields[2] = NULL;

    set_field_buffer(fields[0], 0, _(NM_LC_USB_FORM_MSG));
    field_opts_off(fields[1], O_STATIC);
    set_field_buffer(fields[1], 0, *usb_names.data);
    nm_fields_unset_status(fields);

    form = nm_form_new(form_data, fields);
    nm_form_post(form);

    if (nm_form_draw(&form) != NM_OK) {
        goto out;
    }

    if (nm_usb_unplug_get_data(&usb_data, &db_result) != NM_OK) {
        goto out;
    }

    if (vm_status) {
        (void) nm_qmp_usb_detach(name, &usb_data);
    }

    nm_str_format(&buf, NM_USB_DELETE_SQL,
            name->data,
            usb_dev.name.data,
            usb_dev.vendor_id.data,
            usb_dev.product_id.data,
            usb_data.serial.data);
    nm_db_edit(buf.data);

out:
    NM_FORM_EXIT();
    nm_form_free(form);
    nm_form_data_free(form_data);
    nm_fields_free(fields);
    nm_vect_free(&usb_names, NULL);
    nm_vect_free(&db_result, nm_str_vect_free_cb);
    nm_usb_data_free(&usb_data);
    nm_str_free(&buf);
}

int nm_usb_check_plugged(const nm_str_t *name)
{
    int rc = NM_ERR;
    nm_vect_t db_result = NM_INIT_VECT;
    nm_str_t query = NM_INIT_STR;

    nm_str_format(&query, NM_USB_GET_SQL, name->data);
    nm_db_select(query.data, &db_result);

    if (!db_result.n_memb) {
        rc = NM_OK;
    }

    nm_vect_free(&db_result, nm_str_vect_free_cb);
    nm_str_free(&query);

    return rc;
}

void nm_usb_unplug_list(const nm_vect_t *db_list, nm_vect_t *names, bool num)
{
    size_t dev_count = db_list->n_memb / NM_USB_IDX_COUNT;
    nm_str_t buf = NM_INIT_STR;

    for (size_t n = 0; n < dev_count; n++) {
        size_t idx_shift = NM_USB_IDX_COUNT * n;

        if (num) {
            nm_str_format(&buf, "%zu:%s [serial:%s]", n + 1,
                    nm_vect_str_ctx(db_list, NM_SQL_USB_NAME + idx_shift),
                    nm_vect_str_ctx(db_list, NM_SQL_USB_SERIAL + idx_shift));
        } else {
            nm_str_format(&buf, "%s [serial:%s]",
                    nm_vect_str_ctx(db_list, NM_SQL_USB_NAME + idx_shift),
                    nm_vect_str_ctx(db_list, NM_SQL_USB_SERIAL + idx_shift));
        }
        nm_vect_insert(names, buf.data, buf.len + 1, NULL);
    }

    nm_str_free(&buf);
}

static void nm_usb_plug_list(nm_vect_t *devs, nm_vect_t *names)
{
    nm_usb_get_devs(devs);
    nm_str_t dev_name = NM_INIT_STR;

    for (size_t n = 0; n < devs->n_memb; n++) {
        nm_str_format(&dev_name, "%zu:%s", n + 1,
                nm_usb_name(devs->data[n])->data);
        nm_vect_insert(names, dev_name.data, dev_name.len + 1, NULL);

        nm_debug("usb >> %03u:%03u %s:%s %s\n",
                *nm_usb_bus_num(devs->data[n]),
                *nm_usb_dev_addr(devs->data[n]),
                nm_usb_vendor_id(devs->data[n])->data,
                nm_usb_product_id(devs->data[n])->data,
                nm_usb_name(devs->data[n])->data);
    }

    nm_vect_end_zero(names);
    nm_str_free(&dev_name);
}

static int nm_usb_plug_get_data(const nm_str_t *name, nm_usb_data_t *usb,
        const nm_vect_t *usb_list)
{
    int rc = NM_ERR;
    nm_str_t buf = NM_INIT_STR;
    nm_str_t input = NM_INIT_STR;
    nm_vect_t db_list = NM_INIT_VECT;
    uint32_t idx;
    char *fo;

    nm_get_field_buf(fields[1], &input);
    if (!input.len) {
        nm_warn(_(NM_MSG_USB_EMPTY));
        goto out;
    }

    nm_str_copy(&buf, &input);
    if ((fo = strchr(buf.data, ':')) == NULL) {
        /* Reinsurance, "fo" will always be OK */
        nm_warn(_(NM_MSG_USB_EDATA));
        goto out;
    }

    nm_str_trunc(&input, 0);
    nm_str_add_text(&input, fo + 1);

    *fo = '\0';
    idx = nm_str_stoui(&buf, 10);
    usb->dev = usb_list->data[idx - 1];

    nm_usb_get_serial(usb->dev, &usb->serial);

    nm_str_format(&buf, NM_USB_EXISTS_SQL,
            name->data,
            usb->dev->name.data,
            usb->dev->vendor_id.data,
            usb->dev->product_id.data,
            (usb->serial.data) ? usb->serial.data : "NULL");

    nm_db_select(buf.data, &db_list);

    if (db_list.n_memb) {
        nm_warn(_(NM_MSG_USB_ATTAC));
        goto out;
    }

    rc = NM_OK;
out:
    nm_str_free(&buf);
    nm_str_free(&input);
    nm_vect_free(&db_list, nm_str_vect_free_cb);

    return rc;
}

static int nm_usb_unplug_get_data(nm_usb_data_t *usb, const nm_vect_t *db_list)
{
    int rc = NM_ERR;
    nm_str_t buf = NM_INIT_STR;
    nm_str_t input = NM_INIT_STR;
    uint32_t idx, idx_shift;
    char *fo;

    nm_get_field_buf(fields[1], &input);
    if (!input.len) {
        nm_warn(_(NM_MSG_USB_EMPTY));
        goto out;
    }

    nm_str_copy(&buf, &input);
    if ((fo = strchr(buf.data, ':')) == NULL) {
        /* Reinsurance, "fo" will always be OK */
        nm_warn(_(NM_MSG_USB_EDATA));
        goto out;
    }

    *fo = '\0';
    idx = nm_str_stoui(&buf, 10);
    idx_shift = --idx * NM_USB_IDX_COUNT;
    nm_debug("s:%s, idx=%u\n", buf.data, idx);

    nm_str_copy(&usb->dev->name,
            nm_vect_str(db_list, NM_SQL_USB_NAME + idx_shift));
    nm_str_copy(&usb->dev->vendor_id,
            nm_vect_str(db_list, NM_SQL_USB_VID + idx_shift));
    nm_str_copy(&usb->dev->product_id,
            nm_vect_str(db_list, NM_SQL_USB_PID + idx_shift));
    nm_str_copy(&usb->serial,
            nm_vect_str(db_list, NM_SQL_USB_SERIAL + idx_shift));

    rc = NM_OK;
out:
    nm_str_free(&buf);
    nm_str_free(&input);

    return rc;
}

static void
nm_usb_plug_update_db(const nm_str_t *name, const nm_usb_data_t *usb)
{
    nm_str_t query = NM_INIT_STR;

    nm_str_format(&query, NM_USB_ADD_SQL,
            name->data,
            usb->dev->name.data,
            usb->dev->vendor_id.data,
            usb->dev->product_id.data,
            (usb->serial.data) ? usb->serial.data : "NULL");

    nm_db_edit(query.data);

    nm_str_free(&query);
}
/* vim:set ts=4 sw=4: */
