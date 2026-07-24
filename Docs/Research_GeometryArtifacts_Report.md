# Synthesis: "Dancing polygons" — 4 independent agent reports

Meta-study over `Research_GeometryArtifacts_Report_A1_A.md` / `_A2_A.md`
(sections 1,3,4,6) and `_A3_B.md` / `_A4_B.md` (sections 2,5,7). Disagreements
were resolved by directly re-reading the cited code (moderator checks noted
inline). Per-section verdict: **likely / possible / ruled out** + cheapest test.

## 1. Depth-buffer setup — RULED OUT (A1 = A2, full consensus)

- PS3 allocates Z24S8 (`MDisplayPS3.cpp:598` / `MRenderPS3_RenderTarget.cpp:760`);
  our `GL_DEPTH24_STENCIL8` (`MDisplaySDL2.cpp:508,615`) bit-matches.
- Retail RndrGL contains **zero** `glDepthRange`/`glClipControl` calls; all
  backends run depth range [0,1]. Engine clears Z to 1.0f
  (`WFrontEndMod_Cube.cpp:6004`); retail `glClearDepth(param)` (RndrGL:72194,
  `FUN_10066dc0`) is a pass-through — identical to our
  `glClearDepthf(_ZBufferValue)`.
- Compare-func map equals retail `FUN_10069230` (RndrGL:73719).
- Both agents note one real but TheDream-inactive gap:
  `CDisplayContextSDL2::ClearFrameBuffer` is an empty stub
  (`MDisplaySDL2.cpp:381`) though XRApp calls it every frame (`XRApp.cpp:5706`).

## 2. BSP2 PVS / occlusion stubs — RULED OUT (B3 = B4, full consensus)

- PVS/portal traversal is fully CPU-side: `SceneGraph_PVSLock`
  (`WBSP2Model.cpp:5301-5306`), `Portal_AddNode` recursion
  (`WBSP2Portal.cpp:523,2179-2187`). No backend occlusion query feeds culling.
- Only consumers of occlusion queries: flare fading — caps-gated off since we
  don't set `CRC_CAPS_FLAGS_OCCLUSIONQUERY` (`XRUtil.cpp:1186`,
  `MDisplaySDL2.cpp:1164`) — and the exposure histogram
  (`XREngine.cpp:4537-4596`; stubs `MRender.cpp:6102-6106` → at worst brightness
  flicker, not geometry).
- `Clip_*`: base `CRC_Core` maintains a clip stack; **neither PS3GCM nor GLES3
  override Clip_\*** — but PS3GCM *consumes* it (see §7).

## 3. RTT / FBO mixing — RULED OUT for TheDream (A1 = A2, consensus by log evidence)

- `run_dream2.log`: 75 494 `[GLES3-RT]` lines, **all CopyToTexture, zero
  SetRenderTarget in-game** — the frame renders to a single target; no FBO
  switching occurs while artifacts are visible. Retail GL likewise has no
  FBO/pbuffer — CopyToTexture is the retail RTT path.
- Our blit saves/restores both framebuffer bindings
  (`MDisplaySDL2.cpp:1275-1289`); no stale-binding leak found.
- Latent (inactive in TheDream): per-RTT private depth RBO vs PS3's shared main
  Z (`MRenderPS3_RenderTarget.cpp:776`); Y-flips use `ScreenH()` instead of the
  bound target's height (`MDisplaySDL2.cpp:1357,1590`).

## 4. Viewport / scissor — RULED OUT (A1 = A2, consensus)

- All three backends run the identical algorithm: scale projection columns 0/1
  by 2/W, 2/H + Y-flipped viewport — ours `MDisplaySDL2.cpp:1579-1591`, PS3
  `MRenderPS3_Context.cpp:283-286`, retail `FUN_10031400` (RndrGL:37311-37328).
- No precision loss identified; engine Viewport_Push/Pop is handled by
  `CRC_Core` state save and both agents found the refresh ordering correct.

## 6. Matrix handoff — RULED OUT (A1 = A2, consensus)

- Only MODEL is pushed per drawcall (`MRender.cpp:3430`); projection arrives via
  `Viewport_Update`. `m_ProjMat` can only change inside `Viewport_Update`,
  always followed by fresh `Matrix_SetRender` — no stale window.
