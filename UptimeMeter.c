/*
htop - UptimeMeter.c
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "UptimeMeter.h"
#include "Platform.h"
#include "CRT.h"

/*{
#include "Meter.h"
}*/

int UptimeMeter_attributes[] = {
   UPTIME
};

static void UptimeMeter_updateValues(Meter* this, char* buffer, int len) {
   char daysbuf[13];
   int totalseconds, seconds, minutes, hours, days;

   totalseconds = Platform_getUptime();

   if (totalseconds == -1) {
      xSnprintf(buffer, len, "(unknown)");
      return;
   }

   seconds = totalseconds % 60;
   minutes = (totalseconds/60) % 60;
   hours = (totalseconds/3600) % 24;
   days = totalseconds/86400;

   this->values[0] = days;
   this->values[1] = hours;
   this->values[2] = minutes;
   this->values[3] = seconds;
   this->values[4] = totalseconds;
   if (days > this->total) {
      this->total = days;
   }

   if (days > 1) {
      xSnprintf(daysbuf, sizeof(daysbuf), "%d days, ", days);
   } else if (days == 1) {
      xSnprintf(daysbuf, sizeof(daysbuf), "1 day, ");
   } else {
      daysbuf[0] = '\0';
   }

   xSnprintf(buffer, len, "%s%02d:%02d:%02d", daysbuf, hours, minutes, seconds);
}

static void UptimeMeter_display(Object* cast, RichString* out) {
   char buffer[13];
   int totalseconds, seconds, minutes, hours, days;

   Meter* this = (Meter*)cast;

   RichString_prune(out);

   days = this->values[0];
   hours = this->values[1];
   minutes = this->values[2];
   seconds = this->values[3];
   totalseconds = this->values[4];

   if (totalseconds == -1) {
      RichString_append(out, CRT_colors[METER_VALUE], "(unknown)");
      return;
   }

   if (days) {
      if (days > 1) {
         xSnprintf(buffer, sizeof(buffer), "%d days", days);
      } else {
         xSnprintf(buffer, sizeof(buffer),  "1 day");
      }

      RichString_append(out, days > 100 ? CRT_colors[LARGE_NUMBER] : CRT_colors[METER_VALUE], buffer);
      RichString_append(out, CRT_colors[METER_VALUE], ", ");
   }

   xSnprintf(buffer, sizeof(buffer), "%02d:%02d:%02d", hours, minutes, seconds);
   RichString_append(out, CRT_colors[METER_VALUE], buffer);
}

MeterClass UptimeMeter_class = {
   .super = {
      .extends = Class(Meter),
      .delete = Meter_delete,
      .display = UptimeMeter_display
   },
   .updateValues = UptimeMeter_updateValues,
   .defaultMode = TEXT_METERMODE,
   .maxItems = 5,
   .total = 100.0,
   .attributes = UptimeMeter_attributes,
   .name = "Uptime",
   .uiName = "Uptime",
   .caption = "Uptime: "
};
