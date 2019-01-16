#include <nm_core.h>
#include <nm_utils.h>
#include <nm_string.h>
#include <nm_vector.h>
#include <nm_cfg_file.h>
#include <nm_ini_parser.h>

#define NM_CFG_NAME         "nemu.cfg"
#define NM_DEFAULT_VMDIR    "nemu_vm"
#define NM_DEFAULT_DBFILE   "nemu.db"
#define NM_DEFAULT_VNC      "/usr/bin/vncviewer"
#define NM_DEFAULT_VNCARG   ":%p"
#define NM_DEFAULT_SPICE    "/usr/bin/remote-viewer"
#define NM_DEFAULT_SPICEARG "--title %t spice://127.0.0.1:%p"
#define NM_DEFAULT_TARGET   "x86_64,i386"

#define NM_INI_S_MAIN     "main"
#define NM_INI_S_VIEW     "viewer"
#define NM_INI_S_QEMU     "qemu"

#define NM_INI_P_VM       "vmdir"
#define NM_INI_P_DB       "db"
#define NM_INI_P_HL       "hl_color"
#define NM_INI_P_PROT     "spice_default"
#define NM_INI_P_VBIN     "vnc_bin"
#define NM_INI_P_VARG     "vnc_args"
#define NM_INI_P_SBIN     "spice_bin"
#define NM_INI_P_SARG     "spice_args"
#define NM_INI_P_VANY     "listen_any"
#define NM_INI_P_QTRG     "targets"
#define NM_INI_P_QENL     "enable_log"
#define NM_INI_P_QLOG     "log_cmd"

static nm_cfg_t cfg;

static void nm_generate_cfg(const char *home, const nm_str_t *cfg_path);
static void nm_get_input(const char *msg, nm_str_t *res);
static inline void nm_get_param(const void *ini, const char *section,
                                const char *value, nm_str_t *res);
static inline int nm_get_opt_param(const void *ini, const char *section,
                                   const char *value, nm_str_t *res);
static inline void nm_cfg_get_color(size_t pos, short *color,
                                    const nm_str_t *buf);
#if defined(NM_WITH_VNC_CLIENT) || defined(NM_WITH_SPICE)
static void nm_cfg_get_view(nm_view_args_t *view, const nm_str_t *buf);
#endif