- Retail uploads MODELVIEW per `Matrix_SetRender` (RndrGL:76164-76176) and
  composes at draw time — equivalent to our draw-time MVP upload
  (`MDisplaySDL2.cpp:1826-1828`); PS3 uses a dirty-bit scheme
  (`MRenderPS3_Attrib.cpp:1080-1105`).

## 5. Face-culling winding — **LIKELY** (B3/B4 agree on verdict, disagree on
evidence; moderator check confirms B4)

- **Disagreement:** B3 read retail init as `glFrontFace(GL_CW)`; B4 as
  `GL_CCW`. Moderator read of RndrGL:38438: `glFrontFace(0x901)` = **GL_CCW**,
  `glCullFace(0x405)` = GL_BACK — B4 correct.
- Retail per-attrib (RndrGL:77659-77666): flag 0x1000 (= `CRC_FLAGS_CULLCW`,
  `MRender_Classes.h:381-382`) set → `glCullFace(GL_BACK)`; unset →
  `glCullFace(GL_FRONT)`; front face stays CCW. PS3GCM identical
  (`MRenderPS3_Attrib.cpp:288-294`: CULLCW → cull BACK else FRONT).
- Ours (`MDisplaySDL2.cpp:1410-1416`): `glFrontFace(CULLCW ? GL_CW : GL_CCW);
  glCullFace(GL_BACK)`. Net kept faces: retail keeps CCW-wound under CULLCW /
  CW-wound without; we keep exactly the **opposite** in both branches. No Y-flip
  in our FBO path compensates (shader passes `gl_Position.y` through, :56).
  World surfaces render with `CRC_FLAGS_CULL` on (`XRUtilRS.cpp:493-496`) →
  world draws inside-out → view-dependent extra/occluding triangles. Matches
  the symptom better than anything else found.
- **Test (one line):** `glFrontFace(GL_CCW); glCullFace((F & CRC_FLAGS_CULLCW)
  ? GL_BACK : GL_FRONT);`. Control: existing `RIDDICK_NO_CULL=1` should also
  change the picture.

## 7. Dancing-polygons hypotheses (B3 vs B4 — main divergence)

- **H1 Portal/user clip planes — POSSIBLE (B3: primary suspect; B4: real but
  secondary).** Engine pushes portal clip planes for portal/mirror sub-views
  (`XREngine.cpp:2884,2931`), applied per-VB via `Clip_Set`
  (`XRVBManager.cpp:3586-3594`); PS3GCM clips in the VP
  (`MRenderPS3_Attrib.cpp:901-904,962-977`); retail uses `glClipPlane`
  (`FUN_1006d900`, RndrGL:77418). GLES3 ignores the clip stack entirely
  (`Render_IndexedTriangles` never checks `Clip_IsEnabled`,
  `MDisplaySDL2.cpp:2269-2288`). Disagreement is about scope: planes are pushed
  for **sub-view (texture portal/mirror)** renders, so H1 explains corrupted
  portal/mirror contents; as the *main-view* cause it is weaker than §5.
  Moderator verdict: implement after the winding fix; test = early-return in
  draw paths when `Clip_IsEnabled()` (B3's patch) to see if artifacts vanish.
- **H2 Mirror pass into main framebuffer — RULED OUT** by §3 log evidence (zero
  SetRenderTarget in-game).
- **H3 Shadow-caster/silhouette faces as color — RULED OUT** by
  RIDDICK_SKIP_SHADOWVOL test (task statement).
- **H4 Z-prepass EQUAL compare — POSSIBLE,** noted by B3, uninvestigated by
  both; compare mapping itself matches retail (§1), but interaction of the
  Z-prepass with inverted culling (§5) is a plausible amplifier.

## Bottom line

Sections 1,2,3,4,6: both-pairs consensus, ruled out with strong evidence.
Prime suspect: **§5 inverted cull winding** (verified against retail decomp +
PS3 source). Secondary gap: **§7-H1 unimplemented clip planes** for
portal/mirror sub-views. Suggested order: (1) one-line winding swap; (2)
`RIDDICK_NO_CULL=1` as control; (3) clip-plane support (glClipPlane equivalent
via `gl_ClipDistance` in shader); (4) only then revisit H4.
