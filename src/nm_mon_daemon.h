#ifndef NM_MON_DAEMON_H_
#define NM_MON_DAEMON_H_

#include <nm_string.h>
#include <nm_vector.h>

#include <pthread.h>

typedef struct nm_thrctrl {
    bool stop;
} nm_thr_ctrl_t;

#define NM_THR_CTRL_INIT (nm_thr_ctrl_t) { false }

typedef struct nm_mon_item {
    nm_str_t *name;
    int8_t state;
} nm_mon_item_t;

#define NM_ITEM_INIT (nm_mon_item_t) { NULL, -1 }

typedef struct nm_mon_vms {
    nm_vect_t *list;
    pthread_mutex_t mtx;
} nm_mon_vms_t;

#define NM_MON_VMS_INIT (nm_mon_vms_t) { NULL, PTHREAD_MUTEX_INITIALIZER }

void nm_mon_start(void);
void nm_mon_loop(void);
void nm_mon_ping(void);

static const int NM_MON_SLEEP = 1000;

static inline int8_t nm_mon_item_get_status(const nm_vect_t *v, const size_t idx)
{
    return ((nm_mon_item_t *) nm_vect_at(v, idx))->state;
}

static inline void nm_mon_item_set_status(const nm_vect_t *v, const size_t idx, int8_t s)
{
    ((nm_mon_item_t *) nm_vect_at(v, idx))->state = s;
}

static inline nm_str_t *nm_mon_item_get_name(const nm_vect_t *v, const size_t idx)
{
    return ((nm_mon_item_t *) nm_vect_at(v, idx))->name;
}

static inline char *nm_mon_item_get_name_cstr(const nm_vect_t *v, const size_t idx)
{
    return ((nm_mon_item_t *) nm_vect_at(v, idx))->name->data;
}

#endif
/* vim:set ts=4 sw=4: */
