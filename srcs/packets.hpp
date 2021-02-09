/*
 * @Author: sinpo828
 * @Date: 2021-02-04 14:04:11
 * @LastEditors: sinpo828
 * @LastEditTime: 2021-02-09 17:12:15
 * @Description: file content
 */
#ifndef __PACKETS__
#define __PACKETS__

#include <iostream>
#include <string>

#include "serial.hpp"

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

class Command final
{
private:
    const int max_data_len = 2 * 1024;
    uint8_t *_data;
    uint16_t _reallen;
    std::string modem_name;

private:
    void reinit(REQTYPE);
    void finishup();

    template <typename T>
    void push_back(T);

public:
    Command(const std::string &v);

    ~Command();

    std::string cmdstr();

    uint8_t *data();
    uint32_t datalen();
    uint8_t *rawdata();
    uint32_t rawdatalen();

    /**
     * CRC Algorithm
     * used when talk to boot-code
     */
    uint16_t crc16(char *src, uint32_t len);

    /* CHECK-SUM */
    uint16_t frm_chk(uint16_t *src, uint32_t len);

    /** CRC table for the CRC-16. The poly is 0x8005 (x^16 + x^15 + x^2 + 1) */
    uint16_t crc16(uint16_t crc, uint8_t *src, uint32_t len);

    void newCheckBaud();
    void newConnect();
    void newStartData(uint32_t addr, uint32_t len);
    void newMidstData(uint8_t *buf, uint32_t len);
    void newEndData();
    void newChangeBaud(BAUD);
};

class Response
{
private:
    const int max_data_len = 2 * 1024;
    uint8_t *_data;
    uint16_t _reallen;

public:
    Response();
    ~Response();

    std::string respstr();
    
    void parser(uint8_t *d, uint32_t len);
    void reset();

    REPTYPE cmdtype();

    uint8_t *data();
    uint32_t datalen();
    uint8_t *rawdata();
    uint32_t rawdatalen();
};

/**
 * -> ae 0c 00 00 00   ff 00 00
 * -> 00 00 00 00 00   00 00 00 00 00 00 00
 * <- ae 04 00 00 00   ff 00 00 00 00 00 00
 * 
 * -> ae 11 00 00 00   ff 00 00
 * -> 04 00 00 00 00   80 83 00 40 33 00 00
 * -> 50 44 4c 31 00  // PDL1
 * <- ae 04 00 00 00   ff 00 00 00 00 00 00
 * 
 * 
 * -> ae 0c 08 00 00   ff 00 00
 * -> 05 00 00 00 00   00 00 00 00 08 00 00
 * -> data  2048 (0x800)
 * <- ae 04 00 00 00   ff 00 00 00 00 00 00 
 * 
 * -> ae 0c 08 00 00   ff 00 00
 * -> 05 00 00 00 01   00 00 00 00 08 00 00
 * -> data  2048
 * <- ae 04 00 00 00   ff 00 00 00 00 00 00
 * 
 *             ................
 * -> ae 0c 08 00 00   ff 00 00
 * -> 05 00 00 00 02   00 00 00 00 08 00 00  HOSTFDL_DATA
 * -> data  2048
 * <- ae 04 00 00 00   ff 00 00 00 00 00 00
 * 
 * -> ae 4c 03 00 00   ff 00 00
 * -> 05 00 00 00 06   00 00 00 40 03 00 00
 * -> data  832 (0x340)
 * <- ae 04 00 00 00   ff 00 00 00 00 00 00
 * 
 * -> ae 10 00 00 00   ff 00 00
 * -> 06 00 00 00 00   00 00 00 00 00 00 00
 * -> 1c 3c 6e 06
 * <- ae 04 00 00 00   ff 00 00 00 00 00 00
 * -> ae 0c 00 00 00   ff 00 00 00 00 00 00
 * -> 07 00 00 00 06   00 00 00 00 00 00 00
 * 
 */
#endif //__PACKETS__
