#!/usr/bin/env python

import sys
import serial
from argh import ArghParser, arg, command
import struct
from binarydecoder import BinaryRecord, BinaryField, BinaryFieldType
flags=dict( a_field=0x01,
            start_data=0x02,
            eol=0x04,
            an_integer=0xf1,
            a_long=0xf2,
            a_float=0xf3)
def open_serial(port, baud):
    return serial.Serial(port, baud)

def printitems(data):
    print("{0:5d} {1:5d} {2:5d} {3:5d} , {4:5d} {5:5d} {6:5d} , {7:5d} {8:5d} {9:5d} , {10:9.3f}".format(*data))

@command
def binary(port="/dev/tty.usbmodemfa231", baud=57600, debug=False):
    binary_fields = [
        BinaryField("ticks",    "ulong", 1),
        BinaryField("counter",  "ulong", 1),
        BinaryField("temp",     "int",   1),
        BinaryField("accel",    "int",   3),
        BinaryField("gyro",     "int",   3),
        BinaryField("mag",      "int",   3),
        #BinaryField("nack",     "byte",  1),
        BinaryField("battery",  "float", 1),
        BinaryField("parity",   "byte",  1)
        ]
    incoming = open_serial(port, baud)

    br = BinaryRecord([0x02], binary_fields,[BinaryFieldType("byte",  1, "B", '%02X')])
    
    try:
        while(1):
            line = incoming.read(br.total_byte_count)
            recs = br.decode(line)

            print('\n'.join(map(br.to_string, recs)))
    except KeyboardInterrupt:
        pass
    pass

#@command
def dump(port="/dev/tty.usbmodemfa231", baud=57600, binary=False):
    incoming = open_serial(port, baud)
    while(1):
        if binary:
            line = [repr(a) for a in incoming.read(64)]
        else:
            line = incoming.readline()
        print(line)
    
@command
def ascii(port="/dev/tty.usbmodemfa231", baud=57600):
    #format here is currently
    #M A1 A2 A3, G1 G3 G4, M1 M2 G3, P
    incoming = open_serial(port, baud)
    start =False
    while(1):
        line = incoming.readline()
        if(1):
            try:
                data = map(lambda x: x[0](x[1]) , 
                        zip((long, int, int, int, int, int, int, int, int, int, float), line.replace(","," ").split())
                        )
                printitems(data)
            except Exception, e:
                print(e, type(e))
                data = line
                print("*", data, len(data))
            

#        except:
#            #new format?
#            print "*", line  
    pass


if __name__ == "__main__":
    parser = ArghParser()
    parser.add_commands([binary, ascii, dump])
    parser.dispatch()
    sys.exit(0)

# example invocation:
# $ /opt/local/bin/python2.7 read.py binary --port /dev/tty.usbmodem1a21 --baud 57600

