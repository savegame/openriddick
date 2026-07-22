# RTT / Post-Process Quad Inventory (agent A2)

Scope: every fullscreen/post-process quad or blit in the frame path, how it is
submitted, and how to gate it for a "direct render" mode. All paths relative to
repo root. Note: a `RIDDICK_DIRECT_RENDER` env flag **already exists** in the
GLES3 backend (see §d).

## (a) Inventory

| # | Quad/blit | Kind | Where (file:line) |
|---|-----------|------|-------------------|
| 1 | PresentToWindow composite quad (screen FBO → window, rotation) | FINAL image path | `Source/P5/Shared/MOS/RenderContexts/GLES3/MDisplaySDL2.cpp:678-730` (draw `:706`), shader src `:193/:207`, VBO `:998-1000` |
| 2 | RTT debug overlay quads (grid of live FBO textures) | debug only | `GLES3_RTTOverlay.cpp:62-119` (draw `:115`), invoked `MDisplaySDL2.cpp:710-725`, env `RIDDICK_DBG_RTT` (`GLES3_RTTOverlay.cpp:38-42`) |
| 3 | CopyToTexture blits (framebuffer → RTT texture) | intermediate-texture builder, NOT a quad | `MDisplaySDL2.cpp:1263-1335` (`glBlitFramebuffer` `:1306` / `glCopyTexSubImage2D` `:1324`) |
| 4 | Portal/mirror view capture | scene → texture | `Source/P5/Shared/MOS/XR/XREngine.cpp:2894` |
| 5 | ResolveScreen grabs (scene → texture for refraction/fx) | intermediate | `XREngine.cpp:3019`, `XREngine.cpp:3920`; consumed `WModel_EffectSystem.cpp:1513/1557`, grab `WModel_EffectSystem.cpp:1654`, `WObj_TentacleSystem_Render.cpp:385` |
| 6 | Deferred G-buffer copies (normal/diffuse/specular/motionmap/depth/shadowmask) | intermediate, inactive with current honest caps | `XREngine.cpp:3786`, `:3896-3906`, `:3949`, `:3955` |
| 7 | Envmap/colour-correction cube hexagon quads + copies | intermediate | `XREngine.cpp:4184-4188`, `:4191` (`Engine_CreateColorCorrection`), `:4314` |
| 8 | Engine post-process: screen grab, Gaussian/Radial blur quads, final tone/stretch quad, widescreen border rects | postprocess | `XREngine.cpp:4339` (`Engine_PostProcess`); grab `:4410`; blur quads `Source/P5/Shared/MOS/XR/XRUtil.cpp:3055/3124/3256/3274` (Gaussian), `XRUtil.cpp:3282-3457` (RadialBlur, VB `:3419`, copy callback `:3438`); FINAL quad `XREngine.cpp:4704-4760`; borders `:4770+` |
| 9 | Menu/GUI screen grabs (blur background, journal, Cube frontend downsample pyramid, DV effect) | postprocess/intermediate | `Source/P5/Projects/Main/GameWorld/WClientMod.cpp:800`; `WFrontEndMod_Menus.cpp:5857`; `WFrontEndMod_Cube.cpp:3526/3634/3715/5263/6179/6289/6367`; `WClientMod_DV.cpp:129/215/275/505/571` + RadialBlur `:396` |
| 10 | Character render-to-texture (portraits/mirrors) | intermediate | `WObj_CharRender.cpp:2422/2453` |
| 11 | TriMesh coverage capture | intermediate | `Source/P5/Shared/MOS/XRModels/Model_TriMesh/WTriMesh.cpp:6024` |
| 12 | In-scene screen-space rects (shadowmask/ambience/fog clears) | PART of scene shading | `XREngine.cpp:3729/3746/3757/3776/3982/4009` |
| 13 | GUI window quads (menu/HUD surfaces) | FINAL image content | `Source/P5/Shared/MOS/Classes/Win/MWinGrph.cpp:371/798` via `CXR_Util::Render_Surface`, priority `CXR_VBPRIORITY_WINDOWS=10000` (`Source/P5/Shared/MOS/XR/XRVBPrior.h:65`) |

No bloom/tonemap exists beyond `Engine_PostProcess` (glow + exposure + colour
correction + final stretch). No separate XRShader fullscreen passes found.

## (b) Submission paths

All engine-side quads/blits go through the CXR_VBManager deferred queue:

