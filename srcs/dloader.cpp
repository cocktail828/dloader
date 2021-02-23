/*
 * @Author: sinpo828
 * @Date: 2021-02-07 12:21:12
 * @LastEditors: sinpo828
 * @LastEditTime: 2021-02-23 18:17:37
 * @Description: file content
 */
#include <iostream>
#include <fstream>
#include <thread>

extern "C"
{
#include <getopt.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
}

#include "firmware.hpp"
#include "upgrade_manager.hpp"
#include "devices.hpp"
#include "config.hpp"
#include "common.hpp"

using namespace std;

#define _STRINGFY(v) #v
#define STRINGFY(v) _STRINGFY(v)
#define VERSION_STR     \
    STRINGFY(MAJOR_VER) \
    "." STRINGFY(MINOR_VER) "." STRINGFY(REVISION_VER)

static configuration config;

void usage(const char *prog)
{
    cerr << prog << " version " << VERSION_STR << endl;
    cerr << prog << " [config] [options]" << endl;
    cerr << "    -f pac_file           firmware file, with suffix of '.pac'" << endl;
    cerr << "    -d device             tty device, example(/dev/ttyUSB0)" << endl;
    cerr << "    -p usb port           usb port, it's port string, see '-l' for details" << endl;
    cerr << "    -x pac_file [dir]     exract pac_file only" << endl;
    cerr << "    -c chip_set           udx710(5g) or uix8910(4g)" << endl;
    cerr << "    -l                    list devices" << endl;
    cerr << "    -h                    help message" << endl;
}

/**
 * walk dir to find *.pac
 */
string auto_find_pac(const string path)
{
    struct dirent *entptr = NULL;
    DIR *dirptr = NULL;

    dirptr = opendir(path.c_str());
    if (!dirptr)
    {
        cerr << "cannot open dir " << path << " for " << strerror(errno) << endl;
        return "";
    }

    string choose_file;
    struct timespec modtime = {0, 0};
    while ((entptr = readdir(dirptr)))
    {

        struct stat _stat;
        if (entptr->d_name[0] == '.' ||
            strncasecmp(entptr->d_name + strlen(entptr->d_name) - 4, ".pac", 4))
            continue;

        string fpath = config.pac_path + "/" + entptr->d_name;
        stat(fpath.c_str(), &_stat);
        if ((_stat.st_mtim.tv_sec > modtime.tv_sec) ||
            (_stat.st_mtim.tv_sec == modtime.tv_sec &&
             _stat.st_mtim.tv_nsec > modtime.tv_nsec))
        {
            choose_file = fpath;
            modtime = _stat.st_mtim;
            break;
        }
    }
    closedir(dirptr);

    return choose_file;
}

void auto_find_dev(const string &port)
{
    Device dev;
    int try_time = 0;
    int max_try_time = 15;

    for (; try_time < max_try_time; try_time++)
    {
        dev.reset();
        dev.scan(port);

        for (auto iter = config.devs.begin(); iter != config.devs.end(); iter++)
        {
            auto d = dev.get_interface(iter->usbid, iter->usbif).ttyusb;
            if (!dev.exist(iter->usbid, iter->usbif))
                continue;
            iter->use_flag = true;

            config.device = d.empty() ? "" : ("/dev/" + d);
            config.chipset = iter->chipset;
            return;
        }
        cerr << "find no support device" << endl;
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

#define DEFAULT_CONFIG "/etc/dloader.conf"
void load_config(const string &conf)
{
    int linenum = 0;
    fstream fin(conf);
    char buff[1024];

    while (fin.getline(buff, sizeof(buff)))
    {
        string line(buff);
        linenum++;
        if (line.empty() || line[0] == '#' ||
            line[0] == '\r' || line[0] == '\n')
            continue;

        auto key = line.substr(0, line.find_first_of('='));
        auto val = line.substr(line.find_first_of('=') + 1);
        if (val.empty())
        {
            cerr << "fatal error at line " << linenum << ", key '" << key << "' has no value" << endl;
            exit(0);
        }

        if (key == "dev")
        {
            char phy[32];
            char idstr[32];
            char ifstr[32];
            char chipstr[32];
            sscanf(val.c_str(), "%[^:]:%[^,],%[^,],%s", phy, idstr, ifstr, chipstr);
            config.devs.emplace_back(support_dev{
                false,
                phy,
                idstr,
                ifstr,
                chipstr,
            });
        }
        else if (key == "pac_path")
        {
            config.pac_path = line.substr(line.find_first_of('=') + 1);
        }
        else if (key == "usb_physical_port")
        {
            config.usb_physical_port = line.substr(line.find_first_of('=') + 1);
        }
        else if (key == "reset_normal")
        {
            config.reset_normal = atoi(line.substr(line.find_first_of('=') + 1).c_str());
        }
    }
}

int do_update(const string &name)
{
    UpgradeManager upmgr(config.device, config.pac_path);

    if (!upmgr.prepare())
        return -1;

    return upmgr.upgrade(name, true);
}

int main(int argc, char **argv)
{
    int opt;
    string config_path;
    bool flag_list_device = false;

    if (argc > 1 && argv[1][0] != '-')
    {
        config_path = argv[1];
        argc--;
        argv++;
    }

    load_config(config_path.empty() ? DEFAULT_CONFIG : config_path);

    while ((opt = getopt(argc, argv, "hf:x:p:ld:c:")) > 0)
    {
        switch (opt)
        {
        case 'f':
            config.pac_path = optarg;
            break;

        case 'd':
            config.device = optarg;
            break;

        case 'c':
            config.chipset = optarg;
            break;

        case 'p':
            config.usb_physical_port = optarg;
            break;

        case 'l':
            flag_list_device = true;
            break;

        case 'x':
        {
            if (!access(optarg, F_OK))
            {
                Firmware fm(optarg);
                string extdir = ".";
                if (optind < argc && argv[optind][0] != '-')
                    extdir = argv[optind];
                fm.unpack_all(extdir);
            }
            return 0;
        }
        case 'h':
        default:
            usage(argv[0]);
            return 0;
        }
    }

    if (flag_list_device)
    {
        Device dev;
        dev.scan(config.usb_physical_port);
        return 0;
    }

    if (!config.pac_path.empty() && is_dir(config.pac_path))
        auto_find_pac(config.pac_path);

    if (config.device.empty())
        auto_find_dev(config.usb_physical_port);

    cerr << "choose device: " << config.device << endl;
    cerr << "choose pac: " << config.pac_path << endl;
    cerr << "chip series is: " << config.chipset << endl;
    if (!config.chipset.empty() &&
        !config.device.empty() && !access(config.device.c_str(), F_OK) &&
        !config.pac_path.empty() && !access(config.device.c_str(), F_OK))
        return do_update(config.chipset);

    cerr << "find no support device or no pac file" << endl;
    return -1;
}
