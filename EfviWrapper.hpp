#pragma once

#include <cstdint>
#include <string>
#include <functional>

#include <arpa/inet.h>
#include <netinet/ether.h>
#include <netinet/ip.h>
#include <netinet/udp.h>

#include "etherfabric/ef_vi.h"
#include "etherfabric/memreg.h"
#include "etherfabric/pd.h"
#include "etherfabric/vi.h"



#define CACHE_ALIGN                    __attribute__((aligned(EF_VI_DMA_ALIGN)))
#define MEMBER_OFFSET(c_type, mbr_name) ((uint32_t)(uintptr_t)(&((c_type *)0)->mbr_name))
#define ROUND_UP(p, align)              (((p)+(align)-1u) & ~((align)-1u))

constexpr int PAGE_SIZE = 4096;
constexpr int EFVI_PACKET_SIZE  = 2048;
constexpr int HEADER_LENGTH = sizeof(struct ether_header) + sizeof(struct ip) + sizeof(struct udphdr);
constexpr int EFVI_PAYLOAD_SIZE = EFVI_PACKET_SIZE - HEADER_LENGTH;
constexpr int EV_POLL_BATCH_SIZE = 16;

#pragma pack(push, 1)

struct EtherFrame
{
    struct ether_header eth_;
    struct iphdr ip_;
    struct udphdr udp_;
    uint8_t payload[EFVI_PAYLOAD_SIZE];
};

struct EfviPacketDesc
{
    struct EfviPacketDesc *next;
    ef_addr dmaAddr;
    int dmaId;
    uint8_t *dmaPtr;

    void init(ef_memreg* memreg, int id)
    {
        next = nullptr;
        dmaId = id;
        dmaAddr = ef_memreg_dma_addr(memreg, id * EFVI_PACKET_SIZE);
        dmaPtr = (uint8_t *)this + ROUND_UP(sizeof(EfviPacketDesc), EF_VI_DMA_ALIGN);
    }

    ef_addr getDmaStartAddress()
    {
        return dmaAddr + ROUND_UP(sizeof(EfviPacketDesc), EF_VI_DMA_ALIGN);
    }

    uint8_t *getPayload() { return dmaPtr + efviPrefixLen; }

    static int efviPrefixLen;
};

#pragma pack(pop)

static_assert(sizeof(EtherFrame) == EFVI_PACKET_SIZE, "EtherFrame size not meet ef_vi requirement");

class EfviWrapper
{
public:
    bool init(const std::string &endpint, size_t messageLen);
    int transmit(const uint8_t *buf, size_t len);
    int receive(uint8_t *buf, size_t len);
    void poll(std::function<void(uint8_t*)>&& callback);
    ~EfviWrapper();

private:
    bool initTxFrame(EtherFrame& frame);
    void handleRx(ef_event &event);
    void handleEvent(ef_event &event);
    void refillRxRing();
    void transmitDone();

    int messageSize_{0};

    struct sockaddr_in local_;
    struct sockaddr_in dest_;

    EfviPacketDesc *descs_[1024];
    EfviPacketDesc *sendPd_[1024];
    int totalSendPacketNumber_{0};
    int totalRecvPacketNumber_{0};
    uint8_t *hostSendMem_;
    uint8_t *hostRecvMem_;
    ef_vi vi_{};
    ef_driver_handle dh_{-1};
    ef_pd pd_{};
    ef_memreg memreg_{};
    ef_memreg memregSend_{};
    ef_filter_cookie filter_cookie_{};

    ef_event evs_[EV_POLL_BATCH_SIZE];
    ef_request_id ids_[EF_VI_RECEIVE_BATCH];
    int efEventIndex_{0};
    int efEventSize_{0};
    EfviPacketDesc *freePd_{nullptr};
    int freePdNum_{0};
    int refillThresh_{0};

    int mtu_;
    int maxPayloadLen_;
    EtherFrame frame_;
    uint8_t *outputPtr{nullptr};
    size_t outputOffset{0};
    size_t outputLen{0};
    
    // statistic
    int recvBufferOverflow_{0};
};