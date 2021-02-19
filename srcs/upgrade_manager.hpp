/*
 * @Author: sinpo828
 * @Date: 2021-02-05 08:54:33
 * @LastEditors: sinpo828
 * @LastEditTime: 2021-02-19 14:57:50
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
    void hexdump(bool isreq, uint8_t *buf, uint32_t sz);
    bool talk();
    int connect();
    int transfer(const XMLFileInfo &info, uint32_t maxlen);
    int exec();
    int flash_partition(const XMLFileInfo &info);

public:
    UpgradeManager(const std::string &tty,
                   const std::string &pac);
    ~UpgradeManager();

    // do some preparetion, parser xml, init tty or something
    bool prepare();

    int upgrade_udx710();
};

#endif //__UPDATE__