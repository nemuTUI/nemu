#ifndef ADD_DISK_WINDOW_H_
#define ADD_DISK_WINDOW_H_

#include "base_form_window.h"

namespace QManager {

class AddDiskWindow : public QMFormWindow
{
public:
    AddDiskWindow(
        const std::string &dbf, const std::string &vmdir, const std::string &vm_name,
        int height, int width, int starty = 3
    );
    virtual void Print();

private:
    void Gen_hdd();
    void Create_fields();
    void Config_fields_type();
    void Print_fields_names();
    void Get_data_from_form();
    void Get_data_from_db();
    void Update_db_data();

    std::string vm_name_;
    VectorString guest_old_disk;
    guest_t guest_new;
    char hdd_ch;
};

} // namespace QManager

#endif
