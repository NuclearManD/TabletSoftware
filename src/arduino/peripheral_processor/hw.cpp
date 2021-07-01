
#include <Arduino.h>
#include "sdios.h"
#include "USBHost_t36.h"
#include <Wire.h>
#include <util/atomic.h>
#include "Adafruit_GFX.h"
#include "Adafruit_RA8875.h"

#include "TeensyThreads.h"
#include "src/ntios/ntios.h"
#include "src/ntios/drivers/ctp_drivers.h"
//#include "src/ntios/drivers/graphics/ra8875.h"
#include "hw.h"

#define BUFFER_SZ 1024

#define VIBRATE 4
#define CTP_RESET 5
#define CTP_INT 6
#define USR_BTN 2

#define LCD_BRIGHTNESS 3
#define V_SENS A8

#define RA8875_CS 10
#define RA8875_RESET 3

USBHost myusb;
USBHub hub1(myusb);
USBHub hub2(myusb);
KeyboardController keyboard1(myusb);
MouseController mouse1(myusb);
USBHIDParser hid0(myusb);
USBHIDParser hid1(myusb);
MIDIDevice midi1(myusb);

USBDriver *drivers[] = {&hub1, &hub2, &keyboard1, &hid0, &hid1};

#define CNT_DEVICES (sizeof(drivers)/sizeof(drivers[0]))

const char * driver_names[CNT_DEVICES] = {"hub0", "hub1", "kbd0", "hid0", "hid1"};

volatile int keyboard_buffer[BUFFER_SZ];
volatile int keyboard_buffer_size = 0;
volatile int keyboard_buffer_idx = 0;

Device* device_list[32];

SerialDevice* serial_0;
SerialDevice* pi_serial;
USBKeyboard* ntios_keyboard;
CapacitiveTouchDevice* builtin_touchpad;
RA8875Graphics* builtin_display;

Threads::Mutex spi_lock;

void key_hook(int key) {
  switch (key) {
    case 127:
      key = 8;
      break;
    default:
      break;
  }
  keyboard_buffer[keyboard_buffer_size] = key;
  keyboard_buffer_size = (keyboard_buffer_size + 1) % BUFFER_SZ;

  ntios_keyboard->callback_lock.lock();
  if (ntios_keyboard->onpress != NULL)
    ntios_keyboard->onpress(ntios_keyboard, key);
  ntios_keyboard->callback_lock.unlock();
}

void key_release_hook(int key) {
  switch (key) {
    case 127:
      key = 8;
      break;
    default:
      break;
  }

  ntios_keyboard->callback_lock.lock();
  if (ntios_keyboard->onrelease != NULL)
    ntios_keyboard->onrelease(ntios_keyboard, key);
  ntios_keyboard->callback_lock.unlock();
}

void hid_extra_hook(uint32_t top, uint16_t key) {
  
}

int _lsusb(StreamDevice* io, int ac, char** av) {
  io->println("\n\nUSB Devices:");
  for (int i = 0; i < CNT_DEVICES; i++) {
    if (!(*drivers[i]))
      continue;
    io->print("  ");
    io->print(driver_names[i]);
    const uint8_t *psz = drivers[i]->manufacturer();
    if (psz && *psz) io->printf("  manufacturer: %s\n", psz);
    psz = drivers[i]->product();
    if (psz && *psz) io->printf("  product: %s\n", psz);
    psz = drivers[i]->serialNumber();
    if (psz && *psz) io->printf("  Serial: %s\n", psz);
  }
}


void set_vibrator_state(bool state) {
  digitalWrite(VIBRATE, state);
}

void set_screen_brightness(float percent) {
  if (percent < 1)
    set_min_cpu_hz(MIN_CLOCK_SPEED);
  else
    set_min_cpu_hz(MAX_CLOCK_SPEED / 3);
  analogWrite(LCD_BRIGHTNESS, (int)(percent / 100 * 1024));
}

float get_battery_voltage() {
  float pin_voltage = (analogRead(V_SENS) / 1024.0f * 3.3);
  return pin_voltage * 2;
}

bool read_user_button() {
  return !digitalRead(USR_BTN);
}

