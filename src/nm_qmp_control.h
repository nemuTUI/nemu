#ifndef NM_QMP_CONTROL_H_
#define NM_QMP_CONTROL_H_

void nm_qmp_vm_shut(const nm_str_t *name);
void nm_qmp_vm_stop(const nm_str_t *name);
void nm_qmp_vm_reset(const nm_str_t *name);
void nm_qmp_vm_snapshot(const nm_str_t *drive, const nm_str_t *path,
                        const nm_str_t *snap_name);

#endif /* NM_QMP_CONTROL_H_ */
/* vim:set ts=4 sw=4 fdm=marker: */
