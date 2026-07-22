# Geometry Artifacts — Research Report A1_A

Scope: depth setup, RTT/FBO mixing, viewport/scissor, matrix handoff.
Backend: `CRC_GLES3` (Source/P5/Shared/MOS/RenderContexts/GLES3/MDisplaySDL2.cpp).
Ground truth: PS3GCM backend + RndrGL_dll_decomp.c (retail Win32 GL).

### 1. Depth-buffer setup — does GLES3 backend match engine expectations?

- PS3 allocates Z24S8 (`CELL_GCM_SURFACE_Z24S8`, MRenderPS3_RenderTarget.cpp:760;
  zcull `CELL_GCM_ZCULL_Z24S8`, MDisplayPS3.cpp:616-617). We allocate
  `GL_DEPTH24_STENCIL8` — RTT FBOs (MDisplaySDL2.cpp:508), screen FBO (:615),
  SDL hints 24/8 (:294-295). **Equal.**
- RndrGL contains **no glDepthRange / glClipControl anywhere** (whole-file
  grep). PS3 viewport min=0/max=1 (MRenderPS3_Context.cpp:265-266); we never
  call glDepthRange. All three use depth range [0,1]. Engine NDC.z∈[0,1] is
  remapped in-shader (`z=2z-w`, MDisplaySDL2.cpp:63) — needed only on GL.
- Clear value: retail `glClearDepth(1.0)` at init (RndrGL:44432);
  `glClearDepth(param_1)` in FUN_10066dc0 (RndrGL:72194) → our
  `glClearDepthf(_ZBufferValue)` (MDisplaySDL2.cpp:1337), same passthrough.
  Engine passes **far=1.0f** (WFrontEndMod_Cube.cpp:6004, 6125, 6377) with
  LESS/LESSEQUAL — no reversed Z. PS3's fp32-bits-as-clear-word
  (MRenderPS3_RenderTarget.cpp:31) is an RSX quirk, semantics still 1.0=far.
- In-game depth is NOT cleared via RenderTarget_Clear: it is cleared by
  drawing a frustum quad at z≈0.99 with ZWRITE + ZCOMPARE_ALWAYS
  (`ClearViewport`, XREngine.cpp:198-295, called at :3353). Our ApplyAttribs
  handles this: with CRC_FLAGS_ZCOMPARE off we disable the test but keep the
  depth mask, so GL still writes depth (MDisplaySDL2.cpp:1379-1388).
- Divergence: `CDisplayContextSDL2::ClearFrameBuffer` is an **empty stub**
  (MDisplaySDL2.cpp:381-383) though XRApp calls it per frame with
  Z|STENCIL|COLOR (XRApp.cpp:5700-5708). PS3 implements it
  (MDisplayPS3.cpp:1116-1198). Redundant with the clear quad, but it is the
  only unconditional per-frame Z/stencil scrub.
- Retail defaults glFrontFace(GL_CW)+glCullFace(BACK) (RndrGL:38438-38439);
  we set front face per attrib (MDisplaySDL2.cpp:1414) — equivalent.

**Verdict: ruled out** (format/range/clear-value all match retail). Optional
test: implement ClearFrameBuffer as a real glClear — one line, kills the
last doubt.

### 3. Multi-target / RTT rendering — do we mix framebuffers?

- run_dream2.log after `changemap Pa1_TheDream` (line 55601, DBG_GL=1):
  **zero SetRenderTarget in-game**; ~20531 CopyToTexture over ~2100 frames
  (≈10/frame — deferred-resolve copies, XREngine.cpp:3896-3902, plus UI).
  8 RTT FBOs, all created at startup. The engine never switches FBO mid-frame
  in TheDream, so dancing polygons cannot be cross-FBO leakage.
- Our sequencing: SetRenderTarget binds slot FBO + m_bRTTActive
  (MDisplaySDL2.cpp:1194-1237); TargetID=0 → BindScreenTarget (:656).
  CopyToTexture saves/restores DRAW+READ bindings around the blit
  (:1275-1289). PresentToWindow leaves the screen FBO bound (:725). No
  stale-binding leak found.
