# VoxelSurfing - [Download Windows Demo Zip File](https://github.com/LukeSchoen/VoxelSurfing/archive/refs/heads/main.zip)
High-Resolution Real-Time Voxel Rendering on a Single CPU Thread.
Inspired by Ken Silverman's Voxlap, rebuilt in a minimal form from first principles.

Excellent quality and performance is achived by combining fast Column based occlusion with exact 3D projection.
This repo is for people who enjoy the "how the heck does that render so fast?" feeling.

I never fully grokked Voxlap's code, but I read enough of Ken's forum posts to replicate many of his core ideas.

**What this is**
- A minimal voxel-heightmap renderer: each (x, y) map cell stores a height and a color.
- A fast column renderer: each screen column casts a 2D ray across the map and fills vertical spans.
- A "show your work" codebase: one `main.c`, no dependencies beyond basic Win32 built in libraries.

**What this is not**
- Full Voxlap engine. Voxlap supported full 6 dof camera rotation + multiple height layers per (x, y).
- It's also not a full voxel game engine. This has been deliberately constrained for clarity and speed.

---

**Running The DEMO**
1. Run `voxelsurf.exe`
2. Move, fly (W,A,S,D, + E, X), swap the map with the number keys (1-4).

**Building The SRC**
1. Copy in 'Clang.exe'
2. run `build.bat`

**Requirements For Building**
- Windows (uses basic Win32 windowing + WIC image loader).
- `clang.exe` put in the repo root (copy not included here).

---

**Controls**
- `Mouse` yaw (left/right look)
- `W`, `A`, `S`, `D` move on the ground plane
- `Space` or `E` rise
- `Ctrl` or `X` descend
- `1` `2` `3` `4` load maps: `Ice`, `Hills`, `Forest`, `Temple`
- `Esc` quit

---

**Render Pipeline Explaination (Short Version)**
1. Each screen column casts a ray across the 2D heightmap using DDA grid stepping.
2. As the ray advances, it tracks the highest current screen pixel filled so far.
3. When a terrain sample projects higher than the horizon, that vertical span gets filled.

Comanche-Voxel-landscape-terrain-trick but taken to the extreme!:
An integrated screen-sweeping algorithm provides fast occlusion and rendering.
Ken Silverman once described his engine as 'Comanche but with exact boundaries'.
I replicated that effect here by carefully maintaining 3D-projection-evivalence.

---

**Interesting Tidbits and Tricks**
- The ray marcher is just a 2D DDA (like old-school grid raycasting), but it finds vertical spans.
- `slopeThreshold` lets the renderer compare heights in world space without slow re-projecting.
- `highestSeenY` is the occlusion system. Once a pixel row is filled - it's never drawn again.
- Everything is single-threaded CPU-only. No GPU, no hacks, no magic (well maybe some magic)

---

**Why Voxlap Matters**
Ken Silverman's Voxlap engine did everything this code does and a lot more:
- He Also Added Full camera rotation. (My version of that is close, needs a bit more work)
- Multiple height layers per (x, y) cell. (This one is going to require a serious rewrite)

Kens coding style is intense meaning his advanced software projects are largely unreadable.
This project is meant as a readable, minimal bridge to get into those ideas.

---

**Ideas To Add**
- pitch (look up/down) by shifting the projections center per column.
- Accelerate rays using a directional jump map or other structure.
- Add a multi height layers for arbitrary 3D models, caves etc.

---

**Notes and Known Limitations**
- No pitch/roll yet. It's yaw-only.
- Single height layer per (x, y).
- Windows-only code path (Win32 + WIC).

**Colab / Future**
Feel free to fork / explore! tho! if you do speed it up! plz send your optimisations :D
