#ifndef NM_VM_SNAPSHOT_H_
#define NM_VM_SNAPSHOT_H_

#include <nm_string.h>

void nm_vm_snapshot_create(const nm_str_t *name);
void nm_vm_snapshot_delete(const nm_str_t *name, int vm_status);
void nm_vm_snapshot_load(const nm_str_t *name, int vm_status);

void nm_vm_cli_snapshot_list(const nm_str_t *vm_name);
void nm_vm_cli_snapshot_save(const nm_str_t *vm_name,
        const nm_str_t *snap_name);
void nm_vm_cli_snapshot_load(const nm_str_t *vm_name,
        const nm_str_t *snap_name);
void nm_vm_cli_snapshot_del(const nm_str_t *vm_name,
        const nm_str_t *snap_name);

#endif /* NM_VM_SNAPSHOT_H_ */
/* vim:set ts=4 sw=4: */
