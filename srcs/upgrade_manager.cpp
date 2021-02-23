/*
 * @Author: sinpo828
 * @Date: 2021-02-08 11:36:51
 * @LastEditors: sinpo828
 * @LastEditTime: 2021-02-23 13:56:51
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
    int maxlen = FRAMESZ_DATA > FRAMESZ_FDL ? FRAMESZ_DATA : FRAMESZ_FDL;
    _data = new (std::nothrow) uint8_t[maxlen];
}

UpgradeManager::~UpgradeManager()
{
    if (_data)
        delete[] _data;
    _data = nullptr;
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

        for (int i = 0; i < sz && i < 20; i++)
            snprintf(reinterpret_cast<char *>(_buff) + strlen(_buff),
                     sizeof(_buff), "%02x ", buf[i]);

        if (sz >= 20)
            snprintf(reinterpret_cast<char *>(_buff) + strlen(_buff),
                     sizeof(_buff), "... %02x %02x %02x %02x %02x",
                     buf[sz - 5], buf[sz - 4], buf[sz - 3], buf[sz - 2], buf[sz - 1]);
        std::cerr << _buff << std::endl;
    }
    else
    {
        static REQTYPE pre_reqtype = REQTYPE::BSL_CMD_CHECK_BAUD;

        if (isreq && pre_reqtype == REQTYPE::BSL_CMD_MIDST_DATA && req.type() == pre_reqtype)
        {
            std::cerr << ">";
            return;
        }

        if (isreq && pre_reqtype == REQTYPE::BSL_CMD_READ_PARTITION_SIZE && req.type() == pre_reqtype)
        {
            std::cerr << "<";
            return;
        }

        if (!isreq && (pre_reqtype == REQTYPE::BSL_CMD_MIDST_DATA || pre_reqtype == REQTYPE::BSL_CMD_READ_PARTITION_SIZE) &&
            (resp.type() == REPTYPE::BSL_REP_ACK || resp.type() == REPTYPE::BSL_REP_READ_FLASH))
            return;

        if (pre_reqtype == REQTYPE::BSL_CMD_MIDST_DATA || pre_reqtype == REQTYPE::BSL_CMD_READ_PARTITION_SIZE)
            std::cerr << std::endl;

        snprintf(reinterpret_cast<char *>(_buff) + strlen(_buff),
                 sizeof(_buff), "%s %s(0x%02x) %s", isreq ? ">>>" : "<<<",
                 isreq ? req.typestr().c_str() : resp.typestr().c_str(),
                 isreq ? static_cast<int>(req.type()) : static_cast<int>(resp.type()),
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
        remainlen = le32toh(*reinterpret_cast<uint32_t *>(req.rawdata() + sizeof(cmd_header))) +
                    sizeof(cmd_header) + sizeof(cmd_tail);
    }

    resp.reset();
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
            std::cerr << "recvSync failed, req=" << req.typestr() << std::endl;
            return false;
        }
        resp.push_back(serial.data(), serial.datalen());
        remainlen -= static_cast<int>(serial.datalen());
    } while (remainlen > 0);
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
    uint32_t filesz = info.use_pac_file ? firmware.member_file_size(info.fileid)
                                        : firmware.local_file_size(info.fpath);
    std::ifstream fin = info.use_pac_file ? firmware.open_pac(info.fileid)
                                          : firmware.open_file(info.fpath);

    bool nv_replace_byte = false;

    ON_SCOPE_EXIT
    {
        if (fin.is_open())
            fin.close();
    };

    if (isfdl)
        req.newStartData(info.base, info.realsize);
    else
        req.newStartData(info.blockid, info.realsize, info.checksum);

    if (!talk() || resp.type() != REPTYPE::BSL_REP_ACK)
        goto _exit;

    do
    {
        uint32_t txlen = (filesz > maxlen) ? maxlen : filesz;
        firmware.read(fin, _data, txlen);
        if (!nv_replace_byte && info.crc16)
        {
            *reinterpret_cast<uint16_t *>(_data) = htobe16(info.crc16);
            nv_replace_byte = true;
        }

        req.newMidstData(_data, txlen);
        if (!talk() || resp.type() != REPTYPE::BSL_REP_ACK)
            goto _exit;

        filesz -= txlen;
    } while (filesz > 0);
    firmware.close(fin);

    // this operation may take much more time, so set a much longger timeout
    req.newEndData();
    if (!talk(30000) || resp.type() != REPTYPE::BSL_REP_ACK)
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

// NV should be processed specially
void UpgradeManager::checksum(XMLFileInfo &info)
{
    auto fin = firmware.open_pac(info.fileid);
    uint32_t fsz = firmware.member_file_size(info.fileid);
    uint16_t crc = 0;
    uint32_t cs = 0;

    firmware.read(fin, _data, 2);
    fsz -= 2;
    do
    {
        uint32_t sz = (fsz > 4096) ? 4096 : fsz;
        firmware.read(fin, _data, sz);
        crc = req.crc16_nv(crc, _data, sz);

        for (auto i = 0; i < sz; i++)
            cs += _data[i];

        fsz -= sz;
    } while (fsz > 0);
    firmware.close(fin);

    cs += (crc & 0xff);
    cs += (crc & 0xff00) >> 8;
    info.checksum = cs;
    info.crc16 = crc;
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

    req.set_crc(CRC_MODLE::CRC_FDL);
    req.set_escape_flag(false, false);
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
    std::cerr << __func__ << " fail to backup " << info.blockid << std::endl;
    return -1;
}

int UpgradeManager::flash_fdl(const XMLFileInfo &info, bool isbootcode)
{
    int ret;

    req.set_crc(isbootcode ? CRC_MODLE::CRC_BOOTCODE : CRC_MODLE::CRC_FDL);
    req.set_escape_flag(true, true);
    if (connect())
        goto _exit;

    if (transfer(info, isbootcode ? FRAMESZ_BOOTCODE : FRAMESZ_FDL, true))
        goto _exit;

    ret = exec();
    if (isbootcode)
        return ret;

    if (!ret || resp.type() == REPTYPE::BSL_REP_INCOMPATIBLE_PARTITION)
    {
        req.newExecNandInit();
        if (!talk() || resp.type() != REPTYPE::BSL_REP_ACK)
            goto _exit;
    }
    return 0;

_exit:
    std::cerr << __func__ << " fail to flash " << info.fileid << std::endl;
    return -1;
}

int UpgradeManager::flash_partition(const XMLFileInfo &info)
{
    req.set_crc(CRC_MODLE::CRC_FDL);
    req.set_escape_flag(false, false);

    return transfer(info, FRAMESZ_DATA, false);
}

int UpgradeManager::erase_partition(const XMLFileInfo &info)
{
    req.set_crc(CRC_MODLE::CRC_FDL);
    req.set_escape_flag(false, false);

    req.newErasePartition(info.blockid);
    return (talk() && resp.type() == REPTYPE::BSL_REP_ACK) ? 0 : -1;
}

int UpgradeManager::upgrade_udx710(bool backup)
{
    auto table = firmware.get_partition_vec();
    auto filevec = firmware.get_file_vec();
    if (filevec.empty() || table.empty())
    {
        std::cerr << __func__ << " failed for empty vector" << std::endl;
        return -1;
    }

    // FDL
    for (auto iter = filevec.begin(); iter != filevec.end(); iter++)
    {
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

    // Bakeup
    if (backup)
    {
        for (auto iter = filevec.begin(); iter != filevec.end(); iter++)
        {
            if (iter->isBackup)
            {
                if (iter->isBackup)
                    if (backup_partition(*iter))
                        return -1;
            }
        }
    }

    // update partition table
    req.newWritePartitionTable(table);
    if (!talk() || resp.type() != REPTYPE::BSL_REP_ACK)
        return -1;

    // do update
    for (auto iter = filevec.begin(); iter != filevec.end(); iter++)
    {
        if (string_case_cmp(iter->fileid, "FDL") ||
            string_case_cmp(iter->fileid, "FDL2"))
            continue;

        if (iter->type == "EraseFlash2")
        {
            if (erase_partition(*iter))
                return -1;
        }
        else
        {
            iter->use_pac_file = true;
            // the two partition is special. we should not change it
            if (string_case_cmp(iter->fileid, "ProdNV") ||
                string_case_cmp(iter->fileid, "PhaseCheck"))
            {
                std::cerr << __func__ << " skip flashing " << iter->fileid
                          << "(" << iter->blockid << ")"
                          << " via " << iter->fpath << std::endl;
                continue;
            }

            if (string_case_cmp(iter->fileid, "NV_NR"))
                checksum(*iter);

            if (flash_partition(*iter))
                return -1;
        }
    }

    req.newNormalReset();
    talk();

    std::cerr << __func__ << " finished" << std::endl;
    return 0;
}
