MEMORY
{
  rw (rwx): ORIGIN = 0x00000000, LENGTH = 512K
  /* rw (rwx) : ORIGIN = 0x00080000, LENGTH = 128K */
}

__rom_start__   = ORIGIN(ro);
__rom_size__    = LENGTH(ro);
__ram_start__   = ORIGIN(rw);
__ram_size__    = LENGTH(rw);
__ram_end__     = __ram_start__ + __ram_size__;
__stack_end__   = __ram_end__;      /* Top of RAM */
__stack_size__  = 16K;
__stack_start__ = __stack_end__ - __stack_size__;
__heap_start__  = _ebss;      /* Between bss and stack */
__heap_end__    = __stack_start__;
end             = __heap_start__;

SECTIONS
{
  . = 0;

  .text : {
    KEEP(*(.intvec))          /* Startup code */
    . = 0x20;
    KEEP(*(.startup))         /* Startup code */
    *(.text*)                 /* Program code */
    KEEP(*(.rodata*))         /* Read only data */
    *(.glue_7)
    *(.glue_7t)
    . = ALIGN(4);
    __text_end__ = .;
  } >rw

  _sidata = LOADADDR(.data);

  .data : ALIGN(4) {
    _sdata = .;         /* Used in crt0.S */
    *(.data*)                 /* Initialized data */
    _edata = .;         /* Used in crt0.S */
  } >rw

  .bss (NOLOAD) : ALIGN(4) {
    _sbss = .;          /* Used in crt0.S */
    *(.bss*)                  /* Uninitialized data */
    *(COMMON)                 /* Common data */
    _ebss = .;          /* Used in crt0.S */
  } >rw
}

/* Firmware entry point */

ENTRY(__iar_program_start)
