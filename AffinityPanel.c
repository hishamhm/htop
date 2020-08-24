/*
htop - AffinityPanel.c
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "AffinityPanel.h"
#include "CRT.h"

#include "Vector.h"

#include <assert.h>
#include <string.h>

#ifdef HAVE_LIBHWLOC
#include <hwloc.h>
#endif

/*{
#include "Panel.h"
#include "Affinity.h"
#include "ProcessList.h"

}*/

typedef struct MaskItem_ {
   Object super;
   const char* text;
   const char* indent;
   int value; /* tri-state: 0 - off, 1 - some set, 2 - all set */
   int sub_tree; /* tri-state: 0 - no sub-tree, 1 - open sub-tree, 2 - closed sub-tree */
   Vector *children;
   #ifdef HAVE_LIBHWLOC
   bool ownCpuset;
   hwloc_bitmap_t cpuset;
   #else
   int cpu;
   #endif
} MaskItem;

static void MaskItem_delete(Object* cast) {
   MaskItem* this = (MaskItem*) cast;
   free((void*)this->text);
   if (this->indent)
      free((void*)this->indent);
   Vector_delete(this->children);
   #ifdef HAVE_LIBHWLOC
   if (this->ownCpuset)
      hwloc_bitmap_free(this->cpuset);
   #endif
   free(this);
}

static void MaskItem_display(Object* cast, RichString* out) {
   MaskItem* this = (MaskItem*)cast;
   assert (this != NULL);
   if (this->value == 2)
      RichString_append(out, CRT_colors[CHECK_MARK], CRT_checkStr[CHECK_STR_FULL]);
   else if (this->value == 1)
      RichString_append(out, CRT_colors[CHECK_MARK], CRT_checkStr[CHECK_STR_PARTIAL]);
   else
      RichString_append(out, CRT_colors[CHECK_MARK], CRT_checkStr[CHECK_STR_NONE]);
   RichString_append(out, CRT_colors[CHECK_TEXT], " ");
   if (this->indent)
      RichString_append(out, CRT_colors[PROCESS_TREE], this->indent);
   if (this->sub_tree) {
      RichString_append(out, CRT_colors[PROCESS_TREE],
                        this->sub_tree == 1
                        ? CRT_collapStr[COLLAP_STR_OPEN]
                        : CRT_collapStr[COLLAP_STR_CLOSE]);
      RichString_append(out, CRT_colors[CHECK_TEXT], " ");
   }
   RichString_append(out, CRT_colors[CHECK_TEXT], this->text);
}

static ObjectClass MaskItem_class = {
   .display = MaskItem_display,
   .delete  = MaskItem_delete
};

#ifdef HAVE_LIBHWLOC

static MaskItem* MaskItem_newMask(const char* text, const char* indent, hwloc_bitmap_t cpuset, bool owner) {
   MaskItem* this = AllocThis(MaskItem);
   this->text = xStrdup(text);
   this->indent = xStrdup(indent);
   this->value = 0;
   this->ownCpuset = owner;
   this->cpuset = cpuset;
   this->sub_tree = hwloc_bitmap_weight(cpuset) > 1 ? 1 : 0;
   this->children = Vector_new(Class(MaskItem), true, DEFAULT_SIZE);
   return this;
}

#endif

static MaskItem* MaskItem_newSingleton(const char* text, int cpu, bool isSet) {
   MaskItem* this = AllocThis(MaskItem);
   this->text = xStrdup(text);
   this->indent = NULL;
   this->sub_tree = 0;
   this->children = Vector_new(Class(MaskItem), true, DEFAULT_SIZE);

   #ifdef HAVE_LIBHWLOC
   this->ownCpuset = true;
   this->cpuset = hwloc_bitmap_alloc();
   hwloc_bitmap_set(this->cpuset, cpu);
   (void)isSet;
   #else
   this->cpu = cpu;
   #endif
   this->value = 2 * isSet;

   return this;
}

typedef struct AffinityPanel_ {
   Panel super;
   ProcessList* pl;
   bool topoView;
   Vector *cpuids;
   unsigned width;

   #ifdef HAVE_LIBHWLOC
   MaskItem *topoRoot;
   hwloc_const_cpuset_t allCpuset;
   hwloc_bitmap_t workCpuset;
   #endif
} AffinityPanel;

static void AffinityPanel_delete(Object* cast) {
   AffinityPanel* this = (AffinityPanel*) cast;
   Panel* super = (Panel*) this;
   Panel_done(super);
   Vector_delete(this->cpuids);
   #ifdef HAVE_LIBHWLOC
   hwloc_bitmap_free(this->workCpuset);
   MaskItem_delete((Object*) this->topoRoot);
   #endif
   free(this);
}

#ifdef HAVE_LIBHWLOC

