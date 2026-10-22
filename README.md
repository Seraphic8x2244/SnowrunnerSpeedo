# SnowrunnerSpeedo

An experimental analogue speedometer for SnowRunner (Epic Games PC).

## Goal

Place an analogue needle speedometer alongside the bottom-left fuel HUD and make it follow the game's native HUD positioning and animation where technically possible.

## Initial milestone

Determine whether a runtime plugin can access the live left HUD panel transform. Test with a simple diagnostic marker before adapting the speedometer.

## Scope

Speedometer only. No RPM gauge, needle damping, or unrelated enhancements in the initial milestone. Do not modify the installed game assets as part of the first experiment.

## Development branches

- `dev`: investigation and experiments.
- `stable`: only validated work.

## Upstream research

[SnowLoader](https://github.com/maxpowa/SnowLoader) provides an existing analogue speedometer implementation rendered with ImGui. Its repository currently has no declared licence; **do not copy its code** into this repository unless permission/licensing is clarified. Investigate integration approaches and document findings independently.

SnowRunner and its assets belong to their respective rights holders. Do not commit proprietary game files.
