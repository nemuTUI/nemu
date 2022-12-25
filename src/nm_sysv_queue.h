#include <nm_core.h>

#if defined(NM_OS_DARWIN)

bool nm_sysv_queue_send(const nm_str_t *msg);
bool nm_sysv_queue_recv(nm_str_t *msg);

#endif /* NM_OS_DARWIN */

/* vim:set ts=4 sw=4: */
