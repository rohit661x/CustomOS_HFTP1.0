OUTPUT_FORMAT(elf32-i386)
OUTPUT_ARCH(i386)
ENTRY(_start)

SECTIONS
{
  /* Load the kernel at 64 KiB physical — safely below memmap.h's 1 MiB data structures */
  . = 0x10000;

  .text ALIGN(4) :
  {
    *(.text .text.*)
  }

  .rodata ALIGN(4) :
  {
    *(.rodata .rodata.*)
  }

  .data ALIGN(4) :
  {
    *(.data .data.*)
  }

  .bss ALIGN(4) :
  {
    __bss_start = .;
    *(.bss .bss.*)
    *(COMMON)
    __bss_end = .;
  }

  /DISCARD/ : { *(.eh_frame) *(.note*) *(.comment) }
}
