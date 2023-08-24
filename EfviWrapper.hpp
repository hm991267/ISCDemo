#pragma once

#include <cstdint>

#include <arpa/inet.h>
#include <netinet/ether.h>
#include <netinet/ip.h>
#include <netinet/udp.h>


#include "etherfabric/ef_vi.h"
#include "etherfabric/pd.h"
#include "etherfabric/vi.h"
#include "etherfabric/memreg.h"

#define PAGE_SIZE           4096
#define EFVI_PACKET_SIZE    2048
#define EFVI_PAYLOAD_SIZE   (EFVI_PACKET_SIZE - sizeof(struct ether_header) - sizeof(struct ip) - sizeof(struct udphdr))
#define MEMBER_OFFSET(c_type, mbr_name) \
                     ((uint32_t) (uintptr_t)(&((c_type*)0)->mbr_name))
#define CACHE_ALIGN  __attribute__((aligned(EF_VI_DMA_ALIGN)))


#pragma pack(push, 1)

struct EtherFrame
{
    struct ether_header eth_;
    struct ip       ip_;
    struct udphdr   udp_;
    uint8_t payload[EFVI_PAYLOAD_SIZE];
};

struct EfviPacketDesc {
  struct EfviPacketDesc* next;
  ef_addr         dma_buf_addr;
  int             id;
  uint8_t         dma_buf[1] CACHE_ALIGN;
};

#pragma pack(pop)

static_assert(sizeof(EtherFrame) == EFVI_PACKET_SIZE, "EtherFrame size not meet ef_vi requirement");

class EfviWrapper
{
public:
    bool init();
    int transmit(const uint8_t *buf, size_t len);
    int transmitBatch(const uint8_t *buf, size_t len);

private:
    struct EfviPacketDesc* descs_[1024];
    uint8_t *hostSendMem_;
    uint8_t *hostRecvMem_;
    ef_vi vi_{};
    ef_driver_handle dh_{-1};
    ef_pd pd_{};
    ef_memreg memreg_{};
    ef_filter_cookie filter_cookie_{};
};