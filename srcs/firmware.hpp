/*
 * @Author: sinpo828
 * @Date: 2021-02-07 10:26:30
 * @LastEditors: sinpo828
 * @LastEditTime: 2021-02-10 14:20:08
 * @Description: file content
 */
#ifndef __FIRMWARE__
#define __FIRMWARE__

#include <iostream>
#include <string>
#include <fstream>
#include <vector>

#include "../third-party/tinyxml2/tinyxml2.h"

struct pac_header_t
{
    uint16_t szVersion[22];     // packet struct version; V1->V2 : 24*2 -> 22*2
    uint32_t dwHiSize;          // the whole packet hight size;
    uint32_t dwLoSize;          // the whole packet low size;
    uint16_t szPrdName[256];    // product name
    uint16_t szPrdVersion[256]; // product version
    uint32_t nFileCount;        // the number of files that will be downloaded, the file may be an operation
    uint32_t dwFileOffset;      // the offset from the packet file header to the array of FILE_T struct buffer
    uint32_t dwMode;
    uint32_t dwFlashType;
    uint32_t dwNandStrategy;
    uint32_t dwIsNvBackup;
    uint32_t dwNandPageType;
    uint16_t szPrdAlias[100]; // product alias
    uint32_t dwOmaDmProductFlag;
    uint32_t dwIsOmaDM;
    uint32_t dwIsPreload;
    uint32_t dwReserved[200];
    uint32_t dwMagic;
    uint16_t wCRC1;
    uint16_t wCRC2;
};

struct bin_header_t
{
    uint32_t dwSize;             // size of this struct itself
    uint16_t szFileID[256];      // file ID,such as FDL,Fdl2,NV and etc.
    uint16_t szFileName[256];    // file name,in the packet bin file,it only stores file name
                                 // but after unpacketing, it stores the full path of bin file
    uint16_t szFileVersion[252]; // Reserved now; V1->V2 : 256*2 --> 252*2
    uint32_t dwHiFileSize;       // hight file size
    uint32_t dwHiDataOffset;     // hight file size
    uint32_t dwLoFileSize;       // file size
    uint32_t nFileFlag;          // if "0", means that it need not a file, and
                                 // it is only an operation or a list of operations, such as file ID is "FLASH"
                                 // if "1", means that it need a file
    uint32_t nCheckFlag;         // if "1", this file must be downloaded;
                                 // if "0", this file can not be downloaded;
    uint32_t dwLoDataOffset;     // the offset from the packet file header to this file data
    uint32_t dwCanOmitFlag;      // if "1", this file can not be downloaded and not check it as "All files"
                                 //   in download and spupgrade tool.
    uint32_t dwAddrNum;
    uint32_t dwAddr[5];
    uint32_t dwReserved[249]; // Reserved for future,not used now
};

struct XMLFileInfo
{
    std::string fileid;
    uint32_t base;
    uint32_t size;
    uint32_t realsize;
    bool flag;
    bool checkflag;
};

struct XMLNVInfo
{
};

class Firmware
{
private:
    std::string pac_file;
    pac_header_t *pachdr;
    bin_header_t *binhdr;
    std::vector<XMLFileInfo> xmlfilevec;
    std::vector<XMLNVInfo> xmlnvvec;

private:
    tinyxml2::XMLNode *xmltree_find_node(tinyxml2::XMLDocument *, const std::string &);
    tinyxml2::XMLNode *xmltree_find_node(tinyxml2::XMLNode *, const std::string &);
    int xmlparser_file(tinyxml2::XMLNode *);
    int xmlparser_nv(tinyxml2::XMLNode *);
    bool is_index_valid(int idx);

public:
    Firmware(const std::string pacf);
    ~Firmware();

    std::string wcharToChar(uint16_t *pBuf, int nSize);

    // should parser pac fisrt before all operations
    int pacparser();

    // file counts including file that has size of 0
    uint32_t pac_file_count();

    int unpack(int idx, const std::string &extdir = "packets");
    int unpack(const std::string &idstr, const std::string &extdir = "packets");
    int unpack_all(const std::string &extdir = "packets");

    // xml has a empty fileid, that is ""
    int fileid_to_index(const std::string &idstr);
    size_t file_size(int idx);
    size_t file_size(const std::string &idstr);
    size_t file_offset(int idx);
    size_t file_offset(const std::string &idstr);

    int xmlparser();
    const std::vector<XMLFileInfo> &get_file_vec() const;
    const std::vector<XMLNVInfo> &get_nv_vec() const;

    bool get_data(const std::string &idstr, size_t offset, uint8_t *buf, uint32_t sz);
};

#endif //__FIRMWARE__