/*
htop - Meter.c
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "Meter.h"

#include "RichString.h"
#include "Object.h"
#include "CRT.h"
#include "StringUtils.h"
#include "ListItem.h"
#include "Settings.h"

#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <assert.h>
#include <sys/time.h>

#define METER_BUFFER_LEN 256

#define GRAPH_DELAY (DEFAULT_DELAY/2)

#define GRAPH_HEIGHT 4 /* Unit: rows (lines) */

/*{
#include "ListItem.h"

#include <sys/time.h>

typedef struct Meter_ Meter;

typedef void(*Meter_Init)(Meter*);
typedef void(*Meter_Done)(Meter*);
typedef void(*Meter_UpdateMode)(Meter*, int);
typedef void(*Meter_UpdateValues)(Meter*, char*, int);
typedef void(*Meter_Draw)(Meter*, int, int, int);

typedef struct MeterClass_ {
   ObjectClass super;
   const Meter_Init init;
   const Meter_Done done;
   const Meter_UpdateMode updateMode;
   const Meter_Draw draw;
   const Meter_UpdateValues updateValues;
   const int defaultMode;
   const double total;
   const int* attributes;
   const char* name;
   const char* uiName;
   const char* caption;
   const char* description;
   const char maxItems;
   char curItems;
} MeterClass;

#define As_Meter(this_)                ((MeterClass*)((this_)->super.klass))
#define Meter_initFn(this_)            As_Meter(this_)->init
#define Meter_init(this_)              As_Meter(this_)->init((Meter*)(this_))
#define Meter_done(this_)              As_Meter(this_)->done((Meter*)(this_))
#define Meter_updateModeFn(this_)      As_Meter(this_)->updateMode
#define Meter_updateMode(this_, m_)    As_Meter(this_)->updateMode((Meter*)(this_), m_)
#define Meter_drawFn(this_)            As_Meter(this_)->draw
#define Meter_doneFn(this_)            As_Meter(this_)->done
#define Meter_updateValues(this_, buf_, sz_) \
                                       As_Meter(this_)->updateValues((Meter*)(this_), buf_, sz_)
#define Meter_defaultMode(this_)       As_Meter(this_)->defaultMode
#define Meter_getItems(this_)          As_Meter(this_)->curItems
#define Meter_setItems(this_, n_)      As_Meter(this_)->curItems = (n_)
#define Meter_attributes(this_)        As_Meter(this_)->attributes
#define Meter_name(this_)              As_Meter(this_)->name
#define Meter_uiName(this_)            As_Meter(this_)->uiName

struct Meter_ {
   Object super;
   Meter_Draw draw;
   
   char* caption;
   int mode;
   int param;
   void* drawData;
   int h;
   struct ProcessList_* pl;
   double* values;
   double total;
};

typedef struct MeterMode_ {
   Meter_Draw draw;
   const char* uiName;
   int h;
} MeterMode;

typedef enum {
   CUSTOM_METERMODE = 0,
   BAR_METERMODE,
   TEXT_METERMODE,
   GRAPH_METERMODE,
   LED_METERMODE,
   LAST_METERMODE
} MeterModeId;

typedef struct GraphData_ {
   struct timeval time;
   double values[METER_BUFFER_LEN];
} GraphData;

}*/

#ifndef MIN
#define MIN(a,b) ((a)<(b)?(a):(b))
#endif
#ifndef MAX
#define MAX(a,b) ((a)>(b)?(a):(b))
#endif
#ifndef CLAMP
#define CLAMP(x,low,high) (((x)>(high))?(high):(((x)<(low))?(low):(x)))
#endif

MeterClass Meter_class = {
   .super = {
      .extends = Class(Object)
   }
};

Meter* Meter_new(struct ProcessList_* pl, int param, MeterClass* type) {
   Meter* this = xCalloc(1, sizeof(Meter));
   Object_setClass(this, type);
   this->h = 1;
   this->param = param;
   this->pl = pl;
   type->curItems = type->maxItems;
   this->values = xCalloc(type->maxItems, sizeof(double));
   this->total = type->total;
   this->caption = xStrdup(type->caption);
   if (Meter_initFn(this))
      Meter_init(this);
   Meter_setMode(this, type->defaultMode);
   return this;
}

