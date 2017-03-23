#ifndef NM_VECTOR_H_
#define NM_VECTOR_H_

typedef struct {
    size_t n_memb;  /* unit count */
    size_t n_alloc; /* sizeof allocated memory */
    void **data;    /* ctx */
} nm_vect_t;

/* NOTE: If inserting C string len must include \x00 */
void nm_vect_insert(nm_vect_t *v, void *data, size_t len);
void *nm_vect_at(const nm_vect_t *v, size_t index);
void nm_vect_end_zero(nm_vect_t *v);
void nm_vect_free(nm_vect_t *v);

#define NM_INIT_VECT { 0, 0, 0, NULL }
#define nm_vect_str_get(p) ((nm_str_t *) p)->data

#endif /* NM_VECTOR_H_ */
/* vim:set ts=4 sw=4 fdm=marker: */
