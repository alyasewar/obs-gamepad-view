#include "input_state.h"

#if defined(__APPLE__)
#include <GameController/GameController.h>
#include <Carbon/Carbon.h>
#include <atomic>
#include <mutex>

static std::mutex g_mutex;
static IvInputState g_state;
static GCController *g_controller = nil;

static void iv_set_connected(bool connected) {
  std::lock_guard<std::mutex> lock(g_mutex);
  g_state.connected = connected;
}

static void iv_set_button(bool &field, bool value) {
  field = value;
}

static void iv_set_axis(float &field, float value) {
  field = value;
}

static void iv_controller_connected(NSNotification *note) {
  GCController *controller = note.object;
  if (!controller) return;
  g_controller = controller;
  iv_set_connected(true);
}

static void iv_controller_disconnected(NSNotification *note) {
  GCController *controller = note.object;
  if (controller == g_controller) {
    g_controller = nil;
  }
  iv_set_connected(false);
}

static void iv_ensure_notifications() {
  static dispatch_once_t once;
  dispatch_once(&once, ^{
    [[NSNotificationCenter defaultCenter] addObserverForName:GCControllerDidConnectNotification
                                                      object:nil
                                                       queue:nil
                                                  usingBlock:^(NSNotification *note) {
      iv_controller_connected(note);
    }];
    [[NSNotificationCenter defaultCenter] addObserverForName:GCControllerDidDisconnectNotification
                                                      object:nil
                                                       queue:nil
                                                  usingBlock:^(NSNotification *note) {
      iv_controller_disconnected(note);
    }];
  });
}

static void iv_update_from_gamepad(GCController *controller) {
  if (!controller) return;
  GCGamepad *gamepad = controller.gamepad;
  GCExtendedGamepad *extended = controller.extendedGamepad;

  std::lock_guard<std::mutex> lock(g_mutex);
  g_state.connected = true;

  if (extended) {
    iv_set_button(g_state.a, extended.buttonA.isPressed);
    iv_set_button(g_state.b, extended.buttonB.isPressed);
    iv_set_button(g_state.x, extended.buttonX.isPressed);
    iv_set_button(g_state.y, extended.buttonY.isPressed);
    iv_set_button(g_state.lb, extended.leftShoulder.isPressed);
    iv_set_button(g_state.rb, extended.rightShoulder.isPressed);
    iv_set_button(g_state.back, extended.buttonOptions.isPressed);
    iv_set_button(g_state.start, extended.buttonMenu.isPressed);

    iv_set_axis(g_state.lx, extended.leftThumbstick.xAxis.value);
    iv_set_axis(g_state.ly, extended.leftThumbstick.yAxis.value);
    iv_set_axis(g_state.rx, extended.rightThumbstick.xAxis.value);
    iv_set_axis(g_state.ry, extended.rightThumbstick.yAxis.value);

    iv_set_axis(g_state.lt, extended.leftTrigger.value);
    iv_set_axis(g_state.rt, extended.rightTrigger.value);

    if (extended.dpad) {
      iv_set_button(g_state.dpad_up, extended.dpad.up.isPressed);
      iv_set_button(g_state.dpad_down, extended.dpad.down.isPressed);
      iv_set_button(g_state.dpad_left, extended.dpad.left.isPressed);
      iv_set_button(g_state.dpad_right, extended.dpad.right.isPressed);
    }
    return;
  }

  if (gamepad) {
    iv_set_button(g_state.a, gamepad.buttonA.isPressed);
    iv_set_button(g_state.b, gamepad.buttonB.isPressed);
    iv_set_button(g_state.x, gamepad.buttonX.isPressed);
    iv_set_button(g_state.y, gamepad.buttonY.isPressed);
    iv_set_button(g_state.lb, gamepad.leftShoulder.isPressed);
    iv_set_button(g_state.rb, gamepad.rightShoulder.isPressed);
    if (gamepad.dpad) {
      iv_set_button(g_state.dpad_up, gamepad.dpad.up.isPressed);
      iv_set_button(g_state.dpad_down, gamepad.dpad.down.isPressed);
      iv_set_button(g_state.dpad_left, gamepad.dpad.left.isPressed);
      iv_set_button(g_state.dpad_right, gamepad.dpad.right.isPressed);
    }
  }
}

