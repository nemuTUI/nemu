#ifndef BASE_FORM_WINDOW_H_
#define BASE_FORM_WINDOW_H_

#include "window.h"

namespace QManager {

class QMFormWindow : public QMWindow
{  
public:
    QMFormWindow(int height, int width, int starty = 1)
        : QMWindow(height, width, starty) {}
    virtual void Print() = 0;
    virtual ~QMFormWindow() {};

protected:
    void Delete_form();
    void Draw_form();
    void Draw_title();
    void Enable_color();
    void Post_form(uint32_t size);
    void ExeptionExit(QMException &err);
    void Gen_mac_address(
        struct guest_t<std::string> &guest, uint32_t int_count, const std::string &vm_name
    );

protected:
    std::string sql_query, s_last_mac,
    dbf_, vmdir_, guest_dir, create_guest_dir_cmd, create_img_cmd;
    uint32_t last_vnc, ui_vm_ints;
    uint64_t last_mac;
    VectorString v_last_vnc, v_last_mac, v_name;
    MapString ifaces;
    MapString::iterator itm;
    std::string::iterator its;
    FORM *form;
    std::vector<FIELD*> field;
    int ch, cmd_exit_status;
    MapString u_dev;
    char **UdevList, **ArchList;
    guest_t<std::string> guest;
};

} // namespace QManager

#endif
