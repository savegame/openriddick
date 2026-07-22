# Research: Geometry Artifacts — Agent A2, Report A

Scope: depth setup, RTT/FBO mixing, viewport/scissor, camera-matrix handoff.
Ground truth: RndrGL_dll_decomp.c (retail Win32 GL), PS3GCM sources, run_dream2.log.

Function mappings used below:
- `FUN_10031400` (RndrGL:37256) = `Viewport_Update` → `CRC_GLES3::Viewport_Update` (MDisplaySDL2.cpp:1566)
- `FUN_10066dc0` (RndrGL:72146) = `RenderTarget_Clear` → ours (MDisplaySDL2.cpp:1313)
- `FUN_1006e0a0` (RndrGL:77512) = `Attrib_Set` bundle → `ApplyAttribs` (MDisplaySDL2.cpp:1372)
- `FUN_10032930` (RndrGL:38097) = GL init (sets `glFrontFace(GL_CCW)`/`glCullFace(GL_BACK)` once, RndrGL:38438-38440)
- `FUN_10069230` (RndrGL:73719) = compare-func map 1..8 → GL_NEVER..GL_ALWAYS

### 1. Depth-buffer setup — does GLES3 backend match engine expectations?

- PS3 allocates **Z24S8**: `m_DepthbufferFormat = CELL_GCM_SURFACE_Z24S8` (MDisplayPS3.cpp:598), zcull `CELL_GCM_ZCULL_Z24S8` (MDisplayPS3.cpp:617).
- We allocate **GL_DEPTH24_STENCIL8** for both RTT slots (MDisplaySDL2.cpp:508) and the screen FBO (MDisplaySDL2.cpp:615). Bit-exact match with PS3.
- **DepthRange**: retail RndrGL contains **no `glDepthRange` and no `glClipControl` anywhere** (grep over 50 MB decomp: zero hits). PS3 hard-codes viewport depth min=0/max=1 (MRenderPS3_Context.cpp:265-266). Engine never requests a sub-range; nothing to respect. The [0..1]→[-1..1] NDC remap is done in-shader (MDisplaySDL2.cpp:63).
- **Clear value**: engine passes `1.0f` (far), e.g. WFrontEndMod_Cube.cpp:6004,6125,6377 → `AddClearRenderTarget` → `m_ZClearTo` (XRVBManager.cpp:1794) → `RenderTarget_Clear` (XRVBUtil.h:20). Retail does `glClearDepth((double)param_1)` with that value verbatim (RndrGL:72194) and `glClearDepth(1.0)` at init (RndrGL:44432). Ours: `glClearDepthf(_ZBufferValue)` (MDisplaySDL2.cpp:1337) — identical.
- Compare mapping identical: `GLES3_MapCompare` (MDisplaySDL2.cpp:175-186) == `FUN_10069230` (1→GL_NEVER … 8→GL_ALWAYS); PS3 zcull uses LESS (MDisplayPS3.cpp:616-617). Clear-flag bits match too: CDC_CLEAR_COLOR=0x1/ZBUFFER=0x10/STENCIL=0x20 (MDisplay.h:20-26) == RndrGL mask checks (RndrGL:72180-72200).
- Latent nit (not active in TheDream): scissored clear Y-flips with `ScreenH()` (MDisplaySDL2.cpp:1357) instead of the bound FBO's height.

**Verdict: ruled out.** Formats, clear value, compare funcs all match both reference backends. Test patch (paranoia only): log `_ZBufferValue` in `RenderTarget_Clear`, or force `glDepthFunc(GL_LESS); glClearDepthf(1.0f)` at every `BeginScene` to exclude leaked state.

### 3. Multi-target / RTT rendering — do we mix framebuffers?

- run_dream2.log: **75 494 `[GLES3-RT]` lines, ALL `CopyToTexture`; `grep -c SetRenderTarget` = 0**. In Pa1_TheDream the engine never redirects rendering into an RTT; it snapshots the backbuffer via CopyToTexture. 8 FBOs created (1280x720 x5, 512x512, 256x256, 128x128 — run_dream2.log `[GLES3-RTT]`), sizes match the engine's CopyToTexture rects exactly (e.g. id=15914 512x512, 37 486 copies).
- This matches retail: RndrGL has **no FBO/pbuffer support at all** (no `glBindFramebuffer`, no `wglCreatePbuffer`; only `glCopyTexSubImage*` proc lookups, RndrGL:67934) — CopyToTexture is the retail GL RTT path. PS3 does true target switching: `RenderTarget_SetRenderTarget` → `MRT_SetRenderTarget` switches the GCM surface immediately, asserting equal W/H/format across MRT slots (MDisplayPS3.cpp:664-704).
- Binding hygiene: our flipped-blit path saves/restores DRAW+READ bindings and scissor state (MDisplaySDL2.cpp:1275-1289); `BeginScene` re-binds the screen FBO when no RTT is active (MDisplaySDL2.cpp:1547-1548); `PresentToWindow` leaves the screen FBO bound (MDisplaySDL2.cpp:670-674). No stale-binding leak found.
- RTT sizes: `EnsureFBOFor` takes W/H from `GetTextureDesc` with window fallback (MDisplaySDL2.cpp:481-494) — log confirms engine-respected sizes.

