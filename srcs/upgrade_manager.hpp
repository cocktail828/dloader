/*
 * @Author: sinpo828
 * @Date: 2021-02-05 08:54:33
 * @LastEditors: sinpo828
 * @LastEditTime: 2021-02-10 15:08:03
 * @Description: file content
 */
#ifndef __UPDATE__
#define __UPDATE__

#include <string>

#include "packets.hpp"
#include "serial.hpp"
#include "firmware.hpp"

class UpgradeManager
{
private:
    SerialPort serial;
    Response resp;
    Request req;
    Firmware firmware;

private:
    bool talk();
    int connect();
    int transfer(const XMLFileInfo &info);

public:
    UpgradeManager(const std::string &tty,
                   const std::string &pac);
    ~UpgradeManager();

    // do some preparetion, parser xml, init tty or something
    bool prepare();

    int upgrade(const std::string &mname);
};

#endif //__UPDATE__