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

static std::string find_tty_device(const std::string _dirname)
{
    DIR *pdir = NULL;
    struct dirent *ent = NULL;
    std::string dirname(_dirname + "/tty");
    std::string ttydev("");

    if (access(dirname.c_str(), F_OK))
        dirname = _dirname;

    pdir = opendir(dirname.c_str());
    if (!pdir)
        return ttydev;

    while ((ent = readdir(pdir)) != NULL)
    {
        if (ent->d_name[0] == '.')
            continue;

        if (!strncmp(ent->d_name, "ttyUSB", 6) ||
            !strncmp(ent->d_name, "ttyACM", 6))
        {
            ttydev = ent->d_name;
            break;
        }
    }
    closedir(pdir);

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
    pdir = opendir(rootdir.c_str());
    if (!pdir)
        return -1;

    while ((ent = readdir(pdir)) != NULL)
    {
        std::string file;
        interface iface;
        std::string path(rootdir);
        if (ent->d_name[0] == '.')
            continue;

        path += "/" + std::string(ent->d_name);

        file = std::string(path) + "/bInterfaceClass";
        iface.cls = file_get_xint(file);

        file = std::string(path) + "/bInterfaceSubClass";
        iface.subcls = file_get_xint(file);

        file = std::string(path) + "/bInterfaceProtocol";
        iface.proto = file_get_xint(file);

        file = std::string(path) + "/bInterfaceNumber";
        iface.ifno = file_get_xint(file);

        file = std::string(path) + "/modalias";
        iface.modalias = file_get_line(file);

        iface.ttyusb = find_tty_device(path);

        if (iface.cls == -1 || iface.subcls == -1 || iface.proto == -1 || iface.ifno == -1)
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
        file = std::string(path) + "/idVendor";
        _vid = file_get_xint(file);
        file = std::string(path) + "/idProduct";
        _pid = file_get_xint(file);
        file = std::string(path) + "/devpath";
        _usbport = file_get_line(file);

        if (!usbport.empty() && usbport != _usbport)
            continue;

        if (_vid != -1 && _pid != -1)
            scan_iface(_vid, _pid, _usbport, path);
    }
    closedir(pdir);

    for (auto iter = m_usbdevs.begin(); iter != m_usbdevs.end(); iter++)
    {
        std::cerr << "Device ID " << std::hex << iter->vid << ":" << iter->pid
                  << ", Port " << iter->usbport
                  << ", Path " << iter->devpath << std::endl;
        for (auto iter1 = iter->ifaces.begin(); iter1 != iter->ifaces.end(); iter1++)
        {
            std::cerr << "  |__ If " << iter1->ifno
                      << ", Class=" << iter1->cls
                      << ", SubClass=" << iter1->subcls
                      << ", Proto=" << iter1->proto
                      << ", TTY=" << iter1->ttyusb
                      << ", MODALIAS=" << iter1->modalias
                      << std::endl;
        }
    }

    return 0;
}

bool Device::exist(const std::string &idstr, const std::string &ifstr)
{
    for (auto iter = m_usbdevs.begin(); iter != m_usbdevs.end(); iter++)
    {
        for (auto iter1 = iter->ifaces.begin(); iter1 != iter->ifaces.end(); iter1++)
        {
            if (iter1->modalias.find(idstr) != std::string::npos &&
                iter1->modalias.find(ifstr) != std::string::npos)
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

interface Device::get_interface(const std::string &idstr, const std::string &ifstr)
{
    for (auto iter = m_usbdevs.begin(); iter != m_usbdevs.end(); iter++)
    {
        for (auto iter1 = iter->ifaces.begin(); iter1 != iter->ifaces.end(); iter1++)
        {
            if (iter1->modalias.find(idstr) != std::string::npos &&
                iter1->modalias.find(ifstr) != std::string::npos)
                return *iter1;
        }
    }
    return interface();
}