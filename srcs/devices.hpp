/*
 * @Author: sinpo828
 * @Date: 2021-02-08 14:55:50
 * @LastEditors: sinpo828
 * @LastEditTime: 2021-02-08 14:57:32
 * @Description: file content
 */
#ifndef __DEVICE__
#define __DEVICE__

#include <string>
#include <vector>

struct interface
{
    int cls;
    int subcls;
    int proto;
    int ifno;
    std::string ttyusb;
    interface() : cls(0), subcls(0), proto(0), ifno(0), ttyusb("") {}
};

struct usbdev
{
    int vid;
    int pid;
    std::string usbport;
    std::string devpath;
    std::vector<interface> ifaces;
    usbdev() : usbport(""), devpath("") {}
};

class Device final
{
private:
    std::vector<usbdev> m_usbdevs;

private:
    void reset();
    int scan_iface(int vid, int pid, std::string usbport, std::string rootdir);

public:
    Device() { reset(); }
    ~Device() {}

    // friend bool filter_by_usbport(Device *pdev, const char *path);
    // friend bool filter_by_vid_pid(Device *pdev, const char *path);
    // friend bool filter_by_ifno(Device *pdev, const char *path);
    // friend bool filter_by_ids(Device *pdev, const char *path);
    // friend bool filter_none(Device *pdev, const char *path);

    int scan(const std::string &usbport);
    bool exist(int vid, int pid, int cls, int scls, int proto);
    interface get_interface(int vid, int pid, int cls, int scls, int proto);
};

#endif //__DEVICE__