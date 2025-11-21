/*
 * @Author: sinpo828
 * @Date: 2021-02-08 11:36:51
 * @LastEditors: sinpo828
 * @LastEditTime: 2021-02-26 16:15:17
 * @Description: file content
 */

#include <string>
#include <algorithm>

extern "C" {
#include <unistd.h>
}

#include "pdl.hpp"
#include "fdl.hpp"
#include "serial.hpp"
#include "usbfs.hpp"
#include "upgrade_manager.hpp"
#include "scopeguard.hpp"

bool string_case_cmp(const std::string& s1, const std::string& s2) {
    std::string s1_upper(s1);
    std::string s2_upper(s2);
    std::transform(s1.begin(), s1.end(), s1_upper.begin(), toupper);
    std::transform(s2.begin(), s2.end(), s2_upper.begin(), toupper);

    return s1_upper == s2_upper;
}

UpgradeManager::UpgradeManager(const std::string& tty, const std::string& pac, std::shared_ptr<USBStream>& us)
    : usbstream(us), firmware(pac), pac(pac) {
    int maxlen = FRAMESZ_DATA > FRAMESZ_FDL ? FRAMESZ_DATA : FRAMESZ_FDL;
    _data = new (std::nothrow) uint8_t[maxlen];
}

UpgradeManager::~UpgradeManager() {
    if (_data) delete[] _data;
    _data = nullptr;
}

bool UpgradeManager::prepare() {
    if (firmware.pacparser()) return false;

    if (firmware.xmlparser()) return false;

    return true;
}

void UpgradeManager::hexdump(const std::string& prefix, uint8_t* buf, uint32_t len, uint32_t dumplen) {
    char _buff[4096] = {'\0'};
    uint32_t pos = 0;

    for (pos = 0; pos < len && pos < 1024 && pos < dumplen; pos++) {
        if (pos == 0)
            snprintf(reinterpret_cast<char*>(_buff) + strlen(_buff), sizeof(_buff), "%s %03x  ", prefix.c_str(),
                     pos / 16);
        else if ((pos % 16) == 0)
            snprintf(reinterpret_cast<char*>(_buff) + strlen(_buff), sizeof(_buff), "\n%s %03x  ", prefix.c_str(),
                     pos / 16);
        else if ((pos % 8) == 0)
            snprintf(reinterpret_cast<char*>(_buff) + strlen(_buff), sizeof(_buff), " ");

        snprintf(reinterpret_cast<char*>(_buff) + strlen(_buff), sizeof(_buff), "%02x ", buf[pos]);
    }

    if ((len - pos) >= 5)
        snprintf(reinterpret_cast<char*>(_buff) + strlen(_buff), sizeof(_buff), "... %02x %02x %02x %02x %02x",
                 buf[len - 5], buf[len - 4], buf[len - 3], buf[len - 2], buf[len - 1]);
    std::cerr << _buff << std::endl;
}

void UpgradeManager::verbose(CMDRequest* req) {
    static bool _is_duplicate = false;
    bool verbose_log = !!getenv("VERBOSE");

    if (_is_duplicate && !req->isDuplicate()) std::cerr << std::endl;

    if (req->isDuplicate() && req->onWrite() && !verbose_log)
        std::cerr << ">";
    else if (req->isDuplicate() && req->onRead() && !verbose_log)
        std::cerr << "<";
    else
        std::cerr << ">>> " << req->toString() << " " << req->argString() << " (" << req->rawDataLen() << ") "
                  << std::endl;

    _is_duplicate = req->isDuplicate();

    if (verbose_log) hexdump(">>>", req->rawData(), req->rawDataLen());
}

void UpgradeManager::verbose(CMDResponse* resp, bool ondata) {
    bool verbose_log = !!getenv("VERBOSE");

    if (!ondata) std::cerr << "<<< " << resp->toString() << " (" << resp->rawDataLen() << ")" << std::endl;
    if (verbose_log) hexdump("<<<", resp->rawData(), resp->rawDataLen());
}

