#include <iostream>
#include <sstream>
#include <fstream>

extern "C"
{
#include <dirent.h>
#include <cstdlib>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
}

#include "devices.hpp"

static std::string find_file_with_prefix(const std::string &_dirname, const std::string &_prefix)
{
    DIR *pdir = NULL;
    struct dirent *ent = NULL;
    std::string dirname(_dirname);
    std::string filename("");

    pdir = opendir(dirname.c_str());
    if (!pdir)
        return filename;

    while ((ent = readdir(pdir)) != NULL)
    {
        if (ent->d_name[0] == '.')
            continue;

        if (!strncmp(ent->d_name, _prefix.c_str(), _prefix.length()))
        {
            filename = ent->d_name;
            break;
        }
    }
    closedir(pdir);

    return filename;
}

static std::string find_tty_device(const std::string &_dirname)
{
    std::string dirname(_dirname + "/tty");
    std::string ttydev;

    if (access(dirname.c_str(), F_OK))
        dirname = _dirname;

    ttydev = find_file_with_prefix(dirname, "ttyUSB");
    if (ttydev.empty())
        ttydev = find_file_with_prefix(dirname, "ttyACM");

    return ttydev;
}

static std::string file_get_line(const std::string &file)
{
    std::string line;
    std::ifstream fin(file);

    if (!fin.is_open())
        return "";

    fin >> line;
    fin.close();
    return line;
}

static int file_get_xint(const std::string &file)
{
    int line;
    std::ifstream fin(file);

    if (!fin.is_open())
        return -1;

    fin >> std::hex >> line;
    fin.close();
    return line;
}

static int file_get_int(const std::string &file)
{
    int line;
    std::ifstream fin(file);

    if (!fin.is_open())
        return -1;

    fin >> std::dec >> line;
    fin.close();
    return line;
}

void Device::reset()
{
    m_usbdevs.clear();
}

int Device::scan_iface(int vid, int pid, std::string usbport, std::string rootdir)
{
    struct dirent *ent = NULL;
    DIR *pdir = NULL;
    usbdev udev;

    udev.vid = vid;
    udev.pid = pid;
    udev.usbport = usbport;
    udev.devpath = rootdir;
    udev.busno = file_get_int(rootdir + "/busnum");
    udev.devno = file_get_int(rootdir + "/devnum");

    pdir = opendir(rootdir.c_str());
    if (!pdir)
        return -1;

    while ((ent = readdir(pdir)) != NULL)
    {
        std::string file;
        interface iface;
        std::string path(rootdir);
        std::string endpoint;
        if (ent->d_name[0] == '.')
            continue;

        path += "/" + std::string(ent->d_name);

        iface.cls = file_get_xint(path + "/bInterfaceClass");
        iface.subcls = file_get_xint(path + "/bInterfaceSubClass");
        iface.proto = file_get_xint(path + "/bInterfaceProtocol");
        iface.interface_no = file_get_xint(path + "/bInterfaceNumber");
        iface.modalias = file_get_line(path + "/modalias");

        iface.ttyusb = find_tty_device(path);

        endpoint = find_file_with_prefix(path, "ep_8");
        if (!endpoint.empty())
            iface.endpoint_in = strtoul(endpoint.substr(3).c_str(), NULL, 16);

        endpoint = find_file_with_prefix(path, "ep_0");
        if (!endpoint.empty())
            iface.endpoint_out = strtoul(endpoint.substr(3).c_str(), NULL, 16);

        if (iface.cls == -1 || iface.subcls == -1 || iface.proto == -1 || iface.interface_no == -1)
            continue;

        udev.ifaces.push_back(iface);
    }
    closedir(pdir);

    if (udev.ifaces.size())
        m_usbdevs.push_back(udev);

    return 0;
}

