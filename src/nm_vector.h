#ifndef NM_VECTOR_H_
#define NM_VECTOR_H_

#include <stdlib.h>
#include <string.h>

typedef struct {
    size_t n_memb;  /* unit count */
    size_t n_alloc; /* count of allocated memory in units */
    void **data;    /* ctx */
} nm_vect_t;

#define NM_INIT_VECT { 0, 0, NULL }

typedef void (*nm_vect_ins_cb_pt)(void *unit_p, const void *ctx);
typedef void (*nm_vect_free_cb_pt)(void *unit_p);

/* NOTE: If inserting C string len must include \x00 */
void nm_vect_insert(nm_vect_t *v, const void *data, size_t len, nm_vect_ins_cb_pt cb);
void *nm_vect_at(const nm_vect_t *v, size_t index);
void nm_vect_end_zero(nm_vect_t *v);
void nm_vect_free(nm_vect_t *v, nm_vect_free_cb_pt cb);

static inline void nm_vect_insert_cstr(nm_vect_t *v, const char *data)
{
    nm_vect_insert(v, data, strlen(data) + 1, NULL);
}

#endif /* NM_VECTOR_H_ */
/* vim:set ts=4 sw=4 fdm=marker: */
