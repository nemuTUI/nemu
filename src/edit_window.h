#ifndef EDIT_WINDOW_H_
#define EDIT_WINDOW_H_

#include "base_form_window.h"

namespace QManager {

class EditVmWindow : public QMFormWindow
{
public:
    EditVmWindow(
        const std::string &dbf, const std::string &vmdir, const std::string &vm_name,
        int height, int width, int starty = 3
    );
    virtual void Print();

private:
    void Create_fields();
    void Config_fields_type();
    void Config_fields_buffer();
    void Print_fields_names();
    void Get_data_from_form();
    void Get_data_from_db();
    void Update_db_data();
    void Gen_iface_json();

private:
    VectorString guest_old;
    guest_t guest_new;
    std::string vm_name_;
    uint32_t ints_count;
};

} // namespace QManager

#endif
