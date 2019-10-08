#ifndef NM_STAT_USAGE_H_
#define NM_STAT_USAGE_H_

#include <stdint.h>

extern uint64_t nm_total_cpu_before;
extern uint64_t nm_total_cpu_after;
extern uint64_t nm_proc_cpu_before;
extern uint64_t nm_proc_cpu_after;
extern uint8_t  nm_cpu_iter;
extern float    nm_cpu_usage;

#define NM_STAT_CLEAN() nm_total_cpu_before = nm_total_cpu_after = \
    nm_proc_cpu_before = nm_proc_cpu_after = nm_cpu_iter = nm_cpu_usage = 0;

float nm_stat_get_usage(int pid);

#endif /* NM_STAT_USAGE_H_ */
/* vim:set ts=4 sw=4: */
