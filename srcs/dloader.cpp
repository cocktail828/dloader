/*
 * @Author: sinpo828
 * @Date: 2021-02-07 12:21:12
 * @LastEditors: sinpo828
 * @LastEditTime: 2021-02-25 12:50:21
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
#include "usbfs.hpp"
#include "serial.hpp"
#include "devices.hpp"
#include "config.hpp"
#include "common.hpp"
#include "scopeguard.hpp"

using namespace std;

#define _STRINGFY(v) #v
#define STRINGFY(v) _STRINGFY(v)
#define VERSION_STR     \
    STRINGFY(MAJOR_VER) \
    "." STRINGFY(MINOR_VER) "." STRINGFY(REVISION_VER)

static configuration config;

#define ARGUMENTS                                                                                       \
    _VAL('f', "pac_file", required_argument, "pacfile", "firmware file, with suffix of '.pac'")         \
    _VAL('d', "device", required_argument, "ttydev", "tty device, example(/dev/ttyUSB0)")               \
    _VAL('p', "port", required_argument, "usbport", "usb port, it's port string, see '-l' for details") \
    _VAL('x', "exract", required_argument, "pacfile [dir]", "exract pac_file only")                     \
    _VAL('F', "force", no_argument, "", "force update, even the device is in normal mode")              \
    _VAL('l', "list", no_argument, "", "list devices")                                                  \
    _VAL('h', "help", no_argument, "", "help message")

static const char *shortopts = "f:d:p:x:Flh";
#define _VAL(sarg, larg, haspara, ind, desc) \
    option{larg, haspara, 0, sarg},
const static struct option longopts[] = {
    ARGUMENTS};
#undef _VAL

#define _VAL(sarg, larg, haspara, ind, desc)          \
    do                                                \
    {                                                 \
        std::string line("  -");                      \
        line += sarg;                                 \
        line += ",--";                                \
        line += larg;                                 \
        line += "  ";                                 \
        if (haspara == required_argument)             \
        {                                             \
            line += ind;                              \
        }                                             \
        else if (haspara == required_argument)        \
        {                                             \
            line += "[";                              \
            line += ind;                              \
            line += "]";                              \
        }                                             \
        line += std::string(30 - line.length(), ' '); \
                                                      \
        cerr << line << desc << endl;                 \
    } while (0);

void usage(const char *prog)
{
    cerr << prog << " version " << VERSION_STR << " build at: " << __DATE__ << endl;
    cerr << prog << " [config] [options]" << endl;
    ARGUMENTS
}
#undef _VAL

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

static bool flag_force_update = false;
void auto_find_dev(const string &port)
{
    Device dev;
    int try_time = 0;
    int max_try_time = 15;

    for (; try_time < max_try_time; try_time++)
    {
        dev.reset();
        dev.scan(port);

        // find normal device, try switch mode if set '--force'
        for (auto iter = config.normal_devs.begin(); iter != config.normal_devs.end(); iter++)
        {
            if (dev.exist(iter->vid, iter->pid, iter->ifno) && flag_force_update)
            {
                auto intf = dev.get_interface(iter->vid, iter->pid, iter->ifno);

                if (!intf.ttyusb.empty())
                {
                    std::string ttydev("/dev");
                    std::string edl_command("at+qdownload=1\r\n");
                    SerialPort serial(ttydev + intf.ttyusb);

                    serial.sendSync((uint8_t *)(edl_command.c_str()), edl_command.length());
                    break;
                }
            }
        }

        // find edl device
        for (auto iter = config.edl_devs.begin(); iter != config.edl_devs.end(); iter++)
        {
            if (dev.exist(iter->vid, iter->pid, iter->ifno))
            {
                auto intf = dev.get_interface(iter->vid, iter->pid, iter->ifno);

                iter->use_flag = true;
                if (intf.ttyusb.empty())
                {
                    char buf[128] = {'\0'};
                    auto usbdev = dev.get_usbdevice(iter->vid, iter->pid);

                    snprintf(buf, sizeof(buf), "/dev/bus/usb/%03d/%03d", usbdev.busno, usbdev.devno);
                    config.device = buf;
                    config.endpoint_in = intf.endpoint_in;
                    config.endpoint_out = intf.endpoint_out;
                    config.interface_no = intf.interface_no;
                }
                else
                    config.device = "/dev/" + intf.ttyusb;

                config.chipset = iter->chipset;
                return;
            }
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

        if (key == "edldev")
        {
            char phylink[32];
            char chipstr[32];
            support_dev dev;

            sscanf(val.c_str(), "%[^,],%04x,%04x,%d,%s", phylink, &dev.vid, &dev.pid, &dev.ifno, chipstr);
            dev.chipset = chipstr;
            dev.phylink = phylink;
            config.edl_devs.push_back(dev);
        }
        else if (key == "normaldev")
        {
            char phylink[32];
            char chipstr[32];
            support_dev dev;

            sscanf(val.c_str(), "%[^,],%04x,%04x,%d,%s", phylink, &dev.vid, &dev.pid, &dev.ifno, chipstr);
            dev.chipset = chipstr;
            dev.phylink = phylink;
            config.normal_devs.push_back(dev);
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

int do_update(const string &name, USBStream *us)
{
    UpgradeManager upmgr(config.device, config.pac_path, us);

    if (!upmgr.prepare())
        return -1;

    return upmgr.upgrade(name, true);
}

int main(int argc, char **argv)
{
    int opt;
    string config_path;
    int longidx = 0;
    bool flag_list_device = false;
    USBStream *us = nullptr;

    ON_SCOPE_EXIT
    {
        if (us)
            delete us;
    };

    if (argc < 2)
        return 0;

    if (argc > 1 && argv[1][0] != '-')
        config_path = argv[1];

    load_config(config_path.empty() ? DEFAULT_CONFIG : config_path);

    while ((opt = getopt_long(argc, argv, shortopts, longopts, &longidx)) > 0)
    {
        switch (opt)
        {
        case 'f':
            config.pac_path = optarg;
            break;

        case 'd':
            config.device = optarg;
            break;

        case 'p':
            config.usb_physical_port = optarg;
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

        case 'F':
            flag_force_update = true;
            break;

        case 'l':
            flag_list_device = true;
            break;

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

    if (!config.device.empty())
    {
        if (config.device.find("/dev/bus/usb") != std::string::npos)
            us = new USBFS(config.device, config.interface_no, config.endpoint_in, config.endpoint_out);
        else
            us = new SerialPort(config.device);
    }

    if (!config.device.empty() && !access(config.device.c_str(), F_OK) &&
        !config.pac_path.empty() && !access(config.pac_path.c_str(), F_OK))
        return do_update(config.chipset, us);

    cerr << "find no support device or no pac file" << endl;
    return -1;
}
