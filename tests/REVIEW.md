# Review checklist

Not a style guide. This is the list of things the Windhawk review has actually
caught in this mod, written down so the next pass looks for them before the
reviewer does.

Twenty-five required items across sixteen reviews sort into four groups, and
two of them account for most of it. The questions below are in that order:
most-caught first.

## 1. An address, handle or value that means something else later

Ten of the twenty-five. Every one of them was a table keyed by something the
system is free to hand out again.

- For each table keyed by a pointer or handle: **what removes an entry, and can
  the key come back pointing at something else before that happens?**
  `HWND`, `HTHEME`, `HMODULE` base addresses, `ID3D12GraphicsCommandList*` and
  a stack address have all been reused here.
- Is the key stable for as long as the entry lives? A caller's stack address is
  not a key.
- If a value is cached because it was "already produced" or "already seen": can
  something else legitimately produce the same value?
- Does a module range outlive the module?

Settled in this mod: menu themes evict on `CloseThemeData`, module ranges drop
on unload, command-list slots end at `Reset`, `Close` and `ClearState`, the
produced-color guard is scoped to one paint. Themed windows prune on growth and
accept a bounded error at unload, which the comment says out loud.

## 2. What the mod leaves behind

Six of the twenty-five. Windhawk unmaps the image the moment the mod is
disabled or updated.

- Does anything the mod handed Premiere point **into the mod's image**? A
  string, a callback, a brush, a vtable, a `thread_local` with a destructor.
- Can a thread, a timer or a loader callback still run after unload?
- Is everything the mod changed reverted, and **only** what it changed?
- Does a settings change leak anything per cycle?

## 3. The contract of an API the mod calls

Eight of the twenty-five, and the ones that cost the most rounds.

- For each API called from a hook, read its **state rules**, not just its
  signature. The words to search the docs for are "stale", "must be set
  before", "undefined", "the caller owns", "do not call".
  D3D12 root constants belong to a root signature. `DrawThemeBackground` takes
  a handle that may be recycled. `LoadLibrary` under the loader lock deadlocks.
- What does this API assume about the shape of what it is given? Root parameter
  1 is not always a `float4`.
- Can the call run on a thread, or at a time, the API does not expect?

## 4. Code that disagrees with what it says it does

Six of the twenty-five, and the cheapest to catch — `tests/harness.cpp`
automates most of it now.

- Every setting gates what its description says it gates, in **every** path.
- Settings block, readme tables and code agree on names and defaults.
- A new default is the conservative one.
- The readme's numbers are the code's numbers.

## The rule the rounds were actually lost to

Look at the history: the D3D12 layer went in at `adf649e`, and the five commits
after it are all fixes to that layer, over three review rounds. A new layer
added mid-review costs about three rounds, whatever else is going right.

**So: once the review is open, fix what is there. Do not add a layer.** If
something new is worth having, it is worth a second pull request after this one
is merged.

## And for a fix

A fix is new code and gets reviewed like new code. The defect that cost round
16 was introduced by the fix for round 14.

- What can now happen that could not happen before this fix?
- Does the new path have the same preconditions as the one it joins? The replay
  added for order-independence reached `SetGraphicsRoot32BitConstants` from two
  call sites that did not know which root signature was bound.
- Read the fix cold, as if someone else wrote it, before pushing.
