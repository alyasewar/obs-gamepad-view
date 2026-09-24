// Host-side check for the JSON reader and layout loader. These two files carry
// no OBS dependency on purpose, so the manifest parsing can be exercised
// without an OBS SDK present.
//
//   clang++ -std=c++17 -I src tools/layout_selftest.cpp src/json.cpp \
//       src/layout.cpp -o build/layout_selftest && ./build/layout_selftest .

#include <cmath>
#include <cstdio>
#include <string>

#include "json.h"
#include "layout.h"

static int g_failures = 0;

static void expect(bool condition, const std::string &what) {
  if (!condition) {
    printf("  FAIL %s\n", what.c_str());
    g_failures++;
  }
}

static void expect_near(float got, float want, const std::string &what) {
  if (std::fabs(got - want) > 0.001f) {
    printf("  FAIL %s: got %.3f want %.3f\n", what.c_str(), got, want);
    g_failures++;
  }
}

static void test_json() {
  printf("json\n");

  ivjson::Value v;
  expect(ivjson::parse(R"({"a":1,"b":[1,2,3],"c":{"d":"x"},"e":true,"f":null})", v),
         "parses a nested document");
  expect_near(static_cast<float>(v["a"].num()), 1.0f, "number");
  expect(v["b"].size() == 3, "array length");
  expect_near(static_cast<float>(v["b"][static_cast<size_t>(2)].num()), 3.0f, "array element");
  expect(v["c"]["d"].str() == "x", "nested string");
  expect(v["e"].flag() == true, "bool");
  expect(v["f"].is_null(), "null");

  // Absent lookups must stay chainable rather than blow up.
  expect(v["nope"].is_null(), "missing key is null");
  expect(v["nope"]["deeper"].is_null(), "missing key chains");
  expect_near(static_cast<float>(v["nope"].num(7.0)), 7.0f, "missing key uses fallback");

  ivjson::Value bad;
  expect(!ivjson::parse("{\"a\":}", bad), "rejects malformed object");
  expect(!ivjson::parse("[1,2", bad), "rejects unterminated array");
  expect(!ivjson::parse("", bad), "rejects empty input");
  expect(!ivjson::parse("{} trailing", bad), "rejects trailing junk");

  ivjson::Value esc;
  expect(ivjson::parse(R"({"s":"a\"b\\c\nd"})", esc), "parses escapes");
  expect(esc["s"].str() == "a\"b\\c\nd", "escape decoding");

  ivjson::Value neg;
  expect(ivjson::parse(R"({"n":-12.5,"x":1e2})", neg), "parses signed and exponent numbers");
  expect_near(static_cast<float>(neg["n"].num()), -12.5f, "negative number");
  expect_near(static_cast<float>(neg["x"].num()), 100.0f, "exponent number");
}

static void test_layout(const std::string &root, const std::string &pad,
                        float canvas_w, float canvas_h) {
  printf("layout: %s\n", pad.c_str());

  IvLayout layout = iv_layout_load(pad, "dark", root);
  expect(layout.loaded, "manifest loaded");
  if (!layout.loaded) return;

  expect_near(layout.canvas_w, canvas_w, "canvas width");
  expect_near(layout.canvas_h, canvas_h, "canvas height");
  expect(layout.stick_travel > 0.0f, "stick travel set");

  // Every pad must supply the parts the renderer unconditionally draws.
  const int required[] = {IV_FACE_DOWN, IV_FACE_RIGHT, IV_FACE_LEFT,  IV_FACE_UP,
                          IV_DPAD_UP,   IV_DPAD_DOWN,  IV_DPAD_LEFT,  IV_DPAD_RIGHT,
                          IV_BUMPER_L,  IV_BUMPER_R,   IV_TRIGGER_L,  IV_TRIGGER_R,
                          IV_BACK,      IV_START};
  for (int part : required) {
    expect(layout.parts[part].present,
           std::string("part present: ") + iv_part_key(part));
  }

  for (int i = 0; i < IV_PART_COUNT; i++) {
    const IvSprite &s = layout.parts[i];
    if (!s.present) continue;
    const std::string key = iv_part_key(i);
    expect(s.sx >= 0.0f && s.sy >= 0.0f && s.sx + s.sw <= layout.sheet_w &&
               s.sy + s.sh <= layout.sheet_h,
           "src inside sheet: " + key);
    expect(s.dx >= 0.0f && s.dy >= 0.0f && s.dx + s.sw <= layout.canvas_w &&
               s.dy + s.sh <= layout.canvas_h,
           "dst inside canvas: " + key);
  }

  expect(layout.left_stick.present && layout.right_stick.present, "both sticks present");
  expect(layout.left_stick.pressed.present && layout.right_stick.pressed.present,
         "both sticks have a pressed cap");
  expect(layout.base_png.find("/themes/dark/" + pad + "/base.png") != std::string::npos,
         "base sheet path is theme-scoped");

  // dst defaults to src for in-place sprites, and differs for strip sprites.
  expect_near(layout.parts[IV_FACE_DOWN].dx, layout.parts[IV_FACE_DOWN].sx,
              "in-place sprite dst defaults to src x");
  expect(layout.parts[IV_BUMPER_L].dx != layout.parts[IV_BUMPER_L].sx,
         "strip sprite carries its own dst");
}

static void test_missing(const std::string &root) {
  printf("layout: missing manifest\n");
  IvLayout layout = iv_layout_load("does_not_exist", "dark", root);
  expect(!layout.loaded, "absent manifest reports not loaded");
  IvLayout empty = iv_layout_load("xbox", "dark", "");
  expect(!empty.loaded, "empty assets root reports not loaded");
}

int main(int argc, char **argv) {
  const std::string root = argc > 1 ? argv[1] : ".";
  test_json();
  test_layout(root, "xbox", 1000.0f, 680.0f);
  test_layout(root, "ps5", 1000.0f, 680.0f);
  test_missing(root);

  if (g_failures) {
    printf("\n%d failure(s)\n", g_failures);
    return 1;
  }
  printf("\nall checks passed\n");
  return 0;
}
