#if defined(ARDUINO) && (ARDUINO >= 100)
  #include "Arduino.h"
#else
  #include "WProgram.h"
#endif

#include "HardwareSerial.h"

class SerialPrinter {
protected:
		HardwareSerial *output;

public:
  enum ReadLineError { Success = 0, BufferOverflow = -1, NewlineTimeout = -2 };

	SerialPrinter(HardwareSerial *out) {
		output = out;
	}
	
	template<typename T>
	inline void write(const T val) {
		output->write(val);
	}

	template<typename T>
	inline void print(const T val) {
		output->print(val);
	}
	
	template<typename T>
	inline void println(const T val) {
	  output->println(val);
	}

	inline void println() {
	  output->println();
	}
	
	// why C++ won't match to the proper sized int type, I don't know
	void print_hex(const uint16_t n) {
    if (n > 0xFF)
      print_hex8((uint8_t)(n >> 8));
      
    print_hex8((uint8_t)(n & 0xFF));
	}

	void print_hex8(const uint8_t n) {
    if (n < 0x10)
      output->write('0');
    
    output->print(n, HEX);
	}
	
  ReadLineError read_line(char* buffer, size_t size, unsigned long timeout_ms, bool timeout_warning) {
    unsigned int idx = 0; 
    unsigned long time = millis() + timeout_ms;
  
    time += 2; // ensure the - 1 below doesn't underflow
    
    while (millis() < time) {
      if (!output->available())
        continue;
      
      // we've got a character, so give the system 1ms to get another
      // (yes, this kinda breaks the idea of a timeout, but I don't care for now)
      time += 2;
      
      char c = output->read();
      
      if (c == '\r' || c == '\n') {
//        if (!output->available())
//          delayMicroseconds(500);
//        
//        c = output->peek();
//        
//        if (c == '\r' || c == '\n')
//          output->read();
        
        break;
      }
      if (idx == size - 1) {
        buffer[idx] = '\0';
        output->print("ERROR: serial receive overflow; so far: ");
        output->println(buffer);
        return BufferOverflow;
      }
      
      buffer[idx++] = c;
    }
  
    if (millis() >= time) {
      buffer[idx] = '\0';
      if (timeout_warning && idx > 0) {
        output->print("ERROR: serial receive timeout; so far: ");
        output->println(buffer);
      }
      return NewlineTimeout;
    }
    
    buffer[idx] = '\0';
    
    return Success;
  }
  
  ReadLineError read_line(char* buffer, size_t size, unsigned int timeout_ms) {
    return read_line(buffer, size, timeout_ms, true);
  }
};
