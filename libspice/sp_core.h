#ifndef SP_CORE_H_
#define SP_CORE_H_

#define SP_EXPORT __attribute__((visibility("default")))

struct spice {
    int sd_main;
    int sd_display;
    int sd_inputs;
    uint32_t session_id;
    atomic_bool stop;
    atomic_bool data_ready;
    struct sockaddr_in endpoint;
    int32_t disp_width_px;
    int32_t disp_height_px;
    int32_t term_width_px;
    int32_t term_height_px;
    bool log;
    FILE *logfp;
    char *bmp_buf;
    uint64_t serial;
    uint16_t button_state;
};

#endif /* SP_CORE_H_ */
/* vim:set ts=4 sw=4: */
