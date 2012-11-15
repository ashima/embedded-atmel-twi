#An attempt at a Makefile without actually knowing what I'm trying to compile. CL 12-5-7
LOCAL_INCLUDES = -I./arduino -I../../
LOCAL_LDFLAGS  = arduino/libarduino.a -lm

SOURCES = twi_serial_bridge.cpp ../../TWIMaster.cpp
OBJECTS = $(patsubst %.pde,%.o, $(filter %.pde, $(SOURCES))) \
          $(patsubst %.cpp,%.o, $(filter %.cpp, $(SOURCES)))

PROGRAM = twi_serial_bridge
ELF = $(PROGRAM).elf
HEX = $(PROGRAM).hex
EEP = $(PROGRAM).eep

all: ${HEX}

include Makefile.config

${ARDUINO_LIBRARY}: 
	cd arduino ; $(MAKE) $(MFLAGS)

lib: ${ARDUINO_LIBRARY}

%.elf : ${OBJECTS} lib
	$(CC) $(CFLAGS) $(LDFLAGS) $(OBJECTS) $(LOCAL_LDFLAGS) -o $@

%.elf.dis : %.elf
	$(OBJDUMP) -S $< > $@

%.hex.dis : %.hex
	$(OBJDUMP) -m avr -D $< > $@

%.o.dis : %.o
	$(OBJDUMP) -m avr -S $< > $@

upload: $(HEX)
	$(AVRDUDE) -V -C$(AVRDUDE_CONF) -p$(MCU) -c$(PROGRAMMER) -P$(PORT) -b$(BAUD) -D -Uflash:w:$(HEX):i

$(EXEC): $(OBJECTS)


#	default:#
#	    avr-g++ -L/usr/arduino/lib -I/usr/arduino/include -Wall -DF_CPU=16000000UL -Os -mmcu=atmega328p\
#	      -o main.elf main.c -larduino
#	    avr-objcopy -O ihex -R .eeprom main.elf out.hex
#	upload:
#	    avrdude -c arduino -p m328p -b 57600 -P /dev/ttyUSB0 -U flash:w:out.hex
#upload:
#	echo "I would install here if I knew how"
