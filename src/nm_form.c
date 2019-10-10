#include <nm_core.h>
#include <nm_form.h>
#include <nm_utils.h>
#include <nm_vector.h>
#include <nm_window.h>
#include <nm_database.h>

#include <time.h>
#include <glob.h>
#include <dirent.h>

const char *nm_form_yes_no[] = {
    "yes",
    "no",
    NULL
};

const char *nm_form_net_drv[] = {
    "virtio-net-pci",
    "rtl8139",
    "e1000",
    "vmxnet3",
    NULL
};

const char *nm_form_drive_drv[] = {
    "ide",
    "scsi",
    "virtio",
    NULL
};

const char *nm_form_macvtap[] = {
    "no",
    "macvtap:bridge",
    "macvtap:private",
    NULL
};

const char *nm_form_usbtype[] = {
    "EHCI",
    "XHCI",
    NULL
};

const char *nm_form_svg_layer[] = {
    "UP",
    "DOWN",
    "ALL",
    NULL
};

static int nm_append_path(nm_str_t *path);

void nm_form_free(nm_form_t *form, nm_field_t **fields)
{
    if (form)
    {
        unpost_form(form);
        free_form(form);
        form = NULL;
    }

    if (fields)
    {
        for (; *fields; fields++)
        {
            free_field(*fields);
            *fields = NULL;
        }
    }

    curs_set(0);
}

nm_form_t *
nm_post_form(nm_window_t *w, nm_field_t **field, int begin_x, int color)
{
    nm_form_t *form;
    int rows, cols;

    if (color)
        wbkgd(w, COLOR_PAIR(NM_COLOR_BLACK));

    form = new_form(field);
    if (form == NULL)
         nm_bug("%s: %s", __func__, strerror(errno));

    set_form_win(form, w);
    scale_form(form, &rows, &cols);
    set_form_sub(form, derwin(w, rows, cols, 1, begin_x));
    post_form(form);
    curs_set(1);

    return form;
}

int nm_draw_form(nm_window_t *w, nm_form_t *form)
{
    int confirm = NM_ERR, rc = NM_OK;
    int ch;
    nm_str_t buf = NM_INIT_STR;

    wtimeout(w, 100);

    while ((ch = wgetch(w)) != NM_KEY_ESC)
    {
        if (confirm == NM_OK)
            break;

        switch(ch) {
        case KEY_DOWN:
            form_driver(form, REQ_VALIDATION);
            form_driver(form, REQ_NEXT_FIELD);
            form_driver(form, REQ_END_LINE);
            break;

        case KEY_UP:
            form_driver(form, REQ_VALIDATION);
            form_driver(form, REQ_PREV_FIELD);
            form_driver(form, REQ_END_LINE);
            break;

        case KEY_LEFT:
            if (field_type(current_field(form)) == TYPE_ENUM)
                form_driver(form, REQ_PREV_CHOICE);
            else
                form_driver(form, REQ_PREV_CHAR);
            break;

        case KEY_RIGHT:
            if (field_type(current_field(form)) == TYPE_ENUM)
                form_driver(form, REQ_NEXT_CHOICE);
            else
                form_driver(form, REQ_NEXT_CHAR);
            break;

        case KEY_HOME:
            form_driver(form, REQ_BEG_LINE);
            break;

        case KEY_END:
            form_driver(form, REQ_END_LINE);
            break;

        case KEY_BACKSPACE:
        case 127:
            form_driver(form, REQ_DEL_PREV);
            break;

        case 0x9: /* TAB KEY */
            if (field_type(current_field(form)) != TYPE_REGEXP)
                break;
            {
                form_driver(form, REQ_NEXT_FIELD);
                form_driver(form, REQ_PREV_FIELD);
                form_driver(form, REQ_END_FIELD);

                nm_get_field_buf(current_field(form), &buf);

                if (nm_append_path(&buf) == NM_OK)
                {
                    set_field_buffer(current_field(form), 0, buf.data);
                    form_driver(form, REQ_END_FIELD);
                }
                nm_str_trunc(&buf, 0);
            }
            break;

        case NM_KEY_ENTER:
            confirm = NM_OK;
            if (form_driver(form, REQ_VALIDATION) != E_OK)
                rc = NM_ERR;
            break;

        default:
            form_driver(form, ch);
            break;
        }
    }

    nm_str_free(&buf);

    if ((confirm == NM_OK) && (rc == NM_ERR))
    {
        confirm = NM_ERR;
        NM_FORM_RESET();
        nm_warn(_(NM_MSG_BAD_CTX));
    }

    return confirm;
}

