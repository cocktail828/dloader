/*
 * @Author: sinpo828
 * @Date: 2021-02-08 15:51:43
 * @LastEditors: sinpo828
 * @LastEditTime: 2021-02-23 14:22:15
 * @Description: file content
 */
#ifndef __CONFIG__
#define __CONFIG__

#include <string>
#include <vector>

struct support_dev
{
    bool use_flag;
    std::string phy;
    std::string usbid;
    std::string usbif;
    std::string chipset;
};

struct configuration
{
    std::string device;
    std::string chipset;
    std::string pac_path;
    std::string usb_physical_port;
    bool reset_normal;
    std::vector<support_dev> devs;

    configuration() : device(""),
                      chipset(""),
                      pac_path(""),
                      usb_physical_port(""),
                      reset_normal(true) {}
};

#endif //__CONFIG__