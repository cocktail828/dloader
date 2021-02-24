/*
 * @Author: sinpo828
 * @Date: 2021-02-08 15:51:43
 * @LastEditors: sinpo828
 * @LastEditTime: 2021-02-24 17:34:12
 * @Description: file content
 */
#ifndef __CONFIG__
#define __CONFIG__

#include <string>
#include <vector>

struct support_dev
{
    bool use_flag;
    int vid;
    int pid;
    int ifno;
    std::string phylink;
    std::string chipset;

    support_dev()
        : use_flag(false),
          vid(0), pid(0), ifno(0),
          phylink(""), chipset("") {}
};

struct configuration
{
    std::string device;
    std::string chipset;
    std::string pac_path;
    std::string usb_physical_port;
    bool reset_normal;
    std::vector<support_dev> edl_devs;
    std::vector<support_dev> normal_devs;

    configuration() : device(""),
                      chipset(""),
                      pac_path(""),
                      usb_physical_port(""),
                      reset_normal(true) {}
};

#endif //__CONFIG__