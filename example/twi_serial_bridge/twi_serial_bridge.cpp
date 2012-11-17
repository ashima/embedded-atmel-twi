#include "Arduino.h"
#include "TWIMaster.h"
#include "string_parse.h"
#include "SerialPrinter.h"

/********************************************
Alter the #defines below to suit your device.
********************************************/
#define USB_BAUD 115200
#define USB_SERIAL Serial
#define PIN_TWI PIND
#define SCL (1<<0) // this is the pin # of SCL on PIN_TWI for your AVR
#define SDA (1<<1) //                      SDA 
#define DEFAULT_TWI_ADDRESS 0x50
#define LEDPIN 13 // this is an LED on Arduino boards
const uint8_t MAXCOUNT = 0x20; // max # of bytes to read/write to TWI

// we're using the fact that the low two bits of TWSR are 0 because 
// ../../TWIMaster.h does not use TWPS[1:0]
#define TWI_TIMEOUT (1<<(STATE_SUCCESS_BIT+1))
#define ACK_CHAR '~'

bool _verbose = true;
bool _binary = false;
uint8_t _twi_addr = DEFAULT_TWI_ADDRESS;
volatile state_t *_p = NOSTATE;
volatile uint8_t _i2c_calls = 0;
SerialPrinter usb(&USB_SERIAL);

bool twi_state_ok(uint8_t state) {
  return (state & (1<<STATE_SUCCESS_BIT)) != 0;
}

void print_twi_error() {
  uint8_t v = PIN_TWI;

  usb.print("TWI ERROR: addr=0x");
  usb.print_hex(_twi_addr);
  usb.print(", TWSR=0x");
  usb.print_hex(_p->state);
  usb.print(", SCL=");
  usb.print(v & SCL ? '1' : '0');
  usb.print(", SDA=");
  usb.print(v & SDA ? '1' : '0');
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
    i2c_master_initialize(); // not sure if this is still required
    _p->state = TWI_TIMEOUT;
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

void twi_r(uint16_t mem_addr, uint8_t count) {
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
      usb.print(ACK_CHAR);
    
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

void twi_w(uint16_t mem_addr, uint8_t* write, uint8_t count) {
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
    usb.print(ACK_CHAR);
}

bool test_twi_addr(uint8_t addr) {
  // must read/write at least one byte
  char c;
  _i2c_calls = 0;

  twiQ.enqueue_r(addr, &c, 1, twi_donefunc);
  
  // blink Arduino LED
  uint16_t loops = 0;
  while (++loops != 0 && _i2c_calls == 0)
    PINB |= (1<<PB7);
  
  if (loops == 0) {
    // this line being commented out is what makes this different from 
    // twi_r(...); I forget whether or not this is important; at the time, I
    // was futzing with properly recovering from errors, but this may no longer
    // be necessary
    //i2c_master_initialize(); 
    _p->state = TWI_TIMEOUT;
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

void set_up_twi_spi_debugging() {
  // SS must be set to output or driven high
  DDRB |= (1<<PB0) | (1<<PB1) | (1<<PB2); // PB0 is SS, PB1 is SCK, PB2 is MOSI, PB3 is MISO
	SPCR = (1<<SPE) | (1<<MSTR) | (1<<SPR0);
	SPSR = (1<<SPI2X);

  TCCR2A = 0;
  TCCR2B = (1<<CS21) | (0<<CS20);
  OCR2A = 240;
  TIMSK2 = (1<<OCIE2A) | (1<<TOIE2);
}

void setup() {
  pinMode(LEDPIN, OUTPUT);
  digitalWrite(LEDPIN, HIGH);

  // for debugging; this port is nicely organized on the DIP header on the 
  // Arduino Mega
  PORTA = 0;
  DDRA = 0xFF;

  USB_SERIAL.begin(USB_BAUD);
  usb.println("twi_serial_bridge");
  
  set_up_twi_spi_debugging();
  
  i2c_master_initialize();
}

void loop() {
  // this is designed to work with ../../TWISlaveMem14.c
  // see API.md for details

  const uint8_t MAXSIZE = 0x80;
  char buffer[MAXSIZE];
  
  if (usb.read_line(buffer, MAXSIZE, 0, true) == SerialPrinter::Success) {
    char *p = buffer; // this pointer will get modified as things are parsed
    
    if (consume_char_if(p, 's')) {
      for (int addr = 1; addr <= 127; addr++) {
        if (test_twi_addr(addr)) {
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
          twi_w(addr, write, write_count);
      } else {
        uint8_t count = parse_hex8(p);
        twi_r(addr, max(count, 1));
      }
    }
  }
}

