# RTT / post-process quads — synthesis of A1 + A2 double-blind reports

Sources: `Docs/RTT_A1.md`, `Docs/RTT_A2.md`. Both agents converged on the
architecture and the gating plan; the single real disagreement is the
direct-render target (below). Moderator spot-check confirmed the key shared
claim (existing `RIDDICK_DIRECT_RENDER` flag) against the code.

## Consensus: how quads reach the render queue

All engine-side quads and copies share ONE submission mechanism:

- Quad VBs are built by `CXR_Util::VBM_RenderRect` (`XR/XRUtil.cpp:2589`) —
  4-vert VB + 2D projection + priority — and queued via `CXR_VBManager::AddVB`.
- Copies/target switches are registered as priority-tagged PreRender callback
  VBs: `CXR_VBManager::AddCopyToTexture`/`AddSetRenderTarget`
  (`XR/XRVBManager.cpp:1800-1855`).
- Everything fires during the single flush `pVBM->Render()`
  (`XRApp.cpp:5744-5771`) → quads draw through `CRC_GLES3::Render_VertexBuffer`
  (`MDisplaySDL2.cpp:2631`); copies go to `RenderTarget_CopyToTexture`
  (`MDisplaySDL2.cpp:1263`, a blit, not a drawn quad).

## Consensus: inventory

- **Load-bearing (the image itself):**
  - `PresentToWindow` composite (`MDisplaySDL2.cpp:678-730`): TRIANGLE_STRIP
    from `m_CompVBO`, shader `:193-213`, called from `PageFlip` (:335) before
    `SDL_GL_SwapWindow` (:336). The ONLY pass putting pixels into the window
    today — skipping it under current architecture shows nothing.
  - Engine post-process chain `Engine_PostProcess` (`XREngine.cpp:4339`, called
    from `WClientMod.cpp:781`): screen-capture copies (:4410,:4502), motion
    blur (:4489), exposure/histogram quads (:4581), gaussian glow
    (`XRUtil.cpp:3055-3457`), color-correction hexagons (`XREngine.cpp:4191-
    4320`), and the FINAL fullscreen quad (`XREngine.cpp:4704-4760`, FP20
    `XREngine_Final*` sampling Screen+Blur+ColorCorr). The final quad only adds
    exposure/glow — the base scene already sits in the screen FBO, so skipping
    the whole chain is image-safe.
  - GUI/menu quads (e.g. `WFrontEndMod_Menus.cpp`, priority 0.1f) — UI, must
    stay. The ~30 copies/frame in menus come from cube-menu blur/journal
    passes (`WFrontEndMod_Cube.cpp`).
- **Intermediate textures (not the screen image):** portal/mirror captures
  (`XREngine.cpp:2894`), envmaps, ResolveScreen/deferred copies, camera effects
  (`CXR_Model_CameraEffects`, `WClientMod_DV.cpp`), and the debug
  `CGLES3RTTOverlay` quads (`GLES3_RTTOverlay.cpp`, already gated by
  `RIDDICK_DBG_RTT=1`).

## Consensus: the flag anchor already exists

`RIDDICK_DIRECT_RENDER=1` is already implemented backend-side (moderator-
verified): `SetRenderTarget` redirect of ALL passes into the screen FBO
(`MDisplaySDL2.cpp:1226-1241`) and early-out of `RenderTarget_CopyToTexture`
(`:1270`, note: per-call `getenv`, uncached). It kills the RTT plumbing but
still composites via `PresentToWindow`. Remaining work for the user's "disable
all post-process quads" goal:

1. Early-return at the top of `Engine_PostProcess` (`XREngine.cpp:4339`) —
   kills the whole post chain (incl. the final FP20 quad) in one gate.
2. Skip the camera-effects call (`WClientMod_DV.cpp`).
3. Keep portal/mirror captures behind a separate sub-flag — skipping them
   breaks mirror/portal textures (placeholder).
4. Do NOT gate: GUI quads, Z-prepass/shading rects, stencil-clear rects.
5. Reuse the existing env var (cache it in one place) rather than adding a
   second name; A1 warns the name collides conceptually with a *true* fb-0
   direct mode — pick one semantics and document it in AGENTS.md.

## Disagreement: what is the correct direct-render target?

- **A1:** a TRUE direct render should bind window framebuffer 0 at window size
  (`BindScreenTarget` → fb 0) and make `PresentToWindow` an early-return;
  valid only with rotation = 0.
- **A2:** keep the screen FBO as the target — rotation, `-fbosize`/`-winsize`
  mapping, and guaranteed depth24+stencil8 all live in the FBO+composite pair;
  fb 0 only as a separate mode where `PresentToWindow` is a no-op.

**Moderator assessment:** the positions are two different modes, not a
contradiction. The user's ask ("disable post-process quads") is fully covered
by A2's conservative mode: scene+UI render into the screen FBO exactly as
today, `Engine_PostProcess` and RTT copies gated off, `PresentToWindow` kept —
this is minimal-risk and keeps rotation/letterboxing/stencil working. A1's
fb-0 mode additionally eliminates the last composite quad (one fullscreen
textured strip per frame) but requires: requesting depth24/stencil8 on the SDL
GL context, losing `-rotate`/`-fbosize` independence, and re-checking the
Y-orientation of `CopyToTexture`-free rendering. Recommend: implement the
conservative mode first behind `RIDDICK_DIRECT_RENDER=1` (extending the
existing flag with the `Engine_PostProcess` gate); add fb-0 as
`RIDDICK_DIRECT_RENDER=2` later if the final blit shows up in profiles.

## Suggested implementation order

1. Cache the env flag once (backend init) instead of per-call `getenv`.
2. Gate `Engine_PostProcess` + camera effects (safe skips).
3. Sub-flag for portal/mirror captures; verify mirrors degrade gracefully.
4. Run with `RIDDICK_DBG_RTT=1` overlay OFF and count `[GLES3-RT]` lines in
   run.log — should drop to ~0 in-game.
5. Optional later: fb-0 mode (`=2`) with `PresentToWindow` early-return.
