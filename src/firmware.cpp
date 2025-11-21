/*
 * @Author: sinpo828
 * @Date: 2021-02-07 10:26:30
 * @LastEditors: sinpo828
 * @LastEditTime: 2021-02-26 11:42:31
 * @Description: file content
 */
#include <iostream>
#include <string>
#include <fstream>
#include <functional>
#include <algorithm>

#include <cstring>

extern "C" {
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
}

#include "tinyxml2/tinyxml2.h"

#include "scopeguard.hpp"
#include "firmware.hpp"

Firmware::Firmware(const std::string pacf) : pac_file(pacf), pachdr(nullptr), binhdr(nullptr) {
    pachdr = new (std::nothrow) pac_header_t;
}

Firmware::~Firmware() {
    if (pachdr) delete pachdr;
    pachdr = nullptr;

    if (binhdr) delete[] binhdr;
    binhdr = nullptr;
}

#define WCHARSTR(p) wcharToChar(p, sizeof(p))
std::string Firmware::wcharToChar(uint16_t* pBuf, int nSize) {
    wchar_t wbuf[1024];
    char buf[1024];

    memset(wbuf, 0, sizeof(wbuf));
    memset(buf, 0, sizeof(buf));
    for (int i = 0; i < nSize / 2; i++) wbuf[i] = pBuf[i];

    wcstombs(buf, wbuf, nSize);

    return std::string(buf);
}

int Firmware::pacparser() {
    std::ifstream fin(pac_file, std::ios::binary);

    if (!fin.is_open()) {
        std::cerr << "fail to open " << pac_file << std::endl;
        return -1;
    }

    ON_SCOPE_EXIT {
        if (fin.is_open()) fin.close();
    };

    fin.seekg(0, std::ios::end);
    std::cerr << pac_file << " has size in bytes " << fin.tellg() << std::endl;
    fin.seekg(0, std::ios::beg);

    fin.read(reinterpret_cast<char*>(pachdr), sizeof(*pachdr));
    std::cerr << std::dec << "FileCount: " << pachdr->nFileCount << std::endl;
    std::cerr << "ProductName: " << WCHARSTR(pachdr->szPrdName) << std::endl;
    std::cerr << "ProductVersion: " << WCHARSTR(pachdr->szPrdVersion) << std::endl;
    std::cerr << "ProductAlias: " << WCHARSTR(pachdr->szPrdAlias) << std::endl;
    std::cerr << "Version: " << WCHARSTR(pachdr->szVersion) << std::endl;

    binhdr = new (std::nothrow) bin_header_t[pachdr->nFileCount];
    for (uint32_t i = 0; i < pachdr->nFileCount; i++) {
        fin.read(reinterpret_cast<char*>(&binhdr[i]), sizeof(bin_header_t));
        uint64_t filesz = binhdr[i].dwLoFileSize;
        std::cerr << "idx: " << i << ", FileID: " << WCHARSTR(binhdr[i].szFileID)
                  << ", FileName: " << WCHARSTR(binhdr[i].szFileName) << std::dec << ", Size: " << filesz << std::endl;
    }

    return 0;
}

uint32_t Firmware::pac_file_count() { return pachdr->nFileCount; }

const std::string Firmware::productName() { return WCHARSTR(pachdr->szPrdName); }

const std::string Firmware::productVersion() { return WCHARSTR(pachdr->szPrdVersion); }

int Firmware::unpack(int idx, const std::string& extdir) {
    std::ofstream fout;
    char buff[4 * 1024];
    uint64_t filesz;
    std::string fpath;
    std::ifstream fin(pac_file, std::ios::binary);

    if (!fin.is_open()) {
        std::cerr << "fail to open " << pac_file << std::endl;
        return -1;
    }

    ON_SCOPE_EXIT {
        if (fin.is_open()) fin.close();

        if (fout.is_open()) fout.close();
    };

    filesz = member_file_size(idx);
    fpath = extdir + "/" + WCHARSTR(binhdr[idx].szFileName);
    if (filesz == 0) return 0;

    std::cerr << "Unpack idx: " << idx << ", FileID: " << WCHARSTR(binhdr[idx].szFileID) << ", FileName: " << fpath
              << std::endl;
    fout.open(fpath, std::ios::binary | std::ios::trunc);
    if (!fout.is_open()) {
        std::cerr << "fail to open " << fpath << " for write" << std::endl;
        return -1;
    }

    fin.seekg(member_file_offset(idx), std::ios::beg);
    while (filesz > 0) {
        size_t sz = filesz > sizeof(buff) ? sizeof(buff) : filesz;

        fin.read(buff, sz);
        fout.write(buff, sz);
        filesz -= sz;
    };
    fin.close();
    fout.close();

    return 0;
}

int Firmware::unpack(const std::string& idstr, const std::string& extdir) {
    int idx = fileid_to_index(idstr);

    if (!is_index_valid(idx)) {
        std::cerr << __func__ << " invalid index error" << std::endl;
        return -1;
    }

    return unpack(idx, extdir);
}

