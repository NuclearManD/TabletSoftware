
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

#endif
