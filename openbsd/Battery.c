/*
htop - openbsd/Battery.c
(C) 2015 Hisham H. Muhammad
(C) 2015 Michael McConville
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "BatteryMeter.h"
#include <sys/sysctl.h>
#include <sys/sensors.h>
#include <errno.h>

void Battery_getData(double* level, ACPresence* isOnAC) {
   int devn;
   double charge, last_full_capacity;
   static int mib[] = {CTL_HW, HW_SENSORS, 0, 0, 0};
   struct sensor s;
   size_t slen = sizeof(struct sensor);
   struct sensordev snsrdev;
   size_t sdlen = sizeof(struct sensordev);

   for (devn = 0;; devn++) { 
      mib[2] = devn;
      if (sysctl(mib, 3, &snsrdev, &sdlen, NULL, 0) == -1) {
         {
            if (errno == ENXIO)
               continue;
            if (errno == ENOENT)
               break;
         }
      }
      if (!strcmp("acpibat0", snsrdev.xname)) {
         break;
      }
   }    

   /* last full capacity */
   mib[3] = 7;
   mib[4] = 0;
   if (sysctl(mib, 5, &s, &slen, NULL, 0) != -1) {
      last_full_capacity = (double) s.value;
   }
   /*  remaining capacity */
   mib[3] = 7;
   mib[4] = 3;
   if (sysctl(mib, 5, &s, &slen, NULL, 0) != -1) {
      charge = (double) s.value;
   } else *level = -1;

   *level = 100*(charge / last_full_capacity);
   if (charge >= last_full_capacity )
      *level  = 100;

   for (devn = 0;; devn++) { 
      mib[2] = devn;
      if (sysctl(mib, 3, &snsrdev, &sdlen, NULL, 0) == -1) {
         {
            if (errno == ENXIO)
               continue;
            if (errno == ENOENT)
               break;
         }
      }
      if (!strcmp("acpiac0", snsrdev.xname)) {
         break;
      }
   }
   mib[3] = 9;
   mib[4] = 0;

   if (sysctl(mib, 5, &s, &slen, NULL, 0) != -1) {
      *isOnAC = s.value;
   } else
      *isOnAC = AC_ERROR;
}