int Firmware::unpack_all(const std::string& extdir) {
    if (pacparser()) return -1;

    for (uint32_t i = 0; i < pachdr->nFileCount; i++) unpack(i, extdir);
    return 0;
}

tinyxml2::XMLNode* Firmware::xmltree_find_node(tinyxml2::XMLNode* root, const std::string& nm) {
    if (!root || root->NoChildren()) return nullptr;

    if (std::string(root->Value()) == nm) return root;

    for (auto n = root->FirstChild(); n; n = n->NextSibling()) {
        auto r = xmltree_find_node(n, nm);
        if (r) return r;
    }

    return nullptr;
};

#define CONSTCHARTOINT(p) (p ? atoi(p) : 0)
#define CONSTCHARTOXINT(p) (p ? strtoul(p, NULL, 16) : 0)
int Firmware::xmlparser_file(tinyxml2::XMLNode* node) {
    auto filenode = node->FirstChild();
    if (std::string(filenode->Value()) != "File") {
        std::cerr << __func__ << " error" << std::endl;
        return -1;
    }

    std::cerr << __func__ << " try to farser file info" << std::endl;
    for (; filenode; filenode = filenode->NextSibling()) {
        int idx = 0;
        XMLFileInfo info;
        tinyxml2::XMLNode* n = nullptr;
        const char* backup = "0";

        if (filenode->ToElement() && filenode->ToElement()->FirstAttribute())
            backup = filenode->ToElement()->FirstAttribute()->Value();
        info.isBackup = bool(CONSTCHARTOINT(backup));

        n = xmltree_find_node(filenode, "ID");
        if (n) info.fileid = n->FirstChild()->Value();

        if (info.fileid.empty()) continue;

        idx = fileid_to_index(info.fileid);
        if (idx < 0) continue;

        n = xmltree_find_node(filenode, "Block");
        if (n && n->ToElement() && n->ToElement()->FirstAttribute())
            info.blockid = n->ToElement()->FirstAttribute()->Value();

        n = xmltree_find_node(filenode, "Type");
        if (n) info.type = n->FirstChild()->Value();

        n = xmltree_find_node(filenode, "Base");
        if (n) info.base = CONSTCHARTOXINT(n->FirstChild()->Value());

        n = xmltree_find_node(filenode, "Size");
        if (n) info.size = CONSTCHARTOXINT(n->FirstChild()->Value());

        n = xmltree_find_node(filenode, "Flag");
        if (n) info.flag = CONSTCHARTOINT(n->FirstChild()->Value());

        n = xmltree_find_node(filenode, "CheckFlag");
        if (n) info.checkflag = CONSTCHARTOINT(n->FirstChild()->Value());

        info.realsize = member_file_size(idx);
        xmlfilevec.push_back(info);
        std::cerr << "idx: " << idx << ", FILEID: "
                  << info.fileid
                  //   << ", IDAlias: " << info.fileid_alias
                  << ", Type: " << info.type << ", BlockID: " << info.blockid << ", Base: 0x" << std::hex << info.base
                  << ", Size: 0x" << std::hex << info.size << ", RealSize: 0x" << std::hex
                  << info.realsize
                  //   << ", Flag: " << info.flag
                  //   << ", CheckFlag: " << info.checkflag
                  << ", isBackup: " << info.isBackup << std::dec << std::endl;
    }
    std::cerr << __func__ << " parser file info end" << std::endl;

    return 0;
}

int Firmware::xmlparser_partition(tinyxml2::XMLNode* partitions) {
    for (auto partition = partitions->FirstChild(); partition; partition = partition->NextSibling()) {
        if (partition->ToElement() && partition->ToElement()->FirstAttribute()) {
            partition_info info;

            for (auto a = partition->ToElement()->FirstAttribute(); a; a = a->Next()) {
                if (!a->Name()) continue;

                if (std::string(a->Name()) == "id")
                    info.partition = a->Value();
                else if (std::string(a->Name()) == "size" && a->Value()) {
                    if (std::string(a->Value()).substr(0, 2) == "0x" || std::string(a->Value()).substr(0, 2) == "0X")
                        info.size = CONSTCHARTOXINT(a->Value());
                    else
                        info.size = CONSTCHARTOINT(a->Value());
                }

                if (!info.partition.empty() && info.size) {
                    xmlpartitonvec.push_back(info);
                    std::cerr << "BlockID: " << info.partition << ", Size: " << info.size << std::dec << std::endl;
                }
            }
        }
    }

    std::cerr << __func__ << " parser partition info end" << std::endl;
    return 0;
}

