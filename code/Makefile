TOOLCHAIN ?= arm-none-eabi-
CC      := $(TOOLCHAIN)gcc
OBJCOPY := $(TOOLCHAIN)objcopy
SIZE    := $(TOOLCHAIN)size
OPENOCD ?= openocd
# This clone probe is reliable for STM32F4 flash programming at 100 kHz. It
# connected at 1 MHz but stalled in the flash algorithm on the tested unit.
SWD_KHZ ?= 100
OPENOCD_ARGS := -f interface/jlink.cfg -c "transport select swd" \
	-f target/stm32f4x.cfg -c "adapter speed $(SWD_KHZ)"

TARGET := wm8978_usb_audio
BUILD  := build

C_SOURCES := \
	main.c \
	system.c \
	wm8978.c \
	audio_i2s.c \
	usb_descriptors.c \
	third_party/tinyusb/src/tusb.c \
	third_party/tinyusb/src/common/tusb_fifo.c \
	third_party/tinyusb/src/device/usbd.c \
	third_party/tinyusb/src/class/audio/audio_device.c \
	third_party/tinyusb/src/portable/synopsys/dwc2/dcd_dwc2.c \
	third_party/tinyusb/src/portable/synopsys/dwc2/dwc2_common.c
ASM_SOURCES := startup.s

OBJECTS := $(addprefix $(BUILD)/,$(C_SOURCES:.c=.o)) \
	$(addprefix $(BUILD)/,$(ASM_SOURCES:.s=.o))

INCLUDES := \
	-I. \
	-Ithird_party/tinyusb/src \
	-Ithird_party/cmsis_device_f4/Include \
	-Ithird_party/CMSIS_5/Include

CPUFLAGS := -mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard
CFLAGS := $(CPUFLAGS) -Os -g3 -ffunction-sections -fdata-sections \
	-Wall -Wextra -Werror -std=c11 $(INCLUDES) \
	-DSTM32F407xx -DHSE_VALUE=8000000u \
	-DCFG_TUSB_MCU=OPT_MCU_STM32F4 \
	-DBOARD_TUD_RHPORT=0 -DBOARD_TUD_MAX_SPEED=OPT_MODE_FULL_SPEED
ASFLAGS := $(CPUFLAGS) -g3 -x assembler-with-cpp
LDFLAGS := $(CPUFLAGS) -T linker.ld -nostartfiles --specs=nano.specs \
	--specs=nosys.specs -Wl,--gc-sections -Wl,-Map=$(BUILD)/$(TARGET).map

.PHONY: all clean backup-flash flash test

all: $(BUILD)/$(TARGET).elf $(BUILD)/$(TARGET).hex $(BUILD)/$(TARGET).bin

$(BUILD)/%.o: %.c
	mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/%.o: %.s
	mkdir -p $(@D)
	$(CC) $(ASFLAGS) -c $< -o $@

$(BUILD)/$(TARGET).elf: $(OBJECTS) linker.ld
	$(CC) $(OBJECTS) $(LDFLAGS) -o $@
	$(SIZE) $@

$(BUILD)/$(TARGET).hex: $(BUILD)/$(TARGET).elf
	$(OBJCOPY) -O ihex $< $@

$(BUILD)/$(TARGET).bin: $(BUILD)/$(TARGET).elf
	$(OBJCOPY) -O binary $< $@

flash: all
	$(OPENOCD) $(OPENOCD_ARGS) -c "init" -c "reset halt" \
		-c "program $(BUILD)/$(TARGET).elf verify" -c "reset run" \
		-c "shutdown"

backup-flash: all
	mkdir -p backups
	$(OPENOCD) $(OPENOCD_ARGS) -c "init" -c "reset halt" \
		-c "dump_image backups/flash_before_usb_audio.bin 0x08000000 0x80000" \
		-c "program $(BUILD)/$(TARGET).elf verify" -c "reset run" \
		-c "shutdown"

test:
	$(OPENOCD) $(OPENOCD_ARGS) -c "init" -c "halt" \
		-c "mdw 0x2001FF00 19" -c "resume" -c "shutdown"

clean:
	rm -rf $(BUILD)
