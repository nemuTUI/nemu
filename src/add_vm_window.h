#ifndef ADD_VM_WINDOW_H_
#define ADD_VM_WINDOW_H_

#include "base_form_window.h"

namespace QManager {

class AddVmWindow : public QMFormWindow
{
public:
    AddVmWindow(const std::string &dbf, const std::string &vmdir,
                int height, int width, int starty = 3);
    virtual void Print();

private:
    void Check_input_data();
    void Gen_hdd();
    void Create_fields();
    void Config_fields_buffer();
    void Config_fields_type();
    void Print_fields_names();
    void Get_data_from_form();
    void Get_data_from_db();
    void Update_db_data();
    void Gen_iface_json();

    VectorString q_arch;
};

} // namespace QManager

#endif
