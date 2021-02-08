/*
 * @Author: sinpo828
 * @Date: 2021-02-05 08:54:19
 * @LastEditors: sinpo828
 * @LastEditTime: 2021-02-08 11:47:30
 * @Description: file content
 */
#ifndef __SERIAL__
#define __SERIAL__

#include <iostream>
#include <string>

#include <termios.h>

enum BAUD
{
    BAUD57600 = 0010001,
    BAUD115200 = 0010002,
    BAUD230400 = 0010003,
    BAUD460800 = 0010004,
    BAUD500000 = 0010005,
    BAUD576000 = 0010006,
    BAUD921600 = 0010007,
    BAUD1000000 = 0010010,
    BAUD1152000 = 0010011,
    BAUD1500000 = 0010012,
    BAUD2000000 = 0010013,
    BAUD2500000 = 0010014,
    BAUD3000000 = 0010015,
    BAUD3500000 = 0010016,
    BAUD4000000 = 0010017,
};

class SerialPort
{
private:
    const int max_buf_size = 4 * 1024;
    std::string ttydev;
    int ttyfd;
    int epfd;
    uint8_t *buffer;
    uint8_t bufsize;

public:
    SerialPort(const std::string &tty);
    ~SerialPort();

    bool isOpened();

    bool isDeviceReady();

    uint8_t *data();

    uint16_t datalen();

    void setBaud(BAUD baud);

    ssize_t sendSync(uint8_t *data, uint16_t len);
    ssize_t recvSync(uint32_t timeout);
};

#endif // __SERIAL__