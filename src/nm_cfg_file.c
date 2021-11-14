#include <nm_core.h>
#include <nm_utils.h>
#include <nm_string.h>
#include <nm_vector.h>
#include <nm_cfg_file.h>
#include <nm_mon_daemon.h>
#include <nm_remote_api.h>
#include <nm_ini_parser.h>

#include <dirent.h>

static const char NM_DEFAULT_PLACEHOLDER[] = "DEFAULT";

#ifndef NM_CFG_NAME
static const char NM_CFG_NAME[]         = ".nemu.cfg";
#endif /* NM_CFG_NAME */

#ifndef NM_DEFAULT_VMDIR
static const char NM_DEFAULT_VMDIR[]    = "nemu_vm";
#endif /* NM_DEFAULT_VMDIR */

#ifndef NM_DEFAULT_VNC
static const char NM_DEFAULT_VNC[]      = "/usr/bin/vncviewer";
#endif /* NM_DEFAULT_VNC */

#ifndef NM_DEFAULT_DBFILE
static const char NM_DEFAULT_DBFILE[]   = ".nemu.db";
#endif /* NM_DEFAULT_DBFILE */

#ifndef NM_DEFAULT_SPICE
static const char NM_DEFAULT_SPICE[]    = "/usr/bin/remote-viewer";
#endif /* NM_DEFAULT_SPICE */

#ifndef NM_DEFAULT_QEMUDIR
static const char NM_DEFAULT_QEMUDIR[]  = "/usr/bin";
#endif /* NM_WITH_QEMU */

static const char NM_DEFAULT_VNCARG[]   = ":%p";
static const char NM_DEFAULT_SPICEARG[] = "--title %t spice://127.0.0.1:%p";

static const char NM_DEFAULT_TARGET[]   = "x86_64,i386";

static const char NM_INI_S_MAIN[]       = "main";
static const char NM_INI_S_VIEW[]       = "viewer";
static const char NM_INI_S_QEMU[]       = "qemu";
static const char NM_INI_S_DMON[]       = "nemu-monitor";

static const char NM_INI_P_VM[]         = "vmdir";
static const char NM_INI_P_DB[]         = "db";
static const char NM_INI_P_HL[]         = "hl_color";
static const char NM_INI_P_CS[]         = "cursor_style";
static const char NM_INI_P_DEBUG_PATH[] = "debug_path";
static const char NM_INI_P_PROT[]       = "spice_default";
static const char NM_INI_P_VBIN[]       = "vnc_bin";
static const char NM_INI_P_VARG[]       = "vnc_args";
static const char NM_INI_P_SBIN[]       = "spice_bin";
static const char NM_INI_P_SARG[]       = "spice_args";
static const char NM_INI_P_VANY[]       = "listen_any";
static const char NM_INI_P_QEMUDIR[]    = "qemu_bin_path";
static const char NM_INI_P_QTRG[]       = "targets";
static const char NM_INI_P_QENL[]       = "enable_log";
static const char NM_INI_P_QLOG[]       = "log_cmd";
static const char NM_INI_P_PID[]        = "pid";
static const char NM_INI_P_AUTO[]       = "autostart";
static const char NM_INI_P_SLP[]        = "sleep";
static const char NM_INI_P_GL_SEP[]     = "glyph_separator";
static const char NM_INI_P_GL_CHECK[]   = "glyph_checkbox";
#if defined (NM_WITH_REMOTE)
static const char NM_INI_P_API_SRV[]    = "remote_control";
static const char NM_INI_P_API_IFACE[]  = "remote_interface";
static const char NM_INI_P_API_PORT[]   = "remote_port";
static const char NM_INI_P_API_CERT[]   = "remote_tls_cert";
static const char NM_INI_P_API_KEY[]    = "remote_tls_key";
static const char NM_INI_P_API_SALT[]   = "remote_salt";
static const char NM_INI_P_API_HASH[]   = "remote_hash";
#endif
#if defined (NM_WITH_DBUS)
static const char NM_INI_P_DYES[]       = "dbus_enabled";
static const char NM_INI_P_DTMT[]       = "dbus_timeout";
#endif

