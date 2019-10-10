#ifndef NM_CFG_FILE_H_
#define NM_CFG_FILE_H_

#include <nm_string.h>
#include <nm_vector.h>

#if defined(NM_WITH_VNC_CLIENT) || defined(NM_WITH_SPICE)
typedef struct {
    ssize_t title;
    ssize_t port;
} nm_view_args_t;
#endif

#define NM_INIT_AD_VIEW (nm_view_args_t) {-1, -1}

typedef struct {
    short r;
    short g;
    short b;
} nm_rgb_t;

typedef struct {
    nm_str_t vm_dir;
    nm_str_t db_path;
#if defined(NM_WITH_VNC_CLIENT) || defined(NM_WITH_SPICE)
    nm_str_t vnc_bin;
    nm_str_t vnc_args;
    nm_str_t spice_bin;
    nm_str_t spice_args;
    nm_view_args_t spice_view;
    nm_view_args_t vnc_view;
#endif
    nm_str_t log_path;
    nm_vect_t qemu_targets;
    nm_rgb_t hl_color;
    uint32_t listen_any:1;
    uint32_t spice_default:1;
    uint32_t log_enabled:1;
    uint32_t hl_is_set:1;
} nm_cfg_t;

void nm_cfg_init(void);
void nm_cfg_free(void);
const nm_cfg_t *nm_cfg_get(void);

static inline char** nm_cfg_get_arch() {
    return (char **)nm_cfg_get()->qemu_targets.data;
}

#endif /* NM_CFG_FILE_H_ */
/* vim:set ts=4 sw=4: */
