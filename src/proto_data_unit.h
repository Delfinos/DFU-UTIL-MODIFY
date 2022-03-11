/**
* @file proto_data_unit.h
* @author chenfeiyang (chenfeiyang@cambricon.com)
* @brief 检包算法实现
* @version 1.2
* @date 2020-12-01
* @copyright Copyright(c) 2020 Cambricon Inc.
*/



#ifndef PROTO_DATA_UNIT_H_  // NOLINT
#define PROTO_DATA_UNIT_H_ 1
typedef int RC;
typedef unsigned int UInt32;
typedef unsigned long long UInt64;  // NOLINT
typedef int Int32;
typedef long long Int64;  // NOLINT
typedef unsigned char Bytes;
typedef unsigned short UInt16; // NOLINT
typedef unsigned char UInt8;
enum PDU_RC {
    RC_SUCCESS = 0,
    EP_NO_HF = -1,
    EP_HEAD_FLAG = -2,
    EP_HEAD_CRC = -3, 
    EP_LOGIC_ERROR = -4,
    EP_DST_LEN = -5,
};
#ifdef __cplusplus 

// #include <memory>
// #include <atomic>
// for debug
// #include "foundation_exp.h"  // NOLINT
// #define MAX_PDU_SIZE 65536


template<typename T>
class ArrDeleter{
 public:
    void operator() (T * ptr) const{
        delete[] ptr;
    }
};


/**
 * @brief pdu 对象智能指针
 */
//  typedef std::shared_ptr<ProtoDataUnit> pdu_ptr;

class ProtoDataUnit{
 public:
     enum {
         MAX_PDU_SIZE = 65536,
         // FUCK THIS BUG
         HEADER_LENGTH = 8,
    };

    /*!
        * @brief 通过数据和数据长度直接构造pdu
        * 
        * \param data 数据指针
        * \param len 数据区长度
        * \param ok 构建是否成功
        */
    ProtoDataUnit(const Bytes* data, UInt32 len, bool* ok = nullptr);
    /*!
        * @brief 构造含有内存数据区的对象
        * 
        * \param len 数据区长度
        * \param ok
    */
    explicit ProtoDataUnit(UInt32 len, bool *ok = nullptr);
    /**
     * @brief 用于构造一个默认状态的pdu,通常用于recv场景
     */
    ProtoDataUnit();
    /**
     * @brief 获取PDU的二进制长度
     * 
     * @return UInt32  二进制长度，0 或负值表明此PDU无效，正值表示二进制长度
     */
    UInt32 BinaryLen();

    virtual ~ProtoDataUnit();

    /**
     * @brief 在指定内存区域序列化PDU
     * 
     * @param dst_mem 目标区域的起始地址
     * @param dst_len 目标区域的最大长度
     * @return true 成功
     * @return false 失败
     */
    bool SerializeToBin(Bytes* dst_mem, UInt32 dst_len);
    /*!
     * @brief 在指定内存区域序列化PDU的头部
     * 
     * \param dst_mem 目标区域的起始地址
     * \param dst_len 目标区域的最大长度
     * \return true 成功 false 失败
     */
    bool SerializeHeaderToBin(Bytes* dst_mem, UInt32 dst_len);
    /**
     * @brief 重置PDU
     * 重置后，此PDU拥有的数据将会被释放
     */
    void Reset();
    /**
     * @brief 找到一个packet的开始位置
     * 
     * @param buffer 内存区域
     * @param idx 当前索引
     * @param max_len 内存区域的最大长度
     * @return int 找到的索引位置 -1 表示没有找到
     */
    static RC DetectNextPdu(Bytes* buffer, int idx, int max_len);
    /**
     * @brief 泛型：从一片内存中转换出某些数据
     * 
     * @tparam T 数据类型
     * @param buffer 源内存区域
     * @param max_len 源内存区域最大长度
     * @param idx 当前索引
     * @param value 传出参数，转换的数值
     * @return int -1 超出最大长度，转换失败 >0 表示处理的类型长度。可用于外部做步进长度
     */
    template <typename T>
    static int TConvertBufferToNumeric(const Bytes* buffer,  int max_len, int idx, T& value) { // NOLINT
        int data_size = sizeof(T);
        if (idx + sizeof(T) > max_len) {
            return EP_SRC_LEN;
        }
        // arm may be different with intel
        for (Int32 index = 0; index < data_size; ++index) {
            value <<= 8;
            value |= buffer[idx + index];
        }
        return sizeof(T);
    }
    /**
     * @brief 将整形数据序列化到一片内存中
     * 
     * @tparam T 数据类型
     * @param value 值
     * @param max_len 目标内存区域的最大长度
     * @param idx 当前索引
     * @param buffer 目标内存区域
     * @return int -1 超过最大长度，正值表示处理的类型长度
     */
    template <typename T>
    static int TSerializeNumericToBuffer(const T& value, int max_len, int idx, Bytes* buffer) { // NOLINT
        Int32 data_size = sizeof(T);
        if (idx + data_size > max_len) {
            return EP_DST_LEN;
        }

        for (Int32 i = 0; i < data_size; ++i) {
            buffer[idx + i] = (value >> (8 * (data_size - 1 - i))) & 0xff;
        }

        return sizeof(T);
    }

    /**
     * @brief 判断两个PDU是否相等
     * 
     * @param other 其他pdu对象
     * @return true 相同
     * @return false 不相同
     */
    bool operator == (const ProtoDataUnit& other);

 private:
    /**
     * @brief 初始化PDU
     * 
     * @param bts 源数据起始地址
     * @param len 源数据最大长度
     * @return true 构建成功
     * @return false 构建失败
     */
    bool Init(const Bytes* bts, Int32 len);

 public:
    UInt8    header_flag_;
    UInt8   checksum_;
    // 更新 UInt32 表示大数据包
    UInt32  data_len_;
    UInt16  ext_;
    // 应该使用unique_ptr
    Bytes*  data_ = nullptr;
    /**
     * @brief 辅助变量，不会在pdu序列化的二进制内存中体现
     * 
     */
    UInt8  recv_flag_ = 0x00;
    // update to 32bit
    UInt32 stored_pos_ = 0x0;
    // for debug
    //int _id;
    //static std::atomic<int> _cnt;
};

#endif

#ifdef __cplusplus
extern "C" {
#endif
    void * CreatePdu(const char * data, int len);
#ifdef __cplusplus
}
#endif

#endif  // PROTO_DATA_UNIT_H_  NOLINT
