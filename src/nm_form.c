#include <nm_core.h>
#include <nm_form.h>
#include <nm_utils.h>

const char *nm_form_yes_no[] = {
    "yes","no", NULL
};

const char *nm_form_net_drv[] = {
    "virtio", "rtl8139", "e1000", NULL
};

const char *nm_form_drive_drv[] = {
    "ide", "scsi", "virtio", NULL
};

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

void nm_draw_form(nm_window_t *w, nm_form_t *form)
{
    int confirm = 0;
    int ch;
    nm_str_t buf = NM_INIT_STR;

    wtimeout(w, 500);

    while ((ch = wgetch(w)) != KEY_F(10))
    {
        if (confirm)
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

                nm_get_field_buf(form, &buf);
                /*std::string input = trim_field_buffer(field_buffer(current_field(form), 0), false);
                std::string result;

                if (nm_append_path(input, result))
                {
                    set_field_buffer(current_field(form), 0, result.c_str());
                    form_driver(form, REQ_END_FIELD);
                }*/
                nm_str_trunc(&buf, 0);
            }
            break;

        case KEY_F(2):
            confirm = 1;
            form_driver(form, REQ_VALIDATION);
            break;

        default:
            form_driver(form, ch);
            break;
        }
    }

    nm_str_free(&buf);
}

void nm_get_field_buf(nm_form_t *f, nm_str_t *res)
{
    char *buf = field_buffer(current_field(f), 0);
    char *s = strchr(buf, 0x20);

    if (s != NULL)
        *s = '\0';

    nm_str_add_text(res, buf);
}

#if 0
static int nm_append_path(const nm_str_t *path, nm_str_t *res)
{
    DIR *dp;
    struct dirent *dir_ent;
    std::string input_dir = input;
    std::string input_file = input;
    char *tdir = dirname(const_cast<char *>(input_dir.c_str()));
    char *file = basename(const_cast<char *>(input_file.c_str()));
    std::string dir = tdir, base;
    VectorString matched;
    struct stat file_info;

    if ((dp = opendir(tdir)) == NULL)
        return false;

    while ((dir_ent = readdir(dp)) != NULL)
    {
        if (memcmp(dir_ent->d_name, file, strlen(file)) == 0)
        {
            size_t path_len, arr_len;

            matched.push_back(dir_ent->d_name);

            if ((arr_len = matched.size()) > 1)
            {
                path_len = matched.at(arr_len - 2).size();
                size_t eq_ch_num = 0;
                size_t cur_path_len = strlen(dir_ent->d_name);

                for (size_t n = 0; n < path_len; n++)
                {
                    if (n > cur_path_len)
                        break;

                    if (matched.at(arr_len - 2).at(n) == dir_ent->d_name[n])
                        eq_ch_num++;
                    else
                        break;
                }

                base = matched.at(arr_len - 2).substr(0, eq_ch_num);
            }
        }
    }

    if (matched.size() == 0)
    {
        closedir(dp);
        return false;
    }

    if (matched.size() == 1)
    {
        if (dir == "/")
            result = dir + matched.at(0);
        else
            result = dir + '/' + matched.at(0);
    }
    else
    {
        if (dir == "/")
            result = dir + base;
        else
            result = dir + '/' + base;
    }

    if (stat(result.c_str(), &file_info) != -1)
    {
        if ((file_info.st_mode & S_IFMT) == S_IFDIR)
            result += '/';
    }

    closedir(dp);

    return true;
}
#endif

/* vim:set ts=4 sw=4 fdm=marker: */
