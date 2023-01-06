#ifndef NM_REMOTE_API_H_
#define NM_REMOTE_API_H_

#define NM_API_VERSION "0.3"

#include <nm_mon_daemon.h>

#include <json.h>

static const char NM_API_RET_ARRAY[] = "{\"return\":[%s]}";
static const char NM_API_RET_VAL[]   = "{\"return\":\"%s\"}";
static const char NM_API_RET_OK[]    = "{\"return\":\"ok\"}";
static const char NM_API_RET_ERR[]   = "{\"return\":\"err\",\"error\":\"%s\"}";
static const char NM_API_RET_VAL_UINT[] = "{\"return\":\"%u\"}";

#define NM_API_PORT       0x501D
#define NM_API_MD_LEN     20
#define NM_API_CL_BUF_LEN 2048
#define NM_API_SHA256_LEN 65

typedef struct nm_api_ops {
    const char method[NM_API_MD_LEN];
    void (*run)(struct json_object *request, nm_str_t *reply);
} nm_api_ops_t;

typedef struct nm_api_ctx {
    nm_mon_vms_t *vms;
    nm_thr_ctrl_t *ctrl;
} nm_api_ctx_t;

#define NM_API_CTX_INIT (nm_api_ctx_t) { NULL, NULL }

void *nm_api_server(void *ctx);

#endif /* NM_REMOTE_API_H_ */
/* vim:set ts=4 sw=4: */
