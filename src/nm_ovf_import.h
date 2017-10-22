#ifndef NM_OVF_IMPORT_H_
#define NM_OVF_IMPORT_H_

#if defined (NM_WITH_OVF_SUPPORT)
void nm_ovf_import(void);
#endif

typedef struct {
    nm_str_t file_name;
    nm_str_t capacity;
} nm_drive_t;

#define NM_INIT_DRIVE { NM_INIT_STR, NM_INIT_STR }
#define nm_drive_file(p) ((nm_drive_t *) p)->file_name
#define nm_drive_size(p) ((nm_drive_t *) p)->capacity

#endif /* NM_OVF_IMPORT_H_ */
/* vim:set ts=4 sw=4 fdm=marker: */
