/*
 * @Author: sinpo828
 * @Date: 2021-02-07 10:26:30
 * @LastEditors: sinpo828
 * @LastEditTime: 2021-02-09 15:44:05
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
    : pac_file(pacf), binhdr(nullptr)
{
}

Firmware::~Firmware()
{
    if (binhdr)
        delete[] binhdr;
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
    ON_SCOPE_EXIT { fin.close(); };

    fin.seekg(0, std::ios::end);
    std::cerr << pac_file << " has size in bytes " << fin.tellg() << std::endl;
    fin.seekg(0, std::ios::beg);

    fin.read(reinterpret_cast<char *>(&pachdr), sizeof(pachdr));
    std::cerr << std::dec << "FileCount: " << pachdr.nFileCount << std::endl;
    std::cerr << "ProductName: " << WCHARSTR(pachdr.szPrdName) << std::endl;
    std::cerr << "ProductVersion: " << WCHARSTR(pachdr.szPrdVersion) << std::endl;
    std::cerr << "ProductAlias: " << WCHARSTR(pachdr.szPrdAlias) << std::endl;
    std::cerr << "Version: " << WCHARSTR(pachdr.szVersion) << std::endl;

    binhdr = new bin_header_t[pachdr.nFileCount];
    for (int i = 0; i < pachdr.nFileCount; i++)
    {
        fin.read(reinterpret_cast<char *>(&binhdr[i]), sizeof(bin_header_t));

        uint64_t filesz = binhdr[i].dwLoFileSize;
        std::cerr << "idx: " << i << " FileName: " << WCHARSTR(binhdr[i].szFileName)
                  << ", FileID: " << WCHARSTR(binhdr[i].szFileID)
                  << std::dec << ", Size: " << filesz << std::endl;
    }

    return 0;
}

int Firmware::unpack_by_idx(int idx, const std::string &extdir)
{
    std::ofstream fout;
    char buff[4 * 1024];
    uint64_t filesz;
    std::string fpath;

    if (!file_prepare_by_idx(idx))
    {
        std::cerr << "fail to open " << pac_file << std::endl;
        return -1;
    }

    ON_SCOPE_EXIT { fout.close(); };

    if (access(extdir.c_str(), F_OK))
    {
        std::cerr << "cannot access dir " << extdir << std::endl;
        return -1;
    }

    filesz = file_size_by_idx(idx);
    fpath = extdir + "/" + WCHARSTR(binhdr[idx].szFileName);
    if (WCHARSTR(binhdr[idx].szFileName).empty())
        return -1;

    std::cerr << "idx: " << idx << " Try to unpack FileName: "
              << fpath << ", FileID: " << WCHARSTR(binhdr[idx].szFileID) << std::endl;
    fout.open(fpath, std::ios::binary | std::ios::trunc);
    if (!fout.is_open())
    {
        std::cerr << "fail to open " << fpath << " for write" << std::endl;
        return -1;
    }

    while (filesz > 0)
    {
        size_t sz = filesz > sizeof(buff) ? sizeof(buff) : filesz;

        file_opts.read(buff, sz);
        fout.write(buff, sz);
        filesz -= sz;
    };
    fout.close();

    return 0;
}

int Firmware::unpack_by_id(FILEID id, const std::string &extdir)
{
    int idx = fileid_to_index(id);
    if (idx < 0 || idx > pachdr.nFileCount)
    {
        std::cerr << "error " << __PRETTY_FUNCTION__ << std::endl;
        return -1;
    }

    return unpack_by_idx(idx);
}

int Firmware::unpack_all(const std::string &extdir)
{
    if (!pacparser())
    {
        for (int i = 0; i < pachdr.nFileCount; i++)
            unpack_by_idx(i, extdir);
        return 0;
    }

    return -1;
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

XMLNode *Firmware::xmltree_find_node(XMLDocument *doc, const std::string &nm)
{
    if (!doc || !doc->RootElement())
        return nullptr;

    for (auto n = doc->RootElement()->FirstChild(); n; n = n->NextSibling())
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
        XMLFileInfo info;
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

        info.realsize = file_size_by_idx(fileidstr_to_index(info.fileid));
        xmlfilevec.push_back(info);
        std::cerr << "FILEID: " << info.fileid
                  << ", Base: 0x" << std::hex << info.base
                  << ", Size: 0x" << std::hex << info.size
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
    std::cerr << "current not implemented error " << __PRETTY_FUNCTION__ << std::endl;
    return true;
}

int Firmware::xmlparser()
{
    int idx = xml_index();
    if (!is_index_valid(idx))
    {
        std::cerr << __PRETTY_FUNCTION__ << " fileid_to_index error" << std::endl;
        return -1;
    }
    else
    {
        char *xmlbuf = nullptr;
        XMLDocument doc;
        XMLError xmlerr;
        size_t filesz = file_size_by_idx(idx);

        ON_SCOPE_EXIT
        {
            doc.Clear();
            if (xmlbuf)
                delete[] xmlbuf;
        };

        xmlbuf = new char[filesz];
        memset(xmlbuf, 0, filesz);

        file_prepare_by_idx(idx);
        file_opts.read(xmlbuf, filesz);
        xmlerr = doc.Parse(xmlbuf);
        if (xmlerr != XML_SUCCESS)
        {
            std::cerr << xmlbuf << std::endl;
            std::cerr << "cannot parser xml for Parse failed, err=" << xmlerr << std::endl;
            return -1;
        }
        std::cerr << "xml parser ok" << std::endl;

        XMLNode *scheme = xmltree_find_node(&doc, "Scheme");
        if (scheme)
            return xmlparser_file(scheme);
        return -1;
    }

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

bool Firmware::is_pac_ok()
{
    return (binhdr);
}

bool Firmware::is_index_valid(int idx)
{
    return !(idx < 0 || idx > pachdr.nFileCount);
}

int Firmware::xml_index()
{
    if (!is_pac_ok())
        return -1;

    for (int i = 0; i < pachdr.nFileCount; i++)
    {
        if (WCHARSTR(binhdr[i].szFileName).find(".xml") != std::string::npos)
        {
            std::cerr << "find xml at " << i << std::endl;
            return i;
        }
    }

    std::cerr << "find no file with suffix .xml" << std::endl;
    return -1;
}

int Firmware::fileid_to_index(FILEID id)
{
    std::string fileidstr = FileIDs[static_cast<int>(id)];

    if (!is_pac_ok())
        return -1;

    for (int i = 0; i < pachdr.nFileCount; i++)
    {
        std::string str(WCHARSTR(binhdr[i].szFileID));
        std::transform(str.begin(), str.end(), str.begin(), toupper);
        if (!str.empty() && str == fileidstr)
            return i;
    }

    std::cerr << "find no file with id: " << static_cast<int>(id) << std::endl;
    return -1;
}

int Firmware::fileidstr_to_index(const std::string &idstr)
{
    std::string str;
    std::transform(idstr.begin(), idstr.end(), str.begin(), toupper);
    for (int idx = 0; idx < sizeof(FileIDs) / sizeof(FileIDs[0]); idx++)
    {
        if (str == FileIDs[idx])
            return idx;
    }

    return -1;
}

bool Firmware::file_prepare_by_idx(int idx)
{
    file_opts.close();
    file_opts.open(pac_file);
    if (!file_opts.is_open())
    {
        std::cerr << "fail to open " << pac_file << std::endl;
        return false;
    }

    file_opts.seekg(file_offset_by_idx(idx), std::ios::cur);

    return true;
}

bool Firmware::file_prepare_by_id(FILEID id)
{
    int idx = fileid_to_index(id);
    if (!is_index_valid(idx))
    {
        std::cerr << "fileid_to_index error " << __PRETTY_FUNCTION__ << std::endl;
        return -1;
    }

    return file_prepare_by_idx(idx);
}

size_t Firmware::file_offset_by_idx(int idx)
{
    size_t offset = sizeof(pac_header_t) + sizeof(bin_header_t) * pachdr.nFileCount;
    for (int i = 0; i < pachdr.nFileCount; i++)
    {
        if (i == idx)
            break;
        offset += binhdr[i].dwLoFileSize;
    }

    return offset;
}

size_t Firmware::file_offset_by_id(FILEID id)
{
    int idx = fileid_to_index(id);
    if (!is_index_valid(idx))
    {
        std::cerr << "fileid_to_index error " << __PRETTY_FUNCTION__ << std::endl;
        return -1;
    }

    return file_offset_by_idx(idx);
}

size_t Firmware::file_size_by_idx(int idx)
{
    return binhdr[idx].dwLoFileSize;
}

size_t Firmware::file_size_by_id(FILEID id)
{
    int idx = fileid_to_index(id);
    if (idx < 0 || idx > pachdr.nFileCount)
    {
        std::cerr << "fileid_to_index error " << __PRETTY_FUNCTION__ << std::endl;
        return -1;
    }

    return file_size_by_idx(idx);
}

bool Firmware::open(int idx)
{
    if (!is_index_valid(idx))
    {
        std::cerr << "fileid_to_index error " << __PRETTY_FUNCTION__ << std::endl;
        return false;
    }

    return file_prepare_by_idx(idx);
}

uint32_t Firmware::read(uint8_t *buf, uint32_t len)
{
    file_opts.read(reinterpret_cast<char *>(buf), len);
    return len;
}

void Firmware::close()
{
    file_opts.close();
}