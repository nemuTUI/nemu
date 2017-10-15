#ifndef NM_VM_SNAPSHOT_H_
#define NM_VM_SNAPSHOT_H_

void nm_vm_snapshot_create(const nm_str_t *name);
void nm_vm_snapshot_delete(const nm_str_t *name, int vm_status);
void nm_vm_snapshot_load(const nm_str_t *name, int vm_status);

#endif /* NM_VM_SNAPSHOT_H_ */
/* vim:set ts=4 sw=4 fdm=marker: */
