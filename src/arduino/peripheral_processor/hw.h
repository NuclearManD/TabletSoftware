
#ifndef HW_H
#define HW_H

#include "TeensyThreads.h"
#include "Arduino.h"
#include <Wire.h>

#include "src/ntios/drivers.h"
#include "src/ntios/ntios.h"

class NBrainMutex: public BootloaderMutex {
private:
  Threads::Mutex mutex;
public:
  void lock();
  void unlock();
};

class USBKeyboard: public KeyboardDevice {
public:
  Threads::Mutex callback_lock;
  void (*onpress)(KeyboardDevice*, int) = NULL;
  void (*onrelease)(KeyboardDevice*, int) = NULL;

  // Stream implementation
  int read();
  int available();
  int peek();

  const char* getName();
  void update();

  int setOnPress(void (*event)(KeyboardDevice*, int));
  int setOnRelease(void (*event)(KeyboardDevice*, int));
};

class HWSerialDevice: public SerialDevice {
private:
  HardwareSerial* stream;
  Threads::Mutex lock;
public:
  const char* getName() { return "HW Serial Port"; }

  HWSerialDevice(HardwareSerial* s) { stream = s; }

  size_t write(uint8_t val);
  void flush();

  int read();
  int available();
  int peek();

  void setBaud(uint32_t baud);
};

class USBSerialDevice: public SerialDevice {
private:
  Threads::Mutex lock;
public:
  const char* getName() { return "USB Serial Port"; }

  size_t write(uint8_t val);
  void flush();

  int read();
  int available();
  int peek();

  void setBaud(uint32_t baud);
};

class TeensyPWMPin: public PWMPin {
private:
  char name[7];
  int pinnum;
  double freq;
public:
  const char* getName() { return name; };

  TeensyPWMPin(int pin);

  void setMicros(long us);
  void setDuty(float duty);
  void setFreq(double freq);
};

class TeensyI2CPort: public I2CBusDevice {
private:
  char name[6];
  int busnum;
  Threads::Mutex d_lock;
  TwoWire* i2c;
public:
  const char* getName() { return name; };

  TeensyI2CPort(int busnum);

  bool read(int address, int size, char* data);
  bool write(int address, int size, const char* data);
  void lock();
  void unlock();
};

StreamDevice* get_serial_0();

void start_hw();
void hw_preinit();

void set_vibrator_state(bool state);
void reset_touch_ic();
void set_screen_brightness(float percent);
float get_battery_voltage();
bool read_user_button();

#endif
