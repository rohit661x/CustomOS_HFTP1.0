# Makefile — CustomOS_HFTP1.0
# Produces: build/os.img — a 1.44 MB floppy image bootable under QEMU.
#
# Build pipeline:
#   boot/bootloader.asm  →  nasm -f bin        →  build/boot/bootloader.bin
#   src/entry.asm        →  nasm -f elf32       →  build/src/entry.o
#   src/io.asm           →  nasm -f elf32       →  build/src/io.o
#   src/kernel.c         →  i686-elf-gcc        →  build/src/kernel.o
#   entry.o + io.o + kernel.o  →  i686-elf-ld  →  build/kernel.elf
#   kernel.elf           →  objcopy -O binary   →  build/kernel.bin
#   bootloader.bin + padding + kernel.bin  →  dd  →  build/os.img
#                                           (kernel at LBA 2 = CHS sector 3)
#
# Feature flags (pass on command line, e.g. make FENCE_CONSERVATIVE=1):
#   FENCE_CONSERVATIVE  0|1  full mfence before TX doorbell (default: 0)
#   DOORBELL_WC         0|1  write-combining MMIO doorbell   (default: 0)
#   PIN_CORE            0|1  CPU affinity annotation          (default: 0)
#   CACHE_WARM          0|1  prefetch descriptors at init     (default: 1)

# ── Toolchain ────────────────────────────────────────────────────────
CC      := i686-elf-gcc
LD      := i686-elf-ld
OBJCOPY := i686-elf-objcopy
AS      := nasm
QEMU    := qemu-system-i386

# ── Feature flags ────────────────────────────────────────────────────
FENCE_CONSERVATIVE ?= 0
DOORBELL_WC        ?= 0
PIN_CORE           ?= 0
CACHE_WARM         ?= 1

# ── Compiler flags ───────────────────────────────────────────────────
CFLAGS := \
    -ffreestanding -nostdlib -std=gnu11 \
    -O3 -march=i686 \
    -fno-plt -finline-functions -funroll-loops \
    -fomit-frame-pointer -fno-tree-vectorize \
    -Wall -Wextra \
    -I. \
    -DFENCE_CONSERVATIVE=$(FENCE_CONSERVATIVE) \
    -DDOORBELL_WC=$(DOORBELL_WC)               \
    -DPIN_CORE=$(PIN_CORE)                     \
    -DCACHE_WARM=$(CACHE_WARM)

LDFLAGS := -T linker.ld -nostdlib

# ── Build output directory (must be defined before KERN_OBJS) ────────
BUILD := build

# ── Source lists ─────────────────────────────────────────────────────
BOOT_SRC  := boot/bootloader.asm

# entry.o must be first in the link so _start lands at 0x10000
KERN_OBJS := \
    $(BUILD)/src/entry.o   \
    $(BUILD)/src/io.o      \
    $(BUILD)/src/kernel.o

# ── Default target ───────────────────────────────────────────────────
.PHONY: all clean qemu qemu-debug

all: $(BUILD)/os.img

# ── Bootloader ───────────────────────────────────────────────────────
$(BUILD)/boot/bootloader.bin: $(BOOT_SRC) | $(BUILD)/boot
	$(AS) -f bin $< -o $@

# ── Kernel ASM objects ────────────────────────────────────────────────
$(BUILD)/src/%.o: src/%.asm | $(BUILD)/src
	$(AS) -f elf32 $< -o $@

# ── Kernel C objects ─────────────────────────────────────────────────
$(BUILD)/src/%.o: src/%.c | $(BUILD)/src
	$(CC) $(CFLAGS) -c $< -o $@

# ── Link kernel ELF ──────────────────────────────────────────────────
$(BUILD)/kernel.elf: $(KERN_OBJS) linker.ld
	$(LD) $(LDFLAGS) $(KERN_OBJS) -o $@

# ── Strip to flat binary ─────────────────────────────────────────────
$(BUILD)/kernel.bin: $(BUILD)/kernel.elf
	$(OBJCOPY) -O binary $< $@

# ── Assemble floppy image ─────────────────────────────────────────────
# Layout:
#   LBA 0 (sector 1): bootloader  (512 bytes)
#   LBA 1 (sector 2): padding     (zeroed)
#   LBA 2+           : kernel     (bootloader loads 32 sectors = 16 KB)
$(BUILD)/os.img: $(BUILD)/boot/bootloader.bin $(BUILD)/kernel.bin
	dd if=/dev/zero           of=$@                  bs=512 count=2880  2>/dev/null
	dd if=$(BUILD)/boot/bootloader.bin of=$@ bs=512 seek=0  conv=notrunc 2>/dev/null
	dd if=$(BUILD)/kernel.bin          of=$@ bs=512 seek=2  conv=notrunc 2>/dev/null

# ── QEMU run ─────────────────────────────────────────────────────────
qemu: $(BUILD)/os.img
	$(QEMU) -fda $< -serial stdio -nographic -m 32M

# QEMU with interrupt tracing for debugging
qemu-debug: $(BUILD)/os.img
	$(QEMU) -fda $< -serial stdio -nographic -m 32M -d int,cpu_reset -no-reboot

# ── Directory creation ────────────────────────────────────────────────
$(BUILD)/boot $(BUILD)/src:
	mkdir -p $@

# ── Clean ─────────────────────────────────────────────────────────────
clean:
	rm -rf $(BUILD)
