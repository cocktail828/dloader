/*
 * @Author: sinpo828
 * @Date: 2021-02-23 18:19:08
 * @LastEditors: sinpo828
 * @LastEditTime: 2021-02-26 14:08:41
 * @Description: file content
 */
#ifndef __PROTOCOL__
#define __PROTOCOL__

enum class PROTOCOL {
    PROTO_PDL,
    PROTO_FDL,
};

class CMDRequest {
   protected:
    PROTOCOL proto;

   public:
    CMDRequest(PROTOCOL pro) : proto(pro) {}
    virtual ~CMDRequest() {}

    virtual uint8_t *rawData() = 0;
    virtual uint32_t rawDataLen() = 0;

    virtual std::string toString() = 0;
    virtual std::string argString() = 0;

    virtual int value() = 0;

    virtual bool onWrite() = 0;
    virtual bool onRead() = 0;
    virtual bool isDuplicate() = 0;
    virtual PROTOCOL protocol() final { return proto; }
};

class CMDResponse {
   protected:
    PROTOCOL proto;

   public:
    CMDResponse(PROTOCOL pro) : proto(pro){};
    virtual ~CMDResponse(){};

    virtual uint8_t *rawData() = 0;
    virtual uint32_t rawDataLen() = 0;
    virtual uint32_t expectLength() = 0;
    virtual uint32_t minLength() = 0;

    virtual void reset() = 0;
    virtual std::string toString() = 0;
    virtual int value() = 0;
    virtual void push_back(uint8_t *d, uint32_t len) = 0;
    virtual PROTOCOL protocol() final { return proto; }
};

#endif  //__PROTOCOL__