int Meter_humanUnit(char* buffer, unsigned long int value, int size) {
   const char * prefix = "KMGTPEZY";
   unsigned long int powi = 1;
   unsigned int written, powj = 1, precision = 2;

   for(;;) {
      if (value / 1024 < powi)
         break;

      if (prefix[1] == 0)
         break;

      powi *= 1024;
      ++prefix;
   }

   if (*prefix == 'K')
      precision = 0;

   for (; precision > 0; precision--) {
      powj *= 10;
      if (value / powi < powj)
         break;
   }

   written = snprintf(buffer, size, "%.*f%c",
      precision, (double) value / powi, *prefix);

   return written;
}

void Meter_delete(Object* cast) {
   if (!cast)
      return;
   Meter* this = (Meter*) cast;
   if (Meter_doneFn(this)) {
      Meter_done(this);
   }
   free(this->drawData);
   free(this->caption);
   free(this->values);
   free(this);
}

void Meter_setCaption(Meter* this, const char* caption) {
   free(this->caption);
   this->caption = xStrdup(caption);
}

static inline void Meter_displayBuffer(Meter* this, char* buffer, RichString* out) {
   if (Object_displayFn(this)) {
      Object_display(this, out);
   } else {
      RichString_write(out, CRT_colors[Meter_attributes(this)[0]], buffer);
   }
}

void Meter_setMode(Meter* this, int modeIndex) {
   if (modeIndex > 0 && modeIndex == this->mode)
      return;
   if (!modeIndex)
      modeIndex = 1;
   assert(modeIndex < LAST_METERMODE);
   if (Meter_defaultMode(this) == CUSTOM_METERMODE) {
      this->draw = Meter_drawFn(this);
      if (Meter_updateModeFn(this))
         Meter_updateMode(this, modeIndex);
   } else {
      assert(modeIndex >= 1);
      free(this->drawData);
      this->drawData = NULL;

      MeterMode* mode = Meter_modes[modeIndex];
      this->draw = mode->draw;
      this->h = mode->h;
   }
   this->mode = modeIndex;
}

ListItem* Meter_toListItem(Meter* this, bool moving) {
   char mode[21];
   if (this->mode)
      snprintf(mode, 20, " [%s]", Meter_modes[this->mode]->uiName);
   else
      mode[0] = '\0';
   char number[11];
   if (this->param > 0)
      snprintf(number, 10, " %d", this->param);
   else
      number[0] = '\0';
   char buffer[51];
   snprintf(buffer, 50, "%s%s%s", Meter_uiName(this), number, mode);
   ListItem* li = ListItem_new(buffer, 0);
   li->moving = moving;
   return li;
}

/* ---------- TextMeterMode ---------- */

static void TextMeterMode_draw(Meter* this, int x, int y, int w) {
   char buffer[METER_BUFFER_LEN];
   Meter_updateValues(this, buffer, METER_BUFFER_LEN - 1);
   (void) w;

   attrset(CRT_colors[METER_TEXT]);
   mvaddstr(y, x, this->caption);
   int captionLen = strlen(this->caption);
   x += captionLen;
   attrset(CRT_colors[RESET_COLOR]);
   RichString_begin(out);
   Meter_displayBuffer(this, buffer, &out);
   RichString_printVal(out, y, x);
   RichString_end(out);
}

/* ---------- BarMeterMode ---------- */

static const char BarMeterMode_characters[] = "|#*@$%&.";

