# Research: Geometry Artifacts (Pa1_TheDream) — Agent B3

### 2. BSP2 PVS / cluster visibility — is culling correct?

BSP2 builds the visible set **entirely on CPU**; the backend is never consulted:

- OnRender2 locks PVS for the camera portal-leaf and runs portal traversal:
  `SceneGraph_PVSLock` + `EnableTreeFromNode` + `Portal_AddNode`
  (`Source/P5/Shared/MOS/XRModels/Model_BSP2/WBSP2Model.cpp:5301-5306`),
  then renders per-leaf lists (`WBSP2Model.cpp:4508-4518` RenderPortalLeaf).
- Visibility tests are box/sphere-vs-`CRC_ClipVolume` plane tests in
  `WBSP2Portal.cpp:2179-2187`, `2838`, `3054`, `3211-3219` — pure CPU.
- Light occlusion is CPU scissor-rect accumulation (`m_ScissorShaded`,
  `WBSP2Model.cpp:3753/3935`, `WBSP2Light.cpp:1873`), applied through
  `CRC_FLAGS_SCISSOR` attribs (`WBSP2Light.cpp:1937, 2008`) → our
  `glScissor` (`Source/P5/Shared/MOS/RenderContexts/GLES3/MDisplaySDL2.cpp:1436-1437`).

Occlusion-query stubs are harmless. The only engine consumer of
`OcclusionQuery_*` (`MRender.h:272-278`, base stubs return 0 at
`MRender.cpp:6102-6106`) is flare fading, gated by
`CRC_CAPS_FLAGS_OCCLUSIONQUERY` (`Source/P5/Shared/MOS/XR/XRUtil.cpp:1186-1208`).
PS3 sets that cap (`MRenderPS3_Context.cpp:87`); we deliberately don't
(`MDisplaySDL2.cpp:1164`), so the stub result is never read. **Ruled out.**

Clip_* methods: base `CRC_Core` provides a complete **software** user-clip-plane
stack (`MRender.cpp:3989-4036` push/pop/set/add, `3684` Clip_CutFace,
`3909` Clip_RenderPolygon). It is used for portal/mirror sub-views, not for
main-view PVS. Backends must honor it during VB rendering:
PS3GCM folds clip planes into the vertex program
(`MRenderPS3_Attrib.cpp:901-904, 962-977`); retail RndrGL uses HW
`glClipPlane` (`RndrGL_dll_decomp.c:77418`, inside `FUN_1006d900`
(RndrGL:77084) ≈ pre-draw state setup; software fallback `FUN_1006c070`
when extension bit `*(this+0x3f04)&0x4000` is clear, RndrGL:77401-77405).
**Our GLES3 implements nothing**: no `Clip_` override, and
`Render_IndexedTriangles` never checks `Clip_IsEnabled()`
(`MDisplaySDL2.cpp:2269-2288`). GLES3 has no `glClipPlane`
(`gl_ClipDistance` would be needed).

### 5. Face culling winding — CW vs CCW

Flags: `CRC_FLAGS_CULL=0x800`, `CRC_FLAGS_CULLCW=0x1000`
(`MRender_Classes.h:381-382`).

- Ours (only `glFrontFace` in backend): `MDisplaySDL2.cpp:1410-1415` —
  CULL→enable; `glFrontFace(CULLCW ? GL_CW : GL_CCW)`; `glCullFace(GL_BACK)`
  always. Net: default keeps CCW, CULLCW keeps CW.
- PS3GCM: front face fixed `CELL_GCM_CCW` (`MRenderPS3_Context.cpp:161`);
  CULLCW→`gcmSetCullFace(CELL_GCM_FRONT)` else BACK
  (`MRenderPS3_Attrib.cpp:288-294`). Net: default keeps CCW, CULLCW keeps
  CW — **in GCM's y-down window space**.
- RndrGL (retail, ground truth): `glFrontFace(0x901=GL_CW)` once at scene
  reset (`RndrGL_dll_decomp.c:38438`, `FUN_10032930`, RndrGL:38097 ≈
  BeginScene) with `glCullFace(GL_BACK)` (38439); per-attrib
  (`FUN_1006e0a0`, RndrGL:77512 ≈ `Attrib_Set`): CULL bit 0x800 →
  glEnable/Disable(GL_CULL_FACE) (77649-77657); CULLCW bit 0x1000 →
  `glCullFace(GL_FRONT)` else `GL_BACK` (77659-77666). Net: default keeps
  **CW**, CULLCW keeps CCW — opposite of ours in both branches.

