/*
 * @Author: sinpo828
 * @Date: 2021-02-05 08:54:19
 * @LastEditors: sinpo828
 * @LastEditTime: 2021-02-05 09:55:07
 * @Description: file content
 */
#include <iostream>
#include <string>

#include <fcntl.h>
#include <termios.h>
#include <sys/epoll.h>
#include <unistd.h>

enum BAUD
{
    B115200,
};

class SerialPort
{
private:
    const int max_buf_size = 4 * 1024;
    std::string tty;
    int ttyfd;
    int epfd;
    uint8_t *buffer;
    uint8_t bufsize;

public:
    SerialPort(const std::string &tty) : tty(tty)
    {
        epoll_event event;
        ttyfd = ::open(ttydev.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);
        if (ttyfd > 0)
        {
            buffer = new uint8_t[max_buf_size];

            epfd = epoll_create(10);
            event.data.fd = ttyfd;
            event.events = EPOLLIN | EPOLLRDHUP;
            if (epoll_ctl(epfd, EPOLL_CTL_ADD, ttyfd, &event))
                return;
        }
    }

    ~SerialPort()
    {
        close(ttyfd);
        ttyfd = -1;
        close(epfd);
        epfd = -1;
        delete[] buffer;
    }

    bool isOpened() { return (ttyfd > 0); }

    bool isDeviceReady()
    {
        return (access(ttydev.c_str(), F_OK) == 0);
    }

    uint8_t *data() { return buffer; }

    uint16_t datalen() { return bufsize; }

    void setBaud(BAUD baud)
    {
        struct termios tio;
        struct termios settings;

        memset(&tio, 0, sizeof(tio));
        tio.c_iflag = 0;
        tio.c_oflag = 0;
        tio.c_cflag = CS8 | CREAD | CLOCAL; // 8n1, see termios.h for more information
        tio.c_lflag = 0;
        tio.c_cc[VMIN] = 1;
        tio.c_cc[VTIME] = 5;
        cfsetospeed(&tio, baud);
        cfsetispeed(&tio, baud);
        tcsetattr(ttyfd, TCSANOW, &tio);
        cfmakeraw(&settings);
        settings.c_cflag |= CREAD | CLOCAL;
        tcflush(ttyfd, TCIOFLUSH);
        tcsetattr(ttyfd, TCSANOW, &settings);
    }

    ssize_t sendSync(uint8_t *data, uint16_t len)
    {
        return isDeviceReady() ? write(ttyfd, data, len) : -1;
    }

    ssize_t recvSync(uint32_t timeout)
    {
        int num = epoll_wait(epfd, events, 10, timeout);
        if (num > 0)
        {
            for (int i = 0; i < num; i++)
            {
                // serial closed?
                if ((events[i].events & EPOLLRDHUP) ||
                    (events[i].events & (EPOLLERR | EPOLLHUP)))
                {
                    std::cerr << "get event: 0x" << std::hex << events[i].events << std::endl;
                    return -1;
                }

                if (events[i].events & EPOLLIN)
                {
                    memset(buffer, 0, max_buf_size);
                    bufsize = read(ttyfd, buffer, max_buf_size);
                    return bufsize;
                }
            }
        }

        return -1;
    }
};