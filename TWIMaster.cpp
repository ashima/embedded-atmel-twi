#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <assert.h>
#include "TWIMaster.h"

// for assert(...)
//void abort(){}

#define PIN_s_error     (1<<PA0)
#define PIN_s_bus_error (1<<PA1)
#define PIN_timer_vect  (1<<PA7)
#define PIN_twi_vect    (1<<PA4)
bool _timeout;

static void s_advance();

twiQueue twiQ;

/* TWI init_ support functions.
 */

static inline void init_start() {
  // .len == 0 if the command is a NOP
  if (twiQ.currCmd().len > 0) {
    TWCR = (1<<TWEN)|                             // TWI Interface enabled.
           (1<<TWIE)|(1<<TWINT)|                  // Enable TWI Interupt and clear the flag.
           (0<<TWEA)|(1<<TWSTA)|(0<<TWSTO)|       // Initiate a START condition.
           (0<<TWWC);                             //
  } else {
    twiQ.currCmd().state = (1<<STATE_SUCCESS_BIT);
    s_advance();
  }
}

static inline void init_stop() {
  TWCR = (1<<TWEN)|                                 // TWI Interface enabled
         (0<<TWIE)|(1<<TWINT)|                      // Disable TWI Interrupt and clear the flag
         (0<<TWEA)|(0<<TWSTA)|(1<<TWSTO)|           // Initiate a STOP condition.
         (0<<TWWC);
}

static inline void init_ack() {
  TWCR = (1<<TWEN)|                                 // TWI Interface enabled
         (1<<TWIE)|(1<<TWINT)|                      // Enable TWI Interupt and clear the flag to read next byte
         (1<<TWEA)|(0<<TWSTA)|(0<<TWSTO)|           // Send ACK after reception
         (0<<TWWC);
}

static inline void init_nothing() {
  TWCR = (1<<TWEN)|                                 // TWI Interface enabled
         (1<<TWIE)|(1<<TWINT)|                      // Enable TWI Interupt and clear the flag to read next byte
         (0<<TWEA)|(0<<TWSTA)|(0<<TWSTO)|
         (0<<TWWC);
}

static inline unsigned char twi_int_state() {
  return (TWCR & (1<<TWIE));
}

/* Interput activity
 */

static char *out_p, *out_q;
static bool shouldRunCB = true;

static void backend() {
  #ifdef USINGTIMER
  // reset timer counter if we're still doing TWI; otherwise disable the interrupt
  if (twi_int_state())
    TCNT5 = 0;
  else
    TIMSK5 = 0;
  #endif

  if ( shouldRunCB ) {
    while ( twiQ.hasCallback() ) {
      state_t &s = twiQ.currCallback();

      if ( (callback_fp)0 != s.donefunc ) {
        shouldRunCB = false;
        sei();

        s.donefunc(&s);

        cli();
        shouldRunCB = true;
      }

      twiQ.doneCallback();
    }
  }
}

static void s_nop() {}

static void s_raise() { // Should raise some assertion here.
  //assert( 0 );
}

// see "Miscellaneous States" in the datasheet; it seems to indicate that one ought to
// _only_ set the TWSTO and TWINT flags (because it says the result of doing that is to
// set the TWI bus into "the not addressed Slave mode"); it seems acceptable to just send
// a normal TWI master stop
static void s_advance_bus_error() {
  twiQ.doneCmd();
  init_stop();
  if ( twiQ.hasCmd() )
    init_start();
}

static void s_advance() {
  twiQ.doneCmd();
  if ( twiQ.hasCmd() )
    init_start();
  else
    init_stop();
}

static void s_success() {
  (twiQ.currCmd()).state = (TWSR & TWSR_STATUS_MASK) | (1<<STATE_SUCCESS_BIT);
  s_advance();
}

static void s_BUS_ERROR() {
  PINA |= PIN_s_bus_error;

  if (_timeout) {
    (twiQ.currCmd()).state = (TWSR & TWSR_STATUS_MASK) | (1<<STATE_TIMEOUT_BIT);
    _timeout = false;
  }
  else
    (twiQ.currCmd()).state = (TWSR & TWSR_STATUS_MASK);

  s_advance_bus_error();
}

static void s_ERROR() {
  PINA |= PIN_s_error;
  (twiQ.currCmd()).state = (TWSR & TWSR_STATUS_MASK);
  s_advance();
}

static void s_TX_NEXT() {
  if ( out_p != out_q ) {
    TWDR = *out_p++;
    init_nothing();
  } else
    s_success();
}

static void s_START() {
  state_t &s = twiQ.currCmd();
  out_p = s.buff;
  out_q = out_p + s.len;
  TWDR = s.addr;
  init_nothing();
}

static void s_RX_LAST() {
  *out_p++ = TWDR;
  s_success();
}

static void s_RX_SKIP() {
  if ( out_p != (out_q-1) )
    init_ack();
  else
    init_nothing();
}

static void s_RX_NEXT() {
  *out_p++ = TWDR;
  s_RX_SKIP();
}

