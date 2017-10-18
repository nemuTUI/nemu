#include <nm_core.h>

#if defined (NM_WITH_OVF_SUPPORT)
#include <nm_utils.h>
#include <nm_string.h>

#define NM_TEST_FILE "/home/void/stuff/ova/test.ova"
#define NM_OVA_DIR_TEMPL "/tmp/ova_extract_XXXXXX"

static int nm_ovf_extract(const nm_str_t *path);

void nm_ovf_import(void)
{
    nm_str_t ova_path = NM_INIT_STR;

    nm_str_alloc_text(&ova_path, NM_TEST_FILE);

    nm_ovf_extract(&ova_path);

    nm_str_free(&ova_path);
}

static int nm_ovf_extract(const nm_str_t *path)
{
    int rc = NM_OK;
    char templ_path[] = NM_OVA_DIR_TEMPL;

    if (mkdtemp(templ_path) == NULL)
        nm_bug("%s: mkdtemp error: %s", __func__, strerror(errno));

    return rc;
}

#endif /* NM_WITH_OVF_SUPPORT */
/* vim:set ts=4 sw=4 fdm=marker: */
