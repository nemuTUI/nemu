#include <nm_core.h>
#include <nm_utils.h>
#include <nm_vector.h>

#define NM_VECT_INIT_NMEMB 10

void nm_vect_insert(nm_vect_t *v, const void *data, size_t len, nm_vect_ins_cb_pt cb)
{
    if (v == NULL)
        nm_bug(_("%s: NULL vector pointer value"), __func__);

    if (v->data == NULL) /* list not initialized */
    {
        v->data = nm_alloc(sizeof(void *) * NM_VECT_INIT_NMEMB);
        v->n_alloc = NM_VECT_INIT_NMEMB;
        memset(v->data, 0, v->n_alloc * sizeof(void *));
    }

    if (v->n_memb == v->n_alloc)
    {
        v->data = nm_realloc(v->data, sizeof(void *) * (v->n_alloc * 2));
        memset(v->data + v->n_alloc, 0, v->n_alloc * sizeof(void *));
        v->n_alloc *= 2;
    }

    v->data[v->n_memb] = nm_calloc(1, len);
    if (cb != NULL)
        cb(v->data[v->n_memb], data);
    else
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

void nm_vect_end_zero(nm_vect_t *v)
{
    if (v->n_alloc > v->n_memb)
        return;

    v->data = nm_realloc(v->data, sizeof(void *) * (v->n_alloc + 1));
    v->data[v->n_alloc] = NULL;
    v->n_alloc++;
}

void nm_vect_free(nm_vect_t *v, nm_vect_free_cb_pt cb)
{
    if (v == NULL)
        return;

    for (size_t n = 0; n < v->n_memb; n++)
    {
        if (cb != NULL)
            cb(v->data[n]);
        free(v->data[n]);
    }

    free(v->data);

    v->data = NULL;
    v->n_memb = 0;
    v->n_alloc = 0;
}

/* vim:set ts=4 sw=4 fdm=marker: */
