/*
 * @Author: sinpo828
 * @Date: 2021-02-07 10:26:30
 * @LastEditors: sinpo828
 * @LastEditTime: 2021-02-19 14:03:00
 * @Description: file content
 */
#include <iostream>
#include <string>
#include <fstream>
#include <functional>
#include <algorithm>

#include <cstring>

extern "C"
{
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
}

#include "../third-party/tinyxml2/tinyxml2.h"
using namespace tinyxml2;

#include "scopeguard.hpp"
#include "firmware.hpp"

Firmware::Firmware(const std::string pacf)
    : pac_file(pacf), pachdr(nullptr), binhdr(nullptr)
{
    pachdr = new (std::nothrow) pac_header_t;
}

Firmware::~Firmware()
{
    if (pachdr)
        delete pachdr;
    pachdr = nullptr;

    if (binhdr)
        delete[] binhdr;
    binhdr = nullptr;
}

#define WCHARSTR(p) wcharToChar(p, sizeof(p))
std::string Firmware::wcharToChar(uint16_t *pBuf, int nSize)
{
    wchar_t wbuf[1024];
    char buf[1024];

    memset(wbuf, 0, sizeof(wbuf));
    memset(wbuf, 0, sizeof(buf));
    for (int i = 0; i < nSize / 2; i++)
        wbuf[i] = pBuf[i];

    wcstombs(buf, wbuf, nSize);

    return std::string(buf);
}

int Firmware::pacparser()
{
    std::ifstream fin(pac_file, std::ios::binary);

    if (!fin.is_open())
    {
        std::cerr << "fail to open " << pac_file << std::endl;
        return -1;
    }

    ON_SCOPE_EXIT
    {
        if (fin.is_open())
            fin.close();
    };

    fin.seekg(0, std::ios::end);
    std::cerr << pac_file << " has size in bytes " << fin.tellg() << std::endl;
    fin.seekg(0, std::ios::beg);

    fin.read(reinterpret_cast<char *>(pachdr), sizeof(*pachdr));
    std::cerr << std::dec << "FileCount: " << pachdr->nFileCount << std::endl;
    std::cerr << "ProductName: " << WCHARSTR(pachdr->szPrdName) << std::endl;
    std::cerr << "ProductVersion: " << WCHARSTR(pachdr->szPrdVersion) << std::endl;
    std::cerr << "ProductAlias: " << WCHARSTR(pachdr->szPrdAlias) << std::endl;
    std::cerr << "Version: " << WCHARSTR(pachdr->szVersion) << std::endl;

    binhdr = new (std::nothrow) bin_header_t[pachdr->nFileCount];
    for (int i = 0; i < pachdr->nFileCount; i++)
    {
        fin.read(reinterpret_cast<char *>(&binhdr[i]), sizeof(bin_header_t));

        uint64_t filesz = binhdr[i].dwLoFileSize;
        std::cerr << "idx: " << i
                  << ", FileID: " << WCHARSTR(binhdr[i].szFileID)
                  << ", FileName: " << WCHARSTR(binhdr[i].szFileName)
                  << std::dec << ", Size: " << filesz << std::endl;
    }

    return 0;
}

uint32_t Firmware::pac_file_count()
{
    return pachdr->nFileCount;
}

int Firmware::unpack(int idx, const std::string &extdir)
{
    std::ofstream fout;
    char buff[4 * 1024];
    uint64_t filesz;
    std::string fpath;
    std::ifstream fin(pac_file, std::ios::binary);

    if (!fin.is_open())
    {
        std::cerr << "fail to open " << pac_file << std::endl;
        return -1;
    }

    ON_SCOPE_EXIT
    {
        if (fin.is_open())
            fin.close();

        if (fout.is_open())
            fout.close();
    };

    filesz = file_size(idx);
    fpath = extdir + "/" + WCHARSTR(binhdr[idx].szFileName);
    if (filesz == 0)
        return 0;

    std::cerr << "Unpack idx: " << idx
              << ", FileID: " << WCHARSTR(binhdr[idx].szFileID)
              << ", FileName: " << fpath << std::endl;
    fout.open(fpath, std::ios::binary | std::ios::trunc);
    if (!fout.is_open())
    {
        std::cerr << "fail to open " << fpath << " for write" << std::endl;
        return -1;
    }

    fin.seekg(file_offset(idx), std::ios::beg);
    while (filesz > 0)
    {
        size_t sz = filesz > sizeof(buff) ? sizeof(buff) : filesz;

        fin.read(buff, sz);
        fout.write(buff, sz);
        filesz -= sz;
    };
    fin.close();
    fout.close();

    return 0;
}

