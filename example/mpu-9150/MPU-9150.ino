#include "serial_wrapper.h"
#include "Wire/Wire.h"
#include "I2Cdev/I2Cdev.h"
#include "MPU6050/MPU6050.h"
#include "AK8975/AK8975.h"

// AD0 = low => I2C address = 0x68
MPU6050 mpu;
AK8975 mag;

#define SERIAL Serial3

parityPrinter usb = parityPrinter(&SERIAL, true);

#define LED_PIN 13
const float diode_drop = 0.85;

void initialize_ADC() {
  ADMUX = (1<<REFS0) |              // AVcc voltage reference
          (0<<ADLAR) |              // LSB of value is LSB of ADCL
          (0<<MUX0);

  // 10-bit resolution "requires an input clock frequency between 50 kHz and 200 kHz";
  // 16 MHz / 128 == 125 kHz
  ADCSRA = (1<<ADEN) |
           (7<<ADPS0);

  ADCSRA |= (1<<ADSC);
}

void setup() {
  Wire.begin();
  SERIAL.begin(57600);
  mpu.initialize();

  if (!mpu.testConnection())
    SERIAL.println("MPU6050 connection failed");

  mpu.setDLPFMode(MPU6050_DLPF_BW_42);
  mpu.setRate(1); // DLPF = 42Hz means sample rate of 1KHz; divide this by (1 + 1) = 2
  mpu.setI2CMasterModeEnabled(false);
  mpu.setI2CBypassEnabled(true);

  delay(100);
  mag.initialize();
  
  if (!mag.testConnection())
    SERIAL.println("AK8975 connection failed");

  mpu.setI2CBypassEnabled(false);
  delay(100);

  mpu.setMasterClockSpeed(MPU6050_CLOCK_DIV_258);

  mpu.setSlave4MasterDelay(4); // divide sample rate by (1 + 4) = 5
  mpu.setSlaveAddress(0, 0x80 | AK8975_ADDRESS_00);
  mpu.setSlaveRegister(0, AK8975_RA_HXL);
  mpu.setSlaveWriteMode(0, false);
  // AK8975 is little-endian, while MPU-
  mpu.setSlaveWordByteSwap(0, true);
  mpu.setSlaveWordGroupOffset(0, true);
  mpu.setSlaveDataLength(0, 6);
  mpu.setSlaveDelayEnabled(0, true);
  
  mpu.setSlaveAddress(1, AK8975_ADDRESS_00);
  mpu.setSlaveRegister(1, AK8975_RA_CNTL);
  mpu.setSlaveOutputByte(1, AK8975_MODE_SINGLE);
  mpu.setSlaveDataLength(1, 1);
  mpu.setSlaveDelayEnabled(1, true);
  
  mpu.setSlaveEnabled(0, true);
  mpu.setSlaveEnabled(1, true);

  mpu.setI2CMasterModeEnabled(true);
  
//  SERIAL.print("getXGyroOffset"); SERIAL.println(mpu.getXGyroOffset());
//  SERIAL.print("getYGyroOffset"); SERIAL.println(mpu.getYGyroOffset());
//  SERIAL.print("getZGyroOffset"); SERIAL.println(mpu.getZGyroOffset());
//  SERIAL.print("getXFineGain"); SERIAL.println(mpu.getXFineGain());
//  SERIAL.print("getYFineGain"); SERIAL.println(mpu.getYFineGain());
//  SERIAL.print("getZFineGain"); SERIAL.println(mpu.getZFineGain());
//  SERIAL.print("getXAccelOffset"); SERIAL.println(mpu.getXAccelOffset());
//  SERIAL.print("getYAccelOffset"); SERIAL.println(mpu.getYAccelOffset());
//  SERIAL.print("getYAccelOffset"); SERIAL.println(mpu.getYAccelOffset());
//  SERIAL.print("getXGyroOffsetUser"); SERIAL.println(mpu.getXGyroOffsetUser());
//  SERIAL.print("getYGyroOffsetUser"); SERIAL.println(mpu.getYGyroOffsetUser());
//  SERIAL.print("getZGyroOffsetUser"); SERIAL.println(mpu.getZGyroOffsetUser());

  delay(100);
  
  
//  SERIAL.print("A");
  
  pinMode(LED_PIN, OUTPUT);
  
  TCCR1A = 0; // Arduino code screws with this and I wasted an hour remembering it; ARGH
  TCCR1B = (1 << WGM12)| // CTC mode 
           (1 << CS11) | (1 << CS10); // Fcpu/64
  OCR1A = 1667; // 150Hz at 16MHz AVR clock, with a prescaler of 64 
  TIMSK1 = (1 << OCIE1A); // interrupt on OCR1A 
  
  initialize_ADC();
}

void loop() {
}

ISR(TIMER1_COMPA_vect) { 
//  SERIAL.print("B");

  static uint32_t counter = 1;
  static unsigned long time;	
  int16_t ax, ay, az;
  int16_t gx, gy, gz;
  int16_t mx, my, mz;
  int16_t temp;
  
  time = micros();
  
  TIMSK1 &= ~(1 << OCIE1A);
  
  sei();
  mpu.getMotion6(&ax, &ay, &az, &gx, &gy, &gz);
  temp = mpu.getTemperature();
//  SERIAL.print("C");
//  mag.getOldMeasurement(&mx, &my, &mz);
//  mag.fireSingleMeasurement();
//  SERIAL.print("D");

  usb.printStartP();
  usb.printP(time);
  usb.printSep();
  usb.printP(counter++);
  usb.printSep();
  usb.printP(temp);
  usb.printP(ax);
  usb.printP(ay);
  usb.printP(az);
  usb.printP(gx);
  usb.printP(gy);
  usb.printP(gz);
//  usb.printP(mx);
//  usb.printP(my);
//  usb.printP(mz);
  usb.printP(mpu.getExternalSensorWord(0));
  usb.printP(mpu.getExternalSensorWord(2));
  usb.printP(mpu.getExternalSensorWord(4));
//  usb.printP((uint8_t)((mpu.getSlave0Nack() ? 1 : 0) | (mpu.getSlave1Nack() ? 2 : 0)));
  usb.printP((float)ADC * (3.0 * 5 / 1024) + diode_drop);
  usb.printEOLP();
  
  ADCSRA |= (1<<ADSC);

  cli();
  TIMSK1 |= (1 << OCIE1A);
}
