# Report: P5 dynamic-lighting render pipeline

Per `Docs/Research_LightPass.md`. Headline: there is **no `Attrib_Lights`-driven
per-light state** in the live PC path — lights reach the GPU as **vertex-program
constants** (single-pass multi-light) or as **FP20 fragment-program ext-attributes
+ texgen** (one additive pass per light). Our GLES3 backend ignores both channels
and normals — hence the black scene.

## 1. Where the multi-pass loop lives

No `SubmitLight`/`AddLightPass` anywhere. Each model renderer iterates its lights
and queues one `CXR_VertexBuffer` per pass into the priority-sorted
`CXR_VBManager`; passes of a light share priority `m_iLight + offset` and flush
grouped:

- TriMesh: light loop `XRModels/Model_TriMesh/WTriMesh.cpp:1998`; shadow VBs
  `:2101-2111`, shading via `RenderShading(Light,...)` `:2142`.
- BSP2: `RenderShaderQueue` `XRModels/Model_BSP2/WBSP2Model.cpp:3245` (batch per
  `m_iLight`, `:3283-3324`); `Light_RenderShading` `WBSP2Light.cpp:89-153`.
- One iteration = `CXR_Shader::RenderShading` `XR/XRShader.cpp:1511` (one light, N
  VBs) → `RenderShading_FP20_COREFBB` `XR/XRShader_FP20.cpp:298`: packs
  LightPos/Range/Color/SpecColor/EyePos as FP constants (`:378-399`), tangent-space
  texgen (`:363-366`), queues VB at priority `m_iLight + 0.3f` (`:429-434`).
- Flush: `CXR_VBManager::Render` `XR/XRVBManager.cpp:2316` → per VB `Attrib_Set` +
  `Render_VertexBuffer`/`Render_IndexedTriangles` (`XR/XRVertexBuffer.cpp:229-278`)
  — same entries as the base pass.
- State delta base→light: additive `SourceBlend/DestBlend(CRC_BLEND_ONE, ONE)` +
  blend/cull/colorwrite on, alphawrite off (`XRShader_FP20.cpp:114-117`); stencil
  ref 128, `LESSEQUAL`, write-mask 0 (`:119-121`); Z `EQUAL` (`:199`) against the
  Z-prepass (`WTriMesh.cpp:1207`).

## 2. `CRC_Light` and `Attrib_Lights`

- `CRC_Light`: `MSystem/Raster/MRender_Classes.h:122-143` — `m_Type`
  (POINT/SPOT/PARALLELL/AMBIENT, `:117-120`), `m_Color`, `m_Ambient`, `m_Pos`,
  `m_Direction`, `m_Attenuation[3]` (range folded into `[1]`). No cone angles.
  Engine-side `CXR_Light` is richer: `XR/XRClass.h:431-455` (`m_Range`,
  `m_SpotWidth/Height`, `m_ProjMapID`, `m_iLight`).
