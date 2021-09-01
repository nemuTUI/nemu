#ifndef NM_DBUS_H_
#define NM_DBUS_H_

#if defined(NM_WITH_DBUS)
void nm_dbus_send_notify(const char *title, const char *body);
void nm_dbus_disconnect(void);
#endif

#endif /* NM_DBUS_H_ */
/* vim:set ts=4 sw=4: */
