/*
htop - SolarisMemoryMeter.c
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "SolarisMemoryMeter.h"

#include "CRT.h"
#include "Platform.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <sys/param.h>
#include <assert.h>

/*{
#include "Meter.h"
}*/

int SolarisMemoryMeter_attributes[] = {
   COLOR_MEMORY_USED, COLOR_MEMORY_BUFFERS, COLOR_MEMORY_CACHE
};

static void SolarisMemoryMeter_updateValues(Meter* this, char* buffer, int size) {
   int written;
   Platform_setMemoryValues(this);

   written = Meter_humanUnit(buffer, this->values[0], size);
   buffer += written;
   if ((size -= written) > 0) {
      *buffer++ = '/';
      size--;
      Meter_humanUnit(buffer, this->total, size);
   }
}

static void SolarisMemoryMeter_display(Object* cast, RichString* out) {
   char buffer[50];
   Meter* this = (Meter*)cast;
   RichString_write(out, CRT_colors[COLOR_METER_TEXT], ":");
   Meter_humanUnit(buffer, this->total, 50);
   RichString_append(out, CRT_colors[COLOR_METER_VALUE], buffer);
   Meter_humanUnit(buffer, this->values[0], 50);
   RichString_append(out, CRT_colors[COLOR_METER_TEXT], " used:");
   RichString_append(out, CRT_colors[COLOR_MEMORY_USED], buffer);
   Meter_humanUnit(buffer, this->values[1], 50);
   RichString_append(out, CRT_colors[COLOR_METER_TEXT], " buffers:");
   RichString_append(out, CRT_colors[COLOR_MEMORY_BUFFERS_TEXT], buffer);
   Meter_humanUnit(buffer, this->values[2], 50);
   RichString_append(out, CRT_colors[COLOR_METER_TEXT], " cache:");
   RichString_append(out, CRT_colors[COLOR_MEMORY_CACHE], buffer);
}

MeterClass SolarisMemoryMeter_class = {
   .super = {
      .extends = Class(Meter),
      .delete = Meter_delete,
      .display = SolarisMemoryMeter_display,
   },
   .updateValues = SolarisMemoryMeter_updateValues, 
   .defaultMode = BAR_METERMODE,
   .maxItems = 3,
   .total = 100.0,
   .attributes = SolarisMemoryMeter_attributes,
   .name = "Memory",
   .uiName = "Memory",
   .caption = "Mem"
};
