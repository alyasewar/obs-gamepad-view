#include "renderer.h"

#include <math.h>
#include <obs-module.h>

namespace {

// Eases `current` towards `target` at a rate set by how long a full 0->1 sweep
// should take. Framerate independent: the same press looks the same at 30 and
// 144 fps. Rising and falling use separate durations because a press wants to
// register instantly while a release reads better trailing off.
float approach(float current, float target, float dt, float rise_seconds,
               float fall_seconds) {
  const float duration = (target > current) ? rise_seconds : fall_seconds;
  if (duration <= 0.0f) return target;

  const float step = dt / duration;
  const float delta = target - current;
  if (fabsf(delta) <= step) return target;
  return current + (delta > 0.0f ? step : -step);
}

void draw_sprite(const IvRenderTargets &targets, const IvSprite &sprite,
                 float alpha, float dx_override, float dy_override,
                 bool use_override) {
  if (!sprite.present || alpha <= 0.002f) return;

  struct vec4 tint;
  vec4_set(&tint, alpha, alpha, alpha, alpha);
  gs_effect_set_vec4(gs_effect_get_param_by_name(targets.tint, "tint"), &tint);
  gs_effect_set_texture(gs_effect_get_param_by_name(targets.tint, "image"),
                        targets.overlay);

  const float s = targets.asset_scale;
  const float dx = (use_override ? dx_override : sprite.dx) * s;
  const float dy = (use_override ? dy_override : sprite.dy) * s;

  gs_matrix_push();
  gs_matrix_translate3f(dx, dy, 0.0f);
  while (gs_effect_loop(targets.tint, "Draw")) {
    gs_draw_sprite_subregion(targets.overlay, 0,
                             static_cast<uint32_t>(sprite.sx * s + 0.5f),
                             static_cast<uint32_t>(sprite.sy * s + 0.5f),
                             static_cast<uint32_t>(sprite.sw * s + 0.5f),
                             static_cast<uint32_t>(sprite.sh * s + 0.5f));
  }
  gs_matrix_pop();
}

void draw_part(const IvRenderTargets &targets, const IvSprite &sprite, float alpha) {
  draw_sprite(targets, sprite, alpha, 0.0f, 0.0f, false);
}

// Sticks are the one part the renderer positions itself: the cap sprite is
// parked in the sheet's spare strip and painted at an offset from the well's
// centre, so travel is a translation rather than 8 pre-drawn positions.
void draw_stick(const IvRenderTargets &targets, const IvStickLayout &stick,
                float travel, float ax, float ay, float click, float alpha) {
  if (!stick.present || alpha <= 0.002f) return;

  const float cx = stick.cx + ax * travel;
  // Pad axes are y-up; canvas coordinates are y-down.
  const float cy = stick.cy - ay * travel;

  const IvSprite &neutral = stick.neutral;
  const float nx = cx - neutral.sw * 0.5f;
  const float ny = cy - neutral.sh * 0.5f;
  // Cross-fade the pressed cap over the neutral one so a click does not blink.
  draw_sprite(targets, neutral, alpha * (1.0f - click), nx, ny, true);

  if (stick.pressed.present && click > 0.002f) {
    const IvSprite &pressed = stick.pressed;
    draw_sprite(targets, pressed, alpha * click, cx - pressed.sw * 0.5f,
                cy - pressed.sh * 0.5f, true);
  }
}

}  // namespace

void iv_anim_reset(IvAnim &anim) { anim = IvAnim(); }