static const char * const CURSOR_STYLE_STR[]   = {
    "Default",
    "Blink Block",
    "Steady Block",
    "Blink Underline",
    "Steady Underline",
    "Blinking Bar",
    "Steady Bar"
};

static nm_cfg_t cfg;

static void nm_generate_cfg(const char *home, const nm_str_t *cfg_path);
static void nm_get_input(const char *msg, nm_str_t *res);
static inline void nm_get_param(const void *ini, const char *section,
                                const char *value, nm_str_t *res, const char *default_value);
static inline int nm_get_opt_param(const void *ini, const char *section,
                                   const char *value, nm_str_t *res);
static inline void nm_cfg_get_color(size_t pos, short *color,
                                    const nm_str_t *buf);
static void nm_cfg_get_targets(nm_str_t *buf, const nm_str_t *path);
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

    nm_str_format(&cfg_path, "%s/%s", pw->pw_dir, NM_CFG_NAME);

    nm_generate_cfg(pw->pw_dir, &cfg_path);
    ini = nm_ini_parser_init(&cfg_path);

    /* Get debug file path */
    if (nm_get_opt_param(ini, NM_INI_S_MAIN, NM_INI_P_DEBUG_PATH, &cfg.debug_path) == NM_OK) {
        FILE *fp;
        if ((fp = fopen(cfg.debug_path.data, "a+")) == NULL)
            nm_bug(_("cfg: no write access to %s"), cfg.debug_path.data);
        fclose(fp);

        cfg.debug = 1;
        nm_ini_parser_dump(ini);
    }

    /* Get VM storage directory path */
    nm_str_format(&tmp_buf, "%s/%s", pw->pw_dir, NM_DEFAULT_VMDIR);
    nm_get_param(ini, NM_INI_S_MAIN, NM_INI_P_VM, &cfg.vm_dir, tmp_buf.data);
    nm_str_trunc(&tmp_buf, 0);

    if (stat(cfg.vm_dir.data, &file_info) == -1)
        nm_bug("cfg: %s: %s", cfg.vm_dir.data, strerror(errno));
    if (!S_ISDIR(file_info.st_mode))
        nm_bug(_("cfg: %s is not a directory"), cfg.vm_dir.data);
    if (access(cfg.vm_dir.data, W_OK) != 0)
        nm_bug(_("cfg: no write access to %s"), cfg.vm_dir.data);

    /* Get database file path */
    nm_str_format(&tmp_buf, "%s/%s", pw->pw_dir, NM_DEFAULT_DBFILE);
    nm_get_param(ini, NM_INI_S_MAIN, NM_INI_P_DB, &cfg.db_path, tmp_buf.data);
    nm_str_trunc(&tmp_buf, 0);

    nm_str_dirname(&cfg.db_path, &tmp_buf);
    if (access(tmp_buf.data, W_OK) != 0)
        nm_bug(_("cfg: no write access to %s"), tmp_buf.data);
    nm_str_trunc(&tmp_buf, 0);

#ifdef NM_WITH_VNC_CLIENT
    /* Get the VNC client binary path */
    nm_get_param(ini, NM_INI_S_VIEW, NM_INI_P_VBIN, &cfg.vnc_bin, NM_DEFAULT_VNC);

    if (stat(cfg.vnc_bin.data, &file_info) == -1)
        nm_bug("cfg: %s: %s", cfg.vnc_bin.data, strerror(errno));

    nm_get_param(ini, NM_INI_S_VIEW, NM_INI_P_VARG, &cfg.vnc_args, NM_DEFAULT_VNCARG);
    nm_cfg_get_view(&cfg.vnc_view, &cfg.vnc_args);
#endif /* NM_WITH_VNC_CLIENT */
#ifdef NM_WITH_SPICE
    /* Get the SPICE client binary path */
    nm_get_param(ini, NM_INI_S_VIEW, NM_INI_P_SBIN, &cfg.spice_bin, NM_DEFAULT_SPICE);

    if (stat(cfg.spice_bin.data, &file_info) == -1)
        nm_bug("cfg: %s: %s", cfg.spice_bin.data, strerror(errno));

    nm_get_param(ini, NM_INI_S_VIEW, NM_INI_P_SARG, &cfg.spice_args, NM_DEFAULT_SPICEARG);
    nm_cfg_get_view(&cfg.spice_view, &cfg.spice_args);

    nm_get_param(ini, NM_INI_S_VIEW, NM_INI_P_PROT, &tmp_buf, NULL);
    cfg.spice_default = !!nm_str_stoui(&tmp_buf, 10);
    nm_str_trunc(&tmp_buf, 0);
