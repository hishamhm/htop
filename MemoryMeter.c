/*
htop - MemoryMeter.c
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "MemoryMeter.h"

#include "CRT.h"
#include "ProcessList.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <sys/param.h>
#include <assert.h>

/*{
#include "Meter.h"
}*/

int MemoryMeter_attributes[] = {
   MEMORY_USED, MEMORY_BUFFERS, MEMORY_CACHE
};

static void MemoryMeter_setValues(Meter* htop_this, char* buffer, int size) {
   long int usedMem = htop_this->pl->usedMem;
   long int buffersMem = htop_this->pl->buffersMem;
   long int cachedMem = htop_this->pl->cachedMem;
   usedMem -= buffersMem + cachedMem;
   htop_this->total = htop_this->pl->totalMem;
   htop_this->values[0] = usedMem;
   htop_this->values[1] = buffersMem;
   htop_this->values[2] = cachedMem;
   snprintf(buffer, size, "%ld/%ldMB", (long int) usedMem / 1024, (long int) htop_this->total / 1024);
}

static void MemoryMeter_display(Object* cast, RichString* out) {
   char buffer[50];
   Meter* htop_this = (Meter*)cast;
   int k = 1024; const char* format = "%ldM ";
   long int totalMem = htop_this->total / k;
   long int usedMem = htop_this->values[0] / k;
   long int buffersMem = htop_this->values[1] / k;
   long int cachedMem = htop_this->values[2] / k;
   RichString_write(out, CRT_colors[METER_TEXT], ":");
   sprintf(buffer, format, totalMem);
   RichString_append(out, CRT_colors[METER_VALUE], buffer);
   sprintf(buffer, format, usedMem);
   RichString_append(out, CRT_colors[METER_TEXT], "used:");
   RichString_append(out, CRT_colors[MEMORY_USED], buffer);
   sprintf(buffer, format, buffersMem);
   RichString_append(out, CRT_colors[METER_TEXT], "buffers:");
   RichString_append(out, CRT_colors[MEMORY_BUFFERS], buffer);
   sprintf(buffer, format, cachedMem);
   RichString_append(out, CRT_colors[METER_TEXT], "cache:");
   RichString_append(out, CRT_colors[MEMORY_CACHE], buffer);
}

MeterClass MemoryMeter_class = {
   .super = {
      .extends = Class(Meter),
      .htop_delete = Meter_delete,
      .display = MemoryMeter_display,
   },
   .setValues = MemoryMeter_setValues, 
   .defaultMode = BAR_METERMODE,
   .maxItems = 3,
   .total = 100.0,
   .attributes = MemoryMeter_attributes,
   "Memory",
   "Memory",
   "Mem"
};
