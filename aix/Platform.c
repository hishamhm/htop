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
#include "AixProcessList.h"

#include <sys/proc.h>
#include <utmpx.h>
#include <string.h>
#include <time.h>
#include <math.h>

/*{
#include "Action.h"
#include "BatteryMeter.h"
#include "SignalsPanel.h"
#include "AixProcess.h"

// Used for the load average.
#include <sys/kinfo.h>
// AIX doesn't define this function for userland headers, but it's in libc
extern int getkerninfo(int, char*, int*, int32long64_t);
}*/

#ifndef CLAMP
#define CLAMP(x,low,high) (((x)>(high))?(high):(((x)<(low))?(low):(x)))
#endif

unsigned long long avenrun [3];

const SignalItem Platform_signals[] = {
   { .name = " 0 Cancel",    .number =  0 },
   { .name = " 1 SIGHUP",      .number =  1 },
   { .name = " 2 SIGINT",      .number =  2 },
   { .name = " 3 SIGQUIT",     .number =  3 },
   { .name = " 4 SIGILL",      .number =  4 },
   { .name = " 5 SIGTRAP",     .number =  5 },
   { .name = " 6 SIGABRT/IOT", .number =  6 },
   { .name = " 7 SIGEMT",      .number =  7 },
   { .name = " 8 SIGFPE",      .number =  8 },
   { .name = " 9 SIGKILL",     .number =  9 },
   { .name = "10 SIGBUS",      .number = 10 },
   { .name = "11 SIGSEGV",     .number = 11 },
   { .name = "12 SIGSYS",      .number = 12 },
   { .name = "13 SIGPIPE",     .number = 13 },
   { .name = "14 SIGALRM",     .number = 14 },
   { .name = "15 SIGTERM",     .number = 15 },
   { .name = "16 SIGURG" ,     .number = 16 },
   { .name = "17 SIGSTOP",     .number = 17 },
   { .name = "18 SIGSTP",      .number = 18 },
   { .name = "19 SIGCONT",     .number = 19 },
   { .name = "20 SIGCHLD",     .number = 20 },
   { .name = "21 SIGTTIN",     .number = 21 },
   { .name = "22 SIGTTOU",     .number = 22 },
   { .name = "23 SIGIO",       .number = 23 },
   { .name = "24 SIGXCPU",     .number = 24 },
   { .name = "25 SIGXFSZ",     .number = 25 },
   // no 26
   { .name = "27 SIGMSG",      .number = 27 },
   { .name = "28 SIGWINCH",    .number = 28 },
   { .name = "30 SIGUSR1",     .number = 30 },
   { .name = "31 SIGUSR2",     .number = 31 },
   { .name = "32 SIGPROF",     .number = 32 },
   { .name = "33 SIGDANGER",   .number = 33 },
   { .name = "34 SIGVTALRM",   .number = 34 },
   { .name = "35 SIGMIGRATE",  .number = 35 },
   { .name = "36 SIGPRE",      .number = 36 },
   { .name = "37 SIGVIRT",     .number = 37 },
   { .name = "38 SIGTALRM",    .number = 38 },
   // AIX requests not use SIGWAITING/39
   // don't know about the hole here
   { .name = "48 SIGSYSERROR", .number = 48 },
   { .name = "49 SIGRECOVERY", .number = 49 },
   // Real-time signals 50-57
   //{ .name = "58 SIGRECONFIG", .number = 58 },
   // AIX also requests us to not use SIGCPUFAIL/59
   //{ .name = "60 SIGKAP",      .number = 60 },
   //{ .name = "61 SIGRETRACT",  .number = 61 },
   //{ .name = "62 SIGSOUND",    .number = 62 },
   //{ .name = "63 SIGSAK",      .number = 63 },
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

// identical to Solaris, thanks System V
int Platform_getUptime() {
#ifndef __PASE__
   int boot_time = 0;
   int curr_time = time(NULL);
   struct utmpx * ent;

   while (( ent = getutxent() )) {
      if ( !strcmp("system boot", ent->ut_line )) {
         boot_time = ent->ut_tv.tv_sec;
      }
   }

   endutxent();

   return (curr_time-boot_time);
#else
   // IBM i doesn't expose uptime to PASE, AFAIK
   return 0;
#endif
}

void Platform_getLoadAverage(double* one, double* five, double* fifteen) {
#ifndef __PASE__
   int size = sizeof (avenrun);
   if (getkerninfo(KINFO_GET_AVENRUN, avenrun, &size, 0) != -1) {
      // apply float scaling factor
      *one = (double)avenrun [0] / 65536;
      *five = (double)avenrun [1] / 65536;
      *fifteen = (double)avenrun [2] / 65536;
   }
#else
   // IBM i doesn't generate load averages
   *one = 0;
   *five = 0;
   *fifteen = 0;
#endif
}

int Platform_getMaxPid() {
   return PIDMAX;
}

double Platform_setCPUValues(Meter* this, int cpu) {
   AixProcessList* apl = (AixProcessList*) this->pl;
   CPUData* cpuData;
   cpuData = &(apl->cpus [cpu]);

   double  percent;
   double* v = this->values;

   v[CPU_METER_NICE]   = 0.0;
   v[CPU_METER_NORMAL] = cpuData->utime_p;
   if (this->pl->settings->detailedCPUTime) {
      v[CPU_METER_KERNEL]  = cpuData->stime_p;
      v[CPU_METER_IRQ]     = cpuData->wtime_p;
      Meter_setItems(this, 4);
      percent = v[0]+v[1]+v[2]+v[3];
   } else {
      v[2] = cpuData->stime_p + cpuData->wtime_p;
      Meter_setItems(this, 3);
      percent = v[0]+v[1]+v[2];
   }

   percent = CLAMP(percent, 0.0, 100.0);
   if (isnan(percent)) percent = 0.0;
   return percent;
}

void Platform_setMemoryValues(Meter* this) {
   ProcessList* pl = (ProcessList*) this->pl;
   long int usedMem = pl->usedMem;
   long int buffersMem = pl->buffersMem;
   long int cachedMem = pl->cachedMem;
   usedMem -= buffersMem + cachedMem;
   this->total = pl->totalMem;
   this->values[0] = usedMem;
   this->values[1] = buffersMem;
   this->values[2] = cachedMem;
}

void Platform_setSwapValues(Meter* this) {
   // the mem scan code in AixProcessList does the work
   ProcessList* pl = (ProcessList*) this->pl;
   this->total = pl->totalSwap;
   this->values[0] = pl->usedSwap;
}

bool Process_isThread(Process* this) {
   (void) this;
   return false;
}

char* Platform_getProcessEnv(pid_t pid) {
   (void) pid;
   return NULL;
}
