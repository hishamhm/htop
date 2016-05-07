/*
htop - KernelVersion.c
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "KernelVersion.h"
#include "Platform.h"
#include "CRT.h"

#include <string.h>

/*{
#include "Meter.h"
}*/

int KernelVersionMeter_attributes[] = {
   KERNELVERSION
};

static void KernelVersionMeter_updateValues(Meter* this, char* buffer, int len) {
    (void) this;
    char *kernversion = xMalloc(256);
    Platform_getKernelVersion(kernversion);
    memcpy(buffer, kernversion, len);
    free(kernversion);
}

MeterClass KernelVersionMeter_class = {
   .super = {
      .extends = Class(Meter),
      .delete = Meter_delete,
   },
   .updateValues = KernelVersionMeter_updateValues,
   .defaultMode = TEXT_METERMODE,
   .total = 100.0,
   .attributes = KernelVersionMeter_attributes,
   .name = "Kernel",
   .uiName = "Kernel version",
   .caption = "Kernel version: "
};
