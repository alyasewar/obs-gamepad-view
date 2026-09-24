#pragma once

#include <stdint.h>

#include <string>

// Runtime slice of a theme. The palette itself is baked into the per-theme PNG
// sheets at build time, so the only colour the renderer still needs is the
// optional backdrop; the rest of this is timing.
struct IvTheme {
  bool loaded = false;
  std::string id;
  std::string name;

  // Drawn behind the pad. Fully transparent by default so the overlay drops
  // onto a scene without a card behind it.
  uint32_t background = 0x00000000;
  // Used only by the optional keyboard/mouse strip, which is drawn from solid
  // rects rather than sheet art. Packed 0xAABBGGRR, as OBS expects.
  uint32_t surface = 0xff2b303b;
  uint32_t accent = 0xff4e8cff;
  uint32_t ink = 0xffcdd3e0;

  // Milliseconds for a part to reach full press / return to rest.
  float press_fade_ms = 60.0f;
  float release_fade_ms = 140.0f;
  // Milliseconds for the whole pad to fade in/out as a controller comes and
  // goes, so hot-plugging does not pop.
  float connect_fade_ms = 220.0f;
  float disconnect_fade_ms = 320.0f;
};

IvTheme iv_theme_load(const std::string &theme_id, const std::string &assets_root);
