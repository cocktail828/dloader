/*
 * @Author: sinpo828
 * @Date: 2021-02-05 08:54:19
 * @LastEditors: sinpo828
 * @LastEditTime: 2021-02-09 14:03:39
 * @Description: file content
 */
#ifndef __SERIAL__
#define __SERIAL__

#include <iostream>
#include <string>

#include <termios.h>

#undef _VALUES
#define _VALUES         \
    _VAL(B57600),       \
        _VAL(B115200),  \
        _VAL(B230400),  \
        _VAL(B460800),  \
        _VAL(B500000),  \
        _VAL(B576000),  \
        _VAL(B921600),  \
        _VAL(B1000000), \
        _VAL(B1152000), \
        _VAL(B1500000), \
        _VAL(B2000000), \
        _VAL(B2500000), \
        _VAL(B3000000), \
        _VAL(B3500000), \
        _VAL(B4000000),

#define _VAL(v) BAUD_##v
enum class BAUD
{
    _VALUES
};
#undef _VAL

#define _VAL(v) v
static int BaudARR[] = {_VALUES};
#undef _VAL

class SerialPort
{
private:
    const int max_buf_size = 4 * 1024;
    std::string ttydev;
    int ttyfd;
    int epfd;
    uint8_t *buffer;
    uint32_t bufsize;

public:
    SerialPort(const std::string &tty);
    ~SerialPort();

    bool isOpened();
    void setBaud(BAUD baud);

    uint8_t *data();
    uint16_t datalen();

    bool sendSync(uint8_t *data, uint16_t len);
    bool recvSync(uint32_t timeout);
};

#endif // __SERIAL__