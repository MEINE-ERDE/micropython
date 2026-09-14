#ifndef MICROPY_INCLUDED_STM32_LWIP_LWIPOPTS_H
#define MICROPY_INCLUDED_STM32_LWIP_LWIPOPTS_H

#include STM32_HAL_H

#define LWIP_NETIF_EXT_STATUS_CALLBACK  1

#define LWIP_LOOPIF_MULTICAST           1
#define LWIP_LOOPBACK_MAX_PBUFS         8

#define LWIP_IPV6                       0

#define LWIP_RAND() rng_get()

// Increase memory for lwIP to get better performance.
#if defined(STM32N6)
#define MEM_SIZE                        (64 * 1024)
#define PBUF_POOL_SIZE                  (32)
#define TCP_MSS                         (1460)
#define TCP_WND                         (16 * TCP_MSS)
#define TCP_SND_BUF                     (16 * TCP_MSS)
#define MEMP_NUM_TCP_SEG                (64)
#endif

// H7 boards: the common tier below hands every socket the same 8 KB heap
// and 32-segment pool, so one peer that stops reading (a WebREPL client)
// fills its send queue and leaves MQTT, FTP and the log upload with
// nothing. Give the heap room and size queued pbufs to the write: with
// TCP_OVERSIZE every queued pbuf reserved a full MSS, so 32 queued 100-byte
// lines held 26 KB; without it they hold about 4 KB of the 32 KB heap.
// TCP_SND_QUEUELEN stays at its derived 32: 16 (lwIP's minimum here) made
// a healthy WebREPL echo block the loop into the watchdog. MSS stays at
// 800: the full-size 1460 tier stalled FTP transfers from a host behind a
// 1300-byte MTU, and lwIP does no path MTU discovery.
#if defined(STM32H7)
#define MEM_SIZE                        (32 * 1024)
#define TCP_MSS                         (800)
#define TCP_WND                         (8 * TCP_MSS)
#define TCP_SND_BUF                     (8 * TCP_MSS)
#define MEMP_NUM_TCP_SEG                (64)
#define TCP_OVERSIZE                    (0)
#endif

// Include common lwIP configuration.
#include "extmod/lwip-include/lwipopts_common.h"

extern uint32_t rng_get(void);

#endif // MICROPY_INCLUDED_STM32_LWIP_LWIPOPTS_H
