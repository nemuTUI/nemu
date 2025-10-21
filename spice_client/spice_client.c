#include <time.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>
#include <pthread.h>
#include <sys/select.h>

#include <X11/X.h>
#include <X11/Xutil.h>
#include <X11/cursorfont.h>

#include <libspice.h>

#define LOG_PATH "/tmp/spice_client.log"

int main(int argc, char **argv)
{
    int rc = 0, opt;
    int x11_fd;
    fd_set fdset;
    struct timeval tv;
    bool alt_down = false;
    bool mouse_hide = false;
    pthread_t display_th, main_th, screen_th;
    Display *display;
    Window root_window, focus_window;
    XEvent event;
    Cursor xcursor;
    int revert_to;
    char *spice_addr = NULL;
    char *log_path = NULL;
    int port = SPICE_DEFAULT_PORT;
    spice_t *spice;

    while ((opt = getopt(argc, argv, "a:p:vmh")) != -1) {
        switch (opt) {
        case 'a':
            spice_addr = strdup(optarg);
            if (!spice_addr) {
                fprintf(stderr, "%s: %s\n", __func__, strerror(errno));
                exit(EXIT_FAILURE);
            }
            break;
        case 'p':
            if ((port = atoi(optarg)) == 0) {
                fprintf(stderr, "bad port value\n");
                exit(EXIT_FAILURE);
            }
            break;
        case 'v':
            log_path = strdup(LOG_PATH);
            if (!log_path) {
                fprintf(stderr, "%s: %s\n", __func__, strerror(errno));
                exit(EXIT_FAILURE);
            }
            break;
        case 'm':
            mouse_hide = true;
            break;
        case 'h':
            printf("Usage: %s\n"
                   "Options:\n"
                   " -a <addr> - IPv4 address (default: %s)\n"
                   " -p <port> - SPICE port (default: %u)\n"
                   " -v        - enable log\n"
                   " -m        - hide mouse cursor\n"
                   " -h        - print help and exit\n",
                   *argv, SPICE_DEFAULT_ADDR, SPICE_DEFAULT_PORT);
            exit(EXIT_SUCCESS);
        }
    }

    if (!spice_addr) {
        spice_addr = strdup(SPICE_DEFAULT_ADDR);
        if (!spice_addr) {
            fprintf(stderr, "%s: %s\n", __func__, strerror(errno));
            exit(EXIT_FAILURE);
        }
    }

    if ((spice = spice_init(spice_addr, port, log_path)) == NULL) {
        fprintf(stderr, "spice_init failed\n");
        exit(EXIT_FAILURE);
    }

    if (!spice_channel_init_main(spice)) {
        fprintf(stderr, "init main channel failed\n");
        exit(EXIT_FAILURE);
    }

    if (pthread_create(&main_th, NULL,
                spice_channel_main_loop, spice) != 0) {
        fprintf(stderr, "failed to create session control channel thread\n");
        exit(EXIT_FAILURE);
    }

    if (!spice_channel_init_inputs(spice)) {
        fprintf(stderr, "init inputs channel failed\n");
        exit(EXIT_FAILURE);
    }

    if (pthread_create(&display_th, NULL,
                spice_channel_display_loop, spice) != 0) {
        fprintf(stderr, "failed to create display channel thread\n");
        exit(EXIT_FAILURE);
    }

    display = XOpenDisplay(NULL);
    if (display == NULL) {
        fprintf(stderr, "unable to open X display\n");
        rc = 1;
        goto out;
    }

    if (pthread_create(&screen_th, NULL,
                spice_draw_screen, spice) != 0) {
        fprintf(stderr, "failed to create screen thread\n");
        exit(EXIT_FAILURE);
    }

    root_window = DefaultRootWindow(display);
    xcursor = XCreateFontCursor(display, XC_arrow);
    XGetInputFocus(display, &focus_window, &revert_to);
    XGrabKeyboard(display, root_window, False,
            GrabModeAsync, GrabModeAsync, CurrentTime);
    XGrabPointer(display, focus_window, False,
            PointerMotionMask | ButtonPressMask | ButtonReleaseMask,
            GrabModeAsync, GrabModeAsync, focus_window,
            mouse_hide ? None : xcursor, CurrentTime);
    x11_fd = ConnectionNumber(display);

    for (;;) {
        int fds;

        FD_ZERO(&fdset);
        FD_SET(x11_fd, &fdset);
        tv.tv_usec = 0;
        tv.tv_sec = 1;

        fds = select(x11_fd + 1, &fdset, NULL, NULL, &tv);
        if (fds < 0) {
            goto out;
        }

        if (spice_is_canceled(spice)) {
            goto quit;
        }

        while (XPending(display)) {
            XNextEvent(display, &event);
            KeySym keysym = XkbKeycodeToKeysym(display,
                    event.xkey.keycode, 0, 0);

            switch (event.type) {
            case KeyPress:
                if (keysym == XK_Alt_L) {
                    alt_down = true;
                } else if (keysym != XK_q && alt_down) {
                    alt_down = false;
                }

                if (alt_down && keysym == XK_q) {
                    spice_cancel(spice);
                    goto quit;
                }

                spice_send_key(spice, event.xkey.keycode, keysym, false);
                break;
            case KeyRelease:
                spice_send_key(spice, event.xkey.keycode, keysym, true);
                break;
            case MotionNotify: {
                XButtonEvent *mouse_event;

                mouse_event = (XButtonEvent *) &event;
                spice_send_mouse_motion(spice, mouse_event->x, mouse_event->y);
                }
                break;
            case ButtonPress:
                spice_send_mouse_button(spice, event.xbutton.button, false);
                break;
            case ButtonRelease:
                spice_send_mouse_button(spice, event.xbutton.button, true);
                break;
            }
        }
    }

quit:
    pthread_join(screen_th, NULL);
    pthread_join(display_th, NULL);
    pthread_join(main_th, NULL);
    spice_deinit(spice);
    XUngrabKeyboard(display, CurrentTime);
    XUngrabPointer(display, CurrentTime);
    XFreeCursor(display, xcursor);
    XCloseDisplay(display);
    free(log_path);
    free(spice_addr);
out:
    return rc;
}