void nm_cfg_init(void)
{
    struct stat file_info;
    struct passwd *pw = getpwuid(getuid());
    nm_str_t cfg_path = NM_INIT_STR;
    nm_str_t tmp_buf = NM_INIT_STR;
    nm_ini_node_t *ini;
#ifdef NM_WITH_VNC_CLIENT
    cfg.vnc_view = NM_INIT_AD_VIEW;
#endif
#ifdef NM_WITH_SPICE
    cfg.spice_view = NM_INIT_AD_VIEW;
#endif
#ifndef NM_WITH_SPICE
    cfg.spice_default = 0;
#endif

    if (!pw)
        nm_bug(_("Error get home directory: %s\n"), strerror(errno));

    nm_str_alloc_text(&cfg_path, pw->pw_dir);
    nm_str_add_text(&cfg_path, "/." NM_CFG_NAME);

    nm_generate_cfg(pw->pw_dir, &cfg_path);
    ini = nm_ini_parser_init(&cfg_path);
#ifdef NM_DEBUG
    nm_ini_parser_dump(ini);
#endif
    /* Get VM storage directory path */
    nm_get_param(ini, NM_INI_S_MAIN, NM_INI_P_VM, &cfg.vm_dir);

    if (stat(cfg.vm_dir.data, &file_info) == -1)
        nm_bug("cfg: %s: %s", cfg.vm_dir.data, strerror(errno));
    if ((file_info.st_mode & S_IFMT) != S_IFDIR)
        nm_bug(_("cfg: %s is not a directory"), cfg.vm_dir.data);
    if (access(cfg.vm_dir.data, W_OK) != 0)
        nm_bug(_("cfg: no write access to %s"), cfg.vm_dir.data);

    /* Get database file path */
    nm_get_param(ini, NM_INI_S_MAIN, NM_INI_P_DB, &cfg.db_path);

    nm_str_dirname(&cfg.db_path, &tmp_buf);
    if (access(tmp_buf.data, W_OK) != 0)
        nm_bug(_("cfg: no write access to %s"), tmp_buf.data);
    nm_str_trunc(&tmp_buf, 0);

#ifdef NM_WITH_VNC_CLIENT
    /* Get the VNC client binary path */
    nm_get_param(ini, NM_INI_S_VIEW, NM_INI_P_VBIN, &cfg.vnc_bin);

    if (stat(cfg.vnc_bin.data, &file_info) == -1)
        nm_bug("cfg: %s: %s", cfg.vnc_bin.data, strerror(errno));

    nm_get_param(ini, NM_INI_S_VIEW, NM_INI_P_VARG, &cfg.vnc_args);
    nm_cfg_get_view(&cfg.vnc_view, &cfg.vnc_args);
#endif /* NM_WITH_VNC_CLIENT */
#ifdef NM_WITH_SPICE
    /* Get the SPICE client binary path */
    nm_get_param(ini, NM_INI_S_VIEW, NM_INI_P_SBIN, &cfg.spice_bin);

    if (stat(cfg.spice_bin.data, &file_info) == -1)
        nm_bug("cfg: %s: %s", cfg.spice_bin.data, strerror(errno));

    nm_get_param(ini, NM_INI_S_VIEW, NM_INI_P_SARG, &cfg.spice_args);
    nm_cfg_get_view(&cfg.spice_view, &cfg.spice_args);

    nm_get_param(ini, NM_INI_S_VIEW, NM_INI_P_PROT, &tmp_buf);
    cfg.spice_default = !!nm_str_stoui(&tmp_buf, 10);
    nm_str_trunc(&tmp_buf, 0);
#endif /* NM_WITH_SPICE */
    /* Get the VNC listen value */
    nm_get_param(ini, NM_INI_S_VIEW, NM_INI_P_VANY, &tmp_buf);
    cfg.listen_any = !!nm_str_stoui(&tmp_buf, 10);
    nm_str_trunc(&tmp_buf, 0);

    /* Get QEMU targets list */
    nm_get_param(ini, NM_INI_S_QEMU, NM_INI_P_QTRG, &tmp_buf);
    {
        char *token;
        char *saveptr = tmp_buf.data;
        nm_str_t qemu_bin = NM_INIT_STR;

        while ((token = strtok_r(saveptr, ",", &saveptr)))
        {
            nm_str_format(&qemu_bin, "%s/bin/qemu-system-%s",
                NM_STRING(NM_USR_PREFIX), token);

            if (stat(qemu_bin.data, &file_info) == -1)
                nm_bug(_("cfg: %s: %s"), qemu_bin.data, strerror(errno));

            nm_vect_insert(&cfg.qemu_targets, token, strlen(token) + 1, NULL);
            nm_str_trunc(&qemu_bin, 0);
        }

        nm_vect_end_zero(&cfg.qemu_targets); /* need for ncurses form */
        nm_str_trunc(&tmp_buf, 0);
        nm_str_free(&qemu_bin);
#ifdef NM_DEBUG
        const char **tp = (const char **) cfg.qemu_targets.data;

        nm_debug("\nConfigured QEMU targets:\n");
        for (; *tp != NULL; tp++)
            nm_debug(">> %s\n", *tp);
#endif
    }

    /* Get log enable flag */
    nm_get_param(ini, NM_INI_S_QEMU, NM_INI_P_QENL, &tmp_buf);
    cfg.log_enabled = !!nm_str_stoui(&tmp_buf, 10);
    nm_str_trunc(&tmp_buf, 0);

    if (cfg.log_enabled)
    {
        /* Get file log path */
        nm_get_param(ini, NM_INI_S_QEMU, NM_INI_P_QLOG, &cfg.log_path);

        nm_str_dirname(&cfg.log_path, &tmp_buf);
        if (access(tmp_buf.data, W_OK) != 0)
            nm_bug(_("cfg: no write access to %s"), tmp_buf.data);
    }

    nm_str_trunc(&tmp_buf, 0);
    if (nm_get_opt_param(ini, NM_INI_S_MAIN, NM_INI_P_HL, &tmp_buf) == NM_OK)
    {
        if (tmp_buf.len != 6)
            nm_bug(_("cfg: incorrect color value %s, example:5fafff"), tmp_buf.data);

        nm_cfg_get_color(0, &cfg.hl_color.r, &tmp_buf);
        nm_cfg_get_color(2, &cfg.hl_color.g, &tmp_buf);
        nm_cfg_get_color(4, &cfg.hl_color.b, &tmp_buf);

        cfg.hl_is_set = 1;
        nm_debug("HL color: r:%d g:%d b:%d\n",
                cfg.hl_color.r, cfg.hl_color.g, cfg.hl_color.b);
    }

    nm_ini_parser_free(ini);
    nm_str_free(&cfg_path);
    nm_str_free(&tmp_buf);
}

