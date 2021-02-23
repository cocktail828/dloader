/*
 * @Author: sinpo828
 * @Date: 2021-02-23 18:19:08
 * @LastEditors: sinpo828
 * @LastEditTime: 2021-02-23 20:14:14
 * @Description: file content
 */
#ifndef __PROTOCOL__
#define __PROTOCOL__

class CMDRequest
{
public:
    virtual uint8_t *rawdata() = 0;
    virtual uint32_t rawdatalen() = 0;

    virtual std::string toString() = 0;
    virtual std::string argString() = 0;

    virtual int ordinal() = 0;

    virtual bool isDuplicate() = 0;
    virtual uint32_t expect_len() = 0;
};

class CMDResponse
{
public:
    virtual uint8_t *rawdata() = 0;
    virtual uint32_t rawdatalen() = 0;

    virtual void reset() = 0;
    virtual std::string toString() = 0;
    virtual int ordinal() = 0;
    virtual void push_back(uint8_t *d, uint32_t len) = 0;
};

#endif //__PROTOCOL__