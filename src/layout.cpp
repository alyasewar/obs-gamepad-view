#include "layout.h"

#include <fstream>
#include <sstream>

#include "json.h"

static const char *const kPartKeys[IV_PART_COUNT] = {
    "face_down", "face_right", "face_left",  "face_up",
    "dpad_up",   "dpad_down",  "dpad_left",  "dpad_right",
    "bumper_l",  "bumper_r",   "trigger_l",  "trigger_r",
    "back",      "start",      "guide",      "touchpad",
};

const char *iv_part_key(int part) {
  if (part < 0 || part >= IV_PART_COUNT) return "";
  return kPartKeys[part];
}

static const char *const kKeyLabels[IV_KEY_COUNT] = {
    "W", "A", "S", "D", "Space", "Shift", "Ctrl", "M1", "M2",
};

const char *iv_key_label(int key) {
  if (key < 0 || key >= IV_KEY_COUNT) return "";
  return kKeyLabels[key];
}

static std::string iv_read_file(const std::string &path) {
  std::ifstream file(path, std::ios::binary);
  if (!file.is_open()) return "";
  std::ostringstream buffer;
  buffer << file.rdbuf();
  return buffer.str();
}

// A rect is [x, y, w, h]; anything shorter is treated as absent.
static bool iv_read_rect(const ivjson::Value &v, float &x, float &y, float &w, float &h) {
  if (v.size() < 4) return false;
  x = static_cast<float>(v[static_cast<size_t>(0)].num());
  y = static_cast<float>(v[static_cast<size_t>(1)].num());
  w = static_cast<float>(v[static_cast<size_t>(2)].num());
  h = static_cast<float>(v[static_cast<size_t>(3)].num());
  return w > 0.0f && h > 0.0f;
}

static IvSprite iv_read_sprite(const ivjson::Value &part) {
  IvSprite sprite;
  if (!iv_read_rect(part["src"], sprite.sx, sprite.sy, sprite.sw, sprite.sh)) {
    return sprite;
  }
  // dst defaults to src's corner: in-place sprites sit directly over the art
  // they replace.
  const ivjson::Value &dst = part["dst"];
  if (dst.size() >= 2) {
    sprite.dx = static_cast<float>(dst[static_cast<size_t>(0)].num());
    sprite.dy = static_cast<float>(dst[static_cast<size_t>(1)].num());
  } else {
    sprite.dx = sprite.sx;
    sprite.dy = sprite.sy;
  }
  sprite.present = true;
  return sprite;
}

static IvStickLayout iv_read_stick(const ivjson::Value &stick) {
  IvStickLayout out;
  const ivjson::Value &centre = stick["centre"];
  if (centre.size() < 2) return out;
  out.cx = static_cast<float>(centre[static_cast<size_t>(0)].num());
  out.cy = static_cast<float>(centre[static_cast<size_t>(1)].num());

  ivjson::Value neutral_holder;
  neutral_holder.type = ivjson::Value::Type::Object;
  neutral_holder.object["src"] = stick["neutral"];
  out.neutral = iv_read_sprite(neutral_holder);

  ivjson::Value pressed_holder;
  pressed_holder.type = ivjson::Value::Type::Object;
  pressed_holder.object["src"] = stick["pressed"];
  out.pressed = iv_read_sprite(pressed_holder);

  out.present = out.neutral.present;
  return out;
}

IvLayout iv_layout_load(const std::string &pad_id, const std::string &theme_id,
                        const std::string &assets_root) {
  IvLayout layout;
  layout.id = pad_id;
  layout.name = pad_id;
  if (assets_root.empty() || pad_id.empty()) return layout;

  const std::string manifest = assets_root + "/controllers/" + pad_id + "/layout.json";
  ivjson::Value root;
  if (!ivjson::parse(iv_read_file(manifest), root)) return layout;

  layout.name = root["name"].str(pad_id);
  layout.canvas_w = static_cast<float>(root["canvas"]["w"].num(layout.canvas_w));
  layout.canvas_h = static_cast<float>(root["canvas"]["h"].num(layout.canvas_h));
  layout.sheet_w = static_cast<float>(root["sheet"]["w"].num(layout.sheet_w));
  layout.sheet_h = static_cast<float>(root["sheet"]["h"].num(layout.sheet_h));
  layout.stick_travel = static_cast<float>(root["stickTravel"].num(layout.stick_travel));

  const ivjson::Value &parts = root["parts"];
  for (int i = 0; i < IV_PART_COUNT; i++) {
    layout.parts[i] = iv_read_sprite(parts[kPartKeys[i]]);
  }

  const ivjson::Value &sticks = root["sticks"];
  layout.left_stick = iv_read_stick(sticks["left"]);
  layout.right_stick = iv_read_stick(sticks["right"]);

  // Sheets are rasterised per theme so palette swaps need no runtime tinting.
  const std::string sheet_dir = assets_root + "/themes/" + theme_id + "/" + pad_id + "/";
  layout.base_png = sheet_dir + "base.png";
  layout.overlay_png = sheet_dir + "overlay.png";

  layout.loaded = layout.canvas_w > 0.0f && layout.canvas_h > 0.0f;
  return layout;
}