static void BarMeterMode_draw(Meter* this, int x, int y, int w) {
   char buffer[METER_BUFFER_LEN];
   Meter_updateValues(this, buffer, METER_BUFFER_LEN - 1);

   w -= 2;
   attrset(CRT_colors[METER_TEXT]);
   int captionLen = 3;
   mvaddnstr(y, x, this->caption, captionLen);
   x += captionLen;
   w -= captionLen;
   attrset(CRT_colors[BAR_BORDER]);
   mvaddch(y, x, '[');
   mvaddch(y, x + w, ']');
   
   w--;
   x++;

   if (w < 1) {
      attrset(CRT_colors[RESET_COLOR]);
      return;
   }
   char bar[w + 1];
   
   int blockSizes[10];

   snprintf(bar, w + 1, "%*s", w, buffer);

   // First draw in the bar[] buffer...
   int offset = 0;
   int items = Meter_getItems(this);
   for (int i = 0; i < items; i++) {
      double value = this->values[i];
      value = CLAMP(value, 0.0, this->total);
      if (value > 0) {
         blockSizes[i] = ceil((value/this->total) * w);
      } else {
         blockSizes[i] = 0;
      }
      int nextOffset = offset + blockSizes[i];
      // (Control against invalid values)
      nextOffset = CLAMP(nextOffset, 0, w);
      for (int j = offset; j < nextOffset; j++)
         if (bar[j] == ' ') {
            if (CRT_colorScheme == COLORSCHEME_MONOCHROME) {
               bar[j] = BarMeterMode_characters[i];
            } else {
               bar[j] = '|';
            }
         }
      offset = nextOffset;
   }

   // ...then print the buffer.
   offset = 0;
   for (int i = 0; i < items; i++) {
      attrset(CRT_colors[Meter_attributes(this)[i]]);
      mvaddnstr(y, x + offset, bar + offset, blockSizes[i]);
      offset += blockSizes[i];
      offset = CLAMP(offset, 0, w);
   }
   if (offset < w) {
      attrset(CRT_colors[BAR_SHADOW]);
      mvaddnstr(y, x + offset, bar + offset, w - offset);
   }

   move(y, x + w + 1);
   attrset(CRT_colors[RESET_COLOR]);
}

/* ---------- GraphMeterMode ---------- */

/*{
typedef enum {
   GRAPHSTYLE_ASCII = 0, // Fallback default
   GRAPHSTYLE_UTF8 = 1,  // UTF-8 default (braille)
   LAST_GRAPHSTYLE       // Dummy & unused.
} GraphStyleId;
}*/

static const char *const GraphMeterMode_styles[] = {
   /* 1st byte: num of quantization levels per row per character. (pixPerRow)
    * 2nd byte: num of bytes per multibyte character. Every character to be
    * printed must be padded to this num of bytes.
    * Values of both fields should be <128 (otherwise behavior is undefined).
    * 3rd and 4th bytes are reserved.
    * The size of a whole string should be equal to
    * (4 + (pixPerRow + 1)^2 * size)
    * Total quantization levels will be (pixPerRow * GRAPH_HEIGHT)
    */
   [GRAPHSTYLE_ASCII] = ( /*pixPerRow*/"\2" /*size*/"\1" /*padding*/"\0\0"
   /*00*/" " /*01*/"." /*02*/":"
   /*10*/"." /*11*/"." /*12*/":"
   /*20*/":" /*21*/":" /*22*/":"),
#if HAVE_LIBNCURSESW
   [GRAPHSTYLE_UTF8] = (  /*pixPerRow*/"\4" /*size*/"\4" /*padding*/"\0\0"
   /*00 [ ]       01 [⢀]        02 [⢠]        03 [⢰]        04 [⢸]*/
   " \0\0\0"     "\xe2\xa2\x80\0\xe2\xa2\xa0\0\xe2\xa2\xb0\0\xe2\xa2\xb8\0"
   /*10 [⡀]       11 [⣀]        12 [⣠]        13 [⣰]        14 [⣸]*/
   "\xe2\xa1\x80\0\xe2\xa3\x80\0\xe2\xa3\xa0\0\xe2\xa3\xb0\0\xe2\xa3\xb8\0"
   /*20 [⡄]       21 [⣄]        22 [⣤]        23 [⣴]        24 [⣼]*/
   "\xe2\xa1\x84\0\xe2\xa3\x84\0\xe2\xa3\xa4\0\xe2\xa3\xb4\0\xe2\xa3\xbc\0"
   /*30 [⡆]       31 [⣆]        32 [⣦]        33 [⣶]        34 [⣾]*/
   "\xe2\xa1\x86\0\xe2\xa3\x86\0\xe2\xa3\xa6\0\xe2\xa3\xb6\0\xe2\xa3\xbe\0"
   /*40 [⡇]       41 [⣇]        42 [⣧]        43 [⣷]        44 [⣿]*/
   "\xe2\xa1\x87\0\xe2\xa3\x87\0\xe2\xa3\xa7\0\xe2\xa3\xb7\0\xe2\xa3\xbf"),
   /* XXX: Can we make this 3 bytes per character? Not sure about how data
    * alignment can affect performance. */
#endif // HAVE_LIBNCURSESW
#if 0
   /* Pre-2.0 style perserved here for enthusiasts and also as an example.
    * Note that we avoid '*' and '~' because different fonts render these
    * characters at different heights.
    */
   [GRAPHSTYLE_OLD] = ( /*pixPerRow*/"\6" /*size*/"\1" /*padding*/"\0\0"
   " _.-'`:"
   "_,.-'`:"
   "...-'`:"
   "----'`:"
   "'''''`:"
   "``````:"
   ":::::::"),
#endif // unused
};

