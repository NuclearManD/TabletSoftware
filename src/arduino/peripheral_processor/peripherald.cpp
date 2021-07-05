
#include "hw.h"
#include "ntios_logo.h"
#include "peripherald.h"
#include "Arduino.h"

#include "src/ntios/drivers/ctp_drivers.h"

#define CMD_REQUEST_ACKNOWLEDGE 0x00
#define CMD_SET_CURSOR_PIXELS   0x01
#define CMD_WRITE_TEXT          0x02
#define CMD_DRAW_PIXEL          0x03
#define CMD_FILL_RECT           0x04
#define CMD_DRAW_RECT           0x05
#define CMD_SET_TEXT_COLOR      0x06
#define CMD_WRITE_VRAM          0x07
#define CMD_RENDER_BITMAP       0x08
#define CMD_SELECT_DISPLAY      0x09
#define CMD_DRAW_PALETTE_IMAGE  0x0A
#define CMD_FILL_DISPLAY        0x0B

#define EVENT_ON_CTP_CHANGE     0x01
#define EVENT_BATTERY_DATA      0x02
#define EVENT_ON_USB_KEYPRESS   0x03

#define EVENT_ACKNOWLEDGE       0xFF

#define PERIPHERALD_SERIAL_BUFFER_SIZE  1024
#define PERIPHERALD_VRAM_SIZE           1024 * 256

#define debug(...) Serial.printf(__VA_ARGS__)


extern SerialDevice* pi_serial;
extern USBKeyboard* ntios_keyboard;
extern CapacitiveTouchDevice* builtin_touchpad;
extern RA8875Graphics* builtin_display;


uint16_t* vram;
uint8_t _peripherald_serial_buffer[PERIPHERALD_SERIAL_BUFFER_SIZE];
bool _is_peripherald_loading = true;

/*
 * Wait for a string from the Pi's serial port
 * 
 * if timeout is -1, it will wait for the string indefinitely.  Otherwise, timeout is the
 * maximum number of milliseconds we will wait for the string
 * 
 * Returns the number of milliseconds waited for the string, or a negative number on error.
 */
static long wait_for_string(const char* str, long timeout = -1) {
  char text_in[32];
  uint32_t str_len = strlen(str);
  long start_time = millis();

  // String to check is too big
  if (str_len > sizeof(text_in))
    return -100;

  memset(text_in, 0, str_len);

  while (memcmp(text_in, str, str_len)) {
    while (!pi_serial->available()) {
      bootloader_yield();

      // Check if we timed out
      if (timeout >= 0)
        if (start_time + timeout <= millis())
          return -1;
    }

    memmove(text_in, text_in + 1, str_len - 1);
    text_in[str_len - 1] = pi_serial->read();
  }

  return millis() - start_time;
}

/*
 * Wait for one of two strings from the Pi's serial port
 * 
 * Returns 0 if s1 was found, 1 if s2 was found, or a negative number on error.
 */
static long wait_for_either(const char* s1, const char* s2) {
  char text_in[32];
  uint32_t s1_len = strlen(s1);
  uint32_t s2_len = strlen(s2);
  uint32_t max_str_len = max(s1_len, s2_len);
  const char* s1_text_in = &text_in[max_str_len - s1_len];
  const char* s2_text_in = &text_in[max_str_len - s2_len];

  // String to check is too big
  if (max_str_len > sizeof(text_in))
    return -100;

  memset(text_in, 0, max_str_len);

  while (true) {

    while (!pi_serial->available())
      bootloader_yield();

    memmove(text_in, text_in + 1, max_str_len - 1);
    text_in[max_str_len - 1] = pi_serial->read();

    if (memcmp(s1_text_in, s1, s1_len) == 0)
      return 0;
    if (memcmp(s2_text_in, s2, s2_len) == 0)
      return 1;
  }
}

/*
 * This function just waits for the companion processor to boot up.
 */
static inline void peripherald_wait_companion_boot() {

  // If we are already booted, this will force the terminal to give us a prompt
  pi_serial->write('\n');

  // Don't care if it just booted or already has a prompt open
  wait_for_either("start : cron", ":~# ");
  debug("Companion processor booted.\n");
}

/*
 * Logs in to the companion processor.
 * 
 * Returns a negative number on error, zero on success.
 */
static inline int do_peripherald_login() {

  // Sending an enter key opens the terminal for login
  pi_serial->write('\n');

  // Wait for login prompt
  // This works by checking if the last 6 characters received match "login:"
  if (1 == wait_for_either("login:", ":~# ")) {
    debug("Companion processor already logged in\n");
    return 0;
  }

  debug("Attempting login to companion processor\n");
  pi_serial->print("root\n");
  bootloader_delay(100);
  pi_serial->print("yourpasswordhere\n");

  if (wait_for_string(":~# ", 5000) < 0) {
    // Login detect error
    return -1;
  }

  return 0;
}

