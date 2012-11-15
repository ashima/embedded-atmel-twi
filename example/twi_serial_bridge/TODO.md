twi\_serial\_bridge TODO
========================

* change it so that 1-byte address reads and writes work on standard TWI devices, and 2-byte address reads and writes work on ../../TWISlaveMem14.c TWI devices
* rename 'test_I2C_RX_r' to 'write' or something like that; same with 'test_I2C_RX_w'
* make non-verbose mode spit out even less
* add explanation of TWI error messages to API.md
* document TWI register debug via SPI and probably start with it turned off
* 1<<(STATE_SUCCESS_BIT+1) should be #defined
* figure out errors with trying to re-`make` inside ./arduino/
* stop relying on Arduino libraries
