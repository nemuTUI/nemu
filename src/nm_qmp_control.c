#include <nm_core.h>
#include <nm_string.h>

#define NM_QMP_CMD_INIT    "{\"execute\":\"qmp_capabilities\"}"
#define NM_QMP_CMD_VM_SHUT "{\"execute\":\"system_powerdown\"}"

void nm_qmp_vm_shut(const nm_str_t *name)
{
    //...
}

/* vim:set ts=4 sw=4 fdm=marker: */