#endif /* NM_WITH_SPICE */
    /* Get the VNC listen value */
    nm_get_param(ini, NM_INI_S_VIEW, NM_INI_P_VANY, &tmp_buf, NULL);
    cfg.listen_any = !!nm_str_stoui(&tmp_buf, 10);
    nm_str_trunc(&tmp_buf, 0);

    /* Get QEMU bin directory path */
    nm_get_param(ini, NM_INI_S_QEMU, NM_INI_P_QEMUDIR, &cfg.qemu_bin_path, NM_DEFAULT_QEMUDIR);

    if (stat(cfg.qemu_bin_path.data, &file_info) == -1)
        nm_bug("cfg: %s: %s", cfg.qemu_bin_path.data, strerror(errno));
    if (!S_ISDIR(file_info.st_mode))
        nm_bug(_("cfg: %s is not a directory"), cfg.qemu_bin_path.data);
    if (access(cfg.qemu_bin_path.data, R_OK | X_OK) != 0)
        nm_bug(_("cfg: no read + exec access to %s"), cfg.qemu_bin_path.data);

    /* Check for qemu-img */
    nm_str_format(&tmp_buf, "%s/qemu-img", cfg.qemu_bin_path.data);
    if (stat(tmp_buf.data, &file_info) == -1)
        nm_bug("cfg: %s: %s", tmp_buf.data, strerror(errno));
    if (access(tmp_buf.data, R_OK | X_OK) != 0)
        nm_bug(_("cfg: no read + exec access to %s"), tmp_buf.data);
    nm_str_trunc(&tmp_buf, 0);

    /* Get QEMU targets list */
    nm_get_param(ini, NM_INI_S_QEMU, NM_INI_P_QTRG, &tmp_buf, NM_DEFAULT_TARGET);
    {
        char *token;
        char *saveptr = tmp_buf.data;
        nm_str_t qemu_bin = NM_INIT_STR;

        while ((token = strtok_r(saveptr, ",", &saveptr))) {
            nm_str_format(&qemu_bin, "%s/qemu-system-%s",
                cfg.qemu_bin_path.data, token);

            if (stat(qemu_bin.data, &file_info) == -1)
                nm_bug(_("cfg: %s: %s"), qemu_bin.data, strerror(errno));

            nm_vect_insert(&cfg.qemu_targets, token, strlen(token) + 1, NULL);
        }

        nm_vect_end_zero(&cfg.qemu_targets); /* need for ncurses form */
        nm_str_trunc(&tmp_buf, 0);
        nm_str_free(&qemu_bin);

        if (cfg.debug) {
            const char **tp = (const char **) cfg.qemu_targets.data;

            nm_debug("\nConfigured QEMU targets:\n");
            for (; *tp != NULL; tp++)
                nm_debug(">> %s\n", *tp);
        }
    }

    /* Get log enable flag */
    nm_get_param(ini, NM_INI_S_QEMU, NM_INI_P_QENL, &tmp_buf, NULL);
    cfg.log_enabled = !!nm_str_stoui(&tmp_buf, 10);
    nm_str_trunc(&tmp_buf, 0);

    if (cfg.log_enabled) {
        /* Get file log path */
        nm_get_param(ini, NM_INI_S_QEMU, NM_INI_P_QLOG, &cfg.log_path, NULL);

        nm_str_dirname(&cfg.log_path, &tmp_buf);
        if (access(tmp_buf.data, W_OK) != 0)
            nm_bug(_("cfg: no write access to %s"), tmp_buf.data);
    }

    nm_str_trunc(&tmp_buf, 0);
    if (nm_get_opt_param(ini, NM_INI_S_MAIN, NM_INI_P_HL, &tmp_buf) == NM_OK) {
        if (tmp_buf.len != 6)
            nm_bug(_("cfg: incorrect color value %s, example:5fafff"), tmp_buf.data);

        nm_cfg_get_color(0, &cfg.hl_color.r, &tmp_buf);
        nm_cfg_get_color(2, &cfg.hl_color.g, &tmp_buf);
        nm_cfg_get_color(4, &cfg.hl_color.b, &tmp_buf);

        cfg.hl_is_set = 1;
        nm_debug("HL color: r:%d g:%d b:%d\n",
                cfg.hl_color.r, cfg.hl_color.g, cfg.hl_color.b);
    }
    nm_str_trunc(&tmp_buf, 0);
    if (nm_get_opt_param(ini, NM_INI_S_MAIN, NM_INI_P_CS, &tmp_buf) == NM_OK) {
        if (tmp_buf.len != 1)
            nm_bug(_("cfg: incorrect cursor style value %s, example:1"), tmp_buf.data);

        cfg.cursor_style = nm_str_stoui(&tmp_buf, 10);
        if (cfg.cursor_style >= nm_arr_len(CURSOR_STYLE_STR))
            nm_bug(_("cfg: incorrect cursor style value %d"), cfg.cursor_style);

        nm_debug("Cursor style: %s (%d)\n",
            CURSOR_STYLE_STR[cfg.cursor_style], cfg.cursor_style);
    } else {
        cfg.cursor_style = 0;
    }
    nm_str_trunc(&tmp_buf, 0);
    if (nm_get_opt_param(ini, NM_INI_S_DMON, NM_INI_P_AUTO, &tmp_buf) == NM_OK) {
        cfg.start_daemon = !!nm_str_stoui(&tmp_buf, 10);
    } else {
        cfg.start_daemon = 0;
    }

    nm_get_param(ini, NM_INI_S_DMON, NM_INI_P_PID, &cfg.daemon_pid, NULL);
    nm_str_trunc(&tmp_buf, 0);
    if (nm_get_opt_param(ini, NM_INI_S_DMON, NM_INI_P_SLP, &tmp_buf) == NM_OK) {
        cfg.daemon_sleep = nm_str_stoul(&tmp_buf, 10);
    } else {
        cfg.daemon_sleep = NM_MON_SLEEP;
    }

    nm_str_trunc(&tmp_buf, 0);
    if (nm_get_opt_param(ini, NM_INI_S_MAIN, NM_INI_P_GL_SEP, &tmp_buf) == NM_OK) {
        cfg.glyphs.separator = !!nm_str_stoui(&tmp_buf, 10);
    }
    nm_str_trunc(&tmp_buf, 0);
    if (nm_get_opt_param(ini, NM_INI_S_MAIN, NM_INI_P_GL_CHECK, &tmp_buf) == NM_OK) {
        cfg.glyphs.checkbox = !!nm_str_stoui(&tmp_buf, 10);
    }

