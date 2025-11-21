/*
 * @Author: sinpo828
 * @Date: 2021-02-08 15:51:43
 * @LastEditors: sinpo828
 * @LastEditTime: 2021-02-26 16:22:05
 * @Description: file content
 */
#ifndef __CONFIG__
#define __CONFIG__

#include <string>
#include <vector>

enum class PHYLINK {
    PHYLINK_USB,
    PHYLINK_PCIE,
};

struct usbdev_info {
    int vid;
    int pid;
    int ifno;
    PHYLINK phylink;
    std::string device;  // ttydev or usbpath
};

struct configuration {
    int endpoint_in : 8;
    int endpoint_out : 8;
    int interface_no : 8;
    std::string device;
    std::string pac_path;
    std::string usb_physical_port;
    bool reset_normal;
    std::vector<usbdev_info> edl_devs;
    std::vector<usbdev_info> normal_devs;

    configuration() : endpoint_in(0), endpoint_out(0), interface_no(0), reset_normal(true) {}
};

#endif  //__CONFIG__