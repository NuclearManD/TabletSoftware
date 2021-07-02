
#include "hw.h"
#include "Arduino.h"

#include "src/ntios/drivers/ctp_drivers.h"

#define debug(...) Serial.printf(__VA_ARGS__)


extern SerialDevice* pi_serial;
extern USBKeyboard* ntios_keyboard;
extern CapacitiveTouchDevice* builtin_touchpad;
extern RA8875Graphics* builtin_display;


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

/*
 * Main method of peripheral.d.
 */
void peripherald(void* arg) {
  debug("Starting peripheral.d\n");

  peripherald_wait_companion_boot();

  if (do_peripherald_login() != 0) {
    debug("Login failure: Exiting peripherald\n");
    return;
  }

  // Start the main program
  peripherald_run_command("python3 main.py");

  // Here we would start the peripheral protocol handling stuff
}