void hw_preinit() {
  analogWriteResolution(10);
  pinMode(LCD_BRIGHTNESS, OUTPUT);
  pinMode(CTP_RESET, OUTPUT);
  pinMode(VIBRATE, OUTPUT);
  pinMode(V_SENS, INPUT);
  pinMode(USR_BTN, INPUT_PULLUP);
  analogWriteFrequency(LCD_BRIGHTNESS, 20000.0f);
  set_screen_brightness(0);
  set_vibrator_state(false);

  // Our connections are very noisy...
  Wire.setClock(100000);
}

void start_hw() {
  uint8_t _pins[6] = {
    CTP_RESET, 
    CTP_INT,
    VIBRATE,
    USR_BTN,
    RA8875_CS,
    RA8875_RESET
  };

  myusb.begin();
  keyboard1.attachPress(key_hook);
  keyboard1.attachRelease(key_release_hook);
  keyboard1.attachExtrasPress(hid_extra_hook);
  keyboard1.attachExtrasRelease(hid_extra_hook);

  for (int i = 0; i < 100; i++) {
    myusb.Task();
    delay(5);
  }

  HWSerialDevice* gps_serial;
  I2CBusDevice* i2c0 = new TeensyI2CPort(0);
  SPIBusDevice* spi = new TeensySPIPort();
  GPIODevice* gpio_dev;

  device_list[0] = ntios_keyboard = new USBKeyboard();
  device_list[1] = serial_0 = new USBSerialDevice();
  device_list[2] = pi_serial = new HWSerialDevice(&Serial1);
  device_list[3] = gps_serial = new HWSerialDevice(&Serial2);
  device_list[4] = new HWSerialDevice(&Serial3);
  device_list[5] = new HWSerialDevice(&Serial4);
  device_list[6] = new HWSerialDevice(&Serial5);
  device_list[7] = gpio_dev = new TeensyGPIO(_pins, 6);
  device_list[8] = i2c0;
  device_list[9] = spi;

  _lsusb(serial_0, 0, nullptr);

  ntios_init(device_list, 10, serial_0);

  //gps_serial->setBaud(9600);

  pi_serial->setBaud(115200);
  
  set_vibrator_state(true);
  bootloader_delay(100);
  set_vibrator_state(false);

  //add_virtual_device(new NMEARawGPS(gps_serial, "uBlox"));

  add_virtual_device(builtin_touchpad = new GT911Touch(i2c0, gpio_dev, 0, 1));
  //add_virtual_device(builtin_display = RA8875::create800x480(spi, gpio_dev, 5, 4));
  add_virtual_device(builtin_display = new RA8875Graphics(RA8875_CS, RA8875_RESET));

  //builtin_display->displayOn(true);
  builtin_display->setBrightness(20);
  builtin_display->clearScreen(0);
  builtin_display->fillRect(100, 100, 200, 200, 1970);
}


int USBKeyboard::read() {
  if (keyboard_buffer_idx == keyboard_buffer_size)
    return 0;
  int val = keyboard_buffer[keyboard_buffer_idx];
  keyboard_buffer_idx = (keyboard_buffer_idx + 1) % BUFFER_SZ;
  return val;
}

int USBKeyboard::available() {
  return keyboard_buffer_size - keyboard_buffer_idx;
}

int USBKeyboard::peek() {
  return keyboard_buffer[keyboard_buffer_idx];
}

const char* USBKeyboard::getName() {
  return "USB Keyboard";
}

void USBKeyboard::update() {
  myusb.Task();
}

int USBKeyboard::setOnPress(void (*event)(KeyboardDevice*, int)) {
  callback_lock.lock();
  onpress = event;
  callback_lock.unlock();
  return 0;
}

int USBKeyboard::setOnRelease(void (*event)(KeyboardDevice*, int)) {
  callback_lock.lock();
  onrelease = event;
  callback_lock.unlock();
  return 0;
}

size_t HWSerialDevice::write(uint8_t val) {
  size_t n;
  ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
    n = stream->write(val);
  }
  return n;
}

void HWSerialDevice::flush() {
  stream->flush();
}

int HWSerialDevice::read() {
  int val;
  ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
    val = stream->read();
  }
  return val;
}
int HWSerialDevice::available() {
  int val;
  ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
    val = stream->available();
  }
  return val;
}
int HWSerialDevice::peek() {
  int val;
  ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
    val = stream->peek();
  }
  return val;
}

void HWSerialDevice::setBaud(uint32_t baud) {
  ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
    stream->end();
    stream->begin(baud);
  }
}


