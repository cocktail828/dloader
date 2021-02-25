/*
 * @Author: sinpo828
 * @Date: 2021-02-25 09:59:00
 * @LastEditors: sinpo828
 * @LastEditTime: 2021-02-25 13:15:48
 * @Description: file content
 */
#ifndef __USBCOM__
#define __USBCOM__

#include <string>

class USBStream
{
protected:
    const int max_buf_size = 4 * 1024;
    std::string usb_device;
    uint8_t *_data;
    uint32_t _reallen;

public:
    USBStream(const std::string &dev)
        : usb_device(dev), _reallen(0) { _data = new (std::nothrow) uint8_t[max_buf_size]; }
    virtual ~USBStream()
    {
        if (_data)
            delete[] _data;
        _data = nullptr;
    }

    virtual void init() = 0;
    virtual bool isOpened() = 0;
    virtual bool sendSync(uint8_t *data, uint32_t len, uint32_t timeout = 5000) = 0;
    virtual bool recvSync(uint32_t timeout) = 0;

    virtual uint8_t *data() { return _data; };
    virtual uint32_t datalen() { return _reallen; };
};

#endif //__USBCOM__