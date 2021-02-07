/*
 * @Author: sinpo828
 * @Date: 2021-02-04 14:04:11
 * @LastEditors: sinpo828
 * @LastEditTime: 2021-02-05 18:00:04
 * @Description: file content
 */
#include <iostream>
#include <string>

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

/**
 * 7e -> 7d 5e
 * 7d -> 7d 5d
 */
const static uint8_t EDGE_NUMBER = 0x7e;

struct cmd_header
{
    uint8_t header;
    uint16_t cmd_type;
    uint16_t data_length;
    uint8_t data[0];
} __attribute__((__packed__));

struct cmd_tail
{
    uint16_t crc16;
    uint8_t tail;
} __attribute__((__packed__));

class Command final
{
private:
    const int max_data_len = 4 * 1024;
    uint8_t *_data;
    uint16_t _reallen;
    cmd_header *framehdr;
    cmd_tail *frametail;
    std::string version;

public:
    Command(const std::string &v) : version(v)
    {
        _data = new uint8_t[max_data_len];
        framehdr = reinterpret_cast<cmd_header *>(_data);
    }

    ~Command()
    {
        if (_data)
            delete[] _data;

        _data = nullptr;
        framehdr = nullptr;
    }

    uint8_t *data() { return _data; }

    uint16_t datalen() { return _reallen; }

    void escape(uint8_t *data, uint16_t len)
    {
        int num_of_7e = 0;
        for (auto pos = 0; pos < len; pos++)
        {
            if (EDGE_NUMBER == data[pos])
                num_of_7e++;
        }
    }

    unsigned int crc16(char *buf_ptr, unsigned int len)
    {
#define CRC_16_POLYNOMIAL 0x1021
#define CRC_16_L_POLYNOMIAL 0x8000
#define CRC_16_L_SEED 0x80
#define CRC_16_L_OK 0x00
#define CRC_CHECK_SIZE 0x02

        unsigned int i;
        unsigned short crc = 0;
        while (len-- != 0)
        {
            for (i = CRC_16_L_SEED; i != 0; i = i >> 1)
            {
                if ((crc & CRC_16_L_POLYNOMIAL) != 0)
                {
                    crc = crc << 1;
                    crc = crc ^ CRC_16_POLYNOMIAL;
                }
                else
                    crc = crc << 1;
                if ((*buf_ptr & i) != 0)
                    crc = crc ^ CRC_16_POLYNOMIAL;
            }
            buf_ptr++;
        }
        return (crc);
    }

    void newCheckBaud()
    {
        _data[0] = 0x7e;
        _reallen = 1;
    }

    void newConnect()
    {
        framehdr->header = EDGE_NUMBER;
        framehdr->cmd_type = 0;
        framehdr->data_length = 0;
    }

    void newStartData()
    {
    }

    void newMidstData()
    {
    }

    void newEndData()
    {
    }

    void newChangeBaud(){}
};

class Response
{
public:
    int parser(uint8_t *d, uint16_t len);

    REPTYPE cmdtype() {}

    void unescape() {}

    uint8_t *data() {}

    uint16_t datalen() {}
};