static void AffinityPanel_updateItem(AffinityPanel* this, MaskItem* item) {
   Panel* super = (Panel*) this;

   item->value = hwloc_bitmap_isincluded(item->cpuset, this->workCpuset) ? 2 :
                 hwloc_bitmap_intersects(item->cpuset, this->workCpuset) ? 1 : 0;

   Panel_add(super, (Object*) item);
}

static void AffinityPanel_updateTopo(AffinityPanel* this, MaskItem* item) {
   AffinityPanel_updateItem(this, item);

   if (item->sub_tree == 2)
      return;

   for (int i = 0; i < Vector_size(item->children); i++)
      AffinityPanel_updateTopo(this, (MaskItem*) Vector_get(item->children, i));
}

#endif

static void AffinityPanel_update(AffinityPanel* this, bool keepSelected) {
   Panel* super = (Panel*) this;

   FunctionBar_setLabel(super->currentBar, KEY_F(3), this->topoView ? "Collapse/Expand" : "");
   FunctionBar_draw(super->currentBar, NULL);

   int oldSelected = Panel_getSelectedIndex(super);
   Panel_prune(super);

   #ifdef HAVE_LIBHWLOC
   if (this->topoView)
      AffinityPanel_updateTopo(this, this->topoRoot);
   else {
      for (int i = 0; i < Vector_size(this->cpuids); i++)
         AffinityPanel_updateItem(this, (MaskItem*) Vector_get(this->cpuids, i));
   }
   #else
   Panel_splice(super, this->cpuids);
   #endif

   if (keepSelected)
      Panel_setSelected(super, oldSelected);

   super->needsRedraw = true;
}

static HandlerResult AffinityPanel_eventHandler(Panel* super, int ch) {
   AffinityPanel* this = (AffinityPanel*) super;
   HandlerResult result = IGNORED;
   MaskItem* selected = (MaskItem*) Panel_getSelected(super);
   bool keepSelected = true;

   switch(ch) {
   case KEY_MOUSE:
   case KEY_RECLICK:
   case ' ':
      #ifdef HAVE_LIBHWLOC
      if (selected->value == 2) {
         /* Item was selected, so remove this mask from the top cpuset. */
         hwloc_bitmap_andnot(this->workCpuset, this->workCpuset, selected->cpuset);
         selected->value = 0;
      } else {
         /* Item was not or only partial selected, so set all bits from this object
            in the top cpuset. */
         hwloc_bitmap_or(this->workCpuset, this->workCpuset, selected->cpuset);
         selected->value = 2;
      }
      #else
      selected->value = 2 * !selected->value; /* toggle between 0 and 2 */
      #endif

      result = HANDLED;
      break;

   #ifdef HAVE_LIBHWLOC

   case KEY_F(1):
      hwloc_bitmap_copy(this->workCpuset, this->allCpuset);
      result = HANDLED;
      break;

   case KEY_F(2):
      this->topoView = !this->topoView;
      keepSelected = false;

      result = HANDLED;
      break;

   case KEY_F(3):
   case '-':
   case '+':
      if (selected->sub_tree)
         selected->sub_tree = 1 + !(selected->sub_tree - 1); /* toggle between 1 and 2 */

      result = HANDLED;
      break;

   #endif

   case 0x0a:
   case 0x0d:
   case KEY_ENTER:
      result = BREAK_LOOP;
      break;
   }

   if (HANDLED == result)
      AffinityPanel_update(this, keepSelected);

   return result;
}

#ifdef HAVE_LIBHWLOC

static const char *AffinityPanel_nameOfObject(hwloc_obj_type_t obj) {
   switch (obj)
   {
   case HWLOC_OBJ_SYSTEM:  return "System #";
   case HWLOC_OBJ_MACHINE: return "Machine #";
   case HWLOC_OBJ_MISC:    return "Misc #";
   case HWLOC_OBJ_NODE:    return "Node #";
   case HWLOC_OBJ_SOCKET:  return "Socket #";
   case HWLOC_OBJ_CACHE:   return "Cache #";
   case HWLOC_OBJ_CORE:    return "Core #";
   case HWLOC_OBJ_PU:      return "CPU ";
   default:                return "Unknown #";
   }
}

