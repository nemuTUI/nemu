#ifndef CLONE_WINDOW_H_
#define CLONE_WINDOW_H_

#include "base_form_window.h"

namespace QManager {

class CloneVmWindow : public QMFormWindow
{
public:
    CloneVmWindow(
        const std::string &dbf, const std::string &vmdir, const std::string &vm_name,
        int height, int width, int starty = 3
    );
    virtual void Print();

private:
    void Gen_hdd();
    void Create_fields();
    void Config_fields_type();
    void Config_fields_buffer();
    void Print_fields_names();
    void Get_data_from_form();
    void Get_data_from_db();
    void Update_db_data();
    void Gen_iface_json();

private:
    std::string vm_name_;
    VectorString guest_old;
    guest_t guest_new;
    char hdd_ch;
};

} // namespace QManager

#endif
