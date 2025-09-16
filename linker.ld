ENTRY(_start)
SECTIONS {
    . = 0x40000000;
    PROVIDE(txt_start = .);

    .text.boot : {
        KEEP(*(.text.boot))
    }

    . = ALIGN(4096);

    .text : {
        *(.text .text.*)
    }

    .rodata : {
        *(.rodata) *(.rodata.*)
    }

    .data : {
        *(.data) *(.data.*)
    }

    . = ALIGN(4096);

    .bss : {
        __bss_start = .;
        *(.bss) *(.bss.*)
        __bss_end = .;
    }
    . = ALIGN(4096);

    __primary_stack_start = .;
    . += (4096);
    __primary_stack_end = .;

    /* PROVIDE作用: 若未定义ram_start变量, 则设置该变量为当前地址 */
    PROVIDE(ram_start = .);
}