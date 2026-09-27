# Not_A_Game — desktop

Desktop (Linux) port of the Not_A_Game SRPG engine testbed. Forked from
`notagame@9f7916d`; engine and game sources are kept as close to upstream
as possible so changes can flow back.

## Build & run

Dependencies (Ubuntu): `libglfw3-dev libgles-dev libegl-dev cmake ninja-build`

```sh
cmake -S . -B build -G Ninja
cmake --build build
./build/not_a_game                # assets resolved via build/assets symlink
./build/not_a_game --data ./save  # override save dir (default ~/.local/share/notagame)
cmake -S . -B build-asan -G Ninja -DNAG_SANITIZE=ON   # ASan + UBSan
```

Asserts stay on in every build type (`-UNDEBUG`) because selfTests rely on them.

## Controls

| Input | Action |
|---|---|
| Left click / drag | tap / pan (same as 1-finger touch) |
| Mouse wheel | zoom (replaces pinch) |
| Esc | quit |

## Port notes

- `src/engine/Platform.h` — seam replacing `android_app` / `AAssetManager`:
  asset reads, save dir, framebuffer size, swap, quit.
- `src/desktop/main.cpp` — GLFW window, GLES 3.0 context via EGL, input → Renderer.
- GL stays **GLES 3.0**: shaders (`#version 300 es`) and `GLES3/gl3.h` unchanged;
  Mesa provides GLES natively. Windows/macOS would need ANGLE.
- `TextureAsset` — `AImageDecoder` → `stb_image`.
- `Audio_miniaudio.cpp` — replaces Oboe; mixer (`audio::mix`) unchanged.
- `AndroidOut` — logcat → stderr (`aout` kept so call sites are untouched).