IvInputState iv_input_get() {
  std::lock_guard<std::mutex> lock(g_mutex);
  return g_state;
}

static void iv_set_key(int keycode, bool down) {
  std::lock_guard<std::mutex> lock(g_mutex);
  switch (keycode) {
    case kVK_ANSI_W:
      g_state.key_w = down;
      break;
    case kVK_ANSI_A:
      g_state.key_a = down;
      break;
    case kVK_ANSI_S:
      g_state.key_s = down;
      break;
    case kVK_ANSI_D:
      g_state.key_d = down;
      break;
    case kVK_Space:
      g_state.key_space = down;
      break;
    case kVK_Shift:
    case kVK_RightShift:
      g_state.key_shift = down;
      break;
    case kVK_Control:
    case kVK_RightControl:
      g_state.key_ctrl = down;
      break;
    default:
      break;
  }
}

static void iv_set_mouse_button(CGMouseButton button, bool down) {
  std::lock_guard<std::mutex> lock(g_mutex);
  if (button == kCGMouseButtonLeft) g_state.mouse_left = down;
  if (button == kCGMouseButtonRight) g_state.mouse_right = down;
}

static CGEventRef iv_event_tap_callback(CGEventTapProxy, CGEventType type, CGEventRef event, void *) {
  if (type == kCGEventTapDisabledByTimeout || type == kCGEventTapDisabledByUserInput) {
    return event;
  }

  if (type == kCGEventKeyDown || type == kCGEventKeyUp) {
    int keycode = (int)CGEventGetIntegerValueField(event, kCGKeyboardEventKeycode);
    iv_set_key(keycode, type == kCGEventKeyDown);
  }

  if (type == kCGEventLeftMouseDown || type == kCGEventLeftMouseUp) {
    iv_set_mouse_button(kCGMouseButtonLeft, type == kCGEventLeftMouseDown);
  }

  if (type == kCGEventRightMouseDown || type == kCGEventRightMouseUp) {
    iv_set_mouse_button(kCGMouseButtonRight, type == kCGEventRightMouseDown);
  }

  return event;
}

static void iv_start_event_tap() {
  static dispatch_once_t once;
  dispatch_once(&once, ^{
    CGEventMask mask = CGEventMaskBit(kCGEventKeyDown) |
                       CGEventMaskBit(kCGEventKeyUp) |
                       CGEventMaskBit(kCGEventLeftMouseDown) |
                       CGEventMaskBit(kCGEventLeftMouseUp) |
                       CGEventMaskBit(kCGEventRightMouseDown) |
                       CGEventMaskBit(kCGEventRightMouseUp);
    CFMachPortRef tap = CGEventTapCreate(kCGSessionEventTap,
                                         kCGHeadInsertEventTap,
                                         kCGEventTapOptionDefault,
                                         mask,
                                         iv_event_tap_callback,
                                         nullptr);
    if (!tap) return;
    CFRunLoopSourceRef source = CFMachPortCreateRunLoopSource(kCFAllocatorDefault, tap, 0);
    CFRunLoopAddSource(CFRunLoopGetCurrent(), source, kCFRunLoopCommonModes);
    CGEventTapEnable(tap, true);
    CFRelease(source);
    CFRelease(tap);
  });
}

void iv_input_poll() {
  iv_ensure_notifications();
  iv_start_event_tap();

  if (!g_controller) {
    NSArray<GCController *> *controllers = [GCController controllers];
    if (controllers.count > 0) {
      g_controller = controllers[0];
      iv_set_connected(true);
    } else {
      iv_set_connected(false);
      return;
    }
  }

  iv_update_from_gamepad(g_controller);
}

#else

void iv_input_poll() {}
IvInputState iv_input_get() { return {}; }

#endif
