#include "input_state.h"

#if defined(__APPLE__)

#include <Carbon/Carbon.h>
#include <GameController/GameController.h>

#include <atomic>
#include <cmath>
#include <mutex>

namespace {

std::mutex g_mutex;
IvInputState g_state;          // guarded by g_mutex
GCController *g_controller = nil;  // guarded by g_mutex

// Sticks rest a little off centre on worn hardware. Without this the overlay
// shows a permanently nudged stick and jitters, which reads as a broken
// overlay rather than a broken pad.
constexpr float kStickDeadzone = 0.12f;
// Matches the actuation point Xbox and DualSense triggers report.
constexpr float kTriggerThreshold = 0.12f;

// Rescales past the deadzone so the stick still reaches full travel, and so it
// sits exactly centred at rest.
void apply_deadzone(float &x, float &y) {
  const float magnitude = std::sqrt(x * x + y * y);
  if (magnitude <= kStickDeadzone) {
    x = 0.0f;
    y = 0.0f;
    return;
  }
  const float scaled = (magnitude - kStickDeadzone) / (1.0f - kStickDeadzone);
  const float factor = std::fmin(scaled, 1.0f) / magnitude;
  x *= factor;
  y *= factor;
}

// Class checks go through the runtime so the plugin still builds against SDKs
// predating GCDualSenseGamepad and keeps working on older macOS.
bool is_kind_of(id object, NSString *class_name) {
  Class cls = NSClassFromString(class_name);
  return cls != nil && [object isKindOfClass:cls];
}

IvPadKind detect_kind(GCController *controller) {
  GCExtendedGamepad *pad = controller.extendedGamepad;
  if (is_kind_of(pad, @"GCDualSenseGamepad") || is_kind_of(pad, @"GCDualShockGamepad")) {
    return IV_PAD_PS5;
  }
  if (is_kind_of(pad, @"GCXboxGamepad")) {
    return IV_PAD_XBOX;
  }

  // Older macOS has no per-product gamepad classes; fall back to the product
  // category string, then to the vendor name.
  NSString *hint = nil;
  if ([controller respondsToSelector:@selector(productCategory)]) {
    hint = controller.productCategory;
  }
  if (hint.length == 0) hint = controller.vendorName;
  if (hint.length == 0) return IV_PAD_UNKNOWN;

  NSString *lower = hint.lowercaseString;
  if ([lower containsString:@"dualsense"] || [lower containsString:@"dualshock"] ||
      [lower containsString:@"playstation"] || [lower containsString:@"wireless controller"]) {
    return IV_PAD_PS5;
  }
  if ([lower containsString:@"xbox"]) {
    return IV_PAD_XBOX;
  }
  return IV_PAD_UNKNOWN;
}

bool button_pressed(GCControllerButtonInput *button) {
  return button != nil && button.isPressed;
}

// Reads a button that only exists on newer SDKs / newer pads.
bool optional_button(id owner, NSString *key) {
  if (owner == nil) return false;
  SEL selector = NSSelectorFromString(key);
  if (![owner respondsToSelector:selector]) return false;
  id value = [owner valueForKey:key];
  if (![value isKindOfClass:[GCControllerButtonInput class]]) return false;
  return ((GCControllerButtonInput *)value).isPressed;
}

void read_controller(GCController *controller, IvInputState &out) {
  GCExtendedGamepad *pad = controller.extendedGamepad;
  if (pad == nil) {
    // A pad with no extended profile still counts as attached; it just has
    // nothing this overlay can show.
    out.connected = true;
    return;
  }

  out.connected = true;
  out.kind = detect_kind(controller);

  out.part[IV_FACE_DOWN] = button_pressed(pad.buttonA);
  out.part[IV_FACE_RIGHT] = button_pressed(pad.buttonB);
  out.part[IV_FACE_LEFT] = button_pressed(pad.buttonX);
  out.part[IV_FACE_UP] = button_pressed(pad.buttonY);

  out.part[IV_BUMPER_L] = button_pressed(pad.leftShoulder);
  out.part[IV_BUMPER_R] = button_pressed(pad.rightShoulder);

  out.part[IV_DPAD_UP] = button_pressed(pad.dpad.up);
  out.part[IV_DPAD_DOWN] = button_pressed(pad.dpad.down);
  out.part[IV_DPAD_LEFT] = button_pressed(pad.dpad.left);
  out.part[IV_DPAD_RIGHT] = button_pressed(pad.dpad.right);

  out.part[IV_BACK] = optional_button(pad, @"buttonOptions");
  out.part[IV_START] = button_pressed(pad.buttonMenu);
  out.part[IV_GUIDE] = optional_button(pad, @"buttonHome");
  out.part[IV_TOUCHPAD] = optional_button(pad, @"touchpadButton");

  out.lt = pad.leftTrigger.value;
  out.rt = pad.rightTrigger.value;
  out.part[IV_TRIGGER_L] = out.lt > kTriggerThreshold;
  out.part[IV_TRIGGER_R] = out.rt > kTriggerThreshold;

  out.lx = pad.leftThumbstick.xAxis.value;
  out.ly = pad.leftThumbstick.yAxis.value;
  out.rx = pad.rightThumbstick.xAxis.value;
  out.ry = pad.rightThumbstick.yAxis.value;
  apply_deadzone(out.lx, out.ly);
  apply_deadzone(out.rx, out.ry);

  out.l3 = optional_button(pad, @"leftThumbstickButton");
  out.r3 = optional_button(pad, @"rightThumbstickButton");
}

// ---------------------------------------------------------------------------
// Keyboard / mouse capture
//
// The tap must live on a thread that actually runs a run loop. The previous
// version installed it on whichever thread first rendered a frame -- an OBS
// graphics thread with no run loop -- so the callback never fired.
// ---------------------------------------------------------------------------

std::mutex g_tap_mutex;
CFMachPortRef g_tap = nullptr;
CFRunLoopRef g_tap_runloop = nullptr;
std::atomic<bool> g_tap_running{false};

void set_key(int keycode, bool down) {
  std::lock_guard<std::mutex> lock(g_mutex);
  switch (keycode) {
    case kVK_ANSI_W: g_state.key[IV_KEY_W] = down; break;
    case kVK_ANSI_A: g_state.key[IV_KEY_A] = down; break;
    case kVK_ANSI_S: g_state.key[IV_KEY_S] = down; break;
    case kVK_ANSI_D: g_state.key[IV_KEY_D] = down; break;
    case kVK_Space: g_state.key[IV_KEY_SPACE] = down; break;
    case kVK_Shift:
    case kVK_RightShift: g_state.key[IV_KEY_SHIFT] = down; break;
    case kVK_Control:
    case kVK_RightControl: g_state.key[IV_KEY_CTRL] = down; break;
    default: break;
  }
}

CGEventRef tap_callback(CGEventTapProxy, CGEventType type, CGEventRef event, void *) {
  // The system disables a tap that takes too long. Re-arm it, or keyboard and
  // mouse silently stop updating for the rest of the session.
  if (type == kCGEventTapDisabledByTimeout || type == kCGEventTapDisabledByUserInput) {
    std::lock_guard<std::mutex> lock(g_tap_mutex);
    if (g_tap) CGEventTapEnable(g_tap, true);
    return event;
  }

  switch (type) {
    case kCGEventKeyDown:
    case kCGEventKeyUp:
      set_key((int)CGEventGetIntegerValueField(event, kCGKeyboardEventKeycode),
              type == kCGEventKeyDown);
      break;
    case kCGEventLeftMouseDown:
    case kCGEventLeftMouseUp: {
      std::lock_guard<std::mutex> lock(g_mutex);
      g_state.key[IV_MOUSE_L] = (type == kCGEventLeftMouseDown);
      break;
    }
    case kCGEventRightMouseDown:
    case kCGEventRightMouseUp: {
      std::lock_guard<std::mutex> lock(g_mutex);
      g_state.key[IV_MOUSE_R] = (type == kCGEventRightMouseDown);
      break;
    }
    default:
      break;
  }
  return event;
}

void tap_thread_main() {
  @autoreleasepool {
    const CGEventMask mask =
        CGEventMaskBit(kCGEventKeyDown) | CGEventMaskBit(kCGEventKeyUp) |
        CGEventMaskBit(kCGEventLeftMouseDown) | CGEventMaskBit(kCGEventLeftMouseUp) |
        CGEventMaskBit(kCGEventRightMouseDown) | CGEventMaskBit(kCGEventRightMouseUp);

    CFMachPortRef tap =
        CGEventTapCreate(kCGSessionEventTap, kCGHeadInsertEventTap,
                         kCGEventTapOptionListenOnly, mask, tap_callback, nullptr);
    if (!tap) {
      // No accessibility permission. Gamepad capture is unaffected.
      g_tap_running = false;
      return;
    }

    CFRunLoopSourceRef source = CFMachPortCreateRunLoopSource(kCFAllocatorDefault, tap, 0);
    {
      std::lock_guard<std::mutex> lock(g_tap_mutex);
      g_tap = tap;
      g_tap_runloop = CFRunLoopGetCurrent();
      CFRetain(g_tap_runloop);
    }
    CFRunLoopAddSource(CFRunLoopGetCurrent(), source, kCFRunLoopCommonModes);
    CGEventTapEnable(tap, true);

    CFRunLoopRun();

    CFRunLoopRemoveSource(CFRunLoopGetCurrent(), source, kCFRunLoopCommonModes);
    CGEventTapEnable(tap, false);
    {
      std::lock_guard<std::mutex> lock(g_tap_mutex);
      if (g_tap_runloop) {
        CFRelease(g_tap_runloop);
        g_tap_runloop = nullptr;
      }
      g_tap = nullptr;
    }
    CFRelease(source);
    CFRelease(tap);
    g_tap_running = false;
  }
}

void ensure_notifications() {
  static dispatch_once_t once;
  dispatch_once(&once, ^{
    [[NSNotificationCenter defaultCenter]
        addObserverForName:GCControllerDidConnectNotification
                    object:nil
                     queue:nil
                usingBlock:^(NSNotification *note) {
                  GCController *controller = note.object;
                  if (!controller) return;
                  std::lock_guard<std::mutex> lock(g_mutex);
                  g_controller = controller;
                  g_state.connected = true;
                  g_state.kind = detect_kind(controller);
                }];
    [[NSNotificationCenter defaultCenter]
        addObserverForName:GCControllerDidDisconnectNotification
                    object:nil
                     queue:nil
                usingBlock:^(NSNotification *note) {
                  GCController *controller = note.object;
                  std::lock_guard<std::mutex> lock(g_mutex);
                  if (controller == g_controller) {
                    g_controller = nil;
                    g_state.connected = false;
                  }
                }];
  });
}

}  // namespace

