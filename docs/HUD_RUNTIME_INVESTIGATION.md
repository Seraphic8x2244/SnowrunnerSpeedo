# Left HUD runtime investigation

## Objective

Determine whether a SnowLoader-compatible runtime plugin can access the live transform of SnowRunner's bottom-left fuel HUD panel. Do not implement speedometer redesign or extra features before this is established.

## Verified from upstream source

- `maxpowa/SnowLoader/scripts/speed/script.cpp` renders its analogue gauge inside an independent ImGui window (`ImGui::Begin("##speed_hud_gauge", ...)`). It does not parent the gauge to a game HUD object.
- `OnTick` uses the ImGui display size and optionally saved window coordinates to place its overlay.
- `GetCurrentSpeed` reads an address through SnowLoader's game memory service. Its hardcoded offset must **not** be assumed valid for the current Epic build.
- SnowLoader SDK documents pattern scanning, memory reads and hooks, but no documented Scaleform HUD-object interface.
- The upstream repository has no explicit reuse licence (the README's 'as-is for educational and modding purposes' is not a clear permission grant to redistribute copied code). Seek clarification before copying implementation files.

Source: https://github.com/maxpowa/SnowLoader/blob/main/scripts/speed/script.cpp

## Asset evidence (previous investigation; runtime link unverified)

Extracted `hud_lib.gfx` names include `leftHudPanelContainer`, `leftHudPanelCont`, and motion identifiers. These identify content in the asset, **not** a known in-memory instance address or native binding API.

## First required experiment

1. Locate a live Scaleform movie/HUD instance (or equivalent game UI runtime) on the user's Epic executable build, using a non-destructive read-only diagnostic.
2. Resolve the left panel object and its screen transform; log only presence, transform values and status.
3. If available, render a small marker at the corresponding screen position and verify it follows the panel as the truck moves and the HUD shows/hides.
4. If not available, record precise failure and investigate transform-reading hooks rather than imitating animation.

## Success criteria

A marker follows the game's actual bottom-left HUD movement in gameplay; not just a static pixel offset. No changes to `gfx.pak` or gameplay values. Only then adapt the existing gauge.

## Current status

No live HUD pointer, binding or testable binary established. Requires Windows/Epic runtime investigation; do not claim native attachment works yet.
