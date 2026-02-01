#include "theme_loader.h"

#include <fstream>
#include <sstream>

static uint32_t iv_parse_hex(const std::string &hex) {
  if (hex.size() < 7) return 0xffffffff;
  uint32_t value = 0xffffffff;
  std::stringstream ss;
  ss << std::hex << hex.substr(1);
  ss >> value;
  if (hex.size() <= 7) {
    value |= 0xff000000;
  }
  return value;
}

static std::string iv_read_file(const std::string &path) {
  std::ifstream file(path);
  if (!file.is_open()) return "";
  std::stringstream buffer;
  buffer << file.rdbuf();
  return buffer.str();
}

static std::string iv_json_get_string(const std::string &json, const std::string &key) {
  std::string needle = "\"" + key + "\"";
  size_t pos = json.find(needle);
  if (pos == std::string::npos) return "";
  pos = json.find(':', pos);
  if (pos == std::string::npos) return "";
  pos = json.find('"', pos);
  if (pos == std::string::npos) return "";
  size_t end = json.find('"', pos + 1);
  if (end == std::string::npos) return "";
  return json.substr(pos + 1, end - pos - 1);
}

IvTheme iv_theme_load(const std::string &theme_id, const std::string &base_path) {
  IvTheme theme;
  theme.id = theme_id;
  theme.name = theme_id;

  if (base_path.empty()) return theme;

  std::string base = base_path + "/themes/" + theme_id + "/theme.json";
  std::string json = iv_read_file(base);
  if (json.empty()) return theme;

  theme.name = iv_json_get_string(json, "name");
  theme.layout_path = iv_json_get_string(json, "layout");
  theme.buttons_path = iv_json_get_string(json, "buttons");
  theme.dpad_path = iv_json_get_string(json, "dpad");
  theme.sticks_path = iv_json_get_string(json, "sticks");
  theme.bumper_path = iv_json_get_string(json, "bumper");
  theme.trigger_path = iv_json_get_string(json, "trigger");
  theme.start_select_path = iv_json_get_string(json, "start_select");
  theme.quadrant_path = iv_json_get_string(json, "quadrant");
  theme.disconnected_path = iv_json_get_string(json, "disconnected");

  std::string background = iv_json_get_string(json, "background");
  std::string surface = iv_json_get_string(json, "surface");
  std::string stroke = iv_json_get_string(json, "stroke");
  std::string accent = iv_json_get_string(json, "accent");
  std::string text = iv_json_get_string(json, "text");
  std::string pressed = iv_json_get_string(json, "pressed");

  if (!background.empty()) theme.background = iv_parse_hex(background);
  if (!surface.empty()) theme.surface = iv_parse_hex(surface);
  if (!stroke.empty()) theme.stroke = iv_parse_hex(stroke);
  if (!accent.empty()) theme.accent = iv_parse_hex(accent);
  if (!text.empty()) theme.text = iv_parse_hex(text);
  if (!pressed.empty()) theme.pressed = iv_parse_hex(pressed);

  std::string theme_root = base_path + "/themes/" + theme_id + "/";
  if (!theme.layout_path.empty()) theme.layout_path = theme_root + theme.layout_path;
  if (!theme.buttons_path.empty()) theme.buttons_path = theme_root + theme.buttons_path;
  if (!theme.dpad_path.empty()) theme.dpad_path = theme_root + theme.dpad_path;
  if (!theme.sticks_path.empty()) theme.sticks_path = theme_root + theme.sticks_path;
  if (!theme.bumper_path.empty()) theme.bumper_path = theme_root + theme.bumper_path;
  if (!theme.trigger_path.empty()) theme.trigger_path = theme_root + theme.trigger_path;
  if (!theme.start_select_path.empty()) theme.start_select_path = theme_root + theme.start_select_path;
  if (!theme.quadrant_path.empty()) theme.quadrant_path = theme_root + theme.quadrant_path;
  if (!theme.disconnected_path.empty()) theme.disconnected_path = theme_root + theme.disconnected_path;

  auto replace_ext = [](std::string &path) {
    if (path.size() > 5 && path.substr(path.size() - 5) == ".svgz") {
      path.replace(path.size() - 5, 5, ".png");
    }
  };
  replace_ext(theme.layout_path);
  replace_ext(theme.buttons_path);
  replace_ext(theme.dpad_path);
  replace_ext(theme.sticks_path);
  replace_ext(theme.bumper_path);
  replace_ext(theme.trigger_path);
  replace_ext(theme.start_select_path);
  replace_ext(theme.quadrant_path);
  replace_ext(theme.disconnected_path);

  return theme;
}
