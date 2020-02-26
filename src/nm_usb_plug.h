#ifndef NM_USB_PLUG_H_
#define NM_USB_PLUG_H_

#include <nm_string.h>

void nm_usb_plug(const nm_str_t *name, int status);
void nm_usb_unplug(const nm_str_t *name, int status);
int nm_usb_check_plugged(const nm_str_t *name);

#endif /* NM_USB_PLUG_H_ */
/* vim:set ts=4 sw=4: */
