#pragma once

#include <stdint.h>

struct IvInputState {
  bool connected = false;
  bool a = false;
  bool b = false;
  bool x = false;
  bool y = false;
  bool lb = false;
  bool rb = false;
  bool back = false;
  bool start = false;
  bool dpad_up = false;
  bool dpad_down = false;
  bool dpad_left = false;
  bool dpad_right = false;
  float lx = 0.0f;
  float ly = 0.0f;
  float rx = 0.0f;
  float ry = 0.0f;
  float lt = 0.0f;
  float rt = 0.0f;
  bool key_w = false;
  bool key_a = false;
  bool key_s = false;
  bool key_d = false;
  bool key_space = false;
  bool key_shift = false;
  bool key_ctrl = false;
  bool mouse_left = false;
  bool mouse_right = false;
};

void iv_input_poll();
IvInputState iv_input_get();
