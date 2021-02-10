/*
 * @Author: sinpo828
 * @Date: 2021-02-07 12:21:12
 * @LastEditors: sinpo828
 * @LastEditTime: 2021-02-10 10:34:35
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
#include "packets.hpp"
#include "upgrade.hpp"
#include "devices.hpp"
#include "config.hpp"
#include "common.hpp"

using namespace std;

#define MAJOR_VER 0
#define MINOR_VER 0
#define REVISION_VER 1
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
    cerr << "    -f pac_file     firmware file, with suffix of '.pac'" << endl;
    cerr << "    -d device       device, can be a tty device" << endl;
    cerr << "    -x pac_file     exract pac_file only" << endl;
    cerr << "    -h              help message" << endl;
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
        }
    }

    cerr << "choose pac: " << choose_file << endl;
    return choose_file;
}

string auto_find_dev(const string &port)
{
    Device dev;
    string ttydev;
    int try_time = 0;
    int max_try_time = 15;

    for (; try_time < max_try_time; try_time++)
    {
        dev.reset();
        dev.scan(port);

        for (auto iter = config.devs.begin(); iter != config.devs.end(); iter++)
        {
            if (!dev.exist(iter->usbid, iter->usbif))
                continue;
            iter->use_flag = true;

            ttydev = dev.get_interface(iter->usbid, iter->usbif).ttyusb;
        }
        cerr << "find no support device" << endl;
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    cerr << "choose device: " << config.device << endl;

    return "";
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
            config.devs.emplace_back(supp_dev{
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
    Upgrade up(config.device, name, config.pac_path);

    if (!up.prepare())
        return -1;

    return up.level1();
}

int main(int argc, char **argv)
{
    int opt;
    string config_path;

    if (argc > 1 && argv[1][0] != '-')
    {
        config_path = argv[1];
        argc--;
        argv++;
    }

    load_config(config_path.empty() ? DEFAULT_CONFIG : config_path);

    while ((opt = getopt(argc, argv, "hf:d:x:")) > 0)
    {
        switch (opt)
        {
        case 'f':
            config.pac_path = optarg;
            break;

        case 'd':
            config.device = optarg;
            break;

        case 'x':
        {
            if (!access(optarg, F_OK))
            {
                Firmware fm(optarg);
                string extdir = ".";
                if (optind < argc && argv[optind][0] != '-')
                    extdir = argv[optind];
                cerr << optind << " " << argc << " " << argv[optind] << endl;
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

    if (!config.pac_path.empty() && is_dir(config.pac_path))
        config.pac_path = auto_find_pac(config.pac_path);

    if (config.device.empty())
        config.device = "/dev/" + auto_find_dev(config.usb_physical_port);

    cerr << "choose device: " << config.device << endl;

    string mname;
    for (auto iter = config.devs.begin(); iter != config.devs.end(); iter++)
    {
        if (iter->use_flag)
        {
            mname = iter->name;
            break;
        }
    }

    cerr << "chip series is: " << mname << endl;
    if (mname.empty())
    {
        cerr << "find no support device" << endl;
        return -1;
    }
    else
    {
        return do_update(mname);
    }
}
