/*
 * @Author: sinpo828
 * @Date: 2021-02-05 08:54:33
 * @LastEditors: sinpo828
 * @LastEditTime: 2021-02-08 14:39:56
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
    Upgrade(const std::string &tty, const std::string &mname,
            const std::string &pac, const std::string &ext_dir);
    ~Upgrade();

    bool talk();
    int connect();

    int transfer(const XMLFileInfo &info);

    int upgrade_norflash();
    int upgrade_nand_emmc();
};

#endif //__UPDATE__