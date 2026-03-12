# HFT-OS: Bare-Metal Kernel for Low-Latency Trading Message Processing

**Prototype 1.0 — Experimental Infrastructure Project**

---

## Project Overview

This project is a prototype bare-metal kernel designed to minimize latency in trading message processing. The kernel eliminates common OS-level latency sources — interrupts, system calls, and context switching — by running application logic directly on the hardware with no operating system intermediary.

Incoming market data is processed through a tight spin-poll loop that reads packets from an RX descriptor ring, evaluates trading conditions, and writes order packets to a TX descriptor ring. The entire path from packet arrival to order dispatch executes without kernel transitions or scheduler intervention.

The current implementation runs inside **QEMU/KVM** with a simulated NIC. The system demonstrates the architecture and data path of an ultra-low-latency trading runtime, serving as an experimentation platform before targeting physical NIC hardware or FPGA offload.

| Approach | Pros | Cons |
|---|---|---|
| FPGA | Hard real-time, sub-µs latency | Inflexible, costly, difficult to modify |
| Customized Linux | Flexible, large ecosystem | Syscall overhead, jitter, general-purpose bloat |
| **This project** | **Deterministic execution, no OS overhead, reprogrammable** | **Prototype stage, emulated NIC** |

---

## System Architecture

The message processing pipeline flows through six stages, from market data generation to order transmission:

```
Market Data Generator (Python)
        │
        ▼
Simulated NIC + RTL Parser (Verilog)
        │
        ▼
RX Descriptor Ring (MMIO)
        │
        ▼
Kernel Spin-Poll Loop (bare-metal C)
        │
        ▼
Trading Decision Logic
        │
        ▼
TX Packet Construction → TX Descriptor Ring
```

**Market Data Generator** — A Python harness (`moldudp64generator.py`) that constructs MoldUDP64 packets containing ITCH-style quote messages. Supports warm-up phases, timed bursts, malformed/corrupt packet injection, and configurable inter-packet jitter for latency measurement.

**Simulated NIC + RTL Parser** — A Verilog RTL design (`moldudp64parser.v` and supporting modules) that models a SmartNIC packet parser. The parser performs header extraction, endian conversion, length validation, and message dispatch at the register-transfer level. An alternative ASM/C parser variant also exists for host-side testing.

**RX Descriptor Ring** — A 16-entry descriptor ring at a fixed physical address (`0x00100000`). The kernel discovers the NIC via PCI bus enumeration, maps its MMIO region, and configures RX descriptor base, length, head, and tail registers. Each descriptor points to a 64-byte packet buffer.

**Kernel Spin-Poll Loop** — The core execution loop (`poll_rx_loop`) continuously checks the RX ring for new packets by comparing the DD (descriptor done) status bit. When a packet arrives, it is parsed and handed to the trading decision logic with zero scheduling delay. `rdtsc`/`rdtscp` instructions bracket the critical path for cycle-level latency measurement.

**Trading Decision Logic** — The `handle_msg` function evaluates incoming quotes against tracked best-bid state. When conditions are met (e.g., price improvement on the target symbol), it triggers order construction.

**TX Packet Construction** — The `send_ouch` function builds an OUCH-format enter-order packet and writes it into the TX descriptor ring for transmission. Completed TX descriptors are reclaimed asynchronously.

---

## Protocol Stack

The prototype uses the MoldUDP64 transport protocol carrying ITCH-style messages. These operate at different layers:

```
UDP
 └── MoldUDP64 (transport framing)
      └── ITCH messages (market data payload)
```

**MoldUDP64** is a lightweight UDP-based transport wrapper defined by NASDAQ. It provides session identification, sequence numbering, and message count headers that allow receivers to detect gaps and request retransmissions.

**ITCH** is the message format carried inside MoldUDP64 frames. Each ITCH message describes a market data event — in this prototype, simplified quote messages containing a stock locate code, price, and share quantity.

The generator emits raw MoldUDP64 packets with the following on-wire structure:

| Field | Size | Description |
|---|---|---|
| Session ID | 8 bytes | Identifies the market data session |
| Payload Length | 4 bytes | Total byte length of the ITCH payload |
| Message Type | 1 byte | ITCH message type identifier |
| Stock Locate | 2 bytes | Symbol identifier (e.g., AAPL) |
| Price | 4 bytes | Quote price |
| Shares | 4 bytes | Quote quantity |

---

## Prototype 1.0 — Current Implementation

The following components are implemented and functional in the current prototype:

- **Custom x86 bootloader** — NASM assembly that initializes the CPU from real mode to 32-bit protected mode, loads the GDT, sets up the stack, and jumps to the C kernel entry point
- **Bare-metal C kernel** — Freestanding C runtime (no libc) with serial UART output for diagnostics
- **PCI bus enumeration** — Scans the PCI configuration space to discover the NIC and map its MMIO base address
- **Polling-based RX/TX loop** — Configures 16-entry RX and TX descriptor rings via MMIO registers, then spin-polls for incoming packets with no interrupt overhead
- **Cache-line-aligned data structures** — RX/TX indices and state variables are aligned to 64-byte cache lines to prevent false sharing
- **MoldUDP64 market data generator** — Python test harness with warm-up and timed phases, configurable packet rates, jitter modes, and fault injection (malformed and corrupt packets)
- **Verilog RTL NIC parser** — Multi-module design (`moldudp64parser.v`, `header.v`, `dispatch.v`, `endian_flip.v`, `length.v`, etc.) simulating SmartNIC-level packet parsing
- **ASM/C parser variant** — Alternative host-side MoldUDP64 parser with inline assembly unpacking (`unpack11.S`) for cross-validation
- **Cycle-level latency measurement** — Uses `rdtsc` and `rdtscp` to measure packet-to-decision latency in CPU cycles
- **OUCH order construction** — Builds enter-order packets in the TX ring when trading conditions are met
- **QEMU/KVM execution** — Entire system boots and runs under `qemu-system-i386` from a single `os-image.bin`

---

## Future Work / Target Architecture

The following items are **not yet implemented** and represent the target direction for future development:

- **Physical NIC integration** — Driver development for PCIe-based NICs (e.g., Intel 82599, Mellanox ConnectX) with real DMA and descriptor ring hardware
- **Hardware timestamping** — PTP or NIC-level timestamps for nanosecond-accurate packet arrival timing
- **10G/25G Ethernet** — High-bandwidth network interfaces for production-grade market data feeds
- **Multi-symbol trading pipeline** — Concurrent tracking and decision logic across multiple instruments
- **SmartNIC offload experimentation** — Moving parser and pre-filter logic onto programmable NIC hardware
- **Expanded strategy logic** — More sophisticated order types, position management, and risk controls
- **Long mode (x86_64)** — Transition from 32-bit protected mode to 64-bit long mode with full paging support
- **Host tuning** — CPU isolation (`isolcpus`), hugepages, SMT disabling, and IRQ affinity for latency-optimized execution on physical hardware

---

## Build & Run

### Requirements
- x86_64 host (bare metal or QEMU)
- NASM, GCC (freestanding), LD
- PCIe NIC (Intel/Mellanox recommended)
- USB stick or PXE bootloader for deployment

### Build Instructions

```bash
make bootloader
make runtime
make image



