#ifndef NM_CFG_FILE_H_
#define NM_CFG_FILE_H_

#define NM_CFG_NAME       "nemu.cfg"
#define NM_DEFAULT_VMDIR  "nemu_vm"
#define NM_DEFAULT_DBFILE "nemu.db"
#define NM_DEFAULT_VNC    "/usr/bin/vncviewer"
#define NM_DEFAULT_TARGET "x86_64,i386"

#define NM_INI_S_MAIN   "main"
#define NM_INI_S_VNC    "vnc"
#define NM_INI_S_QEMU   "qemu"

#define NM_INI_P_VM     "vmdir"
#define NM_INI_P_DB     "db"
#define NM_INI_P_LIST   "list_max"
#define NM_INI_P_VBIN   "binary"
#define NM_INI_P_VANY   "listen_any"
#define NM_INI_P_QBIN   "qemu_system_path"
#define NM_INI_P_QTRG   "targets"
#define NM_INI_P_QLOG   "log_cmd"

typedef struct {
    nm_str_t vm_dir;
    nm_str_t db_path;
    nm_str_t vnc_bin;
    nm_str_t qemu_system_path;
    nm_str_t log_path;
    char **qemu_targets;
    uint32_t list_max;
    uint32_t vnc_listen_any:1;
} nm_cfg_t;

void nm_cfg_init(void);
void nm_cfg_free(void);
const nm_cfg_t *nm_cfg_get(void);

#endif /* NM_CFG_FILE_H_ */
/* vim:set ts=4 sw=4 fdm=marker: */
