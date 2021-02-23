/*
 * @Author: sinpo828
 * @Date: 2021-02-10 09:05:20
 * @LastEditors: sinpo828
 * @LastEditTime: 2021-02-23 20:15:46
 * @Description: file content
 */
#include <iostream>
#include <cstring>

#include "pdl.hpp"

static PDLREQ __request_type = PDLREQ::PDL_CMD_CONNECT;

PDLRequest::PDLRequest() : _reallen(0)
{
    _data = new (std::nothrow) uint8_t[PDL_MAX_DATA_LEN];
}

PDLRequest::~PDLRequest()
{
    if (_data)
        delete[] _data;
    _data = nullptr;
}

void PDLRequest::reinit(PDLREQ cmd)
{
    auto hdr = PDL_HEADER(_data);
    auto datahdr = PDL_DATA(_data);

    memset(_data, 0, PDL_MAX_DATA_LEN);
    __request_type = type();

    hdr->ucTag = 0xae;
    hdr->nDataSize = htole32(sizeof(pdl_pkt_data));
    hdr->ucFlowID = htole16(0xff);
    hdr->wReserved = 0;

    datahdr->dwCmdType = htole32(static_cast<uint32_t>(cmd));

    _reallen = sizeof(pdl_pkt_header) + sizeof(pdl_pkt_data);
}

void PDLRequest::push_back(uint8_t *data, uint32_t len)
{
    auto hdr = PDL_HEADER(_data);
    auto tail = PDL_TAIL(_data, _reallen);

    std::copy(data, data + len, reinterpret_cast<uint8_t *>(tail));

    _reallen += len;
    hdr->nDataSize = htole32(_reallen - sizeof(pdl_pkt_header));
}

void PDLRequest::newPDLConnect()
{
    reinit(PDLREQ::PDL_CMD_CONNECT);
}

void PDLRequest::newPDLStart(uint32_t addr, uint32_t size)
{
    auto datahdr = PDL_DATA(_data);
    uint8_t d[] = {'P', 'D', 'L', '1', 0};

    reinit(PDLREQ::PDL_CMD_START_DATA);
    datahdr->dwDataAddr = htole32(addr);
    datahdr->dwDataSize = htole32(size);

    push_back(d, sizeof(d));
}

void PDLRequest::newPDLMidst(uint8_t *data, uint32_t len)
{
    static int index = 0;
    auto datahdr = PDL_DATA(_data);

    reinit(PDLREQ::PDL_CMD_MID_DATA);
    datahdr->dwDataSize = htole32(len);
    datahdr->dwDataSize = htole32(index++);

    push_back(data, len);
}

void PDLRequest::newPDLEnd()
{
    uint8_t end_flag[] = {0x1c, 0x3c, 0x6e, 0x06};

    reinit(PDLREQ::PDL_CMD_END_DATA);
    push_back(end_flag, sizeof(end_flag));
}

void PDLRequest::newPDLExec()
{
    reinit(PDLREQ::PDL_CMD_EXEC_DATA);
}

uint8_t *PDLRequest::rawdata()
{
    return _data;
}

uint32_t PDLRequest::rawdatalen()
{
    return _reallen;
}

PDLREQ PDLRequest::type()
{
    auto datahdr = PDL_DATA(_data);
    return static_cast<PDLREQ>(le32toh(datahdr->dwCmdType));
}

std::string PDLRequest::toString()
{
    auto datahdr = PDL_DATA(_data);
    switch (static_cast<PDLREQ>(le32toh(datahdr->dwCmdType)))
    {
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

std::string PDLRequest::argString()
{
    return "PDL1";
}

int PDLRequest::ordinal()
{
    return static_cast<int>(type());
}

bool PDLRequest::isDuplicate()
{
    return type() == __request_type;
}

uint32_t PDLRequest::expect_len()
{
    return sizeof(pdl_pkt_header) + 4;
}

uint8_t *rawdata();
uint32_t rawdatalen();

/*************************** RESPONSE ***************************/
/*************************** RESPONSE ***************************/
/*************************** RESPONSE ***************************/
PDLResponse::PDLResponse() : _reallen(0)
{
    _data = new (std::nothrow) uint8_t[PDL_MAX_DATA_LEN];
}

PDLResponse ::~PDLResponse()
{
    if (_data)
        delete[] _data;
    _data = nullptr;
}

void PDLResponse::reset()
{
    _reallen = 0;
    memset(_data, 0, PDL_MAX_DATA_LEN);
}

int PDLResponse::ordinal()
{
    return static_cast<int>(type());
}

void PDLResponse::push_back(uint8_t *d, uint32_t len)
{
    std::copy(d, d + len, _data + _reallen);
    _reallen += len;
}

PDLREP PDLResponse::type()
{
    auto datahdr = PDL_DATA(_data);

    return static_cast<PDLREP>(le32toh(datahdr->dwCmdType));
}

uint8_t *PDLResponse::rawdata()
{
    return _data;
}

uint32_t PDLResponse::rawdatalen()
{
    return _reallen;
}

std::string PDLResponse::toString()
{
    auto datahdr = PDL_DATA(_data);
    if (_reallen < 12)
        return "PDL_INCOMPLETE_RESPONSE";

    switch (static_cast<PDLREP>(le32toh(datahdr->dwCmdType)))
    {
    case PDLREP::PDL_RSP_ACK:
        return "";
    case PDLREP::PDL_RSP_INVALID_CMD:
        return "";
    case PDLREP::PDL_RSP_UNKNOWN_CMD:
        return "";
    case PDLREP::PDL_RSP_INVALID_ADDR:
        return "";
    case PDLREP::PDL_RSP_INVALID_BAUDRATE:
        return "";
    case PDLREP::PDL_RSP_INVALD_PARTITION:
        return "";
    case PDLREP::PDL_RSP_SIZE_ERROR:
        return "";
    case PDLREP::PDL_RSP_WAIT_TIMEOUT:
        return "";
    case PDLREP::PDL_RSP_VERIFY_ERROR:
        return "";
    case PDLREP::PDL_RSP_CHECKSUM_ERROR:
        return "";
    case PDLREP::PDL_RSP_OPERATION_FAILED:
        return "";
    case PDLREP::PDL_RSP_DEVICE_ERROR:
        return "";
    case PDLREP::PDL_RSP_NO_MEMORY:
        return "";
    default:
        return "UNKNOW_RESPONSE";
    }
}