#pragma once

#include <stdint.h>

// Parts are named by function, not by console, so one renderer path serves both
// pads: face_down is A on Xbox and Cross on the DualSense. layout.json maps
// these to per-pad sprite rects and human labels.
enum IvPart {
  IV_FACE_DOWN = 0,
  IV_FACE_RIGHT,
  IV_FACE_LEFT,
  IV_FACE_UP,
  IV_DPAD_UP,
  IV_DPAD_DOWN,
  IV_DPAD_LEFT,
  IV_DPAD_RIGHT,
  IV_BUMPER_L,
  IV_BUMPER_R,
  IV_TRIGGER_L,
  IV_TRIGGER_R,
  IV_BACK,
  IV_START,
  IV_GUIDE,
  IV_TOUCHPAD,
  IV_PART_COUNT
};

// Manifest key for each part, indexed by IvPart.
const char *iv_part_key(int part);

// Desktop inputs, kept as an array so the animator can treat them uniformly
// with pad parts.
enum IvKey {
  IV_KEY_W = 0,
  IV_KEY_A,
  IV_KEY_S,
  IV_KEY_D,
  IV_KEY_SPACE,
  IV_KEY_SHIFT,
  IV_KEY_CTRL,
  IV_MOUSE_L,
  IV_MOUSE_R,
  IV_KEY_COUNT
};

const char *iv_key_label(int key);

enum IvPadKind {
  IV_PAD_UNKNOWN = 0,
  IV_PAD_XBOX,
  IV_PAD_PS5,
};

struct IvInputState {
  bool connected = false;
  IvPadKind kind = IV_PAD_UNKNOWN;

  // Digital parts, indexed by IvPart.
  bool part[IV_PART_COUNT] = {};

  // Analog triggers, 0..1. The digital IV_TRIGGER_* flags track whether they
  // are past the actuation point; the renderer fades on these values instead so
  // partial pulls still read on stream.
  float lt = 0.0f;
  float rt = 0.0f;

  // Sticks, -1..1, y positive up.
  float lx = 0.0f, ly = 0.0f;
  float rx = 0.0f, ry = 0.0f;
  bool l3 = false, r3 = false;

  bool key[IV_KEY_COUNT] = {};
};

void iv_input_poll();
IvInputState iv_input_get();

// Starts the keyboard/mouse event tap on its own run loop thread. No-op if
// already running, or if accessibility permission has not been granted.
void iv_input_start_desktop_capture();
void iv_input_stop_desktop_capture();
