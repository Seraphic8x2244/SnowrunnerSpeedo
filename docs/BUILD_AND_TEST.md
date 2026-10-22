# First SnowLoader DLL probe

This is **only a loader/lifecycle test**, not HUD integration. It does not hook a Scaleform object or move the existing speedometer. Its only visible behaviour is a short SnowLoader toast on initialization.

## Build on Windows

Requires Visual Studio C++ and CMake, plus local checkout of [SnowLoader](https://github.com/maxpowa/SnowLoader).

```powershell
git clone https://github.com/maxpowa/SnowLoader.git
git clone -b dev https://github.com/Seraphic8x2244/SnowrunnerSpeedo.git
cmake -S SnowrunnerSpeedo -B SnowrunnerSpeedo/build -DSNOWLOADER_ROOT="$PWD/SnowLoader"
cmake --build SnowrunnerSpeedo/build --config Release
```

Copy the built `left_hud_probe.dll` to the installed `SnowLoader/scripts/` directory, **only after SnowLoader itself is correctly installed**. The built DLL's name is intentionally separate from SnowLoader's `speed.dll`. Check the status overlay (upstream default F7) or `SnowLoader/SnowLoader.log` for script-loading failures.

Expected result: one toast saying that the probe loaded and the transform is **not** resolved. No marker should move, and the speedometer is unaffected.

## Status and real next blocker

- Verified by reading upstream: SnowLoader's `ScriptContext` exposes game-memory service, input, game state and overlays; **no Scaleform movie or HUD object service**.
- Plugin source and CMake definitions are committed.
- **Not built or runtime-tested** in the Epic executable.
- First technical requirement remains discovering a reliably identifiable live HUD instance/transform on the current game build, without guessed offsets or unsafe writes.
- Until then there is no meaningful attachment test to claim.

No `gfx.pak` modification, RPM, damping or other requested-scope additions.
