#ifndef NM_MACHINE_H_
#define NM_MACHINE_H_

typedef struct {
     nm_str_t mach;
     nm_str_t desc;
} nm_mach_data_t;

typedef struct {
     nm_str_t arch;
     nm_vect_t *mach_list;
} nm_mach_t;

void nm_mach_init(void);
void nm_mach_vect_ins_mdata_cb(const void *unit_p, const void *ctx);
void nm_mach_vect_ins_mlist_cb(const void *unit_p, const void *ctx);
void nm_mach_vect_free_mdata_cb(const void *unit_p);
void nm_mach_vect_free_mlist_cb(const void *unit_p);

#define NM_INIT_MDATA { NM_INIT_STR, NM_INIT_STR }
#define NM_INIT_MLIST { NM_INIT_STR, NULL }

#endif /* NM_MACHINE_H_ */
/* vim:set ts=4 sw=4 fdm=marker: */