int nm_form_calc_size(size_t max_msg, size_t f_num, nm_form_data_t *form)
{
    size_t cols, rows;

    getmaxyx(action_window, rows, cols);

    form->w_cols = cols * NM_FORM_RATIO;
    form->w_rows = (f_num * 2) + 1;

    if (form->w_cols < (max_msg + 18) ||
        form->w_rows > rows - 4)
    {
        nm_warn(_(NM_MSG_SMALL_WIN));
        return NM_ERR;
    }

    form->w_start_x = ((1 - NM_FORM_RATIO) * cols) / 2;
    form->form_len = form->w_cols - (max_msg + 6);
    form->form_window = derwin(action_window, form->w_rows,
            form->w_cols, 3, form->w_start_x);

    return NM_OK;
}

void nm_get_field_buf(nm_field_t *f, nm_str_t *res)
{
    char *buf, *s;

    if ((buf = field_buffer(f, 0)) == NULL)
        nm_bug("%s: %s", __func__, strerror(errno));

    s = strrchr(buf, 0x20);

    if (s != NULL)
    {
        while ((s > buf) && (s[-1] == 0x20))
            --s;

        *s = '\0';
    }

    nm_str_add_text(res, buf);
}

static int nm_append_path(nm_str_t *path)
{
    int rc = NM_ERR;
    glob_t res;
    char *rp = NULL;
    struct stat file_info;

    memset(&file_info, 0, sizeof(file_info));

    if (path->data[0] == '~')
    {
        nm_str_t new_path = NM_INIT_STR;
        const char *home;

        if ((home = getenv("HOME")) == NULL)
            return NM_ERR;

        nm_str_format(&new_path, "%s%s", home,
                      (path->len > 1) ? path->data + 1 : "");
        nm_str_trunc(path, 0);
        nm_str_copy(path, &new_path);
        nm_str_free(&new_path);
    }

    if (glob(path->data, 0, NULL, &res) != 0)
    {
        nm_str_t tmp = NM_INIT_STR;

        nm_str_format(&tmp, "%s*", path->data);
        if (glob(tmp.data, 0, NULL, &res) == 0)
        {
            nm_str_trunc(path, 0);
            if ((rp = realpath(res.gl_pathv[0], NULL)) != NULL)
            {
                nm_str_add_text(path, rp);
                nm_str_free(&tmp);
                rc = NM_OK;
                if (stat(path->data, &file_info) != -1)
                {
                    if (S_ISDIR(file_info.st_mode) && path->len > 1)
                        nm_str_add_char(path, '/');
                }
                goto out;
            }
        }

        nm_str_free(&tmp);
        goto out;
    }

    if ((rp = realpath(res.gl_pathv[0], NULL)) != NULL)
    {
        nm_str_trunc(path, 0);
        nm_str_add_text(path, rp);
        rc = NM_OK;
        if (stat(path->data, &file_info) != -1)
        {
            if (S_ISDIR(file_info.st_mode) && path->len > 1)
                nm_str_add_char(path, '/');
        }
    }

out:
    if (rp)
        free(rp);
    globfree(&res);

    return rc;
}

void *nm_progress_bar(void *data)
{
    struct timespec ts;
    int cols = getmaxx(action_window);
    int x1 = 1, x2 = cols - 2;
    int x1_v = 0, x2_v = 1;
    nm_spinner_data_t *dp = data;

    memset(&ts, 0, sizeof(ts));
    ts.tv_nsec = 3e+7; /* 0.03sec */

    curs_set(0);

    for (;;)
    {
        if (*dp->stop)
            break;

        NM_ERASE_TITLE(action, cols);
        mvwaddch(action_window, 1, x1, x1_v ? '<' : '>');
        mvwaddch(action_window, 1, x2, x2_v ? '<' : '>');
        wrefresh(action_window);

        (x1_v == 0) ? x1++ : x1--;
        (x2_v == 0) ? x2++ : x2--;

        if (x1 == cols - 2)
            x1_v = 1;
        if (x1 == 1)
            x1_v = 0;

        if (x2 == 1)
            x2_v = 0;
        if (x2 == cols - 2)
            x2_v = 1;

        nanosleep(&ts, NULL);
    }

    pthread_exit(NULL);
}