int Firmware::unpack(const std::string &idstr, const std::string &extdir)
{
    int idx = fileid_to_index(idstr);

    if (!is_index_valid(idx))
    {
        std::cerr << __func__ << " invalid index error" << std::endl;
        return -1;
    }

    return unpack(idx, extdir);
}

int Firmware::unpack_all(const std::string &extdir)
{
    if (pacparser())
        return -1;

    for (int i = 0; i < pachdr->nFileCount; i++)
        unpack(i, extdir);
    return 0;
}

XMLNode *Firmware::xmltree_find_node(XMLNode *root, const std::string &nm)
{
    if (!root || root->NoChildren())
        return nullptr;

    if (std::string(root->Value()) == nm)
        return root;

    for (auto n = root->FirstChild(); n; n = n->NextSibling())
    {
        auto r = xmltree_find_node(n, nm);
        if (r)
            return r;
    }

    return nullptr;
};

// <Scheme name = "UIX8910_MODEM">
//     <File>
//         <ID> HOST_FDL</ ID>
//         <IDAlias> HOST_FDL</ IDAlias>
//         <Type> HOST_FDL</ Type>
//         <Block>
//             <Base> 0x838000 < / Base >
//             <Size> 0x8000 < / Size >
//         </ Block>
//         <Flag> 1 < / Flag >
//         <CheckFlag> 1 < / CheckFlag >
//         <Description> HOST_FDL</ Description>
//    </ File>
int Firmware::xmlparser_file(XMLNode *node)
{
    auto filenode = node->FirstChild();
    if (std::string(filenode->Value()) != "File")
    {
        std::cerr << __func__ << " error" << std::endl;
        return -1;
    }

    std::cerr << __func__ << " try to farser file info" << std::endl;
#define CONSTCHARTOINT(p) (p ? atoi(p) : 0)
#define CONSTCHARTOXINT(p) (p ? strtoul(p, NULL, 16) : 0)
    for (; filenode; filenode = filenode->NextSibling())
    {
        int idx = 0;
        XMLFileInfo info;
        info.reset();
        auto n = xmltree_find_node(filenode, "ID");
        if (n)
            info.fileid = n->FirstChild()->Value();

        n = xmltree_find_node(filenode, "Base");
        if (n)
            info.base = CONSTCHARTOXINT(n->FirstChild()->Value());

        n = xmltree_find_node(filenode, "Size");
        if (n)
            info.size = CONSTCHARTOXINT(n->FirstChild()->Value());

        n = xmltree_find_node(filenode, "Flag");
        if (n)
            info.flag = CONSTCHARTOINT(n->FirstChild()->Value());

        n = xmltree_find_node(filenode, "CheckFlag");
        if (n)
            info.checkflag = CONSTCHARTOINT(n->FirstChild()->Value());

        idx = fileid_to_index(info.fileid);
        if (idx < 0)
            continue;

        info.realsize = file_size(idx);
        xmlfilevec.push_back(info);
        std::cerr << "idx: " << idx
                  << ", FILEID: " << info.fileid
                  << ", Base: 0x" << std::hex << info.base
                  << ", Size: 0x" << std::hex << info.size
                  << ", RealSize: 0x" << std::hex << info.realsize
                  << ", Flag: " << info.flag
                  << ", CheckFlag: " << info.checkflag
                  << std::dec << std::endl;
    }
    std::cerr << __func__ << " farser file info end" << std::endl;

    return 0;
}

//<Product name = "UIX8910_MODEM">
//    <SchemeName> UIX8910_MODEM</ SchemeName>
//    <FlashTypeID> 1 < / FlashTypeID >
//    <Mode> 0 < / Mode >
//    <NVBackup backup = "1">
//        <NVItem backup = "1" name = "Calibration">
//            <ID> 0xffffffff < / ID >
//            <BackupFlag use = "1" />
//        </ NVItem>
//        <NVItem backup = "1" name = "GSM Calibration">
//            <ID> 0x26d < / ID >
//            <BackupFlag use = "1">
//                <NVFlag check = "1" name = "Continue" />
//            </ BackupFlag>
//        </ NVItem>
//        <NVItem backup = "1" name = "LTE Calibration">
//            <ID> 0x26e < / ID >
//            <BackupFlag use = "1" />
//        </ NVItem>
//        <NVItem backup = "1" name = "IMEI">
//            <ID> 0xffffffff < / ID >
//            <BackupFlag use = "1" />
//        </ NVItem>
//    </ NVBackup>
//    <Chips enable = "0">
//        <ChipItem id = "0x2222" name = "L2" />
//        <ChipItem id = "0x7777" name = "L7" />
//    </ Chips>
//</ Product>

