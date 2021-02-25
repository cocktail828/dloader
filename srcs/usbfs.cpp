/*
 * @Author: sinpo828
 * @Date: 2021-02-25 09:58:10
 * @LastEditors: sinpo828
 * @LastEditTime: 2021-02-25 13:50:23
 * @Description: file content
 */
#include <iostream>

extern "C"
{
#include <unistd.h>
#include <sys/fcntl.h>
#include <sys/types.h>
#include <errno.h>
#include <string.h>
#include <sys/ioctl.h>
#include <linux/usbdevice_fs.h>
#include <linux/version.h>
#if LINUX_VERSION_CODE > KERNEL_VERSION(2, 6, 20)
#include <linux/usb/ch9.h>
#else
#include <linux/usb_ch9.h>
#endif
}

#include "usbfs.hpp"

USBFS::USBFS(const std::string &devpath, int ifno, int epin, int epout)
    : USBStream(devpath), interface_no(ifno), endpoint_in(epin), endpoint_out(epout)
{
    usbfd = open(devpath.c_str(), O_RDWR | O_NOCTTY);
    if (usbfd < 0)
        return;

    if (usbfs_is_kernel_driver_alive())
        usbfs_detach_kernel_driver();

    if (usbfs_claim_interface())
    {
        close(usbfd);
        usbfd = -1;
    }

    init();
}

USBFS::~USBFS()
{
    if (usbfd > 0)
    {
        usbfs_release_interface();
        close(usbfd);
        usbfd = -1;
    }
}

void USBFS::init()
{
    struct usbdevfs_ctrltransfer control;

    control.bRequestType = 0x21;
    control.bRequest = 34;
    control.wValue = endpoint_out << 8 | 1;
    control.wIndex = 0;
    control.wLength = 0;
    control.timeout = 0; /* in milliseconds */
    control.data = NULL;

    ioctl(usbfd, USBDEVFS_CONTROL, &control);
}

bool USBFS::isOpened()
{
    return usbfd > 0;
}

/**
 * will send ZLP if len==0
 * NOTICE: USBFS can only send as much as 16K byte in an transfer
 */
#define MAX_USBFS_BULK_SIZE (16 * 1024)
bool USBFS::sendSync(uint8_t *data, uint32_t len, uint32_t timeout)
{
    int ret;
    uint32_t totallen = len;

    if (!isOpened())
        return false;

    do
    {
        int sz = (len > MAX_USBFS_BULK_SIZE) ? MAX_USBFS_BULK_SIZE : len;
        struct usbdevfs_bulktransfer bulk;
        bulk.ep = endpoint_out;
        bulk.len = sz;
        bulk.data = data;
        bulk.timeout = timeout;

        ret = ioctl(usbfd, USBDEVFS_BULK, &bulk);
        if (ret != sz)
        {
            std::cerr << __func__ << " bulkout error, send "
                      << ret << "/" << sz << ", " << (totallen - len) << "/" << totallen
                      << ", for " << strerror(errno) << std::endl;
            return false;
        }

        len -= sz;
        data += sz;
    } while (len > 0);

    return true;
}

bool USBFS::recvSync(uint32_t timeout)
{
    struct usbdevfs_bulktransfer bulk;
    int n;
    int sz = (max_buf_size > MAX_USBFS_BULK_SIZE) ? MAX_USBFS_BULK_SIZE : max_buf_size;

    if (!isOpened())
        return false;

    bulk.ep = endpoint_in;
    bulk.len = sz;
    bulk.data = _data;
    bulk.timeout = timeout;

    n = ioctl(usbfd, USBDEVFS_BULK, &bulk);
    if (n < 0)
    {
        std::cerr << __func__ << " bulkin error, "
                  << n << "/" << sz << ", for " << strerror(errno) << std::endl;
        return false;
    }

    _reallen = n >= 0 ? n : 0;

    return true;
}

bool USBFS::usbfs_is_kernel_driver_alive()
{
    struct usbdevfs_getdriver usbdrv;

    usbdrv.interface = interface_no;
    if (ioctl(usbfd, USBDEVFS_GETDRIVER, &usbdrv) < 0)
    {
        if (errno != ENODATA)
            std::cerr << __func__ << " ioctl USBDEVFS_GETDRIVER fail, for " << strerror(errno) << std::endl;
        return false;
    }

    std::cerr << __func__ << " interface " << interface_no << " has attach driver: " << usbdrv.driver << std::endl;
    return true;
}

void USBFS::usbfs_detach_kernel_driver()
{
    int ret;
    struct usbdevfs_ioctl operate;

    operate.data = NULL;
    operate.ifno = interface_no;
    operate.ioctl_code = USBDEVFS_DISCONNECT;
    ret = ioctl(usbfd, USBDEVFS_IOCTL, &operate);

    std::cerr << __func__ << " detach kernel driver " << (ret < 0 ? "fail" : "success") << std::endl;
}

int USBFS::usbfs_claim_interface()
{
    return ioctl(usbfd, USBDEVFS_CLAIMINTERFACE, &interface_no);
}

int USBFS::usbfs_release_interface()
{
    return ioctl(usbfd, USBDEVFS_CLAIMINTERFACE, &interface_no);
}

int USBFS::usbfs_max_packet_len()
{
    return MAX_USBFS_BULK_SIZE;
}