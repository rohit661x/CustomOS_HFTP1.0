[bits 32]

; outb(port, value)
global outb
outb:
    mov dx, [esp + 4]    ; port
    mov al, [esp + 8]    ; value
    out dx, al
    ret

; inb(port) -> value
global inb
inb:
    mov dx, [esp + 4]    ; port
    in al, dx
    movzx eax, al        ; zero-extend to 32-bit
    ret

; halt()
global halt
halt:
    hlt
    ret
