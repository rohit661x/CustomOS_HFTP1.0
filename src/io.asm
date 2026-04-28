[bits 32]

; outb(port, val)
global outb
outb:
    mov dx, [esp + 4]
    mov al, [esp + 8]
    out dx, al
    ret

; inb(port) -> val
global inb
inb:
    mov dx, [esp + 4]
    in al, dx
    movzx eax, al
    ret

; outl(port, val)  — 32-bit I/O write (used by PCI config)
global outl
outl:
    mov dx,  [esp + 4]
    mov eax, [esp + 8]
    out dx, eax
    ret

; inl(port) -> val  — 32-bit I/O read (used by PCI config)
global inl
inl:
    mov dx, [esp + 4]
    in eax, dx
    ret

; halt()
global halt
halt:
    hlt
    ret
