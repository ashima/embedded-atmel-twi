#ifndef TWIHelper_H
#define TWIHelper_H

#include "TWIMaster.h"
#include <stddef.h>

#define I2C_WHO_ADDRESS 0

uint8_t twi_who(uint8_t twi_addr) {
  char p[1] = { I2C_WHO_ADDRESS };
  return twiQ.enqueue_wb(twi_addr, p, sizeof(p), NULL) &&
         //delay(100); // this exists in AeroQuad code -- maybe some chips need this delay?
         twiQ.enqueue_rb(twi_addr, p, sizeof(p), NULL)
    ? p[0]
    : 0;
}

uint8_t twi_read8(uint8_t twi_addr, uint8_t mem_addr) {
  char p[] = { mem_addr };
  return twiQ.enqueue_wb(twi_addr, p, sizeof(p), NULL) &&
         twiQ.enqueue_rb(twi_addr, p, sizeof(p), NULL)
    ? p[0]
    : 0;
}

void twi_write8(uint8_t twi_addr, uint8_t mem_addr, uint8_t value) {
  char p[] = { mem_addr, value };
  
  twiQ.enqueue_wb(twi_addr, p, sizeof(p), NULL);
}

#endif