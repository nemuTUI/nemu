#include <iostream>
#include <pwd.h>
#include <libgen.h>
#include <dirent.h>
#include <regex.h>
#include <string.h>
#include <errno.h>

#include <sys/stat.h>

#include "misc.h"

namespace QManager {

static struct config config;

std::atomic<bool> finish(false);

static void generate_cfg(const char *dir, const std::string &path);
static void read_user_value(const char *msg, const std::string &def, std::string &result);

void init_cfg()
{
    struct passwd *pw = getpwuid(getuid());
    std::string cfg_path;
    struct stat file_info;

    if (!pw)
    {
        fprintf(stderr, "Error get home directory: %s\n", strerror(errno));
        exit(1);
    }

    cfg_path = pw->pw_dir + std::string("/.nemu.cfg");
    generate_cfg(pw->pw_dir, cfg_path);

    try
    {
        boost::property_tree::ptree ptr;
        boost::property_tree::ini_parser::read_ini(cfg_path, ptr);

        config.vmdir = ptr.get<std::string>("main.vmdir");
        if (config.vmdir.empty())
            throw std::runtime_error("empty VM directory value");
        if (stat(config.vmdir.c_str(), &file_info) == -1)
            throw std::runtime_error(config.vmdir + " : " + strerror(errno));
        if ((file_info.st_mode & S_IFMT) != S_IFDIR)
            throw std::runtime_error("VM directory is not directory");
        if (access(config.vmdir.c_str(), W_OK) != 0)
            throw std::runtime_error("no write access on VM directory");

        config.db = ptr.get<std::string>("main.db");
        if (config.db.empty())
            throw std::runtime_error("empty database file value");
        std::string t_db(config.db.data(), config.db.length());
        if (access(dirname(const_cast<char *>(t_db.c_str())), W_OK) != 0)
            throw std::runtime_error("no write access on DB directory");

        config.list_max = ptr.get<uint32_t>("main.list_max");
        if ((config.list_max == 0) || (config.list_max > 100))
            throw std::runtime_error("invalid list_max value");

        config.vnc_bin = ptr.get<std::string>("vnc.binary");
        if (config.vnc_bin.empty())
            throw std::runtime_error("empty vnc viewer value");
        if (stat(config.vnc_bin.c_str(), &file_info) == -1)
            throw std::runtime_error(config.vnc_bin + " : " + strerror(errno));

        config.vnc_listen_any = ptr.get<bool>("vnc.listen_any");

        config.qemu_system_path = ptr.get<std::string>("qemu.qemu_system_path");
        if (config.qemu_system_path.empty())
            throw std::runtime_error("empty qemu_system_path value");

        std::string q_targets = ptr.get<std::string>("qemu.targets");
        if (q_targets.empty())
            throw std::runtime_error("empty qemu targets value");

        char *token;
        char *saveptr = const_cast<char *>(q_targets.c_str());

        while ((token = strtok_r(saveptr, ",", &saveptr)))
        {
            std::string qemu_sys_bin = config.qemu_system_path + "-" + token;
            if (stat(qemu_sys_bin.c_str(), &file_info) == -1)
                throw std::runtime_error(qemu_sys_bin + " : " + strerror(errno));

            config.qemu_targets.push_back(token);
        }

        config.log_path = ptr.get<std::string>("qemu.log_cmd");
        if (!config.log_path.empty())
        {
            std::string t_log(config.log_path.data(), config.log_path.length());
            if (access(dirname(const_cast<char *>(t_log.c_str())), W_OK) != 0)
                throw std::runtime_error("no write access on log directory");
        }
    }
    catch (boost::property_tree::ptree_bad_path &err)
    {
        fprintf(stderr, "Error: %s\n", err.what());
        exit(2);
    }
    catch (boost::property_tree::ptree_bad_data &err)
    {
        fprintf(stderr, "Error: %s\n", err.what());
        exit(3);
    }
    catch (boost::property_tree::ini_parser::ini_parser_error &err)
    {
        fprintf(stderr, "Error: %s\n", err.what());
        exit(4);
    }
    catch (const std::runtime_error &err)
    {
        fprintf(stderr, "Error: %s\n", err.what());
        exit(5);
    }
}

static void generate_cfg(const char *dir, const std::string &path)
{
    struct stat file_info;

    if (stat(path.c_str(), &file_info) == 0)
        return;

    printf(_("Config file \"%s\" is not found.\n"), path.c_str());
    printf(_("You can copy example from:\n"));
    printf("%s/share/nemu/templates/config/nemu.cfg.sample\n", STRING(USR_PREFIX));
    printf(_("and edit it manually or let the program generate it.\n\n"));
    printf(_("Generate cfg? (y/n)\n> "));

    switch (getchar()) {
    case 'y':
    case 'Y':
        {
            FILE *cfg;
            std::string vmdir;
            std::string db;
            std::string vnc;
            std::string targets;
            bool dir_created = false;

            if ((cfg = fopen(path.c_str(), "w+")) == NULL)
            {
                fprintf(stderr, "Cannot create file: %s\n", path.c_str());
                exit(1);
            }

            std::cin.ignore();
            while (!dir_created)
            {
                read_user_value(_("VM storage directory"),
                    std::string(dir) + "/qemu_vm", vmdir);
                if (stat(vmdir.c_str(), &file_info) != 0)
                {
                    if (mkdir(vmdir.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) != 0)
                    {
                        printf(_("Cannot create VM storage directory %s: %s\n"),
                            vmdir.c_str(), ::strerror(errno));
                        printf(_("Try again? (y/n)\n> "));
                        switch (getchar()) {
                        case 'y':
                        case 'Y':
                            std::cin.ignore();
                            break;
                        default:
                            exit(1);
                        }
                    }
                    else
                        dir_created = true;
                }
                else
                    dir_created = true;
            }
            read_user_value(_("VM settings database path"),
                std::string(dir) + "/.nemu.db", db);
            read_user_value(_("Path to VNC client (enter \"/bin/false\" if you connect other way)"),
                "/usr/bin/vncviewer", vnc);
            read_user_value(_("Qemu system targets list, separated by comma"),
                "x86_64,i386", targets);

            fprintf(cfg, "[main]\n# VM storage directory\nvmdir = %s\n\n", vmdir.c_str());
            fprintf(cfg, "# VM settings database path\ndb = %s\n\n", db.c_str());
            fprintf(cfg, "# maximum guests numbers in list.\nlist_max = 10\n\n");
            fprintf(cfg, "[vnc]\n# Path to VNC client\nbinary = %s\n\n", vnc.c_str());
            fprintf(cfg, "# listen for vnc connections"
                " (0 = only localhost, 1 = any address).\nlisten_any = 0\n\n");
            fprintf(cfg, "[qemu]\n# path to qemu system binary without arch prefix.\n"
                "qemu_system_path = /usr/bin/qemu-system\n\n");
            fprintf(cfg, "# Qemu system targets list, separated by comma.\n"
                "targets = %s\n\n", targets.c_str());
            fprintf(cfg, "# Log last qemu command. Empty value disable logging.\n"
                "log_cmd = /tmp/qemu_last_cmd.log\n");
            fclose(cfg);
        }
        break;

    default:
        exit(1);
    }
}

static void read_user_value(const char *msg, const std::string &def, std::string &result)
{
        printf(_("%s [default: %s]\n> "), msg, def.c_str());
        std::getline(std::cin, result);
        if (result.empty())
            result = def;
}

const struct config *get_cfg()
{
    return &config;
}

std::string trim_field_buffer(char *buff, bool check)
{
    std::string field(buff);
    field.erase(field.find_last_not_of(" ") + 1);

    if (field.empty() && check)
        throw std::runtime_error("empty value");

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

    try
    {
        for (auto &i : ptr.get_child("ifaces"))
        {
            std::vector<std::string> values;
            std::string key = i.second.get<std::string>("name");
            values.push_back(i.second.get<std::string>("mac"));
            values.push_back(i.second.get<std::string>("drv"));
            values.push_back(i.second.get<std::string>("ip4"));
            result.insert(std::make_pair(key, values));
        }
    }
    catch (const boost::property_tree::ptree_bad_path &e)
    {
        err_exit(e.what(), _("Database error"));
    }

    return result;
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
    const char *regex = "^([0-9a-fA-F]{2}:){5}[0-9a-fA-F]{2}$";
    regex_t reg;
    bool result = false;

    if (regcomp(&reg, regex, REG_EXTENDED) != 0)
    {
        /* TODO log error */
        return false;
    }

    if (regexec(&reg, mac.c_str(), 0, NULL, 0) == 0)
        result = true;

    regfree(&reg);

    return result;
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

bool append_path(const std::string &input, std::string &result)
{
    DIR *dp;
    struct dirent *dir_ent;
    std::string input_dir = input;
    std::string input_file = input;
    char *tdir = dirname(const_cast<char *>(input_dir.c_str()));
    char *file = basename(const_cast<char *>(input_file.c_str()));
    std::string dir = tdir, base;
    VectorString matched;
    struct stat file_info;

    if ((dp = opendir(tdir)) == NULL)
        return false;

    while ((dir_ent = readdir(dp)) != NULL)
    {
        if (memcmp(dir_ent->d_name, file, strlen(file)) == 0)
        {
            size_t path_len, arr_len;

            matched.push_back(dir_ent->d_name);

            if ((arr_len = matched.size()) > 1)
            {
                path_len = matched.at(arr_len - 2).size();
                size_t eq_ch_num = 0;
                size_t cur_path_len = strlen(dir_ent->d_name);

                for (size_t n = 0; n < path_len; n++)
                {
                    if (n > cur_path_len)
                        break;

                    if (matched.at(arr_len - 2).at(n) == dir_ent->d_name[n])
                        eq_ch_num++;
                    else
                        break;
                }

                base = matched.at(arr_len - 2).substr(0, eq_ch_num);
            }
        }
    }

    if (matched.size() == 0)
    {
        closedir(dp);
        return false;
    }

    if (matched.size() == 1)
    {
        if (dir == "/")
            result = dir + matched.at(0);
        else
            result = dir + '/' + matched.at(0);
    }
    else
    {
        if (dir == "/")
            result = dir + base;
        else
            result = dir + '/' + base;
    }

    if (stat(result.c_str(), &file_info) != -1)
    {
        if ((file_info.st_mode & S_IFMT) == S_IFDIR)
            result += '/';
    }

    closedir(dp);

    return true;
}

} // namespace QManager
/* vim:set ts=4 sw=4 fdm=marker: */
