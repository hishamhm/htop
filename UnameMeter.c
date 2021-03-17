/*
htop - UnameMeter.c
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "UnameMeter.h"

#include "CRT.h"

#include <stdio.h>
#include <string.h>
#include <sys/utsname.h>

/*{
#include "Meter.h"
}*/

int UnameMeter_attributes[] = {
   HOSTNAME
};

static void UnameMeter_updateValues(Meter* this, char* buffer, int size) {
   (void) this;
   struct utsname *data = NULL;
   data = xMalloc(sizeof(struct utsname));
   uname(data);
   /* Some OSes have their minor version numbers stored in the version field.
      We print it if it's short. */
   if ((sizeof(data->version) - 1) <= 8 || strlen(data->version) <= 8) {
      /* strlen("12.34.56") == 8 */
      snprintf(buffer, size, "%s %s %s%s%s",
               data->sysname, data->release, data->version, " ",
               data->machine);
   } else {
      snprintf(buffer, size, "%s %s %s%s%s",
               data->sysname, data->release, "", "", data->machine);
   }
   free(data);
}

MeterClass UnameMeter_class = {
   .super = {
      .extends = Class(Meter),
      .delete = Meter_delete
   },
   .updateValues = UnameMeter_updateValues,
   .defaultMode = TEXT_METERMODE,
   .maxItems = 0,
   .total = 1.0,
   .attributes = UnameMeter_attributes,
   .name = "Uname",
   .uiName = "uname",
   .description = "uname: OS kernel name and release",
   .caption = "OS : "
};
