; src/entry.asm
; Kernel entry point — first code executed after the bootloader jumps to 0x10000.
; Zeros BSS (linker-script symbols __bss_start/__bss_end), then calls kernel_main.

[bits 32]

extern kernel_main
extern __bss_start
extern __bss_end

global _start
_start:
    mov edi, __bss_start
    mov ecx, __bss_end
    sub ecx, edi
    shr ecx, 2              ; byte count → dword count (BSS is 4-byte aligned per linker.ld)
    xor eax, eax
    rep stosd               ; zero entire BSS region

    call kernel_main

.hang:
    hlt
    jmp .hang
