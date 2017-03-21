#include <nm_core.h>
#include <nm_utils.h>
#include <nm_string.h>
#include <nm_cfg_file.h>
#include <nm_ini_parser.h>

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
    nm_get_param(ini, "main", "vmdir", &cfg.vm_dir);
    if (stat(cfg.vm_dir.data, &file_info) == -1)
        nm_bug("cfg: %s: %s", cfg.vm_dir.data, strerror(errno));
    if ((file_info.st_mode & S_IFMT) != S_IFDIR)
        nm_bug(_("cfg: %s is not a directory"), cfg.vm_dir.data);
    if (access(cfg.vm_dir.data, W_OK) != 0)
        nm_bug(_("cfg: no write access to %s"), cfg.vm_dir.data);

    nm_get_param(ini, "main", "db", &cfg.db_path);
    nm_str_copy(&tmp_buf, &cfg.db_path);
    if (access(dirname(tmp_buf.data), W_OK) != 0)
        nm_bug(_("cfg: no write access to %s"), tmp_buf.data);
    nm_str_trunc(&tmp_buf, 0);

    nm_get_param(ini, "main", "list_max", &tmp_buf);
    cfg.list_max = nm_str_stoui(&tmp_buf);
    if ((cfg.list_max == 0) || (cfg.list_max > 100))
    {
        nm_bug(_("cfg: bad list_max value: %u, expected: [1-100]"),
            cfg.list_max);
    }

    nm_ini_parser_free(ini);
    nm_cfg_free(); /* XXX tmp */
    
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
                        dir_created = true;
                    }
                }
                else
                {
                    dir_created = true;
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
            fprintf(cfg, "# Log last qemu command. Empty value disable logging.\n"
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
