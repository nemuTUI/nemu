#include <nm_core.h>
#include <nm_form.h>
#include <nm_menu.h>
#include <nm_utils.h>
#include <nm_vector.h>
#include <nm_window.h>
#include <nm_database.h>
#include <nm_network.h>

#include <time.h>
#include <glob.h>
#include <dirent.h>

static const int ROW_HEIGHT = 1;
static const int FIELD_HPAD = 2;
static const int FIELD_VPAD = 1;
static const int FORM_HPAD = 2;
static const int FORM_VPAD = 1;
static const int MIN_EDIT_SIZE = 18;
static const float FORM_RATIO = 0.80;

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
    "nvme",
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
    "NEC-XHCI",
    NULL
};

const char *nm_form_displaytype[] = {
    "qxl",
    "virtio",
    NULL
};

void _nc_Free_Type(nm_field_t *field);
void _nc_Copy_Type(nm_field_t *dst, nm_field_t const *src );

static int nm_append_path(nm_str_t *path);
static nm_field_t *nm_field_resize(nm_field_t *field, nm_form_data_t *form_data);
static nm_form_t *nm_form_redraw(nm_form_t *form);

nm_field_t *nm_field_new(nm_field_type_t type, int row, nm_form_data_t *form_data)
{
    nm_field_data_t *field_data = NULL;
    nm_field_t *field = NULL;
    int height = ROW_HEIGHT;
    int top = (ROW_HEIGHT + form_data->field_vpad) * row;
    int width;
    int left;

    field_data = (nm_field_data_t *)nm_alloc(sizeof(nm_field_data_t));

    field_data->type = type;
    field_data->row = row;
    field_data->children.n_memb = 0;
    field_data->children.n_alloc = 0;
    field_data->children.data = NULL;
    field_data->on_change = NULL;

    switch (type) {
        case NM_FIELD_LABEL:
            width = form_data->msg_len;
            left = 0;
            break;
        case NM_FIELD_EDIT:
            width = (form_data->form_len - form_data->msg_len - form_data->field_hpad);
            left = (form_data->msg_len + form_data->field_hpad);
            break;
        case NM_FIELD_DEFAULT:
            width = form_data->form_len;
            left = 0;
            break;
        default:
            nm_bug("%s: %s", __func__, "Unknown field type");
            break;
    }

    field = new_field(height, width, top, left, 0, 0);
    if (field == NULL)
         nm_bug("%s: %s", __func__, strerror(errno));
    set_field_userptr(field, field_data);

    switch (type) {
        case NM_FIELD_LABEL:
            field_opts_off(field, O_ACTIVE);
            if (form_data->color) {
                set_field_fore(field, COLOR_PAIR(NM_COLOR_BLACK));
                set_field_back(field, COLOR_PAIR(NM_COLOR_BLACK));
            }
            break;
        case NM_FIELD_EDIT:
        case NM_FIELD_DEFAULT:
            field_opts_off(field, O_STATIC);
            break;
        default:
            nm_bug("%s: %s", __func__, "Unknown field type");
            break;
    }

    set_field_status(field, 0);
    return field;
}

static nm_field_t *nm_field_resize(nm_field_t *field, nm_form_data_t *form_data)
{
    nm_field_data_t *field_data = (nm_field_data_t *)field_userptr(field);
    nm_field_t *field_ = NULL;
    int height = ROW_HEIGHT;
    int top = (ROW_HEIGHT + form_data->field_vpad) * field_data->row;
    int width;
    int left;

    switch (field_data->type) {
        case NM_FIELD_LABEL:
            width = form_data->msg_len;
            left = 0;
            break;
        case NM_FIELD_EDIT:
            width = (form_data->form_len - form_data->msg_len - form_data->field_hpad);
            left = (form_data->msg_len + form_data->field_hpad);
            break;
        case NM_FIELD_DEFAULT:
            width = form_data->form_len;
            left = 0;
            break;
        default:
            nm_bug("%s: %s", __func__, "Unknown field type");
            break;
    }

    field_ = new_field(height, width, top, left, 0, 0);
    if (field_ == NULL)
         nm_bug("%s: %s", __func__, strerror(errno));

    set_field_userptr(field_, field_data);
    set_field_opts(field_, field_opts(field));
    set_field_buffer(field_, 0, field_buffer(field, 0));
    set_field_fore(field_, field_fore(field));
    set_field_back(field_, field_back(field));
    _nc_Free_Type(field_);
    _nc_Copy_Type(field_, field);
    set_field_status(field_, field_status(field));
    //@TODO update children somehow

    free_field(field);

    return field_;
}

