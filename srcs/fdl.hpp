/*
 * @Author: sinpo828
 * @Date: 2021-02-04 14:04:11
 * @LastEditors: sinpo828
 * @LastEditTime: 2021-02-24 10:10:11
 * @Description: file content
 */
#ifndef __FDL__
#define __FDL__

#include <iostream>
#include <string>
#include <vector>

#include "serial.hpp"
#include "protocol.hpp"

const static uint8_t MAGIC_7e = 0x7e;
const static uint8_t MAGIC_7d = 0x7d;
const static uint8_t MAGIC_5e = 0x5e;
const static uint8_t MAGIC_5d = 0x5d;
#define FRAMEHDR(p) (reinterpret_cast<cmd_header *>(p))
#define FRAMEDATA(p, n, t) (reinterpret_cast<t *>(p + n))
#define FRAMEDATAHDR(p) (p + sizeof(cmd_header))
#define FRAMETAIL(p, n) (reinterpret_cast<cmd_tail *>(p + n))

enum class REQTYPE
{
    BSL_CMD_CHECK_BAUD = 0x7e,
    BSL_CMD_CONNECT = 0x0,
    BSL_CMD_START_DATA = 0x1,
    BSL_CMD_MIDST_DATA = 0x2,
    BSL_CMD_END_DATA = 0x3,
    BSL_CMD_EXEC_DATA = 0x4,
    BSL_CMD_NORMAL_RESET = 0x5,
    BSL_CMD_READ_FLASH = 0x6,
    BSL_CMD_CHANGE_BAUD = 0x9,
    BSL_CMD_ERASE_FLASH = 0xa,
    BSL_CMD_REPARTITION = 0x0b,
    BSL_CMD_START_READ = 0x10,
    BSL_CMD_READ_MIDST = 0x11,
    BSL_CMD_END_READ = 0x12,
    BSL_CMD_EXEC_NAND_INIT = 0x21,
};

enum class REPTYPE
{
    BSL_REP_ACK = 0x80,
    BSL_REP_VER = 0x81,
    BSL_REP_INVALID_CMD = 0x82,
    BSL_REP_UNKNOW_CMD = 0x83,
    BSL_REP_OPERATION_FAILED = 0x84,
    BSL_REP_NOT_SUPPORT_BAUDRATE = 0x85,
    BSL_REP_DOWN_NOT_START = 0x86,
    BSL_REP_DOWN_MUTI_START = 0x87,
    BSL_REP_DOWN_EARLY_END = 0x88,
    BSL_REP_DOWN_DEST_ERROR = 0x89,
    BSL_REP_DOWN_SIZE_ERROR = 0x8a,
    BSL_REP_VERIFY_ERROR = 0x8b,
    BSL_REP_NOT_VERIFY = 0x8c,
    BSL_REP_READ_FLASH = 0x93,
    BSL_REP_INCOMPATIBLE_PARTITION = 0x96,
};

enum class CRC_MODLE
{
    CRC_BOOTCODE, // crc used when talk with bootcode
    CRC_FDL,      // crc used when talk with fdl
};

#pragma pack(1)
struct cmd_header
{
    uint8_t magic;
    uint16_t cmd_type;
    uint16_t data_length;
};

struct cmd_tail
{
    uint16_t crc16;
    uint8_t magic;
};
#pragma pack()

struct partition_info
{
    std::string partition;
    uint32_t size;
    partition_info() : partition(""), size(0) {}
};

// NOTICE: make sure it not less than data len in Midst message
#define MAX_DATA_LEN 0x4000

class FDLRequest final : public CMDRequest
{
private:
    uint8_t *_data;
    uint32_t _reallen;
    CRC_MODLE crc_modle;
    bool crc_escape_flag;
    bool data_escape_flag;
    std::string _argstr;

private:
    void reinit(REQTYPE);
    void finishup();

    template <typename T>
    void push_back(T);

public:
    FDLRequest();
    virtual ~FDLRequest();

    REQTYPE type();
    int value();
    std::string toString();
    std::string argString();
    void setArgString(const std::string &);

    uint8_t *data();
    uint32_t dataLen();
    uint8_t *rawData();
    uint32_t rawDataLen();

    bool isDuplicate();
    bool onWrite();
    bool onRead();

    void setEscapeFlag(bool data_es_flag, bool crc_es_flag);
    void setCrcModle(CRC_MODLE);
    uint16_t crc16BootCode(const char *src, uint32_t len);
    uint16_t crc16FDL(const uint16_t *src, uint32_t len);
    uint16_t crc16NV(uint16_t crc, const uint8_t *src, uint32_t len);

    void newCheckBaud();
    void newConnect();
    void newStartData(uint32_t addr, uint32_t len, uint32_t cs = 0);
    void newStartData(const std::string &idstr, uint32_t len, uint32_t cs = 0);
    void newMidstData(uint8_t *buf, uint32_t len);
    void newEndData();
    void newExecData();
    void newNormalReset();
    void newReadFlash(uint32_t addr, uint32_t size, uint32_t offset = 0);

    /**
     * old implements
     * set size = 0, let FDL1/FDL2 desides the partition size. usually erase a partition
     * in old implements, addr=0 && size=0xffffffff means erase all
     */
    void newEraseFlash(uint32_t addr, uint32_t size);
    void newErasePartition(uint32_t addr);
    void newEraseALL();
    void newErasePartition(const std::string &partition);

    void newRePartition(const std::vector<partition_info> &table);

    void newChangeBaud(BAUD);
    void newStartRead(const std::string &partition, uint32_t len);
    void newReadMidst(uint32_t rxsz, uint32_t offset);
    void newEndRead();
    void newExecNandInit();
};

/*************************** RESPONSE ***************************/
/*************************** RESPONSE ***************************/
/*************************** RESPONSE ***************************/
/*************************** RESPONSE ***************************/
/*************************** RESPONSE ***************************/

enum class RESP_STATE
{
    RESP_STATE_OK,
    RESP_STATE_INCOMPLETE,
    RESP_STATE_VARIFY_FAIL,
    RESP_STATE_MALFORMED,
};

class FDLResponse final : public CMDResponse
{
private:
    uint8_t *_data;
    uint32_t _reallen;

public:
    FDLResponse();
    virtual ~FDLResponse();

    REPTYPE type();
    int value();
    std::string toString();

    void reset();
    void push_back(uint8_t *d, uint32_t len);

    uint8_t *data();
    uint32_t dataLen();
    uint8_t *rawData();
    uint32_t rawDataLen();
    uint32_t expectLength();
    uint32_t minLength();
};

#endif //__FDL__
