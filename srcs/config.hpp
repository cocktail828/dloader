/*
 * @Author: sinpo828
 * @Date: 2021-02-08 15:51:43
 * @LastEditors: sinpo828
 * @LastEditTime: 2021-02-10 14:50:56
 * @Description: file content
 */
#ifndef __CONFIG__
#define __CONFIG__

#include <string>
#include <vector>

struct supp_dev
{
    bool use_flag;
    std::string phy;
    std::string usbid;
    std::string usbif;
    std::string name;
};

struct configuration
{
    std::string device;
    std::string mname;
    std::string pac_path;
    std::string usb_physical_port;
    bool reset_normal;
    std::vector<supp_dev> devs;
};

#endif //__CONFIG__