/*
 * src/kernel.c — bare-metal HFT kernel
 *
 * Pipeline (per-packet, hot path):
 *   _start (entry.asm) → kernel_main → pci_enumerate → init_rx_ring →
 *   init_tx_ring → poll_rx_loop → handle_packet → send_ouch
 *
 * Timing taps (obs/timing.h) instrument every stage transition so latency
 * can be measured at cycle granularity via UART on new best-bid events.
 *
 * Packet format (from generator/moldudp64generator.py via hw_nic_sim.c):
 *   Offset  0: session (8 bytes, ignored)
 *   Offset  8: payload length BE-uint32 (11 for Add Order)
 *   Offset 12: ITCH payload
 *     [0]    msg_type      char   ('A' = Add Order)
 *     [1..2] stock_locate  LE-u16 (AAPL_LOCATE = 0x0042)
 *     [3..6] price         LE-u32 (ticks)
 *     [7..10] shares       LE-u32
 */

#include <stdint.h>
#include "memmap.h"
#include "../obs/timing.h"

/* ── freestanding type compatibility ──────────────────────────────── */
typedef uint32_t uintptr_t;

/* ── Compile-time invariants ──────────────────────────────────────── */
_Static_assert((RX_DESC_BASE & 63) == 0, "RX_DESC_BASE must be 64-byte aligned");
_Static_assert((TX_DESC_BASE & 63) == 0, "TX_DESC_BASE must be 64-byte aligned");

/* ── I/O primitives from src/io.asm (cdecl) ──────────────────────── */
void     outb(uint16_t port, uint8_t  val);
uint8_t  inb (uint16_t port);
void     outl(uint16_t port, uint32_t val);
uint32_t inl (uint16_t port);

/* ── freestanding memset (no libc) ───────────────────────────────── */
static void kmemset(void *dst, int c, uint32_t n) {
    uint8_t *p = (uint8_t *)dst;
    while (n--) *p++ = (uint8_t)c;
}

/* ── Serial (COM1, 0x3F8, 38400 8N1, polling TX) ─────────────────── */
void serial_init(void) {
    outb(0x3F9, 0x00);   /* IER: disable all interrupts               */
    outb(0x3FB, 0x80);   /* LCR: enable DLAB for divisor access        */
    outb(0x3F8, 0x03);   /* DLL: divisor low byte  (38400 baud → 3)    */
    outb(0x3F9, 0x00);   /* DLH: divisor high byte                     */
    outb(0x3FB, 0x03);   /* LCR: 8-bit, no parity, 1 stop bit          */
    outb(0x3FA, 0xC7);   /* FCR: enable FIFO, clear, 14-byte threshold  */
    outb(0x3FC, 0x0B);   /* MCR: RTS/DSR set                           */
}

void serial_write(const char *s) {
    while (*s) {
        while ((inb(0x3FD) & 0x20) == 0);   /* poll LSR TX empty bit */
        outb(0x3F8, (uint8_t)*s++);
    }
}

static void print_hex32(uint32_t v) {
    static const char h[] = "0123456789ABCDEF";
    char buf[9];
    for (int i = 0; i < 8; i++)
        buf[7 - i] = h[(v >> (i * 4)) & 0xF];
    buf[8] = '\0';
    serial_write(buf);
}

static void emit_cycles(const char *label, uint64_t c) {
    serial_write(label);
    serial_write("=");
    print_hex32((uint32_t)c);   /* lower 32 bits sufficient for sub-us deltas */
    serial_write(" ");
}

