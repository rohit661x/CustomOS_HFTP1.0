#pragma once
#include <stdint.h>

// Parsed message structure — matches kernel's hw_msg_small
typedef struct {
    char     msg_type;      // 1 byte  ('A' = Add Order, 'R' = Reset, 'X' = noise)
    uint16_t stock_locate;  // 2 bytes (little-endian symbol ID, e.g. AAPL_LOCATE = 0x0042)
    uint32_t price;         // 4 bytes (little-endian, price in ticks)
    uint32_t shares;        // 4 bytes (little-endian, share quantity)
} hw_msg_small;

// Parse a raw MoldUDP64 packet into hw_msg_small.
// Returns 0 on success, -1 if too short, -2 if payload length unexpected.
int moldudp64_parser_c(uint8_t *pkt_base, uint32_t pkt_len, hw_msg_small *out_msg);