int Firmware::xmlparser_nv(XMLNode *)
{
    std::cerr << __func__ << " current not implemented error" << std::endl;
    return true;
}

int Firmware::xmlparser()
{
    char *xmlbuf = nullptr;
    XMLDocument doc;
    XMLError xmlerr;
    int xmlidx = -1;

    ON_SCOPE_EXIT
    {
        doc.Clear();
        if (xmlbuf)
            delete[] xmlbuf;
        xmlbuf = nullptr;
    };

    for (int i = 0; i < pachdr->nFileCount; i++)
    {
        if (WCHARSTR(binhdr[i].szFileName).find(".xml") != std::string::npos)
        {
            xmlidx = i;
            break;
        }
    }

    if (!is_index_valid(xmlidx))
    {
        std::cerr << __func__ << " invalid index error" << std::endl;
        return -1;
    }
    else
    {
        size_t filesz = 0;
        size_t fileoffset = 0;
        std::ifstream fin(pac_file);

        filesz = file_size(xmlidx);
        fileoffset = file_offset(xmlidx);
        xmlbuf = new (std::nothrow) char[filesz + 1]();
        memset(xmlbuf, 0, filesz + 1);

        fin.seekg(fileoffset, std::ios::beg);
        fin.read(xmlbuf, filesz);
        fin.close();
        std::cerr << "load xml data from pac file, size:" << std::dec
                  << filesz << " offset:" << fileoffset << std::endl;
    }

    xmlerr = doc.Parse(xmlbuf);
    if (xmlerr == XML_SUCCESS)
    {
        XMLNode *scheme = xmltree_find_node(doc.RootElement(), "Scheme");
        if (scheme)
            return xmlparser_file(scheme);
    }

    std::cerr << xmlbuf << std::endl;
    std::cerr << "cannot parser xml for Parse failed, err=" << xmlerr << std::endl;
    return -1;
}

const std::vector<XMLFileInfo> &Firmware::get_file_vec() const
{
    return xmlfilevec;
}

const std::vector<XMLNVInfo> &Firmware::get_nv_vec() const
{
    return xmlnvvec;
}

bool Firmware::is_index_valid(int idx)
{
    return (idx >= 0 && idx < pachdr->nFileCount);
}

int Firmware::fileid_to_index(const std::string &idstr)
{
    std::string idstr_upper(idstr);
    std::transform(idstr.begin(), idstr.end(), idstr_upper.begin(), toupper);
    for (int idx = 0; idx < pachdr->nFileCount; idx++)
    {
        std::string fileid_upper(WCHARSTR(binhdr[idx].szFileID));
        std::transform(fileid_upper.begin(), fileid_upper.end(), fileid_upper.begin(), toupper);
        if (!idstr.empty() && idstr_upper == fileid_upper)
            return idx;
    }

    std::cerr << __func__ << " invalid file id: " << idstr << std::endl;
    return -1;
}

size_t Firmware::file_size(int idx)
{
    if (!is_index_valid(idx))
    {
        std::cerr << __func__ << " invalid index error" << std::endl;
        return 0;
    }

    return binhdr[idx].dwLoFileSize;
}

size_t Firmware::file_size(const std::string &idstr)
{
    return file_size(fileid_to_index(idstr));
}

size_t Firmware::file_offset(int idx)
{
    size_t offset = sizeof(pac_header_t) + sizeof(bin_header_t) * pachdr->nFileCount;
    if (!is_index_valid(idx))
    {
        std::cerr << __func__ << " invalid index error" << std::endl;
        return 0;
    }

    for (int i = 0; i < pachdr->nFileCount; i++)
    {
        if (i == idx)
            break;
        offset += binhdr[i].dwLoFileSize;
    }

    return offset;
}

size_t Firmware::file_offset(const std::string &idstr)
{
    return file_offset(fileid_to_index(idstr));
}

bool Firmware::get_data(const std::string &idstr, size_t offset, uint8_t *buf, uint32_t sz)
{
    std::ifstream fin(pac_file, std::ios::binary);
    int idx = fileid_to_index(idstr);
    if (!is_index_valid(idx))
    {
        std::cerr << __func__ << " invalid index error" << std::endl;
        return false;
    }

    if (!fin.is_open())
    {
        std::cerr << "fail to open " << pac_file << std::endl;
        return false;
    }

    fin.seekg(file_offset(idx) + offset, std::ios::beg);
    fin.read(reinterpret_cast<char *>(buf), sz);

    fin.close();

    return true;
}