- `Attrib_Lights` declared `MSystem/Raster/MRender.h:176`; base impl stores the
  pointer only (`MSystem/Raster/MRender.cpp:3028`). Live XR callers: **none** on
  the PC path (`XR/XRUtilRS.cpp:123` is `#if 0`'d). FP20 passes light data via
  `CRC_ExtAttributes_FragmentProgram20` instead (`XRShader_FP20.cpp:372-376, 419`).
- `CRC_FLAGS_LIGHTING = 0x00080000` (`MRender_Classes.h:388`) = fixed-function
  **vertex** lighting ("Normals must be supplied"), not the additive-pass marker.
  Additive passes are identified by ONE/ONE blend + `CXR_VBFLAGS_LIGHTSCISSOR` +
  `m_iLight` (`XR/XRVertexBuffer.h:179`).

## 3. What PS3GCM does per light pass

- **No `Attrib_Lights` override** — work happens in `Attrib_Set` →
  `Attrib_SetVPPipeline` `RenderContexts/PS3GCM/MRenderPS3_Attrib.cpp:872`:
  `CRC_FLAGS_LIGHTING` → `VPFormat.SetLights(m_pLights, m_nLights)` (`:894-898`),
  registers filled `:987-993`, uploaded `:1009-1011`.
- Layout: **2 vec4 constants per light** — pos/dir + attenuation, color/255
  (`Classes/Render/MRenderVPGen.h:1041-1085`); base index token `LIGHT`
  (`MRenderVPGen.cpp:294-296`).
- Program selection: AVL cache keyed by full `CRC_VPFormat` incl. `m_nLights:4` +
  per-light type bits (`MRenderVPGen.h:124-128`); miss → generate from template
  `System/GL/VP.xrg` (resource, not in repo) with `LIGHTING`/`LIGHT0_POINT`/...
  defines (`MRenderVPGen.cpp:537-623`, `MRenderPS3_VertexProgram.cpp:273-283`).
  ⇒ **program per light-count/type combo, lighting computed in the vertex shader**;
  no in-program loop.
- Blend: attribute-driven only; `CRC_RASTERMODE_ADD` → ONE/ONE
  (`MRenderPS3_Attrib.cpp:69`).
- Normals: fixed HW slot ATTR1/`_vnrm` (`MRenderPS3_VertexProgram.cpp:19,43`;
  `MRenderPS3_Geometry.cpp:1315-1324`); `SetLights` forces
  `USENORMAL|NORMALIZENORMAL` (`MRenderVPGen.h:465-473`).

## 4. Retail OpenGL DLL: multi-pass equivalent

- No literal `glBlendFunc`; `glBlendFuncSeparate` via proc pointer at
  `RndrGL_dll_decomp.c:77788` and `:78291`, factors mapped from the attribute block
  (`FUN_100691b0`, `:73686`: 2→GL_ONE). ONE/ONE + `GL_EQUAL` depth
  (`FUN_10069230`, `:73719`; `glDepthFunc` `:77791`) supported but **never
  hard-coded** — RndrGL is a pure state machine; pass logic is engine-side.
- `glProgramStringARB` single site `:87208` (`FP_Load`): FP sources are
  **engine-generated strings**. `glProgramLocalParameter4fvARB` never used; params
  are VP env params (`glProgramEnvParameter4fvARB` `:76366-76379`, target `0x8620`).
- Lights → VP constants: `FUN_1006c170` (`:75865`), **3 vec4/light, all lights in
  one pass**, overflow warn at `:77470`. Fixed-function fallback loops 8×
  `glLightfv` (`:75087-75169`).
- Stencil: op map has `GL_INCR_WRAP/DECR_WRAP` (`:73746`); two-sided via
  `glStencil*SeparateATI` (`:70147-70149`, used `:78478/:78521`) — shadow volumes.

## 5. Shadow volumes

- Active path: `Light_RenderDynamicLight` `WBSP2Light.cpp:1684`, per visible
  dynamic light from `OnRender` (`WBSP2Model.cpp:5239-5245`). Silhouette faces per
  frame (`Light_BuildShadowFaces_*` `WBSP2Light.cpp:812-1213`), extruded by
  `Light_CreateShadowVolume` (`:237`, length `m_Range/0.577`, `:511`). Legacy
  `StencilLight_*` (`WBSP2StencilLight.cpp:258-712`) is dead.
- Stencil: cleared to 128 (`m_Unified_Clear`, `XR/XREngine.cpp:1203`) by a scissor
  rect at priority `iLight - 0.001` (`XREngine.cpp:3715-3732`); volume pass at
  `+0.001/+0.002`, no color/Z writes, `GREATEREQUAL`, front DEC / back INC via
  separate stencil (`WBSP2Light.cpp:1913-1975`) — count-behind-depth (z-fail
  equivalent). Shading at `+0.3x` tests `LESSEQUAL 128`, write-mask 0
  (`XRShader_TexEnvCombine.cpp:621-623`). Order per light: **clear → volume →
  shading**.
- Conditionals: `CXR_LIGHT_NOSHADOWS` (`XR/XRClass.h:217`),
  `CXR_RENDERINFO_NOSHADOWVOLUMES` (`:105`), `XR_SHADERFLAGS_NOSTENCILTEST`
  (`XR/XRShader.h:156`). Character shadows commented out
  (`WModel_MultiTriMesh.cpp:438-450`) — only BSP2 world casts.

## 6. Gaps in our GLES3 backend

Confirmed in `RenderContexts/GLES3/MDisplaySDL2.cpp`:

- `Attrib_Lights` override — **not found** (same as PS3; not itself a bug). But
  `ApplyAttribs` (`:1308-1435`) never reads `m_pLights/m_nLights`,
  `CRC_FLAGS_LIGHTING`, `m_TexGen[]` (incl. `CRC_TEXGENMODE_LIGHTING*`, used by
  `XRUtilRS.cpp:128`), `m_TexEnvMode[]`, `m_RasterMode`, `m_FogDensity`, FP20
  ext-attributes — **all light channels ignored**.- Single UI shader `kGLES3_UIFragSrc` (`:33-102`): `vCol*tex0*tex1` + linear fog
  (`:97-100, 1668-1678`); no light math, no normal attribute.
- Normals dropped at fetch: `SUIVert` has none (`:1528`); `BuildVertsFromVBB`
  (`:2185-2260`) never fetches `CRC_VREG_NORMAL`; `VRegFetch` (`:2124-2174`) could
  decode it but is never asked.
- Virtuals PS3GCM overrides but GLES3 does not (render-relevant): `Geometry_Color`,
  real `Internal_RenderPolygon` (ours empty, `:354`), real `Render_Wire/Strip/Loop`,
  `RenderTarget_Copy/SetNextClearParams/SetEnableMasks`, `Attrib_GlobalUpdate`.
- Already working: per-pass blend/depth/stencil/scissor from `CRC_Attributes`
  (`:1315-1434`) — ONE/ONE additive and two-sided stencil map correctly, so shadow
  volumes will run once geometry feeds them.

**Minimal-Lambert implication:** cheapest first step is the PS3-style route —
fetch `CRC_VREG_NORMAL`, honor `CRC_FLAGS_LIGHTING` + `m_pLights` in
`ApplyAttribs`, add a vertex-lighting shader variant (2 vec4/light: pos+atten,
color). The FP20 per-light additive path (ext-attributes + texgen light vector) is
the larger second step — the one PC data actually uses.
