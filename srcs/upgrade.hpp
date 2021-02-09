/*
 * @Author: sinpo828
 * @Date: 2021-02-05 08:54:33
 * @LastEditors: sinpo828
 * @LastEditTime: 2021-02-09 15:35:53
 * @Description: file content
 */
#ifndef __UPDATE__
#define __UPDATE__

#include <string>

#include "packets.hpp"
#include "serial.hpp"
#include "firmware.hpp"

class Upgrade
{
private:
    SerialPort serial;
    Response resp;
    Command cmd;
    Firmware firmware;

public:
    Upgrade(const std::string &tty,
            const std::string &mname,
            const std::string &pac);
    ~Upgrade();

    bool prepare();

    bool talk();
    int connect();

    int transfer(const XMLFileInfo &info);

    int level1();
    int level2();
};

#endif //__UPDATE__