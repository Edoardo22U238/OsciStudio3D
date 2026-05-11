# OsciStudio3D

Convert 3D models into stereo XY audio for real oscilloscopes and CRT displays.  
Built with **C++17**, **JUCE**, **Assimp**, **CMake**.

---

## Build locally (Windows + Visual Studio 2022)

### Prerequisites

| Tool | Version |
|------|---------|
| Visual Studio 2022 | with "Desktop development with C++" workload |
| CMake | ≥ 3.22 — [cmake.org](https://cmake.org/download/) |
| Git | any recent version |

> **No other manual downloads needed.**  
> JUCE and Assimp are fetched automatically by CMake via `FetchContent`.

---
test build

### Steps

```bat
REM 1. Clone the repo
git clone https://github.com/your-name/OsciStudio3D.git
cd OsciStudio3D

REM 2. Configure (downloads JUCE ~700 MB and Assimp ~150 MB on first run)
cmake -B build -G "Visual Studio 17 2022" -A x64

REM 3. Build Release
cmake --build build --config Release --parallel

REM 4. Run
build\OsciStudio3D_artefacts\Release\OsciStudio3D.exe
```

First configure takes **5–15 min** depending on internet speed.  
Subsequent builds are fast (incremental compilation).

---

## Using the app

1. **Open a 3D file** — drag & drop STL/OBJ/PLY/GLB onto the window, or click **Open File**.
2. **Press Play** — the model is converted to XY audio and looped.
3. **Connect audio output** — Left channel = X axis, Right channel = Y axis.
4. **Set oscilloscope to XY mode.**
5. Tune controls in the right panel.

### Oscilloscope connection

```
Sound card L → CH1 (X axis)
Sound card R → CH2 (Y axis)
Oscilloscope → XY mode
```

Click **Audio Settings** to choose the correct output device (e.g. your DAC).

---

## Control reference

| Control | Effect |
|---------|--------|
| **Rot X/Y/Z** | Manual rotation of the model |
| **Auto X/Y/Z** | Continuous auto-spin on each axis |
| **Rot Speed** | Multiplier for auto-spin |
| **Scale** | Overall model size |
| **Offset X/Y** | Shift the image on the display |
| **Detail** | Max number of wireframe edges (200–80 000) |
| **Smooth** | Chaikin corner-cutting iterations (0–5) |
| **LPF** | Low-pass filter alpha — 1.0 = off, 0.01 = maximum smooth |
| **Target Hz** | Refresh rate of the oscilloscope beam (2–120 Hz) |
| **Gain X / Gain Y** | Per-channel gain for X and Y |
| **Master** | Overall output level |
| **Phase** | Y channel sample offset — creates Lissajous-style effects |
| **Decay** | Phosphor trail decay (0.5 = fast, 0.999 = very slow) |
| **Glow** | Gaussian blur glow strength |
| **Brightness** | Final image brightness |
| **Beam R/G/B** | Phosphor colour |
| **Artistic Mode** | Enables path optimisation to minimise jumps |
| **Export WAV** | Save 10-second stereo WAV for later use |

---

## Performance tuning

### Geometry

- **Detail ≤ 5000** for real-time rotation (greedy path reorder runs in <100 ms).
- **Detail > 5000**: path reorder is skipped automatically for speed; edges are projected as-is.
- **Smooth = 2** doubles the point count each iteration — use sparingly with very high Detail.

### Audio

- **Target Hz = 30** → 1600 samples/frame at 48 kHz — good balance.
- **Target Hz = 10** → 4800 samples — more detail per sweep, image may flicker on non-persistent scopes.
- **Target Hz = 60** → 800 samples — very smooth motion, less detail.
- The audio callback is allocation-free: it reads from a pre-built float array.

### LPF & Artistic mode

- **Artistic Mode ON** + **Smooth ≥ 2** + **LPF 0.3–0.8**: produces the smoothest, most CRT-friendly output.
- **Artistic Mode OFF**: raw geometric wireframe, sharp corners, better for technical displays.

### Large meshes

For meshes with > 100 000 triangles:
- Set **Detail** to 3000–5000 (the processor samples edges uniformly).
- Assimp's `JoinIdenticalVertices` pass is applied automatically.

---

## Project structure

```
OsciStudio3D/
├── CMakeLists.txt
├── Source/
│   ├── Main.cpp              — JUCE app entry point
│   ├── MainComponent.h/.cpp  — UI layout, control logic, audio app component
│   ├── AudioEngine.h/.cpp    — Lock-free double-buffered audio output
│   ├── GeometryProcessor.h/.cpp — Assimp loading, edge extraction, path building
│   ├── ScopeRenderer.h/.cpp  — OpenGL phosphor-persistence renderer
│   └── WavExporter.h/.cpp    — Stereo WAV file writer
└── .github/workflows/
    └── windows-build.yml     — CI: builds EXE on every push
```

---

## CI / GitHub Actions

Every push to `main` triggers `.github/workflows/windows-build.yml`:

1. Checks out the repo
2. Restores CMake dep cache (`build/_deps`)
3. Configures with VS 2022
4. Builds Release
5. Uploads `OsciStudio3D.exe` as a workflow artifact (30-day retention)

Download the EXE from **Actions → latest run → Artifacts**.

---

## Dependencies (all fetched automatically)

| Library | Version | License |
|---------|---------|---------|
| JUCE | 7.0.12 | GPLv3 / Commercial |
| Assimp | 5.3.1 | BSD 3-Clause |

> **JUCE licence**: for personal/open-source use, JUCE is free under GPLv3.  
> Commercial products require a JUCE licence from [juce.com](https://juce.com).
