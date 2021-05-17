#include <SPI.h>

#include "TeensyThreads.h"

#include "src/ntios/drivers.h"
#include "src/ntios/ntios.h"
#include "hw.h"

#define PROGRAM_STACK_SPACE 1024 * 6

StreamDevice* UserIO;

void hack_cache();

int num_threads = 0;
int thread_pids[20];
const char* thread_names[20];

void on_new_thread(int pid, const char* name) {
  thread_pids[num_threads] = pid;
  thread_names[num_threads] = name;
  num_threads++;
}

void on_killed_thread(int pid) {
  for (int i = 0; i < num_threads; i++) {
    if (thread_pids[i] == pid) {
      num_threads--;
      thread_pids[i] = thread_pids[num_threads];
      thread_names[i] = thread_names[num_threads];
      break;
    }
  }
}

typedef struct launch_params_s {
  int argc;
  char** argv;
  StreamDevice* io;
} launch_params_t;

typedef struct startf_data_s {
  void* param;
  void (*f)(void*);
} startf_data_t;

void _launch_wrapper(void* raw) {
  launch_params_t* s = (launch_params_t*)raw;
  ntios_system(s->argc, s->argv, s->io);
  free(raw);
  on_killed_thread(threads.id());
}

void ntios_shell_wrapper(void* io) {
  ntios_shell((StreamDevice*)io);
  on_killed_thread(threads.id());
}

void startf_wrapper(void* datap) {
  startf_data_t* data = (startf_data_t*)datap;
  Serial.printf("Starting: %p((void*)%p)\n", data->f, data->param);
  data->f(data->param);
  //free(data);
  on_killed_thread(threads.id());
}

bool launch(int argc, char** argv, StreamDevice* io) {

  // Compute size of memory to store program arguments
  size_t args_size = sizeof(char*) * argc;
  for (int i = 0; i < argc; i++)
    args_size += strlen(argv[i]) + 1;

  // Allocate a single chunk of memory (more CPU and RAM efficient) and get our pointers
  char* mem = (char*)malloc(sizeof(launch_params_t) + args_size);
  launch_params_t* s = (launch_params_t*)mem;
  char** new_argv = (char**)(mem + sizeof(launch_params_t));
  char* arg_buffer = (char*)(mem + sizeof(launch_params_t) + sizeof(char*) * argc);

  // Copy arguments so the caller can safely delete them without us losing them
  for (int i = 0; i < argc; i++) {
    new_argv[i] = arg_buffer;
    strcpy(arg_buffer, argv[i]);
    arg_buffer = strchr(arg_buffer, 0) + 1;
  }

  s->argc = argc;
  s->argv = new_argv;
  s->io = io;
  threads.addThread(_launch_wrapper, (void*)s, PROGRAM_STACK_SPACE);
  return true;
}

bool launch(char* argv, StreamDevice* io) {
  return launch(1, &argv, io);
}

bool start_function(void (*f)(void* param), void* param, int stack_size) {
  startf_data_t* s = (startf_data_t*)malloc(sizeof(startf_data_t));
  if (s == nullptr)
    return false;

  s->f = f;
  s->param = param;
  int id = threads.addThread(startf_wrapper, (void*)s, stack_size);

  if (id >= 0) {
    on_new_thread(id, "[anonymous function]");
    return true;
  }
  return false;
}

void stack_overflow_isr(void) {
  threads.kill(threads.id());
  on_killed_thread(threads.id());
  Serial.println("STACK OVERFLOW!");

  int id = threads.addThread(ntios_shell_wrapper, UserIO, PROGRAM_STACK_SPACE * 4);

  if (id >= 0) {
    on_new_thread(id, "NTIOS Shell (after stack overflow)");
  }
}

int get_bootloader_thread_slice_us() {
  return 10000;
}

void bootloader_delay(long milliseconds) {
  threads.delay(milliseconds);
}

extern void (* _VectorsRam[NVIC_NUM_INTERRUPTS+16])(void);
extern void unused_interrupt_vector(void);