static void GraphMeterMode_draw(Meter* this, int x, int y, int w) {

   if (!this->drawData) this->drawData = xCalloc(1, sizeof(GraphData));
    GraphData* data = (GraphData*) this->drawData;
   const int nValues = METER_BUFFER_LEN;

   const char *graphStyle;
#ifdef HAVE_LIBNCURSESW
   if (CRT_utf8) {
      graphStyle = GraphMeterMode_styles[GRAPHSTYLE_UTF8];
   } else
#endif
   {
      graphStyle = GraphMeterMode_styles[GRAPHSTYLE_ASCII];
   }
   int pixPerRow = (int) graphStyle[0];

   attrset(CRT_colors[METER_TEXT]);
   int captionLen = 3;
   mvaddnstr(y, x, this->caption, captionLen);
   x += captionLen;
   w -= captionLen;
   
   struct timeval now;
   gettimeofday(&now, NULL);
   if (!timercmp(&now, &(data->time), <)) {
      struct timeval delay = { .tv_sec = (int)(CRT_delay/10), .tv_usec = (CRT_delay-((int)(CRT_delay/10)*10)) * 100000 };
      timeradd(&now, &delay, &(data->time));

      for (int i = 0; i < nValues - 1; i++)
         data->values[i] = data->values[i+1];
   
      char buffer[nValues];
      Meter_updateValues(this, buffer, nValues - 1);
   
      double value = 0.0;
      int items = Meter_getItems(this);
      for (int i = 0; i < items; i++)
         value += this->values[i];
      value /= this->total;
      data->values[nValues - 1] = value;
   }
   
   int i = nValues - (w*2) + 2, k = 0;
   if (i < 0) {
      k = -i/2;
      i = 0;
   }
   for (; i < nValues; i+=2, k++) {
      int pix = pixPerRow * GRAPH_HEIGHT;
      int v1 = CLAMP(data->values[i] * pix, 1, pix);
      int v2 = CLAMP(data->values[i+1] * pix, 1, pix);

      int colorIdx = GRAPH_1;
      for (int line = 0; line < GRAPH_HEIGHT; line++) {
         int line1 = CLAMP(v1 - (pixPerRow * (GRAPH_HEIGHT - 1 - line)), 0, pixPerRow);
         int line2 = CLAMP(v2 - (pixPerRow * (GRAPH_HEIGHT - 1 - line)), 0, pixPerRow);

         attrset(CRT_colors[colorIdx]);
         const char *str = graphStyle + 4 + ((int) graphStyle[1]) *
                           ((line1 * (pixPerRow + 1) + line2));
         mvaddnstr(y+line, x+k, str, (int) graphStyle[1]);
         colorIdx = GRAPH_2;
      }
   }
   attrset(CRT_colors[RESET_COLOR]);
}

/* ---------- LEDMeterMode ---------- */

