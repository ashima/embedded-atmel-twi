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
