#pragma once

#include <string>

#include "input_state.h"

// A sprite sliced out of the overlay sheet. `src` is its rect on the sheet,
// `dst` the top-left corner it paints at on the controller canvas. They match
// for parts drawn in place; sprites parked in the sheet's spare strip (the
// shoulder arcs) carry a different dst.
struct IvSprite {
  bool present = false;
  float sx = 0.0f, sy = 0.0f, sw = 0.0f, sh = 0.0f;
  float dx = 0.0f, dy = 0.0f;
};

struct IvStickLayout {
  bool present = false;
  float cx = 0.0f, cy = 0.0f;
  IvSprite neutral;
  IvSprite pressed;
};

// Geometry for one controller type, loaded from controllers/<pad>/layout.json.
struct IvLayout {
  bool loaded = false;
  std::string id;
  std::string name;
  float canvas_w = 1000.0f;
  float canvas_h = 680.0f;
  float sheet_w = 1000.0f;
  float sheet_h = 1000.0f;
  float stick_travel = 22.0f;

  IvSprite parts[IV_PART_COUNT];
  IvStickLayout left_stick;
  IvStickLayout right_stick;

  std::string base_png;
  std::string overlay_png;
};

// `assets_root` is the plugin's data directory. Returns a layout with
// loaded == false if the manifest is missing or malformed; callers should fall
// back rather than treat that as fatal.
IvLayout iv_layout_load(const std::string &pad_id, const std::string &theme_id,
                        const std::string &assets_root);
