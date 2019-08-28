#ifndef NM_OVF_IMPORT_H_
#define NM_OVF_IMPORT_H_

#include <nm_string.h>

#if defined (NM_WITH_OVF_SUPPORT)
void nm_ovf_import(void);
#endif

typedef struct {
    nm_str_t file_name;
    nm_str_t capacity;
} nm_drive_t;

#define NM_INIT_DRIVE (nm_drive_t) { NM_INIT_STR, NM_INIT_STR }

static inline nm_str_t *nm_drive_file(const nm_drive_t *drive)
{
    return (nm_str_t *)&drive->file_name;
}
static inline nm_str_t *nm_drive_size(const nm_drive_t *drive)
{
    return (nm_str_t *)&drive->capacity;
}

#endif /* NM_OVF_IMPORT_H_ */
/* vim:set ts=4 sw=4 fdm=marker: */
