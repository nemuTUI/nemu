#include <nm_core.h>
#include <nm_utils.h>
#if NM_WITH_DBUS
#include <dbus/dbus.h>
#endif

#define DBUS_NOTIFY_OBJECT    "/org/freedesktop/Notifications"
#define DBUS_NOTIFY_INTERFACE "org.freedesktop.Notifications"
#define DBUS_NOTIFY_METHOD    "Notify"

#if NM_WITH_DBUS
static int
notify_build_message(DBusMessage* msg, const char *head, const char *body)
{
    DBusMessageIter args[2];
    dbus_bool_t rc = 0;
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

    dbus_message_iter_open_container(&args[0],DBUS_TYPE_ARRAY,DBUS_TYPE_STRING_AS_STRING,&args[1]);
    dbus_message_iter_close_container(&args[0],&args[1]);
    dbus_message_iter_open_container(&args[0],DBUS_TYPE_ARRAY,"{sv}",&args[1]);
    dbus_message_iter_close_container(&args[0],&args[1]);
    dbus_message_iter_append_basic(&args[0], DBUS_TYPE_INT32, &timeout);

    return rc;
}

void nm_dbus_send_notify(const char *title, const char *body)
{
    DBusMessage *msg = NULL;
    DBusConnection* conn = NULL;
    DBusError err;

    if (!nm_cfg_get()->dbus_enabled)
        return;

    dbus_error_init(&err);
    conn = dbus_bus_get(DBUS_BUS_SESSION, &err);

    if (dbus_error_is_set(&err)) {
        goto errexit;
    }

    if (!conn) {
        goto errexit;
    }

    if ((msg = dbus_message_new_method_call(
                    NULL,
                    DBUS_NOTIFY_OBJECT,
                    DBUS_NOTIFY_INTERFACE,
                    DBUS_NOTIFY_METHOD)) == NULL) {
        goto errexit;
    }

    dbus_message_set_no_reply(msg, TRUE);
    dbus_message_set_auto_start(msg, TRUE);
    dbus_message_set_destination(msg, DBUS_NOTIFY_INTERFACE);

    if (!notify_build_message(msg, title, body)) {
        nm_debug("%s: enomem\n", __func__);
    }

    if (!dbus_connection_send(conn, msg, NULL)) {
        goto errexit;
    }

    dbus_connection_flush(conn);

    goto clean;

errexit:
    nm_debug("%s: dbus error: %s\n", __func__, err.message);
clean:
    if (msg)
        dbus_message_unref(msg);
    if (conn)
        dbus_connection_unref(conn);
}
#endif

/* vim:set ts=4 sw=4: */
