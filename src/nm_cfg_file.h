#ifndef NM_CFG_FILE_H_
#define NM_CFG_FILE_H_

#define NM_CFG_NAME       "nemu.cfg"
#define NM_DEFAULT_VMDIR  "nemu_vm"
#define NM_DEFAULT_DBFILE "nemu.db"
#define NM_DEFAULT_VNC    "/usr/bin/vncviewer"
#define NM_DEFAULT_TARGET "x86_64,i386"

typedef struct {
    nm_str_t *vm_dir;
    nm_str_t *db_path;
    nm_str_t *vnc_bin;
    nm_str_t *qemu_system_path;
    nm_str_t *log_path;
    nm_str_t **qemu_targets;
    uint32_t list_max;
    uint32_t vnc_listen_any:1;
} nm_cfg_t;

void nm_cfg_init(void);
const nm_cfg_t *nm_cfg_get(void);

#endif /* NM_CFG_FILE_H_ */
/* vim:set ts=4 sw=4 fdm=marker: */
