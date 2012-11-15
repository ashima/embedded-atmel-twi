#include "Arduino.h"
#include "TWIMaster.h"
#include "string_parse.h"
#include "SerialPrinter.h"

#define USB_BAUD 115200
#define USB_SERIAL Serial

#define LEDPIN 13

bool _verbose = true;
bool _binary = false;
uint8_t _twi_addr = 0x50;
volatile state_t *_p = NOSTATE;
volatile uint8_t _i2c_calls = 0;
SerialPrinter usb(&USB_SERIAL);

// do I need this if twiQ is defined in TWIMaster.h?
//extern twiQueue twiQ;

bool twi_state_ok(uint8_t state) {
  return (state & (1<<STATE_SUCCESS_BIT)) != 0;
}

void print_twi_error() {
  usb.print("TWI ERROR -> ");
  usb.print_hex(_twi_addr);
  usb.print(": ");
  usb.print_hex(_p->state);
  usb.print(" : ");
  usb.print_hex(PIND & 0x3); // the two I2C pins on ATmega2560
  usb.println();
}

void twi_donefunc(state_t *p) {
  _p = p;
  _i2c_calls++;
}

bool wait_for_twi() {
  // blink Arduino Mega LED (writing to PINx toggles on ATmega2560)
  uint16_t loops = 0;
  while (++loops != 0 && _i2c_calls == 0)
    PINB |= (1<<PB7);
  
  if (loops == 0) {
    i2c_master_initialize();
    _p->state = 1<<(STATE_SUCCESS_BIT+1);
  }
  
  return twi_state_ok(_p->state);
}

bool wait_for_twi_w(char* p, uint8_t count) {
  _i2c_calls = 0;

  twiQ.enqueue_w(_twi_addr, p, count, twi_donefunc);

  return wait_for_twi();  
}

bool wait_for_twi_r(char* p, uint8_t count) {
  _i2c_calls = 0;

  twiQ.enqueue_r(_twi_addr, p, count, twi_donefunc);
  
  return wait_for_twi();  
}

const uint8_t MAXCOUNT = 0x20;

void test_I2C_RX_r(uint16_t mem_addr, uint8_t count) {
  if (_verbose) {
    usb.print("Attempting to read address 0x");
    usb.print_hex(mem_addr);
    if (count > 1) {
      usb.print(" - 0x");
      usb.print_hex(mem_addr + count - 1);
    }
    usb.println();
  }

  char p1[] = { (char)(mem_addr & 0xFF), (char)(mem_addr >> 8) };
  char p2[MAXCOUNT];
  
  count = count < MAXCOUNT ? count : MAXCOUNT;
  
  if (wait_for_twi_w(p1, sizeof(p1)) &&
      wait_for_twi_r(p2, count)) {
    if (!_verbose)
      usb.print('~');
    
    if (!_binary) {
      for (uint8_t i = 0; i < count; i++) {
        usb.print_hex((uint8_t)p2[i]);
        usb.print(" ");
      }
      usb.println();
    } else {
      for (uint8_t i = 0; i < count; i++)
        usb.write(p2[i]);
    }
  } else
    print_twi_error();
}

void test_I2C_RX_w(uint16_t mem_addr, uint8_t* write, uint8_t count) {
  if (_verbose) {
    usb.print("Attempting to write address 0x");
    usb.print_hex(mem_addr);
    if (count > 1) {
      usb.print(" - 0x");
      usb.print_hex(mem_addr + count - 1);
    }
    usb.println();
  }
  
  count = count < MAXCOUNT ? count : MAXCOUNT;

  char p[MAXCOUNT+2];
  p[0] = mem_addr & 0xFF;
  p[1] = mem_addr >> 8;
  
  for (int i = 0; i < count; i++)
    p[i+2] = write[i];
  
  if (!wait_for_twi_w(p, count+2))
    print_twi_error();
  else if (!_verbose)
    usb.print('~');
}

bool test_I2C_addr(uint8_t addr) {
  // must read/write at least one byte
  char c;
  _i2c_calls = 0;

  twiQ.enqueue_r(addr, &c, 1, twi_donefunc);
  
  // blink Arduino LED
  uint16_t loops = 0;
  while (++loops != 0 && _i2c_calls == 0)
    PINB |= (1<<PB7);
  
  if (loops == 0) {
    //i2c_master_initialize();
    _p->state = 1<<(STATE_SUCCESS_BIT+1);
  }
  
  return twi_state_ok(_p->state);
}

