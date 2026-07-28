
/*
	SDL2 display context + GLES3 render context skeleton (phases 3-4).

	Current state: opens an SDL2 window with a GLES 3.0 context, clears
	and swaps on PageFlip. The CRC_GLES3 render methods are still stubs
	inherited from the NULL bring-up renderer; attrib translation, VBO
	streaming, textures and the shader generator land here next.
	If SDL/GL init fails (e.g. headless), it degrades to NULL behaviour.
*/

#include "PCH.h"

#include "../../MSystem/MSystem.h"
#include "../../MSystem/MSystem_Core.h"
#include "../../MSystem/Raster/MRCCore.h"
#include "../../MSystem/Raster/MDisplayPresent.h"
#include "../../MSystem/Raster/MTexture.h"
#include "../../MSystem/Raster/MTextureContainers.h"

#include "GLES3_Texture.h"
#include "GLES3_Shader.h"
#include "GLES3_VBOStreamer.h"
#include "GLES3_RTTOverlay.h"
#include "GLES3_Geometry.h"

#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <cmath>

// Two geometry programs share this vertex layout. m_UIShader (below,
// full feature set: lights/fog/alpha test/second UV) draws UI/2D;
// m_3DShader (kGLES3_3DVertSrc/FragSrc further down, minimal:
// diffuse + debug modes) draws world geometry. Selection happens
// per-draw in SetupCommonUniforms via IsUIDraw().
// Attributes: aPos (vec3 world), aUV (vec2), aUV1 (vec2),
// aCol (vec4, unpacked from CPixel32 BGRA), aNormal (vec3).
// Uniforms: uMVP (mat4), uUseTexture (bool), uTex (sampler2D).
// Max simultaneous per-vertex Lambert lights. CRC_MAXLIGHTS in the
// engine is 8; we mirror that. Each light needs 3 vec4s: pos+range,
// color+ambient scalar, direction+type (unused for POINT).
#define GLES3_MAX_LIGHTS 8

static const char* kGLES3_UIVertSrc =
	"#version 300 es\n"
	"layout(location=0) in vec3 aPos;\n"
	"layout(location=1) in vec2 aUV;\n"
	"layout(location=2) in vec4 aCol;\n"
	"layout(location=3) in vec2 aUV1;\n"
	"layout(location=4) in vec3 aNormal;\n"
	"uniform mat4 uMVP;\n"
	"uniform mat4 uModel;\n"
	"uniform mat4 uTexMat;\n"
	"uniform mat4 uTexMat1;\n"
	// TexGen (CRC_Attributes::m_lTexGenMode -> uTexGenMode0/1): 0 = UV
	// comes from the vertex register (aUV/aUV1, current behaviour), 1 =
	// LINEAR (uv = dot(modelspace pos, U/V) -- see PushTexGenUniforms).
	// Applied BEFORE the texture matrix, exactly like the vertex UV was.
	"uniform int uTexGenMode0;\n"
	"uniform int uTexGenMode1;\n"
	"uniform vec4 uTexGenU0;\n"
	"uniform vec4 uTexGenV0;\n"
	"uniform vec4 uTexGenU1;\n"
	"uniform vec4 uTexGenV1;\n"
	"out vec2 vUV;\n"
	"out vec2 vUV1;\n"
	"out vec4 vCol;\n"
	"out float vDepth;\n"
	"out vec3 vWorldPos;\n"
	"out vec3 vWorldNrm;\n"
	"out vec3 vNrmRaw;\n"
	"out vec3 vPosLocal;\n"
	"void main(){\n"
	"  vNrmRaw = aNormal;\n"
	"  vPosLocal = aPos;\n"
	"  gl_Position = uMVP * vec4(aPos, 1.0);\n"
	// Engine's projection puts NDC.z into [0..1] (D3D convention); GL
	// wants [-1..+1]. Remap: NDC.z_gl = 2*NDC.z_engine - 1, which in
	// clip space is clip.z_gl = 2*clip.z - clip.w. Without this the
	// entire depth range is compressed into [0.5..1] → nasty Z-fighting
	// on any surfaces at similar distance. UI/2D uses the same shader
	// but has z ≈ w so remap keeps it near far clip — no regression.
	"  gl_Position.z = 2.0 * gl_Position.z - gl_Position.w;\n"
	"  vec4 posH = vec4(aPos, 1.0);\n"
	"  vec2 uv0 = (uTexGenMode0 == 1) ? vec2(dot(posH, uTexGenU0), dot(posH, uTexGenV0)) : aUV;\n"
	"  vec2 uv1 = (uTexGenMode1 == 1) ? vec2(dot(posH, uTexGenU1), dot(posH, uTexGenV1)) : aUV1;\n"
	"  vUV = (uTexMat * vec4(uv0, 0.0, 1.0)).xy;\n"
	"  vUV1 = (uTexMat1 * vec4(uv1, 0.0, 1.0)).xy;\n"
	"  vDepth = gl_Position.w;\n"
	"  vCol = aCol;\n"
	// Row-vector convention: worldPos = v * Model. Same layout for GL.
	"  vWorldPos = (uModel * vec4(aPos, 1.0)).xyz;\n"
	"  vWorldNrm = normalize(mat3(uModel) * aNormal);\n"
	"}\n";

// --- Legacy UI fragment shader -------------------------------------------
// Kept for history/reference only (superseded by the minimal UI shader
// below). This was the original everything-shader: dynamic Lambert lights,
// fog, alpha test, second UV channel, 8 debug modes. To A/B against the
// new UI shader, flip the #if and rebuild.
#if 0
static const char* kGLES3_UIFragSrc_Legacy =
	"#version 300 es\n"
	"precision mediump float;\n"
	"in vec2 vUV;\n"
	"in vec2 vUV1;\n"
	"in vec4 vCol;\n"
	"in vec3 vWorldPos;\n"
	"in vec3 vWorldNrm;\n"
	"uniform sampler2D uTex;\n"
	"uniform int uUseTexture;\n"
	// Secondary texture (channel 1: lightmaps etc.) -- modulates RGB.
	"uniform sampler2D uTex1;\n"
	"uniform int uUseTexture1;\n"
	// Debug modes for RIDDICK_DBG_SHADER:
	//   0=off  1=uv (vUV.xy as RG)  2=pos (solid red)  3=no_tex (vCol only)
	//   4=normal (world-normal as RGB, (N+1)*0.5)
	//   5=nrm_raw (RAW aNormal, no model transform/normalize -- proves
	//              per-vertex attribute plumbing regardless of uModel)
	//   6=pos_local (aPos/scale as RGB -- proves attr location=0 varies)
	//   7=tex_only (raw texture(uTex, vUV); no vCol/lighting/fog/alpha; magenta
	//               if uUseTexture is off -- proves diffuse decode/binding)
	"uniform int uDbgMode;\n"
	"in vec3 vNrmRaw;\n"
	"in vec3 vPosLocal;\n"
	// Alpha test: CRC_COMPARE_* code (1=never..8=always, 0=off) + ref
	"uniform int uAlphaFunc;\n"
	"uniform float uAlphaRef;\n"
	// Linear fog (CRC_FLAGS_FOG): mix to uFogColor by view depth
	"uniform int uFogEnable;\n"
	"uniform vec3 uFogColor;\n"
	"uniform float uFogStart;\n"
	"uniform float uFogEnd;\n"
	// Dynamic-light (Phase A: per-fragment Lambert; base-pass modulation).
	// uLightingMode: 0=off (UI/no-normal path). 1=modulate (base pass —
	// c.rgb *= ambient + sum(diffuse)). 2=additive (light-only pass —
	// c.rgb  = sum(diffuse); alpha unchanged, expected to be ONE/ONE-blended).
	"uniform int uLightingMode;\n"
	"uniform vec3 uAmbient;\n"
	"uniform int uNumLights;\n"
	"uniform vec4 uLightPos[8];\n"     // xyz + range
	"uniform vec4 uLightColor[8];\n"   // rgb + ambient scale
	"in float vDepth;\n"
	"out vec4 oColor;\n"
	"void main(){\n"
	"  if (uDbgMode == 1) { oColor = vec4(vUV.x, vUV.y, 0.5, 1.0); return; }\n"
	"  if (uDbgMode == 2) { oColor = vec4(1.0, 0.0, 0.0, 0.5); return; }\n"
	"  if (uDbgMode == 3) { oColor = vCol; return; }\n"
	"  if (uDbgMode == 4) { vec3 N = normalize(vWorldNrm); oColor = vec4(N * 0.5 + 0.5, 1.0); return; }\n"
	"  if (uDbgMode == 5) { oColor = vec4(vNrmRaw * 0.5 + 0.5, 1.0); return; }\n"
	"  if (uDbgMode == 6) { vec3 P = fract(vPosLocal * 0.01); oColor = vec4(P, 1.0); return; }\n"
	"  if (uDbgMode == 7) { oColor = (uUseTexture != 0) ? texture(uTex, vUV) : vec4(1.0, 0.0, 1.0, 1.0); return; }\n"
	"  if (uDbgMode == 8) { oColor = (uUseTexture != 0) ? textureLod(uTex, vUV, 0.0) : vec4(1.0, 0.0, 1.0, 1.0); return; }\n"
	"  vec4 c = vCol;\n"
	"  if (uUseTexture != 0) c *= texture(uTex, vUV);\n"
	"  if (uUseTexture1 != 0) c.rgb *= texture(uTex1, vUV1).rgb;\n"
	"  if (uLightingMode != 0) {\n"
	"    vec3 N = normalize(vWorldNrm);\n"
	"    vec3 lit = (uLightingMode == 1) ? uAmbient : vec3(0.0);\n"
	"    for (int i = 0; i < 8; ++i) {\n"
	"      if (i >= uNumLights) break;\n"
	"      vec3 D = uLightPos[i].xyz - vWorldPos;\n"
	"      float R = max(uLightPos[i].w, 1.0);\n"
	"      float d = length(D);\n"
	"      float a = clamp(1.0 - d / R, 0.0, 1.0);\n"
	"      a *= a;\n"                                  // quadratic falloff
	"      float NdL = max(dot(N, D / max(d, 1e-4)), 0.0);\n"
	"      lit += uLightColor[i].rgb * (NdL * a);\n"
	"    }\n"
	"    if (uLightingMode == 1) c.rgb *= lit;\n"
	"    else                    c.rgb  = lit;\n"      // additive pass
	"  }\n"
	"  if (uAlphaFunc != 0 && uAlphaFunc != 8) {\n"
	"    bool pass = true;\n"
	"    if      (uAlphaFunc == 1) pass = false;\n"
	"    else if (uAlphaFunc == 2) pass = (c.a <  uAlphaRef);\n"
	"    else if (uAlphaFunc == 3) pass = (c.a == uAlphaRef);\n"
	"    else if (uAlphaFunc == 4) pass = (c.a <= uAlphaRef);\n"
	"    else if (uAlphaFunc == 5) pass = (c.a >  uAlphaRef);\n"
	"    else if (uAlphaFunc == 6) pass = (c.a != uAlphaRef);\n"
	"    else if (uAlphaFunc == 7) pass = (c.a >= uAlphaRef);\n"
	"    if (!pass) discard;\n"
	"  }\n"
	"  if (uFogEnable != 0) {\n"
	"    float f = clamp((uFogEnd - vDepth) / max(uFogEnd - uFogStart, 0.001), 0.0, 1.0);\n"
	"    c.rgb = mix(uFogColor, c.rgb, f);\n"
	"  }\n"
	"  oColor = c;\n"
	"}\n";
#endif // 0 -- legacy UI fragment shader

// --- Minimal UI fragment shader -------------------------------------------
// For UI/2D draws only (world geometry goes through kGLES3_3DFragSrc).
// No lighting, no fog, no alpha test, no second UV channel: UI is
// textured vertex-coloured quads, alpha comes from blending state.
// vCol is REQUIRED (text/menu art carries authored per-vertex colours).
// Debug modes (RIDDICK_DBG_SHADERUI): 0=off, 1=uv, 2=pos (solid red).
// Other legacy enum values are ignored -> normal rendering.
static const char* kGLES3_UIFragSrc =
	"#version 300 es\n"
	"precision mediump float;\n"
	"in vec2 vUV;\n"
	"in vec4 vCol;\n"
	"uniform sampler2D uTex;\n"
	"uniform int uUseTexture;\n"
	"uniform int uDbgMode;\n"
	"out vec4 oColor;\n"
	"void main(){\n"
	"  if (uDbgMode == 1) { oColor = vec4(fract(vUV), 0.0, 1.0); return; }\n"
	"  if (uDbgMode == 2) { oColor = vec4(1.0, 0.0, 0.0, 0.5); return; }\n"
	"  vec4 c = vCol;\n"
	"  if (uUseTexture != 0) c *= texture(uTex, vUV);\n"
	"  oColor = c;\n"
	"}\n";

// --- Minimal 3D shader (m_3DShader). ------------------------------------
// Dedicated program for world geometry, switched in SetupCommonUniforms
// (UI draws keep the full m_UIShader path). Deliberately bare: no lights,
// no fog, no alpha test, no second UV channel -- geometry + diffuse only,
// plus debug visualisation modes.
// Same attribute locations as the UI shader (SetVertexAttribPointers is
// shared); aUV1/aCol at locations 2/3 are simply not consumed here.
static const char* kGLES3_3DVertSrc =
	"#version 300 es\n"
	"layout(location=0) in vec3 aPos;\n"
	"layout(location=1) in vec2 aUV;\n"
	"layout(location=2) in vec4 aCol;\n"
	"layout(location=4) in vec3 aNormal;\n"
	"uniform mat4 uMVP;\n"
	"uniform mat4 uModel;\n"
	"uniform mat4 uTexMat;\n"
	// TexGen channel 0 only -- this shader has a single UV channel (no
	// aUV1/vUV1), so there is no channel-1 slot to feed. See
	// kGLES3_UIVertSrc for the full comment on uTexGenMode0 semantics.
	"uniform int uTexGenMode0;\n"
	"uniform vec4 uTexGenU0;\n"
	"uniform vec4 uTexGenV0;\n"
	"out vec2 vUV;\n"
	"out vec4 vCol;\n"
	"out vec3 vWorldPos;\n"
	"out vec3 vWorldNrm;\n"
	"void main(){\n"
	"  gl_Position = uMVP * vec4(aPos, 1.0);\n"
	// Same NDC.z remap as the UI VS: engine projection produces [0..1]
	// (D3D convention), GL wants [-1..+1] (see kGLES3_UIVertSrc).
	"  gl_Position.z = 2.0 * gl_Position.z - gl_Position.w;\n"
	"  vec4 posH = vec4(aPos, 1.0);\n"
	"  vec2 uv0 = (uTexGenMode0 == 1) ? vec2(dot(posH, uTexGenU0), dot(posH, uTexGenV0)) : aUV;\n"
	"  vUV = (uTexMat * vec4(uv0, 0.0, 1.0)).xy;\n"
	"  vCol = aCol;\n"
	// Row-vector convention: worldPos = v * Model (same layout for GL).
	"  vWorldPos = (uModel * vec4(aPos, 1.0)).xyz;\n"
	"  vWorldNrm = mat3(uModel) * aNormal;\n"
	"}\n";

static const char* kGLES3_3DFragSrc =
	"#version 300 es\n"
	"precision mediump float;\n"
	"in vec2 vUV;\n"
	"in vec4 vCol;\n"
	"in vec3 vWorldPos;\n"
	"in vec3 vWorldNrm;\n"
	"uniform sampler2D uTex;\n"
	"uniform int uUseTexture;\n"
	// RIDDICK_NO_LIGHT=1: ignore the baked per-vertex ambient in vCol,
	// draw pure diffuse (host also whitens vCol, this is belt-and-braces
	// so the shader alone guarantees the behaviour).
	"uniform int uNoLight;\n"
	// Ambient floor for the baked vCol (env RIDDICK_AMBIENT_FLOOR, default
	// 0). Maps whose baked vertex ambient is ~0 (Pit had pitch-black
	// walls) render black without it -- the old everything-shader hid
	// this behind dynamic lights + a 0.2 floor. 0.2 reproduces the old
	// floor without any light processing.
	"uniform float uAmbientFloor;\n"
	// Debug modes (own enum, fed from m_Dbg3DShaderMode / RIDDICK_DBG_SHADER):
	//   0=off -> diffuse texture * vertex colour
	//   1=uv       (fract(vUV) as RG -- fract so tiled UVs stay readable)
	//   2=normal   (world normal as RGB, (N+1)*0.5)
	//   3=worldpos (fract(worldPos * 0.01) as RGB -- 100-unit repeat)
	"uniform int uDbgMode;\n"
	"out vec4 oColor;\n"
	"void main(){\n"
	"  if (uDbgMode == 1) { oColor = vec4(fract(vUV), 0.0, 1.0); return; }\n"
	"  if (uDbgMode == 2) { vec3 N = normalize(vWorldNrm); oColor = vec4(N * 0.5 + 0.5, 1.0); return; }\n"
	"  if (uDbgMode == 3) { oColor = vec4(fract(vWorldPos * 0.01), 1.0); return; }\n"
	// vCol carries the engine's baked per-vertex ambient; multiplying keeps
	// the scene's authored brightness. uNoLight wipes it for fullbright,
	// uAmbientFloor lifts it off zero for maps with black-baked ambient.
	"  vec3 bake = max(vCol.rgb, vec3(uAmbientFloor));\n"
	"  vec4 c = (uNoLight != 0) ? vec4(1.0) : vec4(bake, vCol.a);\n"
	"  if (uUseTexture != 0) c *= texture(uTex, vUV);\n"
	"  oColor = c;\n"
	"}\n";

// --- FP20 LFM shader (m_LFMShader). --------------------------------------
// First real fragment-program translation: XRShader_FP20_LFM (BSP2 baked
// lightmaps, directional/radiosity-normal-map light -- see
// Docs/HacksAndHooks.md "FP20 LFM program"). Channel/texcoord contract is
// XRShader_LightField.cpp:511-545; the math is Docs/FP_Reference.md §5.3,
// ported verbatim from shaders/HL_Shading/XRShader_BRDF3.fp:939-969.
// Shares the same vertex layout/attribute locations as m_UIShader/
// m_3DShader (SetVertexAttribPointersFromEntry is common to all three);
// aUV1 here is the LFM atlas UV (CRC_Attributes::m_iTexCoordSet[1]), NOT
// the lightmap-modulate second UV that the UI shader uses aUV1 for.
static const char* kGLES3_LFMVertSrc =
	"#version 300 es\n"
	"layout(location=0) in vec3 aPos;\n"
	"layout(location=1) in vec2 aUV;\n"
	"layout(location=2) in vec4 aCol;\n"
	"layout(location=3) in vec2 aUV1;\n"
	"layout(location=4) in vec3 aNormal;\n"
	"uniform mat4 uMVP;\n"
	"uniform mat4 uTexMat;\n"
	"out vec2 vUV;\n"
	"out vec2 vUVLFM;\n"
	"out vec4 vCol;\n"
	"void main(){\n"
	"  gl_Position = uMVP * vec4(aPos, 1.0);\n"
	// Same NDC.z remap as kGLES3_UIVertSrc/kGLES3_3DVertSrc -- engine
	// projection produces clip.z in [0..1] (D3D convention), GL wants
	// [-1..+1].
	"  gl_Position.z = 2.0 * gl_Position.z - gl_Position.w;\n"
	"  vUV = (uTexMat * vec4(aUV, 0.0, 1.0)).xy;\n"
	// LFM (lightmap-atlas) UV: engine's texcoord set 1. No texture-matrix
	// bias -- the engine doesn't drive one for this channel (this program
	// has no uTexMat1 at all, unlike the UI shader's second UV channel).
	"  vUVLFM = aUV1;\n"
	"  vCol = aCol;\n"
	"}\n";

static const char* kGLES3_LFMFragSrc =
	"#version 300 es\n"
	"precision mediump float;\n"
	"in vec2 vUV;\n"
	"in vec2 vUVLFM;\n"
	"in vec4 vCol;\n"
	"uniform sampler2D uTex;\n"
	"uniform int uUseTexture;\n"
	// Normal map (channel 2, sampled with the diffuse UV). Tangents are
	// not wired up from the engine yet, so even a decoded normal map is
	// treated as already being in the local basis this formula expects --
	// see the TODO further down. Absent -> flat (0,0,1): only the +Z LFM
	// basis (lfm3) contributes, a correct (if flat) degradation.
	"uniform sampler2D uNormalTex;\n"
	"uniform int uUseNormalMap;\n"
	"uniform int uNormalTwoCh;\n"
	// Four lightmap-cluster textures (CRC_Attributes::m_TextureID[10..13]
	// = LFM0..3). lfm1/lfm2/lfm3's alpha channels secretly carry the RGB
	// of the sixth (+Y) basis direction -- ported as-is from the engine's
	// FP20 program, see Docs/FP_Reference.md §5.3.
	"uniform sampler2D uLFM0;\n"
	"uniform sampler2D uLFM1;\n"
	"uniform sampler2D uLFM2;\n"
	"uniform sampler2D uLFM3;\n"
	// TODO: the original scale is `4.0 * lmIntensityScale * LFM_Scale.rgb`,
	// where lmIntensityScale is a per-vertex texcoord-set[4] scalar and
	// LFM_Scale a fragment-program parameter (CRC_ExtAttributes_
	// FragmentProgram20::m_pParams) -- neither is plumbed through yet.
	// This single uniform scalar (RIDDICK_LFM_SCALE, default 4.0) stands
	// in for the whole product until that's done.
	"uniform float uLFMScale;\n"
	// RIDDICK_DBG_LFM: 0=off, 1=show the lightmap UV (fract as RG),
	// 2=show the reconstructed baked colour without the diffuse multiply.
	"uniform int uDbgLFM;\n"
	"out vec4 oColor;\n"
	"void main(){\n"
	"  if (uDbgLFM == 1) { oColor = vec4(fract(vUVLFM), 0.0, 1.0); return; }\n"
	// Same two-channel (I8A8 / 3DC) normal decode as the NDS program:
	// X in .r, Y in .a, Z reconstructed. See uNormalTwoCh there.
	"  vec3 n_ts = vec3(0.0, 0.0, 1.0);\n"
	"  if (uUseNormalMap != 0) {\n"
	"    vec4 nt = texture(uNormalTex, vUV);\n"
	"    if (uNormalTwoCh != 0) {\n"
	"      vec2 nxy = vec2(nt.r, nt.a) * 2.0 - 1.0;\n"
	"      n_ts = vec3(nxy, sqrt(max(0.0, 1.0 - dot(nxy, nxy))));\n"
	"    } else n_ts = normalize(nt.xyz * 2.0 - 1.0);\n"
	"  }\n"
	"  vec4 lfm0 = texture(uLFM0, vUVLFM);\n"
	"  vec4 lfm1 = texture(uLFM1, vUVLFM);\n"
	"  vec4 lfm2 = texture(uLFM2, vUVLFM);\n"
	"  vec4 lfm3 = texture(uLFM3, vUVLFM);\n"
	"  vec3 lfm4 = vec3(lfm1.a, lfm2.a, lfm3.a);\n" // sixth (+Y) direction, hidden in alphas 1..3
	"  vec3 nSat0 = clamp( n_ts, 0.0, 1.0);\n"
	"  vec3 nSat1 = clamp(-n_ts, 0.0, 1.0);\n"
	"  vec3 lfmColor = lfm0.rgb * nSat0.r\n"   // +X
	"                + lfm1.rgb * nSat1.b\n"   // -Z (permutation intentional, matches original)
	"                + lfm2.rgb * nSat1.g\n"   // -Y
	"                + lfm3.rgb * nSat0.b\n"   // +Z
	"                + lfm4     * nSat0.g;\n"  // +Y
	"  lfmColor *= uLFMScale;\n"
	"  if (uDbgLFM == 2) { oColor = vec4(lfmColor, 1.0); return; }\n"
	"  vec4 diff = (uUseTexture != 0) ? texture(uTex, vUV) : vec4(1.0);\n"
	"  oColor = vec4(diff.rgb * lfmColor, diff.a);\n"
	"}\n";

// --- FP20 NDS shader (m_NDSShader). --------------------------------------
// XRShader_FP20_NDS: single dynamic light, additive pass (diffuse + normal
// map + Phong specular, no projection texture -- see XRShader_FP20_NDSP for
// the projected variant, not implemented). Channel/texcoord contract is
// CXR_VirtualAttributes_ShaderFP20_COREFBB (XRShader_FP20.cpp:77-289) +
// CXR_Shader::RenderShading_FP20_COREFBB (ibid:298-436): m_TextureID[0]=
// Diffuse, [2]=Normal(+Specular in alpha), m_iTexCoordSet[0]/[1]=Mapping,
// [2]/[3]=TangentU/TangentV. Texgen channel 1=MSPOS (trivial -- it is just
// the model-space vertex position, no extra vertex data needed), channels
// 3/4=TSLV (tangent-space light vector to the light / to the eye,
// respectively -- see DecodeTexGenChannels). Math ported verbatim from
// shaders/ARB_Fragment_Program/XRShader_SinglePass_Dst2_SpecNormal.fp
// (Docs/FP_Reference.md §4 gives the same formula in pseudo-GLSL, but see
// the self-shadow note below for one place where the assembly and that
// doc disagree -- the assembly wins). Needs the tangent basis (locations
// 5/6, see BindEntryAttrib) -- cache-path draws only, see
// CRC_GLES3::TrySetupNDSProgram and Docs/HacksAndHooks.md "RIDDICK_NDS".
static const char* kGLES3_NDSVertSrc =
	"#version 300 es\n"
	"layout(location=0) in vec3 aPos;\n"
	"layout(location=1) in vec2 aUV;\n"
	"layout(location=4) in vec3 aNormal;\n"
	"layout(location=5) in vec3 aTangentU;\n"
	"layout(location=6) in vec3 aTangentV;\n"
	"uniform mat4 uMVP;\n"
	"uniform mat4 uTexMat;\n"
	// TSLV texgen parameters for texcoord channels 3 (light) / 4 (eye) --
	// CRC_TEXGENMODE_TSLV, see DecodeTexGenChannels: xyz = source position
	// (model space), w = extra scalar scale (engine always sends 1/32.0
	// here, XRShader_FP20.cpp:363/366).
	"uniform vec4 uLightTS;\n"
	"uniform vec4 uEyeTS;\n"
	"out vec2 vUV;\n"
	// fragment.texcoord[3]/[4] of the original program (IPTSLV/IPTSEV) --
	// deliberately NOT normalized here (neither is the ARB source; it
	// normalizes in the fragment stage, see kGLES3_NDSFragSrc).
	"out vec3 vTSLV;\n"
	"out vec3 vTSEV;\n"
	// fragment.texcoord[1] (Animated model space pixel position, MSPOS
	// texgen) -- trivially the model-space vertex position itself.
	"out vec3 vPosMS;\n"
	"void main(){\n"
	"  gl_Position = uMVP * vec4(aPos, 1.0);\n"
	// Same D3D->GL NDC.z remap as every other program in this file (see
	// kGLES3_UIVertSrc for the full comment).
	"  gl_Position.z = 2.0 * gl_Position.z - gl_Position.w;\n"
	"  vUV = (uTexMat * vec4(aUV, 0.0, 1.0)).xy;\n"
	"  vPosMS = aPos;\n"
	// CRC_TEXGENMODE_TSLV (Docs/VP_Reference.md §3.1, shaders/VP.xrg:
	// 1565-1573): component order verified against the template's ARB
	// assembler, NOT the CRC_TEXGENMODE_TSLV enum comment (which claims
	// x=TangU -- the real code puts x=dot(N,L), y=dot(TV,L), z=dot(TU,L)).
	"  vec3 Lto = uLightTS.xyz - aPos;\n"
	"  vTSLV = vec3(dot(aNormal, Lto), dot(aTangentV, Lto), dot(aTangentU, Lto)) * uLightTS.w;\n"
	"  vec3 Eto = uEyeTS.xyz - aPos;\n"
	"  vTSEV = vec3(dot(aNormal, Eto), dot(aTangentV, Eto), dot(aTangentU, Eto)) * uEyeTS.w;\n"
	"}\n";

static const char* kGLES3_NDSFragSrc =
	"#version 300 es\n"
	"precision mediump float;\n"
	"in vec2 vUV;\n"
	"in vec3 vTSLV;\n"
	"in vec3 vTSEV;\n"
	"in vec3 vPosMS;\n"
	"uniform sampler2D uTex;\n"       // Diffuse (channel 0)
	"uniform int uUseTexture;\n"
	"uniform sampler2D uNormalTex;\n" // Normal+Specular(alpha) (channel 2)
	"uniform int uUseNormalMap;\n"
	"uniform int uNormalTwoCh;\n"
	// program.env[0..3] of the ARB program -- see CRC_GLES3::
	// TrySetupNDSProgram / the report for what each of the engine's 6
	// CRC_ExtAttributes_FragmentProgram20::m_pParams slots means; params
	// [4] (EyePos) and [5] (NoiseOffset) are NOT read by this program (the
	// ARB source has EyePosition commented out -- eye info arrives instead
	// via vTSEV, computed vertex-side from the SAME eye position through
	// the channel-4 TSLV texgen).
	"uniform vec4 uLightPos;\n"    // {x,y,z,-} model space
	"uniform vec4 uLightRange;\n"  // {1/R, R, 1/R^2, R^2}
	"uniform vec4 uLightColor;\n"  // {r,g,b,-} (0..2 HDR-ish range)
	"uniform vec4 uSpecColor;\n"   // {r,g,b,SpecPower}
	// RIDDICK_DBG_NDS: 0=off, 1=tslv (normalized light vector as RGB),
	// 2=normal (decoded normal-map normal as RGB), 3=diffuse term only,
	// 4=specular term only.
	"uniform int uDbgMode;\n"
	"out vec4 oColor;\n"
	"void main(){\n"
	"  vec4 diffuseTexel = (uUseTexture   != 0) ? texture(uTex,       vUV) : vec4(1.0);\n"
	"  vec4 normalTexel  = (uUseNormalMap != 0) ? texture(uNormalTex, vUV) : vec4(0.5, 0.5, 1.0, 1.0);\n"
	// Two-channel (I8A8 / 3DC) normal map: rebuild Z from X,Y.
	"  if (uNormalTwoCh != 0) {\n"
	"    vec2 nxy = vec2(normalTexel.r, normalTexel.a) * 2.0 - 1.0;\n"
	"    float nz = sqrt(max(0.0, 1.0 - dot(nxy, nxy)));\n"
	"    normalTexel = vec4(nxy * 0.5 + 0.5, nz * 0.5 + 0.5, 1.0);\n"
	"  }\n"
	// Attenuation -- ARB: SUB/DP3/MUL_SAT/ADD/MUL, i.e.
	// (1 - saturate(distSq/Range^2))^2 (matches Docs/FP_Reference.md §4.2).
	"  vec3 toLight = uLightPos.xyz - vPosMS;\n"
	"  float distSq = dot(toLight, toLight);\n"
	"  float attnLin = clamp(distSq * uLightRange.z, 0.0, 1.0);\n"
	"  float attn = 1.0 - attnLin;\n"
	"  attn = attn * attn;\n"
	// Normal map: bias/scale [0..1] -> [-1..1], normalize.
	"  vec3 N = normalize(normalTexel.rgb * 2.0 - 1.0);\n"
	// ARB normalizes both interpolated tangent-space vectors before use.
	"  vec3 L = normalize(vTSLV);\n"
	"  vec3 E = normalize(vTSEV);\n"
	// Reflection of E about N (ARB literally reflects the EYE vector, not
	// the light vector -- mathematically equivalent to the more familiar
	// reflect(-L,N)/dot(R,E) form because reflection about N is symmetric:
	// dot(L, reflect(E,N)) == dot(E, reflect(L,N)). Do not "fix" this.
	"  vec3 R = 2.0 * dot(N, E) * N - E;\n"
	// Self-shadowing: the ARB source uses the NORMALIZED L.x (i.e. after
	// the "Normalize TSLV" block), NOT the raw interpolated IPTSLV.x that
	// Docs/FP_Reference.md §4.2 describes -- verified against the .fp
	// instruction order (normalize happens first, self-shadow reads the
	// same TSLV register afterwards). Trust the assembly here.
	"  float selfShadow = clamp((0.25 + L.x) * 4.0, 0.0, 1.0);\n"
	"  attn *= selfShadow;\n"
	"  if (uDbgMode == 1) { oColor = vec4(L * 0.5 + 0.5, 1.0); return; }\n"
	"  if (uDbgMode == 2) { oColor = vec4(N * 0.5 + 0.5, 1.0); return; }\n"
	"  vec3 diffuse = uLightColor.rgb * diffuseTexel.rgb * 2.0 * clamp(dot(N, L), 0.0, 1.0);\n"
	"  if (uDbgMode == 3) { oColor = vec4(diffuse, 1.0); return; }\n"
	// Specular: Phong (R, not half-vector), spec mask from the normal
	// map's alpha channel (SpecNormal variant -- see FP header comment
	// "Texture2 = Normal+Specular map").
	"  float specDot = clamp(dot(L, R), 0.0, 1.0);\n"
	"  float specPow = pow(specDot, uSpecColor.a);\n"
	"  vec3 specular = uSpecColor.rgb * specPow * normalTexel.a;\n"
	"  if (uDbgMode == 4) { oColor = vec4(specular, 1.0); return; }\n"
	"  vec3 result = (diffuse + specular) * attn;\n"
	// Alpha is never written by this pass in the engine (Attrib_Disable
	// (CRC_FLAGS_ALPHAWRITE) in PrepareFrame, blend is ONE/ONE) -- the ARB
	// program's oCol.a ends up holding leftover scratch (2*dot(N,E)) from
	// the reflection calc, never an intentional alpha; 1.0 here is simpler
	// and equally inert.
	"  oColor = vec4(result, 1.0);\n"
	"}\n";

