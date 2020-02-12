#ifndef NM_DBUS_H_
#define NM_DBUS_H_

#if NM_WITH_DBUS
void nm_dbus_send_notify(const char *title, const char *body);
#endif

#endif /* NM_DBUS_H_ */
/* vim:set ts=4 sw=4: */
