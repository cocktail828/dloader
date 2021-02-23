/*
 * @Author: sinpo828
 * @Date: 2021-02-05 08:54:19
 * @LastEditors: sinpo828
 * @LastEditTime: 2021-02-23 08:48:58
 * @Description: file content
 */
#ifndef __SERIAL__
#define __SERIAL__

#include <iostream>
#include <string>

#include <termios.h>

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

    bool sendSync(uint8_t *data, uint32_t len);
    bool recvSync(uint32_t timeout);

    uint8_t *data();
    uint32_t datalen();
};

#endif // __SERIAL__