#include <nm_core.h>
#include <nm_string.h>
#include <nm_window.h>

void nm_lan_settings(void)
{
    int ch = 0;
    nm_print_vm_window();

    for (;;)
    {
        switch (ch = getch()) {
        case NM_KEY_ESC:
            return;
        }
    }
}

/* vim:set ts=4 sw=4 fdm=marker: */
