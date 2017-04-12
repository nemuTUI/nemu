#include <nm_core.h>
#include <nm_form.h>
#include <nm_utils.h>
#include <nm_vector.h>
#include <nm_window.h>
#include <nm_database.h>

#include <time.h>
#include <dirent.h>

const char *nm_form_yes_no[] = {
    "yes","no", NULL
};

const char *nm_form_net_drv[] = {
    "virtio", "rtl8139", "e1000", NULL
};

const char *nm_form_drive_drv[] = {
    "ide", "scsi", "virtio", NULL
};

static int nm_append_path(nm_str_t *path);

void nm_form_free(nm_form_t *form, nm_field_t **fields)
{
    if (fields == NULL)
        nm_bug(_("%s NULL pointer fields"), __func__);
    
    unpost_form(form);
    free_form(form);

    for (; *fields; fields++)
        free_field(*fields);

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
    int confirm = NM_ERR;
    int ch;
    nm_str_t buf = NM_INIT_STR;

    wtimeout(w, 500);

    while ((ch = wgetch(w)) != KEY_F(10))
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

        case KEY_F(2):
            confirm = NM_OK;
            form_driver(form, REQ_VALIDATION);
            break;

        default:
            form_driver(form, ch);
            break;
        }
    }

    nm_str_free(&buf);

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
    DIR *dp;
    struct dirent *dir_ent;
    nm_str_t dir = NM_INIT_STR;
    nm_str_t file = NM_INIT_STR;
    nm_str_t base = NM_INIT_STR;
    nm_vect_t matched = NM_INIT_VECT;
    struct stat file_info;
    int rc = NM_OK;

    nm_str_dirname(path, &dir);
    nm_str_basename(path, &file);

    nm_str_trunc(path, 0);

    if ((dp = opendir(dir.data)) == NULL)
    {
        rc = NM_ERR;
        goto out;
    }

    while ((dir_ent = readdir(dp)) != NULL)
    {
        if (memcmp(dir_ent->d_name, file.data, file.len) == 0)
        {
            size_t path_len, arr_len;

            nm_vect_insert(&matched, dir_ent->d_name,
                           strlen(dir_ent->d_name) + 1, NULL);

            if ((arr_len = matched.n_memb) > 1)
            {
                path_len = strlen((char *) matched.data[arr_len - 2]);
                size_t eq_ch_num = 0;
                size_t cur_path_len = strlen(dir_ent->d_name);

                for (size_t n = 0; n < path_len; n++)
                {
                    if (n > cur_path_len)
                        break;

                    if (((char *) matched.data[arr_len - 2])[n] == dir_ent->d_name[n])
                        eq_ch_num++;
                    else
                        break;
                }

                ((char *) matched.data[arr_len - 2])[eq_ch_num] = '\0';
                nm_str_alloc_text(&base, (char *) matched.data[arr_len - 2]);
            }
        }
    }

    if (matched.n_memb == 0)
    {
        closedir(dp);
        rc = NM_ERR;
        goto out;
    }

    nm_str_copy(path, &dir);

    if (matched.n_memb == 1)
    {

        if ((dir.len == 1) && (*dir.data == '/'))
        {
            nm_str_add_text(path, *matched.data);
        }
        else
        {
            nm_str_add_char(path, '/');
            nm_str_add_text(path, *matched.data);
        }
    }
    else
    {
        if ((dir.len == 1) && (*dir.data == '/'))
        {
            nm_str_add_str(path, &base);
        }
        else
        {
            nm_str_add_char(path, '/');
            nm_str_add_str(path, &base);
        }
    }

    if (stat(path->data, &file_info) != -1)
    {
        if ((file_info.st_mode & S_IFMT) == S_IFDIR)
            nm_str_add_char(path, '/');
    }

    closedir(dp);

out:
    nm_str_free(&dir);
    nm_str_free(&file);
    nm_str_free(&base);
    nm_vect_free(&matched, NULL);

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
        *vnc = nm_str_stoui(res.data[1]);
    }
    else
    {
        nm_db_select("SELECT mac FROM lastval", &res);
    }

    *mac = nm_str_stoul(res.data[0]);

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
    nm_str_free(&vm->usb.device);
    nm_str_free(&vm->ifs.driver);
    nm_str_free(&vm->drive.driver);
    nm_str_free(&vm->drive.size);
}

void nm_vm_free_boot(nm_vm_boot_t *vm)
{
    nm_str_free(&vm->tty);
    nm_str_free(&vm->bios);
    nm_str_free(&vm->socket);
    nm_str_free(&vm->kernel);
    nm_str_free(&vm->cmdline);
    nm_str_free(&vm->inst_path);
}

/* vim:set ts=4 sw=4 fdm=marker: */
