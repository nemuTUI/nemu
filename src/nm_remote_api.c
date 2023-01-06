#include <nm_core.h>

#if defined (NM_WITH_REMOTE)
#include <nm_qmp_control.h>
#include <nm_remote_api.h>
#include <nm_mon_daemon.h>
#include <nm_vm_control.h>
#include <nm_database.h>
#include <nm_edit_vm.h>
#include <nm_utils.h>
#include <nm_form.h>

#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <poll.h>

#include <openssl/ssl.h>
#include <openssl/err.h>

#define NM_API_POLL_MAXFDS 256

static nm_api_ctx_t *mon_data;

static int nm_api_socket(int *sock);
static SSL_CTX *nm_api_tls_setup(void);
static void nm_api_serve(SSL *tls);
static int nm_api_check_auth(struct json_object *request, nm_str_t *reply);
static void nm_api_reply(const char *request, nm_str_t *reply);

/* API methods */
static void nm_api_md_version(struct json_object *request, nm_str_t *reply);
static void
nm_api_md_nemu_version(struct json_object *request, nm_str_t *reply);
static void nm_api_md_auth(struct json_object *request, nm_str_t *reply);
static void nm_api_md_vmlist(struct json_object *request, nm_str_t *reply);
static void nm_api_md_vmstart(struct json_object *request, nm_str_t *reply);
static void nm_api_md_vmstop(struct json_object *request, nm_str_t *reply);
static void nm_api_md_vmforcestop(struct json_object *request, nm_str_t *reply);
static void
nm_api_md_vmgetconnectport(struct json_object *request, nm_str_t *reply);
static void
nm_api_md_vmgetsettings(struct json_object *request, nm_str_t *reply);
static void
nm_api_md_vmsetsettings(struct json_object *request, nm_str_t *reply);

static nm_api_ops_t nm_api[] = {
    { .method = "nemu_version",        .run = nm_api_md_nemu_version     },
    { .method = "api_version",         .run = nm_api_md_version          },
    { .method = "auth",                .run = nm_api_md_auth             },
    { .method = "vm_list",             .run = nm_api_md_vmlist           },
    { .method = "vm_start",            .run = nm_api_md_vmstart          },
    { .method = "vm_stop",             .run = nm_api_md_vmstop           },
    { .method = "vm_force_stop",       .run = nm_api_md_vmforcestop      },
    { .method = "vm_get_connect_port", .run = nm_api_md_vmgetconnectport },
    { .method = "vm_get_settings",     .run = nm_api_md_vmgetsettings    },
    { .method = "vm_set_settings",     .run = nm_api_md_vmsetsettings    }
};

