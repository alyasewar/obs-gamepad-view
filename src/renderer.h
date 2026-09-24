#pragma once

#include <graphics/graphics.h>

#include "input_state.h"
#include "layout.h"
#include "theme_loader.h"

// Smoothed mirror of IvInputState. Input is sampled at tick rate but presses
// last only a few frames; easing towards the raw state keeps a tap visible and
// stops a 60 Hz overlay from strobing.
struct IvAnim {
  float part[IV_PART_COUNT] = {};
  float key[IV_KEY_COUNT] = {};
  float l3 = 0.0f, r3 = 0.0f;
  float lx = 0.0f, ly = 0.0f, rx = 0.0f, ry = 0.0f;
  // 0 while no pad is attached, 1 once attached; fades so hot-plugging a
  // controller mid-stream does not pop the overlay into frame.
  float presence = 0.0f;
};

void iv_anim_reset(IvAnim &anim);

// `dt` is seconds since the previous tick.
void iv_anim_step(IvAnim &anim, const IvInputState &state, const IvTheme &theme, float dt);

struct IvRenderTargets {
  gs_texture_t *base = nullptr;
  gs_texture_t *overlay = nullptr;
  gs_effect_t *tint = nullptr;
  // Texels per canvas unit; the sheets are rasterised above 1:1 so the overlay
  // stays crisp when scaled up in a scene.
  float asset_scale = 1.0f;
};

// Draws the pad into a `width` x `height` box. Returns false if it could not
// draw (missing textures), so the caller can fall back to a placeholder.
bool iv_render_pad(const IvRenderTargets &targets, const IvLayout &layout,
                   const IvAnim &anim, float width, float height, float opacity);

// Optional WASD + modifier + mouse strip, drawn from solid rects rather than
// sheet art. Laid out to fill the given box.
void iv_render_keys(const IvAnim &anim, const IvTheme &theme, float x, float y,
                    float width, float height, float opacity, bool keyboard,
                    bool mouse);

// Flat fill used for the optional backdrop.
void iv_fill_rect(float x, float y, float w, float h, uint32_t colour, float opacity);
