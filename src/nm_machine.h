#ifndef NM_MACHINE_H_
#define NM_MACHINE_H_

typedef struct {
     nm_str_t mach;
     nm_str_t desc;
} nm_mach_data_t;

typedef struct {
     nm_str_t arch;
     nm_vect_t mach_list;
} nm_mach_t;

void nm_mach_init(void);

#endif /* NM_MACHINE_H_ */
/* vim:set ts=4 sw=4 fdm=marker: */
