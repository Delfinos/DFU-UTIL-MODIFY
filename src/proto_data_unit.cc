
#include "proto_data_unit.h"
#undef _OFF_T_DEFINED
#include <string.h>
#define _OFF_T_DEFINED
// for debug
// #include <stdio.h>

// #include <iostream>
//#if (defined _WIN32 && defined _OFF_T_DEFINED)
//typedef off_t _off_t;
//#endif
constexpr UInt8 kProtoHeaderLen = 8;
constexpr UInt8 kBitFlagSet = 0x1;
constexpr UInt8 kBitFlagChecksumSet = 0x1 << 1;
constexpr UInt8 kBitFlagLenSet = 0x1 << 2;
constexpr UInt8 kBitFlagExtSet = 0x1 << 3;
constexpr UInt8 kBodySet = 0x1 << 4;
constexpr UInt8 kCompleteFlag = 0x1f;

//std::atomic<int> ProtoDataUnit::_cnt = 0;

ProtoDataUnit::ProtoDataUnit(const Bytes * data, UInt32 len, bool * ok):
    ProtoDataUnit() {
    bool scope_ok = false;
    if (!ok) {
        ok = &scope_ok;
    }
    if (!data) {
        return;
    }
    *ok = Init(data, len);
}

ProtoDataUnit::ProtoDataUnit(UInt32 len, bool * ok):
    ProtoDataUnit() {
    bool scope_ok;
    if (!ok) {
        ok = &scope_ok;
    }
    *ok = Init(nullptr, len);
}

ProtoDataUnit::ProtoDataUnit() {
    this->data_ = nullptr;
    this->data_len_ = 0;
    this->header_flag_ = 0;
    this->checksum_ = 0;
    this->data_len_ = 0;
    this->ext_ = 0;
    this->recv_flag_ = 0x0;
    this->stored_pos_ = 0x0;
}
/// note! some cases, pass a pointer var which value is nullptr,
/// will it cause any bugs? as I want to pass a valid pointer
bool ProtoDataUnit::Init(const Bytes* bts, Int32 len) {
    this->data_len_ = len;
    if (data_len_ == 0) {
        return false;
    }
    ext_ = 0xaaaa;
    checksum_ = (ext_ >> 8) ^ (ext_ & 0xff) ^ \
                (data_len_ >> 24) ^ \
                (data_len_ >> 16 & 0xff) \
                ^ (data_len_ >> 8 &0xff) ^ (data_len_ & 0xff);
    this->data_ = new Bytes[data_len_];
    if (bts) {
        memcpy(this->data_, bts, data_len_);
    }
    this->header_flag_ = 0xaa;
    return true;
}

ProtoDataUnit::~ProtoDataUnit() {
    delete[] this->data_;
    // printf("data_use_count:%d\n", this->data_.use_count());
    // printf("pdu sn:%d destructed!\n", _id);;
}

UInt32 ProtoDataUnit::BinaryLen() {
    if (data_len_ == 0) {
        return 0;
    }
    return data_len_ + kProtoHeaderLen;
}


bool ProtoDataUnit::SerializeToBin(Bytes* dst_mem, UInt32 dst_len) {

    if (!data_len_) {
        return false;
    }

    if (dst_len < data_len_ + kProtoHeaderLen) {
        return false;
    }

    // 注意大小端一致性问题，如何控制?自己编写读写函数
    int idx = 0;
#if 0
    memcpy(dst_mem + idx, &this->header_flag_, sizeof(this->header_flag_));
    idx += sizeof(this->header_flag_);
    memcpy(dst_mem + idx, & this->checksum_, sizeof(this->checksum_));
    idx += sizeof(this->checksum_);
    memcpy(dst_mem+ idx, &this->data_len_, sizeof(this->data_len_));
    idx += sizeof(this->data_len_);
    memcpy(dst_mem + idx, &this->ext_, sizeof(this->ext_));
    idx += sizeof(this->ext_);
    memcpy(dst_mem + idx, this->data_.get(), this->data_len_);
#endif
    idx += TSerializeNumericToBuffer(header_flag_, dst_len, idx, dst_mem);
    idx += TSerializeNumericToBuffer(checksum_, dst_len, idx, dst_mem);
    idx += TSerializeNumericToBuffer(data_len_, dst_len, idx, dst_mem);
    idx += TSerializeNumericToBuffer(ext_, dst_len, idx, dst_mem);
    memcpy(dst_mem + idx, this->data_, this->data_len_);
    return true;
}

bool ProtoDataUnit::SerializeHeaderToBin(Bytes * dst_mem, UInt32 dst_len) {
    if (!data_len_) {
        return false;
    }
    int idx = 0;
    idx += TSerializeNumericToBuffer(header_flag_, dst_len, idx, dst_mem);
    idx += TSerializeNumericToBuffer(checksum_, dst_len, idx, dst_mem);
    idx += TSerializeNumericToBuffer(data_len_, dst_len, idx, dst_mem);
    idx += TSerializeNumericToBuffer(ext_, dst_len, idx, dst_mem);
    return true;
}

void ProtoDataUnit::Reset() {
    this->header_flag_ = 0;
    this->checksum_ = 0;
    this->data_len_ = 0;
    this->ext_ = 0;
    this->recv_flag_ = 0x0;
    // fixed
    this->stored_pos_ = 0x0;
    if (this->data_) {
        delete[] this->data_;
        this->data_ = nullptr;
    }
}


RC ProtoDataUnit::DetectNextPdu(Bytes* buffer, int idx, int max_len) {
    for (; idx < max_len; ++idx) {
        if (buffer[idx] == 0xaa) {
            return idx;
        }
    }
    // 没有找到
    return EP_NO_HF;
}


// only max-len is neccary!
// curr_idx is also useless
bool ProtoDataUnit::operator==(const ProtoDataUnit & other) {

    int ret = 0x0;

    if (this->checksum_ != other.checksum_) {
        return false;
    }

    if (this->ext_ != other.ext_) {
        return false;
    }

    if (this->data_len_ != other.data_len_) {
        return false;
    }

    if (this->header_flag_ != other.header_flag_) {
        return false;
    }

    ret = memcmp(this->data_, other.data_, data_len_);
    if (ret) {
        return false;
    }

    return true;
}

void * CreatePdu(const char * data, int len) {
    bool ok;
    ProtoDataUnit* pdu = new ProtoDataUnit((const Bytes *)data, len, &ok);
    if (!ok) {
        delete pdu;
        return NULL;
    }
    return pdu;
}