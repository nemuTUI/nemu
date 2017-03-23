#ifndef NM_DATABASE_H_
#define NM_DATABASE_H_

#include <nm_vector.h>

void nm_db_init(void);
void nm_db_select(const char *query, nm_vect_t *v);
void nm_db_edit(const char *query);
void nm_db_close(void);

#endif /* NM_DATABASE_H_ */
/* vim:set ts=4 sw=4 fdm=marker: */
