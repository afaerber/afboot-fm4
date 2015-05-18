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

obj-y += foo.o

all: test.elf test

%.o: %.c
	$(CC) -c $(CFLAGS) $< -o $@

test.elf: $(obj-y) Makefile s6e2ccaj.lds
	$(CC) -T s6e2ccaj.lds $(LDFLAGS) -o test.elf $(obj-y)

test: test.elf Makefile
	$(OBJCOPY) -Obinary test.elf test.bin
	$(OBJCOPY) -Osrec test.elf test.srec
	$(OBJDUMP) -S test.elf > test.lst
	$(SIZE) test.elf

clean:
	@rm -f *.o *.elf *.bin *.srec *.lst