size_t USBSerialDevice::write(uint8_t val) {
  size_t n;
  ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
   n = Serial.write(val);
  }
  return n;
}
void USBSerialDevice::flush() {
  ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
    Serial.flush();
  }
}

int USBSerialDevice::read() {
  int val;
  ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
    val = Serial.read();
  }
  return val;
}
int USBSerialDevice::available() {
  int val;
  ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
    val = Serial.available();
  }
  return val;
}
int USBSerialDevice::peek() {
  int val;
  ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
    val = Serial.peek();
  }
  return val;
}

void USBSerialDevice::setBaud(uint32_t baud) {
  ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
    Serial.end();
    Serial.begin(baud);
  }
}

RA8875Graphics::RA8875Graphics(int cs, int rst) {
  spi_lock.lock();
  tft = new Adafruit_RA8875(cs, rst);
  tft->begin(RA8875_800x480);
  tft->displayOn(true);
  tft->GPIOX(true);      // Enable TFT - display enable tied to GPIOX
  tft->PWM1config(true, RA8875_PWM_CLK_DIV1024); // PWM output for backlight
  tft->fillScreen(0);
  tft->setRotation(1);
  tft->setTextColor(0xFFFF, 0);
  spi_lock.unlock();
}

void RA8875Graphics::setBrightness(double brightness) {
  tft->PWM1out((int)(brightness * 2.55));
}

size_t RA8875Graphics::write(uint8_t val) {
  data_lock.lock();
  if (buflen >= 1023) {
    data_lock.unlock();
    flush();
    data_lock.lock();
  }
  buffer[buflen++] = val;
  data_lock.unlock();
  return 1;
}

void RA8875Graphics::flush() {
  while (buflen > 0)
    threads.yield();
}

void RA8875Graphics::renderLines(int nlines){
  spi_lock.lock();
  tft->setCursor(0, 0);
  for (int y = 0; y < nlines; y++) {
    tft->println(getLine(y));
  }
  spi_lock.unlock();
}

void RA8875Graphics::clearScreen(uint16_t color) {
  flush();
  data_lock.lock();
  spi_lock.lock();
  if (color == 0) {
    tft->setTextColor(0);
    spi_lock.unlock();
    renderLines();
    spi_lock.lock();
  } else {
    tft->fillScreen(color);
  }
  tft->setTextColor(0xFFFF, 0);
  tft->setCursor(0, 0);
  spi_lock.unlock();
  lineno = 0;
  lineoff = 0;
  column = 0;
  buflen = 0;
  for (int i = 0; i < 40; i++) {
    for (int j = 0; j < 80; j++)
      lines[i][j] = ' ';
    lines[i][80] = 0;
  }
  data_lock.unlock();
}

int RA8875Graphics::setTextCursor(int x, int y) {
  if (x >= getTextColumns() || y >= getTextLines())
    return ERR_OUT_OF_BOUNDS;
  flush();
  data_lock.lock();
  column = x;
  lineno = y;
  x *= 6 * textsize;
  y *= 8 * textsize;
  data_lock.unlock();
  spi_lock.lock();
  tft->setCursor(x, y);
  spi_lock.unlock();

  return 0;
}

int RA8875Graphics::getTextLines() {
  return 30 / textsize;
}

int RA8875Graphics::getTextColumns() {
  return 100 / textsize;
}

void RA8875Graphics::scrollDown(int lines) {
  spi_lock.lock();
  tft->setTextColor(0);
  spi_lock.unlock();
  renderLines();
  for (int i = 0; i < lines; i++) {
    memset(getLine(i), ' ', 80); // clear top line
    lineoff++; // go down a line
    lineno--; // mark that a line has been deleted (top line is gone)
  }
  spi_lock.lock();
  tft->setTextColor(0xFFFF); // faster if we don't render background color
  spi_lock.unlock();
  renderLines(40 - lines);
  spi_lock.lock();
  tft->setTextColor(0xFFFF, 0); // need backround color now so we can delete old chars
  spi_lock.unlock();
}

void RA8875Graphics::autoScrollDown() {
  // Count the number of newlines so that we scroll less times
  // Each scroll is extremely expensive
  // Make sure the data mutex is locked before entering this function
  // Will scroll down at least once.

  int num_newlines = 0;
  for (int i = 0; i < buflen; i++) {
    if (buffer[i] == '\n')
      num_newlines++;
  }

  scrollDown(max(num_newlines, 1));
}

