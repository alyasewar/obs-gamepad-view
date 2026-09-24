#include <graphics/image-file.h>
#include <obs-module.h>
#include <util/platform.h>

#include <math.h>

#include <string>

#include "input_state.h"
#include "layout.h"
#include "renderer.h"
#include "theme_loader.h"

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("input-visualizer", "en-US")

static const char *kSourceId = "input_visualizer";
static const char *kDefaultPad = "xbox";

struct input_visualizer_source {
  obs_source_t *source = nullptr;

  // Settings.
  std::string theme_id;
  std::string pad_setting;  // "auto", "xbox", "ps5"
  float opacity = 1.0f;
  float scale = 0.5f;
  bool show_backdrop = false;
  bool desktop_enabled = false;
  bool show_keyboard = false;
  bool show_mouse = false;
  bool hide_when_disconnected = false;
  bool preview_mode = false;

  std::string assets_root;
  IvTheme theme;
  IvLayout layout;
  IvInputState state;
  IvAnim anim;

  // Which (theme, pad) pair the loaded textures belong to. Reloading is driven
  // off this rather than off the settings, because "auto" resolves to a
  // different pad as controllers come and go.
  std::string loaded_key;
  // gs_image_file4_t is the variant that takes an alpha mode and a colour
  // space; the plain gs_image_file_t entry point is straight-alpha only.
  gs_image_file4_t base_image = {};
  gs_image_file4_t overlay_image = {};
  bool images_loaded = false;
  gs_effect_t *tint_effect = nullptr;
  bool tint_effect_tried = false;

  uint64_t preview_seed = 0;
  bool desktop_capture_requested = false;
};

// --------------------------------------------------------------------------
// Asset lifecycle
// --------------------------------------------------------------------------

// Must run on the graphics thread.
static void iv_free_images(input_visualizer_source *iv) {
  if (!iv->images_loaded) return;
  gs_image_file4_free(&iv->base_image);
  gs_image_file4_free(&iv->overlay_image);
  iv->base_image = {};
  iv->overlay_image = {};
  iv->images_loaded = false;
}

// Loads the sheets for the active theme/pad once, instead of per frame. The
// previous renderer re-read and re-uploaded its PNG on every single frame,
// which is what made the overlay stutter.
static void iv_ensure_images(input_visualizer_source *iv, const std::string &key) {
  if (iv->images_loaded && iv->loaded_key == key) return;

  iv_free_images(iv);
  iv->loaded_key = key;
  if (!iv->layout.loaded) return;

  // Premultiplied alpha: the sheets are full of antialiased curves and get
  // scaled by the source size, and straight alpha fringes badly under linear
  // filtering. The renderer's blend mode and tint are matched to this.
  gs_image_file4_init(&iv->base_image, iv->layout.base_png.c_str(),
                      GS_IMAGE_ALPHA_PREMULTIPLY_SRGB);
  gs_image_file4_init(&iv->overlay_image, iv->layout.overlay_png.c_str(),
                      GS_IMAGE_ALPHA_PREMULTIPLY_SRGB);
  gs_image_file4_init_texture(&iv->base_image);
  gs_image_file4_init_texture(&iv->overlay_image);

  if (!iv->base_image.image3.image2.image.loaded ||
      !iv->overlay_image.image3.image2.image.loaded) {
    blog(LOG_WARNING,
         "[input-visualizer] could not load sheets for theme '%s' pad '%s' "
         "(%s). Run the asset build step.",
         iv->theme.id.c_str(), iv->layout.id.c_str(), iv->layout.base_png.c_str());
    iv_free_images(iv);
    return;
  }

  iv->images_loaded = true;
}

static void iv_ensure_effect(input_visualizer_source *iv) {
  if (iv->tint_effect || iv->tint_effect_tried) return;
  iv->tint_effect_tried = true;

  char *path = obs_module_file("effects/tint.effect");
  if (!path) {
    blog(LOG_WARNING, "[input-visualizer] effects/tint.effect is missing");
    return;
  }
  char *error = nullptr;
  iv->tint_effect = gs_effect_create_from_file(path, &error);
  if (!iv->tint_effect) {
    blog(LOG_WARNING, "[input-visualizer] failed to compile tint.effect: %s",
         error ? error : "unknown error");
  }
  bfree(error);
  bfree(path);
}

