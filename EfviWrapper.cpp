#include <cstdlib>
#include <cstring>
#include "etherfabric/capabilities.h"
#include "EfviWrapper.hpp"

#include <hammer/ipc/Ringbuffer.hpp>


bool EfviWrapper::init()
{
    const char *interface = "";
    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    int rc;
    // struct ifreq ifr;
    // ifr.ifr_addr.sa_family = AF_INET;
    // strcpy(ifr.ifr_name, interface);
    // int rc = ioctl(fd, SIOCGIFADDR, &ifr);
    // ::close(fd);
    // if (rc != 0) return "ioctl SIOCGIFADDR failed";
    // local_ip = ((struct sockaddr_in*)&ifr.ifr_addr)->sin_addr.s_addr;

    if (ef_driver_open(&dh_) < 0)
        return "ef_driver_open failed";

    if (ef_pd_alloc_by_name(&pd_, dh_, interface, EF_PD_DEFAULT) < 0)
        return "ef_pd_alloc_by_name failed";

    int vi_flags = EF_VI_FLAGS_DEFAULT;
    // int ifindex = if_nametoindex(interface);
    int ifindex = 0;
    unsigned long capability_val = 0;

    if (ef_vi_capabilities_get(dh_, ifindex, EF_VI_CAP_CTPIO, &capability_val) == 0 && capability_val) {
        
    }

    if ((rc = ef_vi_alloc_from_pd(&vi_, dh_, &pd_, dh_, -1, 4096, 4096, NULL, -1,
                                    (enum ef_vi_flags)vi_flags)) < 0)
        return "ef_vi_alloc_from_pd failed";

    // ef_vi_get_mac(&vi, dh, local_mac);
    // receive_prefix_len = ef_vi_receive_prefix_len(&vi);

    posix_memalign((void**)&hostRecvMem_, PAGE_SIZE, PAGE_SIZE * 16);
    if (ef_memreg_alloc(&memreg_, dh_, &pd_, dh_, hostRecvMem_, PAGE_SIZE * 16) < 0)
        return "ef_memreg_alloc failed";

    posix_memalign((void**)&hostSendMem_, PAGE_SIZE, PAGE_SIZE * 16);
    if (ef_memreg_alloc(&memreg_, dh_, &pd_, dh_, hostRecvMem_, PAGE_SIZE * 16) < 0)
        return "ef_memreg_alloc failed";


    for(int i = 0; i < 1024; ++i ) {
        struct EfviPacketDesc* pb = (struct EfviPacketDesc*) &hostRecvMem_[i * EFVI_PACKET_SIZE];
        pb->id = i;
        pb->dma_buf_addr = ef_memreg_dma_addr(&memreg_, i * EFVI_PACKET_SIZE);
        pb->dma_buf_addr += MEMBER_OFFSET(struct EfviPacketDesc, dma_buf);
        descs_[i] = pb;
        if (ef_vi_receive_post(&vi_, pb->dma_buf_addr, pb->id) < 0)
            return "ef_vi_receive_post failed";
    }

    for(int i = 0; i < 1024; ++i ) {
        struct EfviPacketDesc* pb = (struct EfviPacketDesc*) &hostRecvMem_[i * EFVI_PACKET_SIZE];
        pb->id = i;
        pb->dma_buf_addr = ef_memreg_dma_addr(&memreg_, i * EFVI_PACKET_SIZE);
        pb->dma_buf_addr += MEMBER_OFFSET(struct EfviPacketDesc, dma_buf);
        descs_[i] = pb;
        if (ef_vi_receive_post(&vi_, pb->dma_buf_addr, pb->id) < 0)
            return "ef_vi_receive_post failed";
    }
}

int EfviWrapper::transmit(const uint8_t *buf, size_t len)
{
    int i = 0;
    memcpy(descs_[i]->dma_buf, buf, len);
    if (ef_vi_transmit(&vi_, descs_[i]->dma_buf_addr, len, descs_[i]->id) < 0)
        return -1;
    else
        return 1;
}
