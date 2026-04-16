/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup sptoolwindow
 *
 * Minimal editor type that hosts Python panels.
 * Designed to be opened in a temporary floating window via
 * SCREEN_OT_temp_space_show with content_only=True.
 */

#include <algorithm>
#include <cstring>

#include "DNA_space_types.h"
#include "MEM_guardedalloc.h"

#include "BLI_listbase.h"
#include "BLI_rect.h"
#include "BLI_string_utf8.h"

#include "BKE_context.hh"
#include "BKE_screen.hh"

#include "ED_screen.hh"
#include "ED_space_api.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "wm_window.hh"

#include "UI_interface.hh"

#include "BLO_read_write.hh"

namespace blender {

/* -------------------------------------------------------------------- */
/** \name Tool Window Space — Callbacks
 * \{ */

static SpaceLink *toolwindow_create(const ScrArea * /*area*/, const Scene * /*scene*/)
{
  SpaceToolWindow *stw;
  ARegion *region;

  stw = MEM_new<SpaceToolWindow>("inittoolwindow");
  stw->spacetype = SPACE_TOOLWINDOW;

  /* main region — the only region */
  region = BKE_area_region_new();
  BLI_addtail(&stw->regionbase, region);
  region->regiontype = RGN_TYPE_WINDOW;
  region->flag |= RGN_FLAG_DYNAMIC_SIZE;

  return reinterpret_cast<SpaceLink *>(stw);
}

static void toolwindow_free(SpaceLink * /*sl*/) {}

static void toolwindow_init(wmWindowManager * /*wm*/, ScrArea * /*area*/) {}

static SpaceLink *toolwindow_duplicate(SpaceLink *sl)
{
  SpaceToolWindow *stw_old = reinterpret_cast<SpaceToolWindow *>(sl);
  SpaceToolWindow *stw_copy = static_cast<SpaceToolWindow *>(MEM_dupalloc(stw_old));
  return reinterpret_cast<SpaceLink *>(stw_copy);
}

static void toolwindow_operatortypes() {}

static void toolwindow_keymap(wmKeyConfig *keyconf)
{
  WM_keymap_ensure(keyconf, "Tool Window", SPACE_TOOLWINDOW, RGN_TYPE_WINDOW);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Tool Window — Main Region
 * \{ */

static void toolwindow_main_region_init(wmWindowManager *wm, ARegion *region)
{
  region->v2d.scroll = V2D_SCROLL_RIGHT | V2D_SCROLL_VERTICAL_HIDE;

  ED_region_panels_init(wm, region);

  region->flag |= RGN_FLAG_INDICATE_OVERFLOW;
}

static void toolwindow_main_region_layout(const bContext *C, ARegion *region)
{
  wmWindow *win = CTX_wm_window(C);
  SpaceToolWindow *stw = reinterpret_cast<SpaceToolWindow *>(CTX_wm_space_data(C));

  ED_region_panels_layout(C, region);

  if (win == nullptr || stw == nullptr || region->panels.first == nullptr) {
    return;
  }

  const int content_height = std::max(
      int(BLI_rctf_size_y(&region->v2d.tot) / UI_SCALE_FAC + 0.5f) + (UI_UNIT_Y / 2),
      int(UI_UNIT_Y));
  const int max_height = (stw->max_size_y > 0) ? stw->max_size_y : win->sizey;
  const int desired_height = std::min(content_height, max_height);

  win->runtime->lock_size_y = true;
  win->runtime->size_lock_y = desired_height;

  if (desired_height > 0 && desired_height != win->sizey) {
    win->runtime->size_lock_apply_pending = true;
    win->runtime->size_lock_apply_pending_y = desired_height;
  }
  else {
    win->runtime->size_lock_apply_pending = false;
    win->runtime->size_lock_apply_pending_y = 0;
  }
}

static void toolwindow_main_region_listener(const wmRegionListenerParams *params)
{
  ARegion *region = params->region;
  const wmNotifier *wmn = params->notifier;

  /* Redraw on common context changes so Python panels stay up-to-date. */
  switch (wmn->category) {
    case NC_SCENE:
    case NC_OBJECT:
    case NC_MATERIAL:
    case NC_GEOM:
    case NC_SPACE:
    case NC_SCREEN:
      ED_region_tag_redraw(region);
      break;
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Tool Window — Blend Read / Write
 * \{ */

static void toolwindow_blend_read_data(BlendDataReader * /*reader*/, SpaceLink * /*sl*/) {}

static void toolwindow_blend_write(BlendWriter *writer, SpaceLink *sl)
{
  writer->write_struct_cast<SpaceToolWindow>(sl);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Tool Window — Registration
 * \{ */

void ED_spacetype_toolwindow()
{
  std::unique_ptr<SpaceType> st = std::make_unique<SpaceType>();
  ARegionType *art;

  st->spaceid = SPACE_TOOLWINDOW;
  STRNCPY_UTF8(st->name, "Tool Window");

  st->create = toolwindow_create;
  st->free = toolwindow_free;
  st->init = toolwindow_init;
  st->duplicate = toolwindow_duplicate;
  st->operatortypes = toolwindow_operatortypes;
  st->keymap = toolwindow_keymap;
  st->blend_read_data = toolwindow_blend_read_data;
  st->blend_write = toolwindow_blend_write;

  /* regions: main window (the only region) */
  art = MEM_new_zeroed<ARegionType>("spacetype toolwindow region");
  art->regionid = RGN_TYPE_WINDOW;
  art->prefsizex = UI_SIDEBAR_PANEL_WIDTH;
  art->init = toolwindow_main_region_init;
  art->layout = toolwindow_main_region_layout;
  art->draw = ED_region_panels_draw;
  art->listener = toolwindow_main_region_listener;
  art->keymapflag = ED_KEYMAP_UI;

  BLI_addhead(&st->regiontypes, art);

  BKE_spacetype_register(std::move(st));
}

/** \} */

}  // namespace blender

