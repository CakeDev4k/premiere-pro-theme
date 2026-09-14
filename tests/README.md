# Tests

`harness.cpp` holds logic tests for the mod, compiled together with it and with
the Windhawk API stubbed out. They cover:

- the color table and its generations;
- the content scope;
- the palette highlight and its luminance matching;
- the UXP stylesheet rewrite, and the file redirect on real temporary files;
- the custom theme JSON reader, including malformed and oversized input;
- the menu theme bookkeeping;
- how the renamed dvaui functions are counted.

Run them from the repository root, with Windhawk and PowerShell 7 installed:

    pwsh tests/run.ps1

The script first builds the mod itself with Windhawk's compiler and
`-Wall -Wextra`, then builds and runs the tests at `-O2` and at `-O0`. It exits
non-zero if anything fails to build or any test fails.