#if defined (NM_WITH_REMOTE)
    nm_str_trunc(&tmp_buf, 0);
    cfg.api_server = 0;
    if (nm_get_opt_param(ini, NM_INI_S_DMON, NM_INI_P_API_SRV, &tmp_buf) == NM_OK) {
        cfg.api_server = !!nm_str_stoui(&tmp_buf, 10);
    }

    nm_str_trunc(&tmp_buf, 0);
    cfg.api_port = NM_API_PORT;
    if (nm_get_opt_param(ini, NM_INI_S_DMON, NM_INI_P_API_PORT, &tmp_buf) == NM_OK) {
        cfg.api_port = nm_str_stoul(&tmp_buf, 10);
    }

    if (cfg.api_server) {
        nm_get_opt_param(ini, NM_INI_S_DMON, NM_INI_P_API_IFACE, &cfg.api_iface);
        nm_get_param(ini, NM_INI_S_DMON, NM_INI_P_API_CERT, &cfg.api_cert_path, NULL);
        nm_get_param(ini, NM_INI_S_DMON, NM_INI_P_API_KEY, &cfg.api_key_path, NULL);
        nm_get_param(ini, NM_INI_S_DMON, NM_INI_P_API_SALT, &cfg.api_salt, NULL);
        nm_get_param(ini, NM_INI_S_DMON, NM_INI_P_API_HASH, &cfg.api_hash, NULL);
    }