// --------------------------------------------------------------------------
// Settings
// --------------------------------------------------------------------------

// Resolves the "auto" pad setting against whatever is plugged in. Falls back to
// the Xbox layout so the source still shows something for an unrecognised pad.
static std::string iv_resolve_pad(const input_visualizer_source *iv) {
  if (iv->pad_setting != "auto") return iv->pad_setting;
  switch (iv->state.kind) {
    case IV_PAD_PS5: return "ps5";
    case IV_PAD_XBOX: return "xbox";
    case IV_PAD_UNKNOWN: break;
  }
  return kDefaultPad;
}

static void iv_reload_theme(input_visualizer_source *iv) {
  iv->theme = iv_theme_load(iv->theme_id, iv->assets_root);
  iv->layout = iv_layout_load(iv_resolve_pad(iv), iv->theme_id, iv->assets_root);
}

static void iv_read_settings(input_visualizer_source *iv, obs_data_t *settings) {
  iv->theme_id = obs_data_get_string(settings, "theme");
  iv->pad_setting = obs_data_get_string(settings, "pad");
  iv->opacity = static_cast<float>(obs_data_get_double(settings, "opacity"));
  iv->scale = static_cast<float>(obs_data_get_double(settings, "scale"));
  iv->show_backdrop = obs_data_get_bool(settings, "show_backdrop");
  // The checkable group gates both toggles, so unchecking it hides the strip
  // without clearing the user's individual choices.
  iv->desktop_enabled = obs_data_get_bool(settings, "desktop_enabled");
  iv->show_keyboard = iv->desktop_enabled && obs_data_get_bool(settings, "show_keyboard");
  iv->show_mouse = iv->desktop_enabled && obs_data_get_bool(settings, "show_mouse");
  iv->hide_when_disconnected = obs_data_get_bool(settings, "hide_when_disconnected");
  iv->preview_mode = obs_data_get_bool(settings, "preview_mode");

  if (iv->theme_id.empty()) iv->theme_id = "dark";
  if (iv->pad_setting.empty()) iv->pad_setting = "auto";
  if (iv->opacity <= 0.0f || iv->opacity > 1.0f) iv->opacity = 1.0f;
  if (iv->scale <= 0.0f) iv->scale = 0.5f;

  // The event tap needs accessibility permission and watches all input, so it
  // only starts once the user actually asks for a keyboard or mouse overlay.
  const bool wants_desktop = iv->show_keyboard || iv->show_mouse;
  if (wants_desktop && !iv->desktop_capture_requested) {
    iv_input_start_desktop_capture();
    iv->desktop_capture_requested = true;
  }
}

static const char *input_visualizer_get_name(void *) {
  return obs_module_text("SourceName");
}

static void *input_visualizer_create(obs_data_t *settings, obs_source_t *source) {
  auto *iv = new input_visualizer_source;
  iv->source = source;
  iv->preview_seed = os_gettime_ns();

  const char *module_path = obs_get_module_data_path(obs_current_module());
  if (module_path) iv->assets_root = module_path;

  iv_read_settings(iv, settings);
  iv_reload_theme(iv);
  iv_anim_reset(iv->anim);
  return iv;
}

static void input_visualizer_destroy(void *data) {
  auto *iv = static_cast<input_visualizer_source *>(data);

  // Texture and effect teardown has to hold the graphics context.
  obs_enter_graphics();
  iv_free_images(iv);
  if (iv->tint_effect) {
    gs_effect_destroy(iv->tint_effect);
    iv->tint_effect = nullptr;
  }
  obs_leave_graphics();

  delete iv;
}

static void input_visualizer_update(void *data, obs_data_t *settings) {
  auto *iv = static_cast<input_visualizer_source *>(data);
  if (iv->assets_root.empty()) {
    const char *module_path = obs_get_module_data_path(obs_current_module());
    if (module_path) iv->assets_root = module_path;
  }
  iv_read_settings(iv, settings);
  iv_reload_theme(iv);
  // Sheets are reloaded lazily on the next render, where the graphics context
  // is already held.
}

