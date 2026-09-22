# Tests

`harness.cpp` holds logic tests for the mod, compiled together with it and with
the Windhawk API stubbed out. They cover:

- the color table and its generations;
- the content scope;
- the palette highlight and its luminance matching;
- the UXP rewrite, both halves of it: the stylesheet, including that a border
  past the ceiling is recolored while text at the same value is not, and the
  design tokens in the panel's own script, including that an icon stroke and a
  loose string beside them are left alone;
- the file redirect on real temporary files, for a stylesheet and for a script,
  including that the copy carries the extension of the file it stands in for;
- the custom theme fields, checked against the defaults the settings block
  itself ships, including a whole theme written as #RRGGBBAA by a color picker
  — the alpha is dropped rather than the field refused — and the theme the mod
  writes out for sharing, which is written for every palette and never carries
  an alpha back;
- every built-in palette against the rules the readme states: a rising ramp,
  text and accent that carry, and a highlight white can sit on;
- that the palette list in the settings block, the palette tables in the mod's
  readme, and the palettes in the code are all the same set;
- the module ranges, including a dva module that unloads;
- which windows the frame work is spent on: top level, and with a frame;
- the monitor band: the DisplaySurface range, and the executable standing in
  for it where Premiere ships no such module — which must reach nothing until
  Premiere has made its device and has a framed window, because setting it at
  init crashed Premiere on startup — which colors are the band's —
  held against every palette's own conversion of Premiere's gray, not just the
  neutral ones — what counts as a full-surface draw, that only the exact float4
  is taken for a color, that a recording's state does not outlive it at Reset,
  Close or ClearState, that a partial install of the layer's hooks stops it
  acting at all, that the recolor does not depend on the order DisplaySurface
  records in, that a color is never replayed under a root signature it was not
  set for, that each hook is taken from the vtable slot holding the method it
  names, that a thread recording none of DisplaySurface's work never walks the
  slot table, and that recoloring
  rewrites the band's own color while leaving the black behind the picture
  untouched;
- the menu theme bookkeeping;
- how the renamed dvaui functions are counted;
- the toolkit linked into the executable, against a real one: the installed
  Premiere is mapped the way the loader maps it and every entry point is looked
  up in it for real — the class names, the locators, the table lengths, the
  bodies behind the two brush slots, that the monitor renderer is in there
  under its own name, and that a length or a body that is not the measured one
  is refused. This is the one test that needs something
  installed, a Premiere 2026.3 or later; without one it says it skipped.

Run them from the repository root, with Windhawk and PowerShell 7 installed:

    pwsh tests/run.ps1

The script first builds the mod itself with Windhawk's compiler and
`-Wall -Wextra`, then builds and runs the tests at `-O2` and at `-O0`. It exits
non-zero if anything fails to build or any test fails.
