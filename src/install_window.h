#ifndef INSTALL_WINDOW_H_
#define INSTALL_WINDOW_H_

#include "base_form_window.h"

namespace QManager {

class EditInstallWindow : public QMFormWindow
{
public:
    EditInstallWindow(const std::string &dbf, const std::string &vmdir,
                      const std::string &vm_name, int height, int width, 
                      int starty = 3);
    virtual void Print();

private:
    void Create_fields();
    void Config_fields_type();
    void Config_fields_buffer();
    void Print_fields_names();
    void Get_data_from_form();
    void Get_data_from_db();
    void Update_db_data();

    std::string vm_name_;
    VectorString guest_old;
    guest_t guest_new;
};

} // namespace QManager

#endif
/* vim:set ts=4 sw=4 fdm=marker: */