void *nm_api_server(void *ctx)
{
    struct pollfd fds[NM_API_POLL_MAXFDS];
    int sd, timeout, nfds = 1;
    SSL_CTX *tls_ctx = NULL;

    mon_data = ctx;

    SSL_library_init();
    if ((tls_ctx = nm_api_tls_setup()) == NULL) {
        pthread_exit(NULL);
    }

    if (nm_api_socket(&sd) != NM_OK) {
        pthread_exit(NULL);
    }

    memset(fds, 0, sizeof(fds));
    fds[0].fd = sd;
    fds[0].events = POLLIN;
    timeout = 1000; /* 1 second */

    nm_db_init();

    while (!mon_data->ctrl->stop) {
        struct sockaddr_in cl_addr;
        socklen_t len = sizeof(cl_addr);
        bool rebuild_fds = false;
        int cl_sd, rc, cur_fds;

        rc = poll(fds, nfds, timeout);
        if (rc == -1) { /*error */
            nm_debug("%s: poll() error: %s\n", __func__, strerror(errno));
            goto out;
        } else if (rc == 0) { /* nothing happens */
            continue;
        }

        cur_fds = nfds;
        for (int n = 0; n < cur_fds; n++) {
            if (fds[n].revents == 0) {
                continue;
            }

            /* check for unexpected result */
            if (fds[n].revents != POLLIN) {
                nm_debug("%s: unexpected event: %d\n",
                        __func__, fds[n].revents);
                goto out;
            }

            if (fds[n].fd == sd) { /* server socket event */
                do { /* accept all incoming connections */
                    cl_sd = accept(sd, (struct sockaddr *) &cl_addr, &len);
                    if (cl_sd < 0) {
                        if (errno != EWOULDBLOCK) {
                            nm_debug("%s: accept error: %s\n",
                                    __func__, strerror(errno));
                            goto out;
                        }
                        break;
                    }
                    if (fcntl(cl_sd, F_SETFD, FD_CLOEXEC) == -1) {
                        nm_debug("%s: fcntl error: %s\n",
                                __func__, strerror(errno));
                        goto out;
                    }

                    nm_debug("%s: connect to API from: %s\n",
                            __func__, inet_ntoa(cl_addr.sin_addr));
                    fds[nfds].fd = cl_sd;
                    fds[nfds].events = POLLIN;
                    if (nfds + 1 == NM_API_POLL_MAXFDS) {
                        nm_debug("%s: max clients <%d> reached\n",
                                __func__, NM_API_POLL_MAXFDS);
                        continue;
                    }
                    nfds++;
                } while (cl_sd != -1);
            } else { /* client socket event */
                SSL *tls;

                if ((tls = SSL_new(tls_ctx)) == NULL) {
                    nm_debug("%s: %s\n", __func__,
                            ERR_error_string(ERR_get_error(), NULL));
                    close(fds[n].fd);
                    goto out;
                }
                SSL_set_fd(tls, fds[n].fd);
                nm_api_serve(tls);
                SSL_free(tls);
                fds[n].fd = -1;
                rebuild_fds = true;
            }
        }

        if (rebuild_fds) {
            for (int i = 0; i < nfds; i++) {
                if (fds[i].fd == -1) {
                    for (int j = i; j < nfds; j++) {
                        fds[j].fd = fds[j + 1].fd;
                    }
                    i--;
                    nfds--;
                }
            }
            rebuild_fds = false;
        }
    }
out:
    close(sd);
    for (int i = 0; i < nfds; i++) {
        if (fds[i].fd > 0) {
            close(fds[i].fd);
        }
    }

    SSL_CTX_free(tls_ctx);
    nm_db_close();

    pthread_exit(NULL);
}

static void nm_api_reply(const char *request, nm_str_t *reply)
{
    struct json_object *parsed, *exec;
    const char *method;

    parsed = json_tokener_parse(request);
    if (!parsed) {
        nm_str_format(reply, NM_API_RET_ERR, "cannot parse json");
        return;
    }

    json_object_object_get_ex(parsed, "exec", &exec);
    if (!exec) {
        nm_str_format(reply, NM_API_RET_ERR, "exec param is missing");
        return;
    }

    method = json_object_get_string(exec);

    for (size_t n = 0; n < nm_arr_len(nm_api); n++) {
        if (nm_str_cmp_tt(nm_api[n].method, method) == NM_OK) {
            nm_api[n].run(parsed, reply);
            return;
        }
    }

    nm_str_format(reply, NM_API_RET_ERR, "unknown method");
    json_object_put(parsed);
}

static void nm_api_serve(SSL *tls)
{
    char buf[NM_API_CL_BUF_LEN] = {0};
    int sd, nread;

    if (SSL_accept(tls) != 1) {
        nm_debug("%s: %s\n", __func__, ERR_error_string(ERR_get_error(), NULL));
        goto out;
    }

    nread = SSL_read(tls, buf, sizeof(buf));
    if (nread > 0) {
        nm_str_t reply = NM_INIT_STR;

        buf[nread] = '\0';
        if (buf[nread - 1] == '\n') {
            buf[nread - 1] = '\0';
        }
        nm_debug("%s: got API cmd: \"%s\"\n", __func__, buf);
        nm_api_reply(buf, &reply);

        if (reply.len) {
            int rc;

            rc = SSL_write(tls, reply.data, reply.len);
            if (rc <= 0) {
                nm_debug("%s: %s\n", __func__,
                        ERR_error_string(ERR_get_error(), NULL));
            }
        }
        nm_str_free(&reply);
    }

out:
    sd = SSL_get_fd(tls);
    SSL_shutdown(tls);
    close(sd);
}

