LOCAL_INCLUDES = -I./arduino -I./lib
LOCAL_LDFLAGS  = arduino/libarduino.a -lm

SOURCES = MPU-9150.ino lib/MPU6050/MPU6050.cpp lib/I2Cdev/I2Cdev.cpp lib/AK8975/AK8975.cpp
OBJECTS = $(patsubst %.ino,%.o, $(filter %.ino, $(SOURCES))) \
          $(patsubst %.cpp,%.o, $(filter %.cpp, $(SOURCES))) \
	  arduino/Wire/Wire.cpp.o arduino/Wire/utility/twi.c.o

PROGRAM = MPU-9150
ELF = $(PROGRAM).elf
HEX = $(PROGRAM).hex
EEP = $(PROGRAM).eep

all: ${HEX}

include Makefile.config

${ARDUINO_LIBRARY}: force_look
	cd arduino ; $(MAKE) $(MFLAGS)

lib: ${ARDUINO_LIBRARY}

%.elf : ${OBJECTS} lib
	$(CC) $(CFLAGS) $(LDFLAGS) $(OBJECTS) $(LOCAL_LDFLAGS) -o $@

upload: $(HEX)
	$(AVRDUDE) -C$(AVRDUDE_CONF) -p$(MCU) -c$(PROGRAMMER) -P$(PORT) -b$(BAUD) -DV -Uflash:w:$(HEX):i

$(EXEC): $(OBJECTS)
