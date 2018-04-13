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
#include <float.h>
#include <assert.h>
#include <sys/time.h>

#define METER_BUFFER_LEN 256

#define GRAPH_DELAY (DEFAULT_DELAY/2)

#define GRAPH_HEIGHT 4 /* Unit: rows (lines) */
#define GRAPH_NUM_RECORDS 256

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
   // For "full" variable, sign matters.
   // >0: Full/maximum value is stable (at least for a short duration). Will
   //     draw as percent graph. e.g. CPU & swap.
   // <0: No stable maximum. Will draw with dynamic scale. e.g. loadavg.
   // (full == 0) will bring weird behavior for now. Avoid.
   const double full;
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
#define Meter_getMaxItems(this_)       As_Meter(this_)->maxItems
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
   double full;
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
   double* values;
   double* stack1;
   double* stack2;
   int* colors;
   unsigned int colorRowSize;
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
   this->full = type->full;
   this->caption = xStrdup(type->caption);
   if (Meter_initFn(this))
      Meter_init(this);
   Meter_setMode(this, type->defaultMode);
   return this;
}

static const char* Meter_prefixes = "KMGTPEZY";

int Meter_humanUnit(char* buffer, unsigned long int value, int size) {
   const char* prefix = Meter_prefixes;
   unsigned long int powi = 1;
   unsigned int written, powj = 1, precision = 2;

   for(;;) {
      if (value / 1024 < powi)
         break;

      if (prefix[1] == '\0')
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
      xSnprintf(mode, 20, " [%s]", Meter_modes[this->mode]->uiName);
   else
      mode[0] = '\0';
   char number[11];
   if (this->param > 0)
      xSnprintf(number, 10, " %d", this->param);
   else
      number[0] = '\0';
   char buffer[51];
   xSnprintf(buffer, 50, "%s%s%s", Meter_uiName(this), number, mode);
   ListItem* li = ListItem_new(buffer, 0);
   li->moving = moving;
   return li;
}

/* ---------- GraphData ---------- */

static GraphData* GraphData_new(size_t nValues, size_t nItems, bool isPercentGraph) {
   // colors[] is designed to be two-dimensional, but without a column of row
   // pointers (unlike var[m][n] declaration).
   // GraphMeterMode_draw() will print this table in 90-deg counter-clockwise
   // rotated form.
   unsigned int colorRowSize;
   if (nItems <= 1) { // 1 or less item
      colorRowSize = 1;
   } else if (isPercentGraph) { // Percent graph: a linear row of color cells.
      colorRowSize = GRAPH_HEIGHT;
   } else { // Non-percentage & dynamic scale: a binary tree of cells.
      colorRowSize = 2 * GRAPH_HEIGHT;
   }
   GraphData* data = xCalloc(1, sizeof(GraphData) +
                                sizeof(double) * nValues +
                                sizeof(double) * (nItems + 2) * 2 +
                                sizeof(int) * (nValues * colorRowSize));
   data->values = (double*)(data + 1);
   // values[nValues + 1]
   // It's intentional that values[nValues] and stack1[0] share the same cell;
   // the cell's value will be always 0.0.
   data->stack1 = (double*)(data->values + nValues);
   // stack1[nItems + 2]
   data->stack2 = (double*)(data->stack1 + (nItems + 2));
   // stack2[nItems + 2]
   data->colors = (int*)(data->stack2 + (nItems + 2));
   // colors[nValues * colorRowSize]

   // Initialize colors[].
   data->colorRowSize = colorRowSize;
   for (unsigned int i = 0; i < (nValues * colorRowSize); i++) {
      data->colors[i] = BAR_SHADOW;
   }
   return data;
}

static inline void GraphData_delete(GraphData* this) {
   // GraphData is designed to be deleted by a simple free() call,
   // hence you shouldn't call this.
   free(this);
}

static int GraphData_getColor(GraphData* this, int vIndex, int h, int scaleExp) {
   // level step  _________index_________  (tree form)
   //     3    8 |_______________8_______|
   //     2    4 |_______4_______|___10__|
   //     1    2 |___2___|___6___|___10__|
   //     0    1 |_1_|_3_|_5_|_7_|_9_|_11|

   // level step  _________index_________  (linear form (percent graph or
   //     0    1 |_0_|_1_|_2_|_3_|_4_|_5_|  one item))

   if (this->colorRowSize > GRAPH_HEIGHT) {
      const int maxLevel = ((int) log2(GRAPH_HEIGHT - 1)) + 1;
      int exp;
      (void) frexp(MAX(this->values[vIndex], this->values[vIndex + 1]), &exp);
      int level = MIN((scaleExp - exp), maxLevel);
      assert(level >= 0);
      for (int j = 1 << level; j > 0; j >>= 1) {
         size_t offset = (h << (level + 1)) + j;
         if (offset < this->colorRowSize) {
            return this->colors[vIndex * this->colorRowSize + offset];
         }
      }
      return BAR_SHADOW;
   } else if (this->colorRowSize == GRAPH_HEIGHT) {
      return this->colors[vIndex * this->colorRowSize + h];
   } else {
      assert(this->colorRowSize == 1);
      return this->colors[vIndex * this->colorRowSize];
   }
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

   xSnprintf(bar, w + 1, "%*.*s", w, w, buffer);

   // First draw in the bar[] buffer...
   int offset = 0;
   int items = Meter_getItems(this);
   double full = this->full;
   if (full <= 0.0) {
      double sum = 0.0;
      for (int i = 0; i < items; i++) {
         sum += this->values[i];
      }
      if (sum > -(this->full)) {
         this->full = -sum;
      }
      full = -(this->full);
   }
   for (int i = 0; i < items; i++) {
      double value = this->values[i];
      value = CLAMP(value, 0.0, full);
      if (value > 0) {
         blockSizes[i] = ceil((value/full) * w);
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

#ifdef HAVE_LIBNCURSESW

#define PIXPERROW_UTF8 4
static const char* const GraphMeterMode_dotsUtf8[] = {
   /*00*/" ", /*01*/"⢀", /*02*/"⢠", /*03*/"⢰", /*04*/ "⢸",
   /*10*/"⡀", /*11*/"⣀", /*12*/"⣠", /*13*/"⣰", /*14*/ "⣸",
   /*20*/"⡄", /*21*/"⣄", /*22*/"⣤", /*23*/"⣴", /*24*/ "⣼",
   /*30*/"⡆", /*31*/"⣆", /*32*/"⣦", /*33*/"⣶", /*34*/ "⣾",
   /*40*/"⡇", /*41*/"⣇", /*42*/"⣧", /*43*/"⣷", /*44*/ "⣿"
};

#endif

#define PIXPERROW_ASCII 2
static const char* const GraphMeterMode_dotsAscii[] = {
   /*00*/" ", /*01*/".", /*02*/":",
   /*10*/".", /*11*/".", /*12*/":",
   /*20*/":", /*21*/":", /*22*/":"
};

static const char* const* GraphMeterMode_dots;
static int GraphMeterMode_pixPerRow;

static void GraphMeterMode_printScale(unsigned int scaleExp) {
   const char* divisors = "842";
   if (scaleExp > 86) { // > 99 yotta
      return;
   } else if (scaleExp < 10) { // <= 512
      printw("%3d", 1 << scaleExp);
      return;
   } else if (scaleExp % 10 <= 6) { // {1|2|4|8|16|32|64}{K|M|G|T|P|E|Z|Y}
      printw("%2d%c", 1 << (scaleExp % 10),
                      Meter_prefixes[(scaleExp / 10) - 1]);
      return;
   } else {
      // Output like "128K" is more than 3 chars. We express in fractions like
      // "M/8" instead. Likewise for "G/4" (=256M), "T/2" (=512G) etc.
      printw("%c/%c", Meter_prefixes[(scaleExp / 10)],
                      divisors[(scaleExp % 10) - 7]);
      return;
   }
}

static void GraphMeterMode_draw(Meter* this, int x, int y, int w) {
   bool isPercentGraph = (this->full > 0.0);
   if (!this->drawData) {
      this->drawData = (void*) GraphData_new(GRAPH_NUM_RECORDS,
                                             Meter_getMaxItems(this),
                                             isPercentGraph);
   }
   GraphData* data = (GraphData*) this->drawData;

#ifdef HAVE_LIBNCURSESW
   if (CRT_utf8) {
      GraphMeterMode_dots = GraphMeterMode_dotsUtf8;
      GraphMeterMode_pixPerRow = PIXPERROW_UTF8;
   } else
#endif
   {
      GraphMeterMode_dots = GraphMeterMode_dotsAscii;
      GraphMeterMode_pixPerRow = PIXPERROW_ASCII;
   }

   struct timeval now;
   gettimeofday(&now, NULL);
   if (!timercmp(&now, &(data->time), <)) {
      struct timeval delay = { .tv_sec = (int)(CRT_delay/10), .tv_usec = (CRT_delay-((int)(CRT_delay/10)*10)) * 100000 };
      timeradd(&now, &delay, &(data->time));

      for (int i = 0; i < GRAPH_NUM_RECORDS - 1; i++) {
         data->values[i] = data->values[i + 1];
         memcpy(&(data->colors[i * data->colorRowSize]),
                &(data->colors[(i + 1) * data->colorRowSize]),
                sizeof(*data->colors) * data->colorRowSize);
      }
   
      char buffer[METER_BUFFER_LEN];
      Meter_updateValues(this, buffer, METER_BUFFER_LEN - 1);
   
      int items = Meter_getItems(this);

      double *stack1 = data->stack1;
      double *stack2 = data->stack2;
      stack1[0] = stack2[0] = 0.0;
      for (int i = 0; i < items; i++) {
         stack1[i + 1] = stack2[i + 1];
         stack2[i + 1] = stack2[i] + this->values[i];
      }
      // May not assume this->full be constant. (Example: Swap meter when user
      // does swapon/swapoff.)
      stack1[items + 1] = stack2[items + 1];
      stack2[items + 1] = this->full;
      data->values[GRAPH_NUM_RECORDS - 1] = stack2[items];
      double scale;
      if (isPercentGraph) {
         data->values[GRAPH_NUM_RECORDS - 1] /= this->full;
         if (stack1[items + 1] != this->full && stack1[items + 1] > 0) {
            for (int i = 0; i < items; i++) {
               stack1[i + 1] = stack1[i + 1] * this->full / stack1[items + 1];
            }
         }
         scale = this->full;
      } else {
         int exp;
         (void) frexp(MAX(stack1[items], stack2[items]), &exp);
         scale = ldexp(1.0, exp);
      }

      // Determine the dominant color per cell in the graph.
      // O(GRAPH_HEIGHT * items)
      for (int step = 1; ; step <<= 1) {
         double low, high = 0.0;
         for (int h = 0; ; h += step) {
            size_t offset = (data->colorRowSize <= GRAPH_HEIGHT) ? h :
                            (h * 2) + step;
            if (offset >= data->colorRowSize)
               break;

            low = high;
            high = scale * (h + step) / GRAPH_HEIGHT;
            assert(low == scale * (h) / GRAPH_HEIGHT);

            double maxArea = 0.0;
            int color = BAR_SHADOW;
            for (int i = 0; i < items; i++) {
               double area;
               area = CLAMP(stack1[i + 1], low, high) +
                      CLAMP(stack2[i + 1], low, high);
               area -= (CLAMP(stack1[i], low, high) +
                        CLAMP(stack2[i], low, high));
               if (area > maxArea) {
                  maxArea = area;
                  color = Meter_attributes(this)[i];
               }
            }
            data->colors[(GRAPH_NUM_RECORDS - 2) * data->colorRowSize +
                         offset] = color;
         }
         if (data->colorRowSize <= GRAPH_HEIGHT) {
            break;
         } else if (step >= 2 * GRAPH_HEIGHT) {
            break;
         }
      }
   }

   // How many values (and columns) we can draw for this graph.
   const int captionLen = 3;
   w -= captionLen;
   int index = GRAPH_NUM_RECORDS - (w * 2) + 2, col = 0;
   if (index < 0) {
      col = -index / 2;
      index = 0;
   }

   // If it's not percent graph, determine the scale.
   int exp = 0;
   double scale = 1.0;
   if (this->full < 0.0) {
      double max = 1.0 - (DBL_EPSILON / FLT_RADIX); // For minimum scale 1.0.
      for (int j = GRAPH_NUM_RECORDS - 1; j >= index; j--) {
         max = (data->values[j] > max) ? data->values[j] : max;
      }
      (void) frexp(max, &exp);
      scale = ldexp(1.0, exp);
   }

   // Print caption and scale
   move(y, x);
   attrset(CRT_colors[METER_TEXT]);
   if (GRAPH_HEIGHT > 1 && this->full < 0.0) {
      GraphMeterMode_printScale(exp);
      move(y + 1, x);
   }
   addnstr(this->caption, captionLen);
   x += captionLen;

   // Print the graph
   for (; index < GRAPH_NUM_RECORDS - 1; index += 2, col++) {
      int pix = GraphMeterMode_pixPerRow * GRAPH_HEIGHT;
      int v1 = CLAMP((int) lround(data->values[index] / scale * pix), 1, pix);
      int v2 = CLAMP((int) lround(data->values[index + 1] / scale * pix), 1, pix);

      // Vertical bars from bottom up
      for (int h = 0; h < GRAPH_HEIGHT; h++) {
         int line = GRAPH_HEIGHT - 1 - h;
         int col1 = CLAMP(v1 - (GraphMeterMode_pixPerRow * h), 0, GraphMeterMode_pixPerRow);
         int col2 = CLAMP(v2 - (GraphMeterMode_pixPerRow * h), 0, GraphMeterMode_pixPerRow);
         attrset(CRT_colors[GraphData_getColor(data, index, h, exp)]);
         mvaddstr(y+line, x+col, GraphMeterMode_dots[col1 * (GraphMeterMode_pixPerRow + 1) + col2]);
      }
   }
   attrset(CRT_colors[RESET_COLOR]);
}

/* ---------- LEDMeterMode ---------- */

static const char* const LEDMeterMode_digitsAscii[] = {
   " __ ","    "," __ "," __ ","    "," __ "," __ "," __ "," __ "," __ ",
   "|  |","   |"," __|"," __|","|__|","|__ ","|__ ","   |","|__|","|__|",
   "|__|","   |","|__ "," __|","   |"," __|","|__|","   |","|__|"," __|"
};

#ifdef HAVE_LIBNCURSESW

static const char* const LEDMeterMode_digitsUtf8[] = {
   "┌──┐","  ┐ ","╶──┐","╶──┐","╷  ╷","┌──╴","┌──╴","╶──┐","┌──┐","┌──┐",
   "│  │","  │ ","┌──┘"," ──┤","└──┤","└──┐","├──┐","   │","├──┤","└──┤",
   "└──┘","  ╵ ","└──╴","╶──┘","   ╵","╶──┘","└──┘","   ╵","└──┘"," ──┘"
};

#endif

static const char* const* LEDMeterMode_digits;

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
   .full = 1.0,
   .attributes = BlankMeter_attributes,
   .name = "Blank",
   .uiName = "Blank",
   .caption = ""
};
