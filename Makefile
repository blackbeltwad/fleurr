TARGET ?= avr

BUILD_DIR = build

ifeq ($(TARGET), avr)
CC        = avr-gcc
MCU       = atmega328p
F_CPU     = 16000000UL
CFLAGS    = -mmcu=$(MCU) -DF_CPU=$(F_CPU) -O0 -g3 -Wall -Wextra -std=gnu11
INCLUDES  = -Iinclude -Ikernel -Iarch/avr/atmega328p
ARCH_DIR  = arch/avr/atmega328p
EXAMPLE_DIR = examples/avr_led_blink
SRCS = $(wildcard kernel/*.c) $(wildcard $(ARCH_DIR)/*.c) $(wildcard $(ARCH_DIR)/*.S) $(wildcard $(EXAMPLE_DIR)/*.c)
OUT = $(BUILD_DIR)/fleurr_avr.elf
HEX = $(BUILD_DIR)/fleurr_avr.hex
PORT = /dev/cu.usbmodem101
BAUD = 115200
GDB_PORT = 1234

else ifeq ($(TARGET), cortex_m7)
CC        = arm-none-eabi-gcc
GDB       = arm-none-eabi-gdb
OBJCOPY   = arm-none-eabi-objcopy
MCU_FLAGS = -mcpu=cortex-m7 -mthumb -mfloat-abi=hard -mfpu=fpv5-d16
CFLAGS    = $(MCU_FLAGS) -O0 -g3 -Wall -Wextra -std=gnu11
LDFLAGS   = $(MCU_FLAGS) -T arch/cortex_m7/nucleo_f767zi/linker.ld -nostdlib
INCLUDES  = -Iinclude -Ikernel -Iarch/cortex_m7/nucleo_f767zi
ARCH_DIR  = arch/cortex_m7/nucleo_f767zi
EXAMPLE_DIR = examples/cortex_m7_led_blink
SRCS = $(wildcard kernel/*.c) $(wildcard $(ARCH_DIR)/*.c) $(wildcard $(ARCH_DIR)/*.S) $(wildcard $(EXAMPLE_DIR)/*.c)
OUT = $(BUILD_DIR)/fleurr_m7.elf
BIN = $(BUILD_DIR)/fleurr_m7.bin
GDB_PORT = 3333

else
$(error Unknown TARGET '$(TARGET)'. Use TARGET=avr or TARGET=cortex_m7)
endif

.PHONY: build flash clean simavr openocd gdb

build: $(SRCS)
	@mkdir -p $(BUILD_DIR)
ifeq ($(TARGET), avr)
	$(CC) $(CFLAGS) $(INCLUDES) $(SRCS) -o $(OUT)
else ifeq ($(TARGET), cortex_m7)
	$(CC) $(CFLAGS) $(INCLUDES) $(SRCS) $(LDFLAGS) -o $(OUT)
endif

flash: build
ifeq ($(TARGET), avr)
	avr-objcopy -O ihex -R .eeprom $(OUT) $(HEX)
	avrdude -p $(MCU) -c arduino -P $(PORT) -b $(BAUD) -U flash:w:$(HEX):i
else ifeq ($(TARGET), cortex_m7)
	$(OBJCOPY) -O binary $(OUT) $(BIN)
	openocd -f interface/stlink.cfg -f target/stm32f7x.cfg \
		-c "program $(OUT) verify reset exit"
endif

simavr: build
	simavr -m $(MCU) -f $(F_CPU) -g $(OUT)

# Runs OpenOCD debug server for the Cortex-M7 target
openocd:
	openocd -f interface/stlink.cfg -f target/stm32f7x.cfg

gdb: build
ifeq ($(TARGET), avr)
	avr-gdb $(OUT) -ex "target remote localhost:$(GDB_PORT)"
else ifeq ($(TARGET), cortex_m7)
	$(GDB) $(OUT) \
		-ex "target remote localhost:$(GDB_PORT)" \
		-ex "monitor reset halt" \
		-ex "load"
endif

clean:
	rm -rf $(BUILD_DIR)