const nm_cfg_t *nm_cfg_get(void)
{
    return &cfg;
}

void nm_cfg_free(void)
{
    nm_str_free(&cfg.vm_dir);
    nm_str_free(&cfg.db_path);
    nm_str_free(&cfg.vnc_bin);
    nm_str_free(&cfg.spice_bin);
    nm_str_free(&cfg.log_path);
    nm_vect_free(&cfg.qemu_targets, NULL);
}

static void nm_generate_cfg(const char *home, const nm_str_t *cfg_path)
{
    struct stat file_info;

    if (stat(cfg_path->data, &file_info) == 0)
        return;

    printf(_("Config file \"%s\" is not found.\n"), cfg_path->data);
    printf(_("You can copy example from:\n"));
    printf(NM_STRING(NM_USR_PREFIX) "/share/nemu/templates/config/nemu.cfg.sample\n");
    printf(_("and edit it manually or let the program generate it.\n\n"));
    printf(_("Generate cfg? (y/n)\n> "));

    switch (getchar()) {
    case 'y':
    case 'Y':
        {
            FILE *cfg;
            char ch;
            nm_str_t db = NM_INIT_STR;
#ifdef NM_WITH_VNC_CLIENT
            nm_str_t vnc_bin = NM_INIT_STR;
            nm_str_t vnc_args = NM_INIT_STR;
#endif
#ifdef NM_WITH_SPICE
            nm_str_t spice_bin = NM_INIT_STR;
            nm_str_t spice_args = NM_INIT_STR;
#endif
            nm_str_t vmdir = NM_INIT_STR;
            nm_str_t targets = NM_INIT_STR;
            int dir_created = 0;

            nm_str_alloc_text(&db, home);
#ifdef NM_WITH_VNC_CLIENT
            nm_str_alloc_text(&vnc_bin, NM_DEFAULT_VNC);
            nm_str_alloc_text(&vnc_args, NM_DEFAULT_VNCARG);
#endif
#ifdef NM_WITH_SPICE
            nm_str_alloc_text(&spice_bin, NM_DEFAULT_SPICE);
            nm_str_alloc_text(&spice_args, NM_DEFAULT_SPICEARG);
#endif
            nm_str_alloc_text(&vmdir, home);
            nm_str_alloc_text(&targets, NM_DEFAULT_TARGET);

            nm_str_add_text(&db, "/." NM_DEFAULT_DBFILE);
            nm_str_add_text(&vmdir, "/" NM_DEFAULT_VMDIR);

            if ((cfg = fopen(cfg_path->data, "w+")) == NULL)
                nm_bug("Cannot create file: %s\n", cfg_path->data);
           
            /* clear stdin */
            while ((ch = getchar()) != '\n' && ch != EOF) {}

            do {
                nm_get_input(_("VM storage directory"), &vmdir);

                if (stat(vmdir.data, &file_info) != 0)
                {
                    if (mkdir(vmdir.data, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) != 0)
                    {
                        printf(_("Cannot create VM storage directory %s: %s\n"),
                            vmdir.data, strerror(errno));
                        printf(_("Try again? (y/n)\n> "));
                        switch (getchar()) {
                        case 'y':
                        case 'Y':
                            /* clear stdin */
                            while ((ch = getchar()) != '\n' && ch != EOF) {}
                            break;
                        default:
                            exit(NM_OK);
                        }
                    }
                    else
                    {
                        dir_created = 1;
                    }
                }
                else
                {
                    dir_created = 1;
                }
            } while (!dir_created);

            nm_get_input(_("VM settings database path"), &db);
#ifdef NM_WITH_VNC_CLIENT
            nm_get_input(_("VNC client path (enter \"/bin/false\" if you connect other way)"), &vnc_bin);
            nm_get_input(_("VNC client arguments"), &vnc_args);
#endif
#ifdef NM_WITH_SPICE
            nm_get_input(_("SPICE client path(enter \"/bin/false\" if you connect other way)"), &spice_bin);
            nm_get_input(_("SPICE client arguments"), &spice_args);
#endif
            nm_get_input(_("QEMU system targets list, separated by comma"), &targets);

            fprintf(cfg, "[main]\n# virtual machine dir.\nvmdir = %s\n\n", vmdir.data);
            fprintf(cfg, "# path to database file.\ndb = %s\n\n", db.data);
            fprintf(cfg, "# override highlight color of running VM's. Example:\n# hl_color = 00afd7\n\n");
            fprintf(cfg, "[viewer]\n");
#ifdef NM_WITH_SPICE
            fprintf(cfg, "# default protocol (1 - spice, 0 - vnc)\nspice_default = 1\n\n");
#else
            fprintf(cfg, "# default protocol (1 - spice, 0 - vnc)\nspice_default = 0\n\n");
#endif
#ifdef NM_WITH_VNC_CLIENT
            fprintf(cfg, "# vnc client path.\nvnc_bin = %s\n\n", vnc_bin.data);
            fprintf(cfg, "# vnc client args (%%t - title, %%p - port)\nvnc_args = %s\n\n", vnc_args.data);
#endif
#ifdef NM_WITH_SPICE
            fprintf(cfg, "# spice client path.\nspice_bin = %s\n\n", spice_bin.data);
            fprintf(cfg, "# spice client args (%%t - title, %%p - port)\nspice_args = %s\n\n", spice_args.data);
#endif
            fprintf(cfg, "# listen for vnc|spice connections"
                " (0 - only localhost, 1 - any address)\nlisten_any = 0\n\n");
            fprintf(cfg, "[qemu]\n# comma separated QEMU system targets installed.\n"
                "targets = %s\n\n", targets.data);
            fprintf(cfg, "# Log last QEMU command.\n"
                "enable_log = 1\n\n");
            fprintf(cfg, "# Log path.\n"
                "log_cmd = /tmp/qemu_last_cmd.log\n");
            fclose(cfg);

            nm_str_free(&db);
#ifdef NM_WITH_VNC_CLIENT
            nm_str_free(&vnc_bin);
            nm_str_free(&vnc_args);
#endif
#ifdef NM_WITH_SPICE
            nm_str_free(&spice_bin);
            nm_str_free(&spice_args);
#endif
            nm_str_free(&vmdir);
            nm_str_free(&targets);
        }
        break;

    default:
        exit(NM_OK);
    }
}

