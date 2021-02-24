/*
 * @Author: sinpo828
 * @Date: 2021-02-08 14:55:50
 * @LastEditors: sinpo828
 * @LastEditTime: 2021-02-24 16:54:25
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
    std::string modalias;
    std::string ttyusb;
    interface() : cls(0), subcls(0), proto(0),
                  ifno(0), modalias(""), ttyusb("") {}
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
    int scan_iface(int vid, int pid, std::string usbport, std::string rootdir);

public:
    Device() { reset(); }
    ~Device() {}

    void reset();
    int scan(const std::string &usbport);
    bool exist(int vid, int pid, int cls, int scls, int proto);
    bool exist(int vid, int pid, int ifno);
    interface get_interface(int vid, int pid, int cls, int scls, int proto);
    interface get_interface(int vid, int pid, int ifno);
};

#endif //__DEVICE__