bool UpgradeManager::talk(CMDRequest* req, CMDResponse* resp, int rx_timeout, int tx_timeout) {
    if (resp) resp->reset();

    verbose(req);
    if (req->protocol() == PROTOCOL::PROTO_FDL) {
        if (!usbstream->sendSync(req->rawData(), req->rawDataLen(), tx_timeout)) {
            std::cerr << "sendSync failed, req=" << req->toString() << std::endl;
            return false;
        }
    } else {
        uint32_t txlen = 0;

        if (!usbstream->sendSync(req->rawData() + txlen, sizeof(pdl_pkt_header), tx_timeout)) {
            std::cerr << "sendSync failed, req=" << req->toString() << std::endl;
            return false;
        }

        txlen += sizeof(pdl_pkt_header);
        if (!usbstream->sendSync(req->rawData() + txlen, sizeof(pdl_pkt_tag), tx_timeout)) {
            std::cerr << "sendSync failed, req=" << req->toString() << std::endl;
            return false;
        }

        txlen += sizeof(pdl_pkt_tag);
        if (txlen < req->rawDataLen() &&
            !usbstream->sendSync(req->rawData() + txlen, req->rawDataLen() - txlen, tx_timeout)) {
            std::cerr << "sendSync failed, req=" << req->toString() << std::endl;
            return false;
        }
    }

    if (!resp) return true;

    do {
        if (!usbstream->recvSync(rx_timeout)) {
            std::cerr << "recvSync failed, req=" << req->toString() << std::endl;
            return false;
        }
        resp->push_back(usbstream->data(), usbstream->datalen());

        if (resp->rawDataLen() < resp->minLength()) continue;

        // real length may much longgger if crc is escaped
        if (resp->expectLength() <= resp->rawDataLen()) break;
    } while (1);

    verbose(resp, req->onWrite() || req->onRead());

    return true;
}

int UpgradeManager::connect() {
    int max_try = 5;

    do {
        usleep(1000);
        request.newCheckBaud();
        request.setArgString(SerialPort::BaudARRSTR[static_cast<int>(BAUD::BAUD115200)]);
        if (!talk(&request, &response, 3000)) {
            if (--max_try <= 0) return -1;
        } else {
            break;
        }
    } while (1);

    if (response.type() != REPTYPE::BSL_REP_VER) goto _exit;

    request.newConnect();
    if (!talk(&request, &response) || response.type() != REPTYPE::BSL_REP_ACK) goto _exit;

    return 0;

_exit:
    std::cerr << __func__ << " " << request.toString() << " get unexpect response " << response.toString() << std::endl;
    return -1;
}

int UpgradeManager::transfer(const XMLFileInfo& info, uint32_t maxlen) {
    uint32_t filesz = info.use_pac_file ? firmware.member_file_size(info.fileid) : firmware.local_file_size(info.fpath);
    std::ifstream fin = info.use_pac_file ? firmware.open_pac(info.fileid) : firmware.open_file(info.fpath);

    bool nv_replace_byte = false;

    ON_SCOPE_EXIT {
        if (fin.is_open()) fin.close();
    };

    if (info.use_old_proto)
        request.newStartData(info.base, info.realsize, info.checksum);
    else
        request.newStartData(info.blockid, info.realsize, info.checksum);

    if (!talk(&request, &response) || response.type() != REPTYPE::BSL_REP_ACK) goto _exit;

    do {
        uint32_t txlen = (filesz > maxlen) ? maxlen : filesz;
        firmware.read(fin, _data, txlen);
        if (!nv_replace_byte && info.crc16) {
            *reinterpret_cast<uint16_t*>(_data) = htobe16(info.crc16);
            nv_replace_byte = true;
        }

        request.newMidstData(_data, txlen);
        request.setArgString(info.fileid);
        if (!talk(&request, &response) || response.type() != REPTYPE::BSL_REP_ACK) goto _exit;

        filesz -= txlen;
    } while (filesz > 0);
    firmware.close(fin);

    // this operation may take much more time, so set a much longger timeout
    request.newEndData();
    if (!talk(&request, &response, 30000) || response.type() != REPTYPE::BSL_REP_ACK) goto _exit;

    return 0;

_exit:
    std::cerr << __func__ << " " << request.toString() << " get unexpect response " << response.toString() << std::endl;
    return -1;
}

int UpgradeManager::exec() {
    request.newExecData();
    if (!talk(&request, &response) || response.type() != REPTYPE::BSL_REP_ACK) {
        std::cerr << __func__ << " " << request.toString() << " get unexpect response " << response.toString()
                  << std::endl;
        return -1;
    }

    return 0;
}

// NV should be processed specially
void UpgradeManager::checksum(XMLFileInfo& info) {
    auto fin = firmware.open_pac(info.fileid);
    uint32_t fsz = firmware.member_file_size(info.fileid);
    uint16_t crc = 0;
    uint32_t cs = 0;
    uint32_t skip = 2;

    firmware.read(fin, _data, skip);
    fsz -= skip;

    do {
        uint32_t sz = (fsz > 4096) ? 4096 : fsz;
        firmware.read(fin, _data, sz);
        crc = request.crc16NV(crc, _data, sz);

        for (uint32_t i = 0; i < sz; i++) cs += _data[i];

        fsz -= sz;
    } while (fsz > 0);
    firmware.close(fin);

    cs += (crc & 0xff);
    cs += (crc & 0xff00) >> 8;
    info.checksum = cs;
    info.crc16 = crc;
}