#if 0
void *nm_spinner(void *data)
{
    const char spin_chars[] ="/-\\|";
    nm_spinner_data_t *dp = data;
    struct timespec ts;

    memset(&ts, 0, sizeof(ts));
    ts.tv_nsec = 3e+7; /* 0.03sec */

    if (dp == NULL)
        nm_bug(_("%s: NULL pointer"), __func__);

    for (uint32_t i = 0 ;; i++)
    {
        if (*dp->stop)
            break;

        curs_set(0);
        mvaddch(dp->y, dp->x, spin_chars[i & 3]);
        refresh();
        nanosleep(&ts, NULL);
    }

    pthread_exit(NULL);
}
#endif

int nm_print_empty_fields(const nm_vect_t *v)
{
    nm_str_t msg = NM_INIT_STR;

    if (v->n_memb == 0)
        return NM_OK;

    NM_FORM_RESET();

    nm_str_alloc_text(&msg, _(NM_MSG_NULL_FLD));

    for (size_t n = 0; n < v->n_memb; n++)
        nm_str_append_format(&msg, " '%s'", (char *) v->data[n]);

    nm_warn(msg.data);
    nm_str_free(&msg);

    return NM_ERR;
}

int nm_form_name_used(const nm_str_t *name)
{
    int rc = NM_OK;

    nm_vect_t res = NM_INIT_VECT;
    nm_str_t query = NM_INIT_STR;

    nm_str_alloc_text(&query, "SELECT id FROM vms WHERE name='");
    nm_str_add_str(&query, name);
    nm_str_add_char(&query, '\'');

    nm_db_select(query.data, &res);
    if (res.n_memb > 0)
    {
        rc = NM_ERR;
        curs_set(0);
        nm_warn(_(NM_MSG_NAME_BUSY));
    }

    nm_vect_free(&res, NULL);
    nm_str_free(&query);

    return rc;
}

void nm_form_get_last(uint64_t *mac, uint32_t *vnc)
{
    nm_vect_t res = NM_INIT_VECT;

    if (vnc != NULL)
    {
        nm_db_select("SELECT mac,vnc FROM lastval", &res);
        *vnc = nm_str_stoui(res.data[1], 10);
    }
    else
    {
        nm_db_select("SELECT mac FROM lastval", &res);
    }

    *mac = nm_str_stoul(res.data[0], 10);

    nm_vect_free(&res, nm_str_vect_free_cb);
}

void nm_form_update_last_mac(uint64_t mac)
{
    nm_str_t query = NM_INIT_STR;

    nm_str_format(&query, "UPDATE lastval SET mac='%" PRIu64 "'", mac);
    nm_db_edit(query.data);

    nm_str_free(&query);
}

void nm_form_update_last_vnc(uint32_t vnc)
{
    nm_str_t query = NM_INIT_STR;

    vnc++;
    nm_str_format(&query, "UPDATE lastval SET vnc='%u'", vnc);
    nm_db_edit(query.data);

    nm_str_free(&query);
}

void nm_vm_free(nm_vm_t *vm)
{
    nm_str_free(&vm->name);
    nm_str_free(&vm->arch);
    nm_str_free(&vm->cpus);
    nm_str_free(&vm->memo);
    nm_str_free(&vm->srcp);
    nm_str_free(&vm->vncp);
    nm_str_free(&vm->ifs.driver);
    nm_str_free(&vm->drive.driver);
    nm_str_free(&vm->drive.size);
}

void nm_vm_free_boot(nm_vm_boot_t *vm)
{
    nm_str_free(&vm->tty);
    nm_str_free(&vm->bios);
    nm_str_free(&vm->mach);
    nm_str_free(&vm->initrd);
    nm_str_free(&vm->socket);
    nm_str_free(&vm->kernel);
    nm_str_free(&vm->cmdline);
    nm_str_free(&vm->inst_path);
}

/* vim:set ts=4 sw=4: */
