#include <nm_core.h>

#if defined (NM_WITH_OVF_SUPPORT)
#include <nm_utils.h>
#include <nm_string.h>

#include <limits.h>

/* libarchive */
#include <archive.h>
#include <archive_entry.h>

typedef struct archive nm_archive_t;
typedef struct archive_entry nm_archive_entry_t;

#define NM_TEST_FILE "/home/void/stuff/ova/test.ova"
#define NM_OVA_DIR_TEMPL "/tmp/ova_extract_XXXXXX"
#define NM_BLOCK_SIZE 10240

static void nm_ovf_extract(const nm_str_t *ova_path, const char *tmp_dir);
static void nm_archive_copy_data(nm_archive_t *in, nm_archive_t *out);

void nm_ovf_import(void)
{
    char templ_path[] = NM_OVA_DIR_TEMPL;
    nm_str_t ova_path = NM_INIT_STR;

    nm_str_alloc_text(&ova_path, NM_TEST_FILE);

    if (mkdtemp(templ_path) == NULL)
        nm_bug("%s: mkdtemp error: %s", __func__, strerror(errno));

    nm_ovf_extract(&ova_path, templ_path);

    nm_str_free(&ova_path);
}

static void nm_ovf_extract(const nm_str_t *ova_path, const char *tmp_dir)
{
    nm_archive_t *in, *out;
    nm_archive_entry_t *ar_entry;
    char cwd[PATH_MAX] = {0};
    int rc;

    in = archive_read_new();
    out = archive_write_disk_new();

    archive_write_disk_set_options(out, 0);
    archive_read_support_format_tar(in);

    if (archive_read_open_filename(in, ova_path->data, NM_BLOCK_SIZE) != 0)
        nm_bug("%s: ovf read error: %s", __func__, archive_error_string(in));

    /* save current working directory */
    if (getcwd(cwd, sizeof(cwd)) == NULL)
        nm_bug("%s: getcwd error: %s", __func__, strerror(errno));

    /* change current working directory to temporary */
    if (chdir(tmp_dir) == -1)
        nm_bug("%s: chdir error: %s", __func__, strerror(errno));

    for (;;)
    {
        rc = archive_read_next_header(in, &ar_entry);

        if (rc == ARCHIVE_EOF)
            break;

        if (rc != ARCHIVE_OK)
            nm_bug("%s: bad archive: %s", __func__, archive_error_string(in));

#ifdef NM_DEBUG
        nm_debug("ova content: %s\n", archive_entry_pathname(ar_entry));
#endif
        rc = archive_write_header(out, ar_entry);
        if (rc != ARCHIVE_OK)
            nm_bug("%s: bad archive: %s", __func__, archive_error_string(in));
        else
        {
            nm_archive_copy_data(in, out);
            if (archive_write_finish_entry(out) != ARCHIVE_OK)
            {
                nm_bug("%s: archive_write_finish_entry: %s",
                       __func__, archive_error_string(out));
            }
        }
    }

    archive_read_free(in);
    archive_write_free(out);

    /* restore current working directory */
    if (chdir(cwd) == -1)
        nm_bug("%s: chdir error: %s", __func__, strerror(errno));
}

static void nm_archive_copy_data(nm_archive_t *in, nm_archive_t *out)
{
    int rc;
    const void *buf;
    size_t size;
#if ARCHIVE_VERSION_NUMBER >= 3000000
    int64_t offset;
#else
    off_t offset;
#endif

    for (;;)
    {
        rc = archive_read_data_block(in, &buf, &size, &offset);
        if (rc == ARCHIVE_EOF)
            break;
        if (rc != ARCHIVE_OK)
            nm_bug("%s: error read archive: %s", __func__, archive_error_string(in));

        if (archive_write_data_block(out, buf, size, offset) != ARCHIVE_OK)
            nm_bug("%s: error write archive: %s", __func__, archive_error_string(out));
    }
}

#endif /* NM_WITH_OVF_SUPPORT */
/* vim:set ts=4 sw=4 fdm=marker: */
