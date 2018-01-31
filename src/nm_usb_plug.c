#include <nm_core.h>
#include <nm_form.h>
#include <nm_utils.h>
#include <nm_string.h>
#include <nm_window.h>
#include <nm_vector.h>
#include <nm_database.h>
#include <nm_usb_devices.h>
#include <nm_qmp_control.h>

static void nm_usb_plug_list(nm_vect_t *devs, nm_vect_t *names);
static void nm_usb_unplug_list(const nm_vect_t *db_list, nm_vect_t *names);
static int nm_usb_plug_get_data(const nm_str_t *name, nm_usb_data_t *usb,
        const nm_vect_t *usb_list);
static int nm_usb_unplug_get_data(nm_usb_data_t *usb, const nm_vect_t *db_list);
static void nm_usb_plug_update_db(const nm_str_t *name, const nm_usb_data_t *usb);

static nm_field_t *fields[2];

void nm_usb_plug(const nm_str_t *name)
{
    nm_form_t *form = NULL;
    nm_window_t *window = NULL;
    nm_spinner_data_t sp_data = NM_INIT_SPINNER;
    nm_str_t buf = NM_INIT_STR;
    size_t msg_len;
    pthread_t spin_th;
    int done = 0;
    nm_vect_t usb_devs = NM_INIT_VECT;
    nm_vect_t usb_names = NM_INIT_VECT;
    nm_vect_t db_result = NM_INIT_VECT;
    nm_usb_data_t usb = NM_INIT_USB_DATA;

    /* check for usb enabled first */
    nm_str_format(&buf, NM_USB_CHECK_SQL, name->data);
    nm_db_select(buf.data, &db_result);
    nm_str_trunc(&buf, 0);

    if (nm_str_cmp_st(nm_vect_str(&db_result, 0), NM_DISABLE) == NM_OK)
    {
        nm_print_warn(3, 2, _("USB must be enabled at boot time"));
        goto out;
    }

    nm_usb_plug_list(&usb_devs, &usb_names);

    if (usb_names.n_memb == 0)
    {
        nm_print_warn(3, 2, _("There are no usb devices"));
        goto out;
    }
    
    nm_print_title(_(NM_EDIT_TITLE));
    window = nm_init_window(7, 62, 3);
    init_pair(1, COLOR_BLACK, COLOR_WHITE);
    wbkgd(window, COLOR_PAIR(1));

    fields[0] = new_field(1, 45, 2, 0, 0, 0);
    set_field_back(fields[0], A_UNDERLINE);
    fields[1] = NULL;

    set_field_type(fields[0], TYPE_ENUM, usb_names.data, false, false);
    field_opts_off(fields[0], O_STATIC);
    set_field_buffer(fields[0], 0, *usb_names.data);

    nm_str_format(&buf, _("Attach usb to %s"), name->data);
    mvwaddstr(window, 1, 2, buf.data);
    mvwaddstr(window, 4, 2, _("Device"));

    form = nm_post_form(window, fields, 14);
    if (nm_draw_form(window, form) != NM_OK)
        goto out;

    if ((nm_usb_plug_get_data(name, &usb, &usb_devs)) != NM_OK)
        goto out;

    msg_len = mbstowcs(NULL, _(NM_EDIT_TITLE), strlen(_(NM_EDIT_TITLE)));
    sp_data.stop = &done;
    sp_data.x = (getmaxx(stdscr) + msg_len + 2) / 2;

    if (pthread_create(&spin_th, NULL, nm_spinner, (void *) &sp_data) != 0)
        nm_bug(_("%s: cannot create thread"), __func__);
    
    if (nm_qmp_usb_attach(name, &usb) == NM_OK)
        nm_usb_plug_update_db(name, &usb);

    done = 1;
    if (pthread_join(spin_th, NULL) != 0)
        nm_bug(_("%s: cannot join thread"), __func__);

out:
    nm_vect_free(&usb_names, NULL);
    nm_vect_free(&usb_devs, nm_usb_vect_free_cb);
    nm_vect_free(&db_result, nm_str_vect_free_cb);

    nm_form_free(form, fields);
    nm_str_free(&buf);
    nm_str_free(&usb.serial);
}

