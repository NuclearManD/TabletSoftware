
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

#define VIBRATE 14
#define CTP_RESET 15

#define LCD_BRIGHTNESS 33
#define V_SENS 40

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

FT5436Touch* builtin_touchpad;

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

void hw_preinit() {

  /*analogWriteResolution(10);
  pinMode(LCD_BRIGHTNESS, OUTPUT);
  pinMode(V_SENS, INPUT);
  analogWriteFrequency(33, 20000.0f);
  set_screen_brightness(0);*/

}

void start_hw() {

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

  device_list[0] = ntios_keyboard = new USBKeyboard();
  device_list[1] = serial_0 = new USBSerialDevice();
  device_list[2] = new HWSerialDevice(&Serial1);
  device_list[3] = gps_serial = new HWSerialDevice(&Serial2);
  device_list[4] = new HWSerialDevice(&Serial3);
  device_list[5] = new HWSerialDevice(&Serial4);
  device_list[6] = new HWSerialDevice(&Serial5);
  device_list[7] = i2c0 = new TeensyI2CPort(0);
  //device_list[8] = new TeensyI2CPort(1);
  //device_list[9] = new TeensyI2CPort(2);

  _lsusb(serial_0, 0, nullptr);

  ntios_init(device_list, 8, serial_0);

  //gps_serial->setBaud(9600);

  // We need to turn devices on here, otherwise the LCD will not start right.

  /*gpio->pinMode(VIBRATE, GPIO_PIN_MODE_OUTPUT);
  gpio->pinMode(CTP_RESET, GPIO_PIN_MODE_OUTPUT);

  reset_touch_ic();
  
  set_vibrator_state(true);
  delay(100);
  set_vibrator_state(false);*/

  //add_virtual_device(new NMEARawGPS(gps_serial, "uBlox"));
  add_virtual_device(builtin_touchpad = new FT5436Touch(i2c0, nullptr, -1));
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
  i2c->endTransmission();
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
