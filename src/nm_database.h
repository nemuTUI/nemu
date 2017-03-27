#ifndef NM_DATABASE_H_
#define NM_DATABASE_H_

#include <nm_vector.h>

void nm_db_init(void);
void nm_db_select(const char *query, nm_vect_t *v);
void nm_db_edit(const char *query);
void nm_db_close(void);

enum select_idx {
    NM_SQL_ID = 0,
    NM_SQL_NAME,
    NM_SQL_MEM,
    NM_SQL_SMP,
    NM_SQL_KVM,
    NM_SQL_HCPU,
    NM_SQL_VNC,
    NM_SQL_ARCH,
    NM_SQL_ISO,
    NM_SQL_INST,
    NM_SQL_USBF,
    NM_SQL_USBD,
    NM_SQL_BIOS,
    NM_SQL_KERN,
    NM_SQL_OVER,
    NM_SQL_DINT,
    NM_SQL_KAPP,
    NM_SQL_TTY,
    NM_SQL_SOCK
};

#endif /* NM_DATABASE_H_ */
/* vim:set ts=4 sw=4 fdm=marker: */