static void peripherald_run_command(const char* command) {
  pi_serial->print(command);
  pi_serial->write('\n');
  
}

static void loading_swirl_thread(void* arg) {
  (void)arg;

  char i = 0;
  char sequence[] = "/-\\|";

  while (_is_peripherald_loading) {

    // Backspace
    builtin_display->write(8);
    // Swirl
    builtin_display->write(sequence[i]);
    i = (i + 1) & 3;

    bootloader_delay(150);
  }
}

static inline void display_bootup_screen() {
  builtin_display->setBrightness(70);
  builtin_display->clearScreen(0);

  int logo_x = (builtin_display->getWidth() - NTIOS_LOGO_WIDTH) / 2;
  int logo_y = (builtin_display->getHeight() - NTIOS_LOGO_HEIGHT) / 2;
  uint16_t* palette = _ntios_logo_palette_image;
  uint16_t* image_data = &_ntios_logo_palette_image[NTIOS_LOGO_PALETTE_SIZE];
  drawPaletteImage16(logo_x, logo_y, NTIOS_LOGO_WIDTH, NTIOS_LOGO_HEIGHT, palette, image_data);

  // Display the loading message
  builtin_display->setTextCursorPixels(logo_x - 16, logo_y + NTIOS_LOGO_HEIGHT + 32);
  builtin_display->print("Starting system *");

  // Start a thread to update the loading icon
  start_function(loading_swirl_thread, nullptr, 2048, "peripheral.d/ld_disp");
}

/*
 * Main method of peripheral.d.
 */
void peripherald(void* arg) {
  debug("Starting peripheral.d\n");

  vram = (uint16_t*)malloc(PERIPHERALD_VRAM_SIZE);
  if (vram == nullptr) {
    debug("WARNING: Failed to allocate %i bytes for VRAM.  Allocating 32K instead.\n", PERIPHERALD_VRAM_SIZE);
    vram = (uint16_t*)malloc(32 * 1024);
  }

  display_bootup_screen();

  peripherald_wait_companion_boot();

  if (do_peripherald_login() != 0) {
    debug("Login failure: Exiting peripheral.d\n");
    return;
  }

  // Start the main program
  peripherald_run_command("cd ~/firmware/");
  peripherald_run_command("python3 main.py");

  // Say that we stopped loading
  _is_peripherald_loading = false;
  builtin_display->clearScreen(0x0000);
  builtin_display->println("Starting UI...");

  // Here we would start the peripheral protocol handling stuff
  int i = 0;
  while (true) {

    // Wait for some bytes
    while (!pi_serial->available())
      bootloader_yield();

    // Read in the data
    while (pi_serial->available() && i < PERIPHERALD_SERIAL_BUFFER_SIZE)
      _peripherald_serial_buffer[i++] = pi_serial->read();

    // Process input
    int bytes_used = process_gpu_commands(_peripherald_serial_buffer, i);
    if (bytes_used > 0) {
      i -= bytes_used;
      memmove(_peripherald_serial_buffer, &(_peripherald_serial_buffer[bytes_used]), i);
    }
  }
}


void drawPaletteImage16(uint16_t x, uint16_t y, uint8_t w, uint8_t ys, uint16_t* color_palette, uint16_t* image_data) {
  static uint16_t pixel_buffer[256];

  uint8_t compressed_width = ((w + 3) / 4);

  uint16_t h = ys + y;
  
  for (y; y < h; y++) {

    // Extract a row of pixels
    for (uint8_t i = 0; i < compressed_width; i++) {
      pixel_buffer[(i << 2) + 0] = color_palette[15 & (image_data[i] >> 12)];
      pixel_buffer[(i << 2) + 1] = color_palette[15 & (image_data[i] >> 8)];
      pixel_buffer[(i << 2) + 2] = color_palette[15 & (image_data[i] >> 4)];
      pixel_buffer[(i << 2) + 3] = color_palette[15 & (image_data[i] >> 0)];
    }

    builtin_display->drawBitmap16(x, y, w, 1, pixel_buffer);
    image_data = &image_data[compressed_width];
  }
}

/*
 * Processes commands
 */
