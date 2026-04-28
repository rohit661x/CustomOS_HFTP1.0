; boot/bootloader.asm
; HFT-OS bootloader — real mode setup → disk load → A20 → protected mode → kernel
;
; Memory layout:
;   0x7C00        this bootloader (loaded by BIOS)
;   0x10000       kernel binary (loaded here from disk, linked at 0x10000)
;   0x100000+     RX/TX descriptor rings and packet buffers (memmap.h)
;
; Disk layout (floppy / QEMU -fda image):
;   Sector 0  (LBA 0)  bootloader MBR
;   Sector 1  (LBA 1)  padding (unused)
;   Sector 2+ (LBA 2+) kernel binary  ← loaded by this bootloader

[org 0x7C00]
[bits 16]

start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00              ; stack grows down from bootloader base

    mov [boot_drive], dl        ; BIOS sets DL = boot drive on entry — preserve it

    ; ── Stage 1: load kernel from disk into 0x10000 ─────────────────
    ; ES:BX = 0x1000:0x0000  →  linear address 0x10000
    mov ax, 0x1000
    mov es, ax
    xor bx, bx

    mov ah, 0x02                ; INT 13h function: read sectors
    mov al, 32                  ; 32 sectors = 16 KB (ample for any stub kernel)
    mov ch, 0                   ; cylinder 0
    mov cl, 3                   ; sector 3 (1-indexed CHS; LBA 2 = kernel start)
    mov dh, 0                   ; head 0
    mov dl, [boot_drive]
    int 0x13
    jc .disk_err

    ; Restore DS/ES after INT 13h (some BIOSes clobber segment registers)
    xor ax, ax
    mov ds, ax
    mov es, ax

    ; ── Stage 2: enable A20 line (fast method via port 0x92) ────────
    ; Required for the kernel to access 0x100000+ (descriptor rings, packet buffers)
    in  al, 0x92
    or  al, 0x02                ; set A20 enable bit
    and al, 0xFE                ; keep reset bit (bit 0) clear — do not reboot
    out 0x92, al

    ; ── Stage 3: load GDT and enter 32-bit protected mode ───────────
    lgdt [gdt_descriptor]
    mov eax, cr0
    or  eax, 1                  ; set PE bit
    mov cr0, eax
    jmp dword CODE_SEG:init_pm  ; far jump flushes prefetch queue, loads CS

.disk_err:
    ; Best-effort BIOS teletype 'E' before we have serial up
    mov ah, 0x0E
    mov al, 'E'
    int 0x10
    jmp .hang

.hang:
    hlt
    jmp .hang

boot_drive: db 0

; ── Protected mode entry ─────────────────────────────────────────────
[bits 32]
init_pm:
    mov ax, DATA_SEG
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov esp, 0x90000            ; 32-bit stack, safe in low memory
    cld                         ; forward string ops

    call serial_init
    mov esi, bootmsg
    call serial_write_str

    ; ── Hand off to kernel _start (linked at 0x10000 per linker.ld) ─
    jmp dword 0x10000

; ── COM1 (0x3F8) serial, polling, no interrupts ──────────────────────
serial_init:
    mov dx, 0x3F9 ; IER — disable interrupts
    mov al, 0x00
    out dx, al
    mov dx, 0x3FB ; LCR — enable DLAB (baud divisor access)
    mov al, 0x80
    out dx, al
    mov dx, 0x3F8 ; DLL — divisor low byte (38400 baud → divisor 3)
    mov al, 0x03
    out dx, al
    mov dx, 0x3F9 ; DLH — divisor high byte
    mov al, 0x00
    out dx, al
    mov dx, 0x3FB ; LCR — 8 bits, no parity, 1 stop bit, clear DLAB
    mov al, 0x03
    out dx, al
    mov dx, 0x3FA ; FCR — enable FIFO, clear, 14-byte threshold
    mov al, 0xC7
    out dx, al
    mov dx, 0x3FC ; MCR — RTS/DSR set
    mov al, 0x0B
    out dx, al
    ret

serial_write_str:
    push eax
    push ecx
    push edx
.next:
    lodsb
    test al, al
    jz .done
    mov cl, al                  ; save char — LSR poll will overwrite AL
.wait:
    mov dx, 0x3FD               ; LSR
    in  al, dx
    test al, 0x20               ; TX holding register empty?
    jz .wait
    mov dx, 0x3F8               ; THR
    mov al, cl
    out dx, al
    jmp .next
.done:
    pop edx
    pop ecx
    pop eax
    ret

bootmsg: db "Bootloader OK — loading kernel", 13, 10, 0

; ── GDT: flat 4 GB code and data segments ────────────────────────────
align 8
gdt_start:
    dq 0x0000000000000000       ; null descriptor
    dq 0x00CF9A000000FFFF       ; code:  base=0, limit=4 GB, R/X
    dq 0x00CF92000000FFFF       ; data:  base=0, limit=4 GB, R/W
gdt_end:

gdt_descriptor:
    dw gdt_end - gdt_start - 1
    dd gdt_start

CODE_SEG equ 0x08
DATA_SEG equ 0x10

; ── MBR boot signature ────────────────────────────────────────────────
times 510 - ($ - $$) db 0
dw 0xAA55
