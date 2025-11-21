/*
 * @Author: sinpo828
 * @Date: 2021-02-10 09:05:20
 * @LastEditors: sinpo828
 * @LastEditTime: 2021-02-26 15:39:24
 * @Description: file content
 */
#include <iostream>
#include <cstring>

#include "pdl.hpp"

static PDLREQ __request_type = PDLREQ::PDL_CMD_CONNECT;

PDLRequest::PDLRequest() : CMDRequest(PROTOCOL::PROTO_PDL), _reallen(0) {
    _data = new (std::nothrow) uint8_t[PDL_MAX_DATA_LEN];
}

PDLRequest::~PDLRequest() {
    if (_data) delete[] _data;
    _data = nullptr;
}

void PDLRequest::reinit(PDLREQ cmd) {
    auto hdr = PDLHEADER(_data);
    auto tag = PDLTAG(_data);

    __request_type = type();
    memset(_data, 0, PDL_MAX_DATA_LEN);

    hdr->ucTag = 0xae;
    hdr->nDataSize = htole32(sizeof(pdl_pkt_tag));
    hdr->ucFlowID = 0xff;
    hdr->wReserved = 0;

    tag->dwCmdType = htole32(static_cast<uint32_t>(cmd));

    _reallen = sizeof(pdl_pkt_header) + sizeof(pdl_pkt_tag);
}

void PDLRequest::push_back(uint8_t* data, uint32_t len) {
    auto hdr = PDLHEADER(_data);

    std::copy(data, data + len, _data + _reallen);

    _reallen += len;
    hdr->nDataSize = htole32(_reallen - sizeof(pdl_pkt_header));
}

void PDLRequest::newPDLConnect() { reinit(PDLREQ::PDL_CMD_CONNECT); }

void PDLRequest::newPDLStart(uint32_t addr, uint32_t size) {
    auto tag = PDLTAG(_data);
    uint8_t d[] = {'P', 'D', 'L', '1', 0};

    reinit(PDLREQ::PDL_CMD_START_DATA);
    tag->dwDataAddr = htole32(addr);
    tag->dwDataSize = htole32(size);

    push_back(d, sizeof(d));
}

void PDLRequest::newPDLMidst(uint8_t* data, uint32_t len) {
    static int index = 0;
    auto tag = PDLTAG(_data);

    reinit(PDLREQ::PDL_CMD_MID_DATA);
    tag->dwDataAddr = htole32(index++);
    tag->dwDataSize = htole32(len);

    push_back(data, len);
}

void PDLRequest::newPDLEnd() {
    uint8_t end_flag[] = {0x1c, 0x3c, 0x6e, 0x06};

    reinit(PDLREQ::PDL_CMD_END_DATA);
    push_back(end_flag, sizeof(end_flag));
}

void PDLRequest::newPDLExec() { reinit(PDLREQ::PDL_CMD_EXEC_DATA); }

uint8_t* PDLRequest::rawData() { return _data; }

uint32_t PDLRequest::rawDataLen() { return _reallen; }

PDLREQ PDLRequest::type() {
    auto tag = PDLTAG(_data);
    return static_cast<PDLREQ>(le32toh(tag->dwCmdType));
}

int PDLRequest::value() { return static_cast<int>(type()); }

std::string PDLRequest::toString() {
    auto tag = PDLTAG(_data);

    if (_reallen == 0)
        return "PDL_EMPTY_RESPONSE";

    else if (_reallen < sizeof(pdl_pkt_header))
        return "PDL_INCOMPLETE_RESPONSE";

    switch (static_cast<PDLREQ>(le32toh(tag->dwCmdType))) {
        case PDLREQ::PDL_CMD_CONNECT:
            return "PDL_CMD_CONNECT";
        case PDLREQ::PDL_CMD_ERASE_FLASH:
            return "PDL_CMD_ERASE_FLASH";
        case PDLREQ::PDL_CMD_ERASE_PARTITION:
            return "PDL_CMD_ERASE_PARTITION";
        case PDLREQ::PDL_CMD_ERASE_ALL:
            return "PDL_CMD_ERASE_ALL";
        case PDLREQ::PDL_CMD_START_DATA:
            return "PDL_CMD_START_DATA";
        case PDLREQ::PDL_CMD_MID_DATA:
            return "PDL_CMD_MID_DATA";
        case PDLREQ::PDL_CMD_END_DATA:
            return "PDL_CMD_END_DATA";
        case PDLREQ::PDL_CMD_EXEC_DATA:
            return "PDL_CMD_EXEC_DATA";
        case PDLREQ::PDL_CMD_READ_FLASH:
            return "PDL_CMD_READ_FLASH";
        case PDLREQ::PDL_CMD_READ_PARTITIONS:
            return "PDL_CMD_READ_PARTITIONS";
        case PDLREQ::PDL_CMD_NORMAL_RESET:
            return "PDL_CMD_NORMAL_RESET";
        case PDLREQ::PDL_CMD_READ_CHIPID:
            return "PDL_CMD_READ_CHIPID";
        case PDLREQ::PDL_CMD_SET_BAUDRATE:
            return "PDL_CMD_SET_BAUDRATE";
        default:
            return "UNKNOW_REQUEST";
    }
}