void iv_input_poll() {
  @autoreleasepool {
    ensure_notifications();

    GCController *controller = nil;
    {
      std::lock_guard<std::mutex> lock(g_mutex);
      controller = g_controller;
    }

    if (controller == nil) {
      // Connect notifications only arrive while a run loop is pumping, which is
      // not guaranteed for whichever thread ticks us, so also sweep the list.
      NSArray<GCController *> *controllers = [GCController controllers];
      if (controllers.count > 0) {
        controller = controllers.firstObject;
        std::lock_guard<std::mutex> lock(g_mutex);
        g_controller = controller;
      } else {
        std::lock_guard<std::mutex> lock(g_mutex);
        // Drop stale button state so a reconnect does not flash the last frame
        // of input from the previous pad.
        IvInputState cleared;
        for (int i = 0; i < IV_KEY_COUNT; i++) cleared.key[i] = g_state.key[i];
        g_state = cleared;
        return;
      }
    }

    IvInputState sampled;
    read_controller(controller, sampled);

    std::lock_guard<std::mutex> lock(g_mutex);
    // Keyboard and mouse come from the event tap, not from this sample.
    for (int i = 0; i < IV_KEY_COUNT; i++) sampled.key[i] = g_state.key[i];
    g_state = sampled;
  }
}

IvInputState iv_input_get() {
  std::lock_guard<std::mutex> lock(g_mutex);
  return g_state;
}

void iv_input_start_desktop_capture() {
  bool expected = false;
  if (!g_tap_running.compare_exchange_strong(expected, true)) return;

  NSThread *thread = [[NSThread alloc] initWithBlock:^{
    tap_thread_main();
  }];
  thread.name = @"input-visualizer-tap";
  [thread start];
}

void iv_input_stop_desktop_capture() {
  CFRunLoopRef runloop = nullptr;
  {
    std::lock_guard<std::mutex> lock(g_tap_mutex);
    runloop = g_tap_runloop;
    if (runloop) CFRetain(runloop);
  }
  if (!runloop) {
    g_tap_running = false;
    return;
  }
  CFRunLoopStop(runloop);
  CFRelease(runloop);
}

#else

void iv_input_poll() {}
IvInputState iv_input_get() { return IvInputState(); }
void iv_input_start_desktop_capture() {}
void iv_input_stop_desktop_capture() {}

#endif
