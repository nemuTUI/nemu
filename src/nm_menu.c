#include <nm_core.h>

void nm_print_main_menu(WINDOW *w, uint32_t highlight)
{
    const char *nm_main_chioces[] = {
        _("Manage guests"),
        _("Install guest"),
        _("Quit"),
    };

    box(w, 0, 0);

    for (uint32_t x = 2, y = 2, n = 0; n < NM_MAIN_CHOICES; n++, y++)
    {
        if (highlight ==  n + 1)
        {
            wattron(w, A_REVERSE);
            mvwprintw(w, y, x, "%s", nm_main_chioces[n]);
            wattroff(w, A_REVERSE);
        } 
        else
            mvwprintw(w, y, x, "%s", nm_main_chioces[n]);
    }
}
/* vim:set ts=4 sw=4 fdm=marker: */
