
; boot/uart_test_boot.asm
; Minimal 16-bit MBR that initializes COM1 and prints a banner repeatedly.
; For demo only; replace with your real boot path when ready.

BITS 16
ORG 0x7C00

%define COM1        0x3F8
%define BAUD_DIV    0x0001       ; 115200 (assuming 1.8432 MHz clock) -> Divisor 1
%define LF          10
%define CR          13

start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00

    ; Init UART COM1 (16550A)
    mov dx, COM1 + 1       ; Interrupt Enable Register
    mov al, 0x00
    out dx, al

    mov dx, COM1 + 3       ; Line Control Register
    mov al, 0x80           ; DLAB = 1
    out dx, al

    mov dx, COM1 + 0       ; Divisor LSB
    mov al, BAUD_DIV & 0xFF
    out dx, al
    mov dx, COM1 + 1       ; Divisor MSB
    mov al, (BAUD_DIV >> 8) & 0xFF
    out dx, al

    mov dx, COM1 + 3       ; LCR
    mov al, 0x03           ; 8N1, DLAB=0
    out dx, al

    mov dx, COM1 + 2       ; FCR
    mov al, 0xC7           ; Enable FIFO, clear, 14-byte threshold
    out dx, al

    mov dx, COM1 + 4       ; MCR
    mov al, 0x0B           ; IRQs enabled, OUT2
    out dx, al

    ; clear screen through ANSI (via UART)
    mov si, msg_clear
    call puts_uart

    ; print banner
    mov si, msg_banner
    call puts_uart

.hang:
    jmp .hang

; -------------------
; UART helpers
; -------------------
; Wait for transmitter holding register empty
uart_wait_thre:
    mov dx, COM1 + 5
.wait:
    in al, dx
    test al, 0x20          ; THRE bit
    jz .wait
    ret

putc_uart:
    push ax
    push dx
    call uart_wait_thre
    mov dx, COM1
    out dx, al
    pop dx
    pop ax
    ret

puts_uart:
    push ax
    push si
.next:
    lodsb
    test al, al
    jz .done
    call putc_uart
    jmp .next
.done:
    pop si
    pop ax
    ret

msg_clear: db 0x1B, '[', '2', 'J', 0x1B, '[', 'H', 0 ; ANSI clear + home
msg_banner: db 'HFTOS UART ONLINE — Slice 7 TUI stub', 13, 10, 0

; Boot signature
times 510-($-$$) db 0
dw 0xAA55
