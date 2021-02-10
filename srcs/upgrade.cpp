/*
 * @Author: sinpo828
 * @Date: 2021-02-08 11:36:51
 * @LastEditors: sinpo828
 * @LastEditTime: 2021-02-10 09:05:08
 * @Description: file content
 */

#include <string>

#include "packets.hpp"
#include "serial.hpp"
#include "upgrade.hpp"

Upgrade::Upgrade(const std::string &tty,
                 const std::string &mname,
                 const std::string &pac)
    : serial(tty), cmd(mname), firmware(pac) {}

Upgrade::~Upgrade()
{
}

bool Upgrade::prepare()
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

bool Upgrade::talk()
{
    hexdump(cmd.cmdstr() + " ---> ", cmd.data(), cmd.datalen());
    if (!serial.sendSync(cmd.data(), cmd.datalen()))
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
    hexdump(resp.respstr() + " <--- ", resp.data(), resp.datalen());

    return true;
}

int Upgrade::connect()
{
    cmd.newCheckBaud();
    serial.setBaud(BAUD::BAUD_B115200);
    if (!talk())
    {
        std::cerr << __func__ << " talk failed with baud 115200" << std::endl;
        return -1;
    }

    if (resp.cmdtype() != REPTYPE::BSL_REP_VER)
        return -1;

    cmd.newConnect();
    if (!talk())
    {
        std::cerr << __func__ << " talk failed" << std::endl;
        return -1;
    }

    if (resp.cmdtype() != REPTYPE::BSL_REP_ACK)
        return -1;

    return 0;
}

int Upgrade::transfer(const XMLFileInfo &info)
{
    int idx = firmware.fileidstr_to_index(info.fileid);
    uint32_t filesz = firmware.file_size_by_idx(idx);

    cmd.newStartData(info.base, info.realsize);
    if (!talk())
    {
        std::cerr << __func__ << " newStartData talk failed" << std::endl;
        return -1;
    }

    if (resp.cmdtype() != REPTYPE::BSL_REP_ACK)
        return -1;

    firmware.open(idx);
    do
    {
        uint8_t buffer[2 * 1024];
        uint32_t sz = (filesz > sizeof(buffer)) ? sizeof(buffer) : filesz;

        firmware.read(buffer, sz);
        cmd.newMidstData(buffer, sz);
        if (!talk())
        {
            std::cerr << __func__ << " newMidstData talk failed" << std::endl;
            return -1;
        }

        if (resp.cmdtype() != REPTYPE::BSL_REP_ACK)
            return -1;
    } while (1);
    firmware.close();

    cmd.newEndData();
    if (!talk())
    {
        std::cerr << __func__ << " newEndData talk failed" << std::endl;
        return -1;
    }

    if (resp.cmdtype() != REPTYPE::BSL_REP_ACK)
        return -1;

    std::cerr << __func__ << " finished" << std::endl;
    return 0;
}

int Upgrade::level1()
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
