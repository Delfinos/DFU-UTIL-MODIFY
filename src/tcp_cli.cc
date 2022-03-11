#include "tcp_cli.h"
#include <stdlib.h>
#include <errno.h>
#include <fiber/libfiber.h>
#include "patch.h"

#include <atomic>
#include <stdint.h>
#include <thread>
#include <mutex>
#include <vector>
#include <queue>
#include <condition_variable>

#include "proto_data_unit.h"

constexpr int MAX_QUEUE_SIZE = 100;

struct TcpCli::Private {
    SOCKET fd_;
    std::atomic<bool> valid_;
    uint16_t port_;
    std::thread thrd_;
    ACL_FIBER_EVENT* ev_ = NULL; 
    std::mutex queue_mu_;
    // empty is no-need.
    std::condition_variable queue_cv_;
    // std::vector<ProtoDataUnit*> data_queue_;
    std::queue<ProtoDataUnit*> data_queue_;
    std::atomic<uint32_t> packet_count_;
    std::atomic<bool> quit_ = false;
    // TBD:析构函数
    Private() {
        valid_ = false;
        packet_count_ = 0;
        fd_ = INVALID_SOCKET;
        ev_ = acl_fiber_event_create(FIBER_FLAG_USE_MUTEX);
    }

    ~Private() {
        acl_fiber_event_free(ev_);
        ev_ = NULL;
    }
};

TcpCli::TcpCli() {
    d_ = std::make_unique<TcpCli::Private>();
}

TcpCli::~TcpCli() {
    ShutDown();
}

#if 0
void TcpCli::Send(const char * data, int len) {
}
#endif

void TcpCli::ShutDown() {
    // 其实不是特别安全
    if (INVALID_SOCKET != d_->fd_) {
        shutdown(d_->fd_, SD_BOTH);
    }
    {
        std::unique_lock<std::mutex> lg(d_->queue_mu_);
        d_->quit_ = true;
        d_->queue_cv_.notify_all();
    }
    if (d_->thrd_.joinable()) {
        d_->thrd_.join();
    }
    printf("Client exited.\n");
}

unsigned char TcpCli::Valid() {
    return d_->valid_;
}

void TcpCli::Init(unsigned short port) {
    d_->port_ = port;
    socket_init();
    acl_fiber_msg_stdout_enable(1);
    d_->thrd_ = std::thread(TcpCli::AysncInit, this);
    printf("Client thread started");
    std::unique_lock<std::mutex> lg(d_->queue_mu_);
    d_->queue_cv_.wait(lg, [this]() {
        bool ret = d_->valid_;
        return ret;
    });
    if (!d_->valid_) {
        fprintf(stderr, "Create tcp client error!\n");
    }
}

bool TcpCli::AsyncSend(ProtoDataUnit * pdu) {
    std::unique_lock<std::mutex> lg(d_->queue_mu_);
    d_->queue_cv_.wait(lg, [this]() {
        return (d_->packet_count_ < MAX_QUEUE_SIZE);
    });
    if (!d_->valid_) {
        return false;
    }
    d_->data_queue_.push(pdu);
    d_->packet_count_ += 1;
    // acl_fiber_event_notify(d_->ev_);
    d_->queue_cv_.notify_one();
    return true;
}

static void FiberConnectAndLoop(ACL_FIBER *fiber, void *ctx) {
    TcpCli::Private* prv_ = reinterpret_cast<TcpCli::Private*>(ctx);
    prv_->fd_ = socket_connect("127.0.0.1", prv_->port_);
    if (INVALID_SOCKET == prv_->fd_) {
        fprintf(stderr, "connect to 127.0.0.1:%lld error, info:%s\n", prv_->fd_, strerror(errno));
        prv_->valid_ = false;
       
    } else {
        printf("Connect to 127.0.0.1:%lld success\n", prv_->fd_);
        prv_->valid_ = true;
    }
    // 唤醒,通知TCP CLI 已准备好
    {
        std::unique_lock<std::mutex> lg(prv_->queue_mu_);
        prv_->queue_cv_.notify_one();
        if (!prv_->valid_) {
            return;
        }
    }
    printf("Ready to send");
    for (;;) {
#if 0
        if (acl_fiber_event_wait(prv_->ev_) == -1) {
            fprintf(stderr, "Event wait error!\n");
            break;
        }

#endif
        
        // 要考虑效率问题
        // 此函数内频繁加锁去锁
        ProtoDataUnit* pdu = nullptr;
        {
            std::unique_lock<std::mutex> lg(prv_->queue_mu_);
            prv_->queue_cv_.wait(lg, [prv_]() {
                return (prv_->data_queue_.size() > 0 || prv_->quit_);
                });

            if (prv_->quit_) {
                printf("Client quit!\n");
                break;
            }
            pdu = prv_->data_queue_.front();
            prv_->data_queue_.pop();
            prv_->packet_count_ -= 1;
            // notify...
            prv_->queue_cv_.notify_one();
        }
        // 在同一线程中如果对同一个mutex 尝试锁，会抛出异常
        // std::unique_lock<std::mutex> lg(prv_->queue_mu_);
        // TBD:优化...
        char * data = new char[pdu->BinaryLen()];
        pdu->SerializeToBin((Bytes*)data, pdu->BinaryLen());
        int ret = acl_fiber_send(prv_->fd_, data, pdu->BinaryLen(), 0);
        delete[] data;
        
        if (ret <= 0) {
            fprintf(stderr, "Socket send error!\n");
            break;
        }
    }
    acl_fiber_close(prv_->fd_);
    prv_->fd_ = INVALID_SOCKET;
    prv_->valid_ = false;
    // 唤醒等待的接口，并且通知其退出
    std::unique_lock<std::mutex> lg2(prv_->queue_mu_);
#if 0
    prv_->data_queue_.swap(std::queue<ProtoDataUnit*>());
#else 
    for (int idx = prv_->data_queue_.size(); idx > 0; --idx) {
        auto pdu = prv_->data_queue_.front();
        if (pdu) {
            delete pdu;
        }
        prv_->data_queue_.pop();
    }
#endif
    prv_->packet_count_ = 0;
    // 唤醒全部，通知其退出.
    prv_->queue_cv_.notify_all();
}