const state_fp state_table[] PROGMEM = {
  s_BUS_ERROR, // TWI_BUS_ERROR            0x00  // 00 Bus error due to an illegal START or STOP condition
  s_START,   // TWI_START                  0x08  // 01 START has been transmitted
  s_START,   // TWI_REP_START              0x10  // 02 Repeated START has been transmitted
  s_TX_NEXT, // TWI_MTX_ADR_ACK            0x18  // 03 SLA+W has been tramsmitted and ACK received
  s_ERROR,   // TWI_MTX_ADR_NACK           0x20  // 04 SLA+W has been tramsmitted and NACK received
  s_TX_NEXT, // TWI_MTX_DATA_ACK           0x28  // 05 Data byte has been tramsmitted and ACK received
  s_ERROR,   // TWI_MTX_DATA_NACK          0x30  // 06 Data byte has been tramsmitted and NACK received
  s_ERROR,   // TWI_ARB_LOST               0x38  // 07 Arbitration lost
  s_RX_SKIP, // TWI_MRX_ADR_ACK            0x40  // 08 SLA+R has been tramsmitted and ACK received
  s_ERROR,   // TWI_MRX_ADR_NACK           0x48  // 09 SLA+R has been tramsmitted and NACK received
  s_RX_NEXT, // TWI_MRX_DATA_ACK           0x50  // 0a Data byte has been received and ACK tramsmitted
  s_RX_LAST, // TWI_MRX_DATA_NACK          0x58  // 0b Data byte has been received and NACK tramsmitted
  s_raise,   // TWI_SRX_ADR_ACK            0x60  // 0c Own SLA+W has been received ACK has been returned
  s_raise,   // TWI_SRX_ADR_ACK_M_ARB_LOST 0x68  // 0d Arbitration lost in SLA+R/W as Master; own SLA+W has been received; ACK has been returned
  s_raise,   // TWI_SRX_GEN_ACK            0x70  // 0e General call address has been received; ACK has been returned
  s_raise    // TWI_SRX_GEN_ACK_M_ARB_LOST 0x78  // 0f Arbitration lost in SLA+R/W as Master; General call address has been received; ACK has been returned
  ,
  s_raise,   // TWI_SRX_ADR_DATA_ACK       0x80  // 10 Previously addressed with own SLA+W; data has been received; ACK has been returned
  s_raise,   // TWI_SRX_ADR_DATA_NACK      0x88  // 11 Previously addressed with own SLA+W; data has been received; NOT ACK has been returned
  s_raise,   // TWI_SRX_GEN_DATA_ACK       0x90  // 12 Previously addressed with general call; data has been received; ACK has been returned
  s_raise,   // TWI_SRX_GEN_DATA_NACK      0x98  // 13 Previously addressed with general call; data has been received; NOT ACK has been returned
  s_raise,   // TWI_SRX_STOP_RESTART       0xA0  // 14 A STOP condition or repeated START condition has been received while still addressed as Slave
  s_raise,   // TWI_STX_ADR_ACK            0xA8  // 15 Own SLA+R has been received; ACK has been returned
  s_raise,   // TWI_STX_ADR_ACK_M_ARB_LOST 0xB0  // 16 Arbitration lost in SLA+R/W as Master; own SLA+R has been received; ACK has been returned
  s_raise,   // TWI_STX_DATA_ACK           0xB8  // 17 Data byte in TWDR has been transmitted; ACK has been received
  s_raise,   // TWI_STX_DATA_NACK          0xC0  // 18 Data byte in TWDR has been transmitted; NOT ACK has been received
  s_raise,   // TWI_STX_DATA_ACK_LAST_BYTE 0xC8  // 19 Last data byte in TWDR has been transmitted (TWEA = ì0î); ACK has been received
  s_raise,                                       // 1a
  s_raise,                                       // 1b
  s_raise,                                       // 1c
  s_raise,                                       // 1d
  s_raise,                                       // 1e
  s_nop      // TWI_NO_STATE               0xF8  // 1f No relevant state information available; TWINT = ì0î

};

// Front ends. Interupt entry points.

#ifdef USINGTIMER
ISR(TIMER5_COMPA_vect) {
  _timeout = true;

  // reset the bus (dealing with the bus errors caused via illegal 
  // STOP conditions turned out to be a royal pain)
  TWCR = 0;
  TWCR = (1<<TWEN);
  
  // nothing else calls this when there is a timeout (s_advance is not called)
  twiQ.doneCmd();
  
  PINA |= PIN_timer_vect;
  TIMSK5 = 0;
}
#endif

void kick_isr() {
  if ( !twi_int_state() ) {
    if ( twiQ.hasCmd() ) {
      init_start();
      #ifdef USINGTIMER
      TCNT5 = 0;
      // there will likely be a pending interrupt, which we must cancel
      TIFR5 |= (1<<OCF5A); // TIFRn is in the first 32 I/O registers, meaning 
                           // this will turn into an sbi instruction
      TIMSK5 = (1<<OCIE5A);
      #endif
    }
  }

  backend();
}

ISR(TWI_vect) {
  PORTA |= PIN_twi_vect;
  TCNT2 = 0;
  SPDR = TWSR;
  unsigned char twsr = TWSR / 8;

  ( (void (*)()) (pgm_read_word(&state_table[twsr])) ) ();

  backend();
  PORTA &= ~PIN_twi_vect;
}


void twiQueue::blocking_callback(state_t *s) {
  if (_blocking_donefunc != NULL)
    _blocking_donefunc(s);

  // if we could ensure that s->state != 0, we wouldn't need the bool
  _blocking_state = s->state;
  _blocking_callback_called = true;
}

volatile bool twiQueue::_blocking_callback_called = true;
volatile char twiQueue::_blocking_state = 0;
callback_fp twiQueue::_blocking_donefunc = NULL;
