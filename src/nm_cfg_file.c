#include <nm_core.h>
#include <nm_cfg_file.h>

static nm_cfg_t cfg;

void nm_cfg_init(void)
{
    struct stat file_info;
    struct passwd *pw = getpwuid(getuid());
    char *cfg_path;
    size_t cfg_path_len;

    if (!pw)
    {
        fprintf(stderr, _("Error get home directory: %s\n"), strerror(errno));
        exit(NM_ERR);
    }
}

const nm_cfg_t *nm_cfg_get(void)
{
    return &cfg;
}
/* vim:set ts=4 sw=4 fdm=marker: */