static const char* LEDMeterMode_digitsAscii[] = {
   " __ ","    "," __ "," __ ","    "," __ "," __ "," __ "," __ "," __ ",
   "|  |","   |"," __|"," __|","|__|","|__ ","|__ ","   |","|__|","|__|",
   "|__|","   |","|__ "," __|","   |"," __|","|__|","   |","|__|"," __|"
};

#ifdef HAVE_LIBNCURSESW

static const char* LEDMeterMode_digitsUtf8[] = {
   "┌──┐","  ┐ ","╶──┐","╶──┐","╷  ╷","┌──╴","┌──╴","╶──┐","┌──┐","┌──┐",
   "│  │","  │ ","┌──┘"," ──┤","└──┤","└──┐","├──┐","   │","├──┤","└──┤",
   "└──┘","  ╵ ","└──╴","╶──┘","   ╵","╶──┘","└──┘","   ╵","└──┘"," ──┘"
};

#endif

static const char** LEDMeterMode_digits;

static void LEDMeterMode_drawDigit(int x, int y, int n) {
   for (int i = 0; i < 3; i++)
      mvaddstr(y+i, x, LEDMeterMode_digits[i * 10 + n]);
}

static void LEDMeterMode_draw(Meter* this, int x, int y, int w) {
   (void) w;

#ifdef HAVE_LIBNCURSESW
   if (CRT_utf8)
      LEDMeterMode_digits = LEDMeterMode_digitsUtf8;
   else
#endif
      LEDMeterMode_digits = LEDMeterMode_digitsAscii;

   char buffer[METER_BUFFER_LEN];
   Meter_updateValues(this, buffer, METER_BUFFER_LEN - 1);
   
   RichString_begin(out);
   Meter_displayBuffer(this, buffer, &out);

   int yText =
#ifdef HAVE_LIBNCURSESW
      CRT_utf8 ? y+1 :
#endif
      y+2;
   attrset(CRT_colors[LED_COLOR]);
   mvaddstr(yText, x, this->caption);
   int xx = x + strlen(this->caption);
   int len = RichString_sizeVal(out);
   for (int i = 0; i < len; i++) {
      char c = RichString_getCharVal(out, i);
      if (c >= '0' && c <= '9') {
         LEDMeterMode_drawDigit(xx, y, c-48);
         xx += 4;
      } else {
         mvaddch(yText, xx, c);
         xx += 1;
      }
   }
   attrset(CRT_colors[RESET_COLOR]);
   RichString_end(out);
}

static MeterMode BarMeterMode = {
   .uiName = "Bar",
   .h = 1,
   .draw = BarMeterMode_draw,
};

static MeterMode TextMeterMode = {
   .uiName = "Text",
   .h = 1,
   .draw = TextMeterMode_draw,
};

static MeterMode GraphMeterMode = {
   .uiName = "Graph",
   .h = GRAPH_HEIGHT,
   .draw = GraphMeterMode_draw,
};

static MeterMode LEDMeterMode = {
   .uiName = "LED",
   .h = 3,
   .draw = LEDMeterMode_draw,
};

MeterMode* Meter_modes[] = {
   NULL,
   &BarMeterMode,
   &TextMeterMode,
   &GraphMeterMode,
   &LEDMeterMode,
   NULL
};

/* Blank meter */

static void BlankMeter_updateValues(Meter* this, char* buffer, int size) {
   (void) this; (void) buffer; (void) size;
}

static void BlankMeter_display(Object* cast, RichString* out) {
   (void) cast;
   RichString_prune(out);
}

int BlankMeter_attributes[] = {
   DEFAULT_COLOR
};

MeterClass BlankMeter_class = {
   .super = {
      .extends = Class(Meter),
      .delete = Meter_delete,
      .display = BlankMeter_display,
   },
   .updateValues = BlankMeter_updateValues,
   .defaultMode = TEXT_METERMODE,
   .maxItems = 0,
   .total = 100.0,
   .attributes = BlankMeter_attributes,
   .name = "Blank",
   .uiName = "Blank",
   .caption = ""
};
