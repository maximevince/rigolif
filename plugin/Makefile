all: plugin

CFLAGS  += -marm -mfloat-abi=soft -march=armv5te -ffreestanding -Wall -ggdb3 -nostartfiles -fno-common -fno-stack-protector 
CFLAGS  += -Ilib -pie -fpie
LDFLAGS += -Wl,-Map=plugin.map -Tplugin.ld ./lib/ds1000z.a -Wl,-gc-sections $(CFLAGS)

CC 		:= arm-none-eabi-gcc
OBJCOPY := arm-none-eabi-objcopy
SIZE    := arm-none-eabi-size

plugin.o: plugin.c
	$(CC) $(CFLAGS) -c -o $@ $?

plugin: plugin.o
	$(CC) $? $(CFLAGS) $(LDFLAGS) -o $@.elf
	$(OBJCOPY) $@.elf -O binary $@.bin
	$(SIZE) $@.elf

clean:
	rm -f plugin.o plugin.bin plugin.elf

run: plugin.elf
	../rigolif l -fplugin.bin

