#CL 2012-05-07
#Makefile for the arduino 'core library' files.
LOCAL_INCLUDES=-I./
HEADERS := $(wildcard *.h)

C_SOURCES := $(wildcard *.c)
CPP_SOURCES := $(wildcard *.cpp)
OBJECTS = $(patsubst %.c,%.o, $(C_SOURCES)) $(patsubst %.cpp,%.o, $(CPP_SOURCES))
default: lib

include ../Makefile.config

$(ARDUINO_LIBRARY): $(OBJECTS)
	${AR} crs ${ARDUINO_LIBRARY} ${OBJECTS}

lib: $(ARDUINO_LIBRARY)
