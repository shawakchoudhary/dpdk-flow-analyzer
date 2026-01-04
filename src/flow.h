#ifndef FLOW_H
#define FLOW_H

#include <stdint.h>

struct flow6_key {
    uint8_t  src_ip[16];
    uint8_t  dst_ip[16];
    uint16_t src_port;
    uint16_t dst_port;
    uint8_t  proto;
} __attribute__((packed));

struct flow_stats {
    struct flow6_key key;
    uint64_t packets;
    uint64_t bytes;
    uint64_t last_seen_tsc;
    uint8_t valid;
};

#endif

