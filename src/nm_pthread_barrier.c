#include <nm_pthread_barrier.h>

#if defined(NM_OS_DARWIN)
int pthread_barrier_init(pthread_barrier_t *barrier,
        const pthread_barrierattr_t *attr, unsigned int count)
{
    (void) attr;

    if (!count) {
        errno = EINVAL;
        return -1;
    }

    if (pthread_mutex_init(&barrier->mutex, 0) < 0) {
        nm_debug("%s: %s\n", __func__, strerror(errno));
        return -1;
    }

    if (pthread_cond_init(&barrier->cond, 0) < 0) {
        nm_debug("%s: %s\n", __func__, strerror(errno));
        pthread_mutex_destroy(&barrier->mutex);
        return -1;
    }

    barrier->count = count;
    barrier->cur_count = 0;

    return 0;
}

int pthread_barrier_wait(pthread_barrier_t *barrier)
{
    pthread_mutex_lock(&barrier->mutex);

    ++(barrier->cur_count);

    if (barrier->cur_count >= barrier->count) {
        barrier->cur_count = 0;
        pthread_cond_broadcast(&barrier->cond);
        pthread_mutex_unlock(&barrier->mutex);

        return 1;
    }

    pthread_cond_wait(&barrier->cond, &barrier->mutex);
    pthread_mutex_unlock(&barrier->mutex);

    return 0;
}

int pthread_barrier_destroy(pthread_barrier_t *barrier)
{
    pthread_cond_destroy(&barrier->cond);
    pthread_mutex_destroy(&barrier->mutex);

    return 0;
}
#endif /* NM_OS_DARWIN */
/* vim:set ts=4 sw=4: */