std::string PDLRequest::argString() { return "PDL1"; }

bool PDLRequest::isDuplicate() { return type() == __request_type; }

bool PDLRequest::onWrite() { return type() == PDLREQ::PDL_CMD_MID_DATA; }

bool PDLRequest::onRead() { return type() == PDLREQ::PDL_CMD_READ_FLASH; }

/*************************** RESPONSE ***************************/
/*************************** RESPONSE ***************************/
/*************************** RESPONSE ***************************/
PDLResponse::PDLResponse() : CMDResponse(PROTOCOL::PROTO_PDL), _reallen(0) {
    _data = new (std::nothrow) uint8_t[PDL_MAX_DATA_LEN];
}

PDLResponse ::~PDLResponse() {
    if (_data) delete[] _data;
    _data = nullptr;
}

PDLREP PDLResponse::type() {
    auto tag = PDLTAG(_data);

    return static_cast<PDLREP>(le32toh(tag->dwCmdType));
}

int PDLResponse::value() { return static_cast<int>(type()); }

std::string PDLResponse::toString() {
    auto tag = PDLTAG(_data);
    if (_reallen < 12) return "PDL_INCOMPLETE_RESPONSE";

    switch (static_cast<PDLREP>(le32toh(tag->dwCmdType))) {
        case PDLREP::PDL_RSP_ACK:
            return "PDL_RSP_ACK";
        case PDLREP::PDL_RSP_INVALID_CMD:
            return "PDL_RSP_INVALID_CMD";
        case PDLREP::PDL_RSP_UNKNOWN_CMD:
            return "PDL_RSP_UNKNOWN_CMD";
        case PDLREP::PDL_RSP_INVALID_ADDR:
            return "PDL_RSP_INVALID_ADDR";
        case PDLREP::PDL_RSP_INVALID_BAUDRATE:
            return "PDL_RSP_INVALID_BAUDRATE";
        case PDLREP::PDL_RSP_INVALD_PARTITION:
            return "PDL_RSP_INVALD_PARTITION";
        case PDLREP::PDL_RSP_SIZE_ERROR:
            return "PDL_RSP_SIZE_ERROR";
        case PDLREP::PDL_RSP_WAIT_TIMEOUT:
            return "PDL_RSP_WAIT_TIMEOUT";
        case PDLREP::PDL_RSP_VERIFY_ERROR:
            return "PDL_RSP_VERIFY_ERROR";
        case PDLREP::PDL_RSP_CHECKSUM_ERROR:
            return "PDL_RSP_CHECKSUM_ERROR";
        case PDLREP::PDL_RSP_OPERATION_FAILED:
            return "PDL_RSP_OPERATION_FAILED";
        case PDLREP::PDL_RSP_DEVICE_ERROR:
            return "PDL_RSP_DEVICE_ERROR";
        case PDLREP::PDL_RSP_NO_MEMORY:
            return "PDL_RSP_NO_MEMORY";
        default:
            return "UNKNOW_RESPONSE";
    }
}

uint8_t* PDLResponse::rawData() { return _data; }

uint32_t PDLResponse::rawDataLen() { return _reallen; }

uint32_t PDLResponse::expectLength() {
    auto hdr = PDLHEADER(_data);

    if (_reallen >= sizeof(pdl_pkt_header))
        return le32toh(hdr->nDataSize) + sizeof(pdl_pkt_header);
    else
        return 0;
}

uint32_t PDLResponse::minLength() { return sizeof(pdl_pkt_header); }

void PDLResponse::reset() {
    _reallen = 0;
    memset(_data, 0, PDL_MAX_DATA_LEN);
}

void PDLResponse::push_back(uint8_t* d, uint32_t len) {
    std::copy(d, d + len, _data + _reallen);
    _reallen += len;
}
