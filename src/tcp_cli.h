#ifndef TCP_CLI_H_
#define TCP_CLI_H_ 1
#include <stdint.h>
#ifdef __cplusplus
#if (defined _WIN32 && defined _OFF_T_DEFINED)
typedef off_t _off_t;
#endif
#include <memory>
class ProtoDataUnit;
class TcpCli {
public:
    TcpCli();
    ~TcpCli();
    // 发送数据, Recv暂时不用
    void Send(const char* data, int len);
    // 停止client, 基本已不可用
    void ShutDown();
    // 是否可用
    unsigned char Valid();
    void Init(unsigned short port);
    bool AsyncSend(ProtoDataUnit* pdu);
    struct Private;
protected:
    static void AysncInit(void *arg);
private:
    std::unique_ptr<Private> d_ = nullptr;
};
#endif
#ifdef __cplusplus
extern "C" {
#endif
    enum TCP_ERROR_T {
        SERVER_INVALID = -1,
    };

    enum PacketType {
        PT_PROGRESS = 1,
        PT_ERROR = 2,
        PT_FATAL = 3,
    };
    struct Packet {
        uint32_t packet_type_;
        // no need
        // uint32_t packet_data_len_;
        // char* data_;
        uint32_t filename_len_;
        uint32_t total_len_;
        char* data_;
    };
    void MakePacket(uint32_t packet_type, const char* file_name, const char * data, uint32_t len, struct Packet* out);
    void ReleasePacketData(struct Packet* out);
    void *CreateClient(unsigned short port);
    void DestroyClient(void *cli);
    int SendToSvr(void* cli_hnd, struct Packet* packet);
    int SendProgress(void* cli_hnd, const char* file_name, int progress);
    extern void* tcp_cli_hnd;
#define MAX_FILENAME_LEN  1024
    extern char transfer_file_name[MAX_FILENAME_LEN];
    // TBD: FIX... 
#define PRINT_BUFFER_LEN 4096
#define NetSend(hnd, type, fmt, ...) {\
    char aabbccddeeff[PRINT_BUFFER_LEN];\
    memset(aabbccddeeff, 0x0, PRINT_BUFFER_LEN);\
    sprintf(aabbccddeeff, fmt, ##__VA_ARGS__);\
    int len = (int)strlen(aabbccddeeff);\
    struct Packet pack;\
    MakePacket(type, transfer_file_name, aabbccddeeff, len, &pack);\
    int ret = SendToSvr(hnd, &pack);\
    ReleasePacketData(&pack);\
    ret;\
    }
#define NetPerr(hnd, fmt, ...) NetSend(hnd, PT_ERROR, fmt, ##__VA_ARGS__)
#define NetFatal(hnd, fmt, ...) NetSend(hnd, PT_FATAL, fmt, ##__VA_ARGS__)
#ifdef __cplusplus
}
#endif
#endif