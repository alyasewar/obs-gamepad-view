#include <obs-module.h>
#include <graphics/image-file.h>
#include <util/platform.h>
#include <math.h>
#include <string>

#include "input_state.h"
#include "theme_loader.h"

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("input-visualizer", "en-US")

static const char *source_id = "input_visualizer";

struct input_visualizer_source {
  obs_source_t *source;
  std::string theme;
  std::string device;
  float opacity;
  float scale;
  float corner_radius;
  IvInputState state;
  IvTheme theme_data;
  std::string base_path;
  bool show_gamepad;
  bool show_keyboard;
  bool show_mouse;
  bool connected_only;
  bool preview_mode;
  uint64_t preview_seed;
};

static const char *input_visualizer_get_name(void *) {
  return "Input Visualizer";
}

static void *input_visualizer_create(obs_data_t *settings, obs_source_t *source) {
  auto *iv = new input_visualizer_source;
  iv->source = source;
  iv->theme = obs_data_get_string(settings, "theme");
  iv->device = obs_data_get_string(settings, "device");
  iv->opacity = static_cast<float>(obs_data_get_double(settings, "opacity"));
  iv->scale = static_cast<float>(obs_data_get_double(settings, "scale"));
  iv->corner_radius = static_cast<float>(obs_data_get_double(settings, "corner_radius"));
  iv->show_gamepad = obs_data_get_bool(settings, "show_gamepad");
  iv->show_keyboard = obs_data_get_bool(settings, "show_keyboard");
  iv->show_mouse = obs_data_get_bool(settings, "show_mouse");
  iv->connected_only = obs_data_get_bool(settings, "connected_only");
  iv->preview_mode = obs_data_get_bool(settings, "preview_mode");
  iv->preview_seed = os_gettime_ns();
  if (iv->opacity <= 0.0f) iv->opacity = 1.0f;
  if (iv->scale <= 0.0f) iv->scale = 1.0f;
  if (iv->corner_radius <= 0.0f) iv->corner_radius = 10.0f;
  const char *module_path = obs_get_module_data_path(obs_current_module());
  if (module_path) iv->base_path = module_path;
  iv->theme_data = iv_theme_load(iv->theme, iv->base_path);
  return iv;
}

static void input_visualizer_destroy(void *data) {
  auto *iv = static_cast<input_visualizer_source *>(data);
  delete iv;
}

static void input_visualizer_update(void *data, obs_data_t *settings) {
  auto *iv = static_cast<input_visualizer_source *>(data);
  iv->theme = obs_data_get_string(settings, "theme");
  iv->device = obs_data_get_string(settings, "device");
  iv->opacity = static_cast<float>(obs_data_get_double(settings, "opacity"));
  iv->scale = static_cast<float>(obs_data_get_double(settings, "scale"));
  iv->corner_radius = static_cast<float>(obs_data_get_double(settings, "corner_radius"));
  iv->show_gamepad = obs_data_get_bool(settings, "show_gamepad");
  iv->show_keyboard = obs_data_get_bool(settings, "show_keyboard");
  iv->show_mouse = obs_data_get_bool(settings, "show_mouse");
  iv->connected_only = obs_data_get_bool(settings, "connected_only");
  iv->preview_mode = obs_data_get_bool(settings, "preview_mode");
  if (iv->opacity <= 0.0f) iv->opacity = 1.0f;
  if (iv->scale <= 0.0f) iv->scale = 1.0f;
  if (iv->corner_radius <= 0.0f) iv->corner_radius = 10.0f;
  if (iv->base_path.empty()) {
    const char *module_path = obs_get_module_data_path(obs_current_module());
    if (module_path) iv->base_path = module_path;
  }
  iv->theme_data = iv_theme_load(iv->theme, iv->base_path);
}

