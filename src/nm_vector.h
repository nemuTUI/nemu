#ifndef NM_VECTOR_H_
#define NM_VECTOR_H_

typedef struct {
    size_t n_memb;  /* unit count */
    size_t n_alloc; /* count of allocated memory in units */
    void **data;    /* ctx */
} nm_vect_t;

typedef void (*nm_vect_ins_cb_pt)(const void *unit_p, const void *ctx);
typedef void (*nm_vect_free_cb_pt)(const void *unit_p);

/* NOTE: If inserting C string len must include \x00 */
void nm_vect_insert(nm_vect_t *v, const void *data, size_t len, nm_vect_ins_cb_pt cb);
void *nm_vect_at(const nm_vect_t *v, size_t index);
void nm_vect_end_zero(nm_vect_t *v);
void nm_vect_free(nm_vect_t *v, nm_vect_free_cb_pt cb);

#define NM_INIT_VECT { 0, 0, NULL }

#define nm_vect_str(p, idx) ((nm_str_t *) nm_vect_at(p, idx))
#define nm_vect_str_ctx(p, idx) ((nm_str_t *) nm_vect_at(p, idx))->data
#define nm_vect_str_len(p, idx) ((nm_str_t *) nm_vect_at(p, idx))->len
#define nm_vect_item_name(p, idx) ((nm_menu_item_t *) nm_vect_at(p, idx))->name->data
#define nm_vect_item_name_str(p, idx) ((nm_menu_item_t *) nm_vect_at(p, idx))->name
#define nm_vect_item_name_cur(p) nm_vect_item_name_str(p.v, (p.item_first + p.highlight) - 1)
#define nm_vect_item_status(p, idx) ((nm_menu_item_t *) nm_vect_at(p, idx))->status
#define nm_vect_item_status_cur(p) nm_vect_item_status(p.v, (p.item_first + p.highlight) - 1)
#define nm_vect_set_item_status(p, idx, s) ((nm_menu_item_t *) nm_vect_at(p, idx))->status = s

#define nm_vect_insert_cstr(p, data) nm_vect_insert(p, data, strlen(data) + 1, NULL)

#endif /* NM_VECTOR_H_ */
/* vim:set ts=4 sw=4 fdm=marker: */
