*embedded-atmel-twi* is a robust driver module for Atmel's Two Wire Interface
(I2C) on Atmel's AVR ATmega chips. It includes handling for bus errors not
documented in Atmel's TWI [slave] and [master] Application Notes (the
flowcharts are incorrect), and implemented in the [slave code] but not the
[master code]. The Master driver can operate in a synchronous or queued
asynchronous mode. The slave implemented so far implements a multi-byte memory
interface.

[slave]:       http://www.atmel.com/Images/doc2565.pdf
               "AVR311: Using the TWI module as I2C slave"
[slave code]:  http://www.atmel.com/dyn/resources/prod_documents/AVR311.zip
[master]:      http://www.atmel.com/Images/doc2564.pdf
               "AVR315: Using the TWI module as I2C master"
[master code]: http://www.atmel.com/dyn/resources/prod_documents/AVR315.zip

Tags:
[twi](http://ashimagroup.net/os/tag/twi)
[atmel](http://ashimagroup.net/os/tag/atmel)
[embedded](http://ashimagroup.net/os/tag/embedded)