void nm_usb_unplug(const nm_str_t *name)
{
    nm_form_t *form = NULL;
    nm_window_t *window = NULL;
    nm_spinner_data_t sp_data = NM_INIT_SPINNER;
    nm_str_t buf = NM_INIT_STR;
    size_t msg_len;
    pthread_t spin_th;
    int done = 0;
    nm_usb_data_t usb_data = NM_INIT_USB_DATA;
    nm_usb_dev_t usb_dev = NM_INIT_USB;
    nm_vect_t usb_names = NM_INIT_VECT;
    nm_vect_t db_result = NM_INIT_VECT;

    usb_data.dev = &usb_dev;

    nm_str_format(&buf, NM_USB_GET_SQL, name->data);
    nm_db_select(buf.data, &db_result);
    nm_str_trunc(&buf, 0);

    if (!db_result.n_memb)
    {
        nm_print_warn(3, 2, _("There are no devices attached"));
        goto out;
    }

    nm_usb_unplug_list(&db_result, &usb_names);

    nm_print_title(_(NM_EDIT_TITLE));
    window = nm_init_window(7, 62, 3);
    init_pair(1, COLOR_BLACK, COLOR_WHITE);
    wbkgd(window, COLOR_PAIR(1));

    fields[0] = new_field(1, 45, 2, 0, 0, 0);
    set_field_back(fields[0], A_UNDERLINE);
    fields[1] = NULL;

    set_field_type(fields[0], TYPE_ENUM, usb_names.data, false, false);
    field_opts_off(fields[0], O_STATIC);
    set_field_buffer(fields[0], 0, *usb_names.data);

    nm_str_format(&buf, _("Detach usb from %s"), name->data);
    mvwaddstr(window, 1, 2, buf.data);
    mvwaddstr(window, 4, 2, _("Device"));

    form = nm_post_form(window, fields, 14);
    if (nm_draw_form(window, form) != NM_OK)
        goto out;

    if (nm_usb_unplug_get_data(&usb_data, &db_result) != NM_OK)
        goto out;
    
    msg_len = mbstowcs(NULL, _(NM_EDIT_TITLE), strlen(_(NM_EDIT_TITLE)));
    sp_data.stop = &done;
    sp_data.x = (getmaxx(stdscr) + msg_len + 2) / 2;

    if (pthread_create(&spin_th, NULL, nm_spinner, (void *) &sp_data) != 0)
        nm_bug(_("%s: cannot create thread"), __func__);

    nm_qmp_usb_detach(name, &usb_data);

    nm_str_trunc(&buf, 0);
    nm_str_format(&buf, NM_USB_DELETE_SQL,
            name->data,
            usb_dev.name.data,
            usb_dev.vendor_id.data,
            usb_dev.product_id.data,
            usb_data.serial.data);
    nm_db_edit(buf.data);

    done = 1;
    if (pthread_join(spin_th, NULL) != 0)
        nm_bug(_("%s: cannot join thread"), __func__);

out:
    nm_vect_free(&usb_names, NULL);
    nm_vect_free(&db_result, nm_str_vect_free_cb);
    nm_form_free(form, fields);
    nm_usb_data_free(&usb_data);
    nm_str_free(&buf);
}

static void nm_usb_unplug_list(const nm_vect_t *db_list, nm_vect_t *names)
{
    size_t dev_count = db_list->n_memb / NM_USB_IDX_COUNT;
    nm_str_t buf = NM_INIT_STR;

    for (size_t n = 0; n < dev_count; n++)
    {
        size_t idx_shift = NM_USB_IDX_COUNT * n;

        nm_str_format(&buf, "%zu:%s [serial:%s]", n + 1, 
                nm_vect_str_ctx(db_list, NM_SQL_USB_NAME + idx_shift),
                nm_vect_str_ctx(db_list, NM_SQL_USB_SERIAL + idx_shift));
        nm_vect_insert(names, buf.data, buf.len + 1, NULL);

        nm_str_trunc(&buf, 0);
    }

    nm_str_free(&buf);
}

static void nm_usb_plug_list(nm_vect_t *devs, nm_vect_t *names)
{
    nm_usb_get_devs(devs);
    nm_str_t dev_name = NM_INIT_STR;

    for (size_t n = 0; n < devs->n_memb; n++)
    {
        nm_str_format(&dev_name, "%zu:%s", n + 1, nm_usb_name(devs->data[n]).data);
        nm_vect_insert(names, dev_name.data, dev_name.len + 1, NULL);

        nm_str_trunc(&dev_name, 0);

        nm_debug("usb >> %03u:%03u %s:%s %s\n",
                nm_usb_bus_num(devs->data[n]),
                nm_usb_dev_addr(devs->data[n]),
                nm_usb_vendor_id(devs->data[n]).data,
                nm_usb_product_id(devs->data[n]).data,
                nm_usb_name(devs->data[n]).data);
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

    nm_get_field_buf(fields[0], &input);
    if (!input.len)
    {
        nm_print_warn(3, 2, _("Empty device name"));
        goto out;
    }

    nm_str_copy(&buf, &input);
    if ((fo = strchr(buf.data, ':')) == NULL)
    {
        /* Reinsurance, "fo" will always be OK */
        nm_print_warn(3, 2, _("Malformed input data"));
        goto out;
    }

    nm_str_trunc(&input, 0);
    nm_str_add_text(&input, fo + 1);

    *fo = '\0';
    idx = nm_str_stoui(&buf, 10);
    usb->dev = usb_list->data[idx - 1];

    nm_usb_get_serial(usb->dev, &usb->serial);

    nm_str_trunc(&buf, 0);
    nm_str_format(&buf, NM_USB_EXISTS_SQL,
            name->data,
            usb->dev->name.data,
            usb->dev->vendor_id.data,
            usb->dev->product_id.data,
            (usb->serial.data) ? usb->serial.data : "NULL");

    nm_db_select(buf.data, &db_list);

    if (db_list.n_memb)
    {
        nm_print_warn(3, 2, _("Already attached"));
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

    nm_get_field_buf(fields[0], &input);
    if (!input.len)
    {
        nm_print_warn(3, 2, _("Empty device name"));
        goto out;
    }

    nm_str_copy(&buf, &input);
    if ((fo = strchr(buf.data, ':')) == NULL)
    {
        /* Reinsurance, "fo" will always be OK */
        nm_print_warn(3, 2, _("Malformed input data"));
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

static void nm_usb_plug_update_db(const nm_str_t *name, const nm_usb_data_t *usb)
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
/* vim:set ts=4 sw=4 fdm=marker: */