static SSL_CTX *nm_api_tls_setup(void)
{
    const nm_cfg_t *cfg = nm_cfg_get();
    SSL_CTX *ctx = NULL;

    OpenSSL_add_all_algorithms();
    SSL_load_error_strings();

    if ((ctx = SSL_CTX_new(TLS_server_method())) == NULL) {
        nm_debug("%s: %s\n", __func__, ERR_error_string(ERR_get_error(), NULL));
        return NULL;
    }

    if (SSL_CTX_set_min_proto_version(ctx, TLS1_2_VERSION) != 1) {
        nm_debug("%s: %s\n", __func__, ERR_error_string(ERR_get_error(), NULL));
        goto err;
    }

    if (SSL_CTX_use_certificate_file(ctx, cfg->api_cert_path.data,
                SSL_FILETYPE_PEM) != 1) {
        nm_debug("%s: %s\n", __func__, ERR_error_string(ERR_get_error(), NULL));
        goto err;
    }

    if (SSL_CTX_use_PrivateKey_file(ctx, cfg->api_key_path.data,
                SSL_FILETYPE_PEM) != 1) {
        nm_debug("%s: %s\n", __func__, ERR_error_string(ERR_get_error(), NULL));
        goto err;
    }

    if (SSL_CTX_check_private_key(ctx) != 1) {
        nm_debug("%s:%s\n", __func__, ERR_error_string(ERR_get_error(), NULL));
        goto err;
    }

    return ctx;
err:
    SSL_CTX_free(ctx);
    return NULL;
}

