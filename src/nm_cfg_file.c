#include <nm_core.h>
#include <nm_utils.h>
#include <nm_string.h>
#include <nm_vector.h>
#include <nm_cfg_file.h>
#include <nm_ini_parser.h>

#define NM_CFG_NAME       "nemu.cfg"
#define NM_DEFAULT_VMDIR  "nemu_vm"
#define NM_DEFAULT_DBFILE "nemu.db"
#define NM_DEFAULT_VNC    "/usr/bin/vncviewer"
#define NM_DEFAULT_TARGET "x86_64,i386"

#define NM_INI_S_MAIN     "main"
#define NM_INI_S_VNC      "vnc"
#define NM_INI_S_QEMU     "qemu"

#define NM_INI_P_VM       "vmdir"
#define NM_INI_P_DB       "db"
#define NM_INI_P_LIST     "list_max"
#define NM_INI_P_VBIN     "binary"
#define NM_INI_P_VANY     "listen_any"
#define NM_INI_P_QBIN     "qemu_system_path"
#define NM_INI_P_QTRG     "targets"
#define NM_INI_P_QENL     "enable_log"
#define NM_INI_P_QLOG     "log_cmd"

static nm_cfg_t cfg;

static void nm_generate_cfg(const char *home, const nm_str_t *cfg_path);
static void nm_get_input(const char *msg, nm_str_t *res);
static inline void nm_get_param(const void *ini, const char *section,
                                const char *value, nm_str_t *res);

void nm_cfg_init(void)
{
    struct stat file_info;
    struct passwd *pw = getpwuid(getuid());
    nm_str_t cfg_path = NM_INIT_STR;
    nm_str_t tmp_buf = NM_INIT_STR;
    nm_ini_node_t *ini;

    if (!pw)
        nm_bug(_("Error get home directory: %s\n"), strerror(errno));

    nm_str_alloc_text(&cfg_path, pw->pw_dir);
    nm_str_add_text(&cfg_path, "/." NM_CFG_NAME);

    nm_generate_cfg(pw->pw_dir, &cfg_path);
    ini = nm_ini_parser_init(&cfg_path);
#if (NM_DEBUG)
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

    /* Get the size of the displayed list of VMs */
    nm_get_param(ini, NM_INI_S_MAIN, NM_INI_P_LIST, &tmp_buf);

    cfg.list_max = nm_str_stoui(&tmp_buf);
    nm_str_trunc(&tmp_buf, 0);
    if ((cfg.list_max == 0) || (cfg.list_max > 100))
    {
        nm_bug(_("cfg: bad list_max value: %u, expected: [1-100]"),
            cfg.list_max);
    }
#if (NM_WITH_VNC_CLIENT)
    /* Get the VNC client binary path */
    nm_get_param(ini, NM_INI_S_VNC, NM_INI_P_VBIN, &cfg.vnc_bin);

    if (stat(cfg.vnc_bin.data, &file_info) == -1)
        nm_bug("cfg: %s: %s", cfg.vnc_bin.data, strerror(errno));
#endif
    /* Get the VNC listen value */
    nm_get_param(ini, NM_INI_S_VNC, NM_INI_P_VANY, &tmp_buf);
    cfg.vnc_listen_any = !!nm_str_stoui(&tmp_buf);
    nm_str_trunc(&tmp_buf, 0);

    /* Get QEMU binary path */
    nm_get_param(ini, NM_INI_S_QEMU, NM_INI_P_QBIN, &cfg.qemu_system_path);

    /* Get QEMU targets list */
    nm_get_param(ini, NM_INI_S_QEMU, NM_INI_P_QTRG, &tmp_buf);
    {
        char *token;
        char *saveptr = tmp_buf.data;
        nm_str_t qemu_bin = NM_INIT_STR;

        while ((token = strtok_r(saveptr, ",", &saveptr)))
        {
            nm_str_copy(&qemu_bin, &cfg.qemu_system_path);
            nm_str_add_char(&qemu_bin, '-');
            nm_str_add_text(&qemu_bin, token);

            if (stat(qemu_bin.data, &file_info) == -1)
                nm_bug(_("cfg: %s: %s"), qemu_bin.data, strerror(errno));

            nm_vect_insert(&cfg.qemu_targets, token, strlen(token) + 1, NULL);
        }

        nm_vect_end_zero(&cfg.qemu_targets); /* need for ncurses form */
        nm_str_trunc(&tmp_buf, 0);
        nm_str_free(&qemu_bin);
#if (NM_DEBUG)
        const char **tp = (const char **) cfg.qemu_targets.data;

        nm_debug("\nConfigured QEMU targets:\n");
        for (; *tp != NULL; tp++)
            nm_debug(">> %s\n", *tp);
#endif
    }

    /* Get log enable flag */
    nm_get_param(ini, NM_INI_S_QEMU, NM_INI_P_QENL, &tmp_buf);
    cfg.log_enabled = !!nm_str_stoui(&tmp_buf);
    nm_str_trunc(&tmp_buf, 0);

    if (cfg.log_enabled)
    {
        /* Get file log path */
        nm_get_param(ini, NM_INI_S_QEMU, NM_INI_P_QLOG, &cfg.log_path);

        nm_str_dirname(&cfg.log_path, &tmp_buf);
        if (access(tmp_buf.data, W_OK) != 0)
            nm_bug(_("cfg: no write access to %s"), tmp_buf.data);
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
    nm_str_free(&cfg.qemu_system_path);
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
            nm_str_t vnc = NM_INIT_STR;
            nm_str_t vmdir = NM_INIT_STR;
            nm_str_t targets = NM_INIT_STR;
            int dir_created = 0;

            nm_str_alloc_text(&db, home);
            nm_str_alloc_text(&vnc, NM_DEFAULT_VNC);
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
            nm_get_input(_("Path to VNC client (enter \"/bin/false\" if you connect other way)"), &vnc);
            nm_get_input(_("QEMU system targets list, separated by comma"), &targets);

            fprintf(cfg, "[main]\n# VM storage directory\nvmdir = %s\n\n", vmdir.data);
            fprintf(cfg, "# VM settings database path\ndb = %s\n\n", db.data);
            fprintf(cfg, "# maximum guests numbers in list.\nlist_max = 10\n\n");
            fprintf(cfg, "[vnc]\n# Path to VNC client\nbinary = %s\n\n", vnc.data);
            fprintf(cfg, "# listen for vnc connections"
                " (0 = only localhost, 1 = any address).\nlisten_any = 0\n\n");
            fprintf(cfg, "[qemu]\n# path to qemu system binary without arch prefix.\n"
                "qemu_system_path = /usr/bin/qemu-system\n\n");
            fprintf(cfg, "# Qemu system targets list, separated by comma.\n"
                "targets = %s\n\n", targets.data);
            fprintf(cfg, "# Log last QEMU command.\n"
                "enable_log = 1\n\n");
            fprintf(cfg, "# Log path.\n"
                "log_cmd = /tmp/qemu_last_cmd.log\n");
            fclose(cfg);

            nm_str_free(&db);
            nm_str_free(&vnc);
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

/* vim:set ts=4 sw=4 fdm=marker: */
