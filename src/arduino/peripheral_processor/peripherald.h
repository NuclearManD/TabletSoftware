
#ifndef PERIPHERALD_H
#define PERIPHERALD_H

#include "src/ntios/ntios.h"

bool start_function(void (*f)(void* param), void* param, int stack_size, const char* name);

// The main function of the peripheral daemon
void peripherald(void* arg);

// Function to start peripheral.d
static inline void start_peripherald() {
  start_function(peripherald, nullptr, 8192, "peripheral.d");
}

/*
 * Processes ESP32-UART-LCD style commands
 * 
 * Returns the number of bytes processed
 */
int process_gpu_commands(const char* src, int len);

void drawPaletteImage16(uint16_t x, uint16_t y, uint8_t w, uint8_t h, uint16_t* color_palette, uint16_t* image_data);

#endif