void nm_field_free(nm_field_t *field)
{
    if (!field)
        return;

    nm_field_data_t *field_data = (nm_field_data_t *)field_userptr(field);
    if (field_data) {
        nm_vect_free(&field_data->children, NULL);
        free(field_data);
    }
    free_field(field);
}

void nm_fields_free(nm_field_t **fields)
{
    for (; *fields; fields++) {
        nm_field_free(*fields);
        *fields = NULL;
    }
}

void nm_fields_unset_status(nm_field_t **fields)
{
    for (; *fields; fields++)
        set_field_status(*fields, 0);
}

nm_form_data_t *nm_form_data_new(
    nm_window_t *parent, void (*on_redraw)(nm_form_t *),
    size_t msg_len, size_t field_lines, int color)
{
    nm_form_data_t *form_data = nm_alloc(sizeof(nm_form_data_t));
    form_data->parent_window = parent;
    form_data->form_window = NULL;
    form_data->on_redraw = on_redraw;
    form_data->msg_len = msg_len;
    form_data->field_lines = field_lines;
    form_data->color = color;

    form_data->field_hpad = FIELD_HPAD;
    form_data->field_vpad = FIELD_VPAD;
    form_data->form_hpad = FORM_HPAD;
    form_data->form_vpad = FORM_VPAD;
    form_data->form_ratio = FORM_RATIO;
    form_data->min_edit_size = MIN_EDIT_SIZE;

    form_data->w_rows = (ROW_HEIGHT + form_data->field_vpad) * field_lines \
        + form_data->form_vpad;
    form_data->w_start_y = NM_WINDOW_HEADER_HEIGHT;

    return form_data;
}

int nm_form_data_update(nm_form_data_t *form_data, size_t msg_len, size_t field_lines)
{
    size_t cols, rows;

    getmaxyx(form_data->parent_window, rows, cols);

    if (msg_len)
        form_data->msg_len = msg_len;
    if (field_lines)
        form_data->field_lines = field_lines;

    form_data->w_rows = (ROW_HEIGHT + form_data->field_vpad) * form_data->field_lines \
        + form_data->form_vpad;
    form_data->w_cols = cols * form_data->form_ratio;
    form_data->form_len = form_data->w_cols \
        - form_data->field_hpad  - form_data->form_hpad;
    form_data->w_start_x = ((1 - form_data->form_ratio) * cols) / 2;

    if (form_data->w_cols < (form_data->msg_len + form_data->min_edit_size) ||
        form_data->w_rows > rows - form_data->w_start_y - form_data->form_vpad) {
        nm_warn(_(NM_MSG_SMALL_WIN));
        return NM_ERR;
    }

    if (form_data->form_window)
        delwin(form_data->form_window);

    form_data->form_window = derwin(
        form_data->parent_window,
        form_data->w_rows, form_data->w_cols,
        form_data->w_start_y, form_data->w_start_x
    );

    return NM_OK;
}

void nm_form_data_free(nm_form_data_t *form_data)
{
    if (form_data) {
        if (form_data->form_window)
            delwin(form_data->form_window);
        free(form_data);
    }
}

nm_form_t *nm_form_new(nm_form_data_t *form_data, nm_field_t **field)
{
    nm_form_t* form;

    form = new_form(field);
    if (form == NULL)
         nm_bug("%s: %s", __func__, strerror(errno));

    set_form_userptr(form, form_data);
    set_form_win(form, form_data->form_window);

    return form;
}

void nm_form_window_init()
{
    nm_destroy_windows();
    endwin();
    refresh();
    nm_create_windows();
}

