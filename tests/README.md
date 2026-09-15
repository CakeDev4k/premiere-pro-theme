# Tests

`harness.cpp` holds logic tests for the mod, compiled together with it and with
the Windhawk API stubbed out. They cover:

- the color table and its generations;
- the content scope;
- the palette highlight and its luminance matching;
- the UXP stylesheet rewrite, and the file redirect on real temporary files;
- the custom theme fields, checked against the defaults the settings block
  itself ships, and the theme the mod writes out for sharing;
- every built-in palette against the rules the readme states: a rising ramp,
  text and accent that carry, and a highlight white can sit on;
- that the palette list in the settings block, the palette tables in the mod's
  readme, and the palettes in the code are all the same set;
- the module ranges, including a dva module that unloads;
- which windows the frame work is spent on: top level, and with a frame;
- the monitor band: the DisplaySurface range, which colors are the band's —
  held against every palette's own conversion of Premiere's gray, not just the
  neutral ones — what counts as a full-surface draw, that only the exact float4
  is taken for a color, that a recording's state does not outlive it, that a
  partial install of the layer's hooks stops it acting at all, that the recolor
  does not depend on the order DisplaySurface records in, and that recoloring
  rewrites the band's own color while leaving the black behind the picture
  untouched;
- the menu theme bookkeeping;
- how the renamed dvaui functions are counted.

Run them from the repository root, with Windhawk and PowerShell 7 installed:

    pwsh tests/run.ps1

The script first builds the mod itself with Windhawk's compiler and
`-Wall -Wextra`, then builds and runs the tests at `-O2` and at `-O0`. It exits
non-zero if anything fails to build or any test fails.
