/*
htop - aix/Platform.c
(C) 2014 Hisham H. Muhammad
(C) 2015 David C. Hunt
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "Platform.h"
#include "CPUMeter.h"
#include "MemoryMeter.h"
#include "SwapMeter.h"
#include "TasksMeter.h"
#include "LoadAverageMeter.h"
#include "ClockMeter.h"
#include "HostnameMeter.h"
#include "UptimeMeter.h"

#include <sys/proc.h>

/*{
#include "Action.h"
#include "BatteryMeter.h"
#include "SignalsPanel.h"
#include "AixProcess.h"
}*/

const SignalItem Platform_signals[] = {
   { .name = " 0 Cancel",    .number =  0 },
};

const unsigned int Platform_numberOfSignals = sizeof(Platform_signals)/sizeof(SignalItem);

ProcessField Platform_defaultFields[] = { PID, USER, PRIORITY, NICE, M_SIZE, M_RESIDENT, STATE, PERCENT_CPU, PERCENT_MEM, TIME, COMM, 0 };

extern ProcessFieldData Process_fields[];

MeterClass* Platform_meterTypes[] = {
   &CPUMeter_class,
   &ClockMeter_class,
   &LoadAverageMeter_class,
   &LoadMeter_class,
   &MemoryMeter_class,
   &SwapMeter_class,
   &TasksMeter_class,
   &BatteryMeter_class,
   &HostnameMeter_class,
   &UptimeMeter_class,
   &AllCPUsMeter_class,
   &AllCPUs2Meter_class,
   &LeftCPUsMeter_class,
   &RightCPUsMeter_class,
   &LeftCPUs2Meter_class,
   &RightCPUs2Meter_class,
   &BlankMeter_class,
   NULL
};

void Platform_setBindings(Htop_Action* keys) {
   (void) keys;
}

int Platform_numberOfFields = 101;

extern char Process_pidFormat[20];

//ProcessPidColumn Process_pidColumns[] = {
//   { .id = 0, .label = NULL },
//};

int Platform_getUptime() {
   return 0;
}

void Platform_getLoadAverage(double* one, double* five, double* fifteen) {
   *one = 0;
   *five = 0;
   *fifteen = 0;
}

int Platform_getMaxPid() {
   return PIDMAX;
}

double Platform_setCPUValues(Meter* this, int cpu) {
   (void) this;
   (void) cpu;
   return 0.0;
}

void Platform_setMemoryValues(Meter* this) {
   (void) this;
}

void Platform_setSwapValues(Meter* this) {
   (void) this;
}

bool Process_isThread(Process* this) {
   (void) this;
   return false;
}

char* Platform_getProcessEnv(pid_t pid) {
   (void) pid;
   return NULL;
}
