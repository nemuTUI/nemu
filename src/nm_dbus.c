#include <nm_core.h>
#include <nm_utils.h>
#if defined (NM_WITH_DBUS)
#include <dbus/dbus.h>
#endif

#define DBUS_NOTIFY_OBJECT    "/org/freedesktop/Notifications"
#define DBUS_NOTIFY_INTERFACE "org.freedesktop.Notifications"
#define DBUS_NOTIFY_METHOD    "Notify"

#if defined (NM_WITH_DBUS)
static DBusConnection* dbus_conn = NULL;
static DBusError dbus_err;

static int nm_dbus_get_error(void)
{
    if (dbus_error_is_set(&dbus_err)) {
        nm_debug("%s: dbus error: %s\n", __func__, dbus_err.message);
        dbus_error_free(&dbus_err);

        return NM_OK;
    }

    return NM_ERR;
}

int nm_dbus_connect(void)
{
    if (!nm_cfg_get()->dbus_enabled)
        return NM_OK;

    dbus_error_init(&dbus_err);
    dbus_conn = dbus_bus_get(DBUS_BUS_SESSION, &dbus_err);

    if (nm_dbus_get_error() == NM_OK) {
        return NM_ERR;
    }

    if (!dbus_conn) {
        nm_debug("%s: NULL DBusConnection\n", __func__);
        return NM_ERR;
    }

    return NM_OK;
}

void nm_dbus_disconnect(void)
{
    if (!nm_cfg_get()->dbus_enabled)
        return;

    if (dbus_conn) {
        dbus_connection_unref(dbus_conn);
    }
}

static int
nm_dbus_build_message(DBusMessage* msg, const char *head, const char *body)
{
    DBusMessageIter args[2];
    dbus_bool_t rc = 1;
    const char *app[] = {NM_PROGNAME};
    uint32_t replaces_id = -1;
    const char *app_icon[] = {""};
    int32_t timeout = nm_cfg_get()->dbus_timeout;

    /* TODO add return code checking */
    dbus_message_iter_init_append(msg, &args[0]);
    dbus_message_iter_append_basic(&args[0], DBUS_TYPE_STRING, &app);
    dbus_message_iter_append_basic(&args[0], DBUS_TYPE_UINT32, &replaces_id);
    dbus_message_iter_append_basic(&args[0], DBUS_TYPE_STRING, &app_icon);
    dbus_message_iter_append_basic(&args[0], DBUS_TYPE_STRING, &head);
    dbus_message_iter_append_basic(&args[0], DBUS_TYPE_STRING, &body);

    dbus_message_iter_open_container(&args[0], DBUS_TYPE_ARRAY,DBUS_TYPE_STRING_AS_STRING, &args[1]);
    dbus_message_iter_close_container(&args[0], &args[1]);
    dbus_message_iter_open_container(&args[0], DBUS_TYPE_ARRAY, "{sv}", &args[1]);
    dbus_message_iter_close_container(&args[0], &args[1]);
    dbus_message_iter_append_basic(&args[0], DBUS_TYPE_INT32, &timeout);

    return rc;
}

void nm_dbus_send_notify(const char *title, const char *body)
{
    DBusMessage *msg = NULL;

    if (!nm_cfg_get()->dbus_enabled)
        return;

    if ((msg = dbus_message_new_method_call(
                    NULL,
                    DBUS_NOTIFY_OBJECT,
                    DBUS_NOTIFY_INTERFACE,
                    DBUS_NOTIFY_METHOD)) == NULL) {
        nm_dbus_get_error();
        goto clean;
    }

    dbus_message_set_no_reply(msg, TRUE);
    dbus_message_set_auto_start(msg, TRUE);
    dbus_message_set_destination(msg, DBUS_NOTIFY_INTERFACE);

    if (!nm_dbus_build_message(msg, title, body)) {
        nm_debug("%s: enomem\n", __func__);
        goto clean;
    }

    if (!dbus_connection_send(dbus_conn, msg, NULL)) {
        nm_dbus_get_error();
        goto clean;
    }

    dbus_connection_flush(dbus_conn);

clean:
    if (msg)
        dbus_message_unref(msg);
}
#endif

/* vim:set ts=4 sw=4: */
