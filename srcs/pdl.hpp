/*
 * @Author: sinpo828
 * @Date: 2021-02-10 08:46:33
 * @LastEditors: sinpo828
 * @LastEditTime: 2021-02-23 20:14:25
 * @Description: file content
 */
#ifndef __PDL__
#define __PDL__

#include <iostream>
#include <string>

#include "protocol.hpp"

enum class PDLREQ
{
    PDL_CMD_CONNECT,
    PDL_CMD_ERASE_FLASH,
    PDL_CMD_ERASE_PARTITION,
    PDL_CMD_ERASE_ALL,
    PDL_CMD_START_DATA,
    PDL_CMD_MID_DATA,
    PDL_CMD_END_DATA,
    PDL_CMD_EXEC_DATA,
    PDL_CMD_READ_FLASH,
    PDL_CMD_READ_PARTITIONS,
    PDL_CMD_NORMAL_RESET,
    PDL_CMD_READ_CHIPID,
    PDL_CMD_SET_BAUDRATE,
};

enum class PDLREP
{
    PDL_RSP_ACK,

    /// from PC command
    PDL_RSP_INVALID_CMD,
    PDL_RSP_UNKNOWN_CMD,
    PDL_RSP_INVALID_ADDR,
    PDL_RSP_INVALID_BAUDRATE,
    PDL_RSP_INVALD_PARTITION,
    PDL_RSP_SIZE_ERROR,
    PDL_RSP_WAIT_TIMEOUT,

    /// from phone
    PDL_RSP_VERIFY_ERROR,
    PDL_RSP_CHECKSUM_ERROR,
    PDL_RSP_OPERATION_FAILED,

    /// phone internal
    PDL_RSP_DEVICE_ERROR, //DDR,NAND init errors
    PDL_RSP_NO_MEMORY
};

#define PDL_MAX_DATA_LEN 4096

#define PDL_HEADER(p) reinterpret_cast<pdl_pkt_header *>(p)
#define PDL_DATA(p) reinterpret_cast<pdl_pkt_data *>(p + sizeof(pdl_pkt_header))
#define PDL_TAIL(p, l) reinterpret_cast<pdl_pkt_header *>(p + l)
#pragma pack(push, 1)

struct pdl_pkt_header
{
    uint8_t ucTag;      //< 0xAE
    uint32_t nDataSize; //< data size
    uint8_t ucFlowID;   //< 0xFF
    uint16_t wReserved; //< reserved
};

// response only has field 'dwCmdType'
struct pdl_pkt_data
{
    uint32_t dwCmdType;
    uint32_t dwDataAddr;
    uint32_t dwDataSize;
};
#pragma pack(pop)

class PDLRequest final : public CMDRequest
{
private:
    uint8_t *_data;
    uint32_t _reallen;

private:
    void reinit(PDLREQ cmd, uint32_t addr, uint32_t size);
    void reinit(PDLREQ cmd);
    void push_back(uint8_t *data, uint32_t len);

public:
    PDLRequest();
    virtual ~PDLRequest();

    void newPDLConnect();
    void newPDLStart(uint32_t addr, uint32_t size);
    void newPDLMidst(uint8_t *data, uint32_t len);
    void newPDLEnd();
    void newPDLExec();

    uint8_t *rawdata();
    uint32_t rawdatalen();

    PDLREQ type();
    std::string toString();

    std::string argString();
    int ordinal();
    bool isDuplicate();
    uint32_t expect_len();
};

class PDLResponse final : public CMDResponse
{
private:
    uint8_t *_data;
    uint32_t _reallen;

public:
    PDLResponse();
    virtual ~PDLResponse();

    PDLREP type();
    uint8_t *rawdata();
    uint32_t rawdatalen();

    void reset();
    std::string toString();
    int ordinal();
    void push_back(uint8_t *d, uint32_t len);
};

#endif //__PDL__