/*
 * @Author: sinpo828
 * @Date: 2021-02-10 09:05:20
 * @LastEditors: sinpo828
 * @LastEditTime: 2021-02-19 14:31:18
 * @Description: file content
 */
#include <iostream>

#include <cstring>

#include "fdl1.hpp"

#pragma pack(1)
struct fdl_cmd0
{
    uint8_t magic; // fixed 0xae
    uint16_t type;
    uint8_t unknow0;
    uint32_t unknow1;
};

struct fdl_cmd1
{
    uint32_t type;
    uint32_t unknow;
    uint32_t totallen;
};

struct fdl_cmd_data
{
    uint32_t cmd;
    uint32_t index;  // start from 0 little-endian
    uint32_t length; // length of data
};

struct fdl_cmd_end
{
    uint8_t magic; // fixed 0xae
    uint16_t type;
    uint8_t unknow0;
    uint32_t unknow1;
    uint32_t unknow2;
};

#pragma pack()

#define FDL1_MAGIC 0xae
#define FDL1_TAIL 0xff00
class fdl1
{
    uint8_t *_data;
    uint32_t _reallen;
    int _index;

public:
    // ae 04 00 00 00   ff 00 00 00 00 00 00 resp

    void newInitCmd()
    {
        auto hdr = reinterpret_cast<fdl_cmd0 *>(_data);
        hdr->magic = FDL1_MAGIC;
        hdr->type = htole16(0xc);
        hdr->unknow0 = 0;
        hdr->unknow1 = htole16(FDL1_TAIL);

        _reallen = sizeof(fdl_cmd0);
    }

    void newInitDone()
    {
        memset(_data, 0, sizeof(fdl_cmd1));
        _reallen = sizeof(fdl_cmd1);
    }
    // ae 04 00 00 00   ff 00 00 00 00 00 00 resp

    void newStartData()
    {
        auto hdr = reinterpret_cast<fdl_cmd0 *>(_data);
        hdr->magic = FDL1_MAGIC;
        hdr->type = htole16(0x11);
        hdr->unknow0 = 0;
        hdr->unknow1 = htole16(FDL1_TAIL);

        _reallen = sizeof(fdl_cmd0);
    }

    void newStartDataDone(uint32_t sz)
    {
        auto hdr = reinterpret_cast<fdl_cmd1 *>(_data);
        hdr->type = htole32(0x4);
        hdr->unknow = htole32(0x838000);
        hdr->totallen = htole16(sz);

        _reallen = sizeof(fdl_cmd1);
    }

    void newDataName()
    {
        _reallen = 0;
        _data[_reallen++] = 'P';
        _data[_reallen++] = 'D';
        _data[_reallen++] = 'L';
        _data[_reallen++] = '1';
    }
    // ae 04 00 00 00   ff 00 00 00 00 00 00 resp

    void newMinstData(bool full)
    {
        auto hdr = reinterpret_cast<fdl_cmd0 *>(_data);
        hdr->magic = FDL1_MAGIC;
        hdr->type = htole16(full ? 0x080c : 0x034c);
        hdr->unknow0 = 0;
        hdr->unknow1 = htole16(FDL1_TAIL);

        _reallen = sizeof(fdl_cmd0);
    }

    void newMinstDataDone(uint32_t sz)
    {
        auto hdr = reinterpret_cast<fdl_cmd1 *>(_data);
        hdr->type = htole16(0x05);
        hdr->unknow = htole32(_index);
        hdr->totallen = htole32(sz);

        _reallen = sizeof(fdl_cmd1);
    }
    // ... rawdata ...
    // ae 04 00 00 00   ff 00 00 00 00 00 00

    void newEndData()
    {
        auto hdr = reinterpret_cast<fdl_cmd0 *>(_data);
        hdr->magic = FDL1_MAGIC;
        hdr->type = htole16(0x10);
        hdr->unknow0 = 0;
        hdr->unknow1 = htole16(FDL1_TAIL);

        _reallen = sizeof(fdl_cmd0);
    }

    void newEndDataDone()
    {
        auto hdr = reinterpret_cast<fdl_cmd1 *>(_data);
        hdr->type = htole16(0x06);
        hdr->unknow = 0;
        hdr->totallen = 0;

        _reallen = sizeof(fdl_cmd1);
    }

    void newEndData1()
    {
        _reallen = 0;
        _data[_reallen++] = 0x1c;
        _data[_reallen++] = 0x3c;
        _data[_reallen++] = 0x6e;
        _data[_reallen++] = 0x06;
    }
    // ae 04 00 00 00   ff 00 00 00 00 00 00

    void newFinishup()
    {
        auto hdr = reinterpret_cast<fdl_cmd_end *>(_data);
        hdr->magic = FDL1_MAGIC;
        hdr->type = htole16(0x0c);
        hdr->unknow0 = 0;
        hdr->unknow1 = htole32(FDL1_TAIL);
        hdr->unknow2 = 0;

        _reallen = sizeof(fdl_cmd_end);
    }

    void newFinishupDone()
    {
        auto hdr = reinterpret_cast<fdl_cmd1 *>(_data);
        hdr->type = htole16(0x07);
        hdr->unknow = htole32(0x06);
        hdr->totallen = 0;

        _reallen = sizeof(fdl_cmd1);
    }
};