/*
 * @Author: sinpo828
 * @Date: 2021-02-25 09:59:00
 * @LastEditors: sinpo828
 * @LastEditTime: 2021-02-26 16:15:05
 * @Description: file content
 */
#ifndef __USBCOM__
#define __USBCOM__

#include <string>

enum class USBLINK
{
    USBLINK_TTY,
    USBLINK_USBFS,
};

class USBStream
{
protected:
    const int max_buf_size = 4 * 1024;
    std::string usb_device;
    uint8_t *_data;
    uint32_t _reallen;
    USBLINK phylink;

public:
    USBStream(const std::string &dev, USBLINK phy)
        : usb_device(dev), _reallen(0), phylink(phy)
    {
        _data = new (std::nothrow) uint8_t[max_buf_size];
    }

    virtual ~USBStream()
    {
        if (_data)
            delete[] _data;
        _data = nullptr;
    }

    virtual USBLINK physicalLink() final { return phylink; }
    virtual bool isOpened() = 0;
    virtual bool sendSync(uint8_t *data, uint32_t len, uint32_t timeout) = 0;
    virtual bool recvSync(uint32_t timeout) = 0;

    virtual uint8_t *data() final { return _data; };
    virtual uint32_t datalen() final { return _reallen; };
};

#endif //__USBCOM__