ISR(TIMER2_COMPA_vect) {
  SPDR = TWSR;
  while ((SPSR & (1<<SPIF)) == 0)
  {}
}
ISR(TIMER2_OVF_vect) {
  SPDR = TWCR;
  while ((SPSR & (1<<SPIF)) == 0)
  {}
}

void setup()
{
  pinMode(LEDPIN, OUTPUT);
  digitalWrite(LEDPIN, HIGH);

  // debugging
  PORTA = 0;
  DDRA = 0xFF;

  USB_SERIAL.begin(USB_BAUD);
  usb.println("twi_serial_bridge");
  
  // SS must be set to output or driven high
  DDRB |= (1<<PB0) | (1<<PB1) | (1<<PB2); // PB0 is SS, PB1 is SCK, PB2 is MOSI, PB3 is MISO
  
	SPCR = (1<<SPE) | (1<<MSTR) | (1<<SPR0);
	SPSR = (1<<SPI2X);

  TCCR2A = 0;
  TCCR2B = (1<<CS21) | (0<<CS20);
  OCR2A = 240;
  TIMSK2 = (1<<OCIE2A) | (1<<TOIE2);
  
  i2c_master_initialize();
}

void loop()
{
  // This is designed to work with twi_slave.h, and specifically, the example in ../RX/RX.pde .
  // syntax is either X00Y R\n, or X00Y W0 W1 ...\n
  // where X is 0 for no byte alignment, 4 for 2-byte alignment, 8 for 4, C for 8,
  //       Y is 0-7 (which is the size of ../RX/RX.pde -> _twiStore)
  //       R is the number of bytes to read
  //       W0 W1 ... is one or more bytes to write
  //       the \n really just needs to be a non-hexadecimal, non-space character
  // "2-byte alignment" means that two bytes will be buffered before sending; this prevents the
  // TWI interface from sending one byte, then letting other code execute and change both bytes,
  // such that the next byte sent doesn't "match" the first byte. An example is illustrative:
  // 
  // Imagine that we are reading RPMs of a motor and they are hovering between 0x00D0 and 0x0120.
  // When we read the LSB, the RPM happens to be 0x00FF, so we read an 0xFF. But then the motor
  // speeds up a tiny bit, such that it is 0x0100 when we read the MSB. The RPM would appear to
  // be 0x01FF, which is double the average RPM during our TWI request! So, our TWI slave code has
  // the ability to buffer 2-8 bytes inside the TWI interrupt service routine, where interrupts
  // are disabled and therefore the 2-byte RPM value cannot be disturbed between accessing the
  // LSB and the MSB.
  
  const uint8_t MAXSIZE = 0x80;
  char buffer[MAXSIZE];
  
  if (usb.read_line(buffer, MAXSIZE, 0, true) == SerialPrinter::Success) {
    char *p = buffer; // this pointer will get modified as things are parsed
    
    if (consume_char_if(p, 's')) {
      for (int addr = 1; addr <= 127; addr++) {
        if (test_I2C_addr(addr)) {
          usb.print_hex(addr);
          usb.println();
        }
      }
    } else if (consume_char_if(p, '-')) {
      while (*p != '\0') {
        switch (*p++) {
          case 'V':
            _verbose = false;
            break;
          case 'v':
            _verbose = true;
            break;
          case 'B':
            _binary = false;
            break;
          case 'b':
            _binary = true;
            break;
          case 'a':
            _twi_addr = parse_hex8(p);
            
            usb.print("Changing TWI address to 0x");
            usb.print_hex(_twi_addr);
            usb.println();
            break;
          case 'o':
            OCR5A = parse_hex16(p);

            usb.print("Changing OCR5A to 0x");
            usb.print_hex(OCR5A);
            usb.println();
            break;
          default:
            usb.print("Not an option: ");
            usb.print(*--p);
            usb.println();
            goto hit_bad_option;
          }
        }
hit_bad_option: ; // need semicolon for it to compile
    } else if (*p != '\0') {
      uint16_t addr = parse_hex16(p);
    
      if (consume_char_if(p, 'w')) {
        uint8_t write[MAXCOUNT];
        uint8_t write_count = parse_hex_array(p, write, MAXCOUNT);
      
        if (write_count > 0)
          test_I2C_RX_w(addr, write, write_count);
      } else {
        uint8_t count = parse_hex8(p);
        test_I2C_RX_r(addr, max(count, 1));
      }
    }
  }
}

