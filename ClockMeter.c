/*
htop - ClockMeter.c
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "ClockMeter.h"

#include "CRT.h"

#include <time.h>

/*{
#include "Meter.h"
}*/

int ClockMeter_attributes[] = {
   CLOCK
};

static void ClockMeter_setValues(Meter* htop_this, char* buffer, int size) {
   time_t t = time(NULL);
   struct tm *lt = localtime(&t);
   htop_this->values[0] = lt->tm_hour * 60 + lt->tm_min;
   strftime(buffer, size, "%H:%M:%S", lt);
}

MeterClass ClockMeter_class = {
   .super = {
      .extends = Class(Meter),
      .delete = Meter_delete
   },
   .setValues = ClockMeter_setValues, 
   .defaultMode = TEXT_METERMODE,
   .total = 100.0,
   .attributes = ClockMeter_attributes,
   .name = "Clock",
   .uiName = "Clock",
   .caption = "Time: ",
};