void iv_anim_step(IvAnim &anim, const IvInputState &state, const IvTheme &theme,
                  float dt) {
  if (dt <= 0.0f) return;
  // A hitch (scene load, disk stall) must not teleport every animation to its
  // target; clamp so the overlay eases in from wherever it was.
  if (dt > 0.1f) dt = 0.1f;

  const float rise = theme.press_fade_ms / 1000.0f;
  const float fall = theme.release_fade_ms / 1000.0f;

  for (int i = 0; i < IV_PART_COUNT; i++) {
    float target = state.part[i] ? 1.0f : 0.0f;
    // Triggers are analog: fade to the pull amount so a half pull reads as a
    // half-lit trigger rather than snapping on at the actuation point.
    if (i == IV_TRIGGER_L) target = state.lt;
    if (i == IV_TRIGGER_R) target = state.rt;
    anim.part[i] = approach(anim.part[i], target, dt, rise, fall);
  }

  for (int i = 0; i < IV_KEY_COUNT; i++) {
    anim.key[i] = approach(anim.key[i], state.key[i] ? 1.0f : 0.0f, dt, rise, fall);
  }

  anim.l3 = approach(anim.l3, state.l3 ? 1.0f : 0.0f, dt, rise, fall);
  anim.r3 = approach(anim.r3, state.r3 ? 1.0f : 0.0f, dt, rise, fall);

  // Sticks get only enough smoothing to take the stair-step off low frame
  // rates. Anything heavier reads as input lag, which is the opposite of what
  // a controller overlay is for.
  const float stick_seconds = 0.035f;
  anim.lx = approach(anim.lx, state.lx, dt, stick_seconds, stick_seconds);
  anim.ly = approach(anim.ly, state.ly, dt, stick_seconds, stick_seconds);
  anim.rx = approach(anim.rx, state.rx, dt, stick_seconds, stick_seconds);
  anim.ry = approach(anim.ry, state.ry, dt, stick_seconds, stick_seconds);

  anim.presence = approach(anim.presence, state.connected ? 1.0f : 0.0f, dt,
                           theme.connect_fade_ms / 1000.0f,
                           theme.disconnect_fade_ms / 1000.0f);
}

bool iv_render_pad(const IvRenderTargets &targets, const IvLayout &layout,
                   const IvAnim &anim, float width, float height, float opacity) {
  if (!targets.base || !targets.overlay || !targets.tint) return false;
  if (!layout.loaded || layout.canvas_w <= 0.0f || layout.canvas_h <= 0.0f) return false;

  const float alpha = opacity * anim.presence;
  if (alpha <= 0.002f) return true;

  // Fit the canvas into the source box, preserving aspect so a non-matching
  // source size letterboxes instead of stretching the pad.
  const float fit = fminf(width / layout.canvas_w, height / layout.canvas_h);
  const float draw_w = layout.canvas_w * fit;
  const float draw_h = layout.canvas_h * fit;

  // Sheets are loaded premultiplied, so source colour is added as-is rather
  // than scaled by alpha a second time.
  gs_blend_state_push();
  gs_blend_function(GS_BLEND_ONE, GS_BLEND_INVSRCALPHA);

  gs_matrix_push();
  gs_matrix_translate3f((width - draw_w) * 0.5f, (height - draw_h) * 0.5f, 0.0f);
  // gs_draw_sprite_subregion sizes its quad in texels, so scaling by
  // fit/asset_scale lets every coordinate below stay in sheet texels.
  const float unit = fit / targets.asset_scale;
  gs_matrix_scale3f(unit, unit, 1.0f);

  // Body first, then only the parts that are actually lit.
  {
    struct vec4 tint;
    vec4_set(&tint, alpha, alpha, alpha, alpha);
    gs_effect_set_vec4(gs_effect_get_param_by_name(targets.tint, "tint"), &tint);
    gs_effect_set_texture(gs_effect_get_param_by_name(targets.tint, "image"),
                          targets.base);
    while (gs_effect_loop(targets.tint, "Draw")) {
      gs_draw_sprite(targets.base, 0, 0, 0);
    }
  }

  for (int i = 0; i < IV_PART_COUNT; i++) {
    draw_part(targets, layout.parts[i], alpha * anim.part[i]);
  }

  draw_stick(targets, layout.left_stick, layout.stick_travel, anim.lx, anim.ly,
             anim.l3, alpha);
  draw_stick(targets, layout.right_stick, layout.stick_travel, anim.rx, anim.ry,
             anim.r3, alpha);

  gs_matrix_pop();
  gs_blend_state_pop();
  return true;
}


