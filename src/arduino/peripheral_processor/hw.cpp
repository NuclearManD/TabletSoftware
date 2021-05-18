
#include <Arduino.h>
#include "sdios.h"
#include "USBHost_t36.h"
#include <Wire.h>
#include <util/atomic.h>

#include "TeensyThreads.h"
#include "src/ntios/ntios.h"
#include "src/ntios/drivers/ctp_drivers.h"
#include "hw.h"

#define BUFFER_SZ 1024

#define VIBRATE 4
#define CTP_RESET 5
#define CTP_INT 6
#define USR_BTN 2

#define LCD_BRIGHTNESS 3
#define V_SENS A8

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
USBKeyboard* ntios_keyboard;
CapacitiveTouchDevice* builtin_touchpad;


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
  uint8_t _pins[4] = {
    CTP_RESET, 
    CTP_INT,
    VIBRATE,
    USR_BTN
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
  I2CBusDevice* i2c0;
  GPIODevice* gpio_dev;

  device_list[0] = ntios_keyboard = new USBKeyboard();
  device_list[1] = serial_0 = new USBSerialDevice();
  device_list[2] = new HWSerialDevice(&Serial1);
  device_list[3] = gps_serial = new HWSerialDevice(&Serial2);
  device_list[4] = new HWSerialDevice(&Serial3);
  device_list[5] = new HWSerialDevice(&Serial4);
  device_list[6] = new HWSerialDevice(&Serial5);
  device_list[7] = gpio_dev = new TeensyGPIO(_pins, 4);
  device_list[8] = i2c0 = new TeensyI2CPort(0);
  //device_list[8] = new TeensyI2CPort(1);
  //device_list[9] = new TeensyI2CPort(2);

  _lsusb(serial_0, 0, nullptr);

  ntios_init(device_list, 9, serial_0);

  //gps_serial->setBaud(9600);
  
  set_vibrator_state(true);
  bootloader_delay(100);
  set_vibrator_state(false);

  //add_virtual_device(new NMEARawGPS(gps_serial, "uBlox"));

  add_virtual_device(builtin_touchpad = new GT911Touch(i2c0, gpio_dev, 0, 1));
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
