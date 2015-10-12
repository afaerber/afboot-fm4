CROSS_COMPILE ?= arm-none-eabi-

CC = $(CROSS_COMPILE)gcc
OBJCOPY = $(CROSS_COMPILE)objcopy
OBJDUMP = $(CROSS_COMPILE)objdump
SIZE = $(CROSS_COMPILE)size
GDB = $(CROSS_COMPILE)gdb
OPENOCD = openocd

CFLAGS := -mthumb -mcpu=cortex-m4
CFLAGS += -ffunction-sections -fdata-sections
CFLAGS += -Os -std=gnu99 -Wall
LDFLAGS := -nostartfiles -Wl,--gc-sections

all: s6e2cc mb9bf

%.o: %.c
	$(CC) -c $(CFLAGS) $< -o $@

s6e2cc.elf: foo.c Makefile s6e2ccaj.lds
	$(CC) $(CFLAGS) -DS6E2CC -T s6e2ccaj.lds $(LDFLAGS) -o $@ foo.c

s6e2cc: s6e2cc.elf Makefile
	$(OBJCOPY) -Obinary s6e2cc.elf s6e2cc.bin
	$(OBJCOPY) -Osrec s6e2cc.elf s6e2cc.srec
	$(OBJDUMP) -S s6e2cc.elf > s6e2cc.lst
	$(SIZE) s6e2cc.elf

mb9bf.elf: foo.c Makefile mb9bf568r.lds
	$(CC) $(CFLAGS) -DMB9BF -T mb9bf568r.lds $(LDFLAGS) -o $@ foo.c

mb9bf: mb9bf.elf Makefile
	$(OBJCOPY) -Obinary mb9bf.elf mb9bf.bin
	$(OBJCOPY) -Osrec mb9bf.elf mb9bf.srec
	$(OBJDUMP) -S mb9bf.elf > mb9bf.lst
	$(SIZE) mb9bf.elf

clean:
	@rm -f *.o *.elf *.bin *.srec *.lst
