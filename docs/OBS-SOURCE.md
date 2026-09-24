# OBS Source

The plugin registers one source, `Input Visualizer`.

## Settings

The properties panel is grouped into four sections.

### Controller

- **Layout** — `Auto-detect`, `Xbox`, or `PlayStation 5`. Auto-detect reads the
  connected pad and switches on its own; the other two pin a layout regardless
  of what is plugged in.
- **Detected** — read-only. Shows what auto-detect actually resolved to, so a
  wrong layout is diagnosable without digging through logs.

### Appearance

- **Theme** — `Dark`, `Light`, or `Pastel`.
- **Size** — multiplier on the pad's 1000x680 canvas. The default 0.5 gives a
  500x340 source.
- **Opacity**
- **Draw backdrop** — fills the source with the theme background colour. Off by
  default, so the pad drops straight onto a scene.

### Behaviour

- **Hide when no controller is connected** — fades the overlay out on
  disconnect instead of leaving it on screen at rest.
- **Preview mode** — cycles every input so the source can be positioned and
  styled without a controller attached.

### Keyboard & mouse

Off by default. Enabling the group starts a `CGEventTap`, which needs
Accessibility permission (System Settings → Privacy & Security → Accessibility,
then add OBS). Gamepad capture does not need it. Inside the group:

- **Keyboard (WASD, modifiers)**
- **Mouse buttons**

## Sizing and placement

The source reports `canvas x Size` as its native dimensions and letterboxes the
pad inside that box, so it never stretches. Use OBS transform handles for
placement as usual.

## Behaviour notes

- Auto-detect re-resolves whenever the detected controller kind changes, so
  swapping an Xbox pad for a DualSense mid-scene switches layout on the fly.
- The pad fades in over `connectFadeMs` rather than popping into frame.
- With no controller attached and "Hide when disconnected" off, the pad is drawn
  at rest so the source stays visible while you compose a scene.
