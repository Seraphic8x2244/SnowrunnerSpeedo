# SnowrunnerSpeedo — standalone test (dev)

Experimental standalone ASI based on [rodigh/SpeedHud](https://github.com/rodigh/SpeedHud), v44 source. The `dev` branch now draws a basic **analogue needle gauge** driven by the existing v44 speed value. Native HUD movement is not implemented yet.

## Install (Epic PC)

1. Close SnowRunner.
2. Move existing `SnowLoader.asi` and `SpeedHud.asi` out of the game executable folder to avoid conflicting HUD hooks. Keep `dinput8.dll` (Ultimate ASI Loader).
3. Download the `SnowrunnerSpeedo-x64` artifact from the newest successful **Standalone ASI x64** Actions run on `dev`.
4. Copy `SnowrunnerSpeedo.asi` and `speedhud.ini` into the folder containing `SnowRunner.exe`.
5. Launch the game. The basic analogue gauge should appear with optional distance text.

## INI test

Edit `speedhud.ini` while the game is closed, then restart:

```ini
[HUD]
PosX=150
PosY=100
Scale=2.0
ShowSpeed=1
ShowDistance=0
ShowDecimalSpeed=0

[ADDRESS]
SpeedAddress=0x2A8FA2C
```

The test passes if the gauge moves to X=150/Y=100, becomes smaller, reads live vehicle speed, and **no longer shows distance**. Use **Home** to open the in-game settings menu and adjust position/scale/visibility live; the INI is not hot-reloaded. Set `ShowSpeed=0` as well to hide both HUD lines.

The Home-key menu remains available. Unrelated time-control functionality has been removed.

## Branches

- `dev`: standalone ASI development and tests.
- `SnowLoader`: archival branch of the earlier SnowLoader-based investigation.
- `stable` / `main`: unchanged.

The separate [SnowLoader fork](https://github.com/Seraphic8x2244/SnowLoader/tree/dev) continues to preserve the full SnowLoader source and its successful Windows build workflow. The `SnowLoader` branch here is **not yet a self-contained copy of that separate repository**.

No in-game test has been performed for this custom ASI.
