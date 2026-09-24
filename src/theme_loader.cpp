#include "theme_loader.h"

#include <cstdlib>
#include <fstream>
#include <sstream>

#include "json.h"

static std::string iv_read_file(const std::string &path) {
  std::ifstream file(path, std::ios::binary);
  if (!file.is_open()) return "";
  std::ostringstream buffer;
  buffer << file.rdbuf();
  return buffer.str();
}

// Accepts #rgb, #rrggbb and #rrggbbaa, returning OBS's 0xAABBGGRR packing.
// Unparseable input yields `fallback` so a typo in a theme cannot black out the
// source.
static uint32_t iv_parse_hex(const std::string &hex, uint32_t fallback) {
  if (hex.size() < 4 || hex[0] != '#') return fallback;

  std::string digits = hex.substr(1);
  if (digits.size() == 3) {
    std::string expanded;
    for (char c : digits) {
      expanded.push_back(c);
      expanded.push_back(c);
    }
    digits = expanded;
  }
  if (digits.size() != 6 && digits.size() != 8) return fallback;

  for (char c : digits) {
    const bool is_hex = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') ||
                        (c >= 'A' && c <= 'F');
    if (!is_hex) return fallback;
  }

  const uint32_t value = static_cast<uint32_t>(strtoul(digits.c_str(), nullptr, 16));
  uint8_t r, g, b, a;
  if (digits.size() == 6) {
    r = static_cast<uint8_t>((value >> 16) & 0xff);
    g = static_cast<uint8_t>((value >> 8) & 0xff);
    b = static_cast<uint8_t>(value & 0xff);
    a = 0xff;
  } else {
    r = static_cast<uint8_t>((value >> 24) & 0xff);
    g = static_cast<uint8_t>((value >> 16) & 0xff);
    b = static_cast<uint8_t>((value >> 8) & 0xff);
    a = static_cast<uint8_t>(value & 0xff);
  }
  return (static_cast<uint32_t>(a) << 24) | (static_cast<uint32_t>(b) << 16) |
         (static_cast<uint32_t>(g) << 8) | static_cast<uint32_t>(r);
}

static float iv_clamp_ms(double value, float fallback) {
  if (value <= 0.0 || value > 5000.0) return fallback;
  return static_cast<float>(value);
}

IvTheme iv_theme_load(const std::string &theme_id, const std::string &assets_root) {
  IvTheme theme;
  theme.id = theme_id;
  theme.name = theme_id;
  if (assets_root.empty() || theme_id.empty()) return theme;

  const std::string path = assets_root + "/themes/" + theme_id + "/theme.json";
  ivjson::Value root;
  if (!ivjson::parse(iv_read_file(path), root)) return theme;

  theme.name = root["name"].str(theme_id);
  const ivjson::Value &palette = root["palette"];
  theme.background = iv_parse_hex(palette["background"].str(), theme.background);
  theme.surface = iv_parse_hex(palette["shell"].str(), theme.surface);
  theme.accent = iv_parse_hex(palette["accent"].str(), theme.accent);
  theme.ink = iv_parse_hex(palette["ink"].str(), theme.ink);

  const ivjson::Value &animation = root["animation"];
  theme.press_fade_ms = iv_clamp_ms(animation["pressFadeMs"].num(), theme.press_fade_ms);
  theme.release_fade_ms = iv_clamp_ms(animation["releaseFadeMs"].num(), theme.release_fade_ms);
  theme.connect_fade_ms = iv_clamp_ms(animation["connectFadeMs"].num(), theme.connect_fade_ms);
  theme.disconnect_fade_ms = iv_clamp_ms(animation["disconnectFadeMs"].num(), theme.disconnect_fade_ms);

  theme.loaded = true;
  return theme;
}
