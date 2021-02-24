/*
 * @Author: sinpo828
 * @Date: 2021-02-05 08:54:33
 * @LastEditors: sinpo828
 * @LastEditTime: 2021-02-24 10:41:13
 * @Description: file content
 */
#ifndef __UPDATE__
#define __UPDATE__

#include <string>

#include "fdl.hpp"
#include "pdl.hpp"
#include "serial.hpp"
#include "firmware.hpp"

/**
 * MAX_DATA_LEN defines in packets.hpp should not less than those lens
 */
#define FRAMESZ_BOOTCODE 0x210 // frame size for bootcode
#define FRAMESZ_PDL 0x800      // frame size for PDL
#define FRAMESZ_FDL 0x840      // frame size for fdl1
#define FRAMESZ_DATA 0x3000    // frame size for others

class UpgradeManager
{
private:
    SerialPort serial;
    FDLRequest request;
    FDLResponse response;
    Firmware firmware;
    std::string pac;
    uint8_t *_data;

private:
    void hexdump(const std::string &prefix, uint8_t *buf, uint32_t len, uint32_t dumplen = 20);
    void verbose(CMDRequest *req);
    void verbose(CMDResponse *resp, bool ondata);
    bool talk(CMDRequest *req, CMDResponse *resp, int timeout = 5000);
    int connect();
    int transfer(const XMLFileInfo &info, uint32_t maxlen, bool isoldproto = false);
    int exec();
    void checksum(XMLFileInfo &info);

public:
    UpgradeManager(const std::string &tty, const std::string &pac);
    ~UpgradeManager();

    // do some preparetion, parser xml, init tty or something
    bool prepare();

    int backup_partition(XMLFileInfo &info);
    int flash_pdl(const XMLFileInfo &info);
    int flash_fdl(const XMLFileInfo &info);
    int flash_nand_fdl(const XMLFileInfo &info);
    int flash_partition(const XMLFileInfo &info);
    int erase_partition(const XMLFileInfo &info);

    int upgrade(const std::string &chiset, bool backup = false);
};

#endif //__UPDATE__