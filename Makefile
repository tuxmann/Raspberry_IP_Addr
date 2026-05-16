# Makefile for Raspberry Pi IP Address display firmware (ATtiny88)
# Build on host: make
# Flash on Raspberry Pi: make flash (or copy bin/show_ip.hex and run avrdude there)

MCU     = attiny88
# F_CPU must match chip clock. ATtiny88: 8MHz internal (CKDIV8 off) -> 8000000
# If scroll is too fast, chip may be 8MHz but F_CPU was 2000000 (4x error).
F_CPU   = 8000000
TARGET  = show_ip

AVRDUDE = avrdude
AVRDUDE_PROGRAMMER = linuxspi
AVRDUDE_PORT      = /dev/spidev0.0:/dev/gpiochip0
AVRDUDE_FLAGS     = -c $(AVRDUDE_PROGRAMMER) -p t88 -P $(AVRDUDE_PORT) -B 10

CC      = avr-gcc
CFLAGS  = -Wall -Os -DF_CPU=$(F_CPU)UL -mmcu=$(MCU)
OBJCOPY = avr-objcopy

SRC     = $(TARGET).c
OBJ     = $(TARGET).o
HEX     = bin/$(TARGET).hex

.PHONY: all clean flash

all: $(HEX)

$(HEX): $(SRC)
	@mkdir -p bin
	$(CC) $(CFLAGS) -o $(OBJ) -c $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET).elf $(OBJ)
	$(OBJCOPY) -O ihex -R .eeprom $(TARGET).elf $(HEX)
	@rm -f $(OBJ) $(TARGET).elf
	@echo "Built $(HEX)"

clean:
	rm -f $(OBJ) $(TARGET).elf $(HEX)

# Flash firmware (run on Raspberry Pi with board connected)
flash: $(HEX)
	$(AVRDUDE) $(AVRDUDE_FLAGS) -U flash:w:$(HEX):i