void nm_form_post(nm_form_t *form)
{
    int rows, cols;
    nm_form_data_t *form_data = (nm_form_data_t *)form_userptr(form);

    if (form_data->color)
        wbkgd(form_data->form_window, COLOR_PAIR(NM_COLOR_BLACK));

    scale_form(form, &rows, &cols);
    set_form_sub(form,
        derwin(form_data->form_window,
            rows, cols, form_data->form_vpad, form_data->form_hpad));
    post_form(form);
    curs_set(1);
}

int nm_form_draw(nm_form_t **form)
{
    nm_form_data_t *form_data = (nm_form_data_t *)form_userptr(*form);
    int confirm = NM_ERR, rc = NM_OK;
    int ch;
    nm_str_t buf = NM_INIT_STR;

    wtimeout(form_data->parent_window, 100);

    while (confirm != NM_OK && (ch = wgetch(form_data->parent_window)) != NM_KEY_ESC) {
        switch(ch) {
        case KEY_DOWN:
            form_driver(*form, REQ_VALIDATION);
            form_driver(*form, REQ_NEXT_FIELD);
            form_driver(*form, REQ_END_LINE);
            break;

        case KEY_UP:
            form_driver(*form, REQ_VALIDATION);
            form_driver(*form, REQ_PREV_FIELD);
            form_driver(*form, REQ_END_LINE);
            break;

        case KEY_LEFT:
            if (field_type(current_field(*form)) == TYPE_ENUM)
                form_driver(*form, REQ_PREV_CHOICE);
            else
                form_driver(*form, REQ_PREV_CHAR);
            break;

        case KEY_RIGHT:
            if (field_type(current_field(*form)) == TYPE_ENUM)
                form_driver(*form, REQ_NEXT_CHOICE);
            else
                form_driver(*form, REQ_NEXT_CHAR);
            break;

        case KEY_HOME:
            form_driver(*form, REQ_BEG_LINE);
            break;

        case KEY_END:
            form_driver(*form, REQ_END_LINE);
            break;

        case KEY_BACKSPACE:
        case 127:
            form_driver(*form, REQ_DEL_PREV);
            break;

        case 0x9: /* TAB KEY */
            if (field_type(current_field(*form)) != TYPE_REGEXP)
                break;
            {
                form_driver(*form, REQ_NEXT_FIELD);
                form_driver(*form, REQ_PREV_FIELD);
                form_driver(*form, REQ_END_FIELD);

                nm_get_field_buf(current_field(*form), &buf);

                if (nm_append_path(&buf) == NM_OK) {
                    set_field_buffer(current_field(*form), 0, buf.data);
                    form_driver(*form, REQ_END_FIELD);
                }
                nm_str_trunc(&buf, 0);
            }
            break;

        case KEY_PPAGE:
        case KEY_NPAGE:
            if (field_type(current_field(*form)) == TYPE_ENUM) {
                int drop_ch = 0, x, y;
                nm_window_t *drop;
                nm_panel_t *panel;
                nm_args_t *args = field_arg(current_field(*form));
                nm_menu_data_t list = NM_INIT_MENU_DATA;
                nm_vect_t values = NM_INIT_VECT;
                ssize_t list_len = getmaxy(form_data->parent_window) - 4;
                size_t max_len = 0;

                for (ssize_t n = 0; n < args->count; n++) {
                    const char *keyword = args->kwds[n];
                    size_t key_len = strlen(keyword);
                    if (max_len < key_len) {
                        max_len = key_len;
                    }
                    nm_vect_insert_cstr(&values, keyword);
                }

                getyx(form_data->parent_window, y, x);
                x = getbegx(form_sub(*form)) \
                    + form_data->msg_len + form_data->field_hpad;

                list.highlight = 1;
                list_len -= y;
                if (list_len < args->count)
                    list.item_last = list_len;
                else
                    list.item_last = list_len = args->count;
                list.v = &values;

                drop = newwin(list_len + 2, max_len + 4, y + 1, x);
                keypad(drop, TRUE);
                panel = new_panel(drop);
                for(;;) {
                    werase(drop);
                    if (redraw_window) {
                        hide_panel(panel);
                        del_panel(panel);
                        delwin(drop);

                        ((nm_form_data_t *)form_userptr(*form))->on_redraw(*form);
                        *form = nm_form_redraw(*form);
                        wtimeout(form_data->parent_window, 100);

                        getyx(form_data->parent_window, y, x);
                        x = getbegx(form_sub(*form)) \
                            + form_data->msg_len + form_data->field_hpad;

                        drop = newwin(list_len + 2, max_len + 4, y + 1, x);
                        keypad(drop, TRUE);
                        panel = new_panel(drop);

                        update_panels();
                        doupdate();
                        redraw_window = 0;
                    }
                    nm_menu_scroll(&list, list_len, drop_ch);
                    nm_print_dropdown_menu(&list, drop);
                    drop_ch =  wgetch(drop);
                    if (drop_ch == NM_KEY_ENTER || drop_ch == NM_KEY_ESC) {
                        break;
                    }
                }

                if (drop_ch == NM_KEY_ENTER) {
                    nm_args_t *cur_args = field_arg(current_field(*form));
                    set_field_buffer(current_field(*form), 0,
                            cur_args->kwds[(list.item_first + list.highlight) - 1]);
                }

                nm_vect_free(list.v, NULL);
                hide_panel(panel);
                update_panels();
                doupdate();
                curs_set(1);
                del_panel(panel);
                delwin(drop);
            }
            break;

        case NM_KEY_ENTER:
            confirm = NM_OK;
            if (form_driver(*form, REQ_VALIDATION) != E_OK)
                rc = NM_ERR;
            break;

        default:
            form_driver(*form, ch);
            break;
        }

        if (redraw_window) {
            ((nm_form_data_t *)form_userptr(*form))->on_redraw(*form);
            *form = nm_form_redraw(*form);
            wtimeout(form_data->parent_window, 100);
            redraw_window = 0;
        }
    }

    nm_str_free(&buf);

    if ((confirm == NM_OK) && (rc == NM_ERR)) {
        confirm = NM_ERR;
        NM_FORM_RESET();
        nm_warn(_(NM_MSG_BAD_CTX));
    }

    return confirm;
}

