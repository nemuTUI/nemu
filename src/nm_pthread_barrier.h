#include <nm_core.h>

/*
 * Macos does not have pthread_barrier.
 * Let's make them ourselves.
 */

#if defined(NM_OS_DARWIN)
#include <pthread.h>

typedef int pthread_barrierattr_t;

typedef struct {
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    unsigned int count;
    unsigned int cur_count;
} pthread_barrier_t;

int pthread_barrier_init(pthread_barrier_t *barrier,
        const pthread_barrierattr_t *attr, unsigned int count);
int pthread_barrier_wait(pthread_barrier_t *barrier);
int pthread_barrier_destroy(pthread_barrier_t *barrier);

#endif /* NM_OS_DARWIN */

/* vim:set ts=4 sw=4: */