**Verdict: ruled out** for the main-view flicker — in TheDream there is exactly one draw target all frame. Test patch: print `GL_DRAW_FRAMEBUFFER_BINDING` in `[GLES3-CLEAR]`-style at each `Render_VertexBuffer` (first 100 calls) to prove all world draws land on the screen FBO.

### 4. Viewport / scissor state machine

- All three backends run the **same algorithm**: scale projection columns 0/1 by 2/W, 2/H (ours MDisplaySDL2.cpp:1579-1585; PS3 MRenderPS3_Context.cpp:283-286; RndrGL `FUN_10031400` RndrGL:37319-37327) and set a Y-flipped viewport (`glViewport(x, height - p1.y, w, h)`: ours MDisplaySDL2.cpp:1590-1591; RndrGL:37311; PS3 does the flip inside `gcmSetViewport` scale/offset, MRenderPS3_Context.cpp:267-272).
- Push/pop: base `CRC_Core::Viewport_Pop` re-invokes our `Viewport_Update` (MRender.cpp:2508-2514); `Viewport_Push` copies the stack top without state change (MRender.cpp:2500-2506); `BeginScene` → `Viewport_Set` → update (MRender.cpp:2435-2440). `m_ProjMat` is refreshed at exactly those points — no stale-cache window found. We do not override Push/Pop, same as PS3.
- Same `ScreenH()`-vs-FBO-height caveat as §1 (MDisplaySDL2.cpp:1590) — inactive in TheDream (no RTT targets bound).
- Precision: W/H are ints from `GetViewArea`; fp32 scale identical to references. No multi-viewport rounding hazard beyond retail.

**Verdict: ruled out.** Test patch: one-shot stderr dump of `R`, W/H and resulting `glViewport` per `Viewport_Update` for ~60 frames; confirm rect stability when the camera micro-moves.

### 6. Camera / view matrix handoff

- Engine path confirmed: model matrix via `Matrix_SetRender(CRC_MATRIX_MODEL, &Mat)` from `Matrix_Update` (MRender.cpp:3410-3431); projection exclusively via `Viewport_Update` pulling `CRC_Viewport::GetProjectionMatrix` (RndrGL:37297; PS3 MRenderPS3_Context.cpp:249; ours MDisplaySDL2.cpp:1576). `CRC_MATRIX_PROJECTION` through `Matrix_SetRender` is explicitly unsupported on PS3 (MRenderPS3_Attrib.cpp:1117-1119).
- PS3 uploads **Model×Proj constants immediately inside `Matrix_SetRender`** (M_VMatMul + `gcmSetVertexProgramConstants`, MRenderPS3_Attrib.cpp:1080-1105); RndrGL keeps PROJECTION loaded from `FUN_10031400` (`glMatrixMode(0x1701); glLoadMatrixf`, RndrGL:37318-37328) and loads MODEL per change (`glLoadMatrixf`, RndrGL:76171-76469). We cache `m_ModelMat`/`m_ProjMat` and multiply at draw time in `SetupCommonUniforms` (MDisplaySDL2.cpp:1823-1831). Ordering equivalent: engine always flushes matrices before geometry (MRender.cpp:3430), and `m_ProjMat` can only change inside `Viewport_Update`, which is always followed by new `Matrix_SetRender` calls before the next draw.
- NULL-matrix semantics handled (identity reset, MDisplaySDL2.cpp:1521-1522) matching MRender.cpp:3410.

**Verdict: ruled out.** Test patch: in `SetupCommonUniforms`, dump `m_ModelMat` row 3 (translation) for the first 20 draws after each `Viewport_Update`; a stale matrix shows as an unchanged translation across camera moves.

---

**Summary across sections:** all four investigated mechanisms are bit- or behaviour-identical to retail GL and PS3. The flicker source most likely lives outside these sections (vertex/index streaming or shader attribution), not in depth/RTT/viewport/matrix state.
