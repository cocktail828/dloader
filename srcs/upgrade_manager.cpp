/*
 * @Author: sinpo828
 * @Date: 2021-02-08 11:36:51
 * @LastEditors: sinpo828
 * @LastEditTime: 2021-02-19 15:08:09
 * @Description: file content
 */

#include <string>

extern "C"
{
#include <unistd.h>
}

#include "packets.hpp"
#include "serial.hpp"
#include "upgrade_manager.hpp"
#include "scopeguard.hpp"

UpgradeManager::UpgradeManager(const std::string &tty,
                               const std::string &pac)
    : serial(tty), firmware(pac)
{
}

UpgradeManager::~UpgradeManager()
{
}

bool UpgradeManager::prepare()
{
    if (firmware.pacparser())
        return false;

    if (firmware.xmlparser())
        return false;

    return true;
}

void UpgradeManager::hexdump(bool isreq, uint8_t *buf, uint32_t sz)
{
    char _buff[1024] = {'\0'};

    if (getenv("VERBOSE"))
    {
        snprintf(reinterpret_cast<char *>(_buff) + strlen(_buff),
                 sizeof(_buff), "%s %s (%04d)", isreq ? ">>>" : "<<<",
                 isreq ? req.typestr().c_str() : resp.typestr().c_str(), sz);

        for (int i = 0; i < sz && i < 32; i++)
            snprintf(reinterpret_cast<char *>(_buff) + strlen(_buff),
                     sizeof(_buff), "%02x ", buf[i]);

        if (sz >= 32)
            snprintf(reinterpret_cast<char *>(_buff) + strlen(_buff),
                     sizeof(_buff), "... %02x %02x %02x",
                     buf[sz - 3], buf[sz - 2], buf[sz - 1]);
        std::cerr << _buff << std::endl;
    }
    else
    {
        static REQTYPE pre_reqtype = REQTYPE::BSL_CMD_CHECK_BAUD;

        if (pre_reqtype == REQTYPE::BSL_CMD_MIDST_DATA &&
            isreq && req.type() == REQTYPE::BSL_CMD_MIDST_DATA)
        {
            std::cerr << ">";
            return;
        }
        else if (pre_reqtype == REQTYPE::BSL_CMD_MIDST_DATA &&
                 !isreq && resp.type() == REPTYPE::BSL_REP_ACK)
        {
            std::cerr << "<";
            return;
        }

        if (pre_reqtype == REQTYPE::BSL_CMD_MIDST_DATA)
            std::cerr << std::endl;

        snprintf(reinterpret_cast<char *>(_buff) + strlen(_buff),
                 sizeof(_buff), "%s %s", isreq ? ">>>" : "<<<",
                 isreq ? req.typestr().c_str() : resp.typestr().c_str());

        std::cerr << _buff << std::endl;

        pre_reqtype = isreq ? req.type() : pre_reqtype;
    }
}

bool UpgradeManager::talk()
{
    hexdump(true, req.data(), req.datalen());
    if (!serial.sendSync(req.data(), req.datalen()))
    {
        std::cerr << "sendSync failed" << std::endl;
        return false;
    }

    if (!serial.recvSync(3000))
    {
        resp.reset();
        std::cerr << "recvSync failed" << std::endl;
        return false;
    }

    resp.parser(serial.data(), serial.datalen());
    hexdump(false, resp.data(), resp.datalen());

    return true;
}

int UpgradeManager::connect()
{
    int max_try = 10;

    serial.setBaud(BAUD::BAUD115200);
    do
    {
        usleep(1000);
        req.newCheckBaud();
        if (!talk())
        {
            std::cerr << __func__ << " talk failed with baud 115200" << std::endl;
            if (--max_try <= 0)
                return -1;
        }
        else
        {
            break;
        }
    } while (1);

    if (resp.type() != REPTYPE::BSL_REP_VER)
        return -1;

    req.newConnect();
    if (!talk())
    {
        std::cerr << __func__ << " talk failed" << std::endl;
        return -1;
    }

    if (resp.type() != REPTYPE::BSL_REP_ACK)
        return -1;

    return 0;
}

int UpgradeManager::transfer(const XMLFileInfo &info, uint32_t maxlen)
{
    uint32_t filesz = firmware.file_size(info.fileid);
    size_t offset = 0;
    uint8_t *buffer = new (std::nothrow) uint8_t[maxlen];

    std::cerr << "try transfer " << info.fileid << std::endl;

    ON_SCOPE_EXIT
    {
        if (buffer)
            delete[] buffer;
        buffer = nullptr;
    };

    req.newStartData(info.base, info.realsize);
    if (!talk())
    {
        std::cerr << __func__ << " newStartData talk failed" << std::endl;
        return -1;
    }

    if (resp.type() != REPTYPE::BSL_REP_ACK)
        return -1;

    do
    {
        uint32_t txlen = (filesz > maxlen) ? maxlen : filesz;
        firmware.get_data(info.fileid, offset, buffer, txlen);
        req.newMidstData(buffer, txlen);
        if (!talk())
        {
            std::cerr << __func__ << " newMidstData talk failed" << std::endl;
            return -1;
        }

        if (resp.type() != REPTYPE::BSL_REP_ACK)
            return -1;

        filesz -= txlen;
        offset += txlen;
    } while (filesz > 0);

    req.newEndData();
    if (!talk())
    {
        std::cerr << __func__ << " newEndData talk failed" << std::endl;
        return -1;
    }

    if (resp.type() != REPTYPE::BSL_REP_ACK)
        return -1;

    std::cerr << __func__ << " transfer " << info.fileid << " finished" << std::endl;
    return 0;
}

int UpgradeManager::exec()
{
    req.newExecData();
    if (!talk())
    {
        std::cerr << __func__ << " newExecData talk failed" << std::endl;
        return -1;
    }

    if (resp.type() != REPTYPE::BSL_REP_ACK)
    {
        std::cerr << __func__ << " newExecData get unexpect response: "
                  << resp.typestr() << ", code=" << static_cast<int>(resp.type()) << std::endl;
        return -1;
    }

    return 0;
}

/**
 * MAX_DATA_LEN defines in packets.hpp should not less than those lens
 */
#define FRAMESZ_BOOTCODE 0x210 // frame size for bootcode
#define FRAMESZ_FDL 0x840      // frame size for fdl1
#define FRAMESZ_DATA 0x1000    // frame size for others
int UpgradeManager::flash_partition(const XMLFileInfo &info)
{
    if (info.fileid == "FDL")
    {
        req.set_crc(CRC_MODLE::CRC_BOOTCODE);
        if (connect())
            goto _err;

        if (transfer(info, FRAMESZ_BOOTCODE))
            goto _err;

        if (exec())
            goto _err;
    }

    else if (info.fileid == "FDL2")
    {
        req.set_crc(CRC_MODLE::CRC_FDL);
        if (connect())
            goto _err;

        if (transfer(info, FRAMESZ_FDL))
            goto _err;

        if (exec())
            goto _err;
    }
    else
    {
        return 0;
    }

    return 0;

_err:
    std::cerr << __func__ << " fail to flash " << info.fileid << std::endl;
    return -1;
}

int UpgradeManager::upgrade_udx710()
{
    auto vec = firmware.get_file_vec();
    if (vec.empty())
    {
        std::cerr << __func__ << " failed for empty vector" << std::endl;
        return -1;
    }

    for (auto iter = vec.begin(); iter != vec.end(); iter++)
    {
        if (flash_partition(*iter))
            return -1;
        sleep(1);
    }

    req.newNormalReset();
    talk();

    std::cerr << __func__ << " finished" << std::endl;
    return 0;
}
