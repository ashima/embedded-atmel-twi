#if defined(ARDUINO) && (ARDUINO >= 100)
  #include "Arduino.h"
#else
  #include "WProgram.h"
#endif

#include "HardwareSerial.h"


#define START_DATA 0x02
#define EOL 0x04

typedef union{
	unsigned int ui;
	int si;
	uint8_t s[2];
} twobytes;

typedef union{
	long unsigned int ul;
	long int sl;
	float sf;
	uint8_t s[4];
} fourbytes;
	
typedef union{
	double sd;
	uint8_t s[8];
} eightbytes;


class serialPrinter
{
private:
		fourbytes fb;
		eightbytes eb;
		twobytes tb;
protected:
		HardwareSerial *output;
		bool binary;
public:
	serialPrinter(HardwareSerial *out, bool bin=false) 
	{
		fb.sl=0;
		eb.sd=0;
		tb.si=0;
		binary=bin;
		output = out;
	}
	
	void print(short val)
	{
		if (binary)
		{
			output->write(val);
		}
		else
		{ output->print(val);}
		
	}
	void print(int val)
	{
		if (binary)
		{
			tb.si = val;
			for(uint8_t i=0;i<2;++i) output->write(tb.s[i]);
		}
		else
		{			output->print(val);		}
		
		printSpace();
	}
	void print(unsigned int val)
	{
		if (binary)
		{
			tb.ui = val;
			for(uint8_t i=0;i<2;++i) output->write(tb.s[i]);
		}
		else
		{			output->print(val);		}
		printSpace();
	}
	void print(long val)
	{
		if (binary)
		{
			fb.sl = val;
			for(uint8_t i=0;i<4;++i) output->write(fb.s[i]);
		}
		else
		{			output->print(val);		}

		printSpace();
	}
	void print(unsigned long val)
	{
		if (binary)
		{
			fb.ul = val;
			for(uint8_t i=0;i<4;++i) output->write(fb.s[i]);
		}
		else
		{			output->print(val);		}

		printSpace();
	}

	void print(float val)
	{
		if (binary)
		{
			fb.sf = val;
			for(uint8_t i=0;i<4;++i) output->write(fb.s[i]);
		}
		else
		{			output->print(val);		}
		
		printSpace();
	}
	void print(double val)
	{
		if (binary)
		{
			eb.sd = val;
			for(uint8_t i=0;i<8;++i) output->write(eb.s[i]);
		}
		else
		{			output->print(val);		}
		
		printSpace();
	}

	void printStart(void)
	{
		if (binary)
			output->write(START_DATA);
	}
	
		
	void printSep(void)
	{
		if (!binary) 
			output->print(",");
		printSpace();
	}
	void printSpace(void)
	{
		if (!binary)
			output->print(" ");
	}
	void printEOL(void)
	{
		if (binary)
			output->write (EOL);
		else
			output->println();
	}
};

class parityPrinter : public serialPrinter {
public:
  parityPrinter(HardwareSerial *out, bool bin=false)
    : serialPrinter(out, bin)
  {}

  template <class T> 
  void printP(const T val) {
    if (binary) {
      byte *p = (uint8_t*)&val;
      
      for (int i = sizeof(T); i > 0; i--) {
        _parity ^= *p;
        output->write(*p++);
      }
    }
    else
      this->print(val);
  }
    
  void printStartP() {
    this->printStart();
    _parity = 0;
  }
  void printEOLP() {
    if (binary)
      output->write(_parity); // _not_ this->printP
    else
      this->printEOL();
  }
private:
  byte _parity;
};
