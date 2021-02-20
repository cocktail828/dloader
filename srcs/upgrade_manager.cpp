/*
 * @Author: sinpo828
 * @Date: 2021-02-08 11:36:51
 * @LastEditors: sinpo828
 * @LastEditTime: 2021-02-20 19:46:34
 * @Description: file content
 */

#include <string>
#include <algorithm>

extern "C"
{
#include <unistd.h>
}

#include "packets.hpp"
#include "serial.hpp"
#include "upgrade_manager.hpp"
#include "scopeguard.hpp"

bool string_case_cmp(const std::string &s1, const std::string &s2)
{
    std::string s1_upper(s1);
    std::string s2_upper(s2);
    std::transform(s1.begin(), s1.end(), s1_upper.begin(), toupper);
    std::transform(s2.begin(), s2.end(), s2_upper.begin(), toupper);

    return s1_upper == s2_upper;
}

UpgradeManager::UpgradeManager(const std::string &tty,
                               const std::string &pac)
    : serial(tty), firmware(pac), pac(pac)
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
                 sizeof(_buff), "%s %s %s (%04d)", isreq ? ">>>" : "<<<",
                 isreq ? req.typestr().c_str() : resp.typestr().c_str(),
                 isreq ? req.argstr().c_str() : "", sz);

        for (int i = 0; i < sz && i < 32; i++)
            snprintf(reinterpret_cast<char *>(_buff) + strlen(_buff),
                     sizeof(_buff), "%02x ", buf[i]);

        if (sz >= 32)
            snprintf(reinterpret_cast<char *>(_buff) + strlen(_buff),
                     sizeof(_buff), "... %02x %02x %02x %02x %02x",
                     buf[sz - 5], buf[sz - 4], buf[sz - 3], buf[sz - 2], buf[sz - 1]);
        std::cerr << _buff << std::endl;
    }
    else
    {
        static REQTYPE pre_reqtype = REQTYPE::BSL_CMD_CHECK_BAUD;
        auto REQ_CAN_SKIP0 = REQTYPE::BSL_CMD_MIDST_DATA;
        auto REQ_CAN_SKIP1 = REQTYPE::BSL_CMD_READ_PARTITION_SIZE;
        auto REP_CAN_SKIP0 = REPTYPE::BSL_REP_ACK;
        auto REP_CAN_SKIP1 = REPTYPE::BSL_REP_READ_FLASH;

        if ((pre_reqtype == REQ_CAN_SKIP0 || pre_reqtype == REQ_CAN_SKIP1) &&
            isreq && req.type() == pre_reqtype)
        {
            std::cerr << ">";
            return;
        }
        else if ((pre_reqtype == REQ_CAN_SKIP0 || pre_reqtype == REQ_CAN_SKIP1) &&
                 !isreq && (resp.type() == REP_CAN_SKIP0 || resp.type() == REP_CAN_SKIP1))
        {
            std::cerr << "<";
            return;
        }

        if (pre_reqtype == REQ_CAN_SKIP0 || pre_reqtype == REQ_CAN_SKIP1)
            std::cerr << std::endl;

        snprintf(reinterpret_cast<char *>(_buff) + strlen(_buff),
                 sizeof(_buff), "%s %s %s", isreq ? ">>>" : "<<<",
                 isreq ? req.typestr().c_str() : resp.typestr().c_str(),
                 isreq ? req.argstr().c_str() : "");

        std::cerr << _buff << std::endl;

        pre_reqtype = isreq ? req.type() : pre_reqtype;
    }
}

bool UpgradeManager::talk(int timeout)
{
    int remainlen = 0;
    if (req.type() == REQTYPE::BSL_CMD_READ_PARTITION_SIZE)
    {
        auto hdr = FRAMEHDR(req.rawdata());
        remainlen = be16toh(hdr->data_length);
    }

    hexdump(true, req.rawdata(), req.rawdatalen());
    if (!serial.sendSync(req.rawdata(), req.rawdatalen()))
    {
        std::cerr << "sendSync failed, req=" << req.typestr() << std::endl;
        return false;
    }

    do
    {
        if (!serial.recvSync(timeout))
        {
            resp.reset();
            std::cerr << "recvSync failed, req=" << req.typestr() << std::endl;
            return false;
        }

        remainlen -= static_cast<int>(serial.datalen());
    } while (remainlen > 0);

    resp.parser(serial.data(), serial.datalen());
    hexdump(false, resp.rawdata(), resp.rawdatalen());

    return true;
}

int UpgradeManager::connect()
{
    int max_try = 10;

    serial.setBaud(BAUD::BAUD115200);
    do
    {
        usleep(1000);
        req.newCheckBaud(BaudARRSTR[static_cast<int>(BAUD::BAUD115200)]);
        if (!talk())
        {
            if (--max_try <= 0)
                return -1;
        }
        else
        {
            break;
        }
    } while (1);

    if (resp.type() != REPTYPE::BSL_REP_VER)
        goto _exit;

    req.newConnect();
    if (!talk() || resp.type() != REPTYPE::BSL_REP_ACK)
        goto _exit;

    return 0;

_exit:
    std::cerr << __func__ << " " << req.typestr()
              << " get unexpect response " << resp.typestr() << std::endl;
    return -1;
}