static obs_properties_t *input_visualizer_properties(void *) {
  obs_properties_t *props = obs_properties_create();

  obs_property_t *device_list = obs_properties_add_list(
      props, "device", "Device", OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
  obs_property_list_add_string(device_list, "Auto", "auto");
  obs_property_list_add_string(device_list, "Gamepad", "gamepad");
  obs_property_list_add_string(device_list, "Keyboard", "keyboard");
  obs_property_list_add_string(device_list, "Mouse", "mouse");

  obs_property_t *theme_list = obs_properties_add_list(
      props, "theme", "Theme", OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
  obs_property_list_add_string(theme_list, "Light", "light");
  obs_property_list_add_string(theme_list, "Dark", "dark");
  obs_property_list_add_string(theme_list, "Pastel", "pastel");

  obs_properties_add_bool(props, "show_gamepad", "Show gamepad");
  obs_properties_add_bool(props, "show_keyboard", "Show keyboard");
  obs_properties_add_bool(props, "show_mouse", "Show mouse");
  obs_properties_add_bool(props, "connected_only", "Show only when connected");
  obs_properties_add_bool(props, "preview_mode", "Preview mode");

  obs_properties_add_float_slider(props, "opacity", "Opacity", 0.05, 1.0, 0.05);
  obs_properties_add_float_slider(props, "scale", "Scale", 0.25, 3.0, 0.05);
  obs_properties_add_float_slider(props, "corner_radius", "Corner radius", 0.0, 24.0, 1.0);

  return props;
}

static void iv_draw_rect(float x, float y, float w, float h, uint32_t color, float opacity) {
  struct vec4 v;
  vec4_from_rgba(&v, color);
  v.w *= opacity;
  gs_effect_t *effect = obs_get_base_effect(OBS_EFFECT_SOLID);
  gs_effect_set_vec4(gs_effect_get_param_by_name(effect, "color"), &v);
  while (gs_effect_loop(effect, "Solid")) {
    gs_draw_sprite(nullptr, 0, static_cast<uint32_t>(w), static_cast<uint32_t>(h));
  }
}

static void iv_draw_svg_or_rect(const char *path, float w, float h, uint32_t fallback, float opacity) {
  if (path && path[0]) {
    gs_image_file_t image;
    gs_image_file_init(&image, path);
    gs_image_file_init_texture(&image);
    if (image.loaded && image.texture) {
      gs_effect_t *effect = obs_get_base_effect(OBS_EFFECT_DEFAULT);
      gs_effect_set_texture(gs_effect_get_param_by_name(effect, "image"), image.texture);
      while (gs_effect_loop(effect, "Draw")) {
        gs_draw_sprite(image.texture, 0, (uint32_t)w, (uint32_t)h);
      }
      gs_image_file_free(&image);
      return;
    }
    gs_image_file_free(&image);
  }
  iv_draw_rect(0.0f, 0.0f, w, h, fallback, opacity);
}

static void iv_draw_pad(const IvInputState &state, const IvTheme &theme, float scale, float opacity) {
  float s = scale;
  uint32_t stroke = theme.stroke;
  uint32_t pressed = theme.pressed;

  float lt_fill = 80.0f * state.lt;
  float rt_fill = 80.0f * state.rt;
  iv_draw_rect(40.0f * s, 15.0f * s, 80.0f * s, 8.0f * s,
               stroke, opacity);
  iv_draw_rect(40.0f * s, 15.0f * s, lt_fill * s, 8.0f * s,
               pressed, opacity);
  iv_draw_rect(200.0f * s, 15.0f * s, 80.0f * s, 8.0f * s,
               stroke, opacity);
  iv_draw_rect(200.0f * s, 15.0f * s, rt_fill * s, 8.0f * s,
               pressed, opacity);

  iv_draw_rect(40.0f * s, 30.0f * s, 80.0f * s, 8.0f * s,
               state.lb ? pressed : stroke, opacity);
  iv_draw_rect(200.0f * s, 30.0f * s, 80.0f * s, 8.0f * s,
               state.rb ? pressed : stroke, opacity);

  iv_draw_rect(150.0f * s, 90.0f * s, 10.0f * s, 10.0f * s,
               state.back ? pressed : stroke, opacity);
  iv_draw_rect(165.0f * s, 90.0f * s, 10.0f * s, 10.0f * s,
               state.start ? pressed : stroke, opacity);

  iv_draw_rect(70.0f * s, 110.0f * s, 18.0f * s, 18.0f * s,
               state.dpad_up ? pressed : stroke, opacity);
  iv_draw_rect(70.0f * s, 150.0f * s, 18.0f * s, 18.0f * s,
               state.dpad_down ? pressed : stroke, opacity);
  iv_draw_rect(50.0f * s, 130.0f * s, 18.0f * s, 18.0f * s,
               state.dpad_left ? pressed : stroke, opacity);
  iv_draw_rect(90.0f * s, 130.0f * s, 18.0f * s, 18.0f * s,
               state.dpad_right ? pressed : stroke, opacity);

  iv_draw_rect(240.0f * s, 90.0f * s, 14.0f * s, 14.0f * s,
               state.y ? pressed : stroke, opacity);
  iv_draw_rect(255.0f * s, 105.0f * s, 14.0f * s, 14.0f * s,
               state.b ? pressed : stroke, opacity);
  iv_draw_rect(225.0f * s, 105.0f * s, 14.0f * s, 14.0f * s,
               state.x ? pressed : stroke, opacity);
  iv_draw_rect(240.0f * s, 120.0f * s, 14.0f * s, 14.0f * s,
               state.a ? pressed : stroke, opacity);

  float lx = state.lx * 8.0f;
  float ly = -state.ly * 8.0f;
  float rx = state.rx * 8.0f;
  float ry = -state.ry * 8.0f;
  bool left_active = (state.lx * state.lx + state.ly * state.ly) > 0.04f;
  bool right_active = (state.rx * state.rx + state.ry * state.ry) > 0.04f;
  iv_draw_rect((85.0f + lx) * s, (120.0f + ly) * s, 16.0f * s, 16.0f * s,
               left_active ? pressed : stroke, opacity);
  iv_draw_rect((205.0f + rx) * s, (140.0f + ry) * s, 16.0f * s, 16.0f * s,
               right_active ? pressed : stroke, opacity);
}

static void iv_draw_keyboard(const IvInputState &state, const IvTheme &theme, float scale, float opacity) {
  float s = scale;
  uint32_t surface = theme.surface;
  uint32_t stroke = theme.stroke;
  uint32_t pressed = theme.pressed;

  iv_draw_rect(20.0f * s, 40.0f * s, 36.0f * s, 36.0f * s, state.key_w ? pressed : surface, opacity);
  iv_draw_rect(0.0f * s, 80.0f * s, 36.0f * s, 36.0f * s, state.key_a ? pressed : surface, opacity);
  iv_draw_rect(40.0f * s, 80.0f * s, 36.0f * s, 36.0f * s, state.key_s ? pressed : surface, opacity);
  iv_draw_rect(80.0f * s, 80.0f * s, 36.0f * s, 36.0f * s, state.key_d ? pressed : surface, opacity);

  iv_draw_rect(0.0f * s, 130.0f * s, 70.0f * s, 20.0f * s, state.key_shift ? pressed : stroke, opacity);
  iv_draw_rect(80.0f * s, 130.0f * s, 70.0f * s, 20.0f * s, state.key_ctrl ? pressed : stroke, opacity);
  iv_draw_rect(0.0f * s, 160.0f * s, 150.0f * s, 24.0f * s, state.key_space ? pressed : stroke, opacity);
}

static void iv_draw_mouse(const IvInputState &state, const IvTheme &theme, float scale, float opacity) {
  float s = scale;
  uint32_t surface = theme.surface;
  uint32_t stroke = theme.stroke;
  uint32_t pressed = theme.pressed;

  iv_draw_rect(190.0f * s, 120.0f * s, 50.0f * s, 60.0f * s, surface, opacity);
  iv_draw_rect(190.0f * s, 120.0f * s, 25.0f * s, 25.0f * s,
               state.mouse_left ? pressed : stroke, opacity);
  iv_draw_rect(215.0f * s, 120.0f * s, 25.0f * s, 25.0f * s,
               state.mouse_right ? pressed : stroke, opacity);
}

static void input_visualizer_tick(void *data, float) {
  auto *iv = static_cast<input_visualizer_source *>(data);
  iv_input_poll();
  iv->state = iv_input_get();
  if (iv->preview_mode) {
    uint64_t now = os_gettime_ns();
    double t = (double)(now - iv->preview_seed) / 1e9;
    iv->state.connected = true;
    iv->state.a = fmod(t, 2.0) < 1.0;
    iv->state.b = fmod(t + 0.4, 2.0) < 1.0;
    iv->state.x = fmod(t + 0.8, 2.0) < 1.0;
    iv->state.y = fmod(t + 1.2, 2.0) < 1.0;
    iv->state.lb = fmod(t, 3.0) < 1.2;
    iv->state.rb = fmod(t + 1.0, 3.0) < 1.2;
    iv->state.lt = (float)(0.5 + 0.5 * sin(t * 2.0));
    iv->state.rt = (float)(0.5 + 0.5 * cos(t * 2.0));
    iv->state.dpad_up = fmod(t, 1.6) < 0.4;
    iv->state.dpad_right = fmod(t + 0.4, 1.6) < 0.4;
    iv->state.dpad_down = fmod(t + 0.8, 1.6) < 0.4;
    iv->state.dpad_left = fmod(t + 1.2, 1.6) < 0.4;
    iv->state.lx = (float)sin(t * 1.7);
    iv->state.ly = (float)cos(t * 1.9);
    iv->state.rx = (float)sin(t * 1.3);
    iv->state.ry = (float)cos(t * 1.5);
    iv->state.key_w = fmod(t, 1.2) < 0.3;
    iv->state.key_a = fmod(t + 0.3, 1.2) < 0.3;
    iv->state.key_s = fmod(t + 0.6, 1.2) < 0.3;
    iv->state.key_d = fmod(t + 0.9, 1.2) < 0.3;
    iv->state.key_space = fmod(t, 2.2) < 0.5;
    iv->state.key_shift = fmod(t + 0.8, 2.2) < 0.5;
    iv->state.key_ctrl = fmod(t + 1.6, 2.2) < 0.5;
    iv->state.mouse_left = fmod(t, 1.0) < 0.2;
    iv->state.mouse_right = fmod(t + 0.5, 1.0) < 0.2;
  }
}

static void input_visualizer_render(void *data, gs_effect_t *) {
  auto *iv = static_cast<input_visualizer_source *>(data);
  float s = iv->scale;
  float width = 320.0f * s;
  float height = 200.0f * s;

  uint32_t bg = iv->theme_data.background;
  uint32_t surface = iv->theme_data.surface;
  uint32_t stroke = iv->theme_data.stroke;
  uint32_t pressed = iv->theme_data.pressed;

  iv_draw_rect(0, 0, width, height, bg, iv->opacity);
  iv_draw_rect(10.0f * s, 10.0f * s, width - 20.0f * s, height - 20.0f * s,
               surface, iv->opacity);

  gs_matrix_push();
  gs_matrix_translate3f(10.0f * s, 10.0f * s, 0.0f);
  iv_draw_svg_or_rect(iv->theme_data.layout_path.c_str(), width - 20.0f * s,
                      height - 20.0f * s, stroke, iv->opacity);
  gs_matrix_pop();

  bool allow = !iv->connected_only || iv->state.connected;
  if (allow) {
    bool show_gamepad = iv->show_gamepad || iv->device == "gamepad" || iv->device == "auto";
    bool show_keyboard = iv->show_keyboard || iv->device == "keyboard" || iv->device == "auto";
    bool show_mouse = iv->show_mouse || iv->device == "mouse" || iv->device == "auto";
    if (show_gamepad) iv_draw_pad(iv->state, iv->theme_data, s, iv->opacity);
    if (show_keyboard) iv_draw_keyboard(iv->state, iv->theme_data, s, iv->opacity);
    if (show_mouse) iv_draw_mouse(iv->state, iv->theme_data, s, iv->opacity);
  }
}

static uint32_t input_visualizer_get_width(void *data) {
  auto *iv = static_cast<input_visualizer_source *>(data);
  return static_cast<uint32_t>(320.0f * iv->scale);
}

static uint32_t input_visualizer_get_height(void *data) {
  auto *iv = static_cast<input_visualizer_source *>(data);
  return static_cast<uint32_t>(200.0f * iv->scale);
}

static void input_visualizer_defaults(obs_data_t *settings) {
  obs_data_set_default_string(settings, "theme", "light");
  obs_data_set_default_string(settings, "device", "auto");
  obs_data_set_default_double(settings, "opacity", 1.0);
  obs_data_set_default_double(settings, "scale", 1.0);
  obs_data_set_default_double(settings, "corner_radius", 10.0);
  obs_data_set_default_bool(settings, "show_gamepad", true);
  obs_data_set_default_bool(settings, "show_keyboard", false);
  obs_data_set_default_bool(settings, "show_mouse", false);
  obs_data_set_default_bool(settings, "connected_only", false);
  obs_data_set_default_bool(settings, "preview_mode", false);
}

bool obs_module_load(void) {
  obs_source_info si = {};
  si.id = source_id;
  si.type = OBS_SOURCE_TYPE_INPUT;
  si.output_flags = OBS_SOURCE_VIDEO;
  si.get_name = input_visualizer_get_name;
  si.create = input_visualizer_create;
  si.destroy = input_visualizer_destroy;
  si.update = input_visualizer_update;
  si.get_properties = input_visualizer_properties;
  si.get_width = input_visualizer_get_width;
  si.get_height = input_visualizer_get_height;
  si.video_tick = input_visualizer_tick;
  si.video_render = input_visualizer_render;
  si.get_defaults = input_visualizer_defaults;
  obs_register_source(&si);
  return true;
}
