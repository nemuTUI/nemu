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

nm_form_t *nm_post_form(nm_window_t *w, nm_field_t **field, int begin_x)
{
    nm_form_t *form;
    int rows, cols; 

    form = new_form(field);
    if (form == NULL)
         nm_bug("%s: %s", __func__, strerror(errno));

    refresh();
    scale_form(form, &rows, &cols);
    set_form_win(form, w);
    set_form_sub(form, derwin(w, rows, cols, 2, begin_x));
    box(w, 0, 0);
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
        nm_print_warn(3, 2, _("Contents of field is invalid"));
    }

    return confirm;
}

void nm_get_field_buf(nm_field_t *f, nm_str_t *res)
{
    char *buf = field_buffer(f, 0);
    char *s = strrchr(buf, 0x20);

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
        nm_str_copy(&tmp, path);
        nm_str_add_char(&tmp, '*');
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
                    if ((file_info.st_mode & S_IFMT) == S_IFDIR)
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
            if ((file_info.st_mode & S_IFMT) == S_IFDIR)
                nm_str_add_char(path, '/');
        }
    }

out:
    if (rp)
        free(rp);
    globfree(&res);

    return rc;
}

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

const char *nm_form_select_drive(const nm_vect_t *drives)
{
    char *drive = NULL;
    nm_form_t *form = NULL;
    nm_window_t *window = NULL;
    nm_field_t *fields[2];
    nm_vect_t err = NM_INIT_VECT;
    nm_str_t buf = NM_INIT_STR;

    nm_print_title(_(NM_EDIT_TITLE));
    window = nm_init_window(7, 45, 3);
    init_pair(1, COLOR_BLACK, COLOR_WHITE);
    wbkgd(window, COLOR_PAIR(1));

    fields[0] = new_field(1, 30, 2, 1, 0, 0);
    set_field_back(fields[0], A_UNDERLINE);
    fields[1] = NULL;

    set_field_type(fields[0], TYPE_ENUM, drives->data, false, false);
    set_field_buffer(fields[0], 0, *drives->data);

    mvwaddstr(window, 1, 2, _("Select drive"));
    mvwaddstr(window, 4, 2, _("Drive"));

    form = nm_post_form(window, fields, 11);
    if (nm_draw_form(window, form) != NM_OK)
        goto out;

    nm_get_field_buf(fields[0], &buf);
    nm_form_check_data(_("Drive"), buf, err);

    if (nm_print_empty_fields(&err) == NM_ERR)
    {
        nm_vect_free(&err, NULL);
        goto out;
    }

    for (size_t n = 0; n < drives->n_memb; n++)
    {
        char *d = drives->data[n];
        if (nm_str_cmp_st(&buf, d) == NM_OK)
        {
            drive = d;
            break;
        }
    }

out:
    nm_form_free(form, fields);
    nm_str_free(&buf);

    return drive;
}

int nm_print_empty_fields(const nm_vect_t *v)
{
    int y = 1;
    size_t msg_len;

    if (v->n_memb == 0)
        return NM_OK;

    msg_len = mbstowcs(NULL, _(NM_FORM_EMPTY_MSG), strlen(_(NM_FORM_EMPTY_MSG)));

    nm_window_t *err_window = nm_init_window(4 + v->n_memb, msg_len + 2, 2);
    curs_set(0);
    box(err_window, 0, 0);
    mvwprintw(err_window, y++, 1, "%s", _(NM_FORM_EMPTY_MSG));

    for (size_t n = 0; n < v->n_memb; n++)
        mvwprintw(err_window, ++y, 1, "%s", (char *) v->data[n]);

    wrefresh(err_window);
    wgetch(err_window);

    delwin(err_window);

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
        nm_print_warn(3, 2, _(NM_FORM_VMNAME_MSG));
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

    nm_str_alloc_text(&query, "UPDATE lastval SET mac='");
    nm_str_format(&query, "%" PRIu64 "'", mac);

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

/* vim:set ts=4 sw=4 fdm=marker: */
