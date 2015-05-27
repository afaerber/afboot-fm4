ENTRY(reset)
MEMORY
{
	FLASH (RX)  : ORIGIN = 0x00000000, LENGTH = 0x00200000
	SRAM0 (RWX) : ORIGIN = 0x1ffd0000, LENGTH = 0x00030000
	SRAM1 (RWX) : ORIGIN = 0x20038000, LENGTH = 0x00008000
	SRAM2 (RWX) : ORIGIN = 0x20040000, LENGTH = 0x00008000
	STACK (RW)  : ORIGIN = 0x1fffffff, LENGTH = 0
}
SECTIONS
{
	. = ORIGIN(FLASH);

	.text :
	{
		KEEP(*(.vector_table))
		*(.text)
		*(.rodata)
		*(.rodata.*)
		/*. = ALIGN(4);*/
		_end_text = .;
	} >FLASH

	.data :
	{
		_start_data = .;
		*(.data)
		_end_data = .;
	} >SRAM0 AT >FLASH

	.bss :
	{
		_start_bss = .;
		*(.bss)
		_end_bss = .;
	} >SRAM0

	.stack :
	{
		_stack_top = .;
	} >STACK
}