std::string get_real_path(const std::string& pac) {
    std::string fpath = ".";

    if (pac.empty() || pac.find_last_of('/') == std::string::npos) return fpath;
    return pac.substr(0, pac.find_last_of('/'));
}

int UpgradeManager::backup_partition(XMLFileInfo& info) {
    uint32_t totalsz = 0;
    uint32_t partitionsz = info.size;
    std::string name = get_real_path(pac) + "/" + info.fileid + ".bak";
    std::ofstream fout(name, std::ios::trunc);

    request.setCrcModle(CRC_MODLE::CRC_FDL);
    request.setEscapeFlag(false, false);
    if (!fout.is_open()) {
        std::cerr << __func__ << " cannot backup partition " << info.fileid << "(" << info.blockid
                  << ") for fail to open(write) " << name << std::endl;
        return -1;
    }
    info.fpath = name;

    ON_SCOPE_EXIT { fout.close(); };

    request.setArgString(info.fileid);
    if (!info.use_old_proto) {
        request.newStartRead(info.blockid, info.size);
        request.setArgString(info.fileid);
        if (!talk(&request, &response) || response.type() != REPTYPE::BSL_REP_ACK) goto _exit;
    }

    do {
        uint32_t sz = (partitionsz > FRAMESZ_DATA) ? FRAMESZ_DATA : partitionsz;

        if (info.use_old_proto)
            request.newReadFlash(info.base, sz, totalsz);
        else
            request.newReadMidst(sz, totalsz);
        request.setArgString(info.fileid);
        if (!talk(&request, &response) || response.type() != REPTYPE::BSL_REP_READ_FLASH) goto _exit;

        fout.write(reinterpret_cast<char*>(response.data()), response.dataLen());
        partitionsz -= sz;
        totalsz += sz;
    } while (partitionsz > 0);

    if (!info.use_old_proto) {
        request.newEndRead();
        if (!talk(&request, &response) || response.type() != REPTYPE::BSL_REP_ACK) goto _exit;
    }

    return 0;
_exit:
    std::cerr << __func__ << " fail to backup " << info.blockid << std::endl;
    return -1;
}

int UpgradeManager::flash_pdl(const XMLFileInfo& info) {
    PDLRequest req;
    PDLResponse resp;
    uint32_t filesz = info.realsize;
    std::ifstream fin = firmware.open_pac(info.fileid);

    ON_SCOPE_EXIT { firmware.close(fin); };

    req.newPDLConnect();
    if (!talk(&req, &resp, 15000, 15000) || resp.type() != PDLREP::PDL_RSP_ACK) goto _exit;

    req.newPDLStart(info.base, info.realsize);
    if (!talk(&req, &resp) || resp.type() != PDLREP::PDL_RSP_ACK) goto _exit;

    do {
        uint32_t txlen = (filesz > FRAMESZ_PDL) ? FRAMESZ_PDL : filesz;
        firmware.read(fin, _data, txlen);
        req.newPDLMidst(_data, txlen);
        if (!talk(&req, &resp) || resp.type() != PDLREP::PDL_RSP_ACK) goto _exit;

        filesz -= txlen;
    } while (filesz > 0);

    req.newPDLEnd();
    if (!talk(&req, &resp) || resp.type() != PDLREP::PDL_RSP_ACK) goto _exit;

    req.newPDLExec();
    this->talk(&req, nullptr);

    return 0;

_exit:
    std::cerr << __func__ << " fail to flash " << info.fileid << std::endl;
    return -1;
}

int UpgradeManager::flash_fdl(const XMLFileInfo& info) {
    request.setCrcModle(CRC_MODLE::CRC_BOOTCODE);
    request.setEscapeFlag(true, true);

    if (connect()) goto _exit;

    request.setArgString(info.fileid);
    if (transfer(info, FRAMESZ_BOOTCODE)) goto _exit;

    return exec();
_exit:
    std::cerr << __func__ << " fail to flash " << info.fileid << std::endl;
    return -1;
}

int UpgradeManager::flash_nand_fdl(const XMLFileInfo& info) {
    int ret;

    request.setCrcModle(CRC_MODLE::CRC_FDL);
    request.setEscapeFlag(true, true);

    if (connect()) goto _exit;

    request.setArgString(info.fileid);
    if (transfer(info, FRAMESZ_FDL)) goto _exit;

    ret = exec();
    if (!ret) return 0;

    request.setArgString("");
    if (response.type() == REPTYPE::BSL_REP_INCOMPATIBLE_PARTITION) {
        request.newExecNandInit();
        if (!talk(&request, &response) || response.type() != REPTYPE::BSL_REP_ACK) goto _exit;
    }

    return 0;

_exit:
    std::cerr << __func__ << " fail to flash " << info.fileid << std::endl;
    return -1;
}

