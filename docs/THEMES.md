# Themes

Themes define the look of the overlay using a JSON manifest and SVGZ assets.

## Theme structure

```
themes/
  light/
    theme.json
    assets/
  dark/
    theme.json
    assets/
  pastel/
    theme.json
    assets/
```

## Manifest fields

- `name`: Display name in OBS.
- `id`: Unique identifier.
- `version`: Theme version.
- `author`: Theme author.
- `description`: Short description.
- `assets.layout`: Base SVG layout.
- `assets.buttons`: SVG for buttons and states.
- `palette`: Colors used by the renderer.
- `typography.font`: Preferred font name.
- `typography.size`: Base font size.
- `animation`: Fade timings in ms.

## Add a new theme

1. Copy `themes/default/` to `themes/my-theme/`.
2. Update `themes/my-theme/theme.json`.
3. Replace the SVGs in `themes/my-theme/assets/`.
4. Restart OBS or reload the source.