void TcpCli::AysncInit(void * arg) {
    TcpCli* this_ = reinterpret_cast<TcpCli*>(arg);
    std::string ip_ = "127.0.0.1";
    int ev_mode = FIBER_EVENT_KERNEL;
    socket_init();
    acl_fiber_msg_stdout_enable(1);
    TcpCli::Private* d_ = this_->d_.get();
    acl_fiber_create(FiberConnectAndLoop, d_, 1 * 1024 * 1024);
    acl_fiber_schedule_with(FIBER_EVENT_KERNEL);
    socket_end();
}
// C Part
void* tcp_cli_hnd = NULL;
char transfer_file_name[MAX_FILENAME_LEN] = { 0 };
void MakePacket(uint32_t packet_type, const char* file_name, const char * data, uint32_t len, struct Packet* out) {
    int total_len = len + strlen(file_name);
    out->data_ = new char[total_len];
    memcpy(out->data_, file_name, strlen(file_name));
    memcpy(out->data_ + strlen(file_name), data, len);
    out->filename_len_ = strlen(file_name);
    out->packet_type_ = packet_type;
    out->total_len_ = total_len;
}

void ReleasePacketData(Packet * out) {
    delete[] out->data_;
}



void * CreateClient(unsigned short port) {
    TcpCli* cli = new TcpCli;
    cli->Init(port);
    if (cli->Valid()) {
        return cli;
    }
    delete cli;
    return NULL;
}

void DestroyClient(void * cli) {
    if (!cli) {
        return;
    }
    TcpCli* tcli = reinterpret_cast<TcpCli*>(cli);
    tcli->ShutDown();
    delete tcli;
    if (tcli == tcp_cli_hnd) {
        tcp_cli_hnd = NULL;
    }
}

int SendToSvr(void * cli_hnd, struct Packet* packet) {
    TcpCli* cli = reinterpret_cast<TcpCli*>(cli_hnd);
    if (!cli) {
        return -1;
    }
    uint32_t packet_total_len = sizeof(uint32_t) * 2 + packet->total_len_;
    ProtoDataUnit* pdu = new ProtoDataUnit(packet_total_len);
#if 0
    uint32_t offset = ProtoDataUnit::HEADER_LENGTH;
#endif
    uint32_t offset = 0;
    memcpy(pdu->data_ + offset, &packet->packet_type_, sizeof(uint32_t));
    offset += sizeof(uint32_t);
    memcpy(pdu->data_ + offset, &packet->filename_len_, sizeof(uint32_t));
    offset += sizeof(uint32_t);
    memcpy(pdu->data_ + offset, packet->data_, packet->total_len_);
    bool ret = cli->AsyncSend(pdu);
    if (!ret) {
        return SERVER_INVALID;
    }
    return 0;
}

int SendProgress(void* cli_hnd, const char* file_name, int progress) {
    if (!cli_hnd) {
        return -1;
    }
    TcpCli* hnd = reinterpret_cast<TcpCli*>(cli_hnd);
    Packet pack;
    pack.packet_type_ = PT_PROGRESS;
    pack.filename_len_ = strlen(file_name);
    pack.total_len_ = pack.filename_len_ + sizeof(int);
    pack.data_ = new char[pack.total_len_];
    // pack.data_ = (char*) &progress;
    memcpy(pack.data_, file_name, strlen(file_name));
    memcpy(pack.data_ + strlen(file_name), &progress, sizeof(int));
    int ret = SendToSvr(hnd, &pack);
    ReleasePacketData(&pack);
    return ret;
}