int UpgradeManager::flash_partition(const XMLFileInfo& info) {
    request.setCrcModle(CRC_MODLE::CRC_FDL);
    request.setEscapeFlag(info.use_old_proto, info.use_old_proto);
    request.setArgString(info.fileid);

    return transfer(info, info.use_old_proto ? FRAMESZ_PDL : FRAMESZ_DATA);
}

int UpgradeManager::erase_partition(const XMLFileInfo& info) {
    request.setCrcModle(CRC_MODLE::CRC_FDL);
    request.setEscapeFlag(info.use_old_proto, info.use_old_proto);

    if (info.use_old_proto)
        request.newErasePartition(info.base);
    else
        request.newErasePartition(info.blockid);
    request.setArgString(info.fileid);

    return (talk(&request, &response) && response.type() == REPTYPE::BSL_REP_ACK) ? 0 : -1;
}

int UpgradeManager::upgrade(bool backup) {
    auto table = firmware.get_partition_vec();
    auto filevec = firmware.get_file_vec();
    if (filevec.empty()) {
        std::cerr << __func__ << " failed for empty vector" << std::endl;
        goto _exit;
    }

    if (usbstream->physicalLink() == USBLINK::USBLINK_TTY) {
        auto p = reinterpret_cast<SerialPort*>(usbstream.get());
        p->setBaud(BAUD::BAUD115200);
    } else {
        auto p = reinterpret_cast<USBFS*>(usbstream.get());
        if (firmware.productName() == "UIX8910_MODEM") p->setInterface();

        if (firmware.productName() == "UDX710_MODEM") p->sciu2sMessage();
    }

    // FDL
    for (auto iter = filevec.begin(); iter != filevec.end();) {
        if (string_case_cmp(iter->fileid, "FDL")) {
            iter->use_old_proto = true;
            if (flash_fdl(*iter)) goto _exit;
            filevec.erase(iter);
        } else if (string_case_cmp(iter->fileid, "FDL2")) {
            iter->use_old_proto = true;
            if (flash_nand_fdl(*iter)) goto _exit;
            filevec.erase(iter);
        } else if (string_case_cmp(iter->fileid, "HOST_FDL")) {
            iter->use_old_proto = true;
            if (flash_pdl(*iter)) goto _exit;
            filevec.erase(iter);
        } else {
            iter++;
        }
    }

    // Bakeup
    for (auto iter = filevec.begin(); iter != filevec.end(); iter++) {
        if (string_case_cmp(iter->fileid, "NV")) {
            iter->use_old_proto = true;
            if (backup_partition(*iter)) goto _exit;
        }

        else if (string_case_cmp(iter->fileid, "NV_COMM")) {
            iter->use_old_proto = false;
            if (backup_partition(*iter)) goto _exit;
        }
    }

    // update partition table
    if (!table.empty()) {
        request.newRePartition(table);
        if (!talk(&request, &response) || response.type() != REPTYPE::BSL_REP_ACK) goto _exit;
    }

    // do update
    for (auto iter = filevec.begin(); iter != filevec.end(); iter++) {
        if (string_case_cmp(iter->type, "EraseFlash2")) {
            iter->use_old_proto = false;
            if (erase_partition(*iter)) goto _exit;
        } else if (string_case_cmp(iter->type, "EraseFlash")) {
            if (iter->fileid != "FLASH") continue;

            iter->use_old_proto = true;
            if (erase_partition(*iter)) goto _exit;
        } else if (string_case_cmp(iter->type, "NV_COMM")) {
            iter->use_old_proto = false;
            checksum(*iter);
            if (flash_partition(*iter)) goto _exit;
        } else if (string_case_cmp(iter->type, "NV")) {
            iter->use_old_proto = true;
            checksum(*iter);
            if (flash_partition(*iter)) goto _exit;
        } else if (string_case_cmp(iter->type, "CODE")) {
            if (string_case_cmp(iter->fileid, "PhaseCheck")) continue;

            iter->use_old_proto = true;
            if (flash_partition(*iter)) goto _exit;
        } else if (string_case_cmp(iter->type, "YAFFS_IMG2") || string_case_cmp(iter->type, "UBOOT_LOADER2") ||
                   string_case_cmp(iter->type, "CODE2")) {
            if (string_case_cmp(iter->fileid, "PhaseCheck") || string_case_cmp(iter->fileid, "ProdNV")) continue;

            iter->use_old_proto = false;
            if (flash_partition(*iter)) goto _exit;
        } else {
            std::cerr << "skip unknow type: " << iter->type << std::endl;
        }
    }

    request.newNormalReset();
    talk(&request, &response);

    std::cerr << __func__ << " success" << std::endl;
    return 0;

_exit:
    std::cerr << __func__ << " fail" << std::endl;
    return -1;
}
