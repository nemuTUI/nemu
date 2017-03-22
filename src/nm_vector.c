#include <nm_core.h>
#include <nm_utils.h>
#include <nm_vector.h>

#define NM_VECT_INIT_NMEMB 10

void nm_vect_insert(nm_vect_t *v, void *data, size_t len)
{
    if (v == NULL)
        nm_bug(_("%s: NULL vector pointer value"), __func__);

    if (v->data == NULL) /* list not initialized */
    {
        v->data = nm_alloc(sizeof(void *) * NM_VECT_INIT_NMEMB);
        v->n_alloc = NM_VECT_INIT_NMEMB;
    }

    if (v->n_memb == v->n_alloc)
        v->data = nm_realloc(v->data, v->n_alloc * 2);

    v->data[v->n_memb] = nm_calloc(1, len);
    memcpy(v->data[v->n_memb], data, len);

    v->n_memb++;
}

void *nm_vect_at(const nm_vect_t *v, size_t index)
{
    if (v == NULL)
        nm_bug(_("%s: NULL vector pointer value"), __func__);

    if (index >= v->n_memb)
        nm_bug(_("%s: invalid index"), __func__);
    
    return v->data[index];
}

void nm_vect_free(nm_vect_t *v)
{
    if (v == NULL)
        return;

    for (size_t n = 0; n < v->n_memb; n++)
        free(v->data[n]);

    free(v->data);

    v->data = NULL;
    v->n_memb = 0;
    v->n_alloc = 0;
}

/* vim:set ts=4 sw=4 fdm=marker: */
