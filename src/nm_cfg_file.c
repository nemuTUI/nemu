#include <nm_core.h>
#include <nm_utils.h>
#include <nm_string.h>
#include <nm_cfg_file.h>

static nm_cfg_t cfg;

void nm_cfg_init(void)
{
    struct stat file_info;
    struct passwd *pw = getpwuid(getuid());
    nm_str_t cfg_path = NM_INIT_STR;

    if (!pw)
        nm_bug(_("Error get home directory: %s\n"), strerror(errno));

    nm_str_alloc(&cfg_path, pw->pw_dir);
    nm_str_free(&cfg_path);
}

const nm_cfg_t *nm_cfg_get(void)
{
    return &cfg;
}
/* vim:set ts=4 sw=4 fdm=marker: */