/* ── Raw TSC (boot timing only; hot path uses rdtsc_ordered) ─────── */
static inline uint64_t read_tsc(void) {
    uint32_t lo, hi;
    __asm__ volatile ("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
}

/* ── PCI bus enumeration ─────────────────────────────────────────── */
#define PCI_ADDR  0x0CF8U
#define PCI_DATA  0x0CFCU

static uintptr_t nic_mmio_base;

static uint32_t pci_cfg_read(uint8_t bus, uint8_t dev, uint8_t fn, uint8_t off) {
    outl(PCI_ADDR,
         0x80000000U | ((uint32_t)bus << 16) |
         ((uint32_t)dev << 11) | ((uint32_t)fn << 8) | (off & 0xFC));
    return inl(PCI_DATA);
}

void pci_enumerate(void) {
    for (uint32_t bus = 0; bus < 256; bus++) {
        for (uint8_t dev = 0; dev < 32; dev++) {
            for (uint8_t fn = 0; fn < 8; fn++) {
                uint32_t vd = pci_cfg_read((uint8_t)bus, dev, fn, 0x00);
                if ((vd & 0xFFFF) == 0xFFFF) continue;

                uint32_t ci = pci_cfg_read((uint8_t)bus, dev, fn, 0x08);
                if (((ci >> 24) & 0xFF) != 0x02) continue;  /* class: network */
                if (((ci >> 16) & 0xFF) != 0x00) continue;  /* subclass: ethernet */

                serial_write("NIC vendor=0x");
                print_hex32(vd & 0xFFFF);
                serial_write(" device=0x");
                print_hex32((vd >> 16) & 0xFFFF);
                serial_write("\r\n");

                uint32_t bar0 = pci_cfg_read((uint8_t)bus, dev, fn, 0x10);
                if (bar0 & 0x1) {
                    serial_write("BAR0 I/O-space unsupported\r\n");
                } else {
                    nic_mmio_base = bar0 & ~(uint32_t)0xF;
                    serial_write("MMIO=0x");
                    print_hex32((uint32_t)nic_mmio_base);
                    serial_write("\r\n");
                }
                return;
            }
        }
    }
    /* Fallback for QEMU without the NIC plugin */
    serial_write("No NIC; stub MMIO=0x00500000\r\n");
    nic_mmio_base = 0x00500000U;
}

/* ── NIC MMIO helpers ───────────────────────────────────────────────*/
static inline void nic_write32(uint32_t off, uint32_t v) {
    *(volatile uint32_t *)(nic_mmio_base + (uintptr_t)off) = v;
}
static inline uint32_t nic_read32(uint32_t off) {
    return *(volatile uint32_t *)(nic_mmio_base + (uintptr_t)off);
}

/* ── Descriptor layouts (Intel e1000 legacy, 16 bytes each) ────────
 *
 * RX descriptor:
 *   [0..7]  addr    physical address of packet buffer (64-bit)
 *   [8..9]  length  DMA'd byte count
 *   [10..11] csum   packet checksum (ignored)
 *   [12]    status  bit 0 = DD (Descriptor Done)
 *   [13]    errors
 *   [14..15] special
 *
 * TX legacy descriptor:
 *   [0..7]  addr    physical address of TX buffer (64-bit)
 *   [8..9]  length  byte count to transmit
 *   [10]    cso     checksum offset (0)
 *   [11]    cmd     EOP|IFCS|RS
 *   [12]    status  bit 0 = DD (set by NIC on completion)
 *   [13]    css     checksum start (0)
 *   [14..15] special
 */

#define CTRL_RX_EN       0x00000002U
#define CTRL_TX_EN       0x00000004U

#define RX_DD            0x01U
#define TX_CMD_EOP       0x01U
#define TX_CMD_IFCS      0x02U
#define TX_CMD_RS        0x08U

typedef struct {
    uint64_t addr;
    uint16_t length;
    uint16_t csum;
    uint8_t  status;
    uint8_t  errors;
    uint16_t special;
} rx_desc_t;

typedef struct {
    uint64_t addr;
    uint16_t length;
    uint8_t  cso;
    uint8_t  cmd;
    uint8_t  status;
    uint8_t  css;
    uint16_t special;
} tx_desc_t;

_Static_assert(sizeof(rx_desc_t) == 16, "rx_desc_t must be 16 bytes");
_Static_assert(sizeof(tx_desc_t) == 16, "tx_desc_t must be 16 bytes");

/* ── OUCH Enter output packet ───────────────────────────────────────*/
typedef struct {
    char     msg_type;       /* 'O' = OUCH Enter Order  */
    char     version;
    char     reserved[2];
    uint32_t stock_locate;
    uint32_t order_id;
    uint32_t price;
    uint32_t shares;
} OuchEnter;

/* ── Ring init ──────────────────────────────────────────────────────*/
void init_rx_ring(void) {
    kmemset((void *)RX_DESC_BASE, 0, RX_RING_SIZE * DESC_SIZE);

    rx_desc_t *d = (rx_desc_t *)RX_DESC_BASE;
    for (uint32_t i = 0; i < RX_RING_SIZE; i++) {
        d[i].addr   = RX_PKT_BASE + i * PKT_BUF_SIZE;
        d[i].length = PKT_BUF_SIZE;
    }

    nic_write32(NIC_REG_RXDESC_LO,   (uint32_t)(RX_DESC_BASE & 0xFFFFFFFFU));
    nic_write32(NIC_REG_RXDESC_HI,   0);
    nic_write32(NIC_REG_RXDESC_LEN,  RX_RING_SIZE * DESC_SIZE);
    nic_write32(NIC_REG_RXDESC_HEAD, 0);
    nic_write32(NIC_REG_RXDESC_TAIL, 0);
    nic_write32(NIC_REG_CTRL, nic_read32(NIC_REG_CTRL) | CTRL_RX_EN);

    /* Touch all descriptors to warm L1 before entering the poll loop */
    for (uint32_t i = 0; i < RX_RING_SIZE; i++) {
        volatile uint8_t  s = d[i].status;
        volatile uint32_t l = d[i].length;
        volatile uint64_t a = d[i].addr;
        (void)s; (void)l; (void)a;
    }

    serial_write("RX ring init\r\n");
}

void init_tx_ring(void) {
    kmemset((void *)TX_DESC_BASE, 0, TX_RING_SIZE * DESC_SIZE);

    nic_write32(NIC_REG_TXDESC_LO,   (uint32_t)(TX_DESC_BASE & 0xFFFFFFFFU));
    nic_write32(NIC_REG_TXDESC_HI,   0);
    nic_write32(NIC_REG_TXDESC_LEN,  TX_RING_SIZE * DESC_SIZE);
    nic_write32(NIC_REG_TXDESC_HEAD, 0);
    nic_write32(NIC_REG_TXDESC_TAIL, 0);
    nic_write32(NIC_REG_CTRL, nic_read32(NIC_REG_CTRL) | CTRL_TX_EN);

    serial_write("TX ring init\r\n");
}

/* ── Hot-path state (each variable on its own cache line) ─────────── */
#define AAPL_LOCATE 0x0042U

/* MoldUDP64 packet offsets */
#define MOLD_OFFSET_LEN     8U   /* 4-byte big-endian payload length */
#define MOLD_OFFSET_PAYLOAD 12U  /* first byte of ITCH message       */
#define ITCH_PAYLOAD_LEN    11U  /* bytes in our Add Order message   */
#define PKT_MIN_LEN         (MOLD_OFFSET_PAYLOAD + ITCH_PAYLOAD_LEN)

static uint32_t rx_head       __attribute__((aligned(64))) = 0;
static uint32_t tx_idx        __attribute__((aligned(64))) = 0;
static uint32_t tx_head       __attribute__((aligned(64))) = 0;
static uint32_t next_order_id __attribute__((aligned(64))) = 1;
static uint32_t best_bid_price  __attribute__((aligned(64))) = 0;
static uint32_t best_bid_shares __attribute__((aligned(64))) = 0;

static void reclaim_tx(void) {
    uint32_t hw = nic_read32(NIC_REG_TXDESC_HEAD);
    while (tx_head != hw)
        tx_head = (tx_head + 1) % TX_RING_SIZE;
}

/* ── TX path ─────────────────────────────────────────────────────── */
static void send_ouch(uint32_t order_id, uint32_t price, uint32_t shares,
                      timing_sample_t *ts) {
    /* Spin until a TX slot is free */
    uint32_t next = (tx_idx + 1) % TX_RING_SIZE;
    while (__builtin_expect(next == tx_head, 0)) {
        reclaim_tx();
        __asm__ volatile ("pause");
    }

    TAP(*ts, STAGE_ORDER);   /* start: OUCH packet construction */

    OuchEnter *o = (OuchEnter *)(TX_PKT_BASE + tx_idx * PKT_BUF_SIZE);
    o->msg_type     = 'O';
    o->version      = 0;
    o->reserved[0]  = 0;
    o->reserved[1]  = 0;
    o->stock_locate = AAPL_LOCATE;
    o->order_id     = order_id;
    o->price        = price;
    o->shares       = shares;

    tx_desc_t *td = (tx_desc_t *)TX_DESC_BASE;
    td[tx_idx].addr    = TX_PKT_BASE + tx_idx * PKT_BUF_SIZE;
    td[tx_idx].length  = (uint16_t)sizeof(OuchEnter);
    td[tx_idx].cso     = 0;
    td[tx_idx].cmd     = TX_CMD_EOP | TX_CMD_IFCS | TX_CMD_RS;
    td[tx_idx].status  = 0;
    td[tx_idx].css     = 0;
    td[tx_idx].special = 0;

    TAP(*ts, STAGE_TXDESC);  /* OUCH build + descriptor fill complete */

    /* Memory fence before doorbell write.
     * FENCE_CONSERVATIVE=1 → full mfence (guarantees all stores visible).
     * FENCE_CONSERVATIVE=0 → compiler barrier only (store-buffer ordering
     *   is sufficient on TSO x86 for a single producer/consumer). */
#if FENCE_CONSERVATIVE
    __asm__ volatile ("mfence" ::: "memory");
#else
    __asm__ volatile ("" ::: "memory");
#endif

    TAP(*ts, STAGE_FENCE);   /* fence cost */

    nic_write32(NIC_REG_TXDESC_TAIL, tx_idx);

    TAP(*ts, STAGE_DOORBELL); /* MMIO write (NIC kick) */

    tx_idx = (tx_idx + 1) % TX_RING_SIZE;
}

/* ── Per-packet processing ───────────────────────────────────────── */
static void handle_packet(uint32_t idx, uint32_t plen, timing_sample_t *ts) {
    if (plen < PKT_MIN_LEN) return;

    uint8_t *pkt = (uint8_t *)(RX_PKT_BASE + (uintptr_t)idx * PKT_BUF_SIZE);

    /* Read MoldUDP64 big-endian payload length (touches first cache line) */
    uint32_t payload_len =
        ((uint32_t)pkt[MOLD_OFFSET_LEN    ] << 24) |
        ((uint32_t)pkt[MOLD_OFFSET_LEN + 1] << 16) |
        ((uint32_t)pkt[MOLD_OFFSET_LEN + 2] <<  8) |
         (uint32_t)pkt[MOLD_OFFSET_LEN + 3];

    TAP(*ts, STAGE_LOAD);   /* after first cache-line access */

    if (payload_len != ITCH_PAYLOAD_LEN) return;

    uint8_t *p       = pkt + MOLD_OFFSET_PAYLOAD;
    char     msg_type     = (char)p[0];
    uint16_t stock_locate = (uint16_t)p[1] | ((uint16_t)p[2] << 8);
    uint32_t price        = (uint32_t)p[3] | ((uint32_t)p[4] << 8) |
                            ((uint32_t)p[5] << 16) | ((uint32_t)p[6] << 24);
    uint32_t shares       = (uint32_t)p[7] | ((uint32_t)p[8] << 8) |
                            ((uint32_t)p[9] << 16) | ((uint32_t)p[10] << 24);

    TAP(*ts, STAGE_PARSE);  /* all field reads complete */

    if (__builtin_expect(msg_type != 'A' || stock_locate != AAPL_LOCATE, 1))
        return;

    TAP(*ts, STAGE_FILTER); /* filter passed */

    if (price <= best_bid_price) return;

    best_bid_price  = price;
    best_bid_shares = shares;

    TAP(*ts, STAGE_BOOK);     /* book state updated */
    TAP(*ts, STAGE_DECISION); /* decision: send immediately */

    send_ouch(next_order_id++, price, shares, ts);

    /* Structured timing line — only on new best-bid events */
    serial_write("BID ");
    emit_cycles("poll",   stage_cycles(ts, STAGE_POLL));
    emit_cycles("load",   stage_cycles(ts, STAGE_LOAD));
    emit_cycles("parse",  stage_cycles(ts, STAGE_PARSE));
    emit_cycles("filter", stage_cycles(ts, STAGE_FILTER));
    emit_cycles("book",   stage_cycles(ts, STAGE_BOOK));
    emit_cycles("order",  stage_cycles(ts, STAGE_ORDER));
    emit_cycles("txdesc", stage_cycles(ts, STAGE_TXDESC));
    emit_cycles("fence",  stage_cycles(ts, STAGE_FENCE));
    emit_cycles("bell",   stage_cycles(ts, STAGE_DOORBELL));
    serial_write("\r\n");
}

/* ── RX spin loop ────────────────────────────────────────────────── */
void poll_rx_loop(void) {
    serial_write("RX poll\r\n");
    rx_desc_t *ring = (rx_desc_t *)RX_DESC_BASE;

    for (;;) {
        reclaim_tx();
        __asm__ volatile ("pause");

        if (__builtin_expect(ring[rx_head].status & RX_DD, 0)) {
            timing_sample_t ts;
            START_PACKET_TIMING(ts);

            uint32_t plen = ring[rx_head].length;
            TAP(ts, STAGE_POLL);        /* length read from descriptor */

            ring[rx_head].status = 0;   /* clear DD so NIC can reuse slot */

            if (__builtin_expect(plen > 0 && plen <= PKT_BUF_SIZE, 1))
                handle_packet(rx_head, plen, &ts);

            rx_head = (rx_head + 1) % RX_RING_SIZE;
        }
    }
}

/* ── Kernel entry ────────────────────────────────────────────────── */
void kernel_main(void) {
    serial_init();
    serial_write("kernel_main\r\n");

    uint64_t t0 = read_tsc();
    pci_enumerate();
    serial_write("PCI cycles=");
    print_hex32((uint32_t)(read_tsc() - t0));
    serial_write("\r\n");

    init_rx_ring();
    init_tx_ring();

    poll_rx_loop();   /* never returns */
}
