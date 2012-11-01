// adapted from https://github.com/ashima/drone/commit/944dce336e9c2687e66f94b1803d668f5fd88063

#include <stdint.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <util/twi.h>
#include <avr/io.h>

extern void TWIUserError( uint8_t );
extern void TWIUserSignal( uint8_t );

/* TWI init_ support functions.
*/
static inline void init_start() {
  TWCR = (1<<TWEN)|                             // TWI Interface enabled.
         (1<<TWIE)|(1<<TWINT)|                  // Enable TWI Interupt and clear the flag.
         (0<<TWEA)|(1<<TWSTA)|(0<<TWSTO)|       // Initiate a START condition.
         (0<<TWWC);                             //
}

static inline void init_stop() {
  TWCR = (1<<TWEN)|                                 // TWI Interface enabled
         (0<<TWIE)|(1<<TWINT)|                      // Disable TWI Interrupt and clear the flag
         (0<<TWEA)|(0<<TWSTA)|(1<<TWSTO)|           // Initiate a STOP condition.
         (0<<TWWC);
}

static inline void init_clear_bus_error() {
  TWCR = (1<<TWEN)|                                 // TWI Interface enabled
         (1<<TWIE)|(1<<TWINT)|                      // Enable TWI Interupt and clear the flag to read next byte
         (1<<TWEA)|(0<<TWSTA)|(1<<TWSTO)|           // Send ACK after reception
         (0<<TWWC);
}

static inline void init_ack() {
  TWCR = (1<<TWEN)|                                 // TWI Interface enabled
         (1<<TWIE)|(1<<TWINT)|                      // Enable TWI Interupt and clear the flag to read next byte
         (1<<TWEA)|(0<<TWSTA)|(0<<TWSTO)|           // Send ACK after reception
         (0<<TWWC);
}

static inline void init_nack() {
  TWCR = (1<<TWEN)|                                 // TWI Interface enabled
         (1<<TWIE)|(1<<TWINT)|                      // Enable TWI Interupt and clear the flag to read next byte
         (0<<TWEA)|(0<<TWSTA)|(0<<TWSTO)|
         (0<<TWWC);
}


#define MAXGROUP 8

typedef union {
  uint8_t c[MAXGROUP];
  uint16_t i;
} addr_t;
/*
 5 bytes used for setup.
*/
uint8_t *twiStore ;
int  read_len;
int  write_len;

void setup(uint8_t addr, uint8_t* bs, int rlen, int wlen) {
  twiStore = bs;
  read_len  = rlen;
  write_len = wlen;

  TWAR = addr << 1;
#ifdef TWAMR
  TWAMR &= (1 << 0); // ensure there is no mask
#endif
//  TWDR = 0x00;                                      // Default content = SDA released.
  init_ack();
}
#define D8 /8

// N.B. prog_uchar from <avr/pgmspace.h> does not work, as the PROGMEM attribute gets lost from the typedef
const unsigned char PROGMEM mm[] = { 0,1,3,7 };

ISR(TWI_vect) {
  PORTB |= (1<<PB2);
  TCNT2 = 0;
  SPDR = TWSR;
  /* 11 bytes used for local infunction state
   */
  static int16_t i;
  static addr_t buff;
  static uint8_t mask;

  switch (TWSR D8) {
    // we just ACKed our address; note that TWDR will contain SLA+W
    // (we would care about reading TWDR if TWAMR != 0b0000 000x)
    case TW_SR_SLA_ACK D8:
      i = -2;
      mask = 1;
      init_ack();
      break;

    case TW_SR_DATA_ACK D8:
      if (i < write_len) {
        if (0 == mask)
          twiStore[i++] = TWDR;
        else {
          buff.c[i++ & mask] = TWDR;

          if (0 == i) {
            if ((buff.i | 0x7) == 0xffff)
              TWIUserSignal( (uint8_t)(buff.i & 0x7));
            else {
              i = buff.i & 0x3fff;

              mask = pgm_read_byte (&mm[ buff.c[1] / 0x40 ]);
              if ( (i & mask) != 0) {
                TWIUserError(1);
                init_nack(); // Starting address not properly aligned.
                break;       // Ugly!
              }
            }
          } else if ( 0 == (i & mask) ) {
            uint8_t *p, *q;
            p = &twiStore[i]; // passed end of reg block
            q = &buff.c[(mask+1)];  // passwd end of cache

            if (mask > 1) {
              *--p = *--q;
              *--p = *--q;
            }
            if (mask > 3) {
              *--p = *--q;
              *--p = *--q;
              *--p = *--q;
              *--p = *--q;
            }

            // mask >= 1
            *--p = *--q;
            *--p = *--q;
          }
        }
      }
      init_ack();
      break;

    case TW_ST_SLA_ACK D8:
    case TW_ST_DATA_ACK D8:
      if ( i >=0 && i < read_len) {
        if (0 == mask)
          TWDR = twiStore[i++] ;
        else {
          if (0 == (i & mask)) {
            uint8_t *p, *q;
            q = &twiStore[i];
            p = &buff.c[0];

            // mask >= 1
            p[0] = q[0];
            p[1] = q[1];

            if (mask > 1) {
              p[2] = q[2];
              p[3] = q[3];
            }
            if (mask > 3) {
              p[4] = q[4];
              p[5] = q[5];
              p[6] = q[6];
              p[7] = q[7];
            }
          }
          TWDR = buff.c[i++ & mask];
        }
      } else
        TWDR = 0;

      // just fall through
      //init_ack();
      //break;

    case TW_ST_DATA_NACK D8:
    case TW_SR_STOP D8:
      init_ack();
      break;

    case TW_BUS_ERROR D8:
      init_clear_bus_error();
      break;
    case TW_SR_ARB_LOST_SLA_ACK D8:
    case TW_SR_ARB_LOST_GCALL_ACK D8:
    case TW_SR_DATA_NACK D8:
    case TW_SR_GCALL_DATA_NACK  D8:
    case TW_ST_ARB_LOST_SLA_ACK D8:
    /*case TW_ST_DATA_ACK_LAST_BYTE  D8:*/
    /*case TW_ST_LAST_DATA  D8:*/
    //last_error = TWSR;
      TWIUserError( TWSR );
      // break;

    /*case TW_SR_GCALL_ACK:
      case TW_SR_GCALL_DATA_ACK*/
    default:
      init_nack();
      break;
  }
  PORTB &= ~(1<<PB2);
}
