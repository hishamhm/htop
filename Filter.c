/*
htop - Filter.c
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h"

#include "XAlloc.h"
#include "StringUtils.h"

#include <string.h>
#include <stdlib.h>
#include <regex.h>

#include "Filter.h"

/*{
#include <regex.h>
#include <stdbool.h>

typedef struct Filter_ {
   bool regex;
   bool exclude;
   char* rawFilter;
   regex_t regexFilter;
} Filter;

}*/

static Filter* cachedFilter = 0;

Filter* Filter_new(const char* filterString) {
   // reuse cached filter for common case
   if (cachedFilter && strcmp(filterString, cachedFilter->rawFilter)==0) {
      return cachedFilter;
   }
   
   // build a new filter object
   Filter* this = xCalloc(1, sizeof(Filter));
   if (filterString[0]=='$') {
      if (filterString[1]=='!') {
         this->rawFilter = xStrdup(filterString+2);
         this->exclude = true;
      } else {
         this->rawFilter = xStrdup(filterString+1);
      }
   } else {
      if (filterString[0]=='!') {
         this->rawFilter = xStrdup(filterString+1);
         this->exclude = true;
      } else {
         this->rawFilter = xStrdup(filterString);
      }
      int result = regcomp(&this->regexFilter, this->rawFilter, REG_EXTENDED|REG_NOSUB|REG_ICASE);
      if (!result) {
         this->regex = true;
      }
   }
   return this;
}

bool Filter_test(Filter* this, const char* str) {
   if (!this) {
      return true;
   }
   if (this->regex) {
      return regexec(&this->regexFilter, str, 0, NULL, 0) ? this->exclude : !this->exclude;
   }
   return  String_contains_i(str, this->rawFilter) ? !this->exclude : this->exclude;
}

void Filter_delete(Filter* this) {
   if (this==cachedFilter) {
      return;
   }
   if (this) {
      if (cachedFilter) {
         if (cachedFilter->regex) {
            regfree(&cachedFilter->regexFilter);
         }
         free(cachedFilter->rawFilter);
         free(cachedFilter);
      }
      cachedFilter = this;
   }
}
