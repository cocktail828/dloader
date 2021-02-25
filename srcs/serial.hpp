/*
 * @Author: sinpo828
 * @Date: 2021-02-05 08:54:19
 * @LastEditors: sinpo828
 * @LastEditTime: 2021-02-25 13:06:50
 * @Description: file content
 */
#ifndef __SERIAL__
#define __SERIAL__

#include <iostream>
#include <string>

#include <termios.h>

#include "usbcom.hpp"

#undef _VALUES
#define _VALUES        \
    _VAL(57600),       \
        _VAL(115200),  \
        _VAL(230400),  \
        _VAL(460800),  \
        _VAL(500000),  \
        _VAL(576000),  \
        _VAL(921600),  \
        _VAL(1000000), \
        _VAL(1152000), \
        _VAL(1500000), \
        _VAL(2000000), \
        _VAL(2500000), \
        _VAL(3000000), \
        _VAL(3500000), \
        _VAL(4000000),

#define _VAL(v) BAUD##v
enum class BAUD
{
    _VALUES
};
#undef _VAL

#define _VAL(v) v
static int BaudARR[] = {_VALUES};
#undef _VAL

#define _VAL(v) #v
static const char *BaudARRSTR[] = {_VALUES};
#undef _VAL

class SerialPort final : public USBStream
{
private:
    int ttyfd;
    int epfd;

public:
    SerialPort(const std::string &tty);
    ~SerialPort();

    void setBaud(BAUD);
    void init();
    bool isOpened();
    bool sendSync(uint8_t *data, uint32_t len, uint32_t timeout = 0);
    bool recvSync(uint32_t timeout);
};

#endif // __SERIAL__