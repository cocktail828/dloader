/*
 * @Author: sinpo828
 * @Date: 2021-02-08 11:36:51
 * @LastEditors: sinpo828
 * @LastEditTime: 2021-02-23 20:22:21
 * @Description: file content
 */

#include <string>
#include <algorithm>

extern "C"
{
#include <unistd.h>
}

#include "pdl.hpp"
#include "fdl.hpp"
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

void UpgradeManager::hexdump(const std::string &prefix, uint8_t *buf, uint32_t sz, uint32_t dumplen)
{
    char _buff[4096] = {'\0'};
    uint32_t pos = 0;

    snprintf(reinterpret_cast<char *>(_buff) + strlen(_buff),
             sizeof(_buff), "%s (%04d)\n", prefix.c_str(), sz);

    for (pos = 0; pos < sz && pos < dumplen && pos < 1024; pos++)
    {
        if ((pos % 16) == 0)
            snprintf(reinterpret_cast<char *>(_buff) + strlen(_buff),
                     sizeof(_buff), "%03x ", pos / 16);
        else if ((pos % 8) == 0)
            snprintf(reinterpret_cast<char *>(_buff) + strlen(_buff),
                     sizeof(_buff), "  ");

        snprintf(reinterpret_cast<char *>(_buff) + strlen(_buff),
                 sizeof(_buff), "%02x ", buf[pos]);
    }

    if ((sz - pos) >= 5)
        snprintf(reinterpret_cast<char *>(_buff) + strlen(_buff),
                 sizeof(_buff), "... %02x %02x %02x %02x %02x",
                 buf[sz - 5], buf[sz - 4], buf[sz - 3], buf[sz - 2], buf[sz - 1]);
    std::cerr << _buff << std::endl;
}

void UpgradeManager::verbose(CMDRequest *req)
{
    char _buff[1024] = {'\0'};

    if (getenv("VERBOSE"))
    {
        std::string prefix(">>> ");
        prefix += req->toString();

        hexdump(prefix, req->rawdata(), req->rawdatalen());
    }
    else
    {
        snprintf(reinterpret_cast<char *>(_buff) + strlen(_buff),
                 sizeof(_buff), ">>> %s(0x%02x) %s",
                 req->toString().c_str(), req->ordinal(),
                 req->argString().c_str());

        std::cerr << _buff << std::endl;
    }
}

void UpgradeManager::verbose(CMDResponse *resp)
{
    char _buff[1024] = {'\0'};

    if (getenv("VERBOSE"))
    {
        std::string prefix("<<< ");
        prefix += resp->toString();

        hexdump(prefix, resp->rawdata(), resp->rawdatalen());
    }
    else
    {
        snprintf(reinterpret_cast<char *>(_buff) + strlen(_buff),
                 sizeof(_buff), "<<< %s(0x%02x)",
                 resp->toString().c_str(), resp->ordinal());

        std::cerr << _buff << std::endl;
    }
}

bool UpgradeManager::talk(CMDRequest *req, CMDResponse *resp, int timeout)
{
    int remainlen = req->expect_len();

    resp->reset();
    verbose(req);
    if (!serial.sendSync(req->rawdata(), req->rawdatalen()))
    {
        std::cerr << "sendSync failed, req=" << req->toString() << std::endl;
        return false;
    }

    do
    {
        if (!serial.recvSync(timeout))
        {
            std::cerr << "recvSync failed, req=" << req->toString() << std::endl;
            return false;
        }
        resp->push_back(serial.data(), serial.datalen());
        remainlen -= static_cast<int>(serial.datalen());
    } while (remainlen > 0);
    verbose(resp);

    return true;
}

int UpgradeManager::connect()
{
    int max_try = 10;
    FDLRequest req;
    FDLResponse resp;

    serial.setBaud(BAUD::BAUD115200);
    do
    {
        usleep(1000);
        req.newCheckBaud(BaudARRSTR[static_cast<int>(BAUD::BAUD115200)]);
        if (!talk(&req, &resp))
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
    if (!talk(&req, &resp) || resp.type() != REPTYPE::BSL_REP_ACK)
        goto _exit;

    return 0;

_exit:
    std::cerr << __func__ << " " << req.toString()
              << " get unexpect response " << resp.toString() << std::endl;
    return -1;
}

int UpgradeManager::transfer(const XMLFileInfo &info, uint32_t maxlen, bool isfdl)
{
    FDLRequest req;
    FDLResponse resp;
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

    if (!talk(&req, &resp) || resp.type() != REPTYPE::BSL_REP_ACK)
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
        if (!talk(&req, &resp) || resp.type() != REPTYPE::BSL_REP_ACK)
            goto _exit;

        filesz -= txlen;
    } while (filesz > 0);
    firmware.close(fin);

    // this operation may take much more time, so set a much longger timeout
    req.newEndData();
    if (!talk(&req, &resp, 30000) || resp.type() != REPTYPE::BSL_REP_ACK)
        goto _exit;

    return 0;

_exit:
    std::cerr << __func__ << " " << req.toString()
              << " get unexpect response " << resp.toString() << std::endl;
    return -1;
}