// Describes what is actually plugged in, so "Auto-detect" is not a black box:
// the user can see which layout it resolved to before wondering why the wrong
// pad is on screen.
static const char *iv_detected_text(const input_visualizer_source *iv) {
  if (iv && iv->preview_mode) return obs_module_text("Detected.Preview");
  if (!iv || !iv->state.connected) return obs_module_text("Detected.None");
  switch (iv->state.kind) {
    case IV_PAD_XBOX: return obs_module_text("Detected.Xbox");
    case IV_PAD_PS5: return obs_module_text("Detected.PS5");
    case IV_PAD_UNKNOWN: break;
  }
  return obs_module_text("Detected.Unknown");
}

static obs_properties_t *input_visualizer_properties(void *data) {
  auto *iv = static_cast<input_visualizer_source *>(data);
  obs_properties_t *props = obs_properties_create();

  // --- Controller -----------------------------------------------------
  obs_properties_t *controller = obs_properties_create();

  obs_property_t *pad = obs_properties_add_list(controller, "pad",
                                                obs_module_text("Prop.Pad"),
                                                OBS_COMBO_TYPE_LIST,
                                                OBS_COMBO_FORMAT_STRING);
  obs_property_list_add_string(pad, obs_module_text("Prop.Pad.Auto"), "auto");
  obs_property_list_add_string(pad, obs_module_text("Prop.Pad.Xbox"), "xbox");
  obs_property_list_add_string(pad, obs_module_text("Prop.Pad.PS5"), "ps5");
  obs_property_set_long_description(pad, obs_module_text("Prop.Pad.Description"));

  // OBS_TEXT_INFO renders its description as the row's text, and properties are
  // rebuilt each time the dialog opens, so this reads current at display time.
  std::string detected_label = std::string(obs_module_text("Prop.Detected")) +
                               ": " + iv_detected_text(iv);
  obs_properties_add_text(controller, "detected", detected_label.c_str(),
                          OBS_TEXT_INFO);

  obs_properties_add_group(props, "group_controller",
                           obs_module_text("Group.Controller"),
                           OBS_GROUP_NORMAL, controller);

  // --- Appearance -----------------------------------------------------
  obs_properties_t *appearance = obs_properties_create();

  obs_property_t *theme = obs_properties_add_list(appearance, "theme",
                                                  obs_module_text("Prop.Theme"),
                                                  OBS_COMBO_TYPE_LIST,
                                                  OBS_COMBO_FORMAT_STRING);
  obs_property_list_add_string(theme, obs_module_text("Prop.Theme.Dark"), "dark");
  obs_property_list_add_string(theme, obs_module_text("Prop.Theme.Light"), "light");
  obs_property_list_add_string(theme, obs_module_text("Prop.Theme.Pastel"), "pastel");

  obs_property_t *size = obs_properties_add_float_slider(
      appearance, "scale", obs_module_text("Prop.Size"), 0.15, 2.0, 0.05);
  obs_property_float_set_suffix(size, " x");

  obs_property_t *opacity = obs_properties_add_float_slider(
      appearance, "opacity", obs_module_text("Prop.Opacity"), 0.05, 1.0, 0.05);
  obs_property_float_set_suffix(opacity, " x");

  obs_property_t *backdrop = obs_properties_add_bool(
      appearance, "show_backdrop", obs_module_text("Prop.Backdrop"));
  obs_property_set_long_description(backdrop,
                                    obs_module_text("Prop.Backdrop.Description"));

  obs_properties_add_group(props, "group_appearance",
                           obs_module_text("Group.Appearance"),
                           OBS_GROUP_NORMAL, appearance);

  // --- Behaviour ------------------------------------------------------
  obs_properties_t *behaviour = obs_properties_create();

  obs_property_t *hide = obs_properties_add_bool(
      behaviour, "hide_when_disconnected",
      obs_module_text("Prop.HideWhenDisconnected"));
  obs_property_set_long_description(
      hide, obs_module_text("Prop.HideWhenDisconnected.Description"));

  obs_property_t *preview = obs_properties_add_bool(
      behaviour, "preview_mode", obs_module_text("Prop.Preview"));
  obs_property_set_long_description(preview,
                                    obs_module_text("Prop.Preview.Description"));

  obs_properties_add_group(props, "group_behaviour",
                           obs_module_text("Group.Behaviour"),
                           OBS_GROUP_NORMAL, behaviour);

  // --- Keyboard & mouse -----------------------------------------------
  // A checkable group: the whole section is off until the user opts in, which
  // is also what gates the event tap and its permission prompt.
  obs_properties_t *desktop = obs_properties_create();
  obs_properties_add_bool(desktop, "show_keyboard",
                          obs_module_text("Prop.ShowKeyboard"));
  obs_properties_add_bool(desktop, "show_mouse", obs_module_text("Prop.ShowMouse"));
  obs_properties_add_text(desktop, "desktop_note",
                          obs_module_text("Prop.Desktop.Permission"),
                          OBS_TEXT_INFO);

  obs_properties_add_group(props, "desktop_enabled",
                           obs_module_text("Prop.DesktopEnabled"),
                           OBS_GROUP_CHECKABLE, desktop);

  return props;
}