static void nm_get_input(const char *msg, nm_str_t *res)
{
    ssize_t nread;
    nm_str_t buf = NM_INIT_STR;

    printf(_("%s [default: %s]\n> "), msg, res->data);

    if ((nread = getline(&buf.data, &buf.alloc_bytes, stdin)) == -1)
        nm_bug(_("Error get user input data"));

    if (nread > 1)
    {
        if (buf.data[nread - 1] == '\n')
        {
            buf.data[nread - 1] = '\0';
            nread--;
        }

        buf.len = nread;
        nm_str_copy(res, &buf);
    }

    nm_str_free(&buf);
}

static inline void nm_get_param(const void *ini, const char *section,
                                const char *value, nm_str_t *res)
{
    if (nm_ini_parser_find(ini, section, value, res) != NM_OK)
        nm_bug(_("cfg error: %s->%s is missing"), section, value);
    if (res->len == 0)
        nm_bug(_("cfg error: %s->%s is empty"), section, value);
}

static inline int nm_get_opt_param(const void *ini, const char *section,
                                   const char *value, nm_str_t *res)
{
    if (nm_ini_parser_find(ini, section, value, res) != NM_OK)
        return NM_ERR;

    if (res->len == 0)
    {
        nm_bug(_("cfg error: %s->%s is empty"), section, value);
        return NM_ERR;
    }

    return NM_OK;
}

static inline void nm_cfg_get_color(size_t pos, short *color,
                                    const nm_str_t *buf)
{
    nm_str_t hex = NM_INIT_STR;

    assert(color != NULL);

    nm_str_alloc_text(&hex, "0x");
    nm_str_add_char(&hex, buf->data[pos]);
    nm_str_add_char(&hex, buf->data[pos + 1]);
    *color = (nm_str_stoui(&hex, 16)) * 1000 / 255;
    nm_str_free(&hex);
}

#if defined(NM_WITH_VNC_CLIENT) || defined(NM_WITH_SPICE)
static void nm_cfg_get_view(nm_view_args_t *view, const nm_str_t *buf)
{
    int label_found = 0;

    for (size_t n = 0; n < buf->len; n++)
    {
        if (buf->data[n] == '%')
        {
            label_found = 1;
            continue;
        }

        if (label_found && buf->data[n - 1] == '%')
        {
            switch (buf->data[n]) {
            case 't':
                nm_debug("args: got %%t at %zu pos\n", n - 1);
                view->title = n - 1;
                break;
            case 'p':
                nm_debug("args: got %%p at %zu pos\n", n - 1);
                view->port = n - 1;
                break;
            }
            label_found = 0;
        }
    }
}
#endif
/* vim:set ts=4 sw=4 fdm=marker: */
