# Research Task: "Dancing polygons" in P5 GLES3 backend port

## Context

Repo `/home/user/openriddick`. Branch `claude/gles3-sdl2-fbo-crash-2w2izj`.
Starbreeze P5 engine (Chronicles of Riddick) ported PS3 → Linux/SDL2/GLES3.
Our new backend: `Source/P5/Shared/MOS/RenderContexts/GLES3/MDisplaySDL2.cpp`
(class `CRC_GLES3 : CRC_Core`).

## Symptom (visual)

In Pa1_TheDream:
- BSP2 world level geometry is loaded correctly (verified via
  `RIDDICK_DUMP_BSP=<dir>` — opens fine in Blender).
- BSP2 world renders through the GL pipeline (visible dark corridor,
  Lambert lighting active). But it flickers heavily.
- "Polygons dance on the sides" — extra triangles appear/disappear as
  the camera moves even a tiny bit; can occlude the whole view.
- Not skinned characters (RIDDICK_SKIP_SKINNED tested, unchanged).
- Not stencil shadow volumes (RIDDICK_SKIP_SHADOWVOL tested, unchanged).
- Not additive sprites (RIDDICK_SKIP_ADDITIVE tested, killed menu but
  main artifacts persist).

## What has already been ruled out (do not re-investigate)

- BSP2 loader is verified correct (raw vertex+face dump matches map).
- Menu-cube RTT feedback loop (separate white-cube issue).
- Vertex data unpack: `CRC_VRegTransform` (scale+offset) fix applied
  and OBJ dumps from render path look sane after that.
- Test triangle at identity MVP renders correctly (pipeline is fine).
- Basic MVP arithmetic works for menu.
- **The engine's projection matrix maps NDC.z into [0..1] (D3D
  convention); GL wants [-1..+1]. Just fixed with an in-shader remap
  `gl_Position.z = 2 * z - w`. This should have addressed Z-fighting
  from compressed depth range.** The research below is for what
  remains after that fix.

## Files to focus on

- `Source/P5/Shared/MOS/RenderContexts/GLES3/MDisplaySDL2.cpp` — our backend.
- `Source/P5/Shared/MOS/RenderContexts/PS3GCM/` — reference backend, complete,
  with real symbol names. This is our ground truth for "how a real backend
  should implement each virtual".
- Ghidra decomps in repo root — **PRIMARY GROUND TRUTH**, do not skip:
  - `RndrGL_dll_decomp.c` — retail Windows OpenGL renderer. This is the
    only complete GL backend we have. Every hypothesis you form should be
    cross-checked against what RndrGL_dll_decomp.c actually does at the
    equivalent call site (glDepthRange, glClearDepth, glFrontFace,
    glClipControl, glDepthFunc, glStencilFunc, glDrawElements setup, etc).
  - `MXR_dll_decomp.c`, `MSystem_dll_decomp.c` — engine side.

**Convention for the report:** whenever you find a relevant retail function,
map it to the current source. `FUN_10068200 (RndrGL:71400) → CRC_Core::Attrib_Set
(MRender.cpp:XXXX)`. Even partial mappings help — they anchor future work.
When a decomp function has no source-side twin, note that explicitly ("no
mapping found — inspect this by hand"). Do not treat source alone as ground
truth; retail may have quiet fixes the source snapshot pre-dates.

## Questions to answer

Structure the report under exactly these headings. Under 900 words total.
Cite `file:line` for every finding.

### 1. Depth-buffer setup — does GLES3 backend match engine expectations?

- What depth format does the engine expect? Search PS3GCM backend for
  glCreateRenderbufferStorage / cellGcmSetDepthTest / m_ZBufferFormat.
- What did we allocate? `EnsureFBOFor` in our backend uses
  `GL_DEPTH24_STENCIL8`. Is 24-bit enough? What does PS3 use?
- Does the engine call glDepthRange somewhere we don't respect?
- Is `glClearDepthf` called with the right value (near vs far)? Our
  `RenderTarget_Clear` calls `glClearDepthf(_ZBufferValue)` — check
  what the engine passes.

### 2. BSP2 PVS / cluster visibility — is culling correct?

- Read `Source/P5/Shared/MOS/XRModels/Model_BSP2/WBSP2Model.cpp`
  around `PortalLeaf`/`PVS`/`Render_r`. Does BSP2 build the visible
  cluster list itself, or does it rely on backend to help via
  Occlusion queries? (We stub occlusion queries — could that break
  frustum culling downstream?)
- Do we correctly implement `Clip_*` methods? What does base CRC_Core
  provide, and does PS3GCM override any Clip method?

### 3. Multi-target / RTT rendering — do we mix framebuffers?

Our backend allocates one FBO per RTT texture ID
(`Source/P5/Shared/MOS/RenderContexts/GLES3/MDisplaySDL2.cpp` around
line 416, `EnsureFBOFor`). Grep for how many `RenderTarget_SetRenderTarget`
calls happen per frame in a Pa1_TheDream trace (see the earlier
run.log in `/root/.claude/uploads/1666e2f6-*` — 8+ RTT textures created).

- Does the engine assume a specific size for each RTT that we're not
  respecting (e.g. downsampled envmap 512x512 vs 1280x720)?
- Do we need to re-bind default backbuffer FBO between RTT passes?
  Look at PS3GCM to see when it flips render target.
- Could a stale FBO binding leak drawcalls between passes?

### 4. Viewport / scissor state machine

- Our `Viewport_Update` scales projection matrix columns 0/1 by
  `2/W`, `2/H`. Check PS3GCM `Viewport_Update` — does it do the same,
  or something different? Are we losing X/Y precision on multi-viewport
  frames?
- Does the engine push viewport per pass (portals, mirrors)? If yes,
  do we get `Viewport_Push`/`Viewport_Pop`? We have no override for
  these — base CRC_Core saves state, but do we pull it out at the
  right point?

### 5. Face culling winding — CW vs CCW

Engine flag `CRC_FLAGS_CULLCW` toggles winding. Our impl:
`glFrontFace((F & CRC_FLAGS_CULLCW) ? GL_CW : GL_CCW)`. Is this the
right mapping? Check PS3GCM `Attrib_Set` — how does it map CULLCW to
GCM winding? Off-by-one on winding could show up as EXTRA polygons
(back-face triangles rendered from inside a mesh).

### 6. Camera / view matrix handoff

Engine passes matrices via `Matrix_SetRender(CRC_MATRIX_MODEL, ...)`.
Only MODEL gets pushed per drawcall (per `MRender.cpp:3430`). Projection
comes via `Viewport_Update`. Are we consistent?
- Could there be a case where `m_ProjMat` is stale between draws (from a
  previous viewport that wasn't cleared)?
- Look at PS3GCM: does it store a per-draw copy of Model+Proj, or read
  the live pointer at draw time?

### 7. The "polygons dance on the sides" pattern specifically

Given the symptom is view-dependent artifacts that vary massively with
tiny camera movement, hypothesize what could cause this. Consider:
- Portal-plane clipping (near/far clipping to portal). If we don't do
  it, geometry beyond a portal cluster gets drawn.
- Mirror/reflection passes rendering into main framebuffer.
- BSP2 shadow-caster silhouette faces that we somehow render as color.

## Deliverable

`Docs/Research_GeometryArtifacts_Report.md` — under 900 words, structured
by the 7 sections above. Every claim cites `file:line`. For each
hypothesis, say whether it's **likely / possible / ruled out** based on
evidence, and what quick backend patch would test it.

Do not write engine code changes. Research only.