- PS3 switches via MRT_SetRenderTarget → RestoreRenderContext, then always
  Viewport_Update (MDisplayPS3.cpp:793-803). Retail RndrGL CopyToTexture =
  FBO blit when supported (RndrGL:72919/72927; draw-buffer helper
  FUN_1002d8d0, :34361) else glCopyTexSubImage2D (:72957) → our dual path
  (:1273-1303) mirrors it. No pbuffers in retail.
- Divergence: PS3 RTT passes **share the main depth buffer**
  (`BUFFER_ATTACHED_SHARED`, MRenderPS3_RenderTarget.cpp:776); we give each
  RTT FBO a private depth RBO (MDisplaySDL2.cpp:506-508). Harmless in
  TheDream; relevant for portal/mirror levels.

**Verdict: ruled out for TheDream.** For portal levels later: share one
depth RBO across FBOs.

### 4. Viewport / scissor state machine

- Our Viewport_Update (MDisplaySDL2.cpp:1566-1593) scales projection columns
  0/1 by 2/W, 2/H — **identical** to PS3 (MRenderPS3_Context.cpp:283-286) and
  retail (RndrGL:37318-37327 + glLoadMatrixf into GL_PROJECTION). glViewport
  Y-flip uses ScreenH() (:1590); retail flips against current target height
  (RndrGL:37307-37312).
- We don't override Viewport_Push/Pop; base CRC_Core copies the stack and
  fires Viewport_Update on Pop and Set (MRender.cpp:2500-2514, 2435-2440), so
  m_ProjMat refreshes at the right points (XRApp Push/Set/Pop per frame,
  XRApp.cpp:5700-5708). Retail has no extra trick here.
- Divergence: scissor and clear-rect Y-flips always use ScreenH()
  (MDisplaySDL2.cpp:1431-1437, :1357), and SetRenderTarget's glViewport
  (:1229) is later overwritten by Viewport_Update's ScreenH-based one —
  wrong only for non-screen-sized RTT viewports. Inactive in TheDream.
- No precision delta: PS3 gcmSetViewport takes the same floats
  (MRenderPS3_Context.cpp:267-274).

**Verdict: ruled out** for full-screen frames; possible only with RTT
viewports. Test patch: cache bound-target height and use it in the three
flips.

### 6. Camera / view matrix handoff

- Model flows CRC_Core::Matrix_Update → Matrix_SetRender(CRC_MATRIX_MODEL)
  (MRender.cpp:3419-3432); projection never goes through Matrix_SetRender
  (PS3: "Projection matrix stack is not supported",
  MRenderPS3_Attrib.cpp:1117-1120). Ours caches it in Viewport_Update —
  correct channel, as the brief assumed (MRender.cpp:3376+).
- PS3 composes Model×Proj **eagerly in Matrix_SetRender** and uploads 8 VP
  constants (MRenderPS3_Attrib.cpp:1071-1105), re-marking model dirty on
  every Viewport_Update (MRenderPS3_Context.cpp:296). Retail GL:
  glLoadMatrixf(MODELVIEW) per Matrix_SetRender (RndrGL:76164-76176) with
  projection loaded at Viewport_Update — composed by GL at draw time. We
  compose m_ModelMat×m_ProjMat at draw time (SetupCommonUniforms,
  MDisplaySDL2.cpp:1826-1828) — same as retail, and fresher than PS3 (we
  recompose every draw regardless of dirty bits).
- No stale-m_ProjMat path found: Viewport_Set/Pop precede draws and re-fire
  Viewport_Update; m_MatrixChanged flushes per draw (:2043-2044).
- Retail indexed-VB draw `glDrawElements(GL_TRIANGLES, n*3, U16, offset*2)`
  (RndrGL:81614) matches our pIdx + _PrimOffset (MDisplaySDL2.cpp:2684).

**Verdict: ruled out.** Matrix delivery matches both reference backends.

## Summary

All four areas match retail RndrGL and PS3GCM; none explains the dancing
polygons. Real divergences found, all inactive in TheDream: (a) empty
ClearFrameBuffer stub (MDisplaySDL2.cpp:381) vs XRApp.cpp:5706; (b) private
RTT depth vs PS3 shared Z; (c) ScreenH() instead of current target height in
Y-flips. The flicker likely lives outside these sections — per-draw
VB_Get/streamer lifetime or draw ordering, not depth/RTT/viewport/matrix
state.