#ifdef PLATFORM_LINUX

#include <SDL.h>
#include <GLES3/gl3.h>

// --- CRC_* -> GL enum tables (M1). -------------------------------------
static GLenum GLES3_MapBlend(int _CRCBlend)
{
	switch (_CRCBlend)
	{
	case CRC_BLEND_ZERO:         return GL_ZERO;
	case CRC_BLEND_ONE:          return GL_ONE;
	case CRC_BLEND_SRCCOLOR:     return GL_SRC_COLOR;
	case CRC_BLEND_INVSRCCOLOR:  return GL_ONE_MINUS_SRC_COLOR;
	case CRC_BLEND_SRCALPHA:     return GL_SRC_ALPHA;
	case CRC_BLEND_INVSRCALPHA:  return GL_ONE_MINUS_SRC_ALPHA;
	case CRC_BLEND_DESTALPHA:    return GL_DST_ALPHA;
	case CRC_BLEND_INVDESTALPHA: return GL_ONE_MINUS_DST_ALPHA;
	case CRC_BLEND_DESTCOLOR:    return GL_DST_COLOR;
	case CRC_BLEND_INVDESTCOLOR: return GL_ONE_MINUS_DST_COLOR;
	case CRC_BLEND_SRCALPHASAT:  return GL_SRC_ALPHA_SATURATE;
	default:                     return GL_ONE;
	}
}

static GLenum GLES3_MapCompare(int _CRCCompare)
{
	switch (_CRCCompare)
	{
	case CRC_COMPARE_NEVER:        return GL_NEVER;
	case CRC_COMPARE_LESS:         return GL_LESS;
	case CRC_COMPARE_EQUAL:        return GL_EQUAL;
	case CRC_COMPARE_LESSEQUAL:    return GL_LEQUAL;
	case CRC_COMPARE_GREATER:      return GL_GREATER;
	case CRC_COMPARE_NOTEQUAL:     return GL_NOTEQUAL;
	case CRC_COMPARE_GREATEREQUAL: return GL_GEQUAL;
	case CRC_COMPARE_ALWAYS:       return GL_ALWAYS;
	default:                       return GL_LEQUAL;
	}
}

// Composite pass: fullscreen quad sampling the screen FBO with an
// optional 90-degree-step rotation (uRot = rotation/90).
static const char* kGLES3_CompVertSrc =
	"#version 300 es\n"
	"layout(location=0) in vec2 aPos;\n"
	"uniform int uRot;\n"
	"out vec2 vUV;\n"
	"void main(){\n"
	"  gl_Position = vec4(aPos, 0.0, 1.0);\n"
	"  vec2 uv = aPos * 0.5 + 0.5;\n"
	"  if      (uRot == 1) vUV = vec2(uv.y, 1.0 - uv.x);\n"
	"  else if (uRot == 2) vUV = vec2(1.0 - uv.x, 1.0 - uv.y);\n"
	"  else if (uRot == 3) vUV = vec2(1.0 - uv.y, uv.x);\n"
	"  else                vUV = uv;\n"
	"}\n";

static const char* kGLES3_CompFragSrc =
	"#version 300 es\n"
	"precision mediump float;\n"
	"in vec2 vUV;\n"
	"uniform sampler2D uTex;\n"
	"out vec4 oColor;\n"
	"void main(){ oColor = vec4(texture(uTex, vUV).rgb, 1.0); }\n";

// The active CRC_GLES3 instance (single renderer per process); lets
// CDisplayContextSDL2::PageFlip run the composite pass without a typed
// member (the class is nested below).
static void* g_pGLES3RCInst = 0;

// RIDDICK_DIRECT_RENDER=1 -- true direct mode: every engine pass draws
// straight into the window framebuffer (fb0). The screen FBO is never
// created and the PresentToWindow composite is skipped, so nothing
// stands between the geometry and the window. Engine-side the
// post-process chain (XREngine.cpp) and camera effects (WClientMod.cpp)
// are gated on the same flag, so the frame is: world+UI geometry ->
// fb0 -> SDL_GL_SwapWindow. Assumes RIDDICK_ROTATE=0 and
// RIDDICK_FBOSIZE == RIDDICK_WINSIZE (the defaults).
static bool GLES3_DirectRender()
{
	static int s = -1;
	if (s < 0)
	{
		const char* e = getenv("RIDDICK_DIRECT_RENDER");
		s = (e && *e && *e != '0') ? 1 : 0;
	}
	return s != 0;
}

// RIDDICK_NO_LIGHT=1 -- fullbright diagnostic. Every darkening path
// disabled at once: vCol forced to white (vertex-baked ambient wiped),
// uLightingMode=0 (dynamic lights skipped), uFogEnable=0. Fragment
// collapses to `c = texture(uTex,vUV)` -- no matter how dark the map's
// baked ambient is (Pit had it ~0 -> pitch-black walls).
static bool GLES3_NoLight()
{
	static int s = -1;
	if (s < 0)
	{
		const char* e = getenv("RIDDICK_NO_LIGHT");
		s = (e && *e && *e != '0') ? 1 : 0;
	}
	return s != 0;
}

// RIDDICK_NO_VBCACHE=1 -- disable the GPU-resident VBID geometry cache
// (GLES3_Geometry.*) and fall back to the old per-draw path: fetch the
// CRC_BuildVertexBuffer from the engine, scalar-convert every vertex via
// BuildVertsFromVBB, and stream the result through the transient VBO/IBO
// ring every single frame. Kept as an escape hatch while the cache is
// being verified; the two paths should be visually identical.
static bool GLES3_NoVBCache()
{
	static int s = -1;
	if (s < 0)
	{
		const char* e = getenv("RIDDICK_NO_VBCACHE");
		s = (e && *e && *e != '0') ? 1 : 0;
	}
	return s != 0;
}

// RIDDICK_NO_TEXGEN=1 -- force every TexGen channel to mode 0 (UV taken
// from the vertex register, i.e. today's behaviour) regardless of what
// CRC_Attributes::m_lTexGenMode says. A/B switch for the TexGen work
// below: verifies nothing regresses when TexGen decoding is compiled in
// but the engine's own texgen state (BSP2 depth-fog, projective lights,
// env maps) is ignored.
static bool GLES3_NoTexGen()
{
	static int s = -1;
	if (s < 0)
	{
		const char* e = getenv("RIDDICK_NO_TEXGEN");
		s = (e && *e && *e != '0') ? 1 : 0;
	}
	return s != 0;
}

// RIDDICK_FP20=<n> -- EXPERIMENTAL/DBG. Wakes up CXR_Shader's shader-mode
// queue, which is otherwise dead: CXR_Shader::PrepareFrame (XRShader.cpp:
// 1258-1305) only sets bits in ModesAvail for FRAGMENTPROGRAM20 if the caps
// flag is set AND both multitexture counts are >= 8; with the "honest" caps
// this backend advertises by default (2 units, no FP20 flag) ModesAvail
// stays 0, BitScanBwd32(0) == -1, m_ShaderMode == -1, and every
// switch(m_ShaderMode) in RenderShading* is a no-op. Does NOT draw anything
// new by itself -- see CRC_GLES3::Create (caps) and DbgLogFP20 (logging).
// Any non-empty value other than "0" enables it; see GLES3_FP20ModeOverride
// for what the value itself selects. Default (unset) behaviour is
// byte-for-byte unchanged.
static bool GLES3_FP20Enabled()
{
	static int s = -1;
	if (s < 0)
	{
		const char* e = getenv("RIDDICK_FP20");
		s = (e && *e && *e != '0') ? 1 : 0;
	}
	return s != 0;
}

// Raw numeric value of RIDDICK_FP20 (0 if unset, disabled, or non-numeric).
// A value >1 overrides the forward-FP20 default mode (XR_SHADERMODE_
// FRAGMENTPROGRAM20 = 5) with that literal XR_SHADERMODE_* number, so other
// modes can be probed without a rebuild; RIDDICK_FP20=1 keeps the default.
static int GLES3_FP20ModeOverride()
{
	static int s = -1;
	if (s < 0)
	{
		const char* e = getenv("RIDDICK_FP20");
		s = (e && *e) ? atoi(e) : 0;
		if (s < 0) s = 0;
	}
	return s;
}

// RIDDICK_NO_LFM=1 -- A/B switch for the XRShader_FP20_LFM program below
// (see kGLES3_LFMVertSrc/FragSrc, CRC_GLES3::TrySetupLFMProgram): forces
// SetupCommonUniforms to keep going through the old m_UIShader/m_3DShader
// selection even when the engine hands us a LFM ext-attrib draw with all
// four lightmap textures present. Default (unset) draws with the new
// program whenever it qualifies.
// LFM (baked-lightmap) program: OPT-IN, default OFF.
//
// The data is real -- BSP2 clusters do carry lightmap atlas pages (the
// texture names come through as LM0_0..LM0_3, 512x64) and the engine does
// queue XRShader_FP20_LFM passes for them. But our port has no tangent
// basis yet, so the shader takes n_ts straight from the normal map, which
// is sampled with the DIFFUSE uv -- and that tiles (observed v = -7.375 on
// the first vertex of a wall cluster). The basis weights then vary at the
// diffuse tiling frequency and the surface turns into high-frequency
// sparkle instead of smooth baked light (screenshot, 2026-07-27).
//
// Riddick lights the world dynamically anyway (see Docs/FP_Reference.md
// §4a), so this program is not on the critical path: keep the code for
// other content/engines, keep it out of the way by default. Enable with
// RIDDICK_LFM=1 once tangents are wired and the basis can be evaluated
// correctly. RIDDICK_NO_LFM=1 is still honoured as an explicit override
// for anyone who had it in a script.
// RIDDICK_ZEQ_LEQUAL=1 -- see the comment at its use in ApplyAttribs.
static bool GLES3_ZEqualToLEqual()
{
	static int s = -1;
	if (s < 0)
	{
		const char* e = getenv("RIDDICK_ZEQ_LEQUAL");
		s = (e && *e && *e != '0') ? 1 : 0;
	}
	return s != 0;
}

static bool GLES3_NoLFM()
{
	static int s = -1;
	if (s < 0)
	{
		const char* eOff = getenv("RIDDICK_NO_LFM");
		if (eOff && *eOff && *eOff != '0') { s = 1; return s != 0; }
		const char* eOn = getenv("RIDDICK_LFM");
		s = (eOn && *eOn && *eOn != '0') ? 0 : 1;   // default: disabled
	}
	return s != 0;
}

// RIDDICK_DBG_LFM=uv|lm -- debug output of the LFM program (only meaningful
// together with RIDDICK_LFM=1). 'uv' paints fract(vUVLFM) as RG, which shows
// whether the lightmap UV set is a sane per-cluster atlas range (smooth
// gradient) or the tiling diffuse UV (repeating ramps). 'lm' drops the
// diffuse multiply and shows the reconstructed baked colour alone.
static int GLES3_DbgLFMMode()
{
	static int s = -1;
	if (s < 0)
	{
		const char* e = getenv("RIDDICK_DBG_LFM");
		s = 0;
		if (e && *e)
		{
			if      (strcmp(e, "uv") == 0) s = 1;
			else if (strcmp(e, "lm") == 0) s = 2;
		}
	}
	return s;
}

// RIDDICK_LFM_SCALE=<f> -- brightness multiplier for the baked-lightmap
// contribution (kGLES3_LFMFragSrc's uLFMScale). Default 4.0, standing in
// for the engine's `4.0 * lmIntensityScale * LFM_Scale.rgb` (Docs/
// FP_Reference.md §5.3) with the per-vertex lmIntensityScale and the
// FP20 parameter LFM_Scale both folded to 1 -- see the TODO comment on
// kGLES3_LFMFragSrc for what that simplifies away.
static float GLES3_LFMScale()
{
	static float s = -1.0f;
	if (s < 0.0f)
	{
		const char* e = getenv("RIDDICK_LFM_SCALE");
		s = (e && *e) ? (float)atof(e) : 4.0f;
		if (s < 0.0f) s = 4.0f;
	}
	return s;
}

// RIDDICK_NDS=1 -- opt-in switch for the XRShader_FP20_NDS program (single
// dynamic light: diffuse + normal map + Phong specular, see
// CRC_GLES3::TrySetupNDSProgram / kGLES3_NDSVertSrc/FragSrc). Unlike LFM
// (default-on, explicit opt-out) this one defaults OFF: it is new code
// that needs the tangent-basis plumbing (locations 5/6, cache-path draws
// only) to have been exercised for real before flipping it on generally.
// Default (unset) behaviour is byte-for-byte unchanged.
static bool GLES3_NDSEnabled()
{
	static int s = -1;
	if (s < 0)
	{
		const char* e = getenv("RIDDICK_NDS");
		s = (e && *e && *e != '0') ? 1 : 0;
	}
	return s != 0;
}

// RIDDICK_DBG_NDS=tslv|normal|diffuse|spec -- debug output of the NDS
// program (only meaningful together with RIDDICK_NDS=1). 'tslv' shows the
// normalized tangent-space light vector as RGB, 'normal' the decoded
// normal-map normal, 'diffuse'/'spec' isolate one term of the lighting sum.
static int GLES3_DbgNDSMode()
{
	static int s = -1;
	if (s < 0)
	{
		const char* e = getenv("RIDDICK_DBG_NDS");
		s = 0;
		if (e && *e)
		{
			if      (strcmp(e, "tslv")    == 0) s = 1;
			else if (strcmp(e, "normal")  == 0) s = 2;
			else if (strcmp(e, "diffuse") == 0) s = 3;
			else if (strcmp(e, "spec")    == 0) s = 4;
		}
	}
	return s;
}

class CDisplayContextSDL2 : public CDisplayContext
{
protected:
	MRTC_DECLARE;
public:

	CImage m_Image;
	SDL_Window* m_pWindow;
	SDL_GLContext m_GLContext;
	// m_Width/m_Height is the LOGICAL (engine-visible, FBO) resolution;
	// the physical window is m_WinWidth x m_WinHeight. They differ when
	// -rotate 90/270 (swapped) or -fbosize is given.
	int m_Width, m_Height;
	int m_WinWidth, m_WinHeight;
	int m_Rotate;

	static bool ParseSizeArg(const char* _p, int& _W, int& _H)
	{
		if (!_p) return false;
		int w = 0, h = 0;
		if (sscanf(_p, "%dx%d", &w, &h) != 2 || w <= 0 || h <= 0) return false;
		_W = w; _H = h;
		return true;
	}

	CDisplayContextSDL2()
	{
		m_pWindow = NULL;
		m_GLContext = NULL;
		m_WinWidth = 1280;
		m_WinHeight = 720;
		ParseSizeArg(getenv("RIDDICK_WINSIZE"), m_WinWidth, m_WinHeight);
		m_Rotate = 0;
		if (const char* r = getenv("RIDDICK_ROTATE"))
		{
			const int v = atoi(r);
			if (v == 90 || v == 180 || v == 270) m_Rotate = v;
		}
		// Logical size defaults to the window size, swapped at 90/270.
		if (m_Rotate == 90 || m_Rotate == 270)
			{ m_Width = m_WinHeight; m_Height = m_WinWidth; }
		else
			{ m_Width = m_WinWidth; m_Height = m_WinHeight; }
		ParseSizeArg(getenv("RIDDICK_FBOSIZE"), m_Width, m_Height);

		g_RiddickPresent.m_Rotate = m_Rotate;
		g_RiddickPresent.m_WinW = m_WinWidth;  g_RiddickPresent.m_WinH = m_WinHeight;
		g_RiddickPresent.m_FBOW = m_Width;     g_RiddickPresent.m_FBOH = m_Height;

		m_Image.Create(m_Width, m_Height, IMAGE_FORMAT_BGRA8, IMAGE_MEM_IMAGE);
	}

	~CDisplayContextSDL2()
	{
		if (m_GLContext) SDL_GL_DeleteContext(m_GLContext);
		if (m_pWindow) SDL_DestroyWindow(m_pWindow);
	}

	virtual CPnt GetScreenSize(){return CPnt(m_Width, m_Height);};
	virtual CPnt GetMaxWindowSize(){return CPnt(m_Width, m_Height);};

	bool InitWindow()
	{
		if (m_pWindow)
			return true;
		if (SDL_InitSubSystem(SDL_INIT_VIDEO) != 0)
		{
			ConOutL(CStrF("(CDisplayContextSDL2) SDL_Init failed: %s - running headless", SDL_GetError()));
			return false;
		}
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
		SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
		SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
		SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
		m_pWindow = SDL_CreateWindow("OpenRiddick",
			SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
			m_WinWidth, m_WinHeight, SDL_WINDOW_OPENGL);
		if (!m_pWindow)
		{
			ConOutL(CStrF("(CDisplayContextSDL2) SDL_CreateWindow failed: %s - running headless", SDL_GetError()));
			return false;
		}
		m_GLContext = SDL_GL_CreateContext(m_pWindow);
		if (!m_GLContext)
		{
			ConOutL(CStrF("(CDisplayContextSDL2) SDL_GL_CreateContext failed: %s - running headless", SDL_GetError()));
			SDL_DestroyWindow(m_pWindow);
			m_pWindow = NULL;
			return false;
		}
		SDL_GL_SetSwapInterval(1);
		ConOutL(CStrF("(CDisplayContextSDL2) GL_VERSION: %s", (const char*)glGetString(GL_VERSION)));
		ConOutL(CStrF("(CDisplayContextSDL2) window %dx%d, logical (FBO) %dx%d, rotate %d",
			m_WinWidth, m_WinHeight, m_Width, m_Height, m_Rotate));
		return true;
	}

	virtual void Create()
	{
		CDisplayContext::Create();
		InitWindow();
	}

	virtual int PageFlip()
	{
		if (m_pWindow)
		{
			// SDL event pump lives in CInputContext_SDL2::Update now --
			// draining SDL_PollEvent here too would split events
			// between the two consumers.
			// Composite the screen FBO into the window (with rotation)
			// before swapping; see CRC_GLES3::PresentToWindow.
			if (g_pGLES3RCInst)
				((CRC_GLES3*)g_pGLES3RCInst)->PresentToWindow();
			SDL_GL_SwapWindow(m_pWindow);
			// The engine now drives its own clears via
			// CRC_GLES3::RenderTarget_Clear; the bring-up glClear here
			// used to hide unrendered frames but would fight the engine
			// once real drawing lands.
			if (m_spRenderContext)
				m_spRenderContext->DbgFramePrint();
		}
		return CDisplayContext::PageFlip();
	}

	virtual void SetMode(int nr)
	{
	}

	virtual void ModeList_Init()
	{
	}

	virtual int SpawnWindow(int _Flags = 0)
	{
		return 0;
	}

	virtual void DeleteWindow(int _iWnd)
	{
	}

	virtual void SelectWindow(int _iWnd)
	{
	}

	virtual void SetWindowPosition(int _iWnd, CRct _Rct)
	{
	}

	virtual void SetPalette(spCImagePalette _spPal)
	{
	}

	virtual CImage* GetFrameBuffer()
	{
		return &m_Image;
	}

	virtual void ClearFrameBuffer(int _Buffers = (CDC_CLEAR_COLOR | CDC_CLEAR_ZBUFFER | CDC_CLEAR_STENCIL), int _Color = 0)
	{
	}

	int GetMode()
	{
		return 0;
	}


	class CRC_GLES3 : public CRC_Core
	{
	public:

		CDisplayContextSDL2 *m_pDisplayContext;
		int m_iTC = -1;
		int m_iVBCtxRC = -1;


		void Internal_RenderPolygon(int _nV, const CVec3Dfp32* _pV, const CVec3Dfp32* _pN, const CVec4Dfp32* _pCol = NULL, const CVec4Dfp32* _pSpec = NULL, /*const fp32* _pFog = NULL,*/
									const CVec4Dfp32* _pTV0 = NULL, const CVec4Dfp32* _pTV1 = NULL, const CVec4Dfp32* _pTV2 = NULL, const CVec4Dfp32* _pTV3 = NULL, int _Color = 0xffffffff)
		{
		}

		// M1 cached GL state / matrix capture. Uploaded to shader
		// programs by M3 draw calls.
		CMat4Dfp32 m_ProjMat;
		CMat4Dfp32 m_ModelMat;
		CMat4Dfp32 m_TexMat[4];

		// M2: engine TextureID -> GLuint. Sparse; 0 means "not
		// uploaded yet"; the vector grows on first touch.
		TArray<GLuint> m_lGLTex;
		// Source CImage format per texture ID, captured at upload time.
		// Needed because the shaders must know whether a normal map is a
		// two-channel I8A8/3DC texture (X,Y stored, Z reconstructed) or a
		// plain RGB one -- see uNormalTwoCh in the NDS/LFM programs.
		TArray<uint32> m_lTexFmt;

		// 1x1 magenta placeholder handed back for texture IDs the
		// texture context can't produce (typically CTextureContainer_
		// Screen / RTT slots -- we don't render-to-texture yet, so
		// the engine samples "nothing"; give it a loud debug colour
		// so missing-RTT surfaces are obvious rather than invisible.
		GLuint m_PlaceholderTex;
		GLuint m_CheckerTex = 0;
		int    m_ForceTex   = -1;   // -1 = not yet queried from env