int process_gpu_commands(const char* src, int len) {
  int i = 0;
  while (len > i) {
    char command = src[i++];

    if (command == CMD_REQUEST_ACKNOWLEDGE) {
      pi_serial->write(EVENT_ACKNOWLEDGE);

    } else if (command == CMD_SET_CURSOR_PIXELS) {
      if (len - i < 4)
        break;

      char xh = src[i++];
      char xl = src[i++];
      char yh = src[i++];
      char yl = src[i++];
      uint16_t x = (xh << 8) | xl;
      uint16_t y = (yh << 8) | yl;

      builtin_display->setTextCursorPixels(x, y);
      
    } else if (command == CMD_WRITE_TEXT) {
      if (len - i < 1)
        break;

      char s_len = src[i];
      if (len - i - 1 < s_len)
        break;

      builtin_display->write(&src[++i], s_len);
      i += s_len;

    } else if (command == CMD_DRAW_PIXEL) {
      if (len - i < 6)
        break;

      char xh = src[i++];
      char xl = src[i++];
      char yh = src[i++];
      char yl = src[i++];
      char ch = src[i++];
      char cl = src[i++];
      uint16_t x = (xh << 8) | xl;
      uint16_t y = (yh << 8) | yl;
      uint16_t c = (ch << 8) | cl;

      builtin_display->setPixel(x, y, c);

    } else if (command == CMD_FILL_RECT) {
      if (len - i < 10)
        break;

      char x1h = src[i++];
      char x1l = src[i++];
      char y1h = src[i++];
      char y1l = src[i++];
      char x2h = src[i++];
      char x2l = src[i++];
      char y2h = src[i++];
      char y2l = src[i++];
      char ch = src[i++];
      char cl = src[i++];
      uint16_t x1 = (x1h << 8) | x1l;
      uint16_t y1 = (y1h << 8) | y1l;
      uint16_t x2 = (x2h << 8) | x2l;
      uint16_t y2 = (y2h << 8) | y2l;
      uint16_t c = (ch << 8) | cl;

      builtin_display->fillRect(x1, y1, x2, y2, c);

    } else if (command == CMD_DRAW_RECT) {
      if (len - i < 10)
        break;

      char x1h = src[i++];
      char x1l = src[i++];
      char y1h = src[i++];
      char y1l = src[i++];
      char x2h = src[i++];
      char x2l = src[i++];
      char y2h = src[i++];
      char y2l = src[i++];
      char ch = src[i++];
      char cl = src[i++];
      uint16_t x1 = (x1h << 8) | x1l;
      uint16_t y1 = (y1h << 8) | y1l;
      uint16_t x2 = (x2h << 8) | x2l;
      uint16_t y2 = (y2h << 8) | y2l;
      uint16_t c = (ch << 8) | cl;

      builtin_display->drawRect(x1, y1, x2, y2, c);

    } else if (command == CMD_SET_TEXT_COLOR) {
      if (len - i < 2)
        break;

      char ch = src[i++];
      char cl = src[i++];
      uint16_t c = (ch << 8) | cl;

      builtin_display->setTextColor(c);

    } else if (command == CMD_WRITE_VRAM) {
      if (len - i < 2 + 512)
        break;

      uint8_t ih = (uint8_t)src[i];
      uint8_t il = (uint8_t)src[i++];
      size_t index = ((ih << 16) | (il << 8));

      for (char j = 0; j < 256; j++) {
        char high = src[i++];
        char low = src[i++];
        vram[j + index] = ((high << 8) | low);
      }

    } else if (command == CMD_RENDER_BITMAP) {
      if (len - i < 8)
        break;

      uint8_t ih = (uint8_t)src[i];
      uint8_t il = (uint8_t)src[i++];
      char xh = src[i++];
      char xl = src[i++];
      char yh = src[i++];
      char yl = src[i++];
      uint8_t xs = (uint8_t)src[i++];
      uint8_t ys = (uint8_t)src[i++];

      uint16_t x = (xh << 8) | xl;
      uint16_t y = (yh << 8) | yl;
      size_t index = ((ih << 16) | (il << 8));

      builtin_display->drawBitmap16(x, y, xs, ys, &(vram[index]));

    } else if (command == CMD_SELECT_DISPLAY) {
      if (len - i < 1)
        break;

      // For now this command is ignored because we only have one display
      i++;

    } else if (command == CMD_DRAW_PALETTE_IMAGE) {
      if (len - i < 9)
        break;

      uint8_t ih = (uint8_t)src[i];
      uint8_t il = (uint8_t)src[i++];
      char xh = src[i++];
      char xl = src[i++];
      char yh = src[i++];
      char yl = src[i++];
      uint8_t xs = (uint8_t)src[i++];
      uint8_t ys = (uint8_t)src[i++];
      uint8_t palette_size = (uint8_t)src[i++];

      uint16_t x = (xh << 8) | xl;
      uint16_t y = (yh << 8) | yl;
      size_t index = ((ih << 16) | (il << 8));

      uint16_t* color_palette = &(vram[index]);
      uint16_t* image_data = &(vram[index + palette_size]);
      drawPaletteImage16(x, y, xs, ys, color_palette, image_data);

    } else if (command == CMD_FILL_DISPLAY) {
      if (len - i < 2)
        break;

      char ch = src[i++];
      char cl = src[i++];
      uint16_t c = (ch << 8) | cl;

      builtin_display->clearScreen(c);

    }
  }

  return i;
}