int UpgradeManager::transfer(const XMLFileInfo &info, uint32_t maxlen, bool isfdl)
{
    uint32_t filesz = firmware.file_size(info.fileid);
    size_t offset = 0;
    uint8_t *buffer = new (std::nothrow) uint8_t[maxlen];

    ON_SCOPE_EXIT
    {
        if (buffer)
            delete[] buffer;
        buffer = nullptr;
    };

    if (isfdl)
        req.newStartData(info.base, info.realsize);
    else
        req.newStartData(info.blockid, info.realsize);

    if (!talk() || resp.type() != REPTYPE::BSL_REP_ACK)
        goto _exit;

    do
    {
        uint32_t txlen = (filesz > maxlen) ? maxlen : filesz;
        firmware.get_data(info.fileid, offset, buffer, txlen);
        req.newMidstData(buffer, txlen);
        if (!talk() || resp.type() != REPTYPE::BSL_REP_ACK)
            goto _exit;

        filesz -= txlen;
        offset += txlen;
    } while (filesz > 0);

    req.newEndData();
    if (!talk() || resp.type() != REPTYPE::BSL_REP_ACK)
        goto _exit;

    return 0;

_exit:
    std::cerr << __func__ << " " << req.typestr()
              << " get unexpect response " << resp.typestr() << std::endl;
    return -1;
}

int UpgradeManager::exec()
{
    req.newExecData();
    if (!talk() || resp.type() != REPTYPE::BSL_REP_ACK)
    {
        std::cerr << __func__ << " " << req.typestr()
                  << " get unexpect response " << resp.typestr() << std::endl;
        return -1;
    }

    return 0;
}

std::string get_real_path(const std::string &pac)
{
    std::string fpath = ".";

    if (pac.empty() || pac.find_last_of('/') == std::string::npos)
        return fpath;
    return pac.substr(0, pac.find_last_of('/'));
}

int UpgradeManager::backup_partition(XMLFileInfo &info)
{
    uint32_t totalsz = 0;
    uint32_t partitionsz = info.size;
    std::string name = get_real_path(pac) + "/" + info.blockid + ".bak";
    std::ofstream fout(name, std::ios::trunc);

    if (!fout.is_open())
    {
        std::cerr << __func__ << " cannot backup partition " << info.blockid
                  << " for fail to open(write) " << name << std::endl;
        return -1;
    }
    info.fpath = name;

    ON_SCOPE_EXIT
    {
        fout.close();
    };

    req.newStartReadPartition(info.blockid, info.size);
    if (!talk() || resp.type() != REPTYPE::BSL_REP_ACK)
        goto _exit;

    do
    {
        uint32_t sz = (partitionsz > FRAMESZ_DATA) ? FRAMESZ_DATA : partitionsz;

        req.newReadPartitionSize(sz, totalsz);
        if (!talk() || resp.type() != REPTYPE::BSL_REP_READ_FLASH)
            goto _exit;

        fout.write(reinterpret_cast<char *>(resp.data()), resp.datalen());
        partitionsz -= sz;
        totalsz += sz;
    } while (partitionsz > 0);

    req.newEndReadPartition();
    if (!talk() || resp.type() != REPTYPE::BSL_REP_ACK)
        goto _exit;

    return 0;
_exit:
    return -1;
}

int UpgradeManager::flash_fdl(const XMLFileInfo &info, bool isbootcode)
{
    int ret;

    req.set_crc(isbootcode ? CRC_MODLE::CRC_BOOTCODE : CRC_MODLE::CRC_FDL);
    if (connect())
        goto _exit;

    if (transfer(info, isbootcode ? FRAMESZ_BOOTCODE : FRAMESZ_FDL, true))
        goto _exit;

    ret = exec();
    if (isbootcode)
        return ret;

    if (!ret || resp.type() == REPTYPE::BSL_REP_INCOMPATIBLE_PARTITION)
    {
        auto table = firmware.get_partition_vec();
        if (table.empty())
        {
            std::cerr << __func__ << " failed for empty vector" << std::endl;
            return -1;
        }

        req.newExecNandInit();
        if (!talk() || resp.type() != REPTYPE::BSL_REP_ACK)
            goto _exit;

        req.newWritePartitionTable(table);
        if (!talk() || resp.type() != REPTYPE::BSL_REP_ACK)
            goto _exit;
    }
    return 0;

_exit:
    return -1;
}

int UpgradeManager::flash_partition(const XMLFileInfo &info)
{
    return transfer(info, FRAMESZ_DATA, false);
}

int UpgradeManager::erase_partition(const XMLFileInfo &info)
{
    req.newErasePartition(info.blockid);
    return (talk() && resp.type() == REPTYPE::BSL_REP_ACK) ? 0 : -1;
}

int UpgradeManager::upgrade_udx710(bool backup)
{
    auto filevec = firmware.get_file_vec();
    if (filevec.empty())
    {
        std::cerr << __func__ << " failed for empty vector" << std::endl;
        return -1;
    }

    // FDL
    for (auto iter = filevec.begin(); iter != filevec.end(); iter++)
    {
        if (iter->isBackup)
            continue;

        if (string_case_cmp(iter->fileid, "FDL"))
        {
            if (flash_fdl(*iter, true))
                return -1;
        }
        else if (string_case_cmp(iter->fileid, "FDL2"))
        {
            if (flash_fdl(*iter, false))
                return -1;
        }
    }

    // Backup
    for (auto iter = filevec.begin(); iter != filevec.end(); iter++)
    {
        if (iter->isBackup)
            if (backup_partition(*iter))
                return -1;
    }

    for (auto iter = filevec.begin(); iter != filevec.end(); iter++)
    {
        if (iter->isBackup || string_case_cmp(iter->fileid, "FDL") ||
            string_case_cmp(iter->fileid, "FDL2"))
            continue;

        if (iter->type == "EraseFlash2")
        {
            if (erase_partition(*iter))
                return -1;
        }
        else
        {
            if (flash_partition(*iter))
                return -1;
        }
    }

    req.newNormalReset();
    talk();

    std::cerr << __func__ << " finished" << std::endl;
    return 0;
}
