###############################################################################
# Makefile for the project SDrive
###############################################################################

## General Flags
PROJECT = SDrive
MCU = atmega328
TARGET = SDrive.elf
CC = avr-gcc
AWK = awk

## Options common to compile, link and assembly rules
COMMON = -mmcu=$(MCU)

## Compile options common for all C compilation units.
CFLAGS = $(COMMON)
CFLAGS += -Wall -Wpadded -Wshadow -Wa,-gdwarf2     -DF_CPU=16000000UL -Os -funsigned-char -fpack-struct
CFLAGS += -MD -MP -MT $(*F).o -MF dep/$(@F).d 
CFLAGS += -ffunction-sections -mrelax -mcall-prologues

## Assembly specific flags
ASMFLAGS = $(COMMON)
ASMFLAGS += $(CFLAGS)
ASMFLAGS += -x assembler-with-cpp -Wa,-gdwarf2

## Linker flags
LDFLAGS = $(COMMON)
LDFLAGS +=  -Wl,-Map=$@.map -Wl,--gc-sections


## Intel Hex file production flags
HEX_FLASH_FLAGS = -R .eeprom

HEX_EEPROM_FLAGS = -j .eeprom
HEX_EEPROM_FLAGS += --set-section-flags=.eeprom="alloc,load"
HEX_EEPROM_FLAGS += --change-section-lma .eeprom=0 --no-change-warnings

BIN_FLASH_FLAGS = --change-section-address .eeprom=$(FLASHSIZE) --gap-fill 0xff

DATE = $(shell date +%Y%m%d)
FORMAT = binary

#SWVERSIONMAJOR = 1	moved to SDrive.c
#SWVERSIONMINOR = 2

CDEFS += -DDATE=$(DATE)
#CDEFS += -DSWVERSIONMAJOR=$(SWVERSIONMAJOR)
#CDEFS += -DSWVERSIONMINOR=$(SWVERSIONMINOR)
CFLAGS += $(CDEFS) -DILI9329

## Include Directories
#INCLUDES = -I"C:\My\BOB\Sdrive\AVRSDrive\..\..\..\..\AVR\WinAVR\avr\include" -I"C:\My\BOB\Sdrive\AVRSDrive\..\..\..\..\AVR\WinAVR\bin" 

## Objects that must be built in order to link
#OBJECTS = SDrive.o spi.o mmc.o fat.o delay100us.o timer.o
OBJECTS = SDrive.o spi.o mmc.o fat.o usart.o tft.o display.o touchscreen.o

## Objects explicitly added by the user
LINKONLYOBJECTS = 

## Build
all: $(TARGET) SDrive.hex SDrive.eep SDrive.lss SDrive.bin size eeprom_writer.hex

## Compile
#delay100us.o: ../delay100us.s
#	$(CC) $(INCLUDES) $(ASMFLAGS) -c  $<

SDrive.o: ../SDrive.c
	$(CC) $(INCLUDES) $(CFLAGS) -c  $<

spi.o: ../spi.c
	$(CC) $(INCLUDES) $(CFLAGS) -c  $<

mmc.o: ../mmc.c
	$(CC) $(INCLUDES) $(CFLAGS) -c  $<

fat.o: ../fat.c
	$(CC) $(INCLUDES) $(CFLAGS) -c  $<

usart.o: ../usart.c
	$(CC) $(INCLUDES) $(CFLAGS) -c  $<

tft.o: ../tft.c
	$(CC) $(INCLUDES) $(CFLAGS) -c  $<

display.o: ../display.c
	$(CC) $(INCLUDES) $(CFLAGS) -c  $<

touchscreen.o: ../touchscreen.c
	$(CC) $(INCLUDES) $(CFLAGS) -c  $<

##Link
$(TARGET): $(OBJECTS)
	 $(CC) $(LDFLAGS) $(OBJECTS) $(LINKONLYOBJECTS) $(LIBDIRS) $(LIBS) -o $(TARGET)

%.hex: $(TARGET)
	avr-objcopy -O ihex $(HEX_FLASH_FLAGS)  $< $@

%.eep: $(TARGET)
	-avr-objcopy $(HEX_EEPROM_FLAGS) -O ihex $< $@ || exit 0
	-avr-objcopy $(HEX_EEPROM_FLAGS) -O binary $< $@.bin || exit 0
	-xxd -i $@.bin ../eeprom_writer.h || exit 0

%.lss: $(TARGET)
	avr-objdump -h -S $< > $@

%.bin: $(TARGET)
	avr-objcopy -O $(FORMAT) $(BIN_FLASH_FLAGS) $< $@

size: ${TARGET}
	@echo
	@avr-size -C --mcu=${MCU} ${TARGET}

## Clean target
.PHONY: clean
clean:
	-rm -rf $(OBJECTS) SDrive.elf dep/* SDrive.hex SDrive.eep* SDrive.lss *.map SDrive.bin eeprom_writer.*


## Other dependencies
-include $(shell mkdir dep 2>/dev/null) $(wildcard dep/*)

eeprom_writer.hex: ../eeprom_writer.c
	$(CC) $(INCLUDES) $(CFLAGS) $(LDFLAGS) -o eeprom_writer.elf display.o $<
	avr-objcopy -O ihex $(HEX_FLASH_FLAGS) eeprom_writer.elf $@

dist:
	tar cvzf sdrive-ng-$(MCU)-bin.tgz SDrive.hex SDrive.bin SDrive.eep README