static MaskItem *AffinityPanel_addObject(AffinityPanel* this, hwloc_obj_t obj, unsigned indent, MaskItem *parent) {
   const char* type_name = AffinityPanel_nameOfObject(obj->type);
   unsigned depth = obj->depth;
   unsigned index = obj->logical_index;
   size_t off = 0, left = 10 * depth;
   char buf[64], indent_buf[left + 1];

   if (obj->type == HWLOC_OBJ_PU)
      index = Settings_cpuId(this->pl->settings, obj->os_index);

   indent_buf[0] = '\0';
   if (depth > 0) {
      for (unsigned i = 1; i < depth; i++) {
         xSnprintf(&indent_buf[off], left, "%s  ", (indent & (1u << i)) ? CRT_treeStr[TREE_STR_VERT] : " ");
         size_t len = strlen(&indent_buf[off]);
         off += len, left -= len;
      }
      xSnprintf(&indent_buf[off], left, "%s%s ",
             obj->next_sibling ? CRT_treeStr[TREE_STR_RTEE] : CRT_treeStr[TREE_STR_BEND],
             CRT_treeStr[TREE_STR_HORZ]);
      size_t len = strlen(&indent_buf[off]);
      off += len, left -= len;
   }

   xSnprintf(buf, 64, "%s%u", type_name, index);

   MaskItem *item = MaskItem_newMask(buf, indent_buf, obj->online_cpuset, false);
   if (parent)
      Vector_add(parent->children, item);

   unsigned width = 4 + 3 * depth + (item->sub_tree ? 4 : 0) + strlen(buf);
   if (width > this->width)
      this->width = width;

   return item;
}

static MaskItem *AffinityPanel_buildTopology(AffinityPanel* this, hwloc_obj_t obj, unsigned indent, MaskItem *parent) {
   MaskItem *item = AffinityPanel_addObject(this, obj, indent, parent);
   if (obj->next_sibling) {
      indent |= (1u << obj->depth);
   } else {
      indent &= ~(1u << obj->depth);
   }
   for (unsigned i = 0; i < obj->arity; i++)
      AffinityPanel_buildTopology(this, obj->children[i], indent, item);

   return parent == NULL ? item : NULL;
}

#endif

PanelClass AffinityPanel_class = {
   .super = {
      .extends = Class(Panel),
      .delete = AffinityPanel_delete
   },
   .eventHandler = AffinityPanel_eventHandler
};

static const char* const AffinityPanelFunctions[] = {
   "Set    ",
   "Cancel ",
   #ifdef HAVE_LIBHWLOC
   "All",
   "Topology",
   "               ",
   #endif
   NULL
};
static const char* const AffinityPanelKeys[] = {"Enter", "Esc", "F1", "F2", "F3"};
static const int AffinityPanelEvents[] = {13, 27, KEY_F(1), KEY_F(2), KEY_F(3)};

Panel* AffinityPanel_new(ProcessList* pl, Affinity* affinity, int* width) {
   AffinityPanel* this = AllocThis(AffinityPanel);
   Panel* super = (Panel*) this;
   Panel_init(super, 1, 1, 1, 1, Class(MaskItem), false, FunctionBar_new(AffinityPanelFunctions, AffinityPanelKeys, AffinityPanelEvents));

   this->pl = pl;
   this->width = 15;

   this->cpuids   = Vector_new(Class(MaskItem), true, DEFAULT_SIZE);
   this->topoView = false;

   #ifdef HAVE_LIBHWLOC
   //this->topology = Vector_new(Class(MaskItem), true, DEFAULT_SIZE);
   this->allCpuset  = hwloc_topology_get_online_cpuset(pl->topology);
   this->workCpuset = hwloc_bitmap_alloc();
   #endif

   Panel_setHeader(super, "Use CPUs:");

   int curCpu = 0;
   for (int i = 0; i < pl->cpuCount; i++) {
      char number[16];
      xSnprintf(number, 9, "CPU %d", Settings_cpuId(pl->settings, i));
      unsigned width = 4 + strlen(number);
      if (width > this->width)
         this->width = width;

      bool isSet = false;
      if (curCpu < affinity->used && affinity->cpus[curCpu] == i) {
         #ifdef HAVE_LIBHWLOC
         hwloc_bitmap_set(this->workCpuset, i);
         #endif
         isSet = true;
         curCpu++;
      }

      MaskItem* cpuItem = MaskItem_newSingleton(number, i, isSet);
      Vector_add(this->cpuids, (Object*) cpuItem);
   }

   #ifdef HAVE_LIBHWLOC
   this->topoRoot = AffinityPanel_buildTopology(this, hwloc_get_root_obj(pl->topology), 0, NULL);
   #endif

   if (width)
      *width = this->width;

   AffinityPanel_update(this, false);

   return super;
}

Affinity* AffinityPanel_getAffinity(Panel* super, ProcessList* pl) {
   AffinityPanel* this = (AffinityPanel*) super;
   Affinity* affinity = Affinity_new(pl);

   #ifdef HAVE_LIBHWLOC
   int i;
   hwloc_bitmap_foreach_begin(i, this->workCpuset)
      Affinity_add(affinity, i);
   hwloc_bitmap_foreach_end();
   #else
   for (int i = 0; i < this->pl->cpuCount; i++) {
      MaskItem* item = (MaskItem*)Vector_get(this->cpuids, i);
      if (item->value)
         Affinity_add(affinity, item->cpu);
   }
   #endif

   return affinity;
}