		// Bright 32x32 magenta/cyan checkerboard for RIDDICK_FORCE_TEX=1.
		// If world geometry is visible under FORCE_TEX but invisible under
		// DBG_SHADER=nrm_raw/pos_local, the problem is per-vertex attrib
		// plumbing, not draw submission. If invisible even under
		// FORCE_TEX, world draws are being discarded entirely (culled,
		// alpha, depth, or never reaching this shader).
		GLuint GetCheckerTex()
		{
			if (m_CheckerTex) return m_CheckerTex;
			const int N = 32;
			unsigned char* buf = (unsigned char*)malloc(N*N*4);
			for (int y = 0; y < N; ++y) for (int x = 0; x < N; ++x)
			{
				const bool c = ((x >> 2) ^ (y >> 2)) & 1;
				unsigned char* p = buf + (y*N + x)*4;
				p[0] = c ? 255 : 0;
				p[1] = c ? 0   : 255;
				p[2] = c ? 255 : 255;
				p[3] = 255;
			}
			glGenTextures(1, &m_CheckerTex);
			glBindTexture(GL_TEXTURE_2D, m_CheckerTex);
			glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, N, N, 0, GL_RGBA, GL_UNSIGNED_BYTE, buf);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,     GL_REPEAT);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,     GL_REPEAT);
			free(buf);
			return m_CheckerTex;
		}

		bool ForceTexEnabled()
		{
			if (m_ForceTex < 0)
			{
				const char* e = getenv("RIDDICK_FORCE_TEX");
				m_ForceTex = (e && *e && *e != '0') ? 1 : 0;
			}
			return m_ForceTex != 0;
		}

		GLuint GetPlaceholderTex()
		{
			if (m_PlaceholderTex) return m_PlaceholderTex;
			glGenTextures(1, &m_PlaceholderTex);
			glBindTexture(GL_TEXTURE_2D, m_PlaceholderTex);
			// RED, not magenta, so it visually distinguishes from
			// "no texture bound at all" (which shows vCol=white/black).
			// Bright MAGENTA (not red) so it doesn't blend into game
			// palette. Any full-screen magenta on screen = an RTT/texture
			// that our uploader failed on.
			const unsigned char Magenta[4] = { 255, 0, 255, 255 };
			glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0,
				GL_RGBA, GL_UNSIGNED_BYTE, Magenta);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,     GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,     GL_CLAMP_TO_EDGE);
			return m_PlaceholderTex;
		}

		// Render-to-texture support. The engine allocates a set of
		// texture IDs from CTextureContainer_Screen; those IDs have no
		// data on disk -- we're supposed to render into them, then the
		// engine samples them as normal textures for compositing.
		// One SFBOSlot per RTT texture ID: colorTex is what samplers
		// see, depthRbo is a bundled depth+stencil renderbuffer, fbo
		// binds them together.
		struct SFBOSlot
		{
			GLuint m_FBO;
			GLuint m_ColorTex;
			GLuint m_DepthRbo;
			int    m_Width, m_Height;
		};
		TArray<SFBOSlot> m_lFBO; // sparse, indexed by texture id

		SFBOSlot* GetFBOSlot(int _TextureID)
		{
			if (_TextureID <= 0) return 0;
			if (_TextureID >= m_lFBO.Len()) return 0;
			SFBOSlot* p = &m_lFBO[_TextureID];
			return p->m_FBO ? p : 0;
		}

		SFBOSlot* EnsureFBOFor(int _TextureID)
		{
			if (_TextureID <= 0 || !m_pTC) return 0;
			if (_TextureID >= m_lFBO.Len())
			{
				const int Old = m_lFBO.Len();
				m_lFBO.SetLen(_TextureID + 1);
				for (int i = Old; i < m_lFBO.Len(); ++i)
				{
					m_lFBO[i].m_FBO = 0; m_lFBO[i].m_ColorTex = 0;
					m_lFBO[i].m_DepthRbo = 0;
					m_lFBO[i].m_Width = m_lFBO[i].m_Height = 0;
				}
			}
			SFBOSlot& S = m_lFBO[_TextureID];
			if (S.m_FBO) return &S;

			CImage Desc;
			int nMips = 0;
			m_pTC->GetTextureDesc(_TextureID, &Desc, nMips);
			int W = Desc.GetWidth();
			int H = Desc.GetHeight();
			// Screen container may report zero if the engine hasn't
			// called PrepareFrame yet; fall back to the window size --
			// it's what the frontend composition wants anyway.
			if ((W <= 0 || H <= 0) && m_pDisplayContext)
			{
				W = m_pDisplayContext->m_Width;
				H = m_pDisplayContext->m_Height;
			}
			if (W <= 0 || H <= 0) return 0;

			glGenTextures(1, &S.m_ColorTex);
			glBindTexture(GL_TEXTURE_2D, S.m_ColorTex);
			glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, W, H, 0,
				GL_RGBA, GL_UNSIGNED_BYTE, 0);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,     GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,     GL_CLAMP_TO_EDGE);

			glGenRenderbuffers(1, &S.m_DepthRbo);
			glBindRenderbuffer(GL_RENDERBUFFER, S.m_DepthRbo);
			glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, W, H);
			glBindRenderbuffer(GL_RENDERBUFFER, 0);

			glGenFramebuffers(1, &S.m_FBO);
			glBindFramebuffer(GL_FRAMEBUFFER, S.m_FBO);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
				GL_TEXTURE_2D, S.m_ColorTex, 0);
			glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,
				GL_RENDERBUFFER, S.m_DepthRbo);
			GLenum Status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
			glBindFramebuffer(GL_FRAMEBUFFER, 0);
			if (Status != GL_FRAMEBUFFER_COMPLETE)
			{
				fprintf(stderr, "[GLES3-RTT] id=%d FBO incomplete 0x%x (%dx%d)\n",
					_TextureID, (unsigned)Status, W, H);
				glDeleteFramebuffers(1, &S.m_FBO);   S.m_FBO = 0;
				glDeleteRenderbuffers(1, &S.m_DepthRbo); S.m_DepthRbo = 0;
				glDeleteTextures(1, &S.m_ColorTex);  S.m_ColorTex = 0;
				return 0;
			}
			S.m_Width  = W;
			S.m_Height = H;
			// Zero-clear the color: fresh RTT contents are undefined per
			// spec (Mesa observed to return WHITE on Intel iGPU), and the
			// engine samples these textures BEFORE writing them (for the
			// menu-cube envmap: first draw = full-screen quad textured by
			// the capture, then CopyToTexture updates the capture). An
			// uninitialised white capture floods the whole backbuffer with
			// white on the first frame -- root cause of the "white cube"
			// menu bug 2026-07-21.
			GLint PrevDraw = 0;
			glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &PrevDraw);
			const GLboolean bScissor = glIsEnabled(GL_SCISSOR_TEST);
			if (bScissor) glDisable(GL_SCISSOR_TEST);
			glBindFramebuffer(GL_FRAMEBUFFER, S.m_FBO);
			// Force clear: bright green so we notice if it's WHAT gets
			// sampled by subsequent draws (screen turns green instead
			// of white). If we see WHITE the FBO isn't actually the
			// texture the shader samples.
			glClearColor(0.0f, 1.0f, 0.0f, 1.0f);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
			unsigned char Px[4] = { 0xaa, 0xaa, 0xaa, 0xaa };
			glReadPixels(0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, Px);
			GLenum Err = glGetError();
			fprintf(stderr,
				"[GLES3-RTT-VERIFY] id=%d after-clear px=(%u,%u,%u,%u) glErr=0x%x\n",
				_TextureID, Px[0], Px[1], Px[2], Px[3], (unsigned)Err);
			glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)PrevDraw);
			if (bScissor) glEnable(GL_SCISSOR_TEST);
			fprintf(stderr, "[GLES3-RTT] id=%d FBO ok %dx%d  colorTex=%u fbo=%u  CLEARED-TO-BLACK\n",
				_TextureID, W, H, S.m_ColorTex, S.m_FBO);
			fflush(stderr);
			return &S;
		}

		void ReleaseAllFBOs()
		{
			for (int i = 0; i < m_lFBO.Len(); ++i)
			{
				if (m_lFBO[i].m_FBO)       glDeleteFramebuffers(1,  &m_lFBO[i].m_FBO);
				if (m_lFBO[i].m_DepthRbo)  glDeleteRenderbuffers(1, &m_lFBO[i].m_DepthRbo);
				if (m_lFBO[i].m_ColorTex)  glDeleteTextures(1,      &m_lFBO[i].m_ColorTex);
				m_lFBO[i].m_FBO = m_lFBO[i].m_DepthRbo = m_lFBO[i].m_ColorTex = 0;
			}
		}

		// --- Phase 5: screen FBO + rotated composite ----------------
		// The engine renders every "backbuffer" pass into this FBO at
		// the logical resolution (display m_Width x m_Height); PageFlip
		// composites it into the physical window with the configured
		// rotation (g_RiddickPresent).
		GLuint m_ScreenFBO, m_ScreenColorTex, m_ScreenDepthRbo;
		int    m_ScreenW, m_ScreenH;
		bool   m_bScreenFBOFailed;
		CGLES3Shader m_CompShader;
		int    m_CompRotLoc, m_CompTexLoc;
		GLuint m_CompVBO;
		CGLES3RTTOverlay m_RTTOverlay;

		// Logical target height for top-left -> bottom-left Y flips
		// (scissor, clear rects, viewports). This is the FBO height,
		// not the window height -- except in DIRECT_RENDER, where the
		// bound target IS the window framebuffer.
		int ScreenH() const
		{
			if (!m_pDisplayContext) return 0;
			return GLES3_DirectRender() ? m_pDisplayContext->m_WinHeight
			                            : m_pDisplayContext->m_Height;
		}

		bool EnsureScreenFBO()
		{
			if (m_ScreenFBO) return true;
			if (m_bScreenFBOFailed || !m_pDisplayContext) return false;
			const int W = m_pDisplayContext->m_Width;
			const int H = m_pDisplayContext->m_Height;
			if (W <= 0 || H <= 0) return false;

			glGenTextures(1, &m_ScreenColorTex);
			glBindTexture(GL_TEXTURE_2D, m_ScreenColorTex);
			glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, W, H, 0,
				GL_RGBA, GL_UNSIGNED_BYTE, 0);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,     GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,     GL_CLAMP_TO_EDGE);

			glGenRenderbuffers(1, &m_ScreenDepthRbo);
			glBindRenderbuffer(GL_RENDERBUFFER, m_ScreenDepthRbo);
			glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, W, H);
			glBindRenderbuffer(GL_RENDERBUFFER, 0);

			glGenFramebuffers(1, &m_ScreenFBO);
			glBindFramebuffer(GL_FRAMEBUFFER, m_ScreenFBO);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
				GL_TEXTURE_2D, m_ScreenColorTex, 0);
			glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,
				GL_RENDERBUFFER, m_ScreenDepthRbo);
			const GLenum Status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
			glBindFramebuffer(GL_FRAMEBUFFER, 0);
			if (Status != GL_FRAMEBUFFER_COMPLETE)
			{
				fprintf(stderr, "[GLES3] screen FBO incomplete 0x%x (%dx%d) - rendering direct to window\n",
					(unsigned)Status, W, H);
				glDeleteFramebuffers(1, &m_ScreenFBO);      m_ScreenFBO = 0;
				glDeleteRenderbuffers(1, &m_ScreenDepthRbo); m_ScreenDepthRbo = 0;
				glDeleteTextures(1, &m_ScreenColorTex);     m_ScreenColorTex = 0;
				m_bScreenFBOFailed = true;
				return false;
			}
			m_ScreenW = W;
			m_ScreenH = H;
			fprintf(stderr, "[GLES3] screen FBO %dx%d ok (rotate %d)\n",
				W, H, g_RiddickPresent.m_Rotate);
			fflush(stderr);
			return true;
		}

		void ReleaseScreenFBO()
		{
			if (m_ScreenFBO)      { glDeleteFramebuffers(1,  &m_ScreenFBO);      m_ScreenFBO = 0; }
			if (m_ScreenDepthRbo) { glDeleteRenderbuffers(1, &m_ScreenDepthRbo); m_ScreenDepthRbo = 0; }
			if (m_ScreenColorTex) { glDeleteTextures(1,      &m_ScreenColorTex); m_ScreenColorTex = 0; }
			if (m_CompVBO)        { glDeleteBuffers(1,       &m_CompVBO);        m_CompVBO = 0; }
		}

		// Bind the engine's notion of "the backbuffer": in DIRECT_RENDER
		// that is the real window framebuffer (fb0); otherwise the
		// screen FBO when available, else fb0 as fallback.
		bool m_bRTTActive;

		void BindScreenTarget()
		{
			m_bRTTActive = false;
			if (GLES3_DirectRender())
			{
				glBindFramebuffer(GL_FRAMEBUFFER, 0);
				if (m_pDisplayContext)
					glViewport(0, 0, m_pDisplayContext->m_WinWidth, m_pDisplayContext->m_WinHeight);
				return;
			}
			if (EnsureScreenFBO())
			{
				glBindFramebuffer(GL_FRAMEBUFFER, m_ScreenFBO);
				glViewport(0, 0, m_ScreenW, m_ScreenH);
				return;
			}
			glBindFramebuffer(GL_FRAMEBUFFER, 0);
			if (m_pDisplayContext)
				glViewport(0, 0, m_pDisplayContext->m_WinWidth, m_pDisplayContext->m_WinHeight);
		}

		// Called from CDisplayContextSDL2::PageFlip right before
		// SDL_GL_SwapWindow: draw the screen FBO into the window with
		// the configured rotation. Leaves the screen FBO bound again so
		// any engine draws issued before the next SetRenderTarget still
		// land in the right place.
		void PresentToWindow()
		{
			// DIRECT_RENDER: geometry went straight to fb0 this frame --
			// there is no screen FBO to composite, nothing to do.
			if (GLES3_DirectRender()) return;
			if (!m_ScreenFBO || !m_pDisplayContext) return;
			InitGLResources();
			if (!m_CompShader.IsValid()) return;

			glBindFramebuffer(GL_FRAMEBUFFER, 0);
			glViewport(0, 0, m_pDisplayContext->m_WinWidth, m_pDisplayContext->m_WinHeight);
			glDisable(GL_DEPTH_TEST);
			glDisable(GL_STENCIL_TEST);
			glDisable(GL_SCISSOR_TEST);
			glDisable(GL_BLEND);
			glDisable(GL_CULL_FACE);
			glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

			m_CompShader.Use();
			glUniform1i(m_CompRotLoc, (g_RiddickPresent.m_Rotate / 90) & 3);
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, m_ScreenColorTex);
			glUniform1i(m_CompTexLoc, 0);

			glBindVertexArray(m_VAO);
			glBindBuffer(GL_ARRAY_BUFFER, m_CompVBO);
			glEnableVertexAttribArray(0);
			glDisableVertexAttribArray(1);
			glDisableVertexAttribArray(2);
			glDisableVertexAttribArray(3);
			glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), 0);
			glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

			// Debug: draw every live RTT target as a grid on top
			// (RIDDICK_DBG_RTT=1). Window framebuffer is still bound.
			if (m_RTTOverlay.Enabled())
			{
				CGLES3RTTOverlay::SEntry Entries[CGLES3RTTOverlay::MAX_ENTRIES];
				int nEntries = 0;
				for (int i = 0; i < m_lFBO.Len() && nEntries < CGLES3RTTOverlay::MAX_ENTRIES; ++i)
				{
					if (!m_lFBO[i].m_FBO) continue;
					Entries[nEntries].m_Tex   = m_lFBO[i].m_ColorTex;
					Entries[nEntries].m_TexID = i;
					Entries[nEntries].m_W     = m_lFBO[i].m_Width;
					Entries[nEntries].m_H     = m_lFBO[i].m_Height;
					++nEntries;
				}
				m_RTTOverlay.Render(m_pDisplayContext->m_WinWidth,
					m_pDisplayContext->m_WinHeight, Entries, nEntries);
			}

			// Restore the engine's render target for the next frame.
			glBindFramebuffer(GL_FRAMEBUFFER, m_ScreenFBO);
			glViewport(0, 0, m_ScreenW, m_ScreenH);
		}

		// M3: GL resources for the draw path. Initialised lazily on
		// first Render_* call (Create() may run before the SDL2 GL
		// context is current).
		CGLES3Shader      m_UIShader;
		// Second program for world geometry (kGLES3_3DVertSrc/FragSrc).
		// Selected per-draw in SetupCommonUniforms via IsUIDraw(); the UI
		// program above keeps the full feature set (lights/fog/alpha/UV1).
		CGLES3Shader      m_3DShader;
		int m_3DUMVPLoc = -1, m_3DUModelLoc = -1, m_3DUTexMatLoc = -1;
		int m_3DUTexLoc = -1, m_3DUUseTexLoc = -1, m_3DUDbgModeLoc = -1;
		int m_Dbg3DShaderMode = 0; // 0=off, 1=uv, 2=normal, 3=worldpos
		int m_3DUNoLightLoc = -1;
		int m_3DUAmbientFloorLoc = -1;
		// TexGen (channel 0 only -- the 3D shader has no second UV
		// channel). See PushTexGenUniforms.
		int m_3DUTexGenMode0Loc = -1;
		int m_3DUTexGenU0Loc = -1, m_3DUTexGenV0Loc = -1;

		// Third program: XRShader_FP20_LFM (baked lightmap, world geometry
		// only -- see kGLES3_LFMVertSrc/FragSrc and TrySetupLFMProgram,
		// selected ahead of the m_UIShader/m_3DShader pick in
		// SetupCommonUniforms). RIDDICK_NO_LFM=1 disables selection (A/B).
		CGLES3Shader m_LFMShader;
		int m_LFMUMVPLoc = -1, m_LFMUTexMatLoc = -1;
		int m_LFMUTexLoc = -1, m_LFMUUseTexLoc = -1;
		int m_LFMUNormalTexLoc = -1, m_LFMUUseNormalLoc = -1, m_LFMUNormalTwoChLoc = -1;
		int m_LFMULFM0Loc = -1, m_LFMULFM1Loc = -1, m_LFMULFM2Loc = -1, m_LFMULFM3Loc = -1;
		int m_LFMUScaleLoc = -1;
		int m_LFMUDbgLoc = -1;
		// One-shot session log (see TrySetupLFMProgram) + per-interval draw
		// counter (reset alongside the other [GL-DBG] counters, printed as
		// "lfm=N").
		bool m_bDbgLFMLogged = false;
		int  m_DbgLFMLogged = 0;
		int  m_DbgLastLFM0 = -1;
		int  m_DbgLFMDraws = 0;

		// Fourth program: XRShader_FP20_NDS (single dynamic light: diffuse +
		// normal map + Phong specular, see kGLES3_NDSVertSrc/FragSrc and
		// TrySetupNDSProgram, selected ahead of LFM/UI/3D in
		// SetupCommonUniforms). RIDDICK_NDS=1 opt-in (see GLES3_NDSEnabled).
		CGLES3Shader m_NDSShader;
		int m_NDSUMVPLoc = -1, m_NDSUTexMatLoc = -1;
		int m_NDSULightTSLoc = -1, m_NDSUEyeTSLoc = -1;
		int m_NDSUTexLoc = -1, m_NDSUUseTexLoc = -1;
		int m_NDSUNormalTexLoc = -1, m_NDSUUseNormalLoc = -1, m_NDSUNormalTwoChLoc = -1;
		int m_NDSULightPosLoc = -1, m_NDSULightRangeLoc = -1;
		int m_NDSULightColorLoc = -1, m_NDSUSpecColorLoc = -1;
		int m_NDSUDbgLoc = -1;
		// One-shot session log (see TrySetupNDSProgram) + per-interval draw
		// counter (reset alongside the other [GL-DBG] counters, printed as
		// "nds=N").
		bool m_bDbgNDSLogged = false;
		int  m_DbgNDSDraws = 0;
		// Set by SetVertexAttribPointers/SetVertexAttribPointersFromEntry
		// (see BindEntryAttrib) to reflect whether locations 5/6 (tangent
		// basis) are bound to REAL per-vertex data for the draw about to
		// happen, as opposed to the axis-aligned constant fallback. The old
		// streaming path (SUIVert) never carries tangents at all (see
		// Docs/HacksAndHooks.md "RIDDICK_NDS"), so these are always false
		// there; TrySetupNDSProgram uses this as its qualification gate.
		bool m_bTangentUReal = false;
		bool m_bTangentVReal = false;

		int m_UTexMat1Loc = -1, m_UTex1Loc = -1, m_UUseTex1Loc = -1;
		int m_UTexMatLoc = -1, m_UAlphaFuncLoc = -1, m_UAlphaRefLoc = -1;
		int m_UFogEnableLoc = -1, m_UFogColorLoc = -1, m_UFogStartLoc = -1, m_UFogEndLoc = -1;
		int m_UModelLoc = -1;
		int m_ULightingModeLoc = -1, m_UAmbientLoc = -1, m_UNumLightsLoc = -1;
		int m_ULightPosLoc = -1, m_ULightColorLoc = -1;
		// TexGen (channels 0 and 1 -- the UI shader has a second UV
		// channel for lightmap-style modulation). See PushTexGenUniforms.
		int m_UTexGenMode0Loc = -1, m_UTexGenMode1Loc = -1;
		int m_UTexGenU0Loc = -1, m_UTexGenV0Loc = -1;
		int m_UTexGenU1Loc = -1, m_UTexGenV1Loc = -1;

		// Latest light state from Attrib_Lights (engine holds the array,
		// we just cache pointer + count until next Attrib_Set overrides).
		const CRC_Light* m_pRCLights = 0;
		int              m_nRCLights = 0;
		int m_DbgVBIDSkipFmt = 0;
		int m_DbgVBIDLastSkip = -1;
		CGLES3VBOStreamer m_Streamer;
		// Phase 4 M6: GPU-resident VBID cache (static VBO/IBO pairs),
		// see GLES3_Geometry.h. Falls back to m_Streamer (per-frame
		// scalar rebuild) for anything it can't handle -- skinned
		// meshes, exotic primitive types, or RIDDICK_NO_VBCACHE=1.
		CGLES3GeometryCache m_GeomCache;
		int m_DbgDrawCached = 0;   // draws served from m_GeomCache this interval
		int m_DbgDrawStreamed = 0; // draws that fell back to the old per-frame path
		long long m_DbgVConv = 0;  // vertices actually converted this interval
		long long m_DbgVMemo = 0;  // vertices whose conversion was skipped (cache/memo hit)

		// Memo for the CPU-array geometry path (m_Geom): the engine sets
		// one vertex array and then issues MANY draws against it (a whole
		// primitive stream per BSP2 cluster). Without this, every single
		// primitive re-converted the entire cluster -- measured at ~3M
		// vertex conversions per frame for ~81k indices actually drawn.
		// A memo entry says "this exact geometry is already sitting in the
		// streaming VBO at this offset". It is invalidated by (a) any
		// Geometry_VertexBuffer/Geometry_Clear call from the engine --
		// authoritative, since the VBM scratch heap can hand out the same
		// address for different content, (b) the streaming ring wrapping
		// (generation counter), (c) a change in the per-draw inputs that
		// affect conversion output (UV sets, fullbright whitening).
		struct SGeomMemo
		{
			bool        m_bValid = false;
			const void* m_pV = 0;
			const void* m_pTV0 = 0;
			const void* m_pTV1 = 0;
			const void* m_pCol = 0;
			const void* m_pN = 0;
			int         m_nV = 0;
			int         m_UVSet0 = 0, m_UVSet1 = 1;
			bool        m_bWhite = false;
			GLuint      m_Buffer = 0;
			int         m_ByteOffset = 0;
			int         m_Gen = -1;
		};
		SGeomMemo m_GeomMemo;
		// Scratch for flattening a whole primitive stream into one
		// triangle list (Render_IndexedPrimitives) -- a class member so
		// it is allocated once, not per draw.
		TArray<uint16> m_lFlatIdx;
		GLuint            m_VAO;
		bool              m_bGLInited;
		int               m_UMVPLoc;
		int               m_UUseTexLoc;
		int               m_UTexLoc;
		int               m_UDbgModeLoc;
		int               m_DbgShaderMode; // 0=off, 1=uv, 2=pos, 3=no_tex
		CRC_Attributes*   m_pCurAttrib;

		// Debug overrides via env vars (see DbgInit / SetupDbgOverrides).
		int  m_DbgNoDepth;   // RIDDICK_NO_DEPTH=1
		int  m_DbgNoCull;    // RIDDICK_NO_CULL=1
		int  m_DbgNoBlend;   // RIDDICK_NO_BLEND=1
		int  m_DbgNoAlpha;   // RIDDICK_NO_ALPHA=1  (disable alpha test in shader)
		int  m_DbgForceWire; // RIDDICK_FORCE_WIRE=1 (swap prim to GL_LINE_STRIP)

		// Frame dumper: on frame == m_DbgDumpFrameTarget (env RIDDICK_DUMP_FRAME=N)
		// write per-drawcall info to /tmp/openriddick_frame.txt then never again.
		int  m_DbgDumpFrameTarget;
		int  m_DbgDumpActive;    // 1 during the target frame
		int  m_DbgDumpDrawIdx;
		FILE* m_DbgDumpFp;
		// VBID → count histogram, populated in DbgDumpDraw during a dump.
		// Cleared per dump. Summary printed at end-of-dump; hot VBIDs
		// (count > 1) = same mesh drawn multiple times in one frame.
		TArray<unsigned> m_DbgDumpVBIDList;
		TArray<int>      m_DbgDumpVBIDCount;
		void DbgVBIDBumpCount(unsigned _VBID)
		{
			for (int i = 0; i < m_DbgDumpVBIDList.Len(); ++i)
			{
				if (m_DbgDumpVBIDList[i] == _VBID) { ++m_DbgDumpVBIDCount[i]; return; }
			}
			m_DbgDumpVBIDList.Add(_VBID);
			m_DbgDumpVBIDCount.Add(1);
		}
		void DbgDumpVBIDSummary()
		{
			if (!m_DbgDumpFp) return;
			int uniq = m_DbgDumpVBIDList.Len();
			int dup = 0, total = 0, maxN = 0;
			unsigned maxVBID = 0;
			for (int i = 0; i < uniq; ++i)
			{
				total += m_DbgDumpVBIDCount[i];
				if (m_DbgDumpVBIDCount[i] > 1) ++dup;
				if (m_DbgDumpVBIDCount[i] > maxN) { maxN = m_DbgDumpVBIDCount[i]; maxVBID = m_DbgDumpVBIDList[i]; }
			}
			fprintf(m_DbgDumpFp, "-- VBID summary: %d unique / %d total, %d drawn >1x, worst=VBID %u x%d --\n",
				uniq, total, dup, maxVBID, maxN);
			// List all VBIDs drawn more than once, sorted by count desc.
			// Simple selection-sort (list is short).
			TArray<int> lIdx; lIdx.SetLen(uniq);
			for (int i = 0; i < uniq; ++i) lIdx[i] = i;
			for (int i = 0; i < uniq - 1; ++i)
			{
				int best = i;
				for (int j = i + 1; j < uniq; ++j)
					if (m_DbgDumpVBIDCount[lIdx[j]] > m_DbgDumpVBIDCount[lIdx[best]]) best = j;
				if (best != i) { int t = lIdx[i]; lIdx[i] = lIdx[best]; lIdx[best] = t; }
			}
			for (int k = 0; k < uniq; ++k)
			{
				int i = lIdx[k];
				if (m_DbgDumpVBIDCount[i] <= 1) break;
				fprintf(m_DbgDumpFp, "  VBID %u x%d\n", m_DbgDumpVBIDList[i], m_DbgDumpVBIDCount[i]);
			}
			m_DbgDumpVBIDList.SetLen(0);
			m_DbgDumpVBIDCount.SetLen(0);
		}

		// Diagnostic per-frame counters. Enable with RIDDICK_DBG_GL=1;
		// prints one line every DBG_INTERVAL frames.
		enum { DBG_INTERVAL = 60 };
		int m_DbgEnabled;
		int m_DbgTexDumpsLeft;
		int m_DbgFrames;
		int m_DbgDrawTri, m_DbgDrawStrip, m_DbgDrawWire, m_DbgDrawPoly, m_DbgDrawPrim;
		int m_DbgDrawVBID, m_DbgTexBound, m_DbgTexMissing;
		int m_DbgTotalVerts, m_DbgTotalIdx;
		int m_DbgAttribSets, m_DbgMatrixSets, m_DbgBeginScenes;
		int m_DbgUploadRGBA, m_DbgUploadDXT1, m_DbgUploadDXT3, m_DbgUploadDXT5, m_DbgUploadFail;
		// RIDDICK_FP20 diagnostic: draws seen this interval whose current
		// attrib carries an FP20 ext-attrib (CRC_ATTRIBTYPE_FP20). See
		// DbgLogFP20. Reset with the other per-interval counters.
		int m_DbgFPDraws;
		// Session-lifetime (NOT per-interval) "already logged" cache of
		// distinct FP20 program hashes, so DbgLogFP20 prints each unique
		// program once regardless of how many frames it draws in.
		enum { DBG_FP_HASHCACHE = 64 };
		uint32 m_DbgFPHashSeen[DBG_FP_HASHCACHE];
		int m_DbgFPHashCount;

		static int DbgEnvFlag(const char* _Name)
		{
			const char* e = getenv(_Name);
			return (e && *e && *e != '0') ? 1 : 0;
		}
		void DbgInit()
		{
			m_DbgNoDepth   = DbgEnvFlag("RIDDICK_NO_DEPTH");
			m_DbgNoCull    = DbgEnvFlag("RIDDICK_NO_CULL");
			m_DbgNoBlend   = DbgEnvFlag("RIDDICK_NO_BLEND");
			m_DbgNoAlpha   = DbgEnvFlag("RIDDICK_NO_ALPHA");
			m_DbgForceWire = DbgEnvFlag("RIDDICK_FORCE_WIRE");
			m_DbgDumpFrameTarget = 0;
			if (const char* d = getenv("RIDDICK_DUMP_FRAME"))
			{
				int n = atoi(d);
				if (n > 0) m_DbgDumpFrameTarget = n;
			}
			m_DbgDumpActive = 0;
			m_DbgDumpDrawIdx = 0;
			m_DbgDumpFp = 0;
			m_DbgTotalFrames = 0;
			if (const char* d = getenv("RIDDICK_DUMP_OBJ"))
			{
				if (*d)
				{
					char cmd[512];
					snprintf(cmd, sizeof(cmd), "mkdir -p '%s'", d);
					if (system(cmd) == -1) { /* best-effort */ }
					m_DumpObjDir = d;
					m_DumpObjMax = 500;
					fprintf(stderr, "[GEOM-DUMP] enabled -> %s (cap %d files)\n",
						d, m_DumpObjMax);
				}
			}
			if (m_DbgNoDepth || m_DbgNoCull || m_DbgNoBlend || m_DbgNoAlpha ||
				m_DbgForceWire || m_DbgDumpFrameTarget > 0)
			{
				fprintf(stderr, "[GL-DBG] overrides: NoDepth=%d NoCull=%d NoBlend=%d "
					"NoAlpha=%d ForceWire=%d DumpFrame=%d\n",
					m_DbgNoDepth, m_DbgNoCull, m_DbgNoBlend, m_DbgNoAlpha,
					m_DbgForceWire, m_DbgDumpFrameTarget);
			}
			const char* e = getenv("RIDDICK_DBG_GL");
			m_DbgEnabled = (e && *e && *e != '0') ? 1 : 0;
			// One-shot: dump the first 30 draw-call attrib channel
			// arrays to see whether the engine ever uses texture
			// slots > 0 (multitexture).
			m_DbgTexDumpsLeft = m_DbgEnabled ? 30 : 0;
			m_DbgFrames = 0;
			// Session-lifetime, not per-interval -- see field comment.
			m_DbgFPHashCount = 0;
			DbgResetCounters();
		}
		void DbgResetCounters()
		{
			m_DbgDrawTri = m_DbgDrawStrip = m_DbgDrawWire = m_DbgDrawPoly = m_DbgDrawPrim = 0;
			m_DbgDrawVBID = m_DbgTexBound = m_DbgTexMissing = 0;
			m_DbgFPDraws = 0;
			m_DbgLFMDraws = 0;
			m_DbgNDSDraws = 0;
			m_DbgVBIDSkipFmt = 0;
			m_DbgDrawCached = m_DbgDrawStreamed = 0;
			m_DbgVConv = m_DbgVMemo = 0;
			m_DbgTotalVerts = m_DbgTotalIdx = 0;
			m_DbgAttribSets = m_DbgMatrixSets = m_DbgBeginScenes = 0;
			m_DbgUploadRGBA = m_DbgUploadDXT1 = m_DbgUploadDXT3 = m_DbgUploadDXT5 = m_DbgUploadFail = 0;
		}
		int m_DbgTotalFrames;
		void DbgDumpTick()
		{
			++m_DbgTotalFrames;
			// F9 → arm dump for the NEXT frame (this frame's drawcalls
			// already happened). Edge-detect so a long press only fires
			// once. Keyboard state is populated by SDL_PumpEvents which
			// CInputContext_SDL2 does every frame.
			static int s_PrevF9 = 0;
			int NumKeys = 0;
			const Uint8* pState = SDL_GetKeyboardState(&NumKeys);
			int F9 = (pState && NumKeys > SDL_SCANCODE_F9) ? pState[SDL_SCANCODE_F9] : 0;
			if (F9 && !s_PrevF9 && !m_DbgDumpActive && m_DbgDumpFrameTarget == 0)
			{
				m_DbgDumpFrameTarget = m_DbgTotalFrames + 1;
				fprintf(stderr, "[GL-DBG] F9: dump armed for frame %d\n",
					m_DbgDumpFrameTarget);
				// Also re-arm MTX/UV world-log windows so RIDDICK_DBG_MTX
				// captures the next 10 non-UI world draws around the dump.
				m_MtxLog = 0;
				m_UvLog = 0;
				fprintf(stderr, "[GL-DBG] F9: MTX log rearmed (next 10 world draws)\n");
			}
			s_PrevF9 = F9;
			// F10 → arm VBB register log for the next 32 draws.
			static int s_PrevF10 = 0;
			int F10 = (pState && NumKeys > SDL_SCANCODE_F10) ? pState[SDL_SCANCODE_F10] : 0;
			if (F10 && !s_PrevF10)
			{
				m_DbgVBBLogArm   = 32;
				m_DbgDrawLogArm  = 32;
				m_DbgDrawPostArm = 32;
				fprintf(stderr, "[GL-DBG] F10: VBB+DRAW logs armed (next 32 draws)\n");
			}
			s_PrevF10 = F10;
			if (m_DbgDumpActive)
			{
				// Finish: close and never open again.
				if (m_DbgDumpFp)
				{
					fprintf(m_DbgDumpFp, "-- end frame %d, %d drawcalls --\n",
						m_DbgTotalFrames - 1, m_DbgDumpDrawIdx);
					DbgDumpVBIDSummary();
					fclose(m_DbgDumpFp);
					m_DbgDumpFp = 0;
				}
				m_DbgDumpActive = 0;
				m_DbgDumpFrameTarget = 0;
				fprintf(stderr, "[GL-DBG] frame dump written to /tmp/openriddick_frame.txt\n");
			}
			else if (m_DbgDumpFrameTarget > 0 && m_DbgTotalFrames == m_DbgDumpFrameTarget)
			{
				m_DbgDumpFp = fopen("/tmp/openriddick_frame.txt", "w");
				if (m_DbgDumpFp)
				{
					fprintf(m_DbgDumpFp, "-- frame %d dump --\n", m_DbgTotalFrames);
					m_DbgDumpActive = 1;
					m_DbgDumpDrawIdx = 0;
				}
				else
				{
					fprintf(stderr, "[GL-DBG] failed to open /tmp/openriddick_frame.txt\n");
					m_DbgDumpFrameTarget = 0;
				}
			}
		}
		void DbgFramePrint()
		{
			DbgDumpTick();
			if (!m_DbgEnabled) return;
			++m_DbgFrames;
			if (m_DbgFrames < DBG_INTERVAL) return;
			// Snapshot + reset the global upload counters.
			m_DbgUploadRGBA = g_GLES3_UploadRGBA; g_GLES3_UploadRGBA = 0;
			m_DbgUploadDXT1 = g_GLES3_UploadDXT1; g_GLES3_UploadDXT1 = 0;
			m_DbgUploadDXT3 = g_GLES3_UploadDXT3; g_GLES3_UploadDXT3 = 0;
			m_DbgUploadDXT5 = g_GLES3_UploadDXT5; g_GLES3_UploadDXT5 = 0;
			m_DbgUploadFail = g_GLES3_UploadFail; g_GLES3_UploadFail = 0;
			fprintf(stderr,
				"[GL-DBG] %df: draw{tri=%d strip=%d wire=%d poly=%d prim=%d VBID=%d skip=%d lastFmt=%d fp20=%d lfm=%d nds=%d} "
				"verts=%d idx=%d texB=%d texMiss=%d attr=%d mat=%d beg=%d "
				"vbCache{cached=%d streamed=%d built=%d bytesV=%lld bytesI=%lld vconv=%lld vmemo=%lld} "
				"upl{rgba=%d dxt1=%d dxt3=%d dxt5=%d fail=%d}\n",
				m_DbgFrames, m_DbgDrawTri, m_DbgDrawStrip, m_DbgDrawWire,
				m_DbgDrawPoly, m_DbgDrawPrim, m_DbgDrawVBID,
				m_DbgVBIDSkipFmt, m_DbgVBIDLastSkip, m_DbgFPDraws, m_DbgLFMDraws, m_DbgNDSDraws,
				m_DbgTotalVerts, m_DbgTotalIdx, m_DbgTexBound, m_DbgTexMissing,
				m_DbgAttribSets, m_DbgMatrixSets, m_DbgBeginScenes,
				m_DbgDrawCached, m_DbgDrawStreamed, m_GeomCache.m_nBuilt,
				(long long)m_GeomCache.m_nBytesV, (long long)m_GeomCache.m_nBytesI,
				m_DbgVConv, m_DbgVMemo,
				m_DbgUploadRGBA, m_DbgUploadDXT1, m_DbgUploadDXT3, m_DbgUploadDXT5, m_DbgUploadFail);
			fflush(stderr);
			m_DbgFrames = 0;
			DbgResetCounters();
		}

		// RIDDICK_DBG_GL diagnostic for the RIDDICK_FP20 experiment: logs
		// each distinct FP20 fragment program the engine requests, exactly
		// once per m_ProgramNameHash (session lifetime, see m_DbgFPHashSeen).
		// Purely observational -- no FP20 program is compiled or executed by
		// this backend; the current draw still goes through m_UIShader/
		// m_3DShader same as always. Called from SetupCommonUniforms.
		void DbgLogFP20(const CRC_ExtAttributes_FragmentProgram20* _pFP)
		{
			if (!_pFP) return;
			const uint32 Hash = _pFP->m_ProgramNameHash;
			for (int i = 0; i < m_DbgFPHashCount; ++i)
				if (m_DbgFPHashSeen[i] == Hash) return; // already logged once
			if (m_DbgFPHashCount < DBG_FP_HASHCACHE)
				m_DbgFPHashSeen[m_DbgFPHashCount++] = Hash;
			// Cache full: keep logging anyway (duplicate lines are cheaper
			// than silently dropping later distinct programs).

			int Tex[8], TexGen[8];
			for (int i = 0; i < 8; ++i)
			{
				Tex[i]    = m_pCurAttrib ? (int)m_pCurAttrib->m_TextureID[i]   : 0;
				TexGen[i] = m_pCurAttrib ? (int)m_pCurAttrib->m_lTexGenMode[i] : 0;
			}
			const uint32 Flags = m_pCurAttrib ? m_pCurAttrib->m_Flags : 0;
			int Src = 0, Dst = 0;
			if (m_pCurAttrib)
			{
				// Same unpacking as ApplyAttribs' blend handling above
				// (little-endian MAKE_SOURCEDEST_BLEND: low byte = src).
				const uint16 SD = m_pCurAttrib->m_SourceDestBlend;
				Src = SD & 0xff;
				Dst = (SD >> 8) & 0xff;
			}
			// Channels 8..15 matter: the LFM program parks its four
			// lightmap pages in 10..13 (XRShader_LightField.cpp:542-545)
			// and the shading programs put noise textures in 14/15, so a
			// log that stops at channel 7 hides exactly the interesting
			// half of the attribute.
			int TexHi[8] = {0,0,0,0,0,0,0,0};
			for (int c = 8; c < 16 && c < CRC_MAXTEXTURES; ++c)
				TexHi[c - 8] = (int)m_pCurAttrib->m_TextureID[c];
			fprintf(stderr,
				"[GLES3-FP] prog='%s' hash=0x%08x nParams=%d "
				"tex=[%d %d %d %d %d %d %d %d] texHi=[%d %d %d %d %d %d %d %d] "
				"texgen=[%d %d %d %d %d %d %d %d] "
				"flags=0x%08x blend=%d/%d\n",
				_pFP->m_pProgramName ? _pFP->m_pProgramName : "?",
				Hash, _pFP->m_nParams,
				Tex[0], Tex[1], Tex[2], Tex[3], Tex[4], Tex[5], Tex[6], Tex[7],
				TexHi[0], TexHi[1], TexHi[2], TexHi[3], TexHi[4], TexHi[5], TexHi[6], TexHi[7],
				TexGen[0], TexGen[1], TexGen[2], TexGen[3], TexGen[4], TexGen[5], TexGen[6], TexGen[7],
				Flags, Src, Dst);
			int nP = _pFP->m_nParams;
			if (nP > 4) nP = 4;
			for (int p = 0; p < nP; ++p)
			{
				const CVec4Dfp32& V = _pFP->m_pParams[p];
				fprintf(stderr, "[GLES3-FP]   param[%d] = %f %f %f %f\n",
					p, V.k[0], V.k[1], V.k[2], V.k[3]);
			}
			fflush(stderr);
		}

		// Fallback reason log for TrySetupLFMProgram (item 6 of the task):
		// if any of the four LFM textures fails to qualify/upload we must
		// NOT draw black -- fall back to the old m_UIShader/m_3DShader path
		// and say why, once (capped small so a per-draw failure condition
		// can't flood stderr).
		void DbgLogLFMFallback(const char* _Reason)
		{
			static int sLogged = 0;
			if (sLogged >= 4) return;
			++sLogged;
			fprintf(stderr, "[GLES3-LFM] falling back to legacy shader: %s\n", _Reason);
			fflush(stderr);
		}

		// Selects and fully sets up m_LFMShader (XRShader_FP20_LFM, baked
		// BSP2 lightmap) for the current draw if it qualifies: returns true
		// when the program is bound and all uniforms/textures are applied,
		// in which case the caller (SetupCommonUniforms) must skip the
		// normal UI/3D setup for this draw. Returns false -- GL state left
		// untouched -- for every other draw, so the previous behaviour
		// (RIDDICK_FP20 diagnostics + m_UIShader/m_3DShader selection)
		// keeps working byte-for-byte when this doesn't apply or is
		// disabled via RIDDICK_NO_LFM=1.
		//
		// Channel/texcoord contract (XRShader_LightField.cpp:511-545):
		// m_TextureID[0]=Diffuse, [2]=Normal, [10..13]=LFM0..3;
		// m_iTexCoordSet[0]=Mapping (diffuse UV), [1]=LFM (lightmap UV).
		bool TrySetupLFMProgram()
		{
			if (GLES3_NoLFM() || !m_LFMShader.IsValid()) return false;
			if (!m_pCurAttrib || !m_pCurAttrib->m_pExtAttrib ||
			    m_pCurAttrib->m_pExtAttrib->m_AttribType != CRC_ATTRIBTYPE_FP20)
				return false;

			const CRC_ExtAttributes_FragmentProgram20* pFP =
				static_cast<const CRC_ExtAttributes_FragmentProgram20*>(m_pCurAttrib->m_pExtAttrib);

			// Cheap prefilter on the hash, then confirm by name. The hash
			// is computed once from our own literal via the engine's real
			// hash function (StringToHash == CStrBase::StrHash) rather than
			// a constant copied out of a log line -- that would silently
			// stop matching if the djb2 seed/algorithm or the program name
			// ever changed upstream.
			static const uint32 sLFMHash = StringToHash("XRShader_FP20_LFM");
			if (pFP->m_ProgramNameHash != sLFMHash) return false;
			if (!pFP->m_pProgramName || strcmp(pFP->m_pProgramName, "XRShader_FP20_LFM") != 0)
				return false;

			const int TexDiffuseID = (int)m_pCurAttrib->m_TextureID[0];
			const int TexNormalID  = (int)m_pCurAttrib->m_TextureID[2];
			const int TexLFM_ID[4] = {
				(int)m_pCurAttrib->m_TextureID[10], (int)m_pCurAttrib->m_TextureID[11],
				(int)m_pCurAttrib->m_TextureID[12], (int)m_pCurAttrib->m_TextureID[13],
			};
			if (!TexLFM_ID[0] || !TexLFM_ID[1] || !TexLFM_ID[2] || !TexLFM_ID[3])
			{
				DbgLogLFMFallback("CRC_Attributes::m_TextureID[10..13] not all populated");
				return false;
			}

			GLuint LFM[4];
			for (int i = 0; i < 4; ++i)
			{
				LFM[i] = TextureID_EnsureUploaded(TexLFM_ID[i]);
				if (!LFM[i])
				{
					DbgLogLFMFallback("one of LFM0..3 failed to upload (see [GLES3-TEX-FAIL] above)");
					return false;
				}
			}

			// Diffuse and normal are optional -- the formula degrades
			// gracefully without either (flat tangent-space normal, white
			// diffuse). Route through the shared upload helper so a
			// failure there behaves like everywhere else (placeholder
			// texture handed back, one-time [GLES3-TEX-FAIL] log) instead
			// of a second bespoke failure path.
			const GLuint TDiffuse = TexDiffuseID ? TextureID_EnsureUploaded(TexDiffuseID) : 0;
			const GLuint TNormal  = TexNormalID  ? TextureID_EnsureUploaded(TexNormalID)  : 0;

			// Log the first few DISTINCT lightmap sets, not just the very
			// first draw: a single sample turned out to be misleading --
			// it caught a draw whose channels 10..13 held ordinary material
			// textures (Console_02_*) rather than an LM* atlas page, which
			// is exactly the bug we are chasing (2026-07-27 log).
			if (m_DbgLFMLogged < 8 && m_DbgLastLFM0 != TexLFM_ID[0])
			{
				++m_DbgLFMLogged;
				m_DbgLastLFM0 = TexLFM_ID[0];
				m_bDbgLFMLogged = true;
				fprintf(stderr,
					"[GLES3-LFM] diffuse=%d normal=%d lfm=[%d %d %d %d] uvset0=%d uvset1=%d scale=%f\n",
					TexDiffuseID, TexNormalID,
					TexLFM_ID[0], TexLFM_ID[1], TexLFM_ID[2], TexLFM_ID[3],
					(int)m_pCurAttrib->m_iTexCoordSet[0], (int)m_pCurAttrib->m_iTexCoordSet[1],
					GLES3_LFMScale());
				fflush(stderr);
			}
			++m_DbgLFMDraws;

			m_LFMShader.Use();
			CMat4Dfp32 MVP;
			m_ModelMat.Multiply(m_ProjMat, MVP);
			m_LFMShader.SetMat4(m_LFMUMVPLoc, (const float*)&MVP);
			m_LFMShader.SetMat4(m_LFMUTexMatLoc, (const float*)&m_TexMat[0]);

			glActiveTexture(GL_TEXTURE0);
			if (TDiffuse) glBindTexture(GL_TEXTURE_2D, TDiffuse);
			m_LFMShader.SetInt(m_LFMUTexLoc, 0);
			m_LFMShader.SetInt(m_LFMUUseTexLoc, TDiffuse ? 1 : 0);

			glActiveTexture(GL_TEXTURE1);
			if (TNormal) glBindTexture(GL_TEXTURE_2D, TNormal);
			m_LFMShader.SetInt(m_LFMUNormalTexLoc, 1);
			m_LFMShader.SetInt(m_LFMUUseNormalLoc, TNormal ? 1 : 0);
			m_LFMShader.SetInt(m_LFMUNormalTwoChLoc, TextureID_IsTwoChannel(TexNormalID) ? 1 : 0);

			glActiveTexture(GL_TEXTURE2); glBindTexture(GL_TEXTURE_2D, LFM[0]);
			m_LFMShader.SetInt(m_LFMULFM0Loc, 2);
			glActiveTexture(GL_TEXTURE3); glBindTexture(GL_TEXTURE_2D, LFM[1]);
			m_LFMShader.SetInt(m_LFMULFM1Loc, 3);
			glActiveTexture(GL_TEXTURE4); glBindTexture(GL_TEXTURE_2D, LFM[2]);
			m_LFMShader.SetInt(m_LFMULFM2Loc, 4);
			glActiveTexture(GL_TEXTURE5); glBindTexture(GL_TEXTURE_2D, LFM[3]);
			m_LFMShader.SetInt(m_LFMULFM3Loc, 5);

			m_LFMShader.SetFloat(m_LFMUScaleLoc, GLES3_LFMScale());
			m_LFMShader.SetInt(m_LFMUDbgLoc, GLES3_DbgLFMMode());

			// Leave the active texture unit at 0, as convention elsewhere
			// in this file expects (see e.g. the end of the UI multitexture
			// bind block in SetupCommonUniforms).
			glActiveTexture(GL_TEXTURE0);
			return true;
		}

		// Shared texgen-channel walk: same loop/offset-advance rules as
		// PushTexGenUniforms (below, still the sole consumer of the LINEAR
		// outputs), factored out and extended so XRShader_FP20_NDS can pull
		// CRC_TEXGENMODE_TSLV parameters out of the same
		// m_pCurAttrib->m_pTexGenAttr blob without duplicating the walk.
		// _OutTSLV[i]/_OutHaveTSLV[i] receive the raw vec4 for every
		// channel whose mode is TSLV (GetTexGenModeAttribSize(TSLV,*) is
		// always 4 floats = 1 vec4, regardless of _TexGenComp -- see
		// Docs/VP_Reference.md §2.3/§3.1); every other channel's data is
		// still walked (to keep pAttr's offset correct for channels after
		// it) even when neither LINEAR nor TSLV, exactly like the original
		// PushTexGenUniforms loop did.
		void DecodeTexGenChannels(int& _Mode0, float _U0[4], float _V0[4],
		                          int& _Mode1, float _U1[4], float _V1[4],
		                          float _OutTSLV[CRC_MAXTEXCOORDS][4],
		                          bool _OutHaveTSLV[CRC_MAXTEXCOORDS])
		{
			_Mode0 = 0; _Mode1 = 0;
			for (int i = 0; i < 4; ++i) { _U0[i] = 0.0f; _V0[i] = 0.0f; _U1[i] = 0.0f; _V1[i] = 0.0f; }
			for (int i = 0; i < CRC_MAXTEXCOORDS; ++i) _OutHaveTSLV[i] = false;

			if (GLES3_NoTexGen() || !m_pCurAttrib || !m_pCurAttrib->m_pTexGenAttr)
				return;

			const fp32* pAttr = m_pCurAttrib->m_pTexGenAttr;
			for (int iTxt = 0; iTxt < CRC_MAXTEXCOORDS; ++iTxt)
			{
				const int RawMode = m_pCurAttrib->m_lTexGenMode[iTxt];
				const int Comp    = m_pCurAttrib->GetTexGenComp(iTxt);

				if (RawMode == CRC_TEXGENMODE_LINEAR)
				{
					float U[4] = {0,0,0,0}, V[4] = {0,0,0,0};
					const fp32* p = pAttr;
					if (Comp & CRC_TEXGENCOMP_U) { U[0]=p[0]; U[1]=p[1]; U[2]=p[2]; U[3]=p[3]; p += 4; }
					if (Comp & CRC_TEXGENCOMP_V) { V[0]=p[0]; V[1]=p[1]; V[2]=p[2]; V[3]=p[3]; p += 4; }
					if (iTxt == 0) { _Mode0 = 1; memcpy(_U0, U, sizeof(U)); memcpy(_V0, V, sizeof(V)); }
					else if (iTxt == 1) { _Mode1 = 1; memcpy(_U1, U, sizeof(U)); memcpy(_V1, V, sizeof(V)); }
				}
				else if (RawMode == CRC_TEXGENMODE_TSLV)
				{
					_OutTSLV[iTxt][0] = pAttr[0]; _OutTSLV[iTxt][1] = pAttr[1];
					_OutTSLV[iTxt][2] = pAttr[2]; _OutTSLV[iTxt][3] = pAttr[3];
					_OutHaveTSLV[iTxt] = true;
				}
				else if (RawMode != CRC_TEXGENMODE_TEXCOORD)
				{
					DbgNoteTexGenMode(RawMode, iTxt);
				}

				// Skip past this channel's attrib block regardless of
				// whether we decoded it -- offsets must stay correct for
				// channels after this one.
				pAttr += CRC_Attributes::GetTexGenModeAttribSize(RawMode, Comp);
			}
		}

		// Fallback reason log for TrySetupNDSProgram, same cap/pattern as
		// DbgLogLFMFallback.
		void DbgLogNDSFallback(const char* _Reason)
		{
			static int sLogged = 0;
			if (sLogged >= 4) return;
			++sLogged;
			fprintf(stderr, "[GLES3-NDS] falling back to legacy shader: %s\n", _Reason);
			fflush(stderr);
		}

		// Selects and fully sets up m_NDSShader (XRShader_FP20_NDS, single
		// dynamic light) for the current draw if it qualifies -- same
		// contract as TrySetupLFMProgram: returns true with the program
		// bound and every uniform/texture applied (caller must skip the
		// normal UI/3D setup), false with GL state untouched otherwise.
		//
		// Channel/texcoord contract (XRShader_FP20.cpp:77-436,
		// CXR_VirtualAttributes_ShaderFP20_COREFBB::OnSetAttributes +
		// CXR_Shader::RenderShading_FP20_COREFBB): m_TextureID[0]=Diffuse,
		// [2]=Normal(+Specular in alpha); m_iTexCoordSet[0]/[1]=Mapping,
		// [2]=TangentU, [3]=TangentV; texgen channel 1=MSPOS (trivial, see
		// vertex shader), channels 3/4=TSLV (light, eye).
		bool TrySetupNDSProgram()
		{
			if (!GLES3_NDSEnabled() || !m_NDSShader.IsValid()) return false;
			if (!m_pCurAttrib || !m_pCurAttrib->m_pExtAttrib ||
			    m_pCurAttrib->m_pExtAttrib->m_AttribType != CRC_ATTRIBTYPE_FP20)
				return false;

			const CRC_ExtAttributes_FragmentProgram20* pFP =
				static_cast<const CRC_ExtAttributes_FragmentProgram20*>(m_pCurAttrib->m_pExtAttrib);

			// Cheap prefilter on the hash (computed once from our own
			// literal via the engine's real hash function -- see
			// TrySetupLFMProgram for why not a copied-out constant), then
			// confirm by name.
			static const uint32 sNDSHash = StringToHash("XRShader_FP20_NDS");
			if (pFP->m_ProgramNameHash != sNDSHash) return false;
			if (!pFP->m_pProgramName || strcmp(pFP->m_pProgramName, "XRShader_FP20_NDS") != 0)
				return false;

			// Needs a real tangent basis at locations 5/6 (see
			// BindEntryAttrib / Docs/HacksAndHooks.md "RIDDICK_NDS") --
			// the old streaming path (SUIVert) never supplies one, and even
			// on the cache path the source geometry may not carry tangent
			// registers. Either way, drawing with a degenerate/placeholder
			// basis would light the surface with the wrong orientation --
			// fall back instead of showing that as if it were correct.
			if (!m_bTangentUReal || !m_bTangentVReal)
			{
				DbgLogNDSFallback("no real tangent basis bound for this draw "
					"(streamed path, or source geometry has no tangent registers)");
				return false;
			}

			const int TexNormalID = (int)m_pCurAttrib->m_TextureID[2];
			if (!TexNormalID)
			{
				DbgLogNDSFallback("CRC_Attributes::m_TextureID[2] (normal map) is 0");
				return false;
			}
			const GLuint TNormal = TextureID_EnsureUploaded(TexNormalID);
			if (!TNormal)
			{
				DbgLogNDSFallback("normal map failed to upload (see [GLES3-TEX-FAIL] above)");
				return false;
			}
			const int TexDiffuseID = (int)m_pCurAttrib->m_TextureID[0];
			const GLuint TDiffuse = TexDiffuseID ? TextureID_EnsureUploaded(TexDiffuseID) : 0;

			int Mode0, Mode1;
			float U0[4], V0[4], U1[4], V1[4];
			float TSLVParam[CRC_MAXTEXCOORDS][4];
			bool HaveTSLV[CRC_MAXTEXCOORDS];
			DecodeTexGenChannels(Mode0, U0, V0, Mode1, U1, V1, TSLVParam, HaveTSLV);
			(void)Mode0; (void)Mode1; (void)U0; (void)V0; (void)U1; (void)V1;
			if (!HaveTSLV[3] || !HaveTSLV[4])
			{
				DbgLogNDSFallback("no TSLV texgen on texcoord channel 3/4 (light/eye tangent-space vectors)");
				return false;
			}

			if (m_DbgEnabled && !m_bDbgNDSLogged)
			{
				m_bDbgNDSLogged = true;
				const int nP = pFP->m_nParams;
				fprintf(stderr,
					"[GLES3-NDS] diffuse=%d normal=%d uvset0=%d tuset=%d tvset=%d nParams=%d\n",
					TexDiffuseID, TexNormalID,
					(int)m_pCurAttrib->m_iTexCoordSet[0],
					(int)m_pCurAttrib->m_iTexCoordSet[2], (int)m_pCurAttrib->m_iTexCoordSet[3], nP);
				for (int p = 0; p < nP && p < 6; ++p)
				{
					const CVec4Dfp32& V = pFP->m_pParams[p];
					fprintf(stderr, "[GLES3-NDS]   param[%d] = %f %f %f %f\n", p, V.k[0], V.k[1], V.k[2], V.k[3]);
				}
				fflush(stderr);
			}
			++m_DbgNDSDraws;

			m_NDSShader.Use();
			CMat4Dfp32 MVP;
			m_ModelMat.Multiply(m_ProjMat, MVP);
			m_NDSShader.SetMat4(m_NDSUMVPLoc, (const float*)&MVP);
			m_NDSShader.SetMat4(m_NDSUTexMatLoc, (const float*)&m_TexMat[0]);
			m_NDSShader.SetVec4(m_NDSULightTSLoc, TSLVParam[3][0], TSLVParam[3][1], TSLVParam[3][2], TSLVParam[3][3]);
			m_NDSShader.SetVec4(m_NDSUEyeTSLoc,   TSLVParam[4][0], TSLVParam[4][1], TSLVParam[4][2], TSLVParam[4][3]);

			// program.env[0..3] -- see the report / class comment above for
			// what all 6 of the engine's m_pParams slots mean; [4]/[5] are
			// not read by this ARB program.
			if (pFP->m_nParams > 3)
			{
				const CVec4Dfp32& LightPos   = pFP->m_pParams[0];
				const CVec4Dfp32& LightRange = pFP->m_pParams[1];
				const CVec4Dfp32& LightColor = pFP->m_pParams[2];
				const CVec4Dfp32& SpecColor  = pFP->m_pParams[3];
				m_NDSShader.SetVec4(m_NDSULightPosLoc,   LightPos.k[0],   LightPos.k[1],   LightPos.k[2],   LightPos.k[3]);
				m_NDSShader.SetVec4(m_NDSULightRangeLoc, LightRange.k[0], LightRange.k[1], LightRange.k[2], LightRange.k[3]);
				m_NDSShader.SetVec4(m_NDSULightColorLoc, LightColor.k[0], LightColor.k[1], LightColor.k[2], LightColor.k[3]);
				m_NDSShader.SetVec4(m_NDSUSpecColorLoc,  SpecColor.k[0],  SpecColor.k[1],  SpecColor.k[2],  SpecColor.k[3]);
			}

			glActiveTexture(GL_TEXTURE0);
			if (TDiffuse) glBindTexture(GL_TEXTURE_2D, TDiffuse);
			m_NDSShader.SetInt(m_NDSUTexLoc, 0);
			m_NDSShader.SetInt(m_NDSUUseTexLoc, TDiffuse ? 1 : 0);

			glActiveTexture(GL_TEXTURE1);
			glBindTexture(GL_TEXTURE_2D, TNormal);
			m_NDSShader.SetInt(m_NDSUNormalTexLoc, 1);
			m_NDSShader.SetInt(m_NDSUUseNormalLoc, 1);
			m_NDSShader.SetInt(m_NDSUNormalTwoChLoc, TextureID_IsTwoChannel(TexNormalID) ? 1 : 0);

			m_NDSShader.SetInt(m_NDSUDbgLoc, GLES3_DbgNDSMode());

			// Same convention as TrySetupLFMProgram: leave the active unit
			// at 0.
			glActiveTexture(GL_TEXTURE0);
			return true;
		}

		CRC_GLES3()
			: m_VAO(0), m_bGLInited(false), m_UMVPLoc(-1),
			  m_UUseTexLoc(-1), m_UTexLoc(-1), m_UDbgModeLoc(-1),
			  m_DbgShaderMode(0), m_PlaceholderTex(0), m_pCurAttrib(0)
		{
			m_ScreenFBO = m_ScreenColorTex = m_ScreenDepthRbo = 0;
			m_ScreenW = m_ScreenH = 0;
			m_bScreenFBOFailed = false;
			m_CompRotLoc = m_CompTexLoc = -1;
			m_CompVBO = 0;
			m_bRTTActive = false;
			g_pGLES3RCInst = this;
			m_ProjMat.Unit();
			m_ModelMat.Unit();
			for (int i = 0; i < 4; ++i) m_TexMat[i].Unit();
			DbgInit();
			m_RTTOverlay.InitFromEnv();
			// RIDDICK_DBG_SHADERUI -- debug modes of the UI program only
			// (kGLES3_UIFragSrc enum).
			const char* e = getenv("RIDDICK_DBG_SHADERUI");
			if (e)
			{
				if      (strcmp(e, "uv")     == 0) m_DbgShaderMode = 1;
				else if (strcmp(e, "pos")    == 0) m_DbgShaderMode = 2;
				else if (strcmp(e, "no_tex") == 0) m_DbgShaderMode = 3;
				else if (strcmp(e, "normal") == 0) m_DbgShaderMode = 4;
				else if (strcmp(e, "nrm_raw")   == 0) m_DbgShaderMode = 5;
				else if (strcmp(e, "pos_local") == 0) m_DbgShaderMode = 6;
				else if (strcmp(e, "tex_only")  == 0) m_DbgShaderMode = 7;
				else if (strcmp(e, "tex_lod0")  == 0) m_DbgShaderMode = 8;
			}
			// RIDDICK_DBG_SHADER -- debug modes of the 3D program only
			// (kGLES3_3DFragSrc enum): 1=uv, 2=normal, 3=worldpos.
			const char* e3 = getenv("RIDDICK_DBG_SHADER");
			if (e3)
			{
				if      (strcmp(e3, "uv")       == 0) m_Dbg3DShaderMode = 1;
				else if (strcmp(e3, "normal")   == 0) m_Dbg3DShaderMode = 2;
				else if (strcmp(e3, "worldpos") == 0) m_Dbg3DShaderMode = 3;
			}
		}

		~CRC_GLES3()
		{
			if (g_pGLES3RCInst == this) g_pGLES3RCInst = 0;
			m_RTTOverlay.Destroy();
			Texture_ReleaseAll();
			m_GeomCache.DestroyAll();
			ReleaseAllFBOs();
			ReleaseScreenFBO();
			if (m_PlaceholderTex) { glDeleteTextures(1, &m_PlaceholderTex); m_PlaceholderTex = 0; }
			if (m_CheckerTex)     { glDeleteTextures(1, &m_CheckerTex);     m_CheckerTex     = 0; }
			if (m_VAO) { glDeleteVertexArrays(1, &m_VAO); m_VAO = 0; }
		}

		void InitGLResources()
		{
			if (m_bGLInited) return;
			m_bGLInited = true;
			m_Streamer.Create();
			if (m_UIShader.Build(kGLES3_UIVertSrc, kGLES3_UIFragSrc, "UI"))
			{
				m_UMVPLoc     = m_UIShader.UniformLocation("uMVP");
				m_UUseTexLoc  = m_UIShader.UniformLocation("uUseTexture");
				m_UTexLoc     = m_UIShader.UniformLocation("uTex");
				m_UDbgModeLoc   = m_UIShader.UniformLocation("uDbgMode");
				m_UTexMatLoc    = m_UIShader.UniformLocation("uTexMat");
				m_UTexMat1Loc   = m_UIShader.UniformLocation("uTexMat1");
				m_UTex1Loc      = m_UIShader.UniformLocation("uTex1");
				m_UUseTex1Loc   = m_UIShader.UniformLocation("uUseTexture1");
				m_UAlphaFuncLoc = m_UIShader.UniformLocation("uAlphaFunc");
				m_UAlphaRefLoc  = m_UIShader.UniformLocation("uAlphaRef");
				m_UFogEnableLoc = m_UIShader.UniformLocation("uFogEnable");
				m_UFogColorLoc  = m_UIShader.UniformLocation("uFogColor");
				m_UFogStartLoc  = m_UIShader.UniformLocation("uFogStart");
				m_UFogEndLoc    = m_UIShader.UniformLocation("uFogEnd");
				m_UModelLoc     = m_UIShader.UniformLocation("uModel");
				m_ULightingModeLoc = m_UIShader.UniformLocation("uLightingMode");
				m_UAmbientLoc      = m_UIShader.UniformLocation("uAmbient");
				m_UNumLightsLoc    = m_UIShader.UniformLocation("uNumLights");
				m_ULightPosLoc     = m_UIShader.UniformLocation("uLightPos[0]");
				m_ULightColorLoc   = m_UIShader.UniformLocation("uLightColor[0]");
				m_UTexGenMode0Loc  = m_UIShader.UniformLocation("uTexGenMode0");
				m_UTexGenMode1Loc  = m_UIShader.UniformLocation("uTexGenMode1");
				m_UTexGenU0Loc     = m_UIShader.UniformLocation("uTexGenU0");
				m_UTexGenV0Loc     = m_UIShader.UniformLocation("uTexGenV0");
				m_UTexGenU1Loc     = m_UIShader.UniformLocation("uTexGenU1");
				m_UTexGenV1Loc     = m_UIShader.UniformLocation("uTexGenV1");
			}

			// Minimal 3D program (world geometry). NOTE: previously this
			// was a second m_UIShader.Build(...) which Destroy()ed the UI
			// program built above -- it is now a separate CGLES3Shader.
			if (m_3DShader.Build(kGLES3_3DVertSrc, kGLES3_3DFragSrc, "3D"))
			{
				m_3DUMVPLoc     = m_3DShader.UniformLocation("uMVP");
				m_3DUModelLoc   = m_3DShader.UniformLocation("uModel");
				m_3DUTexMatLoc  = m_3DShader.UniformLocation("uTexMat");
				m_3DUTexLoc     = m_3DShader.UniformLocation("uTex");
				m_3DUUseTexLoc  = m_3DShader.UniformLocation("uUseTexture");
				m_3DUDbgModeLoc = m_3DShader.UniformLocation("uDbgMode");
				m_3DUNoLightLoc = m_3DShader.UniformLocation("uNoLight");
				m_3DUAmbientFloorLoc = m_3DShader.UniformLocation("uAmbientFloor");
				m_3DUTexGenMode0Loc = m_3DShader.UniformLocation("uTexGenMode0");
				m_3DUTexGenU0Loc    = m_3DShader.UniformLocation("uTexGenU0");
				m_3DUTexGenV0Loc    = m_3DShader.UniformLocation("uTexGenV0");
			}

			// FP20 LFM program (baked lightmap, world geometry -- see
			// TrySetupLFMProgram).
			if (m_LFMShader.Build(kGLES3_LFMVertSrc, kGLES3_LFMFragSrc, "LFM"))
			{
				m_LFMUMVPLoc       = m_LFMShader.UniformLocation("uMVP");
				m_LFMUTexMatLoc    = m_LFMShader.UniformLocation("uTexMat");
				m_LFMUTexLoc       = m_LFMShader.UniformLocation("uTex");
				m_LFMUUseTexLoc    = m_LFMShader.UniformLocation("uUseTexture");
				m_LFMUNormalTexLoc = m_LFMShader.UniformLocation("uNormalTex");
				m_LFMUUseNormalLoc = m_LFMShader.UniformLocation("uUseNormalMap");
				m_LFMUNormalTwoChLoc = m_LFMShader.UniformLocation("uNormalTwoCh");
				m_LFMULFM0Loc      = m_LFMShader.UniformLocation("uLFM0");
				m_LFMULFM1Loc      = m_LFMShader.UniformLocation("uLFM1");
				m_LFMULFM2Loc      = m_LFMShader.UniformLocation("uLFM2");
				m_LFMULFM3Loc      = m_LFMShader.UniformLocation("uLFM3");
				m_LFMUScaleLoc     = m_LFMShader.UniformLocation("uLFMScale");
				m_LFMUDbgLoc       = m_LFMShader.UniformLocation("uDbgLFM");
			}

			// FP20 NDS program (single dynamic light, world geometry -- see
			// TrySetupNDSProgram).
			if (m_NDSShader.Build(kGLES3_NDSVertSrc, kGLES3_NDSFragSrc, "NDS"))
			{
				m_NDSUMVPLoc        = m_NDSShader.UniformLocation("uMVP");
				m_NDSUTexMatLoc     = m_NDSShader.UniformLocation("uTexMat");
				m_NDSULightTSLoc    = m_NDSShader.UniformLocation("uLightTS");
				m_NDSUEyeTSLoc      = m_NDSShader.UniformLocation("uEyeTS");
				m_NDSUTexLoc        = m_NDSShader.UniformLocation("uTex");
				m_NDSUUseTexLoc     = m_NDSShader.UniformLocation("uUseTexture");
				m_NDSUNormalTexLoc  = m_NDSShader.UniformLocation("uNormalTex");
				m_NDSUUseNormalLoc  = m_NDSShader.UniformLocation("uUseNormalMap");
				m_NDSUNormalTwoChLoc = m_NDSShader.UniformLocation("uNormalTwoCh");
				m_NDSULightPosLoc   = m_NDSShader.UniformLocation("uLightPos");
				m_NDSULightRangeLoc = m_NDSShader.UniformLocation("uLightRange");
				m_NDSULightColorLoc = m_NDSShader.UniformLocation("uLightColor");
				m_NDSUSpecColorLoc  = m_NDSShader.UniformLocation("uSpecColor");
				m_NDSUDbgLoc        = m_NDSShader.UniformLocation("uDbgMode");
			}

			glGenVertexArrays(1, &m_VAO);
			if (m_CompShader.Build(kGLES3_CompVertSrc, kGLES3_CompFragSrc, "Composite"))
			{
				m_CompRotLoc = m_CompShader.UniformLocation("uRot");
				m_CompTexLoc = m_CompShader.UniformLocation("uTex");
			}
			static const float sQuad[8] = { -1,-1,  1,-1,  -1,1,  1,1 };
			glGenBuffers(1, &m_CompVBO);
			glBindBuffer(GL_ARRAY_BUFFER, m_CompVBO);
			glBufferData(GL_ARRAY_BUFFER, sizeof(sQuad), sQuad, GL_STATIC_DRAW);
			glBindBuffer(GL_ARRAY_BUFFER, 0);
		}

		// --- M2 texture path ---------------------------------------
		void Texture_ReleaseAll()
		{
			for (int i = 0; i < m_lGLTex.Len(); ++i)
			{
				if (m_lGLTex[i])
					glDeleteTextures(1, &m_lGLTex[i]);
				m_lGLTex[i] = 0;
			}
		}

		// Ensures engine textureID has been uploaded to a GLuint.
		// Returns 0 on failure (unknown ID, empty container, format
		// not yet supported by GLES3_Texture MapFormat).
		// Sparse "we already logged this failure" flags parallel to
		// m_lGLTex. Byte per texture ID: 0 = not touched, 1 =
		// upload attempted (success or failure). Prevents stderr
		// flood when the engine keeps re-requesting an unsupported
		// texture every frame.
		TArray<uint8> m_lTexLogged;

		// RIDDICK_DBG_GL census: log every distinct texture ID the
		// engine ever puts in a draw attrib, once, with its name.
		// Answers "does the engine even ask for this texture?".
		TArray<uint8> m_lTexSeenDbg;

		void DbgNoteTexID(int _TextureID, int _Channel)
		{
			if (!m_DbgEnabled || _TextureID <= 0) return;
			if (_TextureID >= m_lTexSeenDbg.Len())
			{
				const int Old = m_lTexSeenDbg.Len();
				m_lTexSeenDbg.SetLen(_TextureID + 1);
				for (int i = Old; i < m_lTexSeenDbg.Len(); ++i)
					m_lTexSeenDbg[i] = 0;
			}
			if (m_lTexSeenDbg[_TextureID]) return;
			m_lTexSeenDbg[_TextureID] = 1;
			fprintf(stderr, "[GL-TEXREQ] ch%d id=%d name='%s'\n", _Channel, _TextureID,
				m_pTC ? (const char*)m_pTC->GetName(_TextureID) : "?");
			fflush(stderr);
		}

		// True when the texture was uploaded from a two-channel I8A8 image
		// (MImage.h:63) -- the 3DC/BC5 normal-map layout the engine's own
		// decompressor produces. Valid only after the ID has been uploaded.
		bool TextureID_IsTwoChannel(int _TextureID) const
		{
			if (_TextureID < 0 || _TextureID >= m_lTexFmt.Len()) return false;
			return m_lTexFmt[_TextureID] == (uint32)IMAGE_FORMAT_I8A8;
		}

		GLuint TextureID_EnsureUploaded(int _TextureID)
		{
			if (_TextureID < 0 || !m_pTC) return 0;
			if (_TextureID >= m_lGLTex.Len())
			{
				const int Old = m_lGLTex.Len();
				m_lGLTex.SetLen(_TextureID + 1);
				for (int i = Old; i < m_lGLTex.Len(); ++i)
					m_lGLTex[i] = 0;
			}
			if (_TextureID >= m_lTexLogged.Len())
			{
				const int Old = m_lTexLogged.Len();
				m_lTexLogged.SetLen(_TextureID + 1);
				for (int i = Old; i < m_lTexLogged.Len(); ++i)
					m_lTexLogged[i] = 0;
			}
			if (m_lGLTex[_TextureID])
				return m_lGLTex[_TextureID];
			// If this ID is bound to an FBO (RTT slot), hand back its
			// color texture -- that's the "content" the engine expects
			// to sample after rendering to it.
			if (SFBOSlot* pSlot = GetFBOSlot(_TextureID))
				return pSlot->m_ColorTex;

			// Uploads need the GL context, which is current only on the
			// render thread; world precache calls us from loader threads
			// (glGenTextures returns 0 with glGetError==0 there). Defer:
			// the draw path retries on the GL thread. Confirmed by the
			// 2026-07-20 run log (~500 textures failed exactly this way).
			if (!SDL_GL_GetCurrentContext())
				return 0;

			// Note: file-backed containers (VirtualXTC) page the texel
			// data in inside GetTexture itself (Load(iLocal, iMip)), so
			// no extra force-load call is needed here.
			CImage* pImg = m_pTC->GetTexture(_TextureID, 0, -1);
			if (!pImg)
			{
				if (!m_lTexLogged[_TextureID])
				{
					m_lTexLogged[_TextureID] = 1;
					fprintf(stderr, "[GLES3-TEX-FAIL] id=%d  name='%s'  GetTexture()==NULL -> placeholder\n",
						_TextureID, (const char*)m_pTC->GetName(_TextureID));
					fflush(stderr);
				}
				// Shared magenta placeholder; NOT stored into m_lGLTex
				// (release path would double-free the shared GLuint).
				// No fail-latching: data may arrive later (async
				// loaders), so retry on the next request.
				return GetPlaceholderTex();
			}
			GLuint T = CGLES3TextureUploader::Upload2D(pImg, true);
			m_lGLTex[_TextureID] = T;
			if (_TextureID >= m_lTexFmt.Len())
			{
				const int OldF = m_lTexFmt.Len();
				m_lTexFmt.SetLen(_TextureID + 1);
				for (int i = OldF; i < m_lTexFmt.Len(); ++i)
					m_lTexFmt[i] = 0;
			}
			m_lTexFmt[_TextureID] = (uint32)pImg->GetFormat();
			const bool bLogged = m_lTexLogged[_TextureID] != 0;
			m_lTexLogged[_TextureID] = 1;
			if (T)
			{
				fprintf(stderr, "[GLES3-TEX-OK] id=%d  name='%s'  %dx%d  fmt=0x%x mem=0x%x\n",
					_TextureID, (const char*)m_pTC->GetName(_TextureID),
					pImg->GetWidth(), pImg->GetHeight(),
					(unsigned)pImg->GetFormat(), (unsigned)pImg->GetMemModel());
				fflush(stderr);
			}
			if (!T && !bLogged)
			{
				const int Fmt = pImg->GetFormat();
				const int Mem = pImg->GetMemModel();
				int S3TCSub = -1;
				if (pImg->IsCompressed() && (Mem & IMAGE_MEM_COMPRESSTYPE_S3TC))
				{
					unsigned char* pRaw = (unsigned char*)pImg->LockCompressed();
					if (pRaw)
						S3TCSub = (int)((const CImage_CompressHeader_S3TC*)pRaw)->getCompressType();
				}
				fprintf(stderr,
					"[GLES3-TEX-FAIL] id=%d  %dx%d  format=0x%x  memmodel=0x%x  s3tcSub=%d\n",
					_TextureID, pImg->GetWidth(), pImg->GetHeight(),
					(unsigned)Fmt, (unsigned)Mem, S3TCSub);
				fflush(stderr);
			}
			return T;
		}

		virtual void Texture_Precache(int _TextureID)
		{
			TextureID_EnsureUploaded(_TextureID);
		}

		virtual void Texture_Flush(int _TextureID)
		{
			if (_TextureID >= 0 && _TextureID < m_lGLTex.Len() && m_lGLTex[_TextureID])
			{
				glDeleteTextures(1, &m_lGLTex[_TextureID]);
				m_lGLTex[_TextureID] = 0;
			}
		}

		virtual void Texture_MakeAllDirty(int _iPicMip = -1)
		{
			Texture_ReleaseAll();
		}

		virtual void Texture_PrecacheFlush() {}
		virtual void Texture_PrecacheBegin(int _Count) {}
		virtual void Texture_PrecacheEnd() {}

		void Create(CObj* _pContext, const char* _pParams)
		{
			CRC_Core::Create(_pContext, _pParams);

			MACRO_GetSystem;

			m_Caps_TextureFormats = -1;
			m_Caps_DisplayFormats = -1;
			m_Caps_ZFormats = -1;
			m_Caps_StencilDepth = 8;
			m_Caps_AlphaDepth = 8;
			// Honest caps: the engine picks its render path from these.
			// Claiming everything (-1) routed the world through paths our
			// backend cannot execute: OCCLUSIONQUERY culling (our query
			// stubs report "0 pixels visible" -> whole world culled) and
			// FRAGMENTPROGRAM20/30 shader layers (XRShader.cpp:1268).
			// With a minimal set the engine falls back to the fixed-
			// function/texenv multipass path, which maps onto our
			// attrib-based GLES3 drawing.
			m_Caps_Flags = CRC_CAPS_FLAGS_HWAPI
			             | CRC_CAPS_FLAGS_ARBITRARY_TEXTURE_SIZE
			             | CRC_CAPS_FLAGS_SEPARATESTENCIL;

			// 2 texture units = the engine's most compatible multipass
			// path (base + lightmap per pass) -- exactly what the shader
			// in this backend implements (uTex/uTex1).
			m_Caps_nMultiTexture = 2;
			m_Caps_nMultiTextureCoords = 2;
			m_Caps_nMultiTextureEnv = 2;

			// RIDDICK_FP20 (EXPERIMENTAL/DBG, default off -- see
			// GLES3_FP20Enabled() above for the full rationale): opt in to
			// CRC_CAPS_FLAGS_FRAGMENTPROGRAM20 + 8 texture units/coords so
			// CXR_Shader::PrepareFrame's ModesAvail becomes non-zero and the
			// shader-mode queue starts running. CRC_MAXTEXTURES/CRC_MAXTEX-
			// COORDS on Linux are 16/8 (MRender_Classes_VPUShared.h:17-28),
			// so the arrays comfortably hold 8.
			if (GLES3_FP20Enabled())
			{
				m_Caps_Flags |= CRC_CAPS_FLAGS_FRAGMENTPROGRAM20;
				m_Caps_nMultiTexture = 8;
				m_Caps_nMultiTextureCoords = 8;
				// m_Caps_nMultiTextureEnv deliberately left at 2, not raised
				// to CRC_MAXTEXTUREENV(4): it governs the fixed-function
				// texenv-combiner path, and this backend's UI shader still
				// only implements 2 combine stages (uTex/uTex1). Claiming 4
				// here would tell the engine it can rely on stages we don't
				// execute -- same reasoning as the honest-caps comment above.
				// Deliberately NOT adding CRC_CAPS_FLAGS_MRT / FRAGMENTPROGRAM30
				// / COPYDEPTH: those would additionally unlock
				// XR_SHADERMODE_FRAGMENTPROGRAM20DEFMRT and _20SS in
				// ModesAvail (XRShader.cpp:1271-1278) -- deferred paths that
				// need an MRT G-buffer and FP20SS programs we don't have.

				// Pin the shader mode instead of leaving XR_SHADERMODE_AUTO:
				// with the FP20 cap on, ModesAvail now has both
				// XR_SHADERMODE_FRAGMENTPROGRAM20 (forward -- what the log
				// below can observe) and the derived deferred bit
				// XR_SHADERMODE_FRAGMENTPROGRAM20DEFMM (XRShader.cpp:
				// 1283-1284); AUTO's BitScanBwd32() picks the HIGHEST set
				// bit, i.e. the deferred mode, which needs a G-buffer this
				// backend does not build. PrepareFrame re-reads this every
				// frame via GetValuei("XR_SHADERMODE", ...), so writing it
				// once here at Create() time is enough.
				//
				// NOT SURE / please double-check: MACRO_GetSystemEnvironment
				// (MSystem.h:240-246) is used here as the NULL-safe wrapper
				// around CSystem lookup + GetEnvironment(); it already
				// no-ops (leaves pEnv == NULL) if "SYSTEM" isn't registered
				// yet at this point in bring-up. If Create() genuinely runs
				// before the system object is registered, this silently
				// skips pinning the mode and the engine stays on AUTO
				// (deferred) instead -- verify against a real run with
				// RIDDICK_FP20=1 if that matters.
				MACRO_GetSystemEnvironment(pEnv);
				if (pEnv)
				{
					// XR_SHADERMODE_FRAGMENTPROGRAM20 = 5 (enum in
					// XRShader.h:111-121). Deliberately not #including
					// XRShader.h from this display-context TU just for one
					// constant; mirrored locally instead.
					static const int kXRShaderMode_FP20Forward = 5;
					const int OverrideMode = GLES3_FP20ModeOverride();
					pEnv->SetValuei("XR_SHADERMODE",
						(OverrideMode > 1) ? OverrideMode : kXRShaderMode_FP20Forward);
				}
			}

			// Register with the texture/VB contexts (the PS3 backend does
			// this in its Create; CRC_Core::Create does not) so they can
			// track per-RC state and dirty-notify us.
			m_iTC = m_pTC->AddRenderContext(this);
			m_iVBCtxRC = m_pVBCtx->AddRenderContext(this);

			if (m_pVBCtx)
				m_GeomCache.Init(m_pVBCtx, this);
		}

		const char* GetRenderingStatus() { return ""; }
		virtual void Flip_SetInterval(int _nFrames){};

		// Render target: bind engine-requested FBO if any texture ID is
		// specified, otherwise fall back to the window backbuffer (fb0).
		void RenderTarget_SetRenderTarget(const CRC_RenderTargetDesc& _RenderTarget)
		{
			int TargetID = 0;
			for (int i = 0; i < 4 /*CRC_MAXMRT*/; ++i)
			{
				if (_RenderTarget.m_lColorTextureID[i])
				{
					TargetID = (int)_RenderTarget.m_lColorTextureID[i];
					break;
				}
			}
			if (m_DbgDumpActive && m_DbgDumpFp)
			{
				fprintf(m_DbgDumpFp,
					"---- RenderTarget_SetRenderTarget ids=[%u,%u,%u,%u] target=%d ----\n",
					(unsigned)_RenderTarget.m_lColorTextureID[0],
					(unsigned)_RenderTarget.m_lColorTextureID[1],
					(unsigned)_RenderTarget.m_lColorTextureID[2],
					(unsigned)_RenderTarget.m_lColorTextureID[3], TargetID);
			}
			if (m_DbgEnabled)
			{
				fprintf(stderr, "[GLES3-RT] SetRenderTarget ids=[%u,%u,%u,%u] -> %s\n",
					(unsigned)_RenderTarget.m_lColorTextureID[0],
					(unsigned)_RenderTarget.m_lColorTextureID[1],
					(unsigned)_RenderTarget.m_lColorTextureID[2],
					(unsigned)_RenderTarget.m_lColorTextureID[3],
					TargetID ? "FBO" : "backbuffer");
			}
			// RIDDICK_DIRECT_RENDER=1: force ALL passes to draw straight
			// into the window framebuffer (BindScreenTarget handles the
			// routing). Bypasses env-map RTTs, mirror captures,
			// downsample pyramids, etc. Everything overlaps in one
			// target -- that is the point: raw geometry, no RTT chain.
			if (GLES3_DirectRender())
			{
				BindScreenTarget();
				return;
			}
			if (TargetID > 0)
			{
				SFBOSlot* pSlot = EnsureFBOFor(TargetID);
				if (pSlot)
				{
					glBindFramebuffer(GL_FRAMEBUFFER, pSlot->m_FBO);
					glViewport(0, 0, pSlot->m_Width, pSlot->m_Height);
					m_bRTTActive = true;
					return;
				}
				// Failed to build FBO -- fall through to backbuffer so
				// something renders.
			}
			BindScreenTarget();
		}

		// Copy the current READ framebuffer's colour attachment into the
		// GL texture backing _TextureID. This is how the engine
		// "snapshots" a scene rendered into the backbuffer for later
		// sampling by another pass (PS3 backend uses gcmSetTransferImage
		// for the same purpose -- see MRenderPS3_Texture.cpp:2107).
		void RenderTarget_CopyToTexture(int _TextureID, CRct _SrcRect, CPnt _Dest, bint _bContinueTiling, uint16 _Slice, int _iMRT)
		{
			if (_TextureID <= 0) return;
			// Under DIRECT_RENDER the whole RTT plumbing is bypassed —
			// no copy needed. Any downstream sample of _TextureID will
			// get whatever placeholder we had; that's the price for
			// isolating the pipeline.
			if (GLES3_DirectRender()) return;
			SFBOSlot* pSlot = EnsureFBOFor(_TextureID);
			if (!pSlot) return;

			// The engine's rect is top-left origin, GL is bottom-left.
			// Flip Y so the copy pulls the correct region from the
			// currently-bound framebuffer. Row ORDER inside the copied
			// band is ambiguous (depends on the GUI path the engine
			// picked, which shifted with the caps change) -- so it is
			// runtime-switchable: RIDDICK_COPYTEX_FLIP=1 flips rows via
			// a Y-inverted blit, default is plain copy. A/B test without
			// rebuilding.
			int SrcH = ScreenH();
			int W = _SrcRect.p1.x - _SrcRect.p0.x;
			int H = _SrcRect.p1.y - _SrcRect.p0.y;
			if (W <= 0 || H <= 0) return;

			// Default = flipped blit: confirmed correct on screen
			// 2026-07-20 (menu orientation AND row positions right).
			// RIDDICK_COPYTEX_FLIP=0 switches back to the plain copy.
			static int sFlip = -1;
			if (sFlip < 0)
			{
				const char* e = getenv("RIDDICK_COPYTEX_FLIP");
				sFlip = (e && *e == '0') ? 0 : 1;
			}

			if (sFlip)
			{
				GLint PrevDraw = 0, PrevRead = 0;
				glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &PrevDraw);
				glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &PrevRead);
				const GLboolean bScissor = glIsEnabled(GL_SCISSOR_TEST);
				if (bScissor) glDisable(GL_SCISSOR_TEST);
				glBindFramebuffer(GL_READ_FRAMEBUFFER, (GLuint)PrevDraw);
				glBindFramebuffer(GL_DRAW_FRAMEBUFFER, pSlot->m_FBO);
				glBlitFramebuffer(
					_SrcRect.p0.x, SrcH - _SrcRect.p0.y,
					_SrcRect.p1.x, SrcH - _SrcRect.p1.y,
					_Dest.x, _Dest.y, _Dest.x + W, _Dest.y + H,
					GL_COLOR_BUFFER_BIT, GL_NEAREST);
				glBindFramebuffer(GL_READ_FRAMEBUFFER, (GLuint)PrevRead);
				glBindFramebuffer(GL_DRAW_FRAMEBUFFER, (GLuint)PrevDraw);
				if (bScissor) glEnable(GL_SCISSOR_TEST);
			}
			else
			{
				int SrcY = SrcH - _SrcRect.p1.y;
				// Save + restore currently-bound texture so we don't
				// disturb the current drawcall's binding.
				GLint PrevTex = 0;
				glGetIntegerv(GL_TEXTURE_BINDING_2D, &PrevTex);
				glActiveTexture(GL_TEXTURE0);
				glBindTexture(GL_TEXTURE_2D, pSlot->m_ColorTex);
				glCopyTexSubImage2D(GL_TEXTURE_2D, 0, _Dest.x, _Dest.y,
					_SrcRect.p0.x, SrcY, W, H);
				glBindTexture(GL_TEXTURE_2D, (GLuint)PrevTex);
			}

			if (m_DbgEnabled)
			{
				fprintf(stderr, "[GLES3-RT] CopyToTexture id=%d  src(%d,%d..%d,%d)->dst(%d,%d)  %dx%d\n",
					_TextureID, _SrcRect.p0.x, _SrcRect.p0.y,
					_SrcRect.p1.x, _SrcRect.p1.y, _Dest.x, _Dest.y, W, H);
			}
		}

		void RenderTarget_Clear(CRct _ClearRect, int _WhatToClear, CPixel32 _Color, fp32 _ZBufferValue, int _StecilValue)
		{
			static int sLogged = 0;
			if (sLogged < 20)
			{
				GLint CurFBO = 0;
				glGetIntegerv(GL_FRAMEBUFFER_BINDING, &CurFBO);
				fprintf(stderr, "[GLES3-CLEAR] FBO=%d what=0x%x rgba=(%u,%u,%u,%u) rect=(%d,%d..%d,%d)\n",
					(int)CurFBO, _WhatToClear,
					_Color.GetR(), _Color.GetG(), _Color.GetB(), _Color.GetA(),
					_ClearRect.p0.x, _ClearRect.p0.y, _ClearRect.p1.x, _ClearRect.p1.y);
				fflush(stderr);
				++sLogged;
			}
			GLbitfield Mask = 0;
			if (_WhatToClear & CDC_CLEAR_COLOR)
			{
				const fp32 s = 1.0f / 255.0f;
				glClearColor(_Color.GetR() * s, _Color.GetG() * s, _Color.GetB() * s, _Color.GetA() * s);
				glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
				Mask |= GL_COLOR_BUFFER_BIT;
			}
			if (_WhatToClear & CDC_CLEAR_ZBUFFER)
			{
				glClearDepthf(_ZBufferValue);
				glDepthMask(GL_TRUE);
				Mask |= GL_DEPTH_BUFFER_BIT;
			}
			if (_WhatToClear & CDC_CLEAR_STENCIL)
			{
				glClearStencil(_StecilValue);
				glStencilMask(0xff);
				Mask |= GL_STENCIL_BUFFER_BIT;
			}
			if (!Mask)
				return;

			// Empty rect => full target clear. Otherwise clip via scissor.
			const int w = _ClearRect.p1.x - _ClearRect.p0.x;
			const int h = _ClearRect.p1.y - _ClearRect.p0.y;
			if (w > 0 && h > 0 && m_pDisplayContext)
			{
				glEnable(GL_SCISSOR_TEST);
				// GLES scissor origin is bottom-left; engine rects are top-left.
				const int y = ScreenH() - _ClearRect.p1.y;
				glScissor(_ClearRect.p0.x, y, w, h);
				glClear(Mask);
				glDisable(GL_SCISSOR_TEST);
			}
			else
			{
				glClear(Mask);
			}
		}

		// Full-set translation of a CRC_Attributes bundle to GL state.
		// Called by both Attrib_Set (delta) and Attrib_SetAbsolute
		// (reset). We ignore the delta hint for M1 and re-apply
		// everything -- keep it simple; M4 can turn this into a diff.
		void ApplyAttribs(CRC_Attributes* _pAttrib)
		{
			if (!_pAttrib) return;
			m_pCurAttrib = _pAttrib;
			const uint32 F = _pAttrib->m_Flags;

			// Depth
			if (F & CRC_FLAGS_ZCOMPARE)
			{
				glEnable(GL_DEPTH_TEST);
				// RIDDICK_ZEQ_LEQUAL=1: relax the shading passes'
				// ZCompare EQUAL to LESSEQUAL. The FP20 light/lightmap
				// passes are drawn additively against the depth written
				// by an earlier pass and rely on exact depth equality.
				// Our two vertex-supply paths (GPU geometry cache vs the
				// legacy scalar streamer) can produce bit-different
				// positions for the same surface, and any mismatch makes
				// the whole light pass vanish -- which matches the
				// observed "only some polygons get lit" (2026-07-27).
				// Diagnostic switch: if the world lights up under it, the
				// real fix is making both paths agree bit-for-bit.
				int ZCmp = _pAttrib->m_ZCompare;
				if (ZCmp == CRC_COMPARE_EQUAL && GLES3_ZEqualToLEqual())
					ZCmp = CRC_COMPARE_LESSEQUAL;
				glDepthFunc(GLES3_MapCompare(ZCmp));
			}
			else
			{
				glDisable(GL_DEPTH_TEST);
			}
			glDepthMask((F & CRC_FLAGS_ZWRITE) ? GL_TRUE : GL_FALSE);

			// Blend
			if (F & CRC_FLAGS_BLEND)
			{
				glEnable(GL_BLEND);
				const uint16 SD = _pAttrib->m_SourceDestBlend;
				const uint8  Src = (uint8)(SD & 0xff);
				const uint8  Dst = (uint8)((SD >> 8) & 0xff);
				glBlendFunc(GLES3_MapBlend(Src), GLES3_MapBlend(Dst));
			}
			else
			{
				glDisable(GL_BLEND);
			}

			// Colour + alpha write
			const GLboolean CW = (F & CRC_FLAGS_COLORWRITE) ? GL_TRUE : GL_FALSE;
			const GLboolean AW = (F & CRC_FLAGS_ALPHAWRITE) ? GL_TRUE : GL_FALSE;
			glColorMask(CW, CW, CW, AW);

			// Culling. Matches retail RndrGL exactly (RndrGL:38438 =
			// glFrontFace(GL_CCW) set once at init, :77659-77666 = CULLCW
			// selects glCullFace(GL_BACK) else glCullFace(GL_FRONT)).
			// Verified against the decomp by hand: flag 0x1000 set ->
			// 0x405 (GL_BACK), unset -> 0x404 (GL_FRONT). PS3 backend
			// agrees (MRenderPS3_Attrib.cpp:288-294). Both previous
			// mappings culled the opposite faces (the 7f247c7 "fix" was
			// a behavioural no-op) -> world rendered inside-out: dancing
			// polygons AND the inverted-camera hollow-mask feel.
			if ((F & CRC_FLAGS_CULL) && !m_DbgNoCull)
			{
				glEnable(GL_CULL_FACE);
				// RIDDICK_CULL_MODE (0..3): empirical picker for the
				// FrontFace + CULLCW → GL_CULL_FACE combination. Default
				// retail mapping should have been "right" after our
				// W2V.X flip in XREngine.cpp, but derivations kept
				// disagreeing with what the user sees on screen. Pick
				// the one that shows outward faces:
				//   0: GL_CCW + (CULLCW ? BACK  : FRONT)  retail
				//   1: GL_CCW + (CULLCW ? FRONT : BACK)   inv cull
				//   2: GL_CW  + (CULLCW ? BACK  : FRONT)  inv front
				//   3: GL_CW  + (CULLCW ? FRONT : BACK)   both
				static int sCullMode = -1;
				if (sCullMode < 0)
				{
					const char* e = getenv("RIDDICK_CULL_MODE");
					sCullMode = e ? atoi(e) : 0;
					if (sCullMode < 0 || sCullMode > 3) sCullMode = 0;
				}
				const bool bFrontCW = (sCullMode & 2) != 0;
				const bool bInvCull = (sCullMode & 1) != 0;
				glFrontFace(bFrontCW ? GL_CW : GL_CCW);
				const bool bCw = (F & CRC_FLAGS_CULLCW) != 0;
				const bool bCullBack = bInvCull ? !bCw : bCw;
				glCullFace(bCullBack ? GL_BACK : GL_FRONT);
			}
			else
			{
				glDisable(GL_CULL_FACE);
			}

			// Debug overrides (final say).
			if (m_DbgNoDepth) { glDisable(GL_DEPTH_TEST); glDepthMask(GL_FALSE); }
			if (m_DbgNoBlend) { glDisable(GL_BLEND); }

			// Scissor
			if (F & CRC_FLAGS_SCISSOR)
			{
				uint32 MinX, MinY, MaxX, MaxY;
				_pAttrib->m_Scissor.GetRect(MinX, MinY, MaxX, MaxY);
				const int H = ScreenH();
				const int W = (int)(MaxX - MinX);
				const int Hgt = (int)(MaxY - MinY);
				if (W > 0 && Hgt > 0)
				{
					glEnable(GL_SCISSOR_TEST);
					glScissor((int)MinX, H - (int)MaxY, W, Hgt);
				}
				else
				{
					glDisable(GL_SCISSOR_TEST);
				}
			}
			else
			{
				glDisable(GL_SCISSOR_TEST);
			}

			// Polygon offset
			if (F & CRC_FLAGS_POLYGONOFFSET)
			{
				glEnable(GL_POLYGON_OFFSET_FILL);
				glPolygonOffset(_pAttrib->m_PolygonOffsetScale, _pAttrib->m_PolygonOffsetUnits);
			}
			else
			{
				glDisable(GL_POLYGON_OFFSET_FILL);
			}

			// Stencil
			if (F & CRC_FLAGS_STENCIL)
			{
				glEnable(GL_STENCIL_TEST);
				glStencilMask(_pAttrib->m_StencilWriteMask);
				// Op mapping: 0..7 keep/zero/replace/incr/decr/invert/
				// incr_wrap/decr_wrap; funcs stored 1..8 in CRC_COMPARE_*.
				static const GLenum sOpTbl[] = {
					GL_KEEP, GL_ZERO, GL_REPLACE, GL_INCR, GL_DECR, GL_INVERT,
					GL_INCR_WRAP, GL_DECR_WRAP,
				};
				const int F0 = _pAttrib->m_StencilFrontOpFail   & 7;
				const int F1 = _pAttrib->m_StencilFrontOpZFail  & 7;
				const int F2 = _pAttrib->m_StencilFrontOpZPass  & 7;
				if (F & CRC_FLAGS_SEPARATESTENCIL)
				{
					// Two-sided stencil (engine shadow volumes: incr on
					// front faces, decr on back faces in a single pass).
					const int B0 = _pAttrib->m_StencilBackOpFail   & 7;
					const int B1 = _pAttrib->m_StencilBackOpZFail  & 7;
					const int B2 = _pAttrib->m_StencilBackOpZPass  & 7;
					glStencilFuncSeparate(GL_FRONT, GLES3_MapCompare(_pAttrib->m_StencilFrontFunc),
						_pAttrib->m_StencilRef, _pAttrib->m_StencilFuncAnd);
					glStencilFuncSeparate(GL_BACK, GLES3_MapCompare(_pAttrib->m_StencilBackFunc),
						_pAttrib->m_StencilRef, _pAttrib->m_StencilFuncAnd);
					glStencilOpSeparate(GL_FRONT, sOpTbl[F0], sOpTbl[F1], sOpTbl[F2]);
					glStencilOpSeparate(GL_BACK,  sOpTbl[B0], sOpTbl[B1], sOpTbl[B2]);
				}
				else
				{
					glStencilFunc(GLES3_MapCompare(_pAttrib->m_StencilFrontFunc),
						_pAttrib->m_StencilRef, _pAttrib->m_StencilFuncAnd);
					glStencilOp(sOpTbl[F0], sOpTbl[F1], sOpTbl[F2]);
				}
			}
			else
			{
				glDisable(GL_STENCIL_TEST);
			}
		}

		void Attrib_Set(CRC_Attributes* _pAttrib)         { ++m_DbgAttribSets; ApplyAttribs(_pAttrib); }
		void Attrib_SetAbsolute(CRC_Attributes* _pAttrib) { ++m_DbgAttribSets; ApplyAttribs(_pAttrib); }
		// Cache current light array for shader upload. Engine calls this
		// per drawable when lighting changes; the pointer stays valid
		// until the next call (per MRender.h:176). Base CRC_Core just
		// stores the pointer too — we mirror that AND read it in
		// PushLightUniforms.
		void Attrib_Lights(const CRC_Light* _pLights, int _nLights)
		{
			m_pRCLights = _pLights;
			m_nRCLights = _nLights;
			CRC_Core::Attrib_Lights(_pLights, _nLights);
		}

		void Matrix_SetRender(int _iMode, const CMat4Dfp32* _pMatrix)
		{
			++m_DbgMatrixSets;
			// Matrix_Update passes NULL to mean "matrix is identity"
			// (see MRender.cpp:3410). Reset our cached matrix so a
			// stale value from a previous frame doesn't leak in.
			CMat4Dfp32 Unit; Unit.Unit();
			const CMat4Dfp32& M = _pMatrix ? *_pMatrix : Unit;
			switch (_iMode)
			{
			case CRC_MATRIX_MODEL:      m_ModelMat = M; break;
			case CRC_MATRIX_PROJECTION: m_ProjMat  = M; break;
			case CRC_MATRIX_TEXTURE0:
			case CRC_MATRIX_TEXTURE0 + 1:
			case CRC_MATRIX_TEXTURE0 + 2:
			case CRC_MATRIX_TEXTURE0 + 3:
				m_TexMat[_iMode - CRC_MATRIX_TEXTURE0] = M;
				break;
			default: break;
			}
		}

		// Explicit UI-pass hint from the engine (CRC_Core::Render_SetUIPass
		// override). Set around frontend/HUD rendering (WFrontEnd::
		// OnRender). Authoritative when set -- the matrix/attrib
		// heuristics in IsUI2DDraw remain as fallback for UI geometry
		// issued outside a hinted bracket (and for deferred VBM flushes).
		bool m_bUIPass = false;
		void Render_SetUIPass(bint _bOn) { m_bUIPass = (_bOn != 0); }

		// BeginScene: sync viewport with the active CRC_Viewport rect.
		// The engine calls Viewport_Set separately too, but Base_CRC's
		// stub does nothing; we just do it here so state is coherent
		// before draw calls start.
		// RIDDICK_FP20: pin XR_SHADERMODE to the forward FP20 mode. Create()
		// already tries this, but "SYSTEM" may not be registered in the
		// object manager that early -- and silently staying on AUTO would
		// put the engine on the deferred path (needs an MRT G-buffer we
		// don't build), which is exactly the failure this flag must avoid.
		// Retry once per frame until it takes; PrepareFrame re-reads the
		// value every frame anyway, so a late pin still works.
		void PinShaderModeIfNeeded()
		{
			if (m_bShaderModePinned || !GLES3_FP20Enabled()) return;
			MACRO_GetSystemEnvironment(pEnv);
			if (!pEnv) return;
			static const int kXRShaderMode_FP20Forward = 5; // XRShader.h:111-121
			const int OverrideMode = GLES3_FP20ModeOverride();
			const int Mode = (OverrideMode > 1) ? OverrideMode : kXRShaderMode_FP20Forward;
			pEnv->SetValuei("XR_SHADERMODE", Mode);
			m_bShaderModePinned = true;
			if (m_DbgEnabled)
			{
				fprintf(stderr, "[GLES3-FP] XR_SHADERMODE pinned to %d\n", Mode);
				fflush(stderr);
			}
		}
		bool m_bShaderModePinned = false;

		void BeginScene(CRC_Viewport* _pVP)
		{
			++m_DbgBeginScenes;
			PinShaderModeIfNeeded();
			// Make sure "the backbuffer" means the screen FBO even if
			// the engine never called SetRenderTarget this frame (the
			// composite pass leaves fb0 bound only transiently).
			if (!m_bRTTActive)
				BindScreenTarget();
			CRC_Core::BeginScene(_pVP);
			// CRC_Core::BeginScene already calls Viewport_Set(_pVP)
			// which invokes our overridden Viewport_Update below --
			// projection + glViewport are set from there.
		}

		// Called by CRC_Core whenever the active viewport changes
		// (Viewport_Set from CRC_Core::BeginScene, from
		// CXR_VBManager::Internal_Render mid-scene, from
		// Viewport_Push/Pop). Mirrors CRCPS3GCM::Viewport_Update:
		//
		//   1) pull raw projection matrix from CRC_Viewport
		//   2) scale rows 0 (x) and 1 (y) by 2/width, 2/height --
		//      engine's projection projects into pixel-space
		//      [-w/2, +w/2] / [-h/2, +h/2], not NDC [-1, +1]; the
		//      backend has to bring it into clip space itself
		//   3) set the actual GL viewport (Y-flipped for GLES)
		void Viewport_Update()
		{
			CRC_Core::Viewport_Update();
			CRC_Viewport* pVP = Viewport_Get();
			if (!pVP) return;

			CRct R = pVP->GetViewArea();
			const int W = R.p1.x - R.p0.x;
			const int H = R.p1.y - R.p0.y;

			CMat4Dfp32 ProjMat = pVP->GetProjectionMatrix();
			if (W > 0 && H > 0)
			{
				const fp32 xs = 2.0f / (fp32)W;
				const fp32 ys = 2.0f / (fp32)H;
				ProjMat.k[0][0] *= xs; ProjMat.k[1][0] *= xs;
				ProjMat.k[2][0] *= xs; ProjMat.k[3][0] *= xs;
				ProjMat.k[0][1] *= ys; ProjMat.k[1][1] *= ys;
				ProjMat.k[2][1] *= ys; ProjMat.k[3][1] *= ys;
			}
			m_ProjMat = ProjMat;

			if (m_pDisplayContext && W > 0 && H > 0)
			{
				const int Y = ScreenH() - R.p1.y;
				glViewport(R.p0.x, Y, W, H);
			}

			// RIDDICK_DBG_VP=1: log every viewport change with its
			// projection signature. Perspective routes view-Z into clip-W
			// (flat index 11, see the cw formula in the MTX debug log);
			// 2D/ortho leaves W constant. Run once with this on to verify
			// empirically which viewports are UI and which are world.
			static int sDbgVP = -1;
			if (sDbgVP < 0) sDbgVP = DbgEnvFlag("RIDDICK_DBG_VP");
			if (sDbgVP)
			{
				const float* mp = (const float*)&m_ProjMat;
				fprintf(stderr, "[VP] rect=(%d,%d)-(%d,%d) proj z->w=%g constW=%g projTest=%s hint=%d -> %s\n",
					R.p0.x, R.p0.y, R.p1.x, R.p1.y, mp[11], mp[15],
					Is2DProjection() ? "2D" : "persp", m_bUIPass ? 1 : 0,
					ClassifyUI() ? "UI shader" : "3D shader");
				fflush(stderr);
			}
		}

		// UI/3D discriminator based on the PROJECTION matrix captured in
		// Viewport_Update. Perspective projections route view-space Z into
		// clip W (flat index 11: cw = x*mv[3] + y*mv[7] + z*mv[11] + mv[15],
		// per the MTX debug-log formulas in DrawIndexed); 2D/ortho
		// projections leave W constant (index 11 == 0, index 15 == 1).
		// This replaces the old MODEL-matrix test, which misclassified
		// world geometry: BSP/static-world draws keep Model=identity
		// (vertices already in world space) and identity passes the old
		// "2D" check, so parts of the 3D world went through the UI shader.
		bool Is2DProjection() const
		{
			const float* mp = (const float*)&m_ProjMat;
			return fabsf(mp[11]) < 1e-6f && fabsf(mp[15] - 1.0f) < 1e-3f;
		}

		virtual int Texture_GetBackBufferTextureID() {return 0;}
		virtual int Texture_GetFrontBufferTextureID() {return 0;}
		virtual int Texture_GetZBufferTextureID() {return 0;}
		virtual int Geometry_GetVBSize(int _VBID) {return 0;}

		// --- M3 draw path ------------------------------------------
		// Interleaved 44-byte vertex: pos.xyz + uv0.xy + uv1.xy
		// (lightmap ch1) + colour BGRA packed as uint32 + normal.xyz
		// (object/world space, unit length after unpack). Normal is
		// consumed by Lambert light path (Phase A); UI/base draws
		// leave it (0,0,1) which the shader treats as no-light.
		struct SUIVert { float x,y,z, u,v, u1,v1; uint32_t col; float nx,ny,nz; };

		// Pack CPixel32 (BGRA byte order per MImage.h) to RGBA-word for
		// the shader (glVertexAttribPointer normalized ubyte4 reads in
		// memory order, so we swap B/R to feed vCol.rgb correctly).
		static uint32_t PackColorBGRA_to_RGBA(uint32_t _bgra)
		{
			return ( _bgra & 0xff00ff00u)
				 | ((_bgra & 0x00ff0000u) >> 16)
				 | ((_bgra & 0x000000ffu) << 16);
		}

		// Build interleaved buffer from the CRC_Core-accumulated
		// m_Geom + m_GeomColor. tex-channel 0 is enough for UI; higher
		// channels arrive in M4 shader generator.
		bool BuildInterleavedVerts(SUIVert*& _pOut, int& _nOut, bool& _bMalloced)
		{
			_bMalloced = false;
			// BSP2 solid-world path: engine called Geometry_VertexBuffer(VBID)
			// (setter that just stores VBID in m_GeomVBID — see MRender.cpp:4127)
			// then Render_IndexedTriangles(piPrim, nPrim). m_Geom stays stale
			// from a previous UI draw; we must fetch the real vertices from
			// the VBID here. VBB uses packed formats (I16/NS/NU) that
			// BuildVertsFromVBB (which VRegFetch decodes) already handles.
			// Without this override, retail-Win32 GL delegated to CRC_Core
			// which had a full-VBB fetch built in; our old code was reading
			// stale m_Geom, hence "stretched polygons across the whole
			// screen" symptom in Pa1_TheDream.
			if (m_GeomVBID != 0 && m_pVBCtx)
			{
				CRC_BuildVertexBuffer VBB;
				VBB.Clear();
				m_pVBCtx->VB_Get((int)m_GeomVBID, VBB, VB_GETFLAGS_BUILD);
				int nV = 0;
				SUIVert* p = BuildVertsFromVBB(VBB, nV);
				if (!p) { _pOut = 0; _nOut = 0; return false; }
				_pOut = p;
				_nOut = nV;
				_bMalloced = true;
				return true;
			}

			const int nV = (int)m_Geom.m_nV;
			if (nV <= 0 || !m_Geom.m_pV) { _pOut = 0; _nOut = 0; return false; }
			// Same skip-skinned rule for the m_Geom (CRC_VertexBuffer) path.
			{
				static int sSkipSk = -1;
				if (sSkipSk < 0)
				{
					const char* e = getenv("RIDDICK_SKIP_SKINNED");
					sSkipSk = (e && *e && *e != '0') ? 1 : 0;
				}
				if (sSkipSk && (m_Geom.m_pMI || m_Geom.m_pMW || m_Geom.m_nMWComp))
				{
					_pOut = 0; _nOut = 0; return false;
				}
			}
			static SUIVert sScratch[16384];
			SUIVert* p = (nV <= 16384) ? sScratch : (SUIVert*)malloc(sizeof(SUIVert) * nV);
			if (!p) return false;
			_bMalloced = (nV > 16384);

			// Same Attrib_TexCoordSet honoring as the VBID path (see
			// BuildVertsFromVBB): channel k samples UV set
			// m_iTexCoordSet[k], not necessarily TEXCOORDk.
			int UVSet0 = 0, UVSet1 = 1;
			if (m_pCurAttrib)
			{
				UVSet0 = m_pCurAttrib->m_iTexCoordSet[0];
				UVSet1 = m_pCurAttrib->m_iTexCoordSet[1];
				if (UVSet0 >= CRC_MAXTEXCOORDS) UVSet0 = 0;
				if (UVSet1 >= CRC_MAXTEXCOORDS) UVSet1 = 1;
			}
			const CVec3Dfp32* pV   = m_Geom.m_pV;
			const fp32*       pTV0 = m_Geom.m_pTV[UVSet0];
			const int         nUV  = m_Geom.m_nTVComp[UVSet0]; // 0/2/3/4
			const fp32*       pTV1 = m_Geom.m_pTV[UVSet1];
			const int         nUV1 = m_Geom.m_nTVComp[UVSet1];
			const CPixel32*   pCol = m_Geom.m_pCol;
			const CVec3Dfp32* pN   = m_Geom.m_pN;
			const uint32_t    ConstCol = PackColorBGRA_to_RGBA(*(const uint32_t*)&m_GeomColor);
			// See BuildVertsFromVBB: NO_LIGHT fullbright only on 3D draws.
			const bool bForceWhiteIntl = GLES3_NoLight() && !ClassifyUI();

			for (int i = 0; i < nV; ++i)
			{
				p[i].x = pV[i].k[0];
				p[i].y = pV[i].k[1];
				p[i].z = pV[i].k[2];
				if (pTV0 && nUV >= 2)
				{
					p[i].u = pTV0[i * nUV + 0];
					p[i].v = pTV0[i * nUV + 1];
				}
				else
				{
					p[i].u = 0.0f; p[i].v = 0.0f;
				}
				if (pTV1 && nUV1 >= 2)
				{
					p[i].u1 = pTV1[i * nUV1 + 0];
					p[i].v1 = pTV1[i * nUV1 + 1];
				}
				else
				{
					p[i].u1 = 0.0f; p[i].v1 = 0.0f;
				}
				p[i].col = bForceWhiteIntl ? 0xffffffffu
					: (pCol ? PackColorBGRA_to_RGBA(*(uint32_t*)&pCol[i]) : ConstCol);
				if (pN) { p[i].nx = pN[i].k[0]; p[i].ny = pN[i].k[1]; p[i].nz = pN[i].k[2]; }
				else    { p[i].nx = 0; p[i].ny = 0; p[i].nz = 1; }
			}
			_pOut = p;
			_nOut = nV;
			return true;
		}

		void FreeScratch(SUIVert* _p, int _nV, bool _bMalloced)
		{
			// _bMalloced set by BuildInterleavedVerts when the buffer came
			// from malloc (VBID path always, or fallback path when nV>16384).
			if (_bMalloced) free(_p);
			(void)_nV;
		}

		// Vertex layout for SUIVert at byte offset _Base inside the
		// currently-bound VBO.
		//
		// NOTE (RIDDICK_NDS): SUIVert does NOT carry a tangent basis --
		// extending it (locations 5/6) would touch every hardcoded byte
		// offset in this struct's producers/consumers throughout the file
		// (BuildVertsFromVBB, BuildInterleavedVerts, DrawUserVerts, the
		// TEST_TRI/DumpGeomOBJ scratch data, ...), which was judged too
		// risky for this change -- see Docs/HacksAndHooks.md "RIDDICK_NDS".
		// Locations 5/6 are left disabled with an axis-aligned constant
		// fallback so a shader that declares them (XRShader_FP20_NDS)
		// still gets a well-defined, non-garbage value if it is ever bound
		// while this path is active; m_bTangentUReal/VReal record that the
		// data is NOT real, which is what TrySetupNDSProgram gates on to
		// fall back instead of lighting with the wrong basis.
		void SetVertexAttribPointers(intptr_t _Base)
		{
			const GLsizei S = (GLsizei)sizeof(SUIVert);
			glEnableVertexAttribArray(0);
			glEnableVertexAttribArray(1);
			glEnableVertexAttribArray(2);
			glEnableVertexAttribArray(3);
			glEnableVertexAttribArray(4);
			glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, S, (const void*)(_Base + 0));
			glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, S, (const void*)(_Base + 12));
			glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, S, (const void*)(_Base + 20));
			glVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE, S, (const void*)(_Base + 28));
			glVertexAttribPointer(4, 3, GL_FLOAT, GL_FALSE, S, (const void*)(_Base + 32));
			glDisableVertexAttribArray(5);
			glDisableVertexAttribArray(6);
			glVertexAttrib4f(5, 1.0f, 0.0f, 0.0f, 1.0f);
			glVertexAttrib4f(6, 0.0f, 1.0f, 0.0f, 1.0f);
			m_bTangentUReal = false;
			m_bTangentVReal = false;
		}

		void DisableVertexAttribPointers()
		{
			glDisableVertexAttribArray(0);
			glDisableVertexAttribArray(1);
			glDisableVertexAttribArray(2);
			glDisableVertexAttribArray(3);
			glDisableVertexAttribArray(4);
			glDisableVertexAttribArray(5);
			glDisableVertexAttribArray(6);
		}

		// --- Phase 4 M6: cached-VBID draw path (GLES3_Geometry.h) --------
		// Bind one vertex attribute location from a cached entry's
		// per-register layout, or fall back to a constant value via
		// glVertexAttrib4f if the engine never supplied that register for
		// this VBID (e.g. no per-vertex normal). Mirrors the constant
		// defaults BuildVertsFromVBB bakes into SUIVert for the same
		// cases (col=white, normal=+Z, uv=0). Returns whether a real
		// per-vertex register was bound (true) vs. the constant fallback
		// (false) -- SetVertexAttribPointersFromEntry uses this for the
		// tangent locations to tell TrySetupNDSProgram whether it is
		// looking at a genuine tangent basis or a placeholder.
		bool BindEntryAttrib(const SGLES3GeomEntry& _E, int _Loc, int _Reg,
		                      float _Cx, float _Cy, float _Cz, float _Cw)
		{
			const int Off = _E.m_lRegOffset[_Reg];
			if (Off < 0)
			{
				glDisableVertexAttribArray(_Loc);
				glVertexAttrib4f(_Loc, _Cx, _Cy, _Cz, _Cw);
				return false;
			}
			glEnableVertexAttribArray(_Loc);
			const int Fmt = _E.m_lRegFormat[_Reg];
			const GLsizei S = (GLsizei)_E.m_Stride;
			if (Fmt == CRC_VREGFMT_N4_COL)
				glVertexAttribPointer(_Loc, 4, GL_UNSIGNED_BYTE, GL_TRUE, S, (const void*)(intptr_t)Off);
			else
			{
				int nComp = CRC_VertexFormat::GetRegisterComponents(Fmt);
				if (nComp <= 0) nComp = 1;
				glVertexAttribPointer(_Loc, nComp, GL_FLOAT, GL_FALSE, S, (const void*)(intptr_t)Off);
			}
			return true;
		}

		// Vertex layout for a cached geometry entry: same 5 attribute
		// locations as SetVertexAttribPointers (pos/uv0/col/uv1/normal),
		// sourced from whichever registers CGLES3GeometryCache::Build
		// actually found for this VBID at ITS destination offsets, rather
		// than our fixed SUIVert struct. UV-set selection mirrors
		// BuildVertsFromVBB's Attrib_TexCoordSet honoring. Additionally
		// binds the tangent basis (locations 5/6, RIDDICK_NDS) from
		// registers CRC_VREG_TEXCOORD0 + m_iTexCoordSet[2]/[3] -- the same
		// engine convention as the mapping UV (m_iTexCoordSet[0]/[1]), see
		// CXR_VirtualAttributes_ShaderFP20_COREFBB::OnSetAttributes
		// (XRShader_FP20.cpp). CGLES3GeometryCache::Build includes ANY
		// vertex register the source geometry declares (not a fixed list),
		// so if the source actually carries tangents these binds pick up
		// real data automatically; if not, BindEntryAttrib's constant
		// fallback (1,0,0)/(0,1,0) applies, same spirit as the normal's
		// (0,0,1) default above.
		void SetVertexAttribPointersFromEntry(const SGLES3GeomEntry& _E)
		{
			int UVSet0 = 0, UVSet1 = 1;
			int TUSet = 2, TVSet = 3;
			if (m_pCurAttrib)
			{
				UVSet0 = m_pCurAttrib->m_iTexCoordSet[0];
				UVSet1 = m_pCurAttrib->m_iTexCoordSet[1];
				TUSet  = m_pCurAttrib->m_iTexCoordSet[2];
				TVSet  = m_pCurAttrib->m_iTexCoordSet[3];
				if (UVSet0 >= CRC_MAXTEXCOORDS) UVSet0 = 0;
				if (UVSet1 >= CRC_MAXTEXCOORDS) UVSet1 = 1;
				if (TUSet < 0 || TUSet >= CRC_MAXTEXCOORDS) TUSet = 2;
				if (TVSet < 0 || TVSet >= CRC_MAXTEXCOORDS) TVSet = 3;
			}
			BindEntryAttrib(_E, 0, CRC_VREG_POS,                0.0f, 0.0f, 0.0f, 1.0f);
			BindEntryAttrib(_E, 1, CRC_VREG_TEXCOORD0 + UVSet0, 0.0f, 0.0f, 0.0f, 1.0f);
			BindEntryAttrib(_E, 3, CRC_VREG_TEXCOORD0 + UVSet1, 0.0f, 0.0f, 0.0f, 1.0f);
			BindEntryAttrib(_E, 2, CRC_VREG_COLOR,              1.0f, 1.0f, 1.0f, 1.0f);
			BindEntryAttrib(_E, 4, CRC_VREG_NORMAL,             0.0f, 0.0f, 1.0f, 1.0f);
			m_bTangentUReal = BindEntryAttrib(_E, 5, CRC_VREG_TEXCOORD0 + TUSet, 1.0f, 0.0f, 0.0f, 1.0f);
			m_bTangentVReal = BindEntryAttrib(_E, 6, CRC_VREG_TEXCOORD0 + TVSet, 0.0f, 1.0f, 0.0f, 1.0f);
		}

		// Draw using GPU-resident buffers from m_GeomCache instead of
		// streaming through m_Streamer: no per-draw malloc, no scalar
		// VRegFetch conversion, no re-upload. _E supplies the vertex
		// buffer (position/uv/col/normal registers + stride); _IB
		// supplies the index buffer -- may be the same entry as _E
		// (Render_VertexBuffer, whose VBID carries its own primitive
		// stream) or a separate shared index-pool entry
		// (Render_VertexBuffer_IndexBufferTriangles / BSP2 world
		// clusters). _ByteOffset is the byte offset into _IB's index
		// buffer where this draw's indices start. Returns false (nothing
		// drawn) if either buffer is missing so the caller can fall back
		// to the streaming path.
		bool DrawCachedVB(const SGLES3GeomEntry& _E, const SGLES3GeomEntry& _IB, int _nIdx, intptr_t _ByteOffset)
		{
			if (!_E.m_VBO || !_IB.m_IBO || _nIdx <= 0) return false;

			glBindVertexArray(m_VAO);
			glBindBuffer(GL_ARRAY_BUFFER, _E.m_VBO);
			SetVertexAttribPointersFromEntry(_E);

			SetupCommonUniforms(false);
			m_DbgTotalVerts += _E.m_nV;
			m_DbgTotalIdx   += _nIdx;

			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, _IB.m_IBO);
			const GLenum DrawPrim = m_DbgForceWire ? GL_LINE_STRIP : GL_TRIANGLES;
			glDrawElements(DrawPrim, _nIdx, GL_UNSIGNED_SHORT, (const void*)_ByteOffset);

			DisableVertexAttribPointers();
			glBindVertexArray(0);
			++m_DbgDrawCached;
			return true;
		}

		// Shared uniform + texture setup for the UI shader: MVP,
		// texture matrices, alpha test, fog (optional), textures from
		// the current attrib (channel 0 = base, channel 1 = secondary
		// modulate -- lightmaps).
		// Determine per-drawcall lighting mode from CRC_Attributes +
		// cached Attrib_Lights, then push all light uniforms.
		//   0 = off (UI, or no lights supplied)
		//   1 = modulate (base pass; c.rgb *= ambient + Σ diffuse)
		//   2 = additive (light-only; expected ONE/ONE blend from engine)
		void PushLightUniforms(bool /*_bAllowFog*/)
		{
			int Mode = 0;
			// Engine sets CRC_FLAGS_LIGHTING when a vertex-lit pass is
			// wanted. Additional heuristic: any pass whose blend is ONE/ONE
			// (CRC_RASTERMODE_ADD) with lights supplied is a light-add pass.
			// RIDDICK_NO_LIGHT=1: fullbright diagnostic. Fold vCol,
			// fog and lighting-block to no-ops so fragment reduces to
			// pure diffuse (see GLES3_NoLight() near top of file).
			const bool bHaveLights = !GLES3_NoLight() && (m_pRCLights && m_nRCLights > 0);
			if (bHaveLights && m_pCurAttrib)
			{
				const uint32 F = m_pCurAttrib->m_Flags;
				if (F & CRC_FLAGS_LIGHTING) Mode = 1;
				else if (F & CRC_FLAGS_BLEND)
				{
					const uint16 SD = m_pCurAttrib->m_SourceDestBlend;
					const uint8 Src = SD & 0xff;
					const uint8 Dst = (SD >> 8) & 0xff;
					if (Src == CRC_BLEND_ONE && Dst == CRC_BLEND_ONE) Mode = 2;
				}
			}
			m_UIShader.SetInt(m_ULightingModeLoc, Mode);
			if (Mode == 0)
			{
				m_UIShader.SetInt(m_UNumLightsLoc, 0);
				return;
			}
			// Ambient: sum m_Ambient of point/point-like + baseline AMBIENT
			// light entries so scene has a visible floor.
			float ambR = 0, ambG = 0, ambB = 0;
			int nOut = 0;
			float lightPos[GLES3_MAX_LIGHTS * 4]   = {0};
			float lightColor[GLES3_MAX_LIGHTS * 4] = {0};
			const float inv255 = 1.0f / 255.0f;
			for (int i = 0; i < m_nRCLights && nOut < GLES3_MAX_LIGHTS; ++i)
			{
				const CRC_Light& L = m_pRCLights[i];
				const float aR = L.m_Ambient.GetR() * inv255;
				const float aG = L.m_Ambient.GetG() * inv255;
				const float aB = L.m_Ambient.GetB() * inv255;
				ambR += aR; ambG += aG; ambB += aB;
				if (L.m_Type == CRC_LIGHTTYPE_AMBIENT) continue;
				// Range folded into attenuation[1] per report; fallback 1000.
				float range = L.m_Attenuation[1] > 0 ? L.m_Attenuation[1] : 1000.0f;
				lightPos[nOut*4+0] = L.m_Pos.k[0];
				lightPos[nOut*4+1] = L.m_Pos.k[1];
				lightPos[nOut*4+2] = L.m_Pos.k[2];
				lightPos[nOut*4+3] = range;
				lightColor[nOut*4+0] = L.m_Color.GetR() * inv255;
				lightColor[nOut*4+1] = L.m_Color.GetG() * inv255;
				lightColor[nOut*4+2] = L.m_Color.GetB() * inv255;
				lightColor[nOut*4+3] = 1.0f;
				++nOut;
			}
			// Base-pass path: give it a small ambient floor even if all
			// engine ambients are 0, so texture is visible instead of black.
			if (Mode == 1 && ambR + ambG + ambB < 0.05f)
			{
				ambR = ambG = ambB = 0.2f;
			}
			if (m_UAmbientLoc >= 0)
			{
				const float a[3] = { ambR, ambG, ambB };
				glUniform3fv(m_UAmbientLoc, 1, a);
			}
			m_UIShader.SetInt(m_UNumLightsLoc, nOut);
			if (nOut > 0)
			{
				if (m_ULightPosLoc   >= 0) glUniform4fv(m_ULightPosLoc,   nOut, lightPos);
				if (m_ULightColorLoc >= 0) glUniform4fv(m_ULightColorLoc, nOut, lightColor);
			}
		}

		// --- TexGen ---------------------------------------------------
		// One-shot-per-mode diagnostic: which CRC_TEXGENMODE_* values the
		// engine actually asks for on which channel, so unimplemented
		// modes (LIGHTING, REFLECTION, env maps, ...) can be prioritised
		// next. TSLV is handled by DecodeTexGenChannels (RIDDICK_NDS) and
		// no longer reaches this log. [channel][mode]; mode values are
		// small (see CRC_TEXGENMODE_* enum in MRender_Classes.h, <32).
		bool m_TexGenModeLogged[CRC_MAXTEXCOORDS][32] = {};
		void DbgNoteTexGenMode(int _Mode, int _Channel)
		{
			if (!m_DbgEnabled) return;
			if (_Channel < 0 || _Channel >= CRC_MAXTEXCOORDS) return;
			if (_Mode < 0 || _Mode >= 32) return;
			if (m_TexGenModeLogged[_Channel][_Mode]) return;
			m_TexGenModeLogged[_Channel][_Mode] = true;
			fprintf(stderr, "[GLES3-TEXGEN] unsupported mode=%d on channel %d\n", _Mode, _Channel);
			fflush(stderr);
		}

		// Decode CRC_Attributes::m_lTexGenMode/m_TexGenComp/m_pTexGenAttr
		// into vertex-shader uniforms for texture channels 0 and 1 (the
		// only channels either shader samples). Walks ALL CRC_MAXTEXCOORDS
		// channels in order to keep the m_pTexGenAttr offset correct for
		// every channel, exactly like the engine's own decoder
		// (Classes/Render/MRenderVPGen.h, SetRegisters_TexGenMatrix) --
		// channels we don't implement still have to be skipped over by
		// their correct size, or every later channel reads garbage.
		//
		// Supported: CRC_TEXGENMODE_TEXCOORD (0 -- current behaviour, UV
		// from the vertex register) and CRC_TEXGENMODE_LINEAR (1 -- UV is
		// a linear function of the modelspace vertex position: this is
		// what BSP2's depth-fog pass uses to turn view depth into a ramp-
		// texture lookup, see WBSP2Model.cpp / Docs/Render_Strategy.md
		// §1). Anything else falls back to TEXCOORD and gets logged once.
		// (The channel walk itself now lives in DecodeTexGenChannels, so
		// XRShader_FP20_NDS's TSLV channels 3/4 can share it -- see
		// TrySetupNDSProgram. This function is unchanged behaviourally,
		// just no longer duplicates the walk.)
		void PushTexGenUniforms(bool _bUI)
		{
			int Mode0, Mode1;
			float U0[4], V0[4], U1[4], V1[4];
			float TSLVParam[CRC_MAXTEXCOORDS][4]; bool HaveTSLV[CRC_MAXTEXCOORDS];
			DecodeTexGenChannels(Mode0, U0, V0, Mode1, U1, V1, TSLVParam, HaveTSLV);
			(void)TSLVParam; (void)HaveTSLV; // not consumed by the UI/3D programs

			CGLES3Shader& Sh    = _bUI ? m_UIShader        : m_3DShader;
			const int LocMode0  = _bUI ? m_UTexGenMode0Loc : m_3DUTexGenMode0Loc;
			const int LocU0     = _bUI ? m_UTexGenU0Loc    : m_3DUTexGenU0Loc;
			const int LocV0     = _bUI ? m_UTexGenV0Loc    : m_3DUTexGenV0Loc;
			Sh.SetInt(LocMode0, Mode0);
			Sh.SetVec4(LocU0, U0[0], U0[1], U0[2], U0[3]);
			Sh.SetVec4(LocV0, V0[0], V0[1], V0[2], V0[3]);

			// Channel 1 only exists on the UI program (second UV channel
			// for lightmap-style modulation); the 3D shader has no vUV1.
			if (_bUI)
			{
				m_UIShader.SetInt(m_UTexGenMode1Loc, Mode1);
				m_UIShader.SetVec4(m_UTexGenU1Loc, U1[0], U1[1], U1[2], U1[3]);
				m_UIShader.SetVec4(m_UTexGenV1Loc, V1[0], V1[1], V1[2], V1[3]);
			}
		}

		// Shared matrix tests used by IsUI2DDraw + the classify log.
		bool IsModelIdentity() const
		{
			const CMat4Dfp32& M = m_ModelMat;
			return fabsf(M.k[0][0] - 1.0f) < 1e-3f && fabsf(M.k[1][1] - 1.0f) < 1e-3f
				&& fabsf(M.k[0][2]) < 1e-5f && fabsf(M.k[1][2]) < 1e-5f
				&& fabsf(M.k[2][0]) < 1e-5f && fabsf(M.k[2][1]) < 1e-5f
				&& fabsf(M.k[2][2] - 1.0f) < 1e-3f
				&& fabsf(M.k[3][0]) < 1e-5f && fabsf(M.k[3][1]) < 1e-5f
				&& fabsf(M.k[3][2]) < 1e-5f;
		}
		bool IsModelGet2DPattern() const
		{
			// CRC_Viewport::Get2DMatrix (MRender.cpp): diagonal 3x3 with
			// k[2][2]==1 plus a Z translation to the back plane.
			const CMat4Dfp32& M = m_ModelMat;
			return fabsf(M.k[0][2]) < 1e-5f && fabsf(M.k[1][2]) < 1e-5f
				&& fabsf(M.k[2][0]) < 1e-5f && fabsf(M.k[2][1]) < 1e-5f
				&& fabsf(M.k[2][2] - 1.0f) < 1e-3f;
		}

		// Pure UI/2D classification (no debug overrides). No single test
		// suffices (UI text was observed with PERSPECTIVE projection AND
		// identity model), so three independent signals are combined:
		//  a) true 2D/ortho viewport -> UI (Is2DProjection);
		//  b) Get2DMatrix-style MODEL matrix (non-identity) -> UI drawn
		//     via CRC_Util2D with a model transform to the back plane;
		//  c) identity MODEL + depth test AND depth write both disabled
		//     -> UI whose verts were pre-transformed on the CPU (font
		//     quads from CRC_Font::Write carry pixel coords; the frontend
		//     explicitly disables CRC_FLAGS_ZCOMPARE for interface
		//     rendering). World BSP also keeps Model=identity but always
		//     depth-tests/writes, so it stays on the 3D program.
		bool IsUI2DDraw() const
		{
			if (Is2DProjection()) return true;
			if (IsModelGet2DPattern() && !IsModelIdentity()) return true;
			if (IsModelIdentity() && m_pCurAttrib)
			{
				const uint32 F = m_pCurAttrib->m_Flags;
				if (!(F & CRC_FLAGS_ZCOMPARE) && !(F & CRC_FLAGS_ZWRITE))
					return true;
			}
			return false;
		}

		// Final UI classification: the explicit engine hint
		// (Render_SetUIPass -> m_bUIPass) is authoritative; the
		// matrix/attrib heuristics cover UI issued without a hint.
		bool ClassifyUI() const
		{
			return m_bUIPass || IsUI2DDraw();
		}

		// UI vs world-geometry discriminator for shader selection.
		// RIDDICK_FORCE_3D_SHADER=1 override: every draw uses the 3D
		// program (handy to verify the new shader in isolation).
		bool IsUIDraw() const
		{
			static int sForce3D = -1;
			if (sForce3D < 0)
			{
				const char* e = getenv("RIDDICK_FORCE_3D_SHADER");
				sForce3D = (e && *e && *e != '0') ? 1 : 0;
			}
			if (sForce3D) return false;
			return ClassifyUI();
		}

		void SetupCommonUniforms(bool _bAllowFog)
		{
			// RIDDICK_FP20 diagnostic (RIDDICK_DBG_GL=1): the engine only
			// ever attaches an FP20 ext-attrib once RIDDICK_FP20 has pulled
			// XR_SHADERMODE off AUTO (see CRC_GLES3::Create), so this is a
			// no-op with default settings. m_pExtAttrib is the base
			// CRC_ExtAttributes*; CRC_ATTRIBTYPE_FP20 identifies the real
			// type as CRC_ExtAttributes_FragmentProgram20 (MRender_Classes.h
			// :675-745). We only log and count here -- the draw itself
			// still goes through m_UIShader/m_3DShader unchanged (no FP20
			// program is compiled or executed).
			if (m_pCurAttrib && m_pCurAttrib->m_pExtAttrib &&
			    m_pCurAttrib->m_pExtAttrib->m_AttribType == CRC_ATTRIBTYPE_FP20)
			{
				++m_DbgFPDraws;
				if (m_DbgEnabled)
					DbgLogFP20(static_cast<const CRC_ExtAttributes_FragmentProgram20*>(
						m_pCurAttrib->m_pExtAttrib));
			}

			// XRShader_FP20_LFM (baked BSP2 lightmap): if this draw's FP20
			// ext-attrib names that program AND all four LFM textures are
			// present and upload cleanly, TrySetupLFMProgram binds
			// m_LFMShader and sets every uniform/texture itself -- skip the
			// normal UI/3D setup entirely for this draw. Everything else
			// (including plain FP20 draws that aren't LFM, or LFM draws
			// missing a texture) falls through unchanged to the selection
			// below, same as before this program existed.
			if (TrySetupLFMProgram())
				return;

			// XRShader_FP20_NDS (single dynamic light: diffuse + normal map
			// + Phong specular -- see TrySetupNDSProgram / kGLES3_NDSVertSrc/
			// FragSrc / Docs/HacksAndHooks.md "RIDDICK_NDS"). Same contract
			// as the LFM check above: on success the program is fully bound
			// and this draw is done.
			if (TrySetupNDSProgram())
				return;

			// Pick the program for this draw: UI/2D keeps the full
			// m_UIShader (lights/fog/alpha test/second UV channel), world
			// geometry goes through the minimal m_3DShader. Fall back to
			// the UI program if the 3D one failed to build.
			const bool bUI = IsUIDraw() || !m_3DShader.IsValid();

			// RIDDICK_DBG_CLASSIFY=N: log the classification inputs of the
			// first N draws, then stop. For chasing UI/3D misroutes:
			// projection test, model-matrix class, depth/blend flags and
			// the resulting program choice.
			static int sClsLog = -1;
			if (sClsLog < 0)
			{
				const char* e = getenv("RIDDICK_DBG_CLASSIFY");
				sClsLog = e ? atoi(e) : 0;
			}
			if (sClsLog > 0)
			{
				--sClsLog;
				const uint32 F = m_pCurAttrib ? m_pCurAttrib->m_Flags : 0;
				const CMat4Dfp32& M = m_ModelMat;
				fprintf(stderr, "[CLS] hint=%d proj=%s mdl=%s k00=%.3g k32=%.3g ZCmp=%d ZW=%d Blend=%d Tex0=%d -> %s\n",
					m_bUIPass ? 1 : 0,
					Is2DProjection() ? "2D" : "persp",
					IsModelIdentity() ? "ident" : (IsModelGet2DPattern() ? "2dpat" : "3d"),
					M.k[0][0], M.k[3][2],
					(F & CRC_FLAGS_ZCOMPARE) ? 1 : 0,
					(F & CRC_FLAGS_ZWRITE)   ? 1 : 0,
					(F & CRC_FLAGS_BLEND)    ? 1 : 0,
					m_pCurAttrib ? (int)m_pCurAttrib->m_TextureID[0] : 0,
					bUI ? "UI" : "3D");
				fflush(stderr);
			}

			CGLES3Shader& Sh   = bUI ? m_UIShader     : m_3DShader;
			const int LocMVP   = bUI ? m_UMVPLoc      : m_3DUMVPLoc;
			const int LocModel = bUI ? m_UModelLoc    : m_3DUModelLoc;
			const int LocTexM  = bUI ? m_UTexMatLoc   : m_3DUTexMatLoc;
			const int LocTex   = bUI ? m_UTexLoc      : m_3DUTexLoc;
			const int LocUseT  = bUI ? m_UUseTexLoc   : m_3DUUseTexLoc;
			const int LocDbg   = bUI ? m_UDbgModeLoc  : m_3DUDbgModeLoc;

			Sh.Use();
			CMat4Dfp32 MVP;
			m_ModelMat.Multiply(m_ProjMat, MVP);
			Sh.SetMat4(LocMVP, (const float*)&MVP);
			Sh.SetMat4(LocModel, (const float*)&m_ModelMat);
			Sh.SetMat4(LocTexM,  (const float*)&m_TexMat[0]);

			PushTexGenUniforms(bUI);

			// --- UI-program-only features (lights, alpha test, fog, UV1).
			// The 3D shader has none of these uniforms by design.
			if (bUI)
			{
				m_UIShader.SetMat4(m_UTexMat1Loc, (const float*)&m_TexMat[1]);
				PushLightUniforms(_bAllowFog);

				// Alpha test (no fixed-function path in GLES3; done in shader)
				if (!m_DbgNoAlpha && m_pCurAttrib && m_pCurAttrib->m_AlphaCompare != CRC_COMPARE_ALWAYS)
				{
					m_UIShader.SetInt(m_UAlphaFuncLoc, m_pCurAttrib->m_AlphaCompare);
					m_UIShader.SetFloat(m_UAlphaRefLoc, (float)m_pCurAttrib->m_AlphaRef * (1.0f / 255.0f));
				}
				else
					m_UIShader.SetInt(m_UAlphaFuncLoc, 0);

				if (_bAllowFog && !GLES3_NoLight() && m_pCurAttrib && (m_pCurAttrib->m_Flags & CRC_FLAGS_FOG))
				{
					const CPixel32 FC = m_pCurAttrib->m_FogColor;
					const float Fog[3] = { FC.GetR() * (1.0f/255.0f), FC.GetG() * (1.0f/255.0f), FC.GetB() * (1.0f/255.0f) };
					m_UIShader.SetInt(m_UFogEnableLoc, 1);
					glUniform3fv(m_UFogColorLoc, 1, Fog);
					m_UIShader.SetFloat(m_UFogStartLoc, m_pCurAttrib->m_FogStart);
					m_UIShader.SetFloat(m_UFogEndLoc, m_pCurAttrib->m_FogEnd);
				}
				else
					m_UIShader.SetInt(m_UFogEnableLoc, 0);
			}
			// 3D-program-only: RIDDICK_NO_LIGHT=1 -> shader ignores the
			// baked per-vertex ambient (vCol) and draws pure diffuse.
			// RIDDICK_AMBIENT_FLOOR=f (default 0) -> floor for the baked
			// vCol; maps with black-baked ambient (Pit) render black at 0.
			// 0.2 reproduces the old everything-shader's floor.
			else
			{
				m_3DShader.SetInt(m_3DUNoLightLoc, GLES3_NoLight() ? 1 : 0);
				static float sAmbFloor = -1.0f;
				if (sAmbFloor < 0.0f)
				{
					const char* e = getenv("RIDDICK_AMBIENT_FLOOR");
					sAmbFloor = e ? (float)atof(e) : 0.0f;
				}
				m_3DShader.SetFloat(m_3DUAmbientFloorLoc, sAmbFloor);
			}

			// Textures. Base = channel 0; if channel 0 is empty scan
			// 1..N for the first non-zero (shader-driven UI surfaces
			// sometimes park the main texture in a higher slot) -- in
			// that case there is no secondary. Channel 1 on top of a
			// channel-0 base = multitexture (lightmap modulate) -- UI
			// program only; the 3D shader is single-texture by design.
			int UseTex = 0, UseTex1 = 0;
			if (m_pCurAttrib)
			{
				int Tex0 = (int)m_pCurAttrib->m_TextureID[0];
				int Tex1 = bUI ? (int)m_pCurAttrib->m_TextureID[1] : 0;
				if (!Tex0)
				{
					Tex1 = 0;
					for (int c = 1; c < CRC_MAXTEXTURES; ++c)
						if (m_pCurAttrib->m_TextureID[c]) { Tex0 = (int)m_pCurAttrib->m_TextureID[c]; break; }
				}
				DbgNoteTexID(Tex0, 0);
				DbgNoteTexID(Tex1, 1);
				if (Tex0 > 0)
				{
					GLuint T = TextureID_EnsureUploaded(Tex0);
					m_DbgLastBind0 = T;
					if (T)
					{
						glActiveTexture(GL_TEXTURE0);
						glBindTexture(GL_TEXTURE_2D, T);
						Sh.SetInt(LocTex, 0);
						UseTex = 1;
						++m_DbgTexBound;
					}
					else
						++m_DbgTexMissing;
				}
				else
					m_DbgLastBind0 = 0;
				if (bUI && UseTex && Tex1 > 0)
				{
					GLuint T1 = TextureID_EnsureUploaded(Tex1);
					if (T1)
					{
						glActiveTexture(GL_TEXTURE1);
						glBindTexture(GL_TEXTURE_2D, T1);
						m_UIShader.SetInt(m_UTex1Loc, 1);
						UseTex1 = 1;
						++m_DbgTexBound;
					}
					else
						++m_DbgTexMissing;
					glActiveTexture(GL_TEXTURE0);
				}
			}
			// RIDDICK_FORCE_TEX=1 override: bind a bright magenta/cyan
			// checkerboard to unit 0 on every draw. Skip particles/dust/
			// decorations (any pass with BLEND) so full-screen alpha
			// sprites (TheDream dust clouds) don't paint over the world
			// under diagnostic. Also skip 2D UI so HUD stays readable.
			const bool bIs2DFT = m_pCurAttrib && ClassifyUI();
			const bool bBlendFT = m_pCurAttrib && (m_pCurAttrib->m_Flags & CRC_FLAGS_BLEND);
			const bool bDoForceTex = ForceTexEnabled() && !bIs2DFT && !bBlendFT;
			if (bDoForceTex)
			{
				glActiveTexture(GL_TEXTURE0);
				glBindTexture(GL_TEXTURE_2D, GetCheckerTex());
				Sh.SetInt(LocTex, 0);
				UseTex = 1;
				UseTex1 = 0;
				// Also unbias identity texture matrix so raw vUV samples.
				CMat4Dfp32 I; I.Unit();
				Sh.SetMat4(LocTexM, (const float*)&I);
			}
			Sh.SetInt(LocUseT, UseTex);
			if (bUI)
				m_UIShader.SetInt(m_UUseTex1Loc, UseTex1);
			// FORCE_TEX overrides dbg mode + lighting so the checker actually
			// reaches the framebuffer regardless of other flags.
			if (bDoForceTex)
			{
				Sh.SetInt(LocDbg, 0);
				if (bUI)
				{
					m_UIShader.SetInt(m_ULightingModeLoc, 0);
					m_UIShader.SetInt(m_UAlphaFuncLoc, 0);
				}
			}
			else
				Sh.SetInt(LocDbg, bUI ? m_DbgShaderMode : m_Dbg3DShaderMode);
			m_DbgLastUseTex = UseTex;
			m_DbgLastUseTex1 = UseTex1;
		}
		int    m_DbgLastUseTex   = 0;
		int    m_DbgLastUseTex1  = 0;
		GLuint m_DbgLastBind0    = 0;
		// F9-resettable MTX log counters (class members instead of
		// function-local statics so DbgDumpTick can zero them on demand).
		int    m_MtxLog          = 0;
		int    m_UvLog           = 0;
		// F10-armed VBB register-wiring log (see BuildVertsFromVBB).
		// Counter of remaining draws to log; F10 sets it to 32.
		int    m_DbgVBBLogArm    = 0;
		// F10-armed per-draw m_pCurAttrib.TextureID log (world-sized).
		// Independent counter so both logs run to completion.
		int    m_DbgDrawLogArm   = 0;
		int    m_DbgDrawPostArm  = 0;   // post-uniform outcome for same draws

		// Geometry dumper: on RIDDICK_DUMP_OBJ=<dir>, writes each unique
		// drawn mesh to <dir>/geom_XXXX.obj. Keyed by nV + first-vertex
		// hash so the same VB doesn't spam files every frame. Cap 500.
		int         m_DumpObjMax     = 0;
		int         m_DumpObjCount   = 0;
		const char* m_DumpObjDir     = 0;
		TArray<uint32> m_DumpObjSeen;
		static uint32 HashVerts(const SUIVert* _p, int _n, int _mvpTag)
		{
			uint32 h = 2166136261u ^ (uint32)_n ^ (uint32)_mvpTag;
			int nSample = _n < 8 ? _n : 8;
			for (int i = 0; i < nSample; ++i)
			{
				uint32 x = *(const uint32*)&_p[i].x;
				uint32 y = *(const uint32*)&_p[i].y;
				uint32 z = *(const uint32*)&_p[i].z;
				h = (h ^ x) * 16777619u;
				h = (h ^ y) * 16777619u;
				h = (h ^ z) * 16777619u;
			}
			return h;
		}
		void DumpGeomOBJ(const char* _Tag, const SUIVert* _pV, int _nV,
		                 const uint16* _pI, int _nI, GLenum _Prim)
		{
			if (m_DumpObjMax <= 0 || m_DumpObjCount >= m_DumpObjMax) return;
			if (!_pV || _nV <= 0 || !_pI || _nI <= 0) return;
			if (_Prim != GL_TRIANGLES && _Prim != GL_TRIANGLE_STRIP && _Prim != GL_TRIANGLE_FAN)
				return;
			CMat4Dfp32 MVP; m_ModelMat.Multiply(m_ProjMat, MVP);
			int mvpTag = (int)(((const uint32*)&MVP)[0] ^ ((const uint32*)&MVP)[5]);
			uint32 h = HashVerts(_pV, _nV, mvpTag);
			for (int i = 0; i < m_DumpObjSeen.Len(); ++i)
				if (m_DumpObjSeen[i] == h) return;
			m_DumpObjSeen.Add(h);
			static int sApplyMVP = -1;
			if (sApplyMVP < 0)
			{
				const char* e = getenv("RIDDICK_OBJ_APPLY_MVP");
				sApplyMVP = (e && *e && *e != '0') ? 1 : 0;
			}
			char path[512];
			snprintf(path, sizeof(path), "%s/geom_%04d_%s_v%d_i%d%s.obj",
				m_DumpObjDir, m_DumpObjCount, _Tag, _nV, _nI,
				sApplyMVP ? "_mvp" : "");
			FILE* f = fopen(path, "w");
			if (!f) return;
			int Tex0 = m_pCurAttrib ? (int)m_pCurAttrib->m_TextureID[0] : 0;
			fprintf(f, "# openriddick GLES3 geom dump #%d  tag=%s  Tex0=%d  applyMVP=%d\n",
				m_DumpObjCount, _Tag, Tex0, sApplyMVP);
			fprintf(f, "# nV=%d nI=%d prim=0x%04x\n", _nV, _nI, (unsigned)_Prim);
			const float* m = (const float*)&MVP;
			fprintf(f, "# MVP: %g %g %g %g / %g %g %g %g / %g %g %g %g / %g %g %g %g\n",
				m[0],m[1],m[2],m[3], m[4],m[5],m[6],m[7],
				m[8],m[9],m[10],m[11], m[12],m[13],m[14],m[15]);
			for (int i = 0; i < _nV; ++i)
			{
				float x = _pV[i].x, y = _pV[i].y, z = _pV[i].z;
				if (sApplyMVP)
				{
					// Engine is row-vector: clip = v_row * MVP (same numeric
					// layout as GL's column-major M * v_col).
					const float w1 = 1.0f;
					const float cx = x*m[0] + y*m[4] + z*m[8]  + w1*m[12];
					const float cy = x*m[1] + y*m[5] + z*m[9]  + w1*m[13];
					const float cz = x*m[2] + y*m[6] + z*m[10] + w1*m[14];
					const float cw = x*m[3] + y*m[7] + z*m[11] + w1*m[15];
					const float iw = (cw != 0.0f) ? (1.0f / cw) : 1.0f;
					x = cx * iw; y = cy * iw; z = cz * iw;
				}
				fprintf(f, "v %.6f %.6f %.6f\n", x, y, z);
			}
			for (int i = 0; i < _nV; ++i)
				fprintf(f, "vt %.6f %.6f\n", _pV[i].u, _pV[i].v);
			if (_Prim == GL_TRIANGLES)
			{
				for (int i = 0; i + 2 < _nI; i += 3)
				{
					int a = _pI[i] + 1, b = _pI[i+1] + 1, c = _pI[i+2] + 1;
					fprintf(f, "f %d/%d %d/%d %d/%d\n", a,a, b,b, c,c);
				}
			}
			else if (_Prim == GL_TRIANGLE_STRIP)
			{
				for (int i = 0; i + 2 < _nI; ++i)
				{
					int a = _pI[i] + 1, b = _pI[i+1] + 1, c = _pI[i+2] + 1;
					if (i & 1) { int t = b; b = c; c = t; }
					if (a==b || b==c || a==c) continue;
					fprintf(f, "f %d/%d %d/%d %d/%d\n", a,a, b,b, c,c);
				}
			}
			else
			{
				int a0 = _pI[0] + 1;
				for (int i = 1; i + 1 < _nI; ++i)
				{
					int b = _pI[i] + 1, c = _pI[i+1] + 1;
					fprintf(f, "f %d/%d %d/%d %d/%d\n", a0,a0, b,b, c,c);
				}
			}
			fclose(f);
			++m_DumpObjCount;
			if (m_DumpObjCount == m_DumpObjMax)
				fprintf(stderr, "[GEOM-DUMP] hit cap %d files in %s\n",
					m_DumpObjMax, m_DumpObjDir);
		}

		// Common draw: submits _nInd 16-bit indices with GL primitive
		// _GLPrim, using the currently-set geometry (m_Geom) + attrib
		// (m_pCurAttrib) + captured matrices.
		void DrawIndexed(GLenum _GLPrim, uint16* _pInd, int _nInd)
		{
			if (!_pInd || _nInd <= 0) return;
			if (!m_bGLInited) InitGLResources();
			if (!m_UIShader.IsValid() && !m_3DShader.IsValid()) return;

			// Flush deferred attrib/matrix state (mirrors the PS3
			// backend: engine mutates its own stack, then expects
			// the backend to reify GL state at draw time). Without
			// this our virtual Attrib_Set/Matrix_SetRender never
			// fire and m_pCurAttrib stays null -> no texture ever
			// binds, m_TextureID lookups all return 0.
			if (m_AttribChanged) Attrib_Update();
			if (m_MatrixChanged) Matrix_Update();

			// F10-armed: for the next N draws print what m_pCurAttrib
			// has in its texture slots. Answers "does the diffuse ID we
			// set in WBSP2Model reach the drawcall, or was pVB->m_pAttrib
			// rebound elsewhere?" BSP2 walls draw in small batches
			// (30..100 indices) so we log every drawcall size.
			if (m_DbgDrawLogArm > 0)
			{
				unsigned t0 = m_pCurAttrib ? (unsigned)m_pCurAttrib->m_TextureID[0] : 0;
				unsigned t1 = m_pCurAttrib ? (unsigned)m_pCurAttrib->m_TextureID[1] : 0;
				unsigned t2 = m_pCurAttrib ? (unsigned)m_pCurAttrib->m_TextureID[2] : 0;
				unsigned t3 = m_pCurAttrib ? (unsigned)m_pCurAttrib->m_TextureID[3] : 0;
				unsigned F  = m_pCurAttrib ? (unsigned)m_pCurAttrib->m_Flags : 0;
				fprintf(stderr, "[DRAW] nInd=%d pCurAttrib=%p tex=[%u %u %u %u] Flags=0x%08x\n",
					_nInd, (void*)m_pCurAttrib, t0, t1, t2, t3, F);
				fflush(stderr);
				--m_DbgDrawLogArm;
			}

			// ---- Fast path A: geometry already resident on the GPU ----
			// The in-game world path is Geometry_VertexBuffer(VBID) followed
			// by a stream of Render_Indexed* calls. Previously every one of
			// those re-fetched and re-converted the WHOLE cluster
			// (BuildInterleavedVerts -> VB_Get + malloc + scalar VRegFetch),
			// measured at ~3M vertex conversions/frame for ~81k drawn
			// indices. With the VBID cached in a static VBO we only have to
			// stream this draw's indices.
			if (!GLES3_NoVBCache() && m_GeomVBID != 0 && !GLES3_TestTri())
			{
				if (const SGLES3GeomEntry* pE = m_GeomCache.Ensure((int)m_GeomVBID))
				{
					if (pE->m_VBO && pE->m_nV > 0)
					{
						if (DrawIndexed_ShouldSkip(pE->m_nV)) return;
						glBindVertexArray(m_VAO);
						CGLES3VBOStreamer::SPushResult iRes = m_Streamer.PushIndices(_pInd, _nInd * (int)sizeof(uint16));
						if (iRes.Ok)
						{
							glBindBuffer(GL_ARRAY_BUFFER, pE->m_VBO);
							SetVertexAttribPointersFromEntry(*pE);
							SetupCommonUniforms(true);

							// F10-armed: the legacy path prints [DRAW/post]
							// after SetupCommonUniforms; do the same here or
							// world draws (which all come through this path)
							// silently lose that diagnostic. Also dump the
							// cached entry's register layout -- answers
							// "did this VBID even carry a UV register, and
							// which set fed channel 0" without a GPU readback.
							if (m_DbgDrawPostArm > 0)
							{
								int UVSet0 = m_pCurAttrib ? m_pCurAttrib->m_iTexCoordSet[0] : 0;
								if (UVSet0 >= CRC_MAXTEXCOORDS) UVSet0 = 0;
								fprintf(stderr,
									"  [DRAW/cached] VBID=%d nV=%d stride=%d UVset0=%d "
									"off{pos=%d uv=%d uv1=%d col=%d nrm=%d} UseTex=%d boundGL=%u\n",
									(int)m_GeomVBID, pE->m_nV, pE->m_Stride, UVSet0,
									pE->m_lRegOffset[CRC_VREG_POS],
									pE->m_lRegOffset[CRC_VREG_TEXCOORD0 + UVSet0],
									pE->m_lRegOffset[CRC_VREG_TEXCOORD0 + 1],
									pE->m_lRegOffset[CRC_VREG_COLOR],
									pE->m_lRegOffset[CRC_VREG_NORMAL],
									m_DbgLastUseTex, (unsigned)m_DbgLastBind0);
								fflush(stderr);
								--m_DbgDrawPostArm;
							}
							m_DbgTotalVerts += pE->m_nV;
							m_DbgTotalIdx   += _nInd;
							m_DbgVMemo      += pE->m_nV;
							glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, iRes.Buffer);
							const GLenum CachedPrim = m_DbgForceWire ? GL_LINE_STRIP : _GLPrim;
							glDrawElements(CachedPrim, _nInd, GL_UNSIGNED_SHORT, (const void*)(intptr_t)iRes.ByteOffset);
							DisableVertexAttribPointers();
							glBindVertexArray(0);
							++m_DbgDrawCached;
							return;
						}
						glBindVertexArray(0);
					}
				}
			}

			// ---- Fast path B: same CPU geometry as the previous draw ----
			// Key = the per-draw inputs that change what BuildInterleavedVerts
			// produces. The "did the engine hand us new geometry" question is
			// answered authoritatively by the Geometry_* overrides above,
			// which clear m_GeomMemo.m_bValid; the pointer/count comparison
			// here is a second line of defence.
			int MemoUVSet0 = 0, MemoUVSet1 = 1;
			if (m_pCurAttrib)
			{
				MemoUVSet0 = m_pCurAttrib->m_iTexCoordSet[0];
				MemoUVSet1 = m_pCurAttrib->m_iTexCoordSet[1];
				if (MemoUVSet0 >= CRC_MAXTEXCOORDS) MemoUVSet0 = 0;
				if (MemoUVSet1 >= CRC_MAXTEXCOORDS) MemoUVSet1 = 1;
			}
			const bool bMemoWhite = GLES3_NoLight() && !ClassifyUI();
			// RIDDICK_NO_VBCACHE=1 turns off BOTH fast paths, so the flag
			// is a single clean A/B switch back to "convert everything,
			// every draw, every frame".
			const bool bMemoUsable = (m_GeomVBID == 0) && !GLES3_TestTri() && !GLES3_NoVBCache();
			bool bMemoHit = false;
			CGLES3VBOStreamer::SPushResult vRes; vRes.Ok = false; vRes.Buffer = 0; vRes.ByteOffset = 0;

			SUIVert* pVerts = 0; int nVerts = 0; bool bMalloced = false;
			if (bMemoUsable && m_GeomMemo.m_bValid &&
			    m_GeomMemo.m_Gen      == m_Streamer.GetVBGeneration() &&
			    m_GeomMemo.m_pV       == (const void*)m_Geom.m_pV &&
			    m_GeomMemo.m_nV       == (int)m_Geom.m_nV &&
			    m_GeomMemo.m_pTV0     == (const void*)m_Geom.m_pTV[MemoUVSet0] &&
			    m_GeomMemo.m_pTV1     == (const void*)m_Geom.m_pTV[MemoUVSet1] &&
			    m_GeomMemo.m_pCol     == (const void*)m_Geom.m_pCol &&
			    m_GeomMemo.m_pN       == (const void*)m_Geom.m_pN &&
			    m_GeomMemo.m_UVSet0   == MemoUVSet0 &&
			    m_GeomMemo.m_UVSet1   == MemoUVSet1 &&
			    m_GeomMemo.m_bWhite   == bMemoWhite)
			{
				bMemoHit   = true;
				nVerts     = m_GeomMemo.m_nV;
				vRes.Ok    = true;
				vRes.Buffer     = m_GeomMemo.m_Buffer;
				vRes.ByteOffset = m_GeomMemo.m_ByteOffset;
				m_DbgVMemo += nVerts;
			}
			else
			{
				if (!BuildInterleavedVerts(pVerts, nVerts, bMalloced)) return;
				m_DbgVConv += nVerts;
			}

			if (DrawIndexed_ShouldSkip(nVerts))
			{
				FreeScratch(pVerts, nVerts, bMalloced);
				return;
			}

			// One-shot log: Model, Proj, MVP and vertex[0] → NDC for the
			// first 5 drawcalls of the first frame after startmap load.
			// Diagnoses which matrix mangles vertices.
			int sMtxLog = m_MtxLog; // kept for legacy references below
			// Only fire for draws with non-identity Model (i.e. real
			// world geometry, not UI). UI keeps Model=Unit.
			const float* mmChk = (const float*)&m_ModelMat;
			const bool bModelId = (mmChk[0]==1 && mmChk[5]==1 && mmChk[10]==1 && mmChk[15]==1
				&& mmChk[1]==0 && mmChk[2]==0 && mmChk[3]==0
				&& mmChk[4]==0 && mmChk[6]==0 && mmChk[7]==0
				&& mmChk[8]==0 && mmChk[9]==0 && mmChk[11]==0
				&& mmChk[12]==0 && mmChk[13]==0 && mmChk[14]==0);
			// Log next 10 non-UI drawcalls (world meshes have non-identity
			// Model). F9 resets m_MtxLog to 0 to re-arm.
			(void)sMtxLog;
			if (pVerts && m_MtxLog < 10 && nVerts >= 16 && !bModelId && getenv("RIDDICK_DBG_MTX"))
			{
				const float* mm = (const float*)&m_ModelMat;
				const float* mp = (const float*)&m_ProjMat;
				CMat4Dfp32 MVP; m_ModelMat.Multiply(m_ProjMat, MVP);
				const float* mv = (const float*)&MVP;
				const float x = pVerts[0].x, y = pVerts[0].y, z = pVerts[0].z;
				const float cx = x*mv[0] + y*mv[4] + z*mv[8]  + mv[12];
				const float cy = x*mv[1] + y*mv[5] + z*mv[9]  + mv[13];
				const float cz = x*mv[2] + y*mv[6] + z*mv[10] + mv[14];
				const float cw = x*mv[3] + y*mv[7] + z*mv[11] + mv[15];
				fprintf(stderr, "[MTX #%d] nV=%d v0=(%.2f,%.2f,%.2f)\n"
					"  Model rows: [%g %g %g %g][%g %g %g %g][%g %g %g %g][%g %g %g %g]\n"
					"  Proj  rows: [%g %g %g %g][%g %g %g %g][%g %g %g %g][%g %g %g %g]\n"
					"  clip=(%g,%g,%g,%g) NDC=(%g,%g,%g)\n",
					m_MtxLog, nVerts, x, y, z,
					mm[0],mm[1],mm[2],mm[3], mm[4],mm[5],mm[6],mm[7],
					mm[8],mm[9],mm[10],mm[11], mm[12],mm[13],mm[14],mm[15],
					mp[0],mp[1],mp[2],mp[3], mp[4],mp[5],mp[6],mp[7],
					mp[8],mp[9],mp[10],mp[11], mp[12],mp[13],mp[14],mp[15],
					cx, cy, cz, cw,
					cw != 0 ? cx/cw : 0, cw != 0 ? cy/cw : 0, cw != 0 ? cz/cw : 0);
				fflush(stderr);
				++m_MtxLog;
			}

			// Bisect diagnostic: RIDDICK_TEST_TRI=1 → replace vertex data
			// and MVP with a known-good triangle at identity MVP. If a
			// centered white triangle appears where the game normally
			// draws, our pipeline (shader/attribs/streamer/blend) is fine,
			// so the bug must be in engine → SUIVert conversion or MVP.
			// If nothing (or garbage) shows — pipeline itself is broken.
			// TEST_TRI=1: identity MVP + RGB triangle (proves pipeline).
			// TEST_TRI=2: keep engine MVP + RGB triangle (proves whether
			// engine MVP places geometry in a sane on-screen location).
			static int sTestTri = -1;
			if (sTestTri < 0)
			{
				const char* e = getenv("RIDDICK_TEST_TRI");
				sTestTri = (e && *e) ? atoi(e) : 0;
			}
			// A world-scale triangle (units ≈ 1 meter engine scale) so
			// that when engine's world-view MVP is applied the tri lands
			// somewhere sensible in a real map.
			static SUIVert sTri[3] = {
				{ -1.0f, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0xff0000ffu },
				{  1.0f, -1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0xff00ff00u },
				{  0.0f,  1.0f, 0.0f, 0.5f, 1.0f, 0.0f, 0.0f, 0xffff0000u },
			};
			static uint16 sTriIdx[3] = { 0, 1, 2 };
			if (sTestTri)
			{
				FreeScratch(pVerts, nVerts, bMalloced);
				pVerts = sTri; nVerts = 3; bMalloced = false;
				_pInd = sTriIdx; _nInd = 3;
				if (sTestTri == 1)
				{
					m_ModelMat.Unit();
					m_ProjMat.Unit();
				}
				// mode 2: leave m_ModelMat / m_ProjMat untouched → uses
				// engine's per-drawable MVP. Every drawcall becomes a
				// tiny 2m triangle at that drawable's world position.
			}

			glBindVertexArray(m_VAO);
			// On a memo hit vRes already points at the block pushed by an
			// earlier draw with the same geometry -- skip the re-upload.
			if (!bMemoHit)
				vRes = m_Streamer.PushVertices(pVerts, nVerts * (int)sizeof(SUIVert));
			CGLES3VBOStreamer::SPushResult iRes = m_Streamer.PushIndices (_pInd,  _nInd  * (int)sizeof(uint16));
			if (!vRes.Ok || !iRes.Ok) { FreeScratch(pVerts, nVerts, bMalloced); return; }

			// Remember this upload so the rest of the primitive stream can
			// reuse it. Not done for the TEST_TRI bisect geometry (which
			// substitutes its own vertices) -- bMemoUsable already covers
			// that, as it does the VBID path.
			if (!bMemoHit && bMemoUsable)
			{
				m_GeomMemo.m_bValid     = true;
				m_GeomMemo.m_pV         = (const void*)m_Geom.m_pV;
				m_GeomMemo.m_pTV0       = (const void*)m_Geom.m_pTV[MemoUVSet0];
				m_GeomMemo.m_pTV1       = (const void*)m_Geom.m_pTV[MemoUVSet1];
				m_GeomMemo.m_pCol       = (const void*)m_Geom.m_pCol;
				m_GeomMemo.m_pN         = (const void*)m_Geom.m_pN;
				m_GeomMemo.m_nV         = nVerts;
				m_GeomMemo.m_UVSet0     = MemoUVSet0;
				m_GeomMemo.m_UVSet1     = MemoUVSet1;
				m_GeomMemo.m_bWhite     = bMemoWhite;
				m_GeomMemo.m_Buffer     = vRes.Buffer;
				m_GeomMemo.m_ByteOffset = vRes.ByteOffset;
				m_GeomMemo.m_Gen        = m_Streamer.GetVBGeneration();
			}

			// Attribs (VBO already bound by PushVertices).
			glBindBuffer(GL_ARRAY_BUFFER, vRes.Buffer);
			SetVertexAttribPointers((intptr_t)vRes.ByteOffset);

			SetupCommonUniforms(true);
			m_DbgTotalVerts += nVerts;
			m_DbgTotalIdx   += _nInd;

			// F10-armed post-uniform state (paired with [DRAW] above):
			// what the shader actually sees for this draw. If the pre-log
			// showed tex=[3918 ..] but post shows UseTex=0 boundGL=0, then
			// TextureID_EnsureUploaded failed for that ID on this thread /
			// this frame -- narrows the bug from "attrib routing" to
			// "texture upload lookup".
			if (m_DbgDrawPostArm > 0)
			{
				fprintf(stderr, "  [DRAW/post] UseTex=%d boundGL=%u lastUseTex1=%d\n",
					m_DbgLastUseTex, (unsigned)m_DbgLastBind0, m_DbgLastUseTex1);
				fflush(stderr);
				--m_DbgDrawPostArm;
			}

			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, iRes.Buffer);
			GLenum DrawPrim = m_DbgForceWire ? GL_LINE_STRIP : _GLPrim;
			glDrawElements(DrawPrim, _nInd, GL_UNSIGNED_SHORT, (const void*)(intptr_t)iRes.ByteOffset);

			DbgDumpDraw("DrawIndexed", DrawPrim, pVerts, nVerts, _pInd, _nInd);
			DumpGeomOBJ("DrawIndexed", pVerts, nVerts, _pInd, _nInd, _GLPrim);
			FreeScratch(pVerts, nVerts, bMalloced);

			DisableVertexAttribPointers();
			glBindVertexArray(0);
		}

		// Frame-dumper: writes one line + first-vertex sample per drawcall
		// during the target frame, to /tmp/openriddick_frame.txt. Called
		// from DrawIndexed and DrawUserVerts. pVerts may be NULL if the
		// caller freed it already; then only counts + MVP are dumped.
		void DbgDumpDraw(const char* _Tag, GLenum _GLPrim,
		                 const SUIVert* _pVerts, int _nVerts,
		                 const uint16* _pInd, int _nInd)
		{
			if (!m_DbgDumpActive || !m_DbgDumpFp) return;
			if (m_DbgDumpDrawIdx == 0)
			{
				fprintf(m_DbgDumpFp,
					"UNIFORM LOCATIONS: uMVP=%d uUseTex=%d uUseTex1=%d "
					"uTex=%d uTex1=%d uDbgMode=%d uAlphaFunc=%d\n",
					m_UMVPLoc, m_UUseTexLoc, m_UUseTex1Loc,
					m_UTexLoc, m_UTex1Loc, m_UDbgModeLoc, m_UAlphaFuncLoc);
			}
			CMat4Dfp32 MVP;
			m_ModelMat.Multiply(m_ProjMat, MVP);
			uint32 F = m_pCurAttrib ? m_pCurAttrib->m_Flags : 0;
			GLint CurFBO = 0;
			glGetIntegerv(GL_FRAMEBUFFER_BINDING, &CurFBO);
			int BlendSrc = -1, BlendDst = -1;
			if (m_pCurAttrib && (F & CRC_FLAGS_BLEND))
			{
				const uint16 SD = m_pCurAttrib->m_SourceDestBlend;
				BlendSrc = SD & 0xff; BlendDst = (SD >> 8) & 0xff;
			}
			fprintf(m_DbgDumpFp,
				"[#%d] %s prim=0x%04x nV=%d nI=%d VBID=%u "
				"flags=0x%08x ZTest=%d ZWrite=%d ColW=%d Cull=%s Blend=%d(%d/%d) "
				"Stencil=%d AlphaCmp=%d FBO=%d RTT=%d\n",
				m_DbgDumpDrawIdx, _Tag, (unsigned)_GLPrim, _nVerts, _nInd,
				(unsigned)m_GeomVBID,
				F,
				(F & CRC_FLAGS_ZCOMPARE) ? 1 : 0,
				(F & CRC_FLAGS_ZWRITE)   ? 1 : 0,
				(F & CRC_FLAGS_COLORWRITE) ? 1 : 0,
				(F & CRC_FLAGS_CULL) ? ((F & CRC_FLAGS_CULLCW) ? "CW" : "CCW") : "off",
				(F & CRC_FLAGS_BLEND)    ? 1 : 0, BlendSrc, BlendDst,
				(F & CRC_FLAGS_STENCIL)  ? 1 : 0,
				m_pCurAttrib ? (int)m_pCurAttrib->m_AlphaCompare : -1,
				(int)CurFBO, m_bRTTActive ? 1 : 0);
			// VBID histogram for end-of-frame summary.
			if ((unsigned)m_GeomVBID > 0)
			{
				DbgVBIDBumpCount((unsigned)m_GeomVBID);
			}
			// All texture slots + whether the engine's TextureID has an
			// uploaded GL name in our cache.
			fprintf(m_DbgDumpFp, "  Tex:");
			for (int s = 0; s < 4 && m_pCurAttrib; ++s)
			{
				int Tid = (int)m_pCurAttrib->m_TextureID[s];
				GLuint GLTex = 0;
				if (Tid > 0 && Tid < (int)m_lGLTex.Len()) GLTex = m_lGLTex[Tid];
				// Also call the exact same path the draw uses right now,
				// so we see what the shader actually got (placeholder etc.).
				GLuint LiveTex = (Tid > 0) ? TextureID_EnsureUploaded(Tid) : 0;
				fprintf(m_DbgDumpFp, " [%d]=%d(cache=%u live=%u)", s, Tid,
					(unsigned)GLTex, (unsigned)LiveTex);
			}
			fprintf(m_DbgDumpFp, " placeholder=%u\n", (unsigned)m_PlaceholderTex);
			fprintf(m_DbgDumpFp, "  SHADER: UseTex=%d UseTex1=%d BoundGL0=%u\n",
				m_DbgLastUseTex, m_DbgLastUseTex1, (unsigned)m_DbgLastBind0);
			const float* m = (const float*)&MVP;
			fprintf(m_DbgDumpFp,
				"  MVP: [%8.3f %8.3f %8.3f %8.3f]\n"
				"       [%8.3f %8.3f %8.3f %8.3f]\n"
				"       [%8.3f %8.3f %8.3f %8.3f]\n"
				"       [%8.3f %8.3f %8.3f %8.3f]\n",
				m[0],m[1],m[2],m[3], m[4],m[5],m[6],m[7],
				m[8],m[9],m[10],m[11], m[12],m[13],m[14],m[15]);
			if (_pVerts && _nVerts > 0)
			{
				float mnx=_pVerts[0].x,mny=_pVerts[0].y,mnz=_pVerts[0].z;
				float mxx=mnx,mxy=mny,mxz=mnz;
				for (int i=1;i<_nVerts;++i)
				{
					const SUIVert& v = _pVerts[i];
					if (v.x<mnx)mnx=v.x; if (v.x>mxx)mxx=v.x;
					if (v.y<mny)mny=v.y; if (v.y>mxy)mxy=v.y;
					if (v.z<mnz)mnz=v.z; if (v.z>mxz)mxz=v.z;
				}
				fprintf(m_DbgDumpFp,
					"  Vert bbox: [%.3f..%.3f, %.3f..%.3f, %.3f..%.3f]\n",
					mnx,mxx,mny,mxy,mnz,mxz);
				int nShow = _nVerts < 3 ? _nVerts : 3;
				for (int i = 0; i < nShow; ++i)
				{
					const SUIVert& v = _pVerts[i];
					fprintf(m_DbgDumpFp,
						"    V[%d] pos=(%.3f,%.3f,%.3f) uv=(%.3f,%.3f) col=0x%08x\n",
						i, v.x, v.y, v.z, v.u, v.v, v.col);
				}
			}
			if (_pInd && _nInd > 0)
			{
				int nShow = _nInd < 9 ? _nInd : 9;
				fprintf(m_DbgDumpFp, "  Idx[0..%d]:", nShow-1);
				for (int i = 0; i < nShow; ++i) fprintf(m_DbgDumpFp, " %u", (unsigned)_pInd[i]);
				fprintf(m_DbgDumpFp, "\n");
			}
			++m_DbgDumpDrawIdx;
		}

		void Render_IndexedTriangles(uint16* _pTriVertIndices, int _nTriangles)
		{
			// nTriangles == 0xffff is the "list of lists" recursion
			// convention (see PS3 backend). Handle it too so BSP4 can
			// batch. For M3 (UI) it will effectively never trigger.
			if (_nTriangles == 0xffff)
			{
				mint* pList = (mint*)_pTriVertIndices;
				int nLists = (int)*pList++;
				for (int i = 0; i < nLists; ++i)
				{
					int nTri = (int)*pList++;
					Render_IndexedTriangles(*((uint16**)pList), nTri);
					++pList;
				}
				return;
			}
			++m_DbgDrawTri;
			DrawIndexed(GL_TRIANGLES, _pTriVertIndices, _nTriangles * 3);
		}

		void Render_IndexedTriangleStrip(uint16* _pIndices, int _Len)
		{
			++m_DbgDrawStrip;
			DrawIndexed(GL_TRIANGLE_STRIP, _pIndices, _Len);
		}

		void Render_IndexedWires(uint16* _pIndices, int _Len)
		{
			++m_DbgDrawWire;
			DrawIndexed(GL_LINES, _pIndices, _Len);
		}

		void Render_IndexedPolygon(uint16* _pIndices, int _Len)
		{
			++m_DbgDrawPoly;
			// GLES has no GL_POLYGON. Treat as triangle fan; caller
			// generally sends convex fan-friendly ordering.
			DrawIndexed(GL_TRIANGLE_FAN, _pIndices, _Len);
		}

		void Render_IndexedPrimitives(uint16* _pPrimStream, int _StreamLen)
		{
			++m_DbgDrawPrim;
			if (!_pPrimStream || _StreamLen <= 0) return;

			// Collapse the whole stream into ONE triangle-list draw when
			// every primitive in it is triangle-ish. The engine sends one
			// stream per BSP2 cluster with dozens of primitives in it; the
			// per-primitive loop below turned each of those into a separate
			// DrawIndexed (and, before the geometry cache/memo, a separate
			// full re-conversion of the cluster's vertices). Flattening is
			// exactly what the PS3 backend does at VB build time --
			// CContext_Geometry::Build, MRenderPS3_Geometry.cpp.
			{
				bool bAllTri = true;
				CRCPrimStreamIterator Scan(_pPrimStream, _StreamLen);
				if (!Scan.IsValid()) return;
				do
				{
					const int T = Scan.GetCurrentType();
					if (T != CRC_RIP_TRIANGLES && T != CRC_RIP_TRISTRIP && T != CRC_RIP_TRIFAN)
					{
						bAllTri = false;
						break;
					}
				}
				while (Scan.Next());

				if (bAllTri)
				{
					CRCPrimStreamIterator CountIt(_pPrimStream, _StreamLen);
					const int nMax = CRC_Core::Geometry_BuildTriangleListFromPrimitivesCount(CountIt);
					if (nMax <= 0) return;
					if (m_lFlatIdx.Len() < nMax) m_lFlatIdx.SetLen(nMax);

					CRCPrimStreamIterator It2(_pPrimStream, _StreamLen);
					int iDst = 0;
					uint16 lChunk[1024 * 3];
					while (It2.IsValid())
					{
						int nChunk = 1024 * 3;
						const bool bDone = CRC_Core::Geometry_BuildTriangleListFromPrimitives(It2, lChunk, nChunk);
						if (nChunk > 0 && iDst + nChunk <= nMax)
						{
							memcpy(&m_lFlatIdx[iDst], lChunk, sizeof(uint16) * (size_t)nChunk);
							iDst += nChunk;
						}
						if (bDone) break;
					}
					if (iDst > 0)
						DrawIndexed(GL_TRIANGLES, m_lFlatIdx.GetBasePtr(), iDst);
					return;
				}
			}

			CRCPrimStreamIterator It(_pPrimStream, _StreamLen);
			if (!It.IsValid()) return;
			do
			{
				const uint16* pPrim = It.GetCurrentPointer();
				int nInd = *pPrim;
				GLenum Prim = 0;
				switch (It.GetCurrentType())
				{
				case CRC_RIP_TRIANGLES: Prim = GL_TRIANGLES;      nInd *= 3; break;
				case CRC_RIP_TRISTRIP:  Prim = GL_TRIANGLE_STRIP;            break;
				case CRC_RIP_TRIFAN:    Prim = GL_TRIANGLE_FAN;              break;
				default: break;
				}
				if (Prim && nInd > 0)
					DrawIndexed(Prim, (uint16*)(pPrim + 1), nInd);
			}
			while (It.Next());
		}

		// Geometry setters: the engine calls one of these, then issues a
		// batch of Render_Indexed* draws against it. That transition is
		// the authoritative "the vertex data changed" signal, so it is
		// where the m_Geom memo dies. Overriding costs nothing (these are
		// already virtual in CRenderContext) and does NOT change the
		// vtable layout, so no full-tree rebuild is needed.
		void Geometry_VertexBuffer(const CRC_VertexBuffer& _VB, int _bAllUsed)
		{
			CRC_Core::Geometry_VertexBuffer(_VB, _bAllUsed);
			m_GeomMemo.m_bValid = false;
		}
		void Geometry_VertexBuffer(int _VBID, int _bAllUsed)
		{
			CRC_Core::Geometry_VertexBuffer(_VBID, _bAllUsed);
			m_GeomMemo.m_bValid = false;
		}
		void Geometry_Clear()
		{
			CRC_Core::Geometry_Clear();
			m_GeomMemo.m_bValid = false;
		}

		// Per-draw skip filters that depend only on the attribute state
		// and the vertex count (not on the CPU vertex data), factored out
		// of DrawIndexed so the cached/memo fast paths apply exactly the
		// same rules as the legacy path. Returns true = drop this draw.
		bool DrawIndexed_ShouldSkip(int _nVerts)
		{
			// RIDDICK_ONLY_BSP=1: whitelist only BSP2 world-cluster draws.
			// World clusters are large (nV>=500); UI/particles/tiny meshes
			// are small. This is a positive filter — one flag instead of
			// combining multiple SKIP_* flags.
			static int sOnlyBSP = -1;
			if (sOnlyBSP < 0)
			{
				const char* e = getenv("RIDDICK_ONLY_BSP");
				sOnlyBSP = (e && *e && *e != '0') ? 1 : 0;
			}
			if (sOnlyBSP && _nVerts < 100)
				return true;

			// DIRECT_RENDER effect-skip: drop drawcalls whose Tex0 samples
			// one of our RTT slots (ResolveScreen, DeferredNormal/Diffuse/
			// Specular, MotionMap, ShadowMask, Depth*, etc — engine
			// snapshots backbuffer to these, then re-draws fullscreen with
			// the snapshot as a texture; XREngine.cpp:3019, 3092, 3862+).
			// Under DIRECT_RENDER those slots are never populated → sample
			// returns placeholder = magenta screen. Skip = show raw world.
			// (Belt-and-braces: the engine-side gates in XREngine.cpp /
			// WClientMod.cpp already remove the producers of such quads.)
			if (GLES3_DirectRender() && m_pCurAttrib)
			{
				for (int s = 0; s < 4; ++s)
				{
					const int Tid = (int)m_pCurAttrib->m_TextureID[s];
					if (Tid > 0 && Tid < (int)m_lFBO.Len() && m_lFBO[Tid].m_FBO)
						return true;
				}

				// DIRECT_RENDER pass filter for 3D. UI (2D model matrix)
				// bypasses the filter. RIDDICK_DIRECT_PASS:
				//   both    (default) — no filter, draw everything
				//   solid   — keep only ColW && ZWrite (base-diffuse
				//             pass; my WBSP2Model hack enables COLORWRITE
				//             on the shader-Z base attrib so it becomes
				//             a full color+depth pass).
				//   overlay — keep only ColW && !ZWrite (alpha-blend
				//             detail overlay pass; often carries the
				//             actual UV/textured decal).
				//   skipz   — skip ColW=0 (Z/stencil-only prepass).
				static int sPassMode = -1;   // 0 both, 1 solid, 2 overlay, 3 skipz
				if (sPassMode < 0)
				{
					const char* e = getenv("RIDDICK_DIRECT_PASS");
					sPassMode = 0;
					if (e)
					{
						if      (strcmp(e, "solid")   == 0) sPassMode = 1;
						else if (strcmp(e, "overlay") == 0) sPassMode = 2;
						else if (strcmp(e, "skipz")   == 0) sPassMode = 3;
					}
				}
				const uint32 F = m_pCurAttrib->m_Flags;
				const bool bIs2D = ClassifyUI();
				if (!bIs2D && sPassMode != 0)
				{
					const bool bColW   = (F & CRC_FLAGS_COLORWRITE) != 0;
					const bool bZWrite = (F & CRC_FLAGS_ZWRITE)     != 0;
					if      (sPassMode == 1) return !(bColW && bZWrite);
					else if (sPassMode == 2) return !(bColW && !bZWrite);
					else if (sPassMode == 3) return !bColW;
				}
			}
			return false;
		}

		// RIDDICK_TEST_TRI bisect mode replaces real geometry with a
		// hardcoded triangle, so both fast paths must stand aside for it.
		static bool GLES3_TestTri()
		{
			static int s = -1;
			if (s < 0)
			{
				const char* e = getenv("RIDDICK_TEST_TRI");
				s = (e && *e) ? atoi(e) : 0;
			}
			return s != 0;
		}

		// Draw an already-built interleaved vertex array with explicit
		// indices (used by the VBID path; DrawIndexed keeps its own
		// m_Geom-based flow).
		void DrawUserVerts(GLenum _GLPrim, const SUIVert* _pVerts, int _nVerts, const uint16* _pInd, int _nInd)
		{
			if (!_pVerts || _nVerts <= 0 || !_pInd || _nInd <= 0) return;
			if (!m_bGLInited) InitGLResources();
			if (!m_UIShader.IsValid() && !m_3DShader.IsValid()) return;
			// ONLY_BSP filter (same threshold as DrawIndexed).
			if (getenv("RIDDICK_ONLY_BSP") && _nVerts < 100) return;
			if (m_AttribChanged) Attrib_Update();
			if (m_MatrixChanged) Matrix_Update();

			// Diagnostic: world BSP2 draws come through here via
			// Render_VertexBuffer_IndexBufferTriangles. Log 10 world-ish
			// draws to see actual MVP + vertex → NDC path once the map is up.
			int sUvLog = m_UvLog; // kept for legacy references below
			const float* mmChk = (const float*)&m_ModelMat;
			const bool bModelId2 = (mmChk[0]==1 && mmChk[5]==1 && mmChk[10]==1 && mmChk[15]==1
				&& mmChk[1]==0 && mmChk[2]==0 && mmChk[3]==0
				&& mmChk[4]==0 && mmChk[6]==0 && mmChk[7]==0
				&& mmChk[8]==0 && mmChk[9]==0 && mmChk[11]==0
				&& mmChk[12]==0 && mmChk[13]==0 && mmChk[14]==0);
			(void)sUvLog;
			if (m_UvLog < 10 && _nVerts >= 16 && !bModelId2 && getenv("RIDDICK_DBG_MTX"))
			{
				const float* mm = (const float*)&m_ModelMat;
				const float* mp = (const float*)&m_ProjMat;
				CMat4Dfp32 MVP; m_ModelMat.Multiply(m_ProjMat, MVP);
				const float* mv = (const float*)&MVP;
				const SUIVert& v = _pVerts[0];
				const float cx = v.x*mv[0]+v.y*mv[4]+v.z*mv[8] +mv[12];
				const float cy = v.x*mv[1]+v.y*mv[5]+v.z*mv[9] +mv[13];
				const float cz = v.x*mv[2]+v.y*mv[6]+v.z*mv[10]+mv[14];
				const float cw = v.x*mv[3]+v.y*mv[7]+v.z*mv[11]+mv[15];
				fprintf(stderr, "[UV-MTX #%d] nV=%d prim=0x%04x v0=(%.2f,%.2f,%.2f)\n"
					"  Model: [%g %g %g %g][%g %g %g %g][%g %g %g %g][%g %g %g %g]\n"
					"  Proj : [%g %g %g %g][%g %g %g %g][%g %g %g %g][%g %g %g %g]\n"
					"  clip=(%g,%g,%g,%g) NDC=(%g,%g,%g)\n",
					m_UvLog, _nVerts, (unsigned)_GLPrim, v.x, v.y, v.z,
					mm[0],mm[1],mm[2],mm[3], mm[4],mm[5],mm[6],mm[7],
					mm[8],mm[9],mm[10],mm[11], mm[12],mm[13],mm[14],mm[15],
					mp[0],mp[1],mp[2],mp[3], mp[4],mp[5],mp[6],mp[7],
					mp[8],mp[9],mp[10],mp[11], mp[12],mp[13],mp[14],mp[15],
					cx,cy,cz,cw, cw!=0?cx/cw:0, cw!=0?cy/cw:0, cw!=0?cz/cw:0);
				fflush(stderr);
				++m_UvLog;
			}

			glBindVertexArray(m_VAO);
			CGLES3VBOStreamer::SPushResult vRes = m_Streamer.PushVertices(_pVerts, _nVerts * (int)sizeof(SUIVert));
			CGLES3VBOStreamer::SPushResult iRes = m_Streamer.PushIndices (_pInd,  _nInd  * (int)sizeof(uint16));
			if (!vRes.Ok || !iRes.Ok) return;

			glBindBuffer(GL_ARRAY_BUFFER, vRes.Buffer);
			SetVertexAttribPointers((intptr_t)vRes.ByteOffset);

			SetupCommonUniforms(false);
			m_DbgTotalVerts += _nVerts;
			m_DbgTotalIdx   += _nInd;

			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, iRes.Buffer);
			GLenum DrawPrim = m_DbgForceWire ? GL_LINE_STRIP : _GLPrim;
			glDrawElements(DrawPrim, _nInd, GL_UNSIGNED_SHORT, (const void*)(intptr_t)iRes.ByteOffset);
			DbgDumpDraw("DrawUserVerts", DrawPrim, _pVerts, _nVerts, _pInd, _nInd);
			DumpGeomOBJ("DrawUserVerts", _pVerts, _nVerts, _pInd, _nInd, _GLPrim);

			DisableVertexAttribPointers();
			glBindVertexArray(0);
		}

		// Fetch component _c (0..3) of vertex _i from a vertex register
		// of format _Fmt, following the range semantics documented at
		// the CRC_VREGFMT_* enum (MRender_Classes.h): F32/I16/U16 raw,
		// NSx normalized signed -1..1, NUx normalized unsigned 0..1.
		// Returns false for unsupported formats.
		static bool VRegFetch(const void* _p, int _Fmt, int _i, int _c, float& _Out)
		{
			switch (_Fmt)
			{
			case CRC_VREGFMT_V1_F32: case CRC_VREGFMT_V2_F32:
			case CRC_VREGFMT_V3_F32: case CRC_VREGFMT_V4_F32:
			{
				const int n = _Fmt - CRC_VREGFMT_V1_F32 + 1;
				if (_c >= n) { _Out = (_c == 3) ? 1.0f : 0.0f; return true; }
				_Out = ((const fp32*)_p)[_i * n + _c];
				return true;
			}
			case CRC_VREGFMT_V1_I16: case CRC_VREGFMT_V2_I16:
			case CRC_VREGFMT_V3_I16: case CRC_VREGFMT_V4_I16:
			{
				const int n = _Fmt - CRC_VREGFMT_V1_I16 + 1;
				if (_c >= n) { _Out = (_c == 3) ? 1.0f : 0.0f; return true; }
				_Out = (float)((const int16*)_p)[_i * n + _c];
				return true;
			}
			case CRC_VREGFMT_V1_U16: case CRC_VREGFMT_V2_U16:
			case CRC_VREGFMT_V3_U16: case CRC_VREGFMT_V4_U16:
			{
				const int n = _Fmt - CRC_VREGFMT_V1_U16 + 1;
				if (_c >= n) { _Out = (_c == 3) ? 1.0f : 0.0f; return true; }
				_Out = (float)((const uint16*)_p)[_i * n + _c];
				return true;
			}
			case CRC_VREGFMT_NS1_I16: case CRC_VREGFMT_NS2_I16:
			case CRC_VREGFMT_NS3_I16: case CRC_VREGFMT_NS4_I16:
			{
				int n;
				if      (_Fmt == CRC_VREGFMT_NS1_I16) n = 1;
				else if (_Fmt == CRC_VREGFMT_NS2_I16) n = 2;
				else if (_Fmt == CRC_VREGFMT_NS3_I16) n = 3;
				else                                  n = 4;
				if (_c >= n) { _Out = (_c == 3) ? 1.0f : 0.0f; return true; }
				_Out = (float)((const int16*)_p)[_i * n + _c] * (1.0f / 32767.0f);
				return true;
			}
			case CRC_VREGFMT_NU1_I16: case CRC_VREGFMT_NU2_I16:
			{
				const int n = (_Fmt == CRC_VREGFMT_NU1_I16) ? 1 : 2;
				if (_c >= n) { _Out = (_c == 3) ? 1.0f : 0.0f; return true; }
				_Out = (float)((const uint16*)_p)[_i * n + _c] * (1.0f / 65535.0f);
				return true;
			}
			default:
				return false;
			}
		}

		// VBID path (precached geometry: frontend cube, models, BSP).
		// No GPU-side cache yet: fetch the CPU data from the engine's
		// VB context each draw, convert to SUIVert and stream it.
		// Positions/UVs accept the F32 and packed I16/U16/NSx/NUx
		// formats; anything else bumps m_DbgVBIDSkipFmt.
		// Convert VBID vertex-register data into our interleaved SUIVert
		// buffer. Returns malloc'd buffer (caller frees) or NULL on failure.
		// Extracted so both Render_VertexBuffer and
		// Render_VertexBuffer_IndexBufferTriangles share it.
		SUIVert* BuildVertsFromVBB(CRC_BuildVertexBuffer& VBB, int& _nV_out)
		{
			_nV_out = 0;
			const int nV = VBB.m_nV;
			if (nV <= 0) return NULL;

			// RIDDICK_SKIP_SKINNED=1: skip meshes with matrix-palette
			// skinning attribs. Our BuildInterleavedVerts feeds raw
			// bone-space positions to GL without applying palette, so
			// character meshes come out as scattered spikes on-screen.
			// Hiding them lets the world (BSP2, static props) show up
			// cleanly for verification. Skinning support = separate task.
			static int sSkipSkinned = -1;
			if (sSkipSkinned < 0)
			{
				const char* e = getenv("RIDDICK_SKIP_SKINNED");
				sSkipSkinned = (e && *e && *e != '0') ? 1 : 0;
			}
			if (sSkipSkinned)
			{
				if (VBB.m_lpVReg[CRC_VREG_MI0] || VBB.m_lpVReg[CRC_VREG_MW0] ||
				    VBB.m_lpVReg[CRC_VREG_MI1] || VBB.m_lpVReg[CRC_VREG_MW1])
				{
					return NULL;
				}
			}

			const void* pPos = VBB.m_lpVReg[CRC_VREG_POS];
			const int PosFmt = VBB.m_Format.GetFormat(CRC_VREG_POS);
			{
				float Dummy;
				if (!pPos || !VRegFetch(pPos, PosFmt, 0, 0, Dummy))
				{
					++m_DbgVBIDSkipFmt;
					m_DbgVBIDLastSkip = PosFmt;
					return NULL;
				}
			}

			// UV-set selection honors the engine's Attrib_TexCoordSet
			// (m_iTexCoordSet[channel] = which VB texcoord array feeds
			// texture channel). BSP2 puts diffuse UV in TEXCOORD0 and
			// LIGHTMAP UV in TEXCOORD3 (TEXCOORD1/2 are tangents!), so
			// hardcoding channel k -> TEXCOORDk feeds garbage to the
			// second sampler. Default (attrib cleared) is identity: i.
			int UVSet0 = 0, UVSet1 = 1;
			if (m_pCurAttrib)
			{
				UVSet0 = m_pCurAttrib->m_iTexCoordSet[0];
				UVSet1 = m_pCurAttrib->m_iTexCoordSet[1];
				if (UVSet0 >= CRC_MAXTEXCOORDS) UVSet0 = 0;
				if (UVSet1 >= CRC_MAXTEXCOORDS) UVSet1 = 1;
			}
			const int iUVReg0 = CRC_VREG_TEXCOORD0 + UVSet0;
			const int iUVReg1 = CRC_VREG_TEXCOORD0 + UVSet1;
			const void* pUV  = VBB.m_lpVReg[iUVReg0];
			const int   UVFmt = VBB.m_Format.GetFormat(iUVReg0);
			const void* pUV1 = VBB.m_lpVReg[iUVReg1];
			const int   UV1Fmt = VBB.m_Format.GetFormat(iUVReg1);
			const uint32_t* pCol = 0;
			if (VBB.m_Format.GetFormat(CRC_VREG_COLOR) == CRC_VREGFMT_N4_COL)
				pCol = (const uint32_t*)VBB.m_lpVReg[CRC_VREG_COLOR];
			const void* pNrm  = VBB.m_lpVReg[CRC_VREG_NORMAL];
			const int   NrmFmt = VBB.m_Format.GetFormat(CRC_VREG_NORMAL);

			// F10-armed per-shape log of what registers were wired for
			// each VBB draw (m_DbgVBBLogArm set to 32 by DbgDumpTick on
			// F10 edge). Used to check that BSP2 sends diffuse UV
			// (F32/V2) in TEXCOORD0 as expected, not NULL or wrong slot.
			const bool bLogVBB = (m_DbgVBBLogArm > 0);
			if (bLogVBB)
			{
				float n0x=0, n0y=0, n0z=0; bool nOK = false;
				if (pNrm) nOK = VRegFetch(pNrm, NrmFmt, 0, 0, n0x) &&
				                VRegFetch(pNrm, NrmFmt, 0, 1, n0y) &&
				                VRegFetch(pNrm, NrmFmt, 0, 2, n0z);
				// Sample UV of first 3 vertices to see if they vary or are
				// constant (broken loader / broken UV register on some maps).
				float u0=0,v0=0, u1=0,v1=0, u2=0,v2=0;
				if (pUV)
				{
					VRegFetch(pUV, UVFmt, 0, 0, u0); VRegFetch(pUV, UVFmt, 0, 1, v0);
					if (nV >= 2) { VRegFetch(pUV, UVFmt, 1, 0, u1); VRegFetch(pUV, UVFmt, 1, 1, v1); }
					if (nV >= 3) { VRegFetch(pUV, UVFmt, 2, 0, u2); VRegFetch(pUV, UVFmt, 2, 1, v2); }
				}
				fprintf(stderr,
					"[VBB] nV=%d PosFmt=%d NrmFmt=%d nrmPtr=%s v0N=(%.3f,%.3f,%.3f)%s"
					" UVSet0=%d/reg%d fmt=%d ptr=%s v0UV=(%.3f,%.3f) v1UV=(%.3f,%.3f) v2UV=(%.3f,%.3f)"
					" UVSet1=%d/reg%d fmt=%d ptr=%s"
					" TxEn=0x%08x col=%s\n",
					nV, PosFmt, NrmFmt, pNrm?"y":"n", n0x,n0y,n0z, nOK?"":"[fetchFAIL]",
					UVSet0, iUVReg0, UVFmt, pUV?"y":"n", u0,v0, u1,v1, u2,v2,
					UVSet1, iUVReg1, UV1Fmt, pUV1?"y":"n",
					(unsigned)VBB.m_TransformEnable, pCol?"y":"n");
				fflush(stderr);
				--m_DbgVBBLogArm;
			}

			// Per-register scale+offset (packed formats hold values as
			// raw*Scale+Offset; without applying we get "spikes" as the
			// user observed with the OBJ dumper).
			const bool bPosTx = (VBB.m_TransformEnable & (1u << CRC_VREG_POS)) != 0;
			const CRC_VRegTransform& PosTx = VBB.m_lTransform[CRC_VREG_POS];
			const bool bUVTx  = pUV && (VBB.m_TransformEnable & (1u << iUVReg0)) != 0;
			const CRC_VRegTransform& UVTx  = VBB.m_lTransform[iUVReg0];
			const bool bUV1Tx = pUV1 && (VBB.m_TransformEnable & (1u << iUVReg1)) != 0;
			const CRC_VRegTransform& UV1Tx = VBB.m_lTransform[iUVReg1];

			SUIVert* pVerts = (SUIVert*)malloc(sizeof(SUIVert) * nV);
			if (!pVerts) return NULL;
			// NO_LIGHT fullbright: force white vCol so shader collapses
			// to `c = texture(...)`. But ONLY for 3D world draws -- UI
			// text/HUD/loading-bar carry authored per-vertex colors
			// (yellow captions, red bars) that we must not overwrite.
			// 2D discriminator: engine UI-pass hint + projection/model
			// heuristics (ClassifyUI) -- identity-model world BSP stays 3D.
			const bool bForceWhiteCol = GLES3_NoLight() && !ClassifyUI();
			for (int i = 0; i < nV; ++i)
			{
				VRegFetch(pPos, PosFmt, i, 0, pVerts[i].x);
				VRegFetch(pPos, PosFmt, i, 1, pVerts[i].y);
				VRegFetch(pPos, PosFmt, i, 2, pVerts[i].z);
				if (bPosTx)
				{
					pVerts[i].x = pVerts[i].x * PosTx.m_Scale.k[0] + PosTx.m_Offset.k[0];
					pVerts[i].y = pVerts[i].y * PosTx.m_Scale.k[1] + PosTx.m_Offset.k[1];
					pVerts[i].z = pVerts[i].z * PosTx.m_Scale.k[2] + PosTx.m_Offset.k[2];
				}
				pVerts[i].u = pVerts[i].v = 0.0f;
				if (pUV)
				{
					if (!VRegFetch(pUV, UVFmt, i, 0, pVerts[i].u)) pVerts[i].u = 0.0f;
					if (!VRegFetch(pUV, UVFmt, i, 1, pVerts[i].v)) pVerts[i].v = 0.0f;
					if (bUVTx)
					{
						pVerts[i].u = pVerts[i].u * UVTx.m_Scale.k[0] + UVTx.m_Offset.k[0];
						pVerts[i].v = pVerts[i].v * UVTx.m_Scale.k[1] + UVTx.m_Offset.k[1];
					}
				}
				pVerts[i].u1 = pVerts[i].v1 = 0.0f;
				if (pUV1)
				{
					if (!VRegFetch(pUV1, UV1Fmt, i, 0, pVerts[i].u1)) pVerts[i].u1 = 0.0f;
					if (!VRegFetch(pUV1, UV1Fmt, i, 1, pVerts[i].v1)) pVerts[i].v1 = 0.0f;
					if (bUV1Tx)
					{
						pVerts[i].u1 = pVerts[i].u1 * UV1Tx.m_Scale.k[0] + UV1Tx.m_Offset.k[0];
						pVerts[i].v1 = pVerts[i].v1 * UV1Tx.m_Scale.k[1] + UV1Tx.m_Offset.k[1];
					}
				}
				pVerts[i].col = bForceWhiteCol ? 0xffffffffu
					: (pCol ? PackColorBGRA_to_RGBA(pCol[i]) : 0xffffffffu);
				pVerts[i].nx = 0; pVerts[i].ny = 0; pVerts[i].nz = 1;
				if (pNrm)
				{
					float nx=0, ny=0, nz=1;
					if (VRegFetch(pNrm, NrmFmt, i, 0, nx) &&
					    VRegFetch(pNrm, NrmFmt, i, 1, ny) &&
					    VRegFetch(pNrm, NrmFmt, i, 2, nz))
					{
						pVerts[i].nx = nx; pVerts[i].ny = ny; pVerts[i].nz = nz;
					}
				}
			}
			_nV_out = nV;
			return pVerts;
		}

		void Render_VertexBuffer(int _VBID)
		{
			++m_DbgDrawVBID;
			if (!m_pVBCtx) return;

			// Phase 4 M6: try the GPU-resident cache first -- a hit means
			// no per-frame malloc, no scalar VRegFetch conversion, no
			// re-upload (see GLES3_Geometry.h). Falls through to the old
			// streaming path below for anything the cache can't handle
			// yet (skinned meshes, exotic primitive types) or when
			// RIDDICK_NO_VBCACHE=1 forces the old path.
			if (!GLES3_NoVBCache())
			{
				if (const SGLES3GeomEntry* pE = m_GeomCache.Ensure(_VBID))
				{
					if (pE->m_nIdx > 0 && DrawCachedVB(*pE, *pE, pE->m_nIdx, 0))
						return;
				}
			}

			CRC_BuildVertexBuffer VBB;
			VBB.Clear();
			m_pVBCtx->VB_Get(_VBID, VBB, VB_GETFLAGS_BUILD);
			if (VBB.m_nV <= 0 || !VBB.m_piPrim || !VBB.m_nPrim) return;

			int nV = 0;
			SUIVert* pVerts = BuildVertsFromVBB(VBB, nV);
			if (!pVerts) return;
			++m_DbgDrawStreamed;

			// Walk the primitive stream: header word = index count,
			// then indices; type from the stream iterator.
			CRCPrimStreamIterator It(VBB.m_piPrim, VBB.m_nPrim);
			if (It.IsValid())
			{
				do
				{
					const uint16* pPrim = It.GetCurrentPointer();
					int nInd = *pPrim;
					GLenum Prim;
					switch (It.GetCurrentType())
					{
					case CRC_RIP_TRIANGLES: Prim = GL_TRIANGLES;      nInd *= 3; break;
					case CRC_RIP_TRISTRIP:  Prim = GL_TRIANGLE_STRIP;            break;
					case CRC_RIP_TRIFAN:    Prim = GL_TRIANGLE_FAN;              break;
					default:                Prim = 0;                            break;
					}
					if (Prim && nInd > 0)
						DrawUserVerts(Prim, pVerts, nV, pPrim + 1, nInd);
				}
				while (It.Next());
			}
			free(pVerts);
		}

		// BSP2 solid-world path. VBID delivers positions/UV/UV1/color;
		// IBID delivers the shared index pool (its m_piPrim). _PrimOffset
		// is a 16-bit index count into that pool, _nTriangles*3 indices
		// starting there form the triangle list. Mirrors PS3
		// CRCPS3GCM::Render_VertexBuffer_IndexBufferTriangles
		// (MRenderPS3_Render.cpp:245-267). Without this override, the
		// base CRC_Core stub (MRender.cpp:6031) silently drops every
		// SLC-cluster mesh submitted from CXR_Model_BSP2, so the entire
		// world stays invisible while shadow-volumes (which use the
		// separate Render_VertexBuffer path with almost-black colour)
		// come through faintly — matched Pa1_TheDream symptom exactly.
		void Render_VertexBuffer_IndexBufferTriangles(uint _VBID, uint _IBID,
		                                              uint _nTriangles, uint _PrimOffset)
		{
			++m_DbgDrawVBID;
			if (!m_pVBCtx || _nTriangles == 0) return;

			// Phase 4 M6: cached path. _VBID supplies vertices, _IBID
			// supplies the shared index pool (see GLES3_Geometry.h --
			// this is exactly the BSP2 world-cluster case the cache was
			// built for: CBSP2_SLCIBContainer hands out an index-only
			// VBID for _IBID). Same entry for both when they coincide.
			if (!GLES3_NoVBCache())
			{
				const SGLES3GeomEntry* pVB = m_GeomCache.Ensure((int)_VBID);
				const SGLES3GeomEntry* pIB = (_IBID == _VBID) ? pVB : m_GeomCache.Ensure((int)_IBID);
				if (pVB && pIB)
				{
					const intptr_t ByteOffset = (intptr_t)_PrimOffset * 2; // 2 bytes/uint16 index
					if (DrawCachedVB(*pVB, *pIB, (int)(_nTriangles * 3), ByteOffset))
						return;
				}
			}

			CRC_BuildVertexBuffer VBB;
			VBB.Clear();
			m_pVBCtx->VB_Get(_VBID, VBB, VB_GETFLAGS_BUILD);
			int nV = 0;
			SUIVert* pVerts = BuildVertsFromVBB(VBB, nV);
			if (!pVerts) return;
			++m_DbgDrawStreamed;

			// Fetch the IB (usually a separate VB whose m_piPrim is the
			// shared index pool). May be the same as _VBID.
			const uint16* pIdx = VBB.m_piPrim;
			int nPrimIB = VBB.m_nPrim;
			CRC_BuildVertexBuffer IBB;
			if (_IBID != _VBID)
			{
				IBB.Clear();
				m_pVBCtx->VB_Get(_IBID, IBB, VB_GETFLAGS_BUILD);
				pIdx = IBB.m_piPrim;
				nPrimIB = IBB.m_nPrim;
			}

			// Diagnostic (RIDDICK_DBG_GL=1, first ~20 calls): print resolved
			// VB/IB details and a sample of indices so we can see whether the
			// shared index pool addresses vertices beyond nV (would cause
			// stretched garbage triangles).
			static int s_nDbg = 0;
			if (m_DbgEnabled && s_nDbg < 20 && pIdx)
			{
				const uint16* pFirst = pIdx + _PrimOffset;
				const int nIdxDraw = (int)(_nTriangles * 3);
				uint16 imin = 0xffff, imax = 0;
				const int nSample = Min(nIdxDraw, 12);
				for (int k = 0; k < nSample; ++k)
				{
					uint16 v = pFirst[k];
					if (v < imin) imin = v;
					if (v > imax) imax = v;
				}
				fprintf(stderr, "[GLES3-VBIT] VBID=%u IBID=%u nV=%d nPrimIB=%d off=%u nTri=%u  first idx=[%u,%u,%u,%u,%u,%u]  range=%u..%u\n",
					_VBID, _IBID, nV, nPrimIB, _PrimOffset, _nTriangles,
					(unsigned)pFirst[0], (unsigned)pFirst[1], (unsigned)pFirst[2],
					(unsigned)pFirst[3], (unsigned)pFirst[4], (unsigned)pFirst[5],
					(unsigned)imin, (unsigned)imax);
				++s_nDbg;
			}

			if (pIdx)
				DrawUserVerts(GL_TRIANGLES, pVerts, nV, pIdx + _PrimOffset, _nTriangles * 3);
			free(pVerts);
		}
		void Render_Wire(const CVec3Dfp32& _v0, const CVec3Dfp32& _v1, CPixel32 _Color){}
		void Render_WireStrip(const CVec3Dfp32* _pV, const uint16* _piV, int _nVertices, CPixel32 _Color){}
		void Render_WireLoop(const CVec3Dfp32* _pV, const uint16* _piV, int _nVertices, CPixel32 _Color){}

		// Map change / precache-flush point: drop cached GPU geometry for
		// any VBID the engine no longer flags PRECACHE|ALLOCATED (mirrors
		// PS3's CContext_Geometry flush -- see GLES3_Geometry.cpp).
		virtual void Geometry_PrecacheFlush(){ m_GeomCache.FlushUnused(); }
		virtual void Geometry_PrecacheBegin( int _Count ){}
		virtual void Geometry_PrecacheEnd(){}
		// Loading-screen precache tick (one VBID per call, see
		// WClient_Precache.cpp): eagerly build+upload the GPU entry now
		// so the first real draw of this VBID doesn't stall converting/
		// uploading it. RIDDICK_NO_VBCACHE=1 disables the cache
		// entirely, so skip the eager build in that case too.
		virtual void Geometry_Precache(int _VBID)
		{
			if (!GLES3_NoVBCache())
				m_GeomCache.Ensure(_VBID);
		}
		virtual CDisplayContext* GetDC(){ return m_pDisplayContext;}

		void Register(CScriptRegisterContext & _RegContext){}

	};

	TPtr<CRC_GLES3> m_spRenderContext;


	virtual CRenderContext* GetRenderContext(class CRCLock* _pLock)
	{
		if (!m_spRenderContext)
		{
			m_spRenderContext = MNew(CRC_GLES3);
			m_spRenderContext->m_pDisplayContext = this;
			m_spRenderContext->Create(this, "");

		}

		return m_spRenderContext;
	}

	virtual int Win32_CreateFromWindow(void* _hWnd, int _Flags = 0)
	{
		return 0;
	}

	virtual int Win32_CreateWindow(int _WS, void* _pWndParent, int _Flags = 0)
	{
		return 0;
	}

	virtual void Win32_ProcessMessages()
	{
	}

	virtual void* Win32_GethWnd(int _iWnd = 0)
	{
		return 0;
	}

	virtual void Parser_Modes()
	{
	}

	virtual void Register(CScriptRegisterContext & _RegContext)
	{
		CDisplayContext::Register(_RegContext);
	}

};

MRTC_IMPLEMENT_DYNAMIC_NO_IGNORE(CDisplayContextSDL2, CDisplayContext);

#endif // PLATFORM_LINUX
