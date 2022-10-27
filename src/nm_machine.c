#include <nm_core.h>
#include <nm_utils.h>
#include <nm_string.h>
#include <nm_vector.h>
#include <nm_window.h>
#include <nm_machine.h>
#include <nm_cfg_file.h>

#include <sys/wait.h>

static inline nm_str_t *nm_mach_arch(const nm_mach_t *p)
{
    return (nm_str_t *)&p->arch;
}
static inline nm_str_t *nm_mach_def(const nm_mach_t *p)
{
    return (nm_str_t *)&p->def;
}
static inline nm_vect_t **nm_mach_list(const nm_mach_t *p)
{
    return (nm_vect_t **)&p->list;
}

static nm_vect_t nm_machs = NM_INIT_VECT;

static void nm_mach_init(void);
static void nm_mach_get_data(const char *arch);
static nm_vect_t *nm_mach_parse(const nm_str_t *buf, nm_str_t *def);

static void nm_mach_init(void)
{
    const nm_vect_t *archs = &nm_cfg_get()->qemu_targets;

    for (size_t n = 0; n < archs->n_memb; n++) {
        nm_mach_get_data(((char **) archs->data)[n]);
    }

    if (nm_cfg_get()->debug) {
        nm_debug("\n");
        for (size_t n = 0; n < nm_machs.n_memb; n++) {
            nm_vect_t *v = *nm_mach_list(nm_machs.data[n]);

            nm_debug("Get machine list for %s (default: %s)\n",
                    nm_mach_arch(nm_machs.data[n])->data,
                    nm_mach_def(nm_machs.data[n])->data);

            for (size_t m = 0; m < v->n_memb; m++) {
                nm_debug(">> mach: %s\n", (char *) v->data[m]);
            }
        }
    }
}

const char **nm_mach_get(const nm_str_t *arch)
{
    const char **v = NULL;

    if (nm_machs.data == NULL) {
        nm_mach_init();
    }

    for (size_t n = 0; n < nm_machs.n_memb; n++) {
        if (nm_str_cmp_ss(nm_machs.data[n], arch) == NM_OK) {
            v = (const char **) (*nm_mach_list(nm_machs.data[n]))->data;
            break;
        }
    }

    return v;
}

const char *nm_mach_get_default(const nm_str_t *arch)
{
    const char *def = NULL;

    if (nm_machs.data == NULL) {
        nm_mach_init();
    }

    for (size_t n = 0; n < nm_machs.n_memb; n++) {
        if (nm_str_cmp_ss(nm_machs.data[n], arch) == NM_OK) {
            def = nm_mach_def(nm_machs.data[n])->data;
            break;
        }
    }

    return def;
}

void nm_mach_free(void)
{
    for (size_t n = 0; n < nm_machs.n_memb; n++) {
        nm_vect_free(*nm_mach_list(nm_machs.data[n]), NULL);
    }

    nm_vect_free(&nm_machs, nm_mach_vect_free_mlist_cb);
}

static void nm_mach_get_data(const char *arch)
{
    nm_str_t buf = NM_INIT_STR;
    nm_vect_t argv = NM_INIT_VECT;
    nm_str_t answer = NM_INIT_STR;
    nm_mach_t mach_list = NM_INIT_MLIST;

    nm_str_alloc_text(&mach_list.arch, arch);

    nm_str_format(&buf, "%s/qemu-system-%s",
        nm_cfg_get()->qemu_bin_path.data, arch);
    nm_vect_insert(&argv, buf.data, buf.len + 1, NULL);

    nm_vect_insert_cstr(&argv, "-M");
    nm_vect_insert_cstr(&argv, "help");

    nm_vect_end_zero(&argv);
    if (nm_spawn_process(&argv, &answer) != NM_OK) {
        nm_str_t warn_msg = NM_INIT_STR;

        nm_str_format(&warn_msg,
            _("Cannot get mach for %-6s  . Error was logged"), arch);
        nm_warn(warn_msg.data);
        nm_str_free(&warn_msg);
        goto out;
    }

    nm_str_trunc(&buf, 0);
    mach_list.list = nm_mach_parse(&answer, &buf);
    nm_str_copy(&mach_list.def, &buf);

    nm_vect_insert(&nm_machs, &mach_list,
        sizeof(mach_list), nm_mach_vect_ins_mlist_cb);
out:
    nm_str_free(&buf);
    nm_vect_free(&argv, NULL);
    nm_str_free(&answer);
    nm_str_free(&mach_list.arch);
    nm_str_free(&mach_list.def);
}

static nm_vect_t *nm_mach_parse(const nm_str_t *buf, nm_str_t *def)
{
    if (!buf || !buf->data) {
        return NULL;
    }

    const char *bufp = buf->data;
    int lookup_mach = 1;
    nm_str_t mach = NM_INIT_STR;
    nm_vect_t *v;

    v = nm_calloc(1, sizeof(nm_vect_t));

    /* skip first line */
    while (*bufp != '\n') {
        bufp++;
    }

    bufp++;

    while (*bufp != '\0') {
        if (lookup_mach && (!isblank(*bufp))) {
            nm_str_add_char_opt(&mach, *bufp);
        } else if (lookup_mach && (isblank(*bufp))) {
            lookup_mach = 0;
        }

        if (!lookup_mach && (*bufp == '\n')) {
            nm_str_t item = NM_INIT_STR;

            nm_str_copy(&item, &mach);
            nm_vect_insert(v, item.data, item.len + 1, NULL);
            nm_str_trunc(&mach, 0);
            lookup_mach = 1;

            /* search and save default machine */
            if (!def->len) {
                const char *tmpp = bufp;
                size_t def_len = 0;

                while (!isblank(*tmpp) && (tmpp != buf->data)) {
                    tmpp--;
                    def_len++;
                }

                if (!strncmp(tmpp + 1, "(default)", def_len - 1)) {
                    nm_str_copy(def, &item);
                }
            }

            nm_str_free(&item);
        }

        bufp++;
    }

    nm_vect_end_zero(v);

    nm_str_free(&mach);

    return v;
}

void nm_mach_vect_ins_mlist_cb(void *unit_p, const void *ctx)
{
    nm_str_copy(nm_mach_arch(unit_p), nm_mach_arch(ctx));
    nm_str_copy(nm_mach_def(unit_p), nm_mach_def(ctx));
    memcpy(nm_mach_list(unit_p), nm_mach_list(ctx), sizeof(nm_vect_t *));
}

void nm_mach_vect_free_mlist_cb(void *unit_p)
{
    nm_str_free(nm_mach_arch(unit_p));
    nm_str_free(nm_mach_def(unit_p));
    free(*nm_mach_list(unit_p));
}

/* vim:set ts=4 sw=4: */
