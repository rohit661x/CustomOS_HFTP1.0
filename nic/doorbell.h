
// nic/doorbell.h (Slice 11)
#pragma once
#include <stdint.h>

// Abstract doorbell write; UC or WC mapping decided at init
void* map_tx_doorbell_uc(void);
void* map_tx_doorbell_wc(void);
void  tx_ring_write_desc_and_doorbell(volatile void* db, const void* txdesc, int use_wc, int conservative_fence);