- `CXR_VBManager::AddCopyToTexture` (`Source/P5/Shared/MOS/XR/XRVBManager.cpp:1800-1814`)
  allocates `CXR_PreRenderData_RenderTarget_CopyToTexture1` and registers an
  `AddCallback` VB (color `0xffffff00`, given priority). At flush the
  PreRender callback (`Source/P5/Shared/MOS/XR/XRVBUtil.h:69-74`) calls
  `CRenderContext::RenderTarget_CopyToTexture` → GLES3 blit (#3).
- `AddSetRenderTarget` (`XRVBManager.cpp:1845-1855`, callback `:1837-1843`) →
  `RenderTarget_SetRenderTarget` (`MDisplaySDL2.cpp:1197-1256`) binds the RTT FBO.
- `AddClearRenderTarget` (`XRVBManager.cpp:1785-1798`) → `RenderTarget_Clear`
  (`MDisplaySDL2.cpp:1337-1390`).
- Real quads (blur passes, final tone quad, GUI rects) are ordinary VBs:
  built by `CXR_Util::VBM_RenderRect` / `Geometry_VertexArray` +
  `Render_IndexedTriangles` (e.g. `XRUtil.cpp:3419`), submitted via
  `pVBM->AddVB(...)` with a priority (`XRVBPrior.h`; post-process uses ~0,
  scene 2048, windows 10000 — higher = later).

Frame chain: `CWClient_Mod::Render/Render_GUI` (`WClientMod.cpp:721-803`,
`Engine_PostProcess` at `:781`) and `CWorld_ClientCore::Render_GUI`
(`Source/P5/Shared/MOS/Classes/GameWorld/Client/WClient_Render.cpp:372,790`)
fill the VBM → `XRApp.cpp:5724` `BeginScene`, `pVBM->Render(pRC,...)`
(`XRApp.cpp:5744-5771`) sorts by priority and fires PreRender callbacks +
draws VBs through `CRenderContext::Render_VertexBuffer`
(`MDisplaySDL2.cpp:2631`) / `Render_IndexedTriangles` (`:2318`) → `EndScene` →
`PageFlip` (`XRApp.cpp:5837`).

## (c) Final to-screen path

`CDisplayContextSDL2::PageFlip` (`MDisplaySDL2.cpp:325-345`) calls
`CRC_GLES3::PresentToWindow` (`:335`) then `SDL_GL_SwapWindow` (`:336`).
PresentToWindow binds window framebuffer 0, draws the 4-vertex
`GL_TRIANGLE_STRIP` composite quad sampling `m_ScreenColorTex` (screen FBO,
created `EnsureScreenFBO` `:600-645`) with the composite shader + `uRot`
rotation uniform, then optionally the RTT overlay, then re-binds the screen
FBO (`:728-729`). All "backbuffer" rendering during the frame targets the
screen FBO via `BindScreenTarget` (`:659-671`).

## (d) Flag-gating plan

Existing: `RIDDICK_DIRECT_RENDER=1` already (a) forces every
`RenderTarget_SetRenderTarget` to the screen FBO (`MDisplaySDL2.cpp:1226-1241`)
and (b) early-outs `RenderTarget_CopyToTexture` (`:1270`). That alone makes
the scene render "directly"; everything below refines it per quad kind.

Correct direct target: keep the **screen FBO**, not window fb 0 — fb 0 has a
different size, no guaranteed depth24+stencil8, and rotation/winsize mapping
lives in the composite. PresentToWindow stays unchanged and remains mandatory.

- SAFE to skip (visual loss only): #8 entire `Engine_PostProcess` block —
  gate at the caller (`WClientMod.cpp:769-781`) or early-out
  `XREngine.cpp:4339`; note if the viewport is upscaled the final stretch quad
  (`:4704-4760`) must be kept in a pass-through variant. #9 menu grabs/DV
  (menu bg loses blur, becomes sharp scene). #2 overlay (already env-gated).
- SKIP WITH CARE (breaks dependent materials, placeholder textures): #4
  portal/mirror capture, #5 ResolveScreen (refraction effects sample it), #7
  envmap cube, #10 portraits, #11 coverage. Skip only together with (b) above
  so no stale copies fire; sampled textures fall back to placeholder.
- UNSAFE to skip: #1 PresentToWindow composite (the only window-fb draw —
  skipping shows nothing), #13 GUI quads and #12 shading rects (they ARE the
  image), #3 blits unless the consumer is also gated (ordering: copies fire
  by priority mid-flush; redirecting SetRenderTarget without skipping the
  copy makes the blit read the wrong framebuffer — the existing `:1270`
  early-out handles this).
- Already inactive: #6 deferred copies (require `CRC_CAPS_FLAGS_MRT`/
  `COPYDEPTH`, cleared by the honest-caps fix).

If a future mode binds fb 0 directly: `BindScreenTarget` would bind 0,
PresentToWindow becomes a no-op, and rotation/`-fbosize` are lost; window
depth/stencil (requested `MDisplaySDL2.cpp:294-295`) must suffice.
