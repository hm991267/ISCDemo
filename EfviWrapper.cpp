#include "EfviWrapper.hpp"
#include "etherfabric/capabilities.h"
#include "etherfabric/checksum.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <hammer/ipc/Ringbuffer.hpp>
#include <iostream>
#include <string>
#include <unordered_map>

#include <arpa/inet.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netdb.h>
#include <stddef.h>

std::unordered_map<int, std::string> eventName = {{EF_EVENT_TYPE_RX, "EF_EVENT_TYPE_RX"},
                                                  {EF_EVENT_TYPE_TX, "EF_EVENT_TYPE_TX"},
                                                  {EF_EVENT_TYPE_RX_DISCARD, "EF_EVENT_TYPE_RX_DISCARD"},
                                                  {EF_EVENT_TYPE_TX_ERROR, "EF_EVENT_TYPE_TX_ERROR"},
                                                  {EF_EVENT_TYPE_RX_NO_DESC_TRUNC, "EF_EVENT_TYPE_RX_NO_DESC_TRUNC"},
                                                  {EF_EVENT_TYPE_SW, "EF_EVENT_TYPE_SW"},
                                                  {EF_EVENT_TYPE_OFLOW, "EF_EVENT_TYPE_OFLOW"},
                                                  {EF_EVENT_TYPE_TX_WITH_TIMESTAMP, "EF_EVENT_TYPE_TX_WITH_TIMESTAMP"},
                                                  {EF_EVENT_TYPE_RX_PACKED_STREAM, "EF_EVENT_TYPE_RX_PACKED_STREAM"},
                                                  {EF_EVENT_TYPE_RX_MULTI, "EF_EVENT_TYPE_RX_MULTI"},
                                                  {EF_EVENT_TYPE_TX_ALT, "EF_EVENT_TYPE_TX_ALT"},
                                                  {EF_EVENT_TYPE_RX_MULTI_DISCARD, "EF_EVENT_TYPE_RX_MULTI_DISCARD"},
                                                  {EF_EVENT_TYPE_RESET, "EF_EVENT_TYPE_RESET"},
                                                  {EF_EVENT_TYPE_MEMCPY, "EF_EVENT_TYPE_MEMCPY"},
                                                  {EF_EVENT_TYPE_RX_MULTI_PKTS, "EF_EVENT_TYPE_RX_MULTI_PKTS"},
                                                  {EF_EVENT_TYPE_RX_REF, "EF_EVENT_TYPE_RX_REF"},
                                                  {EF_EVENT_TYPE_RX_REF_DISCARD, "EF_EVENT_TYPE_RX_REF_DISCARD"}};

int EfviPacketDesc::efviPrefixLen = 0;

static std::tuple<std::string, std::string, uint16_t> parseEndpoint(const std::string &endpoint)
{
    auto pos = endpoint.find('|');
    std::string interface = endpoint.substr(0, pos);
    auto pos1 = endpoint.find(':', pos);
    pos++;
    std::string addr = endpoint.substr(pos, pos1 - pos);
    std::string port = endpoint.substr(pos1 + 1);
    return {interface, addr, std::stoi(port)};
}

static std::string getIpFromInterface(const std::string &interface)
{
    struct ifaddrs *ifaddrs, *ifa;
    char *ipaddr = (char *)calloc(NI_MAXHOST, sizeof(char));
    if (getifaddrs(&ifaddrs) < 0)
    {
        return "";
    }
    for (ifa = ifaddrs; ifa != NULL; ifa = ifa->ifa_next)
    {
        if (ifa->ifa_addr == NULL)
            continue;
        if (strcmp(ifa->ifa_name, interface.c_str()) != 0)
            continue;
        if (ifa->ifa_addr->sa_family != AF_INET)
            continue;
        if (getnameinfo(ifa->ifa_addr, sizeof(struct sockaddr_in), ipaddr, NI_MAXHOST, NULL, 0, NI_NUMERICHOST) < 0)
        {
            return "";
        }
        break;
    }
    freeifaddrs(ifaddrs);
    return ipaddr;
}

