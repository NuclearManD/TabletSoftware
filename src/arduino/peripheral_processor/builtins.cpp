
#include <Arduino.h>
#include "USBHost_t36.h"
#include "TeensyThreads.h"
#include <Audio.h>

#include "src/ntios/drivers/ctp_drivers.h"
#include "src/ntios/drivers.h"
#include "src/ntios/ntios.h"
#include "src/ntios/keys.h"

#include "hw.h"

int _lsusb(StreamDevice* io, int ac, char** av);

extern int num_threads;
extern int thread_pids[20];
extern const char* thread_names[20];

extern FT5436Touch* builtin_touchpad;

int builtin_system(int argc, char** argv, StreamDevice* io) {

  if (!strcmp(argv[0], "mkpwm")) {
    if (argc > 1) {
      char* echk;
      int pin = (int)strtol(argv[1], &echk, 10);
      if (*echk != 0 || argv[1][0] == 0 || pin < 3 || pin > 41) {
        io->printf("Error: bad pin '%s'\n", argv[1]);
        return -2;
      } else {
        add_virtual_device(new TeensyPWMPin(pin));
        return 0;
      }
    } else {
      io->println(F("usage: mkpwm PIN"));
      return -1;
    }

  } else if (!strcmp(argv[0], "touchtest")) {
    while (io->available())
      io->read();

    while (!io->available()) {
      int nPress = builtin_touchpad->numPresses();
      if (nPress) {
        Serial.printf("Touch: %hu, %hu, %hu\n",
          builtin_touchpad->pressXCoord(0),
          builtin_touchpad->pressYCoord(0),
          builtin_touchpad->pressZCoord(0)
        );
      }

      threads.delay(15);
    }

    io->read();
    return 0;

  } else if (!strcmp(argv[0], "ps")) {
    io->printf("Total CPU usage %.2f%%.  Collecting data...\n", get_cpu_usage_percent());

    uint32_t total_cycles[num_threads];
    for (int i = 0; i < num_threads; i++)
      total_cycles[i] = threads.getCyclesUsed(thread_pids[i]);
    threads.delay(1000);
    for (int i = 0; i < num_threads; i++)
      total_cycles[i] = threads.getCyclesUsed(thread_pids[i]) - total_cycles[i];
      
    io->println( "  PID  Name                            Usage %");
    for (int i = 0; i < num_threads; i++) {
      int pid = thread_pids[i];
      double total_time_ms = (1000.0 / get_cpu_hz()) * total_cycles[i];
      double percent_usage = 100.0 * (total_time_ms / 1000.0);
      io->printf("  % 2i % 30s  %.2f%%\n", pid, thread_names[i], percent_usage);
    }
    return 0;
  } else if (!strcmp(argv[0], "lsusb")) {
    return _lsusb(io, argc - 1, &(argv[1]));
  } else if (!strcmp(argv[0], "vbatt")) {
    io->printf("Battery voltage: %f\n", (double)get_battery_voltage());
    return 0;
  } else if (!strcmp(argv[0], "btnstatus")) {
    io->printf("User Button: %hhu\n", read_user_button());
    return 0;
  }

  return -100;
}