// Stack frame
//  xPSR
//  ReturnAddress
//  LR (R14) - typically FFFFFFF9 for IRQ or Exception
//  R12
//  R3
//  R2
//  R1
//  R0
// Code from :: https://community.nxp.com/thread/389002
__attribute__((naked))
void custom_interrupt_vector(void)
{
  __asm( ".syntax unified\n"
         "MOVS R0, #4 \n"
         "MOV R1, LR \n"
         "TST R0, R1 \n"
         "BEQ _MSP \n"
         "MRS R0, PSP \n"
         "B _Z25custom_HardFault_HandlerCPj \n"
         "_MSP: \n"
         "MRS R0, MSP \n"
         "B _Z25custom_HardFault_HandlerCPj \n"
         ".syntax divided\n") ;
}


//__attribute__((used, weak))
void custom_HardFault_HandlerC(unsigned int *hardfault_args)
{
  volatile unsigned int nn ;
  volatile unsigned int stacked_r0 ;
  volatile unsigned int stacked_r1 ;
  volatile unsigned int stacked_r2 ;
  volatile unsigned int stacked_r3 ;
  volatile unsigned int stacked_r12 ;
  volatile unsigned int stacked_lr ;
  volatile unsigned int stacked_pc ;
  volatile unsigned int stacked_psr ;
  volatile unsigned int _CFSR ;
  volatile unsigned int _HFSR ;
  volatile unsigned int _DFSR ;
  volatile unsigned int _AFSR ;
  volatile unsigned int _BFAR ;
  volatile unsigned int _MMAR ;
  volatile unsigned int addr ;
  volatile unsigned int fault_instruction;
  volatile unsigned char* pre_fault_instructions;

  stacked_r0 = ((unsigned int)hardfault_args[0]) ;
  stacked_r1 = ((unsigned int)hardfault_args[1]) ;
  stacked_r2 = ((unsigned int)hardfault_args[2]) ;
  stacked_r3 = ((unsigned int)hardfault_args[3]) ;
  stacked_r12 = ((unsigned int)hardfault_args[4]) ;
  stacked_lr = ((unsigned int)hardfault_args[5]) ;
  stacked_pc = ((unsigned int)hardfault_args[6]) ;
  stacked_psr = ((unsigned int)hardfault_args[7]) ;
  fault_instruction = *((unsigned int*)stacked_pc);
  pre_fault_instructions = (char*)(stacked_pc - 256);
  // Configurable Fault Status Register
  // Consists of MMSR, BFSR and UFSR
  //(n & ( 1 << k )) >> k
  _CFSR = (*((volatile unsigned int *)(0xE000ED28))) ;  
  // Hard Fault Status Register
  _HFSR = (*((volatile unsigned int *)(0xE000ED2C))) ;
  // Debug Fault Status Register
  _DFSR = (*((volatile unsigned int *)(0xE000ED30))) ;
  // Auxiliary Fault Status Register
  _AFSR = (*((volatile unsigned int *)(0xE000ED3C))) ;
  // Read the Fault Address Registers. These may not contain valid values.
  // Check BFARVALID/MMARVALID to see if they are valid values
  // MemManage Fault Address Register
  _MMAR = (*((volatile unsigned int *)(0xE000ED34))) ;
  // Bus Fault Address Register
  _BFAR = (*((volatile unsigned int *)(0xE000ED38))) ;
  //__asm("BKPT #0\n") ; // Break into the debugger // NO Debugger here.

  asm volatile("mrs %0, ipsr\n" : "=r" (addr)::);
  Serial.printf("\nFaulted thread ID: %i", threads.id());
  Serial.printf("\nFaulted thread stack pointer: %p", threads.threadp[threads.id()]->sp);
  Serial.printf("\nFaulted thread stack base:    %p\n", threads.threadp[threads.id()]->stack);
  Serial.printf("\nFault irq %d\n", addr & 0x1FF);
  Serial.printf(" stacked_r0 ::  %x\n", stacked_r0);
  Serial.printf(" stacked_r1 ::  %x\n", stacked_r1);
  Serial.printf(" stacked_r2 ::  %x\n", stacked_r2);
  Serial.printf(" stacked_r3 ::  %x\n", stacked_r3);
  Serial.printf(" stacked_r12 ::  %x\n", stacked_r12);
  Serial.printf(" stacked_lr ::  %x\n", stacked_lr);
  Serial.printf(" stacked_pc ::  %x (instruction = %08x)\n", stacked_pc, fault_instruction);
  Serial.printf(" stacked_psr ::  %x\n", stacked_psr);
  Serial.printf(" stack pointer ::  %x\n", (unsigned int)&(hardfault_args[-1]));
  Serial.println("Exception Stack (most recent push last):");
  for (int i = 64; i >= 0; i--) {
    if ((0xFFF00000 & (unsigned int)hardfault_args) == 0x20000000)
      if (0x20050000 <= (unsigned int)&(hardfault_args[i]))
        continue;
    if (i == 8)
      Serial.printf("[exception here, remaining entries are fault register pushes]\n");
    //else
    Serial.printf("   ");
    Serial.printf("0x%08x = 0x%08x", (unsigned int)&(hardfault_args[i]), hardfault_args[i]);
    if ((hardfault_args[i] & 0xFFF00000) == 0x20000000UL) {
      Serial.printf(" -> {");
      for (int j = 0; j < 16; j++) {
        Serial.printf("0x%hhx, ", ((uint8_t*)hardfault_args[i])[j]);
      }
      Serial.printf("...");
    }
    Serial.printf("\n");
  }
  /*ThreadInfo* thread = threads.threadp[threads.id()];
  unsigned int* thread_stack_top = (unsigned int*)(&(thread->stack[thread->stack_size]));
  unsigned int* thread_sp = (unsigned int*)(thread->sp);
  int num_entries = ((unsigned int)thread_stack_top - (unsigned int)thread_sp) / 4;
  Serial.println("\nThread stack:");
  for (int i = 0; i < num_entries; i++) {
    Serial.printf("   0x%08x = 0x%08x", (unsigned int)&(thread_sp[i]), thread_sp[i]);
    if ((thread_sp[i] & 0xFFF00000) == 0x20000000UL) {
      Serial.printf(" -> {");
      for (int j = 0; j < 16; j++) {
        Serial.printf("0x%hhx, ", ((uint8_t*)thread_sp[i])[j]);
      }
      Serial.printf("...");
    }
    Serial.printf("\n");
  }*/

  Serial.println();
  Serial.printf(" _CFSR ::  %x\n", _CFSR);
 
  if(_CFSR > 0){
    //Memory Management Faults
    if((_CFSR & 1) == 1){
    Serial.printf("      (IACCVIOL) Instruction Access Violation\n");
    } else  if(((_CFSR & (0x02))>>1) == 1){
    Serial.printf("      (DACCVIOL) Data Access Violation\n");
    } else if(((_CFSR & (0x08))>>3) == 1){
    Serial.printf("      (MUNSTKERR) MemMange Fault on Unstacking\n");
    } else if(((_CFSR & (0x10))>>4) == 1){
    Serial.printf("      (MSTKERR) MemMange Fault on stacking\n");
    } else if(((_CFSR & (0x20))>>5) == 1){
    Serial.printf("      (MLSPERR) MemMange Fault on FP Lazy State\n");
    }
    if(((_CFSR & (0x80))>>7) == 1){
    Serial.printf("      (MMARVALID) MemMange Fault Address Valid\n");
    }
    //Bus Fault Status Register
    if(((_CFSR & 0x100)>>8) == 1){
    Serial.printf("      (IBUSERR) Instruction Bus Error\n");
    } else  if(((_CFSR & (0x200))>>9) == 1){
    Serial.printf("      (PRECISERR) Data bus error(address in BFAR)\n");
    } else if(((_CFSR & (0x400))>>10) == 1){
    Serial.printf("      (IMPRECISERR) Data bus error but address not related to instruction\n");
    } else if(((_CFSR & (0x800))>>11) == 1){
    Serial.printf("      (UNSTKERR) Bus Fault on unstacking for a return from exception \n");
    } else if(((_CFSR & (0x1000))>>12) == 1){
    Serial.printf("      (STKERR) Bus Fault on stacking for exception entry\n");
    } else if(((_CFSR & (0x2000))>>13) == 1){
    Serial.printf("      (LSPERR) Bus Fault on FP lazy state preservation\n");
    }
    if(((_CFSR & (0x8000))>>15) == 1){
    Serial.printf("      (BFARVALID) Bus Fault Address Valid\n");
    }  
    //Usuage Fault Status Register
    if(((_CFSR & 0x10000)>>16) == 1){
    Serial.printf("      (UNDEFINSTR) Undefined instruction\n");
    } else  if(((_CFSR & (0x20000))>>17) == 1){
    Serial.printf("      (INVSTATE) Instruction makes illegal use of EPSR)\n");
    } else if(((_CFSR & (0x40000))>>18) == 1){
    Serial.printf("      (INVPC) Usage fault: invalid EXC_RETURN\n");
    } else if(((_CFSR & (0x80000))>>19) == 1){
    Serial.printf("      (NOCP) No Coprocessor \n");
    } else if(((_CFSR & (0x1000000))>>24) == 1){
    Serial.printf("      (UNALIGNED) Unaligned access UsageFault\n");
    } else if(((_CFSR & (0x2000000))>>25) == 1){
    Serial.printf("      (DIVBYZERO) Divide by zero\n");
    }
  }
  Serial.printf(" _HFSR ::  %x\n", _HFSR);
  if(_HFSR > 0){
    //Memory Management Faults
    if(((_HFSR & (0x02))>>1) == 1){
    Serial.printf("      (VECTTBL) Bus Fault on Vec Table Read\n");
    } else if(((_HFSR & (0x40000000))>>30) == 1){
    Serial.printf("      (FORCED) Forced Hard Fault\n");
    } else if(((_HFSR & (0x80000000))>>31) == 31){
    Serial.printf("      (DEBUGEVT) Reserved for Debug\n");
    } 
  }
  Serial.printf(" _DFSR ::  %x\n", _DFSR);
  Serial.printf(" _AFSR ::  %x\n", _AFSR);
  Serial.printf(" _BFAR ::  %x\n", _BFAR);
  Serial.printf(" _MMAR ::  %x\n", _MMAR);
  Serial.flush();

  Serial.println();
  Serial.printf("Hexdump before PC, starting at address 0x%08x:\n", pre_fault_instructions);
  for (int i = 0; i < 256 + 16; i++) {
    Serial.printf("%02hhx", pre_fault_instructions[i]);
    if ((i & 15) == 15)
      Serial.println();
    else
      Serial.write(' ');
  }

  Serial.flush();

  while(1);
}

void setup() {
  hw_preinit();
  Serial.begin(115200);

  if (!Serial)
    delay(1000);

  // Hack into the vectors RAM and insert our own interrupts
  for (int i = 0; i < 16; i++)
    if (i != 15 && i != 14 && i != 11) {
      Serial.printf("Changing IVT entry %i\n", i);
      _VectorsRam[i] = &custom_interrupt_vector;
    }

  threads.setSliceMicros(10000);

  start_hw();

  UserIO = get_serial_0();

  int id = threads.addThread(ntios_shell_wrapper, UserIO, PROGRAM_STACK_SPACE * 2);

  if (id >= 0) {
    on_new_thread(id, "NTIOS Shell");
  } else {
    Serial.println("Problem starting shell thread!");
  }
}

void loop() {
  // 66.7Hz
  long next_update = millis() + 15;
  ntios_yield();
  long t = next_update - millis();
  //Serial.println(t);
  threads.delay(next_update - millis());
}

void bootloader_yield() {
  threads.yield();
}
