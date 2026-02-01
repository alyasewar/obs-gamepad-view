#pragma once

#include <string>

struct IvTheme {
  std::string id;
  std::string name;
  std::string layout_path;
  std::string buttons_path;
  std::string dpad_path;
  std::string sticks_path;
  std::string bumper_path;
  std::string trigger_path;
  std::string start_select_path;
  std::string quadrant_path;
  std::string disconnected_path;
  uint32_t background = 0xff14161b;
  uint32_t surface = 0xff1f232b;
  uint32_t stroke = 0xffe6e9ef;
  uint32_t accent = 0xff4e8cff;
  uint32_t text = 0xfff2f4f8;
  uint32_t pressed = 0xff4e8cff;
};

IvTheme iv_theme_load(const std::string &theme_id, const std::string &base_path);
