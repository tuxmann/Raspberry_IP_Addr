# AVR target microcontroller and CPU clock configuration.
MCU     = attiny88
F_CPU   = 8000000
TARGET  = climate_node

# Flash tool settings (linuxspi on Raspberry Pi).
AVRDUDE = avrdude
AVRDUDE_PROGRAMMER = linuxspi
AVRDUDE_PORT      = /dev/spidev0.0:/dev/gpiochip0
AVRDUDE_FLAGS     = -c $(AVRDUDE_PROGRAMMER) -p t88 -P $(AVRDUDE_PORT) -B 10

# Compiler and binary conversion tools.
CC      = avr-gcc
CFLAGS  = -Wall -Wextra -Os -std=gnu99 -DF_CPU=$(F_CPU)UL -mmcu=$(MCU)
OBJCOPY = avr-objcopy

# Project sources and generated outputs.
SRC     = src/main.c src/dht22.c
OBJ     = $(SRC:.c=.o)
HEX     = bin/$(TARGET).hex
ELF     = $(TARGET).elf

.PHONY: all clean flash

all: $(HEX)

$(HEX): $(SRC)
	# Ensure output folder exists.
	@mkdir -p bin
	# Compile each source file to an object file.
	$(CC) $(CFLAGS) -c src/main.c -o src/main.o
	$(CC) $(CFLAGS) -c src/dht22.c -o src/dht22.o
	# Link object files into one ELF executable.
	$(CC) $(CFLAGS) -o $(ELF) $(OBJ)
	# Convert ELF into Intel HEX for AVR flashing.
	$(OBJCOPY) -O ihex -R .eeprom $(ELF) $(HEX)
	# Remove temporary build files.
	@rm -f $(OBJ) $(ELF)
	@echo "Built $(HEX)"

clean:
	rm -f src/main.o src/dht22.o $(ELF) $(HEX)

flash: $(HEX)
	$(AVRDUDE) $(AVRDUDE_FLAGS) -U flash:w:$(HEX):i
