/*
 * @Author: sinpo828
 * @Date: 2021-02-05 08:54:19
 * @LastEditors: sinpo828
 * @LastEditTime: 2021-02-25 13:04:31
 * @Description: file content
 */
#ifndef __USBFS__
#define __USBFS__

#include <iostream>
#include <string>

#include "usbcom.hpp"

class USBFS final : public USBStream
{
private:
    int usbfd;
    int interface_no;
    uint8_t endpoint_in;
    uint8_t endpoint_out;

public:
    USBFS(const std::string &devpath, int ifno, int epin, int epout);
    ~USBFS();

    void init();
    bool isOpened();
    bool sendSync(uint8_t *data, uint32_t len, uint32_t timeout = 0);
    bool recvSync(uint32_t timeout);

    bool usbfs_is_kernel_driver_alive();
    void usbfs_detach_kernel_driver();
    int usbfs_claim_interface();
    int usbfs_release_interface();
    int usbfs_max_packet_len();
};

#endif // __USBFS__