int UpgradeManager::exec()
{
    FDLRequest req;
    FDLResponse resp;

    req.newExecData();
    if (!talk(&req, &resp) || resp.type() != REPTYPE::BSL_REP_ACK)
    {
        std::cerr << __func__ << " " << req.toString()
                  << " get unexpect response " << resp.toString() << std::endl;
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
    FDLRequest req;

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
    FDLRequest req;
    FDLResponse resp;
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

    req.newStartRead(info.blockid, info.size);
    if (!talk(&req, &resp) || resp.type() != REPTYPE::BSL_REP_ACK)
        goto _exit;

    do
    {
        uint32_t sz = (partitionsz > FRAMESZ_DATA) ? FRAMESZ_DATA : partitionsz;

        req.newReadMidst(sz, totalsz);
        if (!talk(&req, &resp) || resp.type() != REPTYPE::BSL_REP_READ_FLASH)
            goto _exit;

        fout.write(reinterpret_cast<char *>(resp.data()), resp.datalen());
        partitionsz -= sz;
        totalsz += sz;
    } while (partitionsz > 0);

    req.newEndRead();
    if (!talk(&req, &resp) || resp.type() != REPTYPE::BSL_REP_ACK)
        goto _exit;

    return 0;
_exit:
    std::cerr << __func__ << " fail to backup " << info.blockid << std::endl;
    return -1;
}

int UpgradeManager::flash_pdl(const XMLFileInfo &info)
{
    PDLRequest req;
    PDLResponse resp;
    uint32_t filesz = info.realsize;
    std::ifstream fin = firmware.open_pac(info.fileid);

    ON_SCOPE_EXIT
    {
        firmware.close(fin);
    };

    req.newPDLConnect();
    if (!talk(&req, &resp) || resp.type() != PDLREP::PDL_RSP_ACK)
        goto _exit;

    req.newPDLStart(info.base, info.realsize);
    if (!talk(&req, &resp) || resp.type() != PDLREP::PDL_RSP_ACK)
        goto _exit;

    do
    {
        uint32_t sz = (filesz > FRAMESZ_PDL) ? FRAMESZ_PDL : filesz;
        firmware.read(fin, _data, sz);
        req.newPDLMidst(_data, sz);
        filesz -= sz;
    } while (filesz > 0);

    req.newPDLEnd();
    if (!talk(&req, &resp) || resp.type() != PDLREP::PDL_RSP_ACK)
        goto _exit;

    req.newPDLExec();
    if (!talk(&req, &resp) || resp.type() != PDLREP::PDL_RSP_ACK)
        goto _exit;

    return 0;

_exit:
    std::cerr << __func__ << " fail to flash " << info.fileid << std::endl;
    return -1;
}

int UpgradeManager::flash_fdl(const XMLFileInfo &info, bool isbootcode)
{
    int ret;
    FDLRequest req;
    FDLResponse resp;

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
        if (!talk(&req, &resp) || resp.type() != REPTYPE::BSL_REP_ACK)
            goto _exit;
    }
    return 0;

_exit:
    std::cerr << __func__ << " fail to flash " << info.fileid << std::endl;
    return -1;
}

int UpgradeManager::flash_partition(const XMLFileInfo &info)
{
    FDLRequest req;
    FDLResponse resp;
    req.set_crc(CRC_MODLE::CRC_FDL);
    req.set_escape_flag(false, false);

    return transfer(info, FRAMESZ_DATA, false);
}

int UpgradeManager::erase_partition(const XMLFileInfo &info)
{
    FDLRequest req;
    FDLResponse resp;

    req.set_crc(CRC_MODLE::CRC_FDL);
    req.set_escape_flag(false, false);

    req.newErasePartition(info.blockid);
    return (talk(&req, &resp) && resp.type() == REPTYPE::BSL_REP_ACK) ? 0 : -1;
}

int UpgradeManager::upgrade(const std::string &chipset, bool backup)
{
    FDLRequest req;
    FDLResponse resp;
    auto table = firmware.get_partition_vec();
    auto filevec = firmware.get_file_vec();
    if (filevec.empty())
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
        else if (string_case_cmp(iter->fileid, "HOST_FDL"))
        {
            if (flash_pdl(*iter))
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
    if (!table.empty())
    {
        req.newRePartition(table);
        if (!talk(&req, &resp) || resp.type() != REPTYPE::BSL_REP_ACK)
            return -1;
    }

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
    talk(&req, &resp);

    std::cerr << __func__ << " finished" << std::endl;
    return 0;
}