// --------------------------------------------------------------------------
// Tick / render
// --------------------------------------------------------------------------

static void iv_apply_preview(input_visualizer_source *iv) {
  const double t = (double)(os_gettime_ns() - iv->preview_seed) / 1e9;
  IvInputState &s = iv->state;

  s.connected = true;
  s.part[IV_FACE_DOWN] = fmod(t, 2.0) < 1.0;
  s.part[IV_FACE_RIGHT] = fmod(t + 0.4, 2.0) < 1.0;
  s.part[IV_FACE_LEFT] = fmod(t + 0.8, 2.0) < 1.0;
  s.part[IV_FACE_UP] = fmod(t + 1.2, 2.0) < 1.0;
  s.part[IV_BUMPER_L] = fmod(t, 3.0) < 1.2;
  s.part[IV_BUMPER_R] = fmod(t + 1.0, 3.0) < 1.2;
  s.part[IV_DPAD_UP] = fmod(t, 1.6) < 0.4;
  s.part[IV_DPAD_RIGHT] = fmod(t + 0.4, 1.6) < 0.4;
  s.part[IV_DPAD_DOWN] = fmod(t + 0.8, 1.6) < 0.4;
  s.part[IV_DPAD_LEFT] = fmod(t + 1.2, 1.6) < 0.4;
  s.part[IV_START] = fmod(t, 4.0) < 0.5;
  s.part[IV_BACK] = fmod(t + 2.0, 4.0) < 0.5;
  s.part[IV_TOUCHPAD] = fmod(t + 1.0, 5.0) < 0.6;

  s.lt = (float)(0.5 + 0.5 * sin(t * 2.0));
  s.rt = (float)(0.5 + 0.5 * cos(t * 2.0));
  s.part[IV_TRIGGER_L] = s.lt > 0.12f;
  s.part[IV_TRIGGER_R] = s.rt > 0.12f;

  s.lx = (float)sin(t * 1.7);
  s.ly = (float)cos(t * 1.9);
  s.rx = (float)sin(t * 1.3);
  s.ry = (float)cos(t * 1.5);
  s.l3 = fmod(t, 2.4) < 0.4;
  s.r3 = fmod(t + 1.2, 2.4) < 0.4;

  for (int i = 0; i < IV_KEY_COUNT; i++) {
    s.key[i] = fmod(t + i * 0.3, 1.8) < 0.4;
  }
}

static void input_visualizer_tick(void *data, float seconds) {
  auto *iv = static_cast<input_visualizer_source *>(data);

  iv_input_poll();
  const IvPadKind previous_kind = iv->state.kind;
  iv->state = iv_input_get();

  if (iv->preview_mode) iv_apply_preview(iv);

  // In auto mode a newly attached pad can change the layout, so pick up the
  // new manifest as soon as the detected kind changes.
  if (iv->pad_setting == "auto" && iv->state.kind != previous_kind) {
    iv->layout = iv_layout_load(iv_resolve_pad(iv), iv->theme_id, iv->assets_root);
  }

  if (iv->hide_when_disconnected && !iv->state.connected) {
    // Let presence fall to zero so the pad fades out rather than vanishing.
    IvInputState hidden;
    hidden.connected = false;
    for (int i = 0; i < IV_KEY_COUNT; i++) hidden.key[i] = iv->state.key[i];
    iv_anim_step(iv->anim, hidden, iv->theme, seconds);
    return;
  }

  IvInputState effective = iv->state;
  if (!iv->hide_when_disconnected) {
    // The pad stays on screen at rest when nothing is attached, so the source
    // does not disappear from a scene the streamer is still composing.
    effective.connected = true;
  }
  iv_anim_step(iv->anim, effective, iv->theme, seconds);
}

