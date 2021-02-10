/*
 * @Author: sinpo828
 * @Date: 2021-02-08 11:36:51
 * @LastEditors: sinpo828
 * @LastEditTime: 2021-02-10 15:10:13
 * @Description: file content
 */

#include <string>

#include "packets.hpp"
#include "serial.hpp"
#include "upgrade_manager.hpp"

UpgradeManager::UpgradeManager(const std::string &tty,
                               const std::string &pac)
    : serial(tty), firmware(pac) {}

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

static void hexdump(const std::string &s, uint8_t *buf, uint32_t sz)
{
    char _buff[1024] = {'\0'};

    snprintf(_buff + strlen(_buff), sizeof(_buff), "%s ", s.c_str());
    if (getenv("VERBOSE"))
    {
        snprintf(_buff + strlen(_buff), sizeof(_buff), "(%02d) ", sz);
        for (int i = 0; i < sz && i < 100; i++)
            snprintf(_buff + strlen(_buff), sizeof(_buff), "%02x ", buf[i]);
    }
    std::cerr << _buff << std::endl;
}

bool UpgradeManager::talk()
{
    hexdump(req.typestr() + " ---> ", req.data(), req.datalen());
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
    hexdump(resp.typestr() + " <--- ", resp.data(), resp.datalen());

    return true;
}

int UpgradeManager::connect()
{
    req.newCheckBaud();
    serial.setBaud(BAUD::BAUD115200);
    if (!talk())
    {
        std::cerr << __func__ << " talk failed with baud 115200" << std::endl;
        return -1;
    }

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

int UpgradeManager::transfer(const XMLFileInfo &info)
{
    uint32_t filesz = firmware.file_size(info.fileid);
    size_t offset = 0;

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
        uint8_t buffer[2 * 1024];
        uint32_t sz = (filesz > sizeof(buffer)) ? sizeof(buffer) : filesz;

        firmware.get_data(info.fileid, offset, buffer, sz);
        req.newMidstData(buffer, sz);
        if (!talk())
        {
            std::cerr << __func__ << " newMidstData talk failed" << std::endl;
            return -1;
        }

        if (resp.type() != REPTYPE::BSL_REP_ACK)
            return -1;

        filesz -= sz;
        offset += sz;
    } while (1);

    req.newEndData();
    if (!talk())
    {
        std::cerr << __func__ << " newEndData talk failed" << std::endl;
        return -1;
    }

    if (resp.type() != REPTYPE::BSL_REP_ACK)
        return -1;

    std::cerr << __func__ << " finished" << std::endl;
    return 0;
}

int UpgradeManager::upgrade(const std::string &mname)
{
    auto vec = firmware.get_file_vec();
    if (vec.empty())
    {
        std::cerr << __func__ << " failed for empty vector" << std::endl;
        return -1;
    }

    if (connect())
        return -1;

    for (auto iter = vec.begin(); iter != vec.end(); iter++)
    {
        if (iter->fileid == "HOST_FDL")
            continue;

        transfer(*iter);
    }

    std::cerr << __func__ << " finished" << std::endl;
    return 0;
}
