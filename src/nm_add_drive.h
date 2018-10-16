#ifndef NM_ADD_DRIVE_H_
#define NM_ADD_DRIVE_H_

void nm_add_drive(const nm_str_t *name);
void nm_del_drive(const nm_str_t *name);

#define NM_DRIVE_LIMIT 30

#endif /* NM_ADD_DRIVE_H_ */
/* vim:set ts=4 sw=4 fdm=marker: */
