/*
 * @Author: sinpo828
 * @Date: 2021-02-07 10:26:30
 * @LastEditors: sinpo828
 * @LastEditTime: 2021-02-07 18:55:50
 * @Description: file content
 */
#ifndef __FIRMWARE__
#define __FIRMWARE__

#include <iostream>
#include <string>
#include <fstream>

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

enum class FILEID
{
    HOST_FDL,
    FDL2,
    BOOTLOADER,
    AP,
    PS,
    FMT_FSSYS,
    FLASH,
    NV,
    PREPACK,
    PHASECHECK,
    USER_XML, // user added fileid
};

class Firmware
{
private:
    std::string pac_file;
    std::string ext_dir;
    pac_header_t pachdr;
    bin_header_t *binhdr;
    std::ifstream file_opts;

public:
    Firmware(const std::string pacf, const std::string ext_dir = "packets");
    ~Firmware();

    std::string wcharToChar(uint16_t *pBuf, int nSize);
    int pacparser(bool unpack_files = false);
    int unpack(FILEID id);
    int unpack(int idx);

    int xmlparser();

    bool is_pac_ok();
    int fileid_to_index(FILEID id);
    bool file_prepare_by_idx(int idx);
    bool file_prepare_by_id(FILEID id);
    size_t file_offset_by_idx(int idx);
    size_t file_offset_by_id(FILEID id);
    size_t file_size_by_idx(int idx);
    size_t file_size_by_id(FILEID id);
};

#endif //__FIRMWARE__