void nm_form_free(nm_form_t *form)
{
    if (form) {
        unpost_form(form);
        free_form(form);

        curs_set(0);
    }
}

static nm_form_t *nm_form_redraw(nm_form_t *form)
{
    nm_form_t *form_;
    nm_form_data_t *form_data = (nm_form_data_t *)form_userptr(form);
    nm_field_t **fields = form_fields(form);
    nm_field_t *cur_field = current_field(form);
    int maxfield = form->maxfield;

    form_driver(form, REQ_VALIDATION);

    if (nm_form_data_update(form_data, form_data->msg_len, form_data->field_lines) != NM_OK) {
        mvwaddstr(form_data->parent_window, 3, 1, _("Window too small"));
        wrefresh(form_data->parent_window);
        return form;
    }

    unpost_form(form);
    free_form(form);

    for (int n = 0; n < maxfield; n++) {
        if(fields[n] == cur_field) {
            fields[n] = nm_field_resize(fields[n], form_data);
            cur_field = fields[n];
        } else {
            fields[n] = nm_field_resize(fields[n], form_data);
        }
    }

    form_ = nm_form_new(form_data, fields);
    if (form_ == NULL)
         nm_bug("%s: %s", __func__, strerror(errno));

    nm_form_post(form_);
    if (cur_field) {
        set_current_field(form_, cur_field);
        form_driver(form_, REQ_END_LINE);
    }

    return form_;
}

