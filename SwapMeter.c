/*
htop - SwapMeter.c
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "SwapMeter.h"

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

#define KILOBYTE 1
#define MEGABYTE 1024
#define GIGABYTE 1048576

int SwapMeter_attributes[] = {
   SWAP
};

/* NOTE: Value is in kilobytes */
static void SwapMeter_humanNumber(char* buffer, const long int* value) {
   if (*value >= 10*GIGABYTE)
      sprintf(buffer, "%ldG ", *value / GIGABYTE);
   else if (*value >= 10*MEGABYTE)
      sprintf(buffer, "%ldM ", *value / MEGABYTE);
   else
      sprintf(buffer, "%ldK ", *value);
}

static void SwapMeter_setValues(Meter* htop_this, char* buffer, int len) {
   long int usedSwap = htop_this->pl->usedSwap;
   htop_this->total = htop_this->pl->totalSwap;
   htop_this->values[0] = usedSwap;
   snprintf(buffer, len, "%ld/%ldMB", (long int) usedSwap / MEGABYTE, (long int) htop_this->total / MEGABYTE);
}

static void SwapMeter_display(Object* cast, RichString* out) {
   char buffer[50];
   Meter* htop_this = (Meter*)cast;
   long int swap = (long int) htop_this->values[0];
   long int total = (long int) htop_this->total;
   RichString_write(out, CRT_colors[METER_TEXT], ":");
   SwapMeter_humanNumber(buffer, &total);
   RichString_append(out, CRT_colors[METER_VALUE], buffer);
   SwapMeter_humanNumber(buffer, &swap);
   RichString_append(out, CRT_colors[METER_TEXT], "used:");
   RichString_append(out, CRT_colors[METER_VALUE], buffer);
}

MeterClass SwapMeter_class = {
   .super = {
      .extends = Class(Meter),
      .htop_delete = Meter_delete,
      .display = SwapMeter_display,
   },
   .setValues = SwapMeter_setValues, 
   .defaultMode = BAR_METERMODE,
   .total = 100.0,
   .attributes = SwapMeter_attributes,
   .name = "Swap",
   .uiName = "Swap",
   .caption = "Swp"
};
