twi\_serial\_bridge API
=====================

basic syntax
------------

These are text commands sent to the  twi\_serial\_bridge MCU over its UART.

    AAAA[ NN]\n               # for reads
    AAAA w BB[ BB ...]\n      # for writes
    AAAA w BBBB[...]\n        # alternative write syntax
    AAAA w BBBB[ BBBB ...]\n  # yet another alternative write syntax

where

* `AAAA` is a 16-bit value in hexadecimal, used by ../../TWISlaveMem14.c:  
  the 14 low bits are the address  
  the 2 high bits are the # bytes to buffer on the slave: 
  01b for 2 bytes, 10b for 4 bytes, and 11b for 8 bytes
* `NN` is the # of bytes to read, in hexadcimal  
  between 1 and `MAXCOUNT` may be specified, will default to 1 if not specified
* `BB[ BB ...]` are the bytes to write (in hexadecimal)  
  between 1 and `MAXCOUNT` may be specified  
  spaces can be added between any number of bytes  
  if multiple bytes are used with no spaces, they must be 0-padded
* `\n` can be any combination of `\n` and `\r`

Note that `MAXCOUNT` was 20h at the time this document was created.

options
-------

    -v      turn verbose printing on (default)
    -V      turn verbose printing off
    -b      turn binary mode on
    -B      turn binary mode off (default)
    -aHH    set TWI address to HH, in hexadecimal
    -oHHHH  change the TWI timeout value (see TWI documentation)

Options may be combined, e.g. `-Vba50\n`. A newline is required. Usually you will use verbose and not binary (`-vB`, the default), or vice versa (`-Vb`).

normal vs. binary mode
----------------------

In verbose + non-binary mode (`-vB`), a read followed by a write will look like
this (`<` means data sent to the MCU; `>` means data received from MCU; all newline characters are displayed):

    < 9c w 012345\r
    > Attempting to write address 0x9C - 0x9E\r\n
    < 9c 4\r
    > Attempting to read address 0x9C - 0x9F\r\n
    > 01 23 45 00 \r\n

Here is the exchange in non-verbose, binary mode (`-Vb`):

    < 9c w 012345\r
    > ~
    < 9c 4\r
    > ~\x01#E\x00
 
The `~` is an acknowledge from non-verbose mode. The response from reading 4 bytes is in binary, with characters rendered when they can be. (`ord('#') == 23h`, `ord('E') == 45h`)

Note that in binary mode, text will still be printed; e.g. when there is an error and when the TWI address is changed. This should be easily detected by disabling verbose mode (`-V`), sending a read or write command, and then checking for the existence of the `~` acknowledge character.

example
-------

    < -vB\r
    < -a50\r
    > Changing TWI address to 0x50\r\n
    < 9c 4\r
    > Attempting to read address 0x9C - 0x9F\r\n
    > 01 23 45 00 \r\n
    < 9c w 09876543\r
    > Attempting to write address 0x9C - 0x9F\r\n
    < 9c 4\r
    > Attempting to read address 0x9C - 0x9F\r\n
    > 09 87 65 43 \r\n

buffering
---------

This is with respect to the 2 MSB of `AAAA` in the **options** section.

Imagine that we are reading RPMs of a motor and they are hovering between 0x00D0 and 0x0120. When we read the LSB, the RPM happens to be 0x00FF, so we read an 0xFF. But then the motor speeds up a tiny bit, such that it is 0x0100 when we read the MSB. The RPM would appear to be 0x01FF, which is double the average RPM during our TWI request! So, our TWI slave code has the ability to buffer 2-8 bytes inside the TWI interrupt service routine, where interrupts are disabled and therefore the 2-byte RPM value cannot be disturbed between accessing the LSB and the MSB.

twi bus scan
------------

Sending `s` (newline not required) will cause the bridge MCU to attempt to read a single byte from every TWI address between 1 and 127 (0 is supposed to be general broadcast). It will report on all TWI devices which responded without error.

twi errors
----------

Should an error occur on the TWI bus, an error message will be printed out which looks like the following:

    TWI ERROR: addr=0x50, TWSR=0x20, SCL=1, SDA=1\r\n

TWSR will have 02h ORed in if a timeout occurred. If SCL and/or SDA are 0, it is likely that an external TWI device is pulling the lines low for some reason. The master _may_ be able to flush the bus by pulsing SCL if only SDA is 0; if SCL is 0, you'll need to reset the troublemaking TWI device.
