#include <nm_core.h>
#include <nm_utils.h>
#include <nm_string.h>
#include <nm_vector.h>
#include <nm_window.h>
#include <nm_machine.h>
#include <nm_cfg_file.h>

#include <sys/wait.h>

#define NM_INIT_MLIST { NM_INIT_STR, NULL }

#define nm_mach_arch(p) ((nm_mach_t *) p)->arch
#define nm_mach_list(p) ((nm_mach_t *) p)->list

static nm_vect_t nm_machs = NM_INIT_VECT;

static void nm_mach_init(void);
static void nm_mach_get_data(const char *arch);
static nm_vect_t *nm_mach_parse(const nm_str_t *buf);

static void nm_mach_init(void)
{
    const nm_vect_t *archs = &nm_cfg_get()->qemu_targets;

    for (size_t n = 0; n < archs->n_memb; n++)
        nm_mach_get_data(((char **) archs->data)[n]);

#ifdef NM_DEBUG
    nm_debug("\n");
    for (size_t n = 0; n < nm_machs.n_memb; n++)
    {
        nm_vect_t *v = nm_mach_list(nm_machs.data[n]);
        nm_debug("Get machine list for %s:\n",
            nm_mach_arch(nm_machs.data[n]).data);

        for (size_t n = 0; n < v->n_memb; n++)
        {
            nm_debug(">> mach: %s\n", (char *) v->data[n]);
        }
    }
#endif
}

const char **nm_mach_get(const nm_str_t *arch)
{
    const char **v = NULL;

    if (nm_machs.data == NULL)
        nm_mach_init();

    for (size_t n = 0; n < nm_machs.n_memb; n++)
    {
        if (nm_str_cmp_ss(nm_machs.data[n], arch) == NM_OK)
        {
            v = (const char **) nm_mach_list(nm_machs.data[n])->data;
            break;
        }
    }

    return v;
}

void nm_mach_free(void)
{
    for (size_t n = 0; n < nm_machs.n_memb; n++)
        nm_vect_free(nm_mach_list(nm_machs.data[n]), NULL);

    nm_vect_free(&nm_machs, nm_mach_vect_free_mlist_cb);
}

static void nm_mach_get_data(const char *arch)
{
    nm_str_t cmd = NM_INIT_STR;
    nm_str_t answer = NM_INIT_STR;
    nm_mach_t mach_list = NM_INIT_MLIST;

    nm_str_alloc_text(&mach_list.arch, arch);

    nm_str_format(&cmd, "%s/bin/qemu-system-%s -M help",
        NM_STRING(NM_USR_PREFIX), arch);

    if (nm_spawn_process(&cmd, &answer) != NM_OK)
    {
        nm_str_t warn_msg = NM_INIT_STR;
        nm_str_format(&warn_msg,
            _("Cannot get mach for %-6s  . Error was logged"), arch);
        nm_warn(warn_msg.data);
        nm_str_free(&warn_msg);
        goto out;
    }

    mach_list.list = nm_mach_parse(&answer);

    nm_vect_insert(&nm_machs, &mach_list,
        sizeof(mach_list), nm_mach_vect_ins_mlist_cb);
out:
    nm_str_free(&cmd);
    nm_str_free(&answer);
    nm_str_free(&mach_list.arch);
}

static nm_vect_t *nm_mach_parse(const nm_str_t *buf)
{
    const char *bufp = buf->data;
    int lookup_mach = 1;
    nm_str_t mach = NM_INIT_STR;
    nm_vect_t *v = NULL;

    assert(bufp != NULL);

    v = nm_calloc(1, sizeof(nm_vect_t));

    /* skip first line */
    while (*bufp != '\n')
        bufp++;

    bufp++;

    while (*bufp != '\0')
    {
        if (lookup_mach && (*bufp != 0x20))
        {
            nm_str_add_char_opt(&mach, *bufp);
        }
        else if (lookup_mach && (*bufp == 0x20))
        {
            lookup_mach = 0;
        }

        if (!lookup_mach && (*bufp == '\n'))
        {
            nm_str_t item = NM_INIT_STR;

            nm_str_copy(&item, &mach);
            nm_vect_insert(v, item.data, item.len + 1, NULL);
            nm_str_trunc(&mach, 0);
            lookup_mach = 1;

            nm_str_free(&item);
        }

        bufp++;
    }

    nm_vect_end_zero(v);

    nm_str_free(&mach);

    return v;
}

void nm_mach_vect_ins_mlist_cb(const void *unit_p, const void *ctx)
{
    nm_str_copy(&nm_mach_arch(unit_p), &nm_mach_arch(ctx));
    memcpy(&nm_mach_list(unit_p), &nm_mach_list(ctx), sizeof(nm_vect_t *));
}

void nm_mach_vect_free_mlist_cb(const void *unit_p)
{
    nm_str_free(&nm_mach_arch(unit_p));
    free(nm_mach_list(unit_p));
}

/* vim:set ts=4 sw=4 fdm=marker: */
