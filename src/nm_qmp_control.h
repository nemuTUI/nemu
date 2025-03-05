#ifndef NM_QMP_CONTROL_H_
#define NM_QMP_CONTROL_H_

#include <nm_string.h>
#include <nm_edit_net.h>
#include <nm_usb_devices.h>

void nm_qmp_vm_shut(const nm_str_t *name);
void nm_qmp_vm_stop(const nm_str_t *name);
void nm_qmp_vm_reset(const nm_str_t *name);
void nm_qmp_vm_pause(const nm_str_t *name);
void nm_qmp_vm_resume(const nm_str_t *name);
void nm_qmp_take_screenshot(const nm_str_t *name, const nm_str_t *path);
int nm_qmp_savevm(const nm_str_t *name, const nm_str_t *snap);
int nm_qmp_loadvm(const nm_str_t *name, const nm_str_t *snap);
int nm_qmp_delvm(const nm_str_t *name, const nm_str_t *snap);
int nm_qmp_usb_attach(const nm_str_t *name, const nm_usb_data_t *usb);
int nm_qmp_usb_detach(const nm_str_t *name, const nm_usb_data_t *usb);
int nm_qmp_nic_attach(const nm_str_t *name, const nm_iface_t *nic);
int nm_qmp_nic_detach(const nm_str_t *name, const nm_iface_t *nic);
int nm_qmp_test_socket(const nm_str_t *name);
void nm_qmp_vm_exec_async(const nm_str_t *name, const char *cmd,
        const char *jobid);

#endif /* NM_QMP_CONTROL_H_ */
/* vim:set ts=4 sw=4: */