#endif

#if defined (NM_WITH_DBUS)
    nm_str_trunc(&tmp_buf, 0);
    if (nm_get_opt_param(ini, NM_INI_S_DMON, NM_INI_P_DYES, &tmp_buf) == NM_OK) {
        cfg.dbus_enabled = !!nm_str_stoui(&tmp_buf, 10);
    } else {
        cfg.dbus_enabled = 0;
    }

    nm_str_trunc(&tmp_buf, 0);
    if (nm_get_opt_param(ini, NM_INI_S_DMON, NM_INI_P_DTMT, &tmp_buf) == NM_OK) {
        cfg.dbus_timeout = nm_str_stol(&tmp_buf, 10);
        if (cfg.dbus_timeout > INT32_MAX || cfg.dbus_timeout < INT32_MIN)
            nm_bug(_("cfg: incorrect D-Bus timeout value %ld"), cfg.dbus_timeout);
    } else {
        cfg.dbus_timeout = -1;
    }
#endif
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
    nm_str_free(&cfg.debug_path);
#if defined(NM_WITH_VNC_CLIENT) || defined(NM_WITH_SPICE)
    nm_str_free(&cfg.vnc_bin);
    nm_str_free(&cfg.spice_bin);
    nm_str_free(&cfg.vnc_args);
    nm_str_free(&cfg.spice_args);
#endif
    nm_str_free(&cfg.log_path);
    nm_str_free(&cfg.daemon_pid);
    nm_str_free(&cfg.qemu_bin_path);
    nm_vect_free(&cfg.qemu_targets, NULL);
#if defined (NM_WITH_REMOTE)
    nm_str_free(&cfg.api_cert_path);
    nm_str_free(&cfg.api_key_path);
    nm_str_free(&cfg.api_salt);
    nm_str_free(&cfg.api_hash);
    nm_str_free(&cfg.api_iface);
#endif
}

