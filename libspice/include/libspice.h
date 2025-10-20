#ifndef LIBSPICE_H_
#define LIBSPICE_H_

#define SPICE_DEFAULT_PORT 5900
#define SPICE_DEFAULT_ADDR "127.0.0.1"

#include <stdio.h>
#include <stdbool.h>
#include <stdatomic.h>
#include <netinet/in.h>

#include <X11/XKBlib.h>

#ifdef _cplusplus
extern "C" {
#endif

typedef struct spice spice_t;

spice_t *spice_init(const char *addr, uint16_t port, const char *log);
void spice_deinit(spice_t *spice);
void spice_cancel(spice_t *spice);
bool spice_is_canceled(const spice_t *spice);
bool spice_channel_init_main(spice_t *spice);
bool spice_channel_init_inputs(spice_t *spice);
void *spice_channel_main_loop(void *ctx);
void *spice_channel_display_loop(void *ctx); /* channel init is not required */
void *spice_draw_screen(void *ctx);
void spice_send_key(spice_t *spice, uint32_t keycode, KeySym keysym, bool up);
void spice_send_mouse_motion(spice_t *spice, int x, int y);
void spice_send_mouse_button(spice_t *spice, unsigned int button, bool up);

#ifdef _cplusplus
}
#endif

#endif /* LIBSPICE_H_ */
/* vim:set ts=4 sw=4: */
