

char hexc2int(char c) {
  if ('0' <= c && c <= '9')
    return c - '0';
  else if ('a' <= c && c <= 'f')
    return c - 'a' + 10;
  else if ('A' <= c && c <= 'F')
    return c - 'A' + 10;
  else
    return -1;
}

// reads a sequence of bytes, which can be formatted either as
// 01234567...
// or
// 0123 4567
// or
// 1 23 45 67
// but not:
// 123 4567
// (the 123 would be interpreted as 12 3; the 4567 would be interpreted as 45 67)
// Note that the first byte consumed is the first entry of the array, etc.
// Advances p to the first non-space, non-hexadecimal character.
uint8_t parse_hex_array(char *&p, uint8_t *buffer, size_t max_count) {
  if (max_count == 0)
    return 0;
  
  uint8_t i = 0;
  uint8_t which_nibble = 0;

  for(; *p != '\0'; p++) {
    char hex = hexc2int(*p);
    
    if (hex >= 0) {
      if (which_nibble == 0) {
        buffer[i] = hex;
        which_nibble++;
      }
      else {
        buffer[i] = (buffer[i] * 16) + hex;
        which_nibble = 0;
        i++;
      }
    }
    else if (*p == ' ') {
      if (which_nibble == 1) {
        which_nibble = 0;
        i++;
      }
    }
    else
      break;

    if (i == max_count)
      break;
  }
  
  return i + which_nibble;
}

// consumes all hexadecimal characters and the first non-hexadecimal character, returning the
// 16-bit value defined by the last >= 4 characters read
// Advances p to the first non-hexadecimal character, skipping the next character if it is a space.
uint16_t parse_hex16(char *&p) {
  uint16_t v = 0;

  // the while (true) version makes it easier to eat the very next serial character, which is
  // desirable for simple debugging purposes
  //for (uint8_t i = 4; i > 0; i--)
  for(; *p != '\0'; p++) {
    char n = hexc2int(*p);
    
    if (n >= 0)
      v = (v * 16) + n;
    else
      break;
  }
  
  if (*p == ' ')
    p++;
  
  return v;    
}

// consumes all hexadecimal characters and the first non-hexadecimal character, returning the
// 8-bit value defined by the last >= 2 characters read
// Advances p to the first non-hexadecimal character, skipping the next character if it is a space.
uint8_t parse_hex8(char *&p) {
  uint8_t v = 0;

  // the while (true) version makes it easier to eat the very next serial character, which is
  // desirable for simple debugging purposes
  //for (uint8_t i = 2; i > 0; i--)
  for(; *p != '\0'; p++) {
    char n = hexc2int(*p);
    
    if (n >= 0)
      v = (v * 16) + n;
    else
      break;
  }

  if (*p == ' ')
    p++;
  
  return v;    
}

// If (*p == c), returns true and advances p one, another if the next character is a space.
// Otherwise, returns false and does not alter p.
bool consume_char_if(char *&p, char c) {
  if (*p != c)
    return false;

  p++;
  
  if (*p == ' ')
    p++;
  
  return true;
}