
#ifndef HW_H
#define HW_H

#include "TeensyThreads.h"
#include "Arduino.h"
#include <Wire.h>
#include "Adafruit_GFX.h"
#include "Adafruit_RA8875.h"

#include "src/ntios/drivers.h"
#include "src/ntios/ntios.h"
#include "src/ntios/drivers/graphics/graphics.h"

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

class RA8875Graphics: public GraphicsDisplayDevice {
private:
  Adafruit_RA8875* tft;

public:

  RA8875Graphics(int cs, int rst);

  void setBrightness(double brightness);

  // Print implementation
  using Print::write; // pull in write(str) and write(buf, size) from Print

  const char* getName();
  void update() {}

  void scrollDownPixels(uint32_t pixels);

  void clearScreen(uint16_t color = 0);
  uint32_t getWidth() { return 800; }
  uint32_t getHeight() { return 480; }

  void setPixel(int x, int y, uint16_t color);
  void fillRect(int x1, int y1, int x2, int y2, uint16_t color);
  void drawBitmap16(int x, int y, int w, int h, uint16_t* data);

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
  int readSome(int address, int size, char* data);
  bool write(int address, int size, const char* data);
  void lock();
  void unlock();
};

class TeensySPIPort: public SPIBusDevice {
private:
  char* name = "SPI Bus";
  Threads::Mutex d_lock;
public:
  const char* getName() { return name; };

  TeensySPIPort();

  void exchange(int size, const char* out, char* in);
  void exchange(int size, char out, char* in);
  void lock();
  void unlock();
};

class TeensyGPIO: public GPIODevice {
public:
  TeensyGPIO(uint8_t* pins, int n_pins);

  virtual void update() {};
  virtual const char* getName() { return "Teensy GPIO"; }

  virtual int pinCount() { return pin_count; }
  virtual bool pinMode(int pin, int mode);
  virtual bool readPin(int pin);
  virtual bool writePin(int pin, bool state);

private:
  uint8_t pin_arr[32];
  int pin_count;
};

StreamDevice* get_serial_0();

void start_hw();
void hw_preinit();

void set_vibrator_state(bool state);
void set_screen_brightness(float percent);
float get_battery_voltage();
bool read_user_button();

#endif
