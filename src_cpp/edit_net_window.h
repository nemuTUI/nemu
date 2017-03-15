#ifndef EDIT_NET_WINDOW_H_
#define EDIT_NET_WINDOW_H_

#include "base_form_window.h"

namespace QManager {

class EditNetWindow : public QMFormWindow
{
public:
    EditNetWindow(
        const std::string &dbf, const std::string &vmdir, const std::string &vm_name,
        int height, int width, int starty = 3
    );
    virtual void Print();

private:
    void Create_fields();
    void Config_fields_type();
    void Print_fields_names();
    void Get_data_from_form();
    void Get_data_from_db();
    void Gen_hdd();
    void Gen_iface_json();
    void Update_db_data();

private:
    std::string vm_name_;
    VectorString guest_old_ints;
    guest_t guest_new;
    MapStringVector ifs;
    VectorString iflist, all_ints;
    char **IfaceList;
};

} // namespace QManager

#endif
/* vim:set ts=4 sw=4 fdm=marker: */