void nm_get_field_buf(nm_field_t *f, nm_str_t *res)
{
    char *buf, *s;

    if ((buf = field_buffer(f, 0)) == NULL)
        nm_bug("%s: %s", __func__, strerror(errno));

    s = strrchr(buf, 0x20);

    if (s != NULL) {
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

    if (path->data[0] == '~') {
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

    if (glob(path->data, 0, NULL, &res) != 0) {
        nm_str_t tmp = NM_INIT_STR;

        nm_str_format(&tmp, "%s*", path->data);
        if (glob(tmp.data, 0, NULL, &res) == 0) {
            nm_str_trunc(path, 0);
            if ((rp = realpath(res.gl_pathv[0], NULL)) != NULL) {
                nm_str_add_text(path, rp);
                nm_str_free(&tmp);
                rc = NM_OK;
                if (stat(path->data, &file_info) != -1) {
                    if (S_ISDIR(file_info.st_mode) && path->len > 1)
                        nm_str_add_char(path, '/');
                }
                goto out;
            }
        }

        nm_str_free(&tmp);
        goto out;
    }

    if ((rp = realpath(res.gl_pathv[0], NULL)) != NULL) {
        nm_str_trunc(path, 0);
        nm_str_add_text(path, rp);
        rc = NM_OK;
        if (stat(path->data, &file_info) != -1) {
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

    for (;;) {
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

void *nm_file_progress(void *data)
{
    struct timespec ts;
    int cols = getmaxx(action_window);
    nm_spinner_data_t *dp = data;
    const nm_vm_t *vm = dp->ctx;
    nm_str_t dst = NM_INIT_STR;
    struct stat img_info, dst_info;

    memset(&ts, 0x0, sizeof(ts));
    ts.tv_nsec = 1e+8;

    stat(vm->srcp.data, &img_info);

    nm_str_format(&dst, "%s/%s/%s_a.img",
            nm_cfg_get()->vm_dir.data, vm->name.data, vm->name.data);

    curs_set(0);

    for (;;) {
        int64_t perc;
        memset(&dst_info, 0x0, sizeof(dst_info));

        if (*dp->stop)
            break;

        stat(dst.data, &dst_info);
        perc = (dst_info.st_size * 100) / img_info.st_size;
        NM_ERASE_TITLE(action, cols);
        mvwprintw(action_window, 1, 2, "%d%% %ldmb/%ldmb",
                perc, dst_info.st_size / 1024, img_info.st_size / 1024);
        wrefresh(action_window);

        nanosleep(&ts, NULL);
    }

    nm_str_free(&dst);
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

    for (uint32_t i = 0 ;; i++) {
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

//@TODO Does this work at all?
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
    if (res.n_memb > 0) {
        rc = NM_ERR;
        curs_set(0);
        nm_warn(_(NM_MSG_NAME_BUSY));
    }

    nm_vect_free(&res, NULL);
    nm_str_free(&query);

    return rc;
}

uint64_t nm_form_get_last_mac()
{
    uint64_t mac;
    nm_vect_t res = NM_INIT_VECT;

    nm_db_select("SELECT COALESCE(MAX(mac_addr), 'de:ad:be:ef:00:00') FROM ifaces", &res);

    mac = nm_net_mac_s2n(res.data[0]);

    nm_vect_free(&res, nm_str_vect_free_cb);

    return mac;
}

uint32_t nm_form_get_free_vnc()
{
    uint32_t vnc;
    nm_vect_t res = NM_INIT_VECT;

    nm_db_select("\
SELECT DISTINCT vnc + 1 FROM vms \
UNION SELECT 0 EXCEPT SELECT DISTINCT vnc FROM vms \
ORDER BY vnc ASC LIMIT 1", &res);

    vnc = nm_str_stoui(res.data[0], 10);

    return vnc;
}

void nm_vm_free(nm_vm_t *vm)
{
    nm_str_free(&vm->name);
    nm_str_free(&vm->arch);
    nm_str_free(&vm->cpus);
    nm_str_free(&vm->memo);
    nm_str_free(&vm->srcp);
    nm_str_free(&vm->vncp);
    nm_str_free(&vm->mach);
    nm_str_free(&vm->cmdappend);
    nm_str_free(&vm->group);
    nm_str_free(&vm->usb_type);
    nm_str_free(&vm->ifs.driver);
    nm_str_free(&vm->drive.driver);
    nm_str_free(&vm->drive.size);
}

void nm_vm_free_boot(nm_vm_boot_t *vm)
{
    nm_str_free(&vm->bios);
    nm_str_free(&vm->initrd);
    nm_str_free(&vm->kernel);
    nm_str_free(&vm->cmdline);
    nm_str_free(&vm->inst_path);
}

/* vim:set ts=4 sw=4: */