static void nm_generate_cfg(const char *home, const nm_str_t *cfg_path)
{
    struct stat file_info;

    if (stat(cfg_path->data, &file_info) == 0)
        return;

    printf(_("Config file \"%s\" is not found.\n"), cfg_path->data);
    printf(_("You can copy example from:\n"));
    printf("%s/nemu/templates/config/nemu.cfg.sample\n", NM_FULL_DATAROOTDIR);
    printf(_("and edit it manually or let the program generate it.\n\n"));
    printf(_("Generate cfg? (y/n)\n> "));

    switch (getchar()) {
    case 'y':
    case 'Y':
        {
            nm_str_t tmp_dir_path = NM_INIT_STR;
            FILE *cfg_file;
            int ch;
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
            nm_str_t qemu_bin_path = NM_INIT_STR;
            nm_str_t targets = NM_INIT_STR;
            int dir_created = 0;

            if (cfg_path->data[cfg_path->len - 1] == '/')
                nm_bug(_("%s: config filepath \"%s\" ends with /"), __func__, cfg_path->data);
            nm_str_dirname(cfg_path, &tmp_dir_path);
            nm_mkdir_parent(&tmp_dir_path, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);

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
            nm_str_alloc_text(&qemu_bin_path, NM_DEFAULT_QEMUDIR);

            nm_str_append_format(&db, "/%s", NM_DEFAULT_DBFILE);
            nm_str_append_format(&vmdir, "/%s", NM_DEFAULT_VMDIR);

            if ((cfg_file = fopen(cfg_path->data, "w+")) == NULL)
                nm_bug("Cannot create file: %s\n", cfg_path->data);

            /* clear stdin */
            while ((ch = getchar()) != '\n' && ch != EOF) {}

            do {
                nm_get_input(_("VM storage directory"), &vmdir);

                if (stat(vmdir.data, &file_info) != 0) {
                    if (nm_mkdir_parent(&vmdir, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) != 0) {
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
                            nm_exit(NM_OK);
                        }
                    } else {
                        dir_created = 1;
                    }
                } else {
                    dir_created = 1;
                }
            } while (!dir_created);

            nm_get_input(_("VM settings database path"), &db);
            if (db.data[db.len - 1] == '/')
                nm_bug(_("%s: database filepath \"%s\" ends with /"), __func__, db.data);
            nm_str_dirname(&db, &tmp_dir_path);
            nm_mkdir_parent(&tmp_dir_path, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
#ifdef NM_WITH_VNC_CLIENT
            nm_get_input(_("VNC client path (enter \"/bin/false\" if you connect other way)"), &vnc_bin);
            nm_get_input(_("VNC client arguments"), &vnc_args);
#endif
#ifdef NM_WITH_SPICE
            nm_get_input(_("SPICE client path(enter \"/bin/false\" if you connect other way)"), &spice_bin);
            nm_get_input(_("SPICE client arguments"), &spice_args);
#endif
            nm_get_input(_("Path to directory, where QEMU binary can be found"), &qemu_bin_path);
            nm_cfg_get_targets(&targets, &qemu_bin_path);
            nm_get_input(_("QEMU system targets list, separated by comma"), &targets);

            fprintf(cfg_file, "[main]\n# virtual machine dir.\nvmdir = %s\n\n", vmdir.data);
            fprintf(cfg_file, "# path to database file.\ndb = %s\n\n", db.data);
            fprintf(cfg_file, "# path to debug log file. Example:\n# debug_path = /tmp/nemu_debug.log\n\n");
            fprintf(cfg_file, "# override highlight color of running VM's. Example:\n# hl_color = 00afd7\n\n");
            fprintf(cfg_file, "# glyph_checkbox = 1\n# glyph_separator = 0\n\n");
            fprintf(cfg_file,
                "# change cursor style for nemu.\n"
                "# see https://terminalguide.namepad.de/seq/csi_sq_t_space/\n"
                "# if not set VTE's default cursor style will be used\n"
                "# cursor_style = 1\n\n");
            fprintf(cfg_file, "[viewer]\n");
#ifdef NM_WITH_SPICE
            fprintf(cfg_file, "# default protocol (1 - spice, 0 - vnc)\nspice_default = 1\n\n");
#else
            fprintf(cfg_file, "# default protocol (1 - spice, 0 - vnc)\nspice_default = 0\n\n");
#endif
#ifdef NM_WITH_VNC_CLIENT
            fprintf(cfg_file, "# vnc client path.\nvnc_bin = %s\n\n", vnc_bin.data);
            fprintf(cfg_file, "# vnc client args (%%t - title, %%p - port)\nvnc_args = %s\n\n", vnc_args.data);
#endif
#ifdef NM_WITH_SPICE
            fprintf(cfg_file, "# spice client path.\nspice_bin = %s\n\n", spice_bin.data);
            fprintf(cfg_file, "# spice client args (%%t - title, %%p - port)\nspice_args = %s\n\n", spice_args.data);
#endif
            fprintf(cfg_file, "# listen for vnc|spice connections"
                " (0 - only localhost, 1 - any address)\nlisten_any = 0\n\n");
            fprintf(cfg_file, "[qemu]\n# path to directory, where QEMU binary can be found.\n"
                "qemu_bin_path = %s\n\n", qemu_bin_path.data);
            fprintf(cfg_file, "# comma separated QEMU system targets installed.\n"
                "targets = %s\n\n", targets.data);
            fprintf(cfg_file, "# Log last QEMU command.\n"
                "enable_log = 1\n\n");
            fprintf(cfg_file, "# Log path.\n"
                "log_cmd = /tmp/qemu_last_cmd.log\n\n");
            fprintf(cfg_file, "[nemu-monitor]\n"
                    "# Auto start monitoring daemon\nautostart = 1\n\n"
                    "# Daemon sleep interval (ms) (default: 1000)\n#sleep = 1000\n\n"
                    "# Monitoring daemon pid file\npid = /tmp/nemu-monitor.pid"
#ifdef NM_WITH_DBUS
                    "\n\n# Enable D-Bus feature\ndbus_enabled = 1\n\n"
                    "# Message timeout (ms)\ndbus_timeout = 2000"
#endif
                    "\n");
#ifdef NM_WITH_REMOTE
            fprintf(cfg_file, "\n# Enable remote control (default: disabled)\n"
                "#remote_control = 0\n\n");
            fprintf(cfg_file, "# Remote control listen interface (default: any)\n"
                "#remote_interface = eth0\n\n");
            fprintf(cfg_file, "# Remote control port (default: %d)\n"
                "#remote_port = %d\n\n", NM_API_PORT, NM_API_PORT);
            fprintf(cfg_file, "# Remote control public certificate path\n"
                "#remote_tls_cert = /path\n\n");
            fprintf(cfg_file, "# Remote control private key path\n"
                "#remote_tls_key = /path\n\n");
            fprintf(cfg_file, "# Remote control password salt\n"
                "#remote_salt = salt\n\n");
            fprintf(cfg_file, "# Remote control \"password+salt\" hash (sha256)\n"
                "#remote_hash = hash\n");
#endif
            fclose(cfg_file);

            nm_str_free(&tmp_dir_path);
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
            nm_str_free(&qemu_bin_path);
            nm_str_free(&targets);
        }
        break;

    default:
        nm_exit(NM_OK);
    }
}

