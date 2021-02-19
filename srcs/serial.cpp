/*
 * @Author: sinpo828
 * @Date: 2021-02-08 11:41:38
 * @LastEditors: sinpo828
 * @LastEditTime: 2021-02-19 14:26:42
 * @Description: file content
 */

#include <iostream>
#include <string>
#include <cstring>

extern "C"
{
#include <fcntl.h>
#include <termios.h>
#include <sys/epoll.h>
#include <unistd.h>
}

#include "serial.hpp"

SerialPort::SerialPort(const std::string &tty)
    : ttydev(tty), ttyfd(-1), epfd(-1), buffer(nullptr), bufsize(0)
{
    std::cerr << "serial try open " << ttydev << std::endl;
    ttyfd = open(ttydev.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (ttyfd > 0)
    {
        epoll_event event;
        std::cerr << "serial successfully open " << ttydev << std::endl;
        buffer = new (std::nothrow) uint8_t[max_buf_size]();

        epfd = epoll_create(10);
        memset(&event, 0, sizeof(event));
        event.data.fd = ttyfd;
        event.events = EPOLLIN | EPOLLRDHUP;
        if (epoll_ctl(epfd, EPOLL_CTL_ADD, ttyfd, &event))
            std::cerr << "epoll_ctl EPOLL_CTL_ADD fails, ttyfd=" << ttyfd
                      << ", error=" << strerror(errno) << std::endl;
    }
}

SerialPort::~SerialPort()
{
    if (buffer)
        delete[] buffer;
    buffer = nullptr;

    if (epfd > 0)
    {
        struct epoll_event ev;
        ev.data.fd = ttyfd;
        ev.events = EPOLLIN;
        epoll_ctl(epfd, EPOLL_CTL_DEL, ttyfd, &ev);
        close(epfd);
        epfd = -1;
    }

    if (ttyfd > 0)
    {
        // do not know why close will take too much time
        close(ttyfd);
        ttyfd = -1;
    }
}

bool SerialPort::isOpened()
{
    return (access(ttydev.c_str(), F_OK) == 0) && (ttyfd > 0);
}

void SerialPort::setBaud(BAUD baud)
{
    struct termios tio;
    struct termios settings;

    std::cerr << "serial set baud " << std::dec << BaudARR[static_cast<int>(baud)] << std::endl;
    memset(&tio, 0, sizeof(tio));
    tio.c_iflag = 0;
    tio.c_oflag = 0;
    tio.c_cflag = CS8 | CREAD | CLOCAL; // 8n1, see termios.h for more information
    tio.c_lflag = 0;
    tio.c_cc[VMIN] = 1;
    tio.c_cc[VTIME] = 5;
    cfsetospeed(&tio, BaudARR[static_cast<int>(baud)]);
    cfsetispeed(&tio, BaudARR[static_cast<int>(baud)]);
    tcsetattr(ttyfd, TCSANOW, &tio);

    memset(&settings, 0, sizeof(settings));
    cfmakeraw(&settings);
    settings.c_cflag |= CREAD | CLOCAL;
    tcflush(ttyfd, TCIOFLUSH);
    tcsetattr(ttyfd, TCSANOW, &settings);
}

bool SerialPort::sendSync(uint8_t *data, uint16_t len)
{
    ssize_t ret = write(ttyfd, data, len);

    if (ret <= 0)
    {
        std::cerr << "write data failed for " << strerror(errno) << std::endl;
        return false;
    }

    if (ret != len)
    {
        std::cerr << "write data implement, write "
                  << std::dec << ret << "/" << len << " bytes" << std::endl;
        return false;
    }

    return true;
}

bool SerialPort::recvSync(uint32_t timeout)
{
    struct epoll_event events[10];
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
                return false;
            }

            if (events[i].events & EPOLLIN)
            {
                memset(buffer, 0, max_buf_size);
                ssize_t len = read(ttyfd, buffer, max_buf_size);
                if (len <= 0)
                    std::cerr << __func__ << " read " << std::dec << len
                              << " bytes, err " << strerror(errno) << std::endl;
                bufsize = (len <= 0) ? 0 : len;
                return (bufsize > 0);
            }
        }
    }
    if (num == 0)
    {
        std::cerr << "epoll timeout" << std::endl;
        return false;
    }
    else
    {
        std::cerr << "epoll fail for " << strerror(errno) << std::endl;
        return false;
    }
}

uint8_t *SerialPort::data()
{
    return buffer;
}

uint16_t SerialPort::datalen()
{
    return bufsize;
}
