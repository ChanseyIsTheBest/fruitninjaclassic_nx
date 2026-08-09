# Fruit Ninja Classic+ — Nintendo Switch port (Unity 2022.3 / IL2CPP wrapper)
 
This is a native wrapper / loader that runs the original ARM64 Android build of
*Fruit Ninja Classic+* on Switch homebrew. It contains **no game code and no game
assets** — it loads the game's own libraries and recreates, natively, the thin
Android/JNI layer the Unity engine expects.
 
## Install & run
 
You need files from your own copy of Fruit Ninja Classic+ (unzip the APK).
 
Put the `.nro` in `sdmc:/switch/fruitninja/` and place your game files next to it:
 
```
sdmc:/switch/fruitninja
├── fruitninja_nx.nro
├── libmain.so  libunity.so  libil2cpp.so   <- from your APK: lib/arm64-v8a/
└── assets/                                 <- your APK's assets/
```
 
Launch via title override (hold R while starting an installed game) or a
forwarder.
 
## Controls
 
Fruit Ninja wants two fingers, so the controller gives you **two cursors** —
blue on the left stick, red on the right.
 
| Input | Action |
|---|---|
| Touchscreen | Direct multi-touch — the native fit for this game |
| Left stick / right stick | Move the blue / red cursor |
| ZL / ZR | Slice with the blue / red cursor |
| + | Toggle both cursors on/off |
| – | Toggle gyro pointing (tilt to aim) |
| L / R | Recenter the blue / red cursor |
| D-pad up / down | Adjust sensitivity of whatever is driving the cursors |
 
Hiding the cursors also stops them slicing, so `+` gets them out of the way when
you are playing on the touchscreen. Stick and gyro sensitivity are remembered in
`pointer.cfg` after you adjust them in game. There is no USB mouse support.
 
## Settings
 
`config.txt` is written next to the `.nro` on first launch, with the options
documented inline:
 
```
handheld_res 720     # 720 or 1080
docked_res   1080    # 720 or 1080
```
 
## Building
 
Requires devkitPro with the `switch-dev` group plus these portlibs:
 
```
pacman -S switch-dev
pacman -S switch-mesa switch-libdrm_nouveau switch-sdl2 switch-libpng switch-zlib
 
export DEVKITPRO=/opt/devkitpro
make                        # -> fruitninja_nx.nro
```
 
`source/` is a single flat tree, edited directly — no overlay, no patch step.
Two checks are worth running from a plain Linux host:
 
```
python3 tools/buildcheck.py source
python3 tools/verify_offsets.py libil2cpp.so libunityfruitninja.so [script.json]
```
 
The first catches implicit declarations and unreachable macros before devkitA64
does. The second re-derives every offset the loader patches against your actual
binaries — run it if the game ever updates, because a stale offset patches the
wrong instruction and fails somewhere unrelated.
 
## Notes
 
`debug.log` is written next to the `.nro`. If something goes wrong it is the
first thing to read; `HANDOFF.md` lists the lines worth grepping for and what
they mean, and `AUDIT.md` has the full round-by-round history.
 
## Credits
 
The loader/shim infrastructure (`so_util`, `libc_shim`, `jni_fake`, `unity_jni`,
`opensles`, the pointer module, diagnostics) derives from the open-source Switch
`.so`-loader lineage — Andy Nguyen, fgsfds and ChanseyIsTheBest, building on
TheOfficialFloW's Vita/Switch loader tradition — reaching this project via the
Zookeeper DX and PvZ Fusion ports, with the Animal Crossing: Pocket Camp port as
the reference for the Boehm GC stop-the-world bridge. All MIT-licensed. Thanks to
everyone in that lineage for making this approach possible.
