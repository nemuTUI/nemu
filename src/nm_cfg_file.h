#ifndef NM_CFG_FILE_H_
#define NM_CFG_FILE_H_

#include <nm_vector.h>

typedef struct {
    nm_str_t vm_dir;
    nm_str_t db_path;
    nm_str_t vnc_bin;
    nm_str_t log_path;
    nm_vect_t qemu_targets;
    uint32_t vnc_listen_any:1;
    uint32_t log_enabled:1;
} nm_cfg_t;

void nm_cfg_init(void);
void nm_cfg_free(void);
const nm_cfg_t *nm_cfg_get(void);

#define nm_cfg_get_arch() (char **) nm_cfg_get()->qemu_targets.data

#endif /* NM_CFG_FILE_H_ */
/* vim:set ts=4 sw=4 fdm=marker: */
