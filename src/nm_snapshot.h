#ifndef NM_SNAPSHOT_H_
#define NM_SNAPSHOT_H_

void nm_snapshot_create(const nm_str_t *name);
void nm_snapshot_revert(const nm_str_t *name);

#endif /* NM_SNAPSHOT_H_ */
/* vim:set ts=4 sw=4 fdm=marker: */
