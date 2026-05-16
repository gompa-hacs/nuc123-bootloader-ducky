##############################################################################
# Makefile for nuc123-dfu-bootloader
# Targeting NUC123SD4AN0 (Ducky One 2 SF DKON1967ST)
##############################################################################

TARGET = nuc123-dfu-bootloader

CC      = arm-none-eabi-gcc
AS      = arm-none-eabi-gcc
LD      = arm-none-eabi-gcc
OBJCOPY = arm-none-eabi-objcopy
SIZE    = arm-none-eabi-size

CPU     = -mcpu=cortex-m0
THUMB   = -mthumb

LDSCRIPT = ldrom.ld

# Removed: uart.c retarget.c _syscalls.c (debug/stdio bloat not needed)
CSRC = \
	CMSIS/system_NUC123.c \
	Library/clk.c \
	Library/sys.c \
	User/descriptors.c \
	User/dfu_transfer.c \
	User/fmc_user.c \
	User/usbd_user.c \
	User/main.c

ASRC = \
	User/startup_NUC123_user.s

INCLUDES = \
	-ILibrary/Device/Nuvoton/NUC123/Include \
	-ILibrary/CMSIS/Include \
	-ILibrary/StdDriver/inc \
	-ICMSIS \
	-IUser

DEFS = -DUSE_ASSERT=0

CFLAGS  = $(CPU) $(THUMB) $(INCLUDES) $(DEFS)
CFLAGS += -Wall -fmessage-length=0 -fsigned-char
CFLAGS += -ffunction-sections -fdata-sections
CFLAGS += -Os -flto
CFLAGS += -std=c99
CFLAGS += --specs=nano.specs

ASFLAGS  = $(CPU) $(THUMB)
ASFLAGS += -x assembler-with-cpp
ASFLAGS += $(INCLUDES)

LDFLAGS  = $(CPU) $(THUMB)
LDFLAGS += -T$(LDSCRIPT)
LDFLAGS += -Wl,--gc-sections
LDFLAGS += -Wl,-Map=$(TARGET).map
LDFLAGS += --specs=nano.specs
LDFLAGS += --specs=nosys.specs
LDFLAGS += -flto
LDFLAGS += -nostartfiles

OBJS  = $(CSRC:.c=.o)
OBJS += $(ASRC:.s=.o)

all: $(TARGET).hex $(TARGET).bin
	$(SIZE) $(TARGET).elf

$(TARGET).elf: $(OBJS) $(LDSCRIPT)
	$(LD) $(LDFLAGS) $(OBJS) -o $@

$(TARGET).hex: $(TARGET).elf
	$(OBJCOPY) -O ihex $< $@

$(TARGET).bin: $(TARGET).elf
	$(OBJCOPY) -O binary $< $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.s
	$(AS) $(ASFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET).elf $(TARGET).hex $(TARGET).bin $(TARGET).map

.PHONY: all clean