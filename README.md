# Premiere Pro Theme

A [Windhawk](https://windhawk.net/) mod that recolors the whole Adobe Premiere
Pro interface — panels, timeline, monitors, window frame and menu bar — far
past what the Appearance brightness slider reaches.

Seven palettes, three of them near-black and four tinted, plus a custom one.

## Screenshots

The same project in each palette. First, Premiere without the mod:

![Premiere Pro without the mod](images/stock.png)

**Onyx** — the default: near black, and neutral:

![Onyx palette](images/onyx.png)

**Premiere** — the violet sampled from the app icon:

![Premiere palette](images/premiere.png)

**Comfy** — warm brown, low contrast for long sessions:

![Comfy palette](images/comfy.png)

**Neon** — near black with magenta:

![Neon palette](images/neon.png)

**Glitch** — acid green with a magenta accent:

![Glitch palette](images/glitch.png)

## Install

1. Install [Windhawk](https://windhawk.net/).
2. Find **Premiere Pro Theme** in the mod browser and install it.
3. Restart Premiere. Premiere only repaints everything on startup.

To build it yourself instead: create a new mod in Windhawk, paste
[`premiere-pro-theme.wh.cpp`](premiere-pro-theme.wh.cpp) over the template, and
compile.

## What it does

Premiere paints its interface through four different mechanisms, and the mod
covers all four:

- **`dvaui.dll`** — Adobe's UI toolkit, which hands out the Spectrum gray ramp
  (`#1D1D1D`, `#262626`, `#303030`, `#4B4B4B`). Thirty-one color functions are
  intercepted here: the classic theme, the Spectrum ramp, and the DNA/skins
  families.
- **`UIFramework.dll`** — Premiere's own drawing layer, for surfaces that fill a
  rectangle without ever consulting the theme.
- **The Direct2D brush factory** — where most solid fills pass through,
  whoever picked the color.
- **Win32** — the title bar, the `File / Edit / Clip` menu bar and the dropdown
  menus, none of which any theme reaches.

The full explanation of how each is found, what is deliberately left untouched,
and why the palettes are the colors they are, is in the mod's own readme at the
top of [`premiere-pro-theme.wh.cpp`](premiere-pro-theme.wh.cpp) — which is also
what Windhawk shows on the mod's page. It is kept there rather than duplicated
here, so there is only one copy to keep true.

## Compatibility

Every color function is looked up by name at startup; the ones the running build
exports get hooked and the rest are logged and skipped. A Premiere version that
moved or dropped a function loses that surface, not the mod.

| Premiere | Color functions found | Drawing primitives |
|----------|-----------------------|--------------------|
| 2026     | 31 of 31              | 4 of 4             |
| 2023     | 27 of 31              | 4 of 4             |

The window frame, menu bar and native dialogs are Windows, not Premiere, and
work on any version. Native dark mode needs Windows 10 build 17763 or newer.

## Known limitations

Two surfaces are not themed, and the mod says so rather than patching blind:

- **The Home screen**, which is rendered by UXP with its own CSS and hands out
  no color at all.
- **The band around the video in the monitors**, with a tinted palette. Zoomed
  out, the monitors paint the area around the picture a gray made from the red
  channel of the panel color, through a path none of the mod's hooks reaches.
  On the near-black palettes that is the panel color itself; on a tinted one it
  reads as a neutral band.

Both are written up in full, with the measurements, in the mod's readme.

## Credits

The menu bar technique — the undocumented `WM_UAHDRAWMENU` messages and the
`DarkMode::Menu` theme class — comes from
[win32-darkmode](https://github.com/adzm/win32-darkmode) by adzm, MIT licensed.

## License

MIT. See [LICENSE](LICENSE).

Not affiliated with or endorsed by Adobe. Adobe and Premiere Pro are trademarks
of Adobe Inc.
