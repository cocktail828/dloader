/*
 * @Author: sinpo828
 * @Date: 2021-02-05 08:54:33
 * @LastEditors: sinpo828
 * @LastEditTime: 2021-02-23 13:39:34
 * @Description: file content
 */
#ifndef __UPDATE__
#define __UPDATE__

#include <string>

#include "packets.hpp"
#include "serial.hpp"
#include "firmware.hpp"

/**
 * MAX_DATA_LEN defines in packets.hpp should not less than those lens
 */
#define FRAMESZ_BOOTCODE 0x210 // frame size for bootcode
#define FRAMESZ_FDL 0x840      // frame size for fdl1
#define FRAMESZ_DATA 0x3000    // frame size for others

class UpgradeManager
{
private:
    SerialPort serial;
    Response resp;
    Request req;
    Firmware firmware;
    std::string pac;
    uint8_t *_data;

private:
    void hexdump(bool isreq, uint8_t *buf, uint32_t sz);
    bool talk(int timeout = 5000);
    int connect();
    int transfer(const XMLFileInfo &info, uint32_t maxlen, bool isfdl = false);
    int exec();
    void checksum(XMLFileInfo &info);

public:
    UpgradeManager(const std::string &tty, const std::string &pac);
    ~UpgradeManager();

    // do some preparetion, parser xml, init tty or something
    bool prepare();

    int backup_partition(XMLFileInfo &info);
    int flash_fdl(const XMLFileInfo &info, bool isbootcode);
    int flash_partition(const XMLFileInfo &info);
    int erase_partition(const XMLFileInfo &info);

    int upgrade_udx710(bool backup = false);
};

#endif //__UPDATE__