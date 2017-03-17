#ifndef NM_CFG_FILE_H_
#define NM_CFG_FILE_H_

#define NM_CFG_NAME "nemu.cfg"

typedef struct {
    char *vm_dir;
    char *db_path;
    char *vnc_bin;
    char *qemu_system_path;
    char *log_path;
    char **qemu_targets;
    uint32_t list_max;
    uint32_t vnc_listen_any:1;
} nm_cfg_t;

void nm_cfg_init(void);
const nm_cfg_t *nm_cfg_get(void);

#endif /* NM_CFG_FILE_H_ */
/* vim:set ts=4 sw=4 fdm=marker: */
