#include "misc.h"

namespace QManager {

static struct config config;

std::atomic<bool> finish(false);

void init_cfg()
{
/*
    struct passwd *pw = getpwuid(getuid());

    if (!pw)
        err_exit("Error get home directory", (std::string) strerror(errno));

    std::string home_dir = pw->pw_dir;

    try
    {
        boost::property_tree::ptree ptr;
        boost::property_tree::ini_parser::read_ini(cfg, ptr);
        value = ptr.get<T>(param);
    }
    catch (boost::property_tree::ptree_bad_path &err)
    {
        err_exit("Error parsing config file! missing parameter.", (std::string) err.what());
    }
    catch (boost::property_tree::ptree_bad_data &err)
    {
        err_exit("Error parsing config file! bad value.", (std::string) err.what());
    }
    catch (boost::property_tree::ini_parser::ini_parser_error &err)
    {
        err_exit("Config file not found!", (std::string) err.what());
    }*/
}

const struct config *get_cfg()
{
    return &config;
}

std::string trim_field_buffer(char *buff)
{
    std::string field(buff);
    field.erase(field.find_last_not_of(" ") + 1);

    return field;
}

MapString gen_mac_addr(uint64_t &mac,
                       uint32_t &int_num,
                       const std::string &vm_name)
{
    MapString ifaces;
    char buff[64] = {0};
    std::string if_name;

    for (uint32_t i=0, n=1; i<int_num; ++i, ++n)
    {
        uint64_t m = mac + n;
        int pos = 0;

        for (size_t byte = 0; byte < 6; ++byte)
        {
            uint32_t octet = ((m >> 40) & 0xff);

            pos += snprintf(buff + pos, sizeof(buff) - pos, "%02x:", octet);
            m <<= 8;
        }

        buff[--pos] = '\0';

        if_name = vm_name + "_eth" + std::to_string(i); //TODO cut guest name to 9 characters if > 9 !!!

        ifaces.insert(std::make_pair(if_name, buff));
    }

    return ifaces;
}

MapString Gen_map_from_str(const std::string &str)
{
    MapString result;
    typedef boost::tokenizer<boost::char_separator<char> > tokenizer;

    boost::char_separator<char> sep("=;");
    tokenizer tokens(str, sep);

    for (tokenizer::iterator tok_iter = tokens.begin(); tok_iter != tokens.end();)
    {
        std::string key = *tok_iter++;
        
        if (tok_iter == tokens.end())
            return MapString();
        std::string val = *tok_iter++;

        result.insert(std::make_pair(key, val));
    }

    return result;
}

MapStringVector Read_ifaces_from_json(const std::string &str)
{
    MapStringVector result;

    std::istringstream ss(str);
    boost::property_tree::ptree ptr;
    boost::property_tree::json_parser::read_json(ss, ptr);

    for (auto &i : ptr.get_child("ifaces"))
    {
        std::vector<std::string> values;
        std::string key = i.second.get<std::string>("name");
        values.push_back(i.second.get<std::string>("mac"));
        values.push_back(i.second.get<std::string>("drv"));
        result.insert(std::make_pair(key, values));
    }

    return result;
}

bool check_root()
{
    uid_t uid = getuid();
    if (uid != 0)
        return false;

    return true;
}

void err_exit(const char *msg, const std::string &err)
{
    clear();
    mvprintw(2, 3, "%s", msg);
    mvprintw(3, 3, "%s", err.c_str());
    getch();
    refresh();
    endwin();
    exit(1);
}

bool verify_mac(const std::string &mac)
{
    const std::string regex = ("^([0-9a-fA-F]{2}:){5}[0-9a-fA-F]{2}$");

    boost::regex re(regex, boost::regex_constants::extended);
    boost::smatch m;

    if (boost::regex_match(std::string(mac), m, re))
        return true;
    
    return false;
}


void spinner(uint32_t pos_x, uint32_t pos_y)
{
    static char const spin_chars[] ="/-\\|";

    for (uint32_t i = 0 ;; i++)
    {
        if (finish)
            break;

        curs_set(0);
        mvaddch(pos_x, pos_y, spin_chars[i & 3]);
        refresh();
        usleep(100000);
    }
}

} // namespace QManager