static int nm_api_socket(int *sock)
{
    struct sockaddr_in addr;
    int sd, opt = 1;

    if ((sd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        nm_debug("%s: error create socket: %s\n", __func__, strerror(errno));
        return NM_ERR;
    }

    if (setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(int)) == -1) {
        nm_debug("%s: error set socket opts: %s\n", __func__, strerror(errno));
        close(sd);
        return NM_ERR;
    }

    if (fcntl(sd, F_SETFL, O_NONBLOCK) == -1) {
        nm_debug("%s: set O_NONBLOCK failed: %s\n", __func__, strerror(errno));
        close(sd);
        return NM_ERR;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(nm_cfg_get()->api_port);

    if (!nm_cfg_get()->api_iface.len) {
        addr.sin_addr.s_addr = INADDR_ANY;
    } else {
        struct ifreq ifr;

        memset(&ifr, 0, sizeof(ifr));
        strncpy(ifr.ifr_name, nm_cfg_get()->api_iface.data, IFNAMSIZ - 1);

        if (ioctl(sd, SIOCGIFADDR, &ifr) != 0) {
            nm_debug("%s: cannot get addr: %s\n", __func__, strerror(errno));
            return NM_ERR;
        }

        addr.sin_addr = ((struct sockaddr_in *) &ifr.ifr_addr)->sin_addr;
    }

    if (bind(sd, (struct sockaddr *) &addr, sizeof(addr)) == -1) {
        nm_debug("%s: cannot bind: %s\n", __func__, strerror(errno));
        return NM_ERR;
    }

    if (listen(sd, SOMAXCONN) == -1) {
        nm_debug("%s: cannot listen: %s\n", __func__, strerror(errno));
        return NM_ERR;
    }

    *sock = sd;

    return NM_OK;
}

static int nm_api_check_auth(struct json_object *request, nm_str_t *reply)
{
    unsigned char hash[SHA256_DIGEST_LENGTH];
    char hash_str[NM_API_SHA256_LEN];
    struct json_object *auth;
    const char *pass;
    nm_str_t salted_pass = NM_INIT_STR;

    json_object_object_get_ex(request, "auth", &auth);
    if (!auth) {
        nm_str_format(reply, NM_API_RET_ERR, "auth param is missing");
        goto out;
    }

    pass = json_object_get_string(auth);
    nm_str_format(&salted_pass, "%s%s", pass, nm_cfg_get()->api_salt.data);

#if OPENSSL_VERSION_NUMBER < 0x30000000L
    SHA256_CTX sha256;

    SHA256_Init(&sha256);
    SHA256_Update(&sha256, salted_pass.data, salted_pass.len);
    SHA256_Final(hash, &sha256);
#else
    uint32_t md_len;
    EVP_MD_CTX *mdctx = EVP_MD_CTX_create();

    EVP_DigestInit_ex(mdctx, EVP_sha256(), NULL);
    EVP_DigestUpdate(mdctx, salted_pass.data, salted_pass.len);
    EVP_DigestFinal_ex(mdctx, hash, &md_len);
    EVP_MD_CTX_destroy(mdctx);
    EVP_cleanup();
#endif

    for (int n = 0; n < SHA256_DIGEST_LENGTH; n++) {
        sprintf(hash_str + (n * 2), "%02x", hash[n]);
    }
    hash_str[NM_API_SHA256_LEN - 1] = '\0';

    if (nm_str_cmp_st(&nm_cfg_get()->api_hash, hash_str) == NM_OK) {
        nm_str_free(&salted_pass);
        return NM_OK;
    }

    nm_str_format(reply, NM_API_RET_ERR, "access denied");
out:
    nm_str_free(&salted_pass);
    return NM_ERR;
}

static void nm_api_md_version(struct json_object *request, nm_str_t *reply)
{
    nm_str_format(reply, NM_API_RET_VAL, NM_API_VERSION);
    json_object_put(request);
}

static void nm_api_md_nemu_version(struct json_object *request, nm_str_t *reply)
{
    int rc = nm_api_check_auth(request, reply);

    if (rc != NM_OK) {
        goto out;
    }

    nm_str_format(reply, NM_API_RET_VAL, NM_VERSION);
out:
    json_object_put(request);
}

static void nm_api_md_auth(struct json_object *request, nm_str_t *reply)
{
    int rc = nm_api_check_auth(request, reply);

    if (rc == NM_OK) {
        nm_str_format(reply, "%s", NM_API_RET_OK);
    }

    json_object_put(request);
}

static void nm_api_md_vmlist(struct json_object *request, nm_str_t *reply)
{
    int rc = nm_api_check_auth(request, reply);
    nm_mon_vms_t *vms = mon_data->vms;
    nm_str_t list = NM_INIT_STR;

    if (rc != NM_OK) {
        goto out;
    }

    pthread_mutex_lock(&vms->mtx);
    for (size_t n = 0; n < vms->list->n_memb; n++) {
        nm_str_append_format(&list, "%s{\"name\":\"%s\",\"status\":%s}",
                (n) ? "," : "",
                nm_mon_item_get_name_cstr(vms->list, n),
                (nm_mon_item_get_status(vms->list, n) == NM_TRUE) ?
                "true" : "false");
    }
    pthread_mutex_unlock(&vms->mtx);
    nm_str_format(reply, NM_API_RET_ARRAY, list.data);

out:
    nm_str_free(&list);
    json_object_put(request);
}

static void nm_api_md_vmstart(struct json_object *request, nm_str_t *reply)
{
    int rc = nm_api_check_auth(request, reply);
    nm_mon_vms_t *vms = mon_data->vms;
    nm_str_t vmname = NM_INIT_STR;
    struct json_object *name;
    bool vm_exist = false;
    const char *name_str;

    if (rc != NM_OK) {
        goto out;
    }

    json_object_object_get_ex(request, "name", &name);
    if (!name) {
        nm_str_format(reply, NM_API_RET_ERR, "name param is missing");
        goto out;
    }

    name_str = json_object_get_string(name);
    for (size_t n = 0; n < vms->list->n_memb; n++) {
        if (nm_str_cmp_st(nm_mon_item_get_name(vms->list, n),
                    name_str) ==  NM_OK) {
            vm_exist = true;
            break;
        }
    }

    if (!vm_exist) {
        nm_str_format(reply, NM_API_RET_ERR, "VM does not exists");
        goto out;
    }

    nm_str_format(&vmname, "%s", name_str);
    if (nm_qmp_test_socket(&vmname) != NM_OK) {
        nm_vmctl_start(&vmname, 0);
        nm_str_format(reply, "%s", NM_API_RET_OK);
    } else {
        nm_str_format(reply, NM_API_RET_ERR, "already started");
    }
out:
    nm_str_free(&vmname);
    json_object_put(request);
}

static void nm_api_md_vmstop(struct json_object *request, nm_str_t *reply)
{
    int rc = nm_api_check_auth(request, reply);
    nm_mon_vms_t *vms = mon_data->vms;
    nm_str_t vmname = NM_INIT_STR;
    struct json_object *name;
    bool vm_exist = false;
    const char *name_str;

    if (rc != NM_OK) {
        goto out;
    }

    json_object_object_get_ex(request, "name", &name);
    if (!name) {
        nm_str_format(reply, NM_API_RET_ERR, "name param is missing");
        goto out;
    }

    name_str = json_object_get_string(name);
    for (size_t n = 0; n < vms->list->n_memb; n++) {
        if (nm_str_cmp_st(nm_mon_item_get_name(vms->list, n),
                    name_str) ==  NM_OK) {
            vm_exist = true;
            break;
        }
    }

    if (!vm_exist) {
        nm_str_format(reply, NM_API_RET_ERR, "VM does not exists");
        goto out;
    }

    nm_str_format(&vmname, "%s", name_str);
    nm_qmp_vm_shut(&vmname);
    nm_str_format(reply, "%s", NM_API_RET_OK);
out:
    nm_str_free(&vmname);
    json_object_put(request);
}

static void nm_api_md_vmforcestop(struct json_object *request, nm_str_t *reply)
{
    int rc = nm_api_check_auth(request, reply);
    nm_mon_vms_t *vms = mon_data->vms;
    nm_str_t vmname = NM_INIT_STR;
    struct json_object *name;
    bool vm_exist = false;
    const char *name_str;

    if (rc != NM_OK) {
        goto out;
    }

    json_object_object_get_ex(request, "name", &name);
    if (!name) {
        nm_str_format(reply, NM_API_RET_ERR, "name param is missing");
        goto out;
    }

    name_str = json_object_get_string(name);
    for (size_t n = 0; n < vms->list->n_memb; n++) {
        if (nm_str_cmp_st(nm_mon_item_get_name(vms->list, n),
                    name_str) ==  NM_OK) {
            vm_exist = true;
            break;
        }
    }

    if (!vm_exist) {
        nm_str_format(reply, NM_API_RET_ERR, "VM does not exists");
        goto out;
    }

    nm_str_format(&vmname, "%s", name_str);
    nm_qmp_vm_stop(&vmname);
    nm_str_format(reply, "%s", NM_API_RET_OK);
out:
    nm_str_free(&vmname);
    json_object_put(request);
}

static void
nm_api_md_vmgetconnectport(struct json_object *request, nm_str_t *reply)
{
    int rc = nm_api_check_auth(request, reply);
    nm_mon_vms_t *vms = mon_data->vms;
    nm_str_t query = NM_INIT_STR;
    nm_vect_t res = NM_INIT_VECT;
    struct json_object *name;
    bool vm_exist = false;
    const char *name_str;
    uint32_t port;

    if (rc != NM_OK) {
        goto out;
    }

    json_object_object_get_ex(request, "name", &name);
    if (!name) {
        nm_str_format(reply, NM_API_RET_ERR, "name param is missing");
        goto out;
    }

    name_str = json_object_get_string(name);
    for (size_t n = 0; n < vms->list->n_memb; n++) {
        if (nm_str_cmp_st(nm_mon_item_get_name(vms->list, n),
                    name_str) ==  NM_OK) {
            vm_exist = true;
            break;
        }
    }

    if (!vm_exist) {
        nm_str_format(reply, NM_API_RET_ERR, "VM does not exists");
        goto out;
    }

    nm_str_format(&query, NM_VMCTL_GET_VNC_PORT_SQL, name_str);
    nm_db_select(query.data, &res);
    port = nm_str_stoui(nm_vect_str(&res, 0), 10) + NM_STARTING_VNC_PORT;
    nm_str_format(reply, NM_API_RET_VAL_UINT, port);
out:
    nm_str_free(&query);
    nm_vect_free(&res, nm_str_vect_free_cb);
    json_object_put(request);
}

static struct json_object *
nm_api_json_kv_int(const char *key, int value)
{
    struct json_object *kv, *val;

    if ((kv = json_object_new_object()) == NULL) {
        return NULL;
    }

    if ((val = json_object_new_int(value)) == NULL) {
        json_object_put(kv);
        return NULL;
    }

    json_object_object_add(kv, key, val);

    return kv;
}

static struct json_object *
nm_api_json_kv_str(const char *key, const char *value)
{
    struct json_object *kv, *val;

    if ((kv = json_object_new_object()) == NULL) {
        return NULL;
    }

    if ((val = json_object_new_string(value)) == NULL) {
        json_object_put(kv);
        return NULL;
    }

    json_object_object_add(kv, key, val);

    return kv;
}

static struct json_object *
nm_api_json_kv_bool(const char *key, json_bool value)
{
    struct json_object *kv, *val;

    if ((kv = json_object_new_object()) == NULL) {
        return NULL;
    }

    if ((val = json_object_new_boolean(value)) == NULL) {
        json_object_put(kv);
        return NULL;
    }

    json_object_object_add(kv, key, val);

    return kv;
}

static struct json_object *
nm_api_json_kv_append_arr_str(struct json_object *kv, const char *key,
        const char **values)
{
    struct json_object *arr;

    if ((arr = json_object_new_array()) == NULL) {
        return NULL;
    }

    while (*values) {
        struct json_object *val = json_object_new_string(*values);

        if (val == NULL) {
            json_object_put(arr);
            return NULL;
        }
        json_object_array_add(arr, val);
        values++;
    }

    json_object_object_add(kv, key, arr);

    return kv;
}

static void
nm_api_md_vmgetsettings(struct json_object *request, nm_str_t *reply)
{
    struct json_object *name, *jrep, *kv;
    const char **disk_drivers = nm_form_drive_drv;
    int rc = nm_api_check_auth(request, reply);
    nm_vmctl_data_t vm = NM_VMCTL_INIT_DATA;
    nm_mon_vms_t *vms = mon_data->vms;
    nm_str_t name_str = NM_INIT_STR;
    nm_str_t settings = NM_INIT_STR;
    bool vm_exist = false;

    jrep = kv = NULL;

    if (rc != NM_OK) {
        goto out;
    }

    json_object_object_get_ex(request, "name", &name);
    if (!name) {
        nm_str_format(reply, NM_API_RET_ERR, "name param is missing");
        goto out;
    }

    nm_str_format(&name_str, "%s", json_object_get_string(name));
    for (size_t n = 0; n < vms->list->n_memb; n++) {
        if (nm_str_cmp_ss(nm_mon_item_get_name(vms->list, n),
                    &name_str) ==  NM_OK) {
            vm_exist = true;
            break;
        }
    }

    if (!vm_exist) {
        nm_str_format(reply, NM_API_RET_ERR, "VM does not exists");
        goto out;
    }

    nm_vmctl_get_data(&name_str, &vm);

    if ((jrep = json_object_new_object()) == NULL) {
        nm_str_format(reply, NM_API_RET_ERR, "Internal error");
        goto out;
    }

    /* CPU count */
    if ((kv = nm_api_json_kv_str("value",
                    nm_vect_str_ctx(&vm.main, NM_SQL_SMP))) == NULL) {
        nm_str_format(reply, NM_API_RET_ERR, "Internal error");
        goto out;
    }
    json_object_object_add(jrep, "smp", kv);

    /* the amount of RAM */
    if ((kv = nm_api_json_kv_int("value", nm_str_stoui(
                        nm_vect_str(&vm.main, NM_SQL_MEM), 10))) == NULL) {
        nm_str_format(reply, NM_API_RET_ERR, "Internal error");
        goto out;
    }
    json_object_object_add(jrep, "mem", kv);

    /* KVM status */
    if ((kv = nm_api_json_kv_bool("value",
                    (nm_str_cmp_st(nm_vect_str(&vm.main,
                                               NM_SQL_KVM), NM_ENABLE)
                     == NM_OK) ? TRUE : FALSE)) == NULL) {
        nm_str_format(reply, NM_API_RET_ERR, "Internal error");
        goto out;
    }
    json_object_object_add(jrep, "kvm", kv);

    /* host CPU status */
    if ((kv = nm_api_json_kv_bool("value",
                    (nm_str_cmp_st(nm_vect_str(&vm.main,
                                               NM_SQL_HCPU), NM_ENABLE)
                     == NM_OK) ? TRUE : FALSE)) == NULL) {
        nm_str_format(reply, NM_API_RET_ERR, "Internal error");
        goto out;
    }
    json_object_object_add(jrep, "hcpu", kv);

    /* network interface count */
    if ((kv = nm_api_json_kv_int("value",
                    vm.ifs.n_memb / NM_IFS_IDX_COUNT)) == NULL) {
        nm_str_format(reply, NM_API_RET_ERR, "Internal error");
        goto out;
    }
    json_object_object_add(jrep, "netifs", kv);

    /* disk interface driver */
    if ((kv = nm_api_json_kv_str("value", nm_vect_str_ctx(&vm.drives,
                        NM_SQL_DRV_TYPE))) == NULL) {
        nm_str_format(reply, NM_API_RET_ERR, "Internal error");
        goto out;
    }
    if ((kv = nm_api_json_kv_append_arr_str(kv, "value_list",
                    disk_drivers)) == NULL) {
        json_object_put(kv);
        nm_str_format(reply, NM_API_RET_ERR, "Internal error");
        goto out;
    }
    json_object_object_add(jrep, "disk_iface", kv);

    nm_str_format(reply, "%s", json_object_to_json_string(jrep));

out:
    json_object_put(jrep);
    nm_str_free(&name_str);
    nm_str_free(&settings);
    nm_vmctl_free_data(&vm);
    json_object_put(request);
}

static void
nm_api_md_vmsetsettings(struct json_object *request, nm_str_t *reply)
{
    regex_t reg;
    bool vm_exist = false;
    nm_str_t query = NM_INIT_STR;
    nm_str_t name_str = NM_INIT_STR;
    nm_mon_vms_t *vms = mon_data->vms;
    struct json_object *jreq;
    nm_vm_t vm_new = NM_INIT_VM;
    nm_vmctl_data_t vm_cur = NM_VMCTL_INIT_DATA;
    int rc = nm_api_check_auth(request, reply);

    memset(&reg, 0, sizeof(regex_t));

    if (rc != NM_OK) {
        goto out;
    }

    json_object_object_get_ex(request, "name", &jreq);
    if (!jreq) {
        nm_str_format(reply, NM_API_RET_ERR, "name param is missing");
        goto out;
    }

    nm_str_format(&name_str, "%s", json_object_get_string(jreq));
    for (size_t n = 0; n < vms->list->n_memb; n++) {
        if (nm_str_cmp_ss(nm_mon_item_get_name(vms->list, n),
                    &name_str) ==  NM_OK) {
            vm_exist = true;
            break;
        }
    }

    if (!vm_exist) {
        nm_str_format(reply, NM_API_RET_ERR, "VM does not exists");
        goto out;
    }

    nm_vmctl_get_data(&name_str, &vm_cur);

    json_object_object_get_ex(request, "smp", &jreq);
    if (jreq) {
        const char *regex = "^[0-9]+(:[0-9]+)?(:[0-9]+)?$";

        if (regcomp(&reg, regex, REG_EXTENDED) != 0) {
            nm_bug("%s: regcomp failed", __func__);
        }

        if (json_object_get_type(jreq) != json_type_string) {
            nm_str_format(reply, NM_API_RET_ERR, "Wrong `smp` type");
            goto out;
        }

        nm_str_format(&vm_new.cpus, "%s", json_object_get_string(jreq));
        if (regexec(&reg, vm_new.cpus.data, 0, NULL, 0) != 0) {
            nm_str_format(reply, NM_API_RET_ERR, "Incorrect `smp` value");
            goto out;
        }

        if (nm_str_cmp_ss(nm_vect_str(&vm_cur.main, NM_SQL_SMP),
                    &vm_new.cpus) != NM_OK) {
            nm_str_format(&query, "UPDATE vms SET smp='%s' WHERE name='%s'",
                    vm_new.cpus.data, name_str.data);
            nm_db_edit(query.data);
        }
    }

    json_object_object_get_ex(request, "mem", &jreq);
    if (jreq) {
        if (json_object_get_type(jreq) != json_type_int) {
            nm_str_format(reply, NM_API_RET_ERR, "Wrong `mem` type");
            goto out;
        }
        nm_str_format(&vm_new.memo, "%d", json_object_get_int(jreq));
        if (nm_str_cmp_ss(nm_vect_str(&vm_cur.main, NM_SQL_MEM),
                    &vm_new.memo) != NM_OK) {
            nm_str_format(&query, "UPDATE vms SET mem='%s' WHERE name='%s'",
                    vm_new.memo.data, name_str.data);
            nm_db_edit(query.data);
        }
    }

    json_object_object_get_ex(request, "kvm", &jreq);
    if (jreq) {
        bool cur_value = false;

        if (json_object_get_type(jreq) != json_type_boolean) {
            nm_str_format(reply, NM_API_RET_ERR, "Wrong `kvm` type");
            goto out;
        }

        vm_new.kvm.enable = json_object_get_boolean(jreq);
        if (nm_str_cmp_st(nm_vect_str(&vm_cur.main, NM_SQL_KVM),
                    NM_ENABLE) == NM_OK) {
            cur_value = true;
        }

        if (cur_value != vm_new.kvm.enable) {
            nm_str_format(&query, "UPDATE vms SET kvm='%s' WHERE name='%s'",
                    vm_new.kvm.enable ? NM_ENABLE : NM_DISABLE, name_str.data);
            nm_db_edit(query.data);
        }
    }

    json_object_object_get_ex(request, "hcpu", &jreq);
    if (jreq) {
        bool cur_value = false;

        if (json_object_get_type(jreq) != json_type_boolean) {
            nm_str_format(reply, NM_API_RET_ERR, "Wrong `hcpu` type");
            goto out;
        }

        vm_new.kvm.hostcpu_enable = json_object_get_boolean(jreq);
        if (nm_str_cmp_st(nm_vect_str(&vm_cur.main, NM_SQL_HCPU),
                    NM_ENABLE) == NM_OK) {
            cur_value = true;
        }

        if (cur_value != vm_new.kvm.hostcpu_enable) {
            nm_str_format(&query, "UPDATE vms SET hcpu='%s' WHERE name='%s'",
                    vm_new.kvm.hostcpu_enable ? NM_ENABLE : NM_DISABLE,
                    name_str.data);
            nm_db_edit(query.data);
        }
    }

    json_object_object_get_ex(request, "netifs", &jreq);
    if (jreq) {
        if (json_object_get_type(jreq) != json_type_int) {
            nm_str_format(reply, NM_API_RET_ERR, "Wrong `netifs` type");
            goto out;
        }
        vm_new.ifs.count = json_object_get_int(jreq);
        if ((vm_cur.ifs.n_memb / NM_IFS_IDX_COUNT) != vm_new.ifs.count) {
            nm_edit_update_ifs(&query,
                    &vm_cur, &vm_new, nm_form_get_last_mac());
        }
    }

    json_object_object_get_ex(request, "disk_iface", &jreq);
    if (jreq) {
        const char **disk_drivers = nm_form_drive_drv;
        bool found = false;

        if (json_object_get_type(jreq) != json_type_string) {
            nm_str_format(reply, NM_API_RET_ERR, "Wrong `disk_iface` type");
            goto out;
        }

        nm_str_format(&vm_new.drive.driver, "%s", json_object_get_string(jreq));
        while (*disk_drivers) {
            if (nm_str_cmp_st(&vm_new.drive.driver, *disk_drivers) == NM_OK) {
                found = true;
                break;
            }
            disk_drivers++;
        }

        if (!found) {
            nm_str_format(reply, NM_API_RET_ERR, "Wrong `disk_iface` value");
            goto out;
        }

        if (nm_str_cmp_ss(nm_vect_str(&vm_cur.drives, NM_SQL_DRV_TYPE),
                    &vm_new.drive.driver) != NM_OK) {
            nm_str_format(&query, "UPDATE drives SET drive_drv='%s' "
                    "WHERE vm_name='%s'",
                    vm_new.drive.driver.data, name_str.data);
            nm_db_edit(query.data);
        }
    }

    nm_str_format(reply, "%s", NM_API_RET_OK);

out:
    regfree(&reg);
    nm_str_free(&query);
    nm_str_free(&name_str);
    nm_vmctl_free_data(&vm_cur);
    nm_vm_free(&vm_new);
    json_object_put(request);
}

#endif /* NM_WITH_REMOTE */
/* vim:set ts=4 sw=4: */
