/*
 * @Author: sinpo828
 * @Date: 2021-02-05 08:54:33
 * @LastEditors: sinpo828
 * @LastEditTime: 2021-02-07 09:22:28
 * @Description: file content
 */
#include <string>

#include <packets.hpp>
#include <serial.hpp>

class Upgrade
{
private:
    SerialPort serial;
    Response resp;
    Command cmd;

public:
    Upgrade(const std::string &tty) : serial(tty), cmd("old") {}

    bool talk()
    {
        serial.sendSync(cmd.data(), cmd.datalen());
        serial.recvSync(30);
        resp.parser(serial.data(), serial.datalen());

        return true;
    }

    int connect()
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

            cmd.newChangeBaud();
            talk();
            if (resp.cmdtype() != REPTYPE::BSL_REP_ACK)
                break;

            return 0;
        } while (0);

        std::cerr << __func__ << "-> response with type: 0x"
                  << std::hex << static_cast<int>(resp.cmdtype()) << std::endl;
        return -1;
    }

    int transfer(const std::string &file)
    {
        do
        {
            cmd.newStartData();
            talk();
            if (resp.cmdtype() != REPTYPE::BSL_REP_ACK)
                break;

            do
            {
                cmd.newMidstData();
                talk();
                if (resp.cmdtype() != REPTYPE::BSL_REP_ACK)
                    break;
            } while (1);

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

    int upgrade_norflash()
    {
        std::cerr << __func__ << " start" << std::endl;
        if (connect())
            return -1;
    }

    int upgrade_nand_emmc()
    {
        std::cerr << __func__ << " start" << std::endl;
        if (connect())
            return -1;

        if (transfer("fdl2"))
            return -1;
    }
};