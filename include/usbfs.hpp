/*
 * @Author: sinpo828
 * @Date: 2021-02-05 08:54:19
 * @LastEditors: sinpo828
 * @LastEditTime: 2021-02-26 14:05:25
 * @Description: file content
 */
#ifndef __USBFS__
#define __USBFS__

#include <iostream>
#include <string>

#include "usbcom.hpp"

class USBFS final : public USBStream {
   private:
    int usbfd;
    int interface_no;
    uint8_t endpoint_in;
    uint8_t endpoint_out;

   public:
    USBFS(const std::string &devpath, int ifno, int epin, int epout);
    ~USBFS();

    void sciu2sMessage();
    void setInterface();

    bool isOpened();
    bool sendSync(uint8_t *data, uint32_t len, uint32_t timeout);
    bool recvSync(uint32_t timeout);

    bool usbfs_is_kernel_driver_alive();
    void usbfs_detach_kernel_driver();
    int usbfs_claim_interface();
    int usbfs_release_interface();
    int usbfs_max_packet_len();
};

#endif  // __USBFS__