void RA8875Graphics::update() {
  // screen characters are 40x80 when character size is 1
  char c;

  if (buflen == 0) return;

  // Use at most 30ms to update at a time, to avoid blocking other drivers
  long timeout = millis() + 15;

  data_lock.lock();
  //Serial.printf("[[[ %s ]]]\n", buffer);
  for (int i = 0; i < buflen; i++) {
    c = buffer[i];
    if (c == 8) {
      // backspace
      if (column > 0)
        column--;
      getLine(lineno)[column] = ' ';
      int x = column * 6 * textsize;
      int y = lineno * 8 * textsize;
      spi_lock.lock();
      tft->fillRect(x, y, 6 * textsize, 8 * textsize, 0);
      tft->setCursor(x, y);
      spi_lock.unlock();
    } else if (c == 9) {
      int sx = column * 6 * textsize;
      int sy = lineno * 8 * textsize;
      while (column < 80 / textsize) {
        getLine(lineno)[column++] = ' ';
        if (column % 4 == 0)
          break;
      }
      if (column >= 80 / textsize) {
        lineno++;
        if (lineno >= 40 / textsize)
          autoScrollDown();
        else {
          spi_lock.lock();
          tft->println();
          spi_lock.unlock();
        }
        column = 0;
      } else {
        int x = column * 6 * textsize;
        int y = lineno * 8 * textsize;
        spi_lock.lock();
        tft->setCursor(x, y);
        tft->fillRect(sx, sy, x - sx, 8 * textsize, 0);
        spi_lock.unlock();
      }
    } else if (c == '\n') {
      lineno++;
      if (lineno >= 40 / textsize) {
        autoScrollDown();
      } else {
        spi_lock.lock();
        tft->println();
        spi_lock.unlock();
      }
      column = 0;
    } else if (c == '\r') ;
    else if (c == ' ') {
      getLine(lineno)[column++] = ' ';
      int x = column * 6 * textsize;
      int y = lineno * 8 * textsize;
      spi_lock.lock();
      tft->fillRect(x, y, 6 * textsize, 8 * textsize, 0);
      tft->write(c);
      spi_lock.unlock();
      if (column >= 80 / textsize) {
        column = 0;
        lineno++;
        if (lineno >= 40 / textsize) {
          autoScrollDown();
        }
      }
    } else {
      if (c == 0) c = ' ';
      getLine(lineno)[column++] = c;
      spi_lock.lock();
      tft->write(c);
      spi_lock.unlock();
      if (column >= 80 / textsize) {
        column = 0;
        lineno++;
        if (lineno >= 40 / textsize) {
          autoScrollDown();
        }
      }
    }

    if (timeout < millis()) {
      buflen -= i + 1;
      memmove(buffer, &(buffer[i + 1]), buflen);
      buffer[buflen] = 0; // for easier printing
      data_lock.unlock();
      return;
    }
  }
  buflen = 0;
  data_lock.unlock();
}

const char* RA8875Graphics::getName() {
  return "RA8875 TFT LCD";
}

TeensyPWMPin::TeensyPWMPin(int pin) {
  pinnum = pin;
  strcpy(name, "Pin ");
  name[4] = (pin / 10) + '0';
  name[5] = (pin % 10) + '0';
  name[6] = 0;

  pinMode(pin, OUTPUT);
}

void TeensyPWMPin::setMicros(long us) {
  // us = duty * 1000000 / freq
  // duty = us * freq / 1000000

  // 244.2 = 1000000 / 4095
  // 4095 is the maximum PWM duty cycle value understood by Teensy API
  // We translate the duty cycle from a float between 0 and 1 to an int between 0 and 4095

  analogWrite(pinnum, us * freq / 244.2);
}

void TeensyPWMPin::setDuty(float duty) {
  analogWrite(pinnum, duty * 4095);
}

void TeensyPWMPin::setFreq(double freq) {
  analogWriteFrequency(pinnum, freq);
  this->freq = freq;
}



TeensyI2CPort::TeensyI2CPort(int busnum) : busnum(busnum){
  i2c = &Wire;
  if (busnum == 1)
    i2c = &Wire1;
  if (busnum == 2)
    i2c = &Wire2;
  i2c->begin();
  strcpy(name, "I2C  ");
  name[4] = busnum + '0';
}

bool TeensyI2CPort::read(int address, int size, char* data) {
  //Serial.printf("%02X read << ", address);
  //Serial.flush();
  int bytes;
  bytes = i2c->requestFrom(address, size);
  for (int i = 0; i < bytes; i++) {
    while (!i2c->available()) {
      threads.yield();
    }
    data[i] = i2c->read();
    //Serial.printf("%02X,", data[i]);
    //Serial.flush();
  }
  //Serial.println();
  //Serial.flush();
  return bytes >= size;
}

int TeensyI2CPort::readSome(int address, int size, char* data) {
  int i = 0;

  if (size > 31)
    size = 31;

  i2c->requestFrom(address, size);
  while (i2c->available()) {
    if (i < size)
      data[i] = i2c->read();
    i++;
  }

  return i;
}

bool TeensyI2CPort::write(int address, int size, const char* data) {
  //Serial.print(address, HEX);
  //Serial.print(" : ");
  //Serial.flush();
  i2c->beginTransmission(address);
  for (int i = 0; i < size; i++) {
    i2c->write(data[i]);
    //Serial.printf("%02X,", data[i]);
    //Serial.flush();
  }
  //Serial.println();
  //Serial.flush();
  int status = i2c->endTransmission();
  if (status != 0) {
    Serial.printf("I2C write errno %i\n", status);
    return false;
  }
  return true;
}

void TeensyI2CPort::lock() {
  //Serial.printf("Locking %s\n", name);
  //Serial.flush();
  d_lock.lock();
}

void TeensyI2CPort::unlock() {
  //Serial.printf("Unlocking %s\n", name);
  //Serial.flush();
  d_lock.unlock();
}

TeensySPIPort::TeensySPIPort() {
  SPI.begin();
}

void TeensySPIPort::exchange(int size, const char* out, char* in) {
  SPI.beginTransaction(SPISettings(4000000, MSBFIRST, SPI_MODE0));
  if (in != nullptr) {
    for (int i = 0; i < size; i++)
      in[i] = SPI.transfer(out[i]);
  } else {
    for (int i = 0; i < size; i++)
      SPI.transfer(out[i]);
  }
  SPI.endTransaction();
}

void TeensySPIPort::exchange(int size, char out, char* in) {
  SPI.beginTransaction(SPISettings(4000000, MSBFIRST, SPI_MODE0));
  if (in != nullptr) {
    for (int i = 0; i < size; i++)
      in[i] = SPI.transfer(out);
  } else {
    for (int i = 0; i < size; i++)
      SPI.transfer(out);
  }
  SPI.endTransaction();
}

void TeensySPIPort::lock() {
  d_lock.lock();
}

void TeensySPIPort::unlock() {
  d_lock.unlock();
}

TeensyGPIO::TeensyGPIO(uint8_t* pins, int n_pins) {
  memcpy(pin_arr, pins, n_pins);
  pin_count = n_pins;
}

bool TeensyGPIO::pinMode(int pin, int mode) {
  if (pin < 0 || pin >= pin_count)
    return false;

  if (mode == GPIO_PIN_MODE_INPUT)
    pinMode(pin_arr[pin], INPUT);
  else if (mode == GPIO_PIN_MODE_OUTPUT)
    pinMode(pin_arr[pin], OUTPUT);
  else if (mode == GPIO_PIN_MODE_INPUT_PULLUP)
    pinMode(pin_arr[pin], INPUT_PULLUP);
  else if (mode == GPIO_PIN_MODE_INPUT_PULLDOWN)
    pinMode(pin_arr[pin], INPUT_PULLDOWN);
  else if (mode == GPIO_PIN_MODE_HIGH_Z)
    pinMode(pin_arr[pin], INPUT);
  else
    return false;

  return true;
}

bool TeensyGPIO::readPin(int pin) {
  if (pin < 0 || pin >= pin_count)
    return false;

  return digitalRead(pin_arr[pin]);
}

bool TeensyGPIO::writePin(int pin, bool state) {
  if (pin < 0 || pin >= pin_count)
    return false;

  digitalWrite(pin_arr[pin], state);
  return true;
}



StreamDevice* get_serial_0() {
  return serial_0;
}

void NBrainMutex::lock() {
  mutex.lock();
}

void NBrainMutex::unlock() {
  mutex.unlock();
}

BootloaderMutex* bootloader_make_mutex() {
  return new NBrainMutex();
}