PS3↔GL difference is explained by GCM's y-down window coordinates (same
triangle has opposite winding verdict). Our FBO, however, is plain GL:
`PresentToWindow` composite does **not** flip Y (`vUV=uv` at uRot=0,
`MDisplaySDL2.cpp:199-204`) and the image is upright, so winding verdicts
in our FBO equal retail-GL window verdicts. ⇒ Our mapping appears
**inverted vs RndrGL**: we keep CCW where retail keeps CW. The engine
itself toggles CULLCW for mirrored view contexts (`WBSP2Model.cpp:1888-1889`,
`2247`; `WBSP2Light.cpp:1941, 2012`), so the error flips inside mirror views.
Immediate and VB paths share one `ApplyAttribs` (`MDisplaySDL2.cpp:1372`,
via 1501-1502; `DrawIndexed` runs `Attrib_Update` at 2043) — consistent, no
divergence there. CPU-side `Clip_IsVisible` honors CULLCW+mirrored matrix
(`MRCCore.h:530-535`) and matches GL convention.

Symptom match: inverted culling draws backfaces of every surface at the same
depth as the front would be — enclosed corridor still "renders", but open,
single-layer and near-coincident geometry flickers with sub-pixel camera
motion: matches "polygons dance". Tests (no code change): `RIDDICK_NO_CULL=1`
(`MDisplaySDL2.cpp:757,789`). One-line fix candidate: swap to
`glFrontFace((F & CRC_FLAGS_CULLCW) ? GL_CCW : GL_CW)`.

### 7. The "polygons dance on the sides" pattern specifically

- **H1 — missing user clip planes: LIKELY (primary).** Portal sub-views are
  rendered with engine clip planes and *rely on the backend to cut geometry*:
  texture portals push the portal plane (`XREngine.cpp:2841, 2884`; edge
  planes commented out at 2842-2850) and recursive in-place portals push all
  portal-polygon planes (`XREngine.cpp:2920-2931`); `XRVBManager` applies
  them per-VB via `Clip_Set` (`XRVBManager.cpp:3586-3594`). PS3 clips in the
  VP, RndrGL via `glClipPlane`; we clip nothing, so a portal/mirror sub-view
  splats unclipped geometry across the framebuffer — maximally view-dependent,
  can cover the whole view. `run_dream.log` shows live RTT targets incl.
  1280x720 (`[GLES3-RTT] id=15905 FBO ok 1280x720`) → texture portals active
  in this level. Note the intended fallback contract: base
  `CRC_Core::Render_IndexedTriangles` only acts when `Clip_IsEnabled()`
  (`MRender.cpp:5755-5796`); overrides must call it — ours doesn't. Caveat:
  the software path needs `Clip_InitVertexMasks` first (call is commented out
  at `MRender.cpp:5759`; only `MRenderCapture.cpp:514` inits masks).
  **Test patch:** in `Render_IndexedTriangles`/`Render_IndexedTriangleStrip`/
  VB path of `MDisplaySDL2.cpp` early-return when `Clip_IsEnabled()` — if the
  dancing triangles vanish, H1 is confirmed; then implement
  `gl_ClipDistance` or the CPU fallback.
- **H2 — inverted winding (§5): POSSIBLE, cheap to test.** One-line swap or
  `RIDDICK_NO_CULL=1` (see §5).
- **H3 — texture-portal render/copy/clear misalignment: POSSIBLE.** Texture
  portals render the sub-view into the current framebuffer at the texture
  viewport, `AddCopyToTexture`, then `ClearViewport` the region
  (`XREngine.cpp:2856-2916`). Our CopyToTexture does a flipped blit
  (`MDisplaySDL2.cpp:1251-1273`) and scissor/clear use `ScreenH()` for
  Y-flip (`MDisplaySDL2.cpp:1357, 1431-1437`) — wrong height when an RTT FBO
  is bound; a misaligned clear leaves sub-view remnants on screen.
  **Test:** log viewport/clear rects during portal frames; force
  `RIDDICK_COPYTEX_FLIP=0`.
- **H4 — shadow-caster silhouette leak: RULED OUT** by user's
  RIDDICK_SKIP_SHADOWVOL; SV attribs are stencil-only, no COLORWRITE
  (`WBSP2Model.cpp:95-129`), and `glColorMask` honors the flags
  (`MDisplaySDL2.cpp:1405-1407`).
- **H5 — Z-prepass / EQUAL depth: NOT RULED OUT, uninvestigated.** BSP2 uses
  a separate Z attrib set (`m_RenderZBuffer`, `WBSP2Model.cpp:1888-1889`);
  verify `GLES3_MapCompare` maps `CRC_COMPARE_EQUAL` correctly.
