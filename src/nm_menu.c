#include <nm_core.h>
#include <nm_menu.h>
#include <nm_utils.h>
#include <nm_string.h>
#include <nm_cfg_file.h>

void nm_print_main_menu(nm_window_t *w, uint32_t highlight)
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

   /* XXX wrefresh(w); */
}

void nm_print_vm_menu(nm_window_t *w, nm_vm_list_t *vm)
{
    int x = 2, y = 2;
    struct stat file_info;
    nm_str_t lock_path = NM_INIT_STR;

    memset(&file_info, 0, sizeof(file_info));

    init_pair(1, COLOR_WHITE, COLOR_BLACK);
    init_pair(2, COLOR_GREEN, COLOR_BLACK);
    wattroff(w, COLOR_PAIR(1));
    wattroff(w, COLOR_PAIR(2));
    box(w, 0, 0);

    for (size_t n = vm->vm_first, i = 0; n < vm->vm_last; n++, i++)
    {
        if (n >= vm->v->n_memb)
            nm_bug(_("%s: invalid index: %zu"), __func__, n);

        nm_str_alloc_str(&lock_path, &nm_cfg_get()->vm_dir);
        nm_str_add_char(&lock_path, '/');
        nm_str_add_text(&lock_path, nm_vect_vm_name(vm->v, n));
        nm_str_add_text(&lock_path, "/" NM_VM_PID_FILE);

        if (stat(lock_path.data, &file_info) != -1)
        {
            nm_vect_set_vm_status(vm->v, n, 1);
            wattron(w, COLOR_PAIR(2));
        }
        else
        {
            nm_vect_set_vm_status(vm->v, n, 0);
            wattron(w, COLOR_PAIR(1));
        }

        if (vm->highlight == i + 1)
        {
            wattron(w, A_REVERSE);
            mvwprintw(w, y, x, "%-20s%s", nm_vect_vm_name(vm->v, n),
                nm_vect_vm_status(vm->v, n) ? NM_VM_RUNNING : NM_VM_STOPPED);
            wattroff(w, A_REVERSE);
        }
        else
        {
            mvwprintw(w, y, x, "%-20s%s", nm_vect_vm_name(vm->v, n),
                nm_vect_vm_status(vm->v, n) ? NM_VM_RUNNING : NM_VM_STOPPED);
        }

        y++;
        wrefresh(w);
    }

    nm_str_free(&lock_path);
}
/* vim:set ts=4 sw=4 fdm=marker: */
