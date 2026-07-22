# Research: Geometry artifacts (Pa1_TheDream) — Report A4_B

Scope: BSP2 PVS/culling, clip planes, face-culling winding, "dancing polygons" pattern.
Ground truth cross-checked against `RndrGL_dll_decomp.c` (retail Win32 GL renderer).

### 2. BSP2 PVS / cluster visibility — is culling correct?

**BSP2 builds the visible set entirely on the CPU; backend occlusion queries play no role in it.**
- OnRender2 seeds traversal with `SceneGraph_PVSLock(0, ...)` (WBSP2Model.cpp:5301) and recurses `Portal_AddNode` (WBSP2Portal.cpp:523), which walks portal leaves, intersects portal polygons with CPU-side `CRC_ClipVolume`s ("RPortals", WBSP2Model.cpp:5124-5131) and fills `m_liVisPortalLeaves`. Rendering then just loops that list (`RenderPortalLeaf`, WBSP2Model.cpp:4479-4513, 4117). Per-leaf scissor rects are also CPU-computed (WBSP2Model.cpp:4385-4433).
- No occlusion-query call exists anywhere in the BSP2 path (grep `OcclusionQuery_` over Source/P5: only engine post-process, flares, PS3GCM backend).

**What occlusion queries are actually used for:**
- Dynamic-exposure histogram: XREngine.cpp:4537-4596 (`CXR_PreRenderData_Histogram*`, XRVBUtil.h:83-114). `m_bDynamicExposure` defaults to true (XREngine.cpp:41), so this runs even with our caps. Our backend has **no** OcclusionQuery override (zero hits in RenderContexts/GLES3/), so the CRC_Core stubs apply: Begin/End no-op, `GetVisiblePixelCount` returns 0 (MRender.cpp:6092-6106). Consequence: exposure adaptation sees "0 visible pixels" → wrong/flickering brightness, **not** geometry culling. Cannot produce dancing polygons.
- Flare fading (XRUtil.cpp:1186, 1296): early-outs when neither READDEPTH nor OCCLUSIONQUERY cap is set; our caps (MDisplaySDL2.cpp:1170-1173) exclude both → harmless.

**Clip_* methods:** base `CRC_Core` owns a CPU clip stack (MRCCore.h:408-413, 881-885; MRender.cpp:3989-4030) and software-clips only immediate-mode polygons (`CRC_FLAGS_CLIP` paths, MRender.cpp:5763-5766, 5842-5844). For VB rendering the backend must implement clipping:
- PS3GCM: vertex-program clip planes — `VPFormat.SetClipPlanes(true)` + plane registers in model space (MRenderPS3_Attrib.cpp:901-904, 962-977).
- Retail RndrGL: `glClipPlane` + `glEnable(GL_CLIP_PLANEn)` in `FUN_1006d900` (RndrGL_dll_decomp.c:77418, gated on caps bit 0x4000 at ~77397) → maps to `CRCPS3GCM::GeometryContext_Update` (MRenderPS3_Attrib.cpp); no GLES3 twin.
- Our GLES3: nothing — no `Clip_` override, no `CRC_FLAGS_CLIP` handling (zero grep hits in RenderContexts/GLES3/).

Engine-side use: `CXR_VBManager::Clip_Push` (XRVBManager.cpp:3947) is invoked only around **texture-portal / see-through-portal** view rendering (XREngine.cpp:2884, 2931), and re-applied per-VB via `Clip_Set` during VB flush (XRVBManager.cpp:3586-3594). Main-view BSP2 rendering never enables it.

### 5. Face culling winding — CW vs CCW

**Our mapping is exactly inverted vs both retail backends.**
- Ours (MDisplaySDL2.cpp:1410-1416): `glFrontFace(CULLCW ? GL_CW : GL_CCW); glCullFace(GL_BACK);` → CULLCW keeps CW-wound, default keeps CCW-wound.
- PS3GCM: front face fixed CCW (MRenderPS3_Context.cpp:161); `CULLCW → gcmSetCullFace(BACK)`, else `FRONT` (MRenderPS3_Attrib.cpp:288-294) → CULLCW keeps CCW-wound, default keeps CW-wound.
- Retail RndrGL confirms PS3GCM: `glFrontFace(GL_CCW)` set once at init (RndrGL_dll_decomp.c:38438, inside `FUN_10032930` ≈ device Create); attrib apply `FUN_1006e0a0` (≈ `CRC_Core::Attrib_Set` equivalent): flag 0x1000 (`CRC_FLAGS_CULLCW`, MRender_Classes.h:382) → `glCullFace(GL_BACK=0x405)`, else `glCullFace(GL_FRONT=0x404)` (RndrGL_dll_decomp.c:77653-77666).