static void nm_get_input(const char *msg, nm_str_t *res)
{
    ssize_t nread;
    nm_str_t buf = NM_INIT_STR;

    printf(_("%s [default: %s]\n> "), msg, res->data);

    if ((nread = getline(&buf.data, &buf.alloc_bytes, stdin)) == -1)
        nm_bug(_("Error get user input data"));

    if (nread > 1) {
        if (buf.data[nread - 1] == '\n') {
            buf.data[nread - 1] = '\0';
            nread--;
        }

        buf.len = nread;
        nm_str_copy(res, &buf);
    }

    nm_str_free(&buf);
}

static inline void nm_get_param(const void *ini, const char *section,
                                const char *value, nm_str_t *res, const char *default_value)
{
    if (nm_ini_parser_find(ini, section, value, res) != NM_OK)
        nm_bug(_("cfg error: %s->%s is missing"), section, value);
    if (res->len == 0)
        nm_bug(_("cfg error: %s->%s is empty"), section, value);

    if (default_value && nm_str_cmp_st(res, NM_DEFAULT_PLACEHOLDER) == NM_OK)
        nm_str_alloc_text(res, default_value);
}

static inline int nm_get_opt_param(const void *ini, const char *section,
                                   const char *value, nm_str_t *res)
{
    if (nm_ini_parser_find(ini, section, value, res) != NM_OK)
        return NM_ERR;

    if (res->len == 0) {
        nm_bug(_("cfg error: %s->%s is empty"), section, value);
        return NM_ERR;
    }

    return NM_OK;
}

static inline void nm_cfg_get_color(size_t pos, short *color,
                                    const nm_str_t *buf)
{
    if(color == NULL)
        return;

    nm_str_t hex = NM_INIT_STR;

    nm_str_alloc_text(&hex, "0x");
    nm_str_add_char(&hex, buf->data[pos]);
    nm_str_add_char(&hex, buf->data[pos + 1]);
    //@TODO This uint32_t to short int conversion kinda scares me
    *color = (nm_str_stoui(&hex, 16)) * 1000 / 255;
    nm_str_free(&hex);
}

static void nm_cfg_get_targets(nm_str_t *buf, const nm_str_t *path)
{
    const char qemu_bin_prefix[] = "qemu-system-";
    DIR *d = opendir(path->data);

    if (d) {
        struct dirent *dir;

        nm_str_free(buf);
        while ((dir = readdir(d)) != NULL) {
            if (strstr(dir->d_name, qemu_bin_prefix) == dir->d_name)
                nm_str_append_format(buf, "%s%s",
                    buf->len ? "," : "",
                    dir->d_name + nm_arr_len(qemu_bin_prefix) - 1);
        }
        closedir(d);
    }

    if (!buf->len)
        nm_str_format(buf, "%s", NM_DEFAULT_TARGET);
}

#if defined(NM_WITH_VNC_CLIENT) || defined(NM_WITH_SPICE)
static void nm_cfg_get_view(nm_view_args_t *view, const nm_str_t *buf)
{
    int label_found = 0;

    for (size_t n = 0; n < buf->len; n++) {
        if (buf->data[n] == '%') {
            label_found = 1;
            continue;
        }

        if (label_found && buf->data[n - 1] == '%') {
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
/* vim:set ts=4 sw=4: */