int Device::scan(const std::string &usbport)
{
    struct dirent *ent = NULL;
    DIR *pdir = NULL;
    const char *rootdir = "/sys/bus/usb/devices";

    pdir = opendir(rootdir);
    if (!pdir)
        return -1;

    while ((ent = readdir(pdir)) != NULL)
    {
        std::string path(rootdir);
        std::string file;
        std::string _usbport;
        int _vid;
        int _pid;
        if (ent->d_name[0] == '.' || ent->d_name[0] == 'u')
            continue;

        path += "/" + std::string(ent->d_name);

        _vid = file_get_xint(path + "/idVendor");
        _pid = file_get_xint(path + "/idProduct");
        _usbport = file_get_line(path + "/devpath");

        if (!usbport.empty() && usbport != _usbport)
            continue;

        if (_vid != -1 && _pid != -1)
            scan_iface(_vid, _pid, _usbport, path);
    }
    closedir(pdir);

    for (auto iter = m_usbdevs.begin(); iter != m_usbdevs.end(); iter++)
    {
        std::cerr << "Bus " << std::dec << iter->busno << ".Port " << iter->usbport
                  << ", Dev " << iter->devno
                  << ", ID " << iter->vid << ":" << iter->pid
                  << ", Port " << iter->usbport
                  << ", Path " << iter->devpath << std::endl;
        for (auto iter1 = iter->ifaces.begin(); iter1 != iter->ifaces.end(); iter1++)
        {
            std::cerr << "  |_ If " << std::hex << iter1->interface_no
                      << ", Class=" << iter1->cls
                      << ", SubClass=" << iter1->subcls
                      << ", Proto=" << iter1->proto
                      << ", EPIn=" << iter1->endpoint_in
                      << ", EPout=" << iter1->endpoint_out
                      << ", TTY=" << iter1->ttyusb
                      //   << ", MODALIAS=" << iter1->modalias
                      << std::dec << std::endl;
        }
    }

    return 0;
}

bool Device::exist(int vid, int pid)
{
    for (auto iter = m_usbdevs.begin(); iter != m_usbdevs.end(); iter++)
    {
        if (iter->vid != vid || iter->pid != pid)
            continue;

        return true;
    }
    return false;
}

bool Device::exist(int vid, int pid, int ifno)
{
    for (auto iter = m_usbdevs.begin(); iter != m_usbdevs.end(); iter++)
    {
        if (iter->vid != vid || iter->pid != pid)
            continue;

        for (auto iter1 = iter->ifaces.begin(); iter1 != iter->ifaces.end(); iter1++)
        {
            if (iter1->interface_no == ifno)
                return true;
        }
    }
    return false;
}

bool Device::exist(int vid, int pid, int cls, int scls, int proto)
{
    for (auto iter = m_usbdevs.begin(); iter != m_usbdevs.end(); iter++)
    {
        if (iter->vid != vid || iter->pid != pid)
            continue;

        for (auto iter1 = iter->ifaces.begin(); iter1 != iter->ifaces.end(); iter1++)
        {
            if (iter1->cls == cls && iter1->subcls == scls && iter1->proto == proto)
                return true;
        }
    }
    return false;
}

usbdev Device::get_usbdevice(int vid, int pid)
{
    for (auto iter = m_usbdevs.begin(); iter != m_usbdevs.end(); iter++)
    {
        if (iter->vid != vid || iter->pid != pid)
            continue;

        return *iter;
    }
    return usbdev();
}

interface Device::get_interface(int vid, int pid, int ifno)
{
    for (auto iter = m_usbdevs.begin(); iter != m_usbdevs.end(); iter++)
    {
        if (iter->vid != vid || iter->pid != pid)
            continue;

        for (auto iter1 = iter->ifaces.begin(); iter1 != iter->ifaces.end(); iter1++)
        {
            if (iter1->interface_no == ifno)
                return *iter1;
        }
    }
    return interface();
}

interface Device::get_interface(int vid, int pid, int cls, int scls, int proto)
{
    for (auto iter = m_usbdevs.begin(); iter != m_usbdevs.end(); iter++)
    {
        if (iter->vid != vid || iter->pid != pid)
            continue;

        for (auto iter1 = iter->ifaces.begin(); iter1 != iter->ifaces.end(); iter1++)
        {
            if (iter1->cls == cls && iter1->subcls == scls && iter1->proto == proto)
                return *iter1;
        }
    }
    return interface();
}