static uint32_t input_visualizer_get_width(void *data) {
  auto *iv = static_cast<input_visualizer_source *>(data);
  const float w = iv->layout.loaded ? iv->layout.canvas_w : 1000.0f;
  return static_cast<uint32_t>(w * iv->scale);
}

static uint32_t input_visualizer_get_height(void *data) {
  auto *iv = static_cast<input_visualizer_source *>(data);
  const float h = iv->layout.loaded ? iv->layout.canvas_h : 680.0f;
  return static_cast<uint32_t>(h * iv->scale);
}

static void input_visualizer_render(void *data, gs_effect_t *) {
  auto *iv = static_cast<input_visualizer_source *>(data);

  const float width = static_cast<float>(input_visualizer_get_width(iv));
  const float height = static_cast<float>(input_visualizer_get_height(iv));
  if (width <= 0.0f || height <= 0.0f) return;

  if (iv->show_backdrop) {
    iv_fill_rect(0.0f, 0.0f, width, height, iv->theme.background, iv->opacity);
  }

  iv_ensure_effect(iv);
  iv_ensure_images(iv, iv->theme_id + "/" + iv->layout.id);

  IvRenderTargets targets;
  targets.tint = iv->tint_effect;
  if (iv->images_loaded) {
    targets.base = iv->base_image.image3.image2.image.texture;
    targets.overlay = iv->overlay_image.image3.image2.image.texture;
    // Sheets are rasterised above 1:1; derive the factor from the texture that
    // was actually loaded rather than assuming the build's scale setting.
    if (iv->layout.sheet_w > 0.0f) {
      targets.asset_scale =
          static_cast<float>(iv->overlay_image.image3.image2.image.cx) /
          iv->layout.sheet_w;
    }
  }

  const bool drew = iv_render_pad(targets, iv->layout, iv->anim, width, height,
                                  iv->opacity);
  if (!drew) {
    // Missing or unbuilt assets: show a muted block so the source is still
    // selectable and positionable in the scene.
    iv_fill_rect(0.0f, 0.0f, width, height, iv->theme.surface,
                 iv->opacity * 0.5f);
  }

  if (iv->show_keyboard || iv->show_mouse) {
    const float strip_h = height * 0.28f;
    iv_render_keys(iv->anim, iv->theme, width * 0.04f, height - strip_h,
                   width * 0.92f, strip_h, iv->opacity, iv->show_keyboard,
                   iv->show_mouse);
  }
}

static void input_visualizer_defaults(obs_data_t *settings) {
  obs_data_set_default_string(settings, "theme", "dark");
  obs_data_set_default_string(settings, "pad", "auto");
  obs_data_set_default_double(settings, "opacity", 1.0);
  obs_data_set_default_double(settings, "scale", 0.5);
  obs_data_set_default_bool(settings, "show_backdrop", false);
  obs_data_set_default_bool(settings, "hide_when_disconnected", false);
  obs_data_set_default_bool(settings, "desktop_enabled", false);
  obs_data_set_default_bool(settings, "show_keyboard", true);
  obs_data_set_default_bool(settings, "show_mouse", true);
  obs_data_set_default_bool(settings, "preview_mode", false);
}

bool obs_module_load(void) {
  obs_source_info si = {};
  si.id = kSourceId;
  si.type = OBS_SOURCE_TYPE_INPUT;
  si.output_flags = OBS_SOURCE_VIDEO | OBS_SOURCE_CUSTOM_DRAW;
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

void obs_module_unload(void) { iv_input_stop_desktop_capture(); }
