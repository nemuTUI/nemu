#ifndef EDIT_WINDOW_H_
#define EDIT_WINDOW_H_

#include "qemu_manage.h"

namespace QManager
{
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
      guest_t<VectorString> guest_old;
      guest_t<std::string> guest_new;
      std::string vm_name_;
      uint32_t ints_count;
  };
}// namespace QManager

#endif
