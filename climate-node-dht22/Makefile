# AVR target microcontroller and CPU clock configuration.
MCU     = attiny88
F_CPU   = 8000000
TARGET  = climate_node

# Flash tool settings (linuxspi on Raspberry Pi).
AVRDUDE = avrdude
AVRDUDE_PROGRAMMER = linuxspi
AVRDUDE_PORT      = /dev/spidev0.0:/dev/gpiochip0
AVRDUDE_FLAGS     = -c $(AVRDUDE_PROGRAMMER) -p t88 -P $(AVRDUDE_PORT) -B 125kHz

CC      = avr-gcc
CFLAGS  = -Wall -Wextra -Os -std=gnu99 -DF_CPU=$(F_CPU)UL -mmcu=$(MCU)
OBJCOPY = avr-objcopy

SRC     = src/main.c src/dht22.c src/ambient_dim.c src/display_pwm.c
OBJ     = $(SRC:.c=.o)
HEX     = bin/$(TARGET).hex
ELF     = $(TARGET).elf

DEBUG_TARGET = debug_brightness
DEBUG_HEX    = bin/$(DEBUG_TARGET).hex
DEBUG_ELF    = $(DEBUG_TARGET).elf

.PHONY: all clean flash debug flash-debug

all: $(HEX)

debug:
	@mkdir -p bin
	$(CC) $(CFLAGS) -c src/debug_brightness.c -o src/debug_brightness.o
	$(CC) $(CFLAGS) -c src/display_pwm.c -o src/display_pwm.o
	$(CC) $(CFLAGS) -o $(DEBUG_ELF) src/debug_brightness.o src/display_pwm.o
	$(OBJCOPY) -O ihex -R .eeprom $(DEBUG_ELF) $(DEBUG_HEX)
	@rm -f src/debug_brightness.o src/display_pwm.o $(DEBUG_ELF)
	@echo "Built $(DEBUG_HEX)"

$(HEX): $(SRC)
	@mkdir -p bin
	$(CC) $(CFLAGS) -c src/main.c -o src/main.o
	$(CC) $(CFLAGS) -c src/dht22.c -o src/dht22.o
	$(CC) $(CFLAGS) -c src/ambient_dim.c -o src/ambient_dim.o
	$(CC) $(CFLAGS) -c src/display_pwm.c -o src/display_pwm.o
	$(CC) $(CFLAGS) -o $(ELF) $(OBJ)
	$(OBJCOPY) -O ihex -R .eeprom $(ELF) $(HEX)
	@rm -f $(OBJ) $(ELF)
	@echo "Built $(HEX)"

clean:
	rm -f src/main.o src/dht22.o src/ambient_dim.o src/display_pwm.o \
		src/debug_brightness.o $(ELF) $(HEX) $(DEBUG_ELF) $(DEBUG_HEX)

flash: $(HEX)
	$(AVRDUDE) $(AVRDUDE_FLAGS) -U flash:w:$(HEX):i

flash-debug: debug
	$(AVRDUDE) $(AVRDUDE_FLAGS) -U flash:w:$(DEBUG_HEX):i
