/*
 * @Author: sinpo828
 * @Date: 2021-02-08 11:36:51
 * @LastEditors: sinpo828
 * @LastEditTime: 2021-02-08 14:40:05
 * @Description: file content
 */

#include <string>

#include "packets.hpp"
#include "serial.hpp"
#include "update.hpp"

Upgrade::Upgrade(const std::string &tty, const std::string &mname,
                 const std::string &pac, const std::string &ext_dir)
    : serial(tty), cmd(mname), firmware(pac, ext_dir) {}

Upgrade::~Upgrade() {}

bool Upgrade::talk()
{
    serial.sendSync(cmd.data(), cmd.datalen());
    serial.recvSync(30);
    resp.parser(serial.data(), serial.datalen());

    return true;
}

int Upgrade::connect()
{
    do
    {
        cmd.newCheckBaud();
        do
        {
            talk();
            if (resp.cmdtype() == REPTYPE::BSL_REP_VER)
                break;
        } while (1);

        cmd.newConnect();
        talk();
        if (resp.cmdtype() != REPTYPE::BSL_REP_ACK)
            break;

        // cmd.newChangeBaud();
        // talk();
        // if (resp.cmdtype() != REPTYPE::BSL_REP_ACK)
        //     break;

        return 0;
    } while (0);

    std::cerr << __func__ << "-> response with type: 0x"
              << std::hex << static_cast<int>(resp.cmdtype()) << std::endl;
    return -1;
}

int Upgrade::transfer(const XMLFileInfo &info)
{
    do
    {
        cmd.newStartData(info.base, info.size);
        talk();
        if (resp.cmdtype() != REPTYPE::BSL_REP_ACK)
            break;

        int idx = firmware.fileidstr_to_index(info.fileid);
        firmware.open(idx);
        uint32_t filesz = firmware.file_size_by_idx(idx);
        do
        {
            uint8_t buffer[2 * 1024];
            uint32_t sz = (filesz > sizeof(buffer)) ? sizeof(buffer) : filesz;

            firmware.read(buffer, sz);
            cmd.newMidstData(buffer, sz);
            talk();
            if (resp.cmdtype() != REPTYPE::BSL_REP_ACK)
                break;
        } while (1);
        firmware.close();

        cmd.newEndData();
        talk();
        if (resp.cmdtype() != REPTYPE::BSL_REP_ACK)
            break;

        return 0;
    } while (0);

    std::cerr << __func__ << "-> response with type: 0x"
              << std::hex << static_cast<int>(resp.cmdtype()) << std::endl;
    return -1;
}

int Upgrade::upgrade_norflash()
{
    std::cerr << __func__ << " start" << std::endl;
    if (connect())
        return -1;

    return 0;
}

int Upgrade::upgrade_nand_emmc()
{
    std::cerr << __func__ << " start" << std::endl;
    if (connect())
        return -1;

    // if (transfer("fdl2"))
    //     return -1;

    return 0;
}
