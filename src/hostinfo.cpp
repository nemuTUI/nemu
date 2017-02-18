#include "hostinfo.h"

uint32_t QManager::total_memory()
{
    meminfo();
    return kb_main_total / 1024;
}

uint32_t QManager::disk_free(const std::string &vmdir)
{
    struct statvfs df;
    statvfs(vmdir.c_str(), &df);

    return (df.f_bsize * df.f_bavail) / 1024 / 1024 / 1024;
}

uint32_t QManager::cpu_count()
{
    uint32_t cores = std::thread::hardware_concurrency();

    return cores;
}

QManager::MapString QManager::list_usb()
{
    MapString u_list;
    char buff[1024] = {0};
    std::string regex(".*\\s+ID\\s+([A-Fa-f0-9]{4}:[A-Fa-f0-9]{4})\\s+(.*\\S)\\s*\n");

    FILE *cmd = popen("lsusb", "r");
    
    if (cmd)
    {
        while (char *line = fgets(buff, sizeof(buff), cmd))
        {
            boost::regex re(regex, boost::regex_constants::extended);
            boost::smatch m;
            
            if (boost::regex_match(std::string(line), m, re))
            {
                std::string usb_id = m[1].str();
                std::string usb_name = m[2].str();
                u_list.insert(std::make_pair(usb_name, usb_id));
            }
        }
        pclose(cmd);
        return u_list;
    }

    return MapString();
}
