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

* `AAAA` is a 16-bit value in hexadecimal:  
  the 14 low bits are the address  
  the 2 high bits are the # bytes to buffer on the slave: 
  `1` for 2 bytes, `2` for 4 bytes, and `3` for 8 bytes
* `NN` is the # of bytes to read, in hexadcimal  
  between 1 and `MAXCOUNT` may be specified, will default to 1 if not specified
* `BB[ BB ...]` are the bytes to write (in hexadecimal)  
  between 1 and `MAXCOUNT` may be specified  
  spaces can be added between any number of bytes 
* `\n` can be any combination of `\n` and `\r`

options
-------

        -v      turn verbose printing on (default)
        -V      turn verbose printing off
        -b      turn binary mode on
        -B      turn binary mode off (default)
        -aHH    set TWI address to HH, in hexadecimal
        -oHHHH  change the TWI timeout value (see TWI documentation)

Options may be combined, e.g. `-Vba50`. A newline is not required. Usually you will use verbose and not binary (`-vB`, the default), or vice versa (`-Vb`).

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
