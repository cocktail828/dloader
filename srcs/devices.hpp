/*
 * @Author: sinpo828
 * @Date: 2021-02-08 14:55:50
 * @LastEditors: sinpo828
 * @LastEditTime: 2021-02-25 12:21:34
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
    int interface_no;
    int endpoint_in;
    int endpoint_out;
    std::string modalias;
    std::string ttyusb;
    interface() : cls(0), subcls(0), proto(0),
                  interface_no(0), modalias(""), ttyusb("") {}
};

struct usbdev
{
    int vid;
    int pid;
    int busno;
    int devno;
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
    bool exist(int vid, int pid);
    bool exist(int vid, int pid, int ifno);
    bool exist(int vid, int pid, int cls, int scls, int proto);
    usbdev get_usbdevice(int vid, int pid);
    interface get_interface(int vid, int pid, int ifno);
    interface get_interface(int vid, int pid, int cls, int scls, int proto);
};

#endif //__DEVICE__