No compensating mirror exists in our path: the vertex shader passes `gl_Position.y` through unchanged (MDisplaySDL2.cpp:56) and the composite blit is 1:1 (MDisplaySDL2.cpp:199-204), so FBO window-space winding equals retail backbuffer winding. Engine side, `CULLCW` is cleared for normal views and set only for mirrored VCs (XREngine.h:417-422, XRUtilRS.cpp:1091-1094, XREngine.cpp:2655). Surfaces without `XW_SURFFLAGS_NOCULL` render with `CRC_FLAGS_CULL` enabled (XRUtilRS.cpp:493-496, 506-509) — i.e. most world geometry is HW-culled, so the inversion is fully visible. Cull handling is consistent between immediate and VB paths (single `ApplyAttribs`, MDisplaySDL2.cpp:1372, reified at draw time :2043). Symptom match: inverted culling shows backfaces of walls/props behind the visible shell — view-dependent extra triangles that can fill the screen. Matches.

### 7. The "polygons dance on the sides" pattern specifically

- **Inverted face culling — LIKELY.** Evidence above (§5). Explains extra triangles appearing/disappearing with minimal camera motion and full-screen occlusion events. Test patch: MDisplaySDL2.cpp:1414 → `glFrontFace(GL_CCW); glCullFace((F & CRC_FLAGS_CULLCW) ? GL_BACK : GL_FRONT);` (or one-line probe: swap `GL_CW/GL_CCW`).
- **Missing portal clip planes in VB path — POSSIBLE (secondary).** Engine clips portal/mirror RTT views to the portal polygon via the clip stack (XREngine.cpp:2884, 2931; XRVBManager.cpp:3586-3594); both retail backends implement it (glClipPlane / VP clip); we ignore it. Effect is confined to portal/mirror textures (unclipped geometry inside the mirror image), not the main framebuffer — cannot alone explain main-view dancing. Test: add `gl_ClipDistance`/discard support, or temporarily skip `XR_SCENETYPE_TEXTUREPORTAL` views (XREngine.cpp:2856) to see if artifacts change.
- **Mirror/reflection pass leaking into main framebuffer — UNLIKELY.** Mirror faces are only queued via `Render_AddMirror` (WBSP2Model.cpp:567) and drawn through the texture-portal RTT path (XREngine.cpp:2856-2894); the main view only samples the result in `RenderPortalSurface` (WBSP2Model.cpp:418). Our RTT FBO switching exists (`[GLES3-RTT]`). Not directly re-verified this pass, but no code path renders a mirror view to the default target.
- **Stencil shadow-caster geometry leaking into color — RULED OUT** (already excluded via `RIDDICK_SKIP_SHADOWVOL`; also BSP2 SV passes disable color write, WBSP2Model.cpp:103-211).
- **Z-prepass vs `CRC_COMPARE_EQUAL` multipass — RULED OUT as a source of *extra* polys.** Fog/light passes reuse identical transforms with EQUAL (WBSP2Model.cpp:1765, 1930, 3188); with the in-shader NDC.z remap (MDisplaySDL2.cpp:63) applied uniformly, EQUAL is self-consistent. A failure would remove shading, not add triangles.
- **Scissor from light occlusion — UNLIKELY.** Our scissor Y-flip (`H - MaxY`, MDisplaySDL2.cpp:1437) matches retail (`glScissor(x, H-h-y, w, h)`, RndrGL_dll_decomp.c:72184). Scissor can only remove pixels.
- **Exposure histogram reading 0 (stubbed occlusion queries) — side effect only.** Brightness/exposure flicker possible (XREngine.cpp:4537-4596 + MRender.cpp:6102-6106), unrelated to geometry. Test: `m_bDynamicExposure=false` if brightness pumping is observed.

**Top recommendation:** fix the `glFrontFace`/`glCullFace` mapping at MDisplaySDL2.cpp:1410-1416 first; it is the only finding that contradicts both retail backends and directly produces view-dependent extra geometry.