int Firmware::xmlparser() {
    char* xmlbuf = nullptr;
    tinyxml2::XMLDocument doc;
    tinyxml2::XMLError xmlerr;
    int xmlidx = -1;

    ON_SCOPE_EXIT {
        doc.Clear();
        if (xmlbuf) delete[] xmlbuf;
        xmlbuf = nullptr;
    };

    for (uint32_t i = 0; i < pachdr->nFileCount; i++) {
        if (WCHARSTR(binhdr[i].szFileName).find(".xml") != std::string::npos) {
            xmlidx = i;
            break;
        }
    }

    if (!is_index_valid(xmlidx)) {
        std::cerr << __func__ << " invalid index error" << std::endl;
        return -1;
    } else {
        size_t filesz = 0;
        size_t fileoffset = 0;
        std::ifstream fin(pac_file);

        filesz = member_file_size(xmlidx);
        fileoffset = member_file_offset(xmlidx);
        xmlbuf = new (std::nothrow) char[filesz + 1]();
        memset(xmlbuf, 0, filesz + 1);

        fin.seekg(fileoffset, std::ios::beg);
        fin.read(xmlbuf, filesz);
        fin.close();
        std::cerr << "load xml data from pac file, size:" << std::dec << filesz << " offset:" << fileoffset
                  << std::endl;
    }

    xmlerr = doc.Parse(xmlbuf);
    if (xmlerr == tinyxml2::XML_SUCCESS) {
        tinyxml2::XMLNode* scheme = xmltree_find_node(doc.RootElement(), "Scheme");
        tinyxml2::XMLNode* partitions = xmltree_find_node(doc.RootElement(), "Partitions");
        if (!scheme || xmlparser_file(scheme)) return -1;

        if (!partitions)
            std::cerr << __func__ << " warnning, xml contains no partition info" << std::endl;
        else
            xmlparser_partition(partitions);

        std::cerr << __func__ << " parser xml finish" << std::endl;
        return 0;
    }

    std::cerr << xmlbuf << std::endl;
    std::cerr << "cannot parser xml for Parse failed, err=" << xmlerr << std::endl;
    return -1;
}

const std::vector<XMLFileInfo>& Firmware::get_file_vec() const { return xmlfilevec; }

const std::vector<partition_info>& Firmware::get_partition_vec() const { return xmlpartitonvec; }

bool Firmware::is_index_valid(int idx) { return (idx >= 0 && idx < int(pachdr->nFileCount)); }

int Firmware::fileid_to_index(const std::string& idstr) {
    std::string idstr_upper(idstr);
    std::transform(idstr.begin(), idstr.end(), idstr_upper.begin(), toupper);
    for (uint32_t idx = 0; idx < pachdr->nFileCount; idx++) {
        std::string fileid_upper(WCHARSTR(binhdr[idx].szFileID));
        std::transform(fileid_upper.begin(), fileid_upper.end(), fileid_upper.begin(), toupper);
        if (!idstr.empty() && idstr_upper == fileid_upper) return idx;
    }

    std::cerr << __func__ << " invalid file id: " << idstr << std::endl;
    return -1;
}

size_t Firmware::member_file_size(int idx) {
    if (!is_index_valid(idx)) {
        std::cerr << __func__ << " invalid index error" << std::endl;
        return 0;
    }

    return binhdr[idx].dwLoFileSize;
}

size_t Firmware::member_file_size(const std::string& idstr) { return member_file_size(fileid_to_index(idstr)); }

size_t Firmware::member_file_offset(int idx) {
    size_t offset = sizeof(pac_header_t) + sizeof(bin_header_t) * pachdr->nFileCount;
    if (!is_index_valid(idx)) {
        std::cerr << __func__ << " invalid index error" << std::endl;
        return 0;
    }

    for (uint32_t i = 0; i < pachdr->nFileCount; i++) {
        if (i == uint32_t(idx)) break;
        offset += binhdr[i].dwLoFileSize;
    }

    return offset;
}

size_t Firmware::member_file_offset(const std::string& idstr) { return member_file_offset(fileid_to_index(idstr)); }

std::ifstream Firmware::open_pac(const std::string& idstr) {
    std::ifstream fin(pac_file, std::ios::binary);
    int idx = fileid_to_index(idstr);
    if (!is_index_valid(idx)) {
        fin.close();
        std::cerr << __func__ << " invalid index error" << std::endl;
    }

    fin.seekg(member_file_offset(idx), std::ios::beg);
    return fin;
}

std::ifstream Firmware::open_file(const std::string& fpath) {
    std::ifstream fin(pac_file, std::ios::binary);

    return fin;
}

void Firmware::close(std::ifstream& fin) {
    if (fin.is_open()) fin.close();
}

bool Firmware::read(std::ifstream& fin, uint8_t* buf, uint32_t sz) {
    if (!fin.is_open()) {
        std::cerr << __func__ << " pac file is not open" << std::endl;
        return false;
    }

    fin.read(reinterpret_cast<char*>(buf), sz);

    return true;
}

uint32_t Firmware::local_file_size(const std::string& fpath) {
    uint32_t sz = 0;
    std::ifstream fin(fpath, std::ios::binary);

    fin.seekg(0, std::ios::end);
    sz = fin.tellg();
    fin.close();

    return sz;
}