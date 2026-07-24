# Research Task: P5 dynamic-lighting render pipeline

## Context

Repo `/home/user/openriddick` — Starbreeze P5 engine (Chronicles of Riddick:
EFBB / Dark Athena). PS3 snapshot in `Source/P5/`. Being ported to
Linux/SDL2/GLES3 on branch `claude/gles3-sdl2-fbo-crash-2w2izj`. Our new
backend lives in `Source/P5/Shared/MOS/RenderContexts/GLES3/MDisplaySDL2.cpp`
(class `CRC_GLES3 : CRC_Core`) plus a few helper files in the same directory.

**Symptom:** in Pa1_TheDream we see actual level geometry but the scene is
almost entirely black. Suspicion: engine issues a base pass + N additive
light passes; our backend collapses everything into one fallback
`vCol*tex0*tex1` shader that ignores per-light state, so all light passes
contribute nothing → scene stays at ambient/AO which is near-black.

The user has stated the game does NOT use lightmaps — lighting is fully
dynamic + AO. So our fallback shader is definitely missing per-light math.

## Ground rules

- **Do not write engine code.** This is a research task; produce a report,
  not a patch.
- **Cite `file:line` for every finding.** No claims without a source.
- **If you cannot find something concrete, say so.** Do not invent.
- Stay within `/home/user/openriddick`.
- Ignore the menu-cube "white texture" problem — that is unrelated and
  under separate investigation.

## Data available

### Sources (editable)
- `Source/P5/Shared/MOS/` — engine core (MSystem/XR/Classes/RenderContexts).
- `Source/P5/Shared/MOS/MSystem/Raster/MRender.h` — `CRenderContext` interface.
- `Source/P5/Shared/MOS/MSystem/Raster/MRCCore.h` — `CRC_Core` intermediate class.
- `Source/P5/Shared/MOS/MSystem/Raster/MRender_Classes.h` — `CRC_Attributes`,
  `CRC_VertexBuffer`, `CRC_BuildVertexBuffer`, `CRC_VRegTransform`, `CRC_Light`.
- `Source/P5/Shared/MOS/XR/XRShader*` — shader / material layer.
- `Source/P5/Shared/MOS/XR/XREngine*` — scene / draw driver.
- `Source/P5/Shared/MOS/RenderContexts/PS3GCM/` — the ONLY complete concrete
  backend in the tree (~19k lines). This is your primary reference for
  "what does a real backend do per drawcall".
- `Source/P5/Shared/MOS/RenderContexts/GLES3/` — our port in progress.

### Ghidra decompilations (read-only, ~50MB total)
- `RndrGL_dll_decomp.c` — retail Windows OpenGL renderer DLL.
- `MSystem_dll_decomp.c` — retail Windows MSystem.
- `MXR_dll_decomp.c` — retail Windows XR module.
- `GameWorld_Win32_x86_dll_decomp.c`
- `GameClasses_Win32_x86_dll_decomp.c`

Decomp caveat: **no symbol names**. Functions are `FUN_1006xxxx`, fields
are `*(int *)(this + 0x4e50)`. Cross-reference with the source headers to
figure out what each offset means.

## Questions to answer

Structure the answer under these exact headings. **Keep the whole document
under 800 words.**

### 1. Where the multi-pass loop lives

Which class + method in `Source/P5/Shared/MOS/XR/` iterates lights and
submits N additive passes per drawable? Search terms to try:
`SubmitLight`, `RenderLight`, `AddLightPass`, `PerLight`, `_iLight`,
`Light_Render`, `Attrib_Lights`, `XR_SHADERFLAGS`, `XRShaderStage`,
`ShaderPass`, `nPass`.

For the found method:
- Signature + `file:line`.
- What does one iteration do? What API on `CRenderContext` does it call
  per pass — same `Render_IndexedTriangles`, or a different entry point?
- What state is different between the base pass and a light pass? (Blend
  mode, texture bindings, alpha test, stencil, matrix?)

### 2. `CRC_Light` and `Attrib_Lights`

- `CRC_Light` struct definition and `file:line`. What fields describe a
  light (position, color, range, direction, cone, type flags)?
- Where is `CRC_Core::Attrib_Lights(const CRC_Light*, int _nLights)`
  called from? How many lights per drawable in typical use — 1? N?
- Which flag in `CRC_Attributes::m_Flags` says "this pass consumes lights"
  vs "this pass is ambient/base"? Check `CRC_FLAGS_LIGHTING` in
  `MRender_Classes.h`.

### 3. What the PS3GCM backend does per light pass

The most useful ground truth is the PS3 backend, because it is complete
and has real symbol names. In `RenderContexts/PS3GCM/`:
- Where in the PS3 backend is `Attrib_Lights` overridden?
- Where is per-light state pushed into the GPU program (`_iLight`,
  `SetLight`, `LightPos`, `LightRange`, `Cell_SetLight`)?
- Which shader program object handles a light pass? What are its inputs
  (uniforms/registers)? Does the backend generate a program per light
  count, or use a loop inside one program?

### 4. Retail OpenGL DLL: multi-pass equivalent

In `RndrGL_dll_decomp.c`:
- Find `glBlendFunc` calls with `GL_ONE, GL_ONE` (or 0x1, 0x1) — that is
  the additive light pass. Line numbers, please.
- Nearby: what ARB fragment program / vertex program is bound? Any
  `wglGetProcAddress("glProgramLocalParameter4fvARB")` calls set light
  parameters?
- Roughly, what is the sequence per drawable when >0 lights are present?

### 5. Shadow volumes

- Are stencil shadow volumes used in Pa1_TheDream? Check `XRShader*` and
  `XREngine*` for `ShadowVolume`, `SVGen`, `SVRender`, `StencilShadow`.
- How is their geometry generated (from mesh silhouettes)?
- What stencil ops are used (front/back incr/decr)?
- Do they run BEFORE or AFTER each light pass?

### 6. Gaps in our GLES3 backend

`Source/P5/Shared/MOS/RenderContexts/GLES3/MDisplaySDL2.cpp` currently
does not implement:
- `Attrib_Lights` override — confirm this is absent (grep it).
- Per-light shader — we have exactly one UI-style shader in
  `kGLES3_UIFragSrc` (~line 55). No light math.
- Any handling of `CRC_FLAGS_LIGHTING`.
- `Fog_*` besides basic linear fog uniforms.
- Anything with normals — `SUIVert` has no normal component (`x,y,z,u,v,u1,v1,col`).

Confirm each of these by grep (`file:line` or "not found"). Also list any
other Render_*/Attrib_*/Geometry_* virtuals from `MRender.h` that we do
not override AND that PS3GCM does override — those are the real gaps.

## Deliverable

A markdown file `Docs/Research_LightPass_Report.md` with the six sections
above, each with `file:line` cites and short bullets. Under 800 words.

The report will be used to design a minimal Lambert light-pass in the
GLES3 backend — so bias the depth of investigation toward "how does the
engine hand light data to the backend and how does the backend consume
it", not "how is scene lighting computed globally".