bool EfviWrapper::init(const std::string &endpoint, size_t messageSize)
{
    std::string interface;
    std::string address;
    uint16_t port = 0;
    std::tie(interface, address, port) = parseEndpoint(endpoint);
    printf("%s %s %d\n", interface.c_str(), address.c_str(), port);

    if (interface.empty() || address.empty() || port == 0)
        return false;

    messageSize_ = messageSize;
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
    {
        printf("%s\n", "ef_driver_open failed");
        return false;
    }

    if (ef_pd_alloc_by_name(&pd_, dh_, interface.c_str(), EF_PD_DEFAULT) < 0)
    {
        printf("%s\n", "ef_pd_alloc_by_name failed");
        return false;
    }

    int vi_flags = EF_VI_FLAGS_DEFAULT;
    if ((rc = ef_vi_alloc_from_pd(&vi_, dh_, &pd_, dh_, -1, -1, -1, NULL, -1, (enum ef_vi_flags)vi_flags)) < 0)
    {
        printf("%s\n", "ef_vi_alloc_from_pd failed");
        return false;
    }

    std::string localAddr = getIpFromInterface(interface);
    printf("%s:%s\n", interface.c_str(), localAddr.c_str());
    if (localAddr.empty())
    {
        local_.sin_addr.s_addr = inet_addr("10.0.0.1");
        local_.sin_port = htons(10000);
        printf("bind default\n");
    }
    else
    {
        local_.sin_addr.s_addr = inet_addr(localAddr.c_str());
        local_.sin_port = htons(10000);
        printf("bind localAddr\n");
    }
    dest_.sin_addr.s_addr = inet_addr(address.c_str());
    dest_.sin_port = htons(port);
    initTxFrame(frame_);

    uint8_t mac[6];
    ef_vi_get_mac(&vi_, dh_, mac);
    printf("mac=%02x:%02x:%02x:%02x:%02x:%02x\n", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    // receive_prefix_len = ef_vi_receive_prefix_len(&vi);
    size_t dmaBufferSize = PAGE_SIZE * 16;

    posix_memalign((void **)&hostRecvMem_, PAGE_SIZE, dmaBufferSize);
    if (ef_memreg_alloc(&memreg_, dh_, &pd_, dh_, hostRecvMem_, dmaBufferSize) < 0)
    {
        printf("%s\n", "ef_memreg_alloc for recv failed");
        return false;
    }

    posix_memalign((void **)&hostSendMem_, PAGE_SIZE, dmaBufferSize);
    if (ef_memreg_alloc(&memregSend_, dh_, &pd_, dh_, hostSendMem_, dmaBufferSize) < 0)
    {
        printf("%s\n", "ef_memreg_alloc for send failed");
        return false;
    }
    EfviPacketDesc::efviPrefixLen = ef_vi_receive_prefix_len(&vi_);
    refillThresh_ = ef_vi_receive_capacity(&vi_) - EV_POLL_BATCH_SIZE;
    totalRecvPacketNumber_ = dmaBufferSize / EFVI_PACKET_SIZE;
    totalSendPacketNumber_ = dmaBufferSize / EFVI_PACKET_SIZE;

    for (int i = 0; i < totalRecvPacketNumber_; ++i)
    {
        struct EfviPacketDesc *pd = (struct EfviPacketDesc *)&hostRecvMem_[i * EFVI_PACKET_SIZE];
        pd->init(&memreg_, i);
        descs_[i] = pd;
        pd->next = freePd_;
        freePd_ = pd;
        freePdNum_++;
    }

    for (int i = 0; i < totalSendPacketNumber_; ++i)
    {
        struct EfviPacketDesc *pd = (struct EfviPacketDesc *)&hostSendMem_[i * EFVI_PACKET_SIZE];
        pd->init(&memregSend_, i);
        initTxFrame(*(EtherFrame *)pd->getPayload());
        sendPd_[i] = pd;
    }

    mtu_ = ef_vi_mtu(&vi_, dh_);
    maxPayloadLen_ = mtu_ - HEADER_LENGTH;

    ef_filter_spec filter;
    ef_filter_flags flag;
    flag = EF_FILTER_FLAG_NONE;
    ef_filter_spec_init(&filter, flag);
    if (ef_filter_spec_set_ip4_local(&filter, IPPROTO_UDP, dest_.sin_addr.s_addr, dest_.sin_port) < 0)
    {
        printf("%s\n", "filter ip failed");
    }
    // if (ef_filter_spec_set_multicast_all(&filter) < 0)
    // {
    //     printf("%s\n", "filter mc failed");
    // }
    // if (ef_filter_spec_set_port_sniff(&filter, 1) < 0)
    // {
    //     printf("%s\n", "promisc failed");
    // }
    if (ef_vi_filter_add(&vi_, dh_, &filter, nullptr) < 0)
    {
        printf("%s\n", "set filter failed");
    }

    refillRxRing();
}

bool EfviWrapper::initTxFrame(EtherFrame &frame)
{
    uint8_t mac[6];
    frame.eth_.ether_dhost[0] = 0x01;
    frame.eth_.ether_dhost[1] = 0x00;
    frame.eth_.ether_dhost[2] = 0x5e;
    frame.eth_.ether_dhost[3] = 0x00;
    frame.eth_.ether_dhost[4] = 0x00;
    frame.eth_.ether_dhost[5] = 0x51;
    frame.eth_.ether_dhost[0] = 0x01;
    frame.eth_.ether_dhost[1] = 0x02;
    frame.eth_.ether_dhost[2] = 0x03;
    frame.eth_.ether_dhost[3] = 0x04;
    frame.eth_.ether_dhost[4] = 0x05;
    frame.eth_.ether_dhost[5] = 0x06;
    ef_vi_get_mac(&vi_, dh_, frame.eth_.ether_shost);
    frame.eth_.ether_type = htons(ETH_P_IP);
    frame.ip_.ihl = 5;
    frame.ip_.version = 4;
    frame.ip_.id = 0;
    frame.ip_.tos = 0;
    frame.ip_.tot_len = 0;
    uint16_t frag = IP_DF;
    frame.ip_.frag_off = htons(frag);
    frame.ip_.protocol = IPPROTO_UDP;
    frame.ip_.ttl = 1;
    frame.ip_.daddr = dest_.sin_addr.s_addr;
    frame.ip_.saddr = local_.sin_addr.s_addr;
    frame.ip_.check = ef_ip_checksum(&frame.ip_);

    frame.udp_.dest = dest_.sin_port;
    frame.udp_.source = local_.sin_port;
    frame.udp_.len = 0;
    frame.udp_.check = 0;
    return true;
}

int EfviWrapper::transmit(const uint8_t *buf, size_t len)
{
    int offset = 0;
    int dmaId = 0;
    while (offset < len)
    {
        int bytes = std::min(maxPayloadLen_, (int)len - offset);
        bytes = bytes / messageSize_ * messageSize_;

        EtherFrame *frame = (EtherFrame *)sendPd_[dmaId]->getPayload();
        memcpy(frame->payload, buf + offset, bytes);
        
        frame->udp_.len = htons(bytes + sizeof(struct udphdr));
        frame->ip_.tot_len = htons(sizeof(struct ip) + sizeof(struct udphdr) + bytes);
        frame->ip_.check = ef_ip_checksum(&frame->ip_);
        struct iovec iov = {.iov_base = (void *)(buf + offset), .iov_len = bytes};
        frame->udp_.check = ef_udp_checksum(&frame->ip_, &frame->udp_, &iov, 1);
        // printf("====send====> %d\n", bytes);

        int ret;
        do
        {
            ret = ef_vi_transmit_init(&vi_, sendPd_[dmaId]->getDmaStartAddress(), bytes + HEADER_LENGTH,
                                      sendPd_[dmaId]->dmaId);    /* code */
            if (ret == -EAGAIN)
            {
                transmitDone();
                continue;
            }
            else if (ret < 0)
                return -1;
        } while (ret != 0);
        dmaId++;
        offset += bytes;
    }
    ef_vi_transmit_push(&vi_);
    transmitDone();
    return len / messageSize_;
}

void EfviWrapper::transmitDone(void)
{
    int n_ev, i, j, n_unbundled = 0;
    n_ev = ef_eventq_poll(&vi_, evs_, EV_POLL_BATCH_SIZE);
    if (n_ev > 0)
    {
        for (i = 0; i < n_ev; ++i)
        {
            handleEvent(evs_[i]);
        }
    }
}

void EfviWrapper::refillRxRing()
{
    if (ef_vi_receive_fill_level(&vi_) > refillThresh_ || freePdNum_ < EV_POLL_BATCH_SIZE)
        return;

    do
    {
        struct EfviPacketDesc *pd;
        for (int i = 0; i < EV_POLL_BATCH_SIZE; ++i)
        {
            pd = freePd_;
            freePd_ = pd->next;
            --freePdNum_;
            ef_vi_receive_init(&vi_, pd->getDmaStartAddress(), pd->dmaId);
        }
    } while (freePdNum_ >= EV_POLL_BATCH_SIZE);
    ef_vi_receive_push(&vi_);
}

void EfviWrapper::handleRx(ef_event &event)
{
    if (!(EF_EVENT_RX_SOP(event) && !EF_EVENT_RX_CONT(event)))
    {
        return;
    }
    int dmaId = EF_EVENT_RX_RQ_ID(event);
    int rxBytes = EF_EVENT_RX_BYTES(event) - EfviPacketDesc::efviPrefixLen;
    struct EfviPacketDesc *pd = descs_[dmaId];
    struct EtherFrame *frame = (struct EtherFrame *)pd->getPayload();
    // for (int i = 0; i < rxBytes; i++)
    // {
    //     printf("%02x ", pd->getPayload()[i]);
    //     if (i % 16 == 15)
    //         printf("\n");
    // }
    // printf("\n");

    if (frame->ip_.protocol != IPPROTO_UDP)
        return;
    if (frame->ip_.daddr != dest_.sin_addr.s_addr)
        return;
    if (frame->udp_.dest != dest_.sin_port)
        return;

    int segLen = htons(frame->udp_.len) - sizeof(struct udphdr);
    if (outputOffset + segLen <= outputLen)
    {
        memcpy(outputPtr + outputOffset, frame->payload, segLen);
        outputOffset += segLen;
    }
    else
    {
        recvBufferOverflow_++;
    }

    pd->next = freePd_;
    freePd_ = pd;
    ++freePdNum_;
}

void EfviWrapper::handleEvent(ef_event &event)
{
    // printf("recv event:%s, len:%d\n", eventName[EF_EVENT_TYPE(event)].c_str(), EF_EVENT_RX_BYTES(event));
    switch (EF_EVENT_TYPE(event))
    {
    case EF_EVENT_TYPE_RX: /** Good data was received. */
        handleRx(event);
        break;
    case EF_EVENT_TYPE_TX: /** Packets have been sent. */
    {
        ef_request_id ids[EF_VI_TRANSMIT_BATCH];
        ef_vi_transmit_unbundle(&vi_, &event, ids);
        break;
    }
    case EF_EVENT_TYPE_RX_DISCARD: /** Data received and buffer consumed, but something is wrong. */
        break;
    case EF_EVENT_TYPE_TX_ERROR: /** Transmit of packet failed. */
        break;
    case EF_EVENT_TYPE_RX_NO_DESC_TRUNC: /** Received packet was truncated due to a lack of descriptors. */
        break;
    case EF_EVENT_TYPE_SW: /** Software generated event. */
        break;
    case EF_EVENT_TYPE_OFLOW: /** Event queue overflow. */
        break;
    case EF_EVENT_TYPE_TX_WITH_TIMESTAMP: /** TX timestamp event. */
        break;
    case EF_EVENT_TYPE_RX_PACKED_STREAM: /** A batch of packets was received in a packed stream. */
        break;
    case EF_EVENT_TYPE_RX_MULTI: /** A batch of packets was received on a RX event merge vi. */
        break;
    case EF_EVENT_TYPE_TX_ALT: /** Packet has been transmitted via a "TX alternative". */
        break;
    case EF_EVENT_TYPE_RX_MULTI_DISCARD: /** A batch of packets was received with error condition set. */
        break;
    case EF_EVENT_TYPE_RESET: /** Event queue has been forcibly halted (hotplug, reset, etc.) */
        break;
    case EF_EVENT_TYPE_MEMCPY: /** A ef_vi_transmit_memcpy_sync() request has completed. */
        break;
    case EF_EVENT_TYPE_RX_MULTI_PKTS: /** A batch of packets was received. */
        break;
    case EF_EVENT_TYPE_RX_REF: /** Good packets have been received on an efct adapter */
        break;
    case EF_EVENT_TYPE_RX_REF_DISCARD: /** Packets with a bad checksum have been received on an efct adapter */
        break;
    }
}

int EfviWrapper::receive(uint8_t *buf, size_t len)
{
    outputPtr = buf;
    outputLen = len;
    outputOffset = 0;
    refillRxRing();
    int n = ef_eventq_poll(&vi_, evs_, EV_POLL_BATCH_SIZE);
    for (int i = 0; i < n; i++)
    {
        handleEvent(evs_[i]);
    }
    outputPtr = nullptr;
    outputLen = 0;
    return outputOffset / messageSize_;
}

void EfviWrapper::poll(std::function<void(uint8_t *)> &&callback)
{
    refillRxRing();
    int n = ef_eventq_poll(&vi_, evs_, EV_POLL_BATCH_SIZE);
    for (int i = 0; i < n; ++i)
    {
        handleEvent(evs_[i]);
    }
}

EfviWrapper::~EfviWrapper()
{
    ef_memreg_free(&memreg_, dh_);
    ef_memreg_free(&memregSend_, dh_);

    free(hostRecvMem_);
    free(hostSendMem_);
   
    ef_vi_free(&vi_, dh_);
    ef_pd_free(&pd_, dh_);
    ef_driver_close(dh_);
}