void iv_fill_rect(float x, float y, float w, float h, uint32_t colour, float opacity) {
  if (w <= 0.0f || h <= 0.0f || opacity <= 0.002f) return;

  struct vec4 rgba;
  vec4_from_rgba(&rgba, colour);
  rgba.w *= opacity;
  if (rgba.w <= 0.002f) return;

  gs_effect_t *effect = obs_get_base_effect(OBS_EFFECT_SOLID);
  gs_eparam_t *param = gs_effect_get_param_by_name(effect, "color");
  gs_effect_set_vec4(param, &rgba);

  // gs_draw_sprite always draws at the origin, so the position has to come from
  // the matrix. Passing x/y as arguments (as this plugin used to) silently
  // stacked every element on top of each other at 0,0.
  gs_matrix_push();
  gs_matrix_translate3f(x, y, 0.0f);
  while (gs_effect_loop(effect, "Solid")) {
    gs_draw_sprite(nullptr, 0, static_cast<uint32_t>(w), static_cast<uint32_t>(h));
  }
  gs_matrix_pop();
}

namespace {

// Blends between the resting surface colour and the accent, so a key fades in
// step with the pad's buttons instead of switching hard.
uint32_t blend_rgba(uint32_t from, uint32_t to, float t) {
  if (t <= 0.0f) return from;
  if (t >= 1.0f) return to;
  uint32_t out = 0;
  for (int shift = 0; shift < 32; shift += 8) {
    const float a = static_cast<float>((from >> shift) & 0xff);
    const float b = static_cast<float>((to >> shift) & 0xff);
    const uint32_t v = static_cast<uint32_t>(a + (b - a) * t + 0.5f);
    out |= (v & 0xff) << shift;
  }
  return out;
}

}  // namespace

void iv_render_keys(const IvAnim &anim, const IvTheme &theme, float x, float y,
                    float width, float height, float opacity, bool keyboard,
                    bool mouse) {
  if (!keyboard && !mouse) return;
  if (width <= 0.0f || height <= 0.0f) return;

  // Three rows: WASD sits over a modifier row, with the mouse buttons beside
  // them. Sized off the box so the strip tracks the source's scale.
  const float unit = fminf(width / 12.0f, height / 3.4f);
  const float gap = unit * 0.16f;
  const float key = unit;

  auto cell = [&](float cx, float cy, float cw, float ch, int index) {
    const uint32_t colour = blend_rgba(theme.surface, theme.accent, anim.key[index]);
    iv_fill_rect(x + cx, y + cy, cw, ch, colour, opacity);
  };

  if (keyboard) {
    const float row0 = 0.0f;
    const float row1 = key + gap;
    const float row2 = (key + gap) * 2.0f;

    cell(key + gap, row0, key, key, IV_KEY_W);
    cell(0.0f, row1, key, key, IV_KEY_A);
    cell(key + gap, row1, key, key, IV_KEY_S);
    cell((key + gap) * 2.0f, row1, key, key, IV_KEY_D);

    const float wide = key * 1.6f;
    cell(0.0f, row2, wide, key * 0.72f, IV_KEY_SHIFT);
    cell(wide + gap, row2, wide, key * 0.72f, IV_KEY_CTRL);
    cell((wide + gap) * 2.0f, row2, key * 2.4f, key * 0.72f, IV_KEY_SPACE);
  }

  if (mouse) {
    const float mx = keyboard ? (key + gap) * 4.2f : 0.0f;
    const float mw = key * 0.9f;
    cell(mx, 0.0f, mw, key * 1.3f, IV_MOUSE_L);
    cell(mx + mw + gap, 0.0f, mw, key * 1.3f, IV_MOUSE_R);
  }
}
