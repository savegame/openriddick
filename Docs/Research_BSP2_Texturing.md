# Research: текстурирование BSP2-уровня (GLES3-порт)

Дата: 2026-07-22. Два независимых explore-агента (движковый путь + бэкенд/эталон RndrGL),
сводка модератора. Связанные доки: `Research_LightPass_Report.md` (почему динсвет/лайтмап-проход
сейчас не исполняется), `Research_GeometryArtifacts_Report.md`.

## 1. Поток рендера BSP2 за кадр

`WBSP2Model.cpp`: `OnRender` → `OnRender2` (:4918/:4926) → `RenderLeafList` (:5335) →
`RenderPortalLeaf` (:4117) — раскладка примитивов лиц по очередям (`VB_AddPrimitives_LocalVBM`, :4217).
Далее: `RenderShaderQueue` (:5494) → `VB_RenderFaceQueues` (:5495, тесселяция) →
`VB_Flush` (:5497) → `VB_RenderQueues` (:5498, **базовый проход** с поверхностями).

VB кластеров **статические** (VBID на весь уровень, `VB_AllocVBID` :2669 из лоадера
`WBSP2Loader.cpp:1606`). CPU-данные — `CBSP2_VertexBuffer` (`WBSP2Model.h:692-725`):
`m_lV, m_lN, m_lTangU, m_lTangV, m_lTV1, m_lTV2, m_lCol, m_lLMScale`. Создаются лениво
через `Tesselate` (`WBSP2Model.cpp:808`): diffuse-UV (TV1) и лайтмап-UV (TV2) + LMScale
(:928-952), тангенты (:1036-1070). В очередь кладётся только `CXR_VBIDChain`; GPU-данные
поднимаются через `CXR_VBContext::VB_Get` → колбэк `CXR_Model_BSP2::Get` (:3047).

## 2. Формат вершин BSP2 — КЛЮЧЕВАЯ НАХОДКА

Маппинг каналов в `CXR_Model_BSP2::Get` (`WBSP2Model.cpp:3060-3097`):

| VREG-канал | Источник | Содержимое |
|---|---|---|
| POS | `m_lV` | позиция |
| NORMAL | `m_lN` | нормаль |
| COLOR | `m_lCol` | цвет (0xFF) |
| **TEXCOORD0** | `m_lTV1` | **diffuse UV** |
| **TEXCOORD1** | `m_lTangU` | **тангент U (3 комп.) — НЕ лайтмапа!** |
| TEXCOORD2 | `m_lTangV` | тангент V (3 комп.) |
| **TEXCOORD3** | `m_lTV2` | **лайтмап-UV** |
| TEXCOORD4 | `m_lLMScale` | интенсивность лайтмапы (1 комп.) |

Форматы F32 (`SetWantTransform<e_Platform_Default>` пуст, :2732).

**Следствие для порта:** наш шейдер «tex0 × tex1(лайтмапа)» читает TEXCOORD1 = тангент —
мусор вместо лайтмап-UV. Настоящий лайтмап-UV — TEXCOORD3. Бэкенд обязан выбирать UV-сет
по `Attrib_TexCoordSet(channel, uvSet)` (`m_iTexCoordSet`), а не хардкодом «канал k = TEXCOORDk».

## 3. Поверхности и TextureID

- Чанк `SURFACES` из .xw → `CXW_Surface::Read` (`WBSP2Loader.cpp:2410`); `InitTextures(false)`
  (:3017) резолвит имена в ID (`XRSurf.cpp:924-990`, промах → SPECIAL_DEFAULT).
- Слои: `CXW_SurfaceKeyFrame::m_lTextures` = `CXW_SurfaceLayer[]` (`XRSurf.h:288-333`):
  `m_Type` (XW_LAYERTYPE_*), `m_TexChannel`, `m_TexCoordNr`, `m_TexEnvMode`, `m_RasterMode`,
  `m_TextureID`, `m_lOper`. Спец-ID ≥ 0xf000 (LIGHTMAP=0xf002 и пр., :115-119) подставляются
  в рантайме.
- Попадание в атрибуты:
  - **Fast path** (`InitFastPath`, `XRSurf.cpp:2468-2514`): однослойные поверхности →
    `CXR_VirtualAttributes_Surface01`, `m_TextureID[0]`; батчится в `Render_SurfaceArray`
    (`XRUtilRS.cpp:1979-2059`).
  - **Общий путь**: `CXR_Util::Render_Surface` (`PS2XRUtilRS.cpp:437`, там лог `[SURF]`
    :449-476): на слой `Attrib_TextureID(iChannel, ID)` (:726) и
    `Attrib_TexCoordSet(iChannel, pLayer->m_TexCoordNr)` (:621).
- Группировка слоёв: `XW_LAYERFLAGS_GROUP` → один draw, несколько `m_TextureID[0..N]`,
  комбинирование по `m_TexEnvMode` слоя (:577-810, :911). Негруппированные — мультипроходно,
  блендинг `m_RasterMode`, альфа `XW_LAYERFLAGS_ALPHACOMPARE`.
- Z-препасс: вся непрозрачная геометрия сначала Z-only (`CXR_VBPRIORITY_UNIFIED_ZBUFFER`,
  :2411-2426), цветные проходы с `ZCompare EQUAL/LESSEQUAL`.

## 4. Лайтмапы (почему их сейчас нет на экране)

- Данные: `LIGHTMAPINFO/2` (:2688-2743), `LIGHTMAPS3` (:2770) → `m_spLMTC` (PC:
  `CTextureContainer_LMC`, :2975-2983); `m_lLMTextureIDs[i]` — ID страниц (:2850-2858).
  Кластер LMC = **4 текстуры** (light-field mapping: 3 цветовые + scale).
- BSP2 применяет лайтмапу **отдельным проходом** (не как `m_TextureIDLightMap` базового
  прохода — это только BSP1 и сплайн-кисти): батч по `m_iLMTexture` →
  `CXR_ShaderParams_LightFieldMapping::CreateLFM(..., &m_lLMTextureIDs[iLM*4], 3, 4)`
  (`WBSP2Model.cpp:2363-2392`) → `CXR_Shader::RenderShading_LightFieldMapping`
  (`XRShader.cpp:1658`) → только FP20-ветки (:1682-1696).
- Выбор шейдер-мода: `CXR_Shader::Create` (`XRShader.cpp:1252-1315`): наши caps
  (без FRAGMENTPROGRAM20, 2 юнита) дают `ModesAvail=0` → `BitScanBwd32(0) = -1`
  (`MMisc.h:397`) → `m_ShaderMode = -1` → **ни RenderShading, ни LFM не исполняются**.
  BSP сейчас = только базовый diffuse-проход.
- Лазейка на будущее: существует `RenderShading_TexEnvCombine` (`XRShader_TexEnvCombine.cpp`) —
  fixed-function путь (2 юнита, без FP20), но case TEXENVCOMBINE в `CXR_Shader::RenderShading`
  отсутствует; кто его вызывает — не отслежено. Если его подключить (caps/env),
  динсвет и, возможно, LFM можно гнать через texenv-цепочки, реализуемые в нашем шейдере.

## 5. Текстурные матрицы / юниты

- `Attrib_TexCoordSet(channel, uvSet)` — единственный механизм назначения UV→канал
  (`PS2XRUtilRS.cpp:621,:939,:987`). У нас — хардкод.
- `CRC_MATRIX_TEXTURE+n`: BSP2-база не ставит; трансформы — VB-операторы слоёв (`m_lOper`,
  scroll/rotate). Проективные динлайты ставят `CRC_MATRIX_TEXTURE1` (`WBSPLight.cpp:1779,1856`)
  — наш `uTexMat1` это покрывает.
- Юнитов у нас 2 (`CRC_GLOBALVAR_NUMTEXTURES`), что ограничивает «lightmap как +1 канал».

## 6. GLES3-бэкенд: что есть и пробелы

Есть: аплоад DXT1/3/5 + BGRA/BGRX/I8/I8A8 (`GLES3_Texture.cpp`), placeholder magenta при
промахе с ретраем, биндинг каналов 0/1 (`SetupCommonUniforms`, `MDisplaySDL2.cpp:1931-1986`),
`uTexMat/uTexMat1`, alpha-test, туман, modulate tex0×tex1.

Пробелы (блокеры корректного BSP):
1. **UV-хардкод**: канал k = TEXCOORDk; нужен `m_iTexCoordSet` (и для BSP2 — TEXCOORD3 как
   лайтмап-источник, см. §2).
2. **Каналы 2/3 не биндятся** (:1936-1986) — многослойные материалы теряются.
3. **TexEnvMode игнорируется** (`m_TexEnvMode[]`, `MRender_Classes.h:453-478`): нет
   MULTIPLY2 (RGB_SCALE=2 — динлайт-проход в 2 раза темнее), ADD, DOT3, LERP, env-color.
   Второй слой всегда modulate-RGB, его альфа не участвует.
4. **TexGen не реализован** (`m_lTexGenMode`/`m_TexGenComp`) — проективные лайты, env-map.
5. **Wrap-режимы несогласованы**: DXT → GL_REPEAT (`GLES3_Texture.cpp:179`), некомпресс →
   GL_CLAMP_TO_EDGE (:275) → швы на тайлящихся RGBA-текстурах BSP.
6. Мипы захардкожены (`Upload2D(...,true)`), анизотропии нет; текстурные матрицы каналов 2/3
   кэшируются, но не подаются.
7. `Internal_RenderPolygon` — пустой стаб (:426-429); используется ли в BSP2-пути — не проверено.
8. 3DC/BC5 нормал-мапы через engine-Decompress (:187-197) — работоспособность не подтверждена.

## 7. Эталон RndrGL (мультитекстура)

- ARB_multitexture: указатели `glActiveTextureARB` и пр. (`RndrGL_dll_decomp.c:69343-69376`).
- Per-unit texenv: CRC-enum → `GL_MODULATE/GL_BLEND/GL_DECAL/GL_COMBINE` (:77808-77832);
  COMBINE-настройка с `GL_RGB_SCALE=1|2` (MULTIPLY2 → 2, :78791-78899), источники
  `GL_PREVIOUS/GL_TEXTURE/GL_CONSTANT`, env-цвет `glTexEnvfv` (:78764).
- UV юнита 1+: immediate-mode `glMultiTexCoord2fvARB` (:71947-71996).
- Текстурные матрицы per-unit: `Matrix_SetRender` режимы 2..5 → `glMatrixMode(GL_TEXTURE)`
  + `glLoadMatrixf(M × корректирующая @0x3fa0..)` (:76397-76476).

## 8. План наладки текстурирования BSP (по нарастающей)

- **Шаг 1 (база): СДЕЛАН** — в `BuildVertsFromVBB`/`BuildInterleavedVerts` UV-сеты выбираются
  по `m_iTexCoordSet[0..1]` (MDisplaySDL2.cpp).
- **Шаг 2:** wrap-режимы → GL_REPEAT везде для мировых текстур (убрать CLAMP_TO_EDGE
  в некомпресс-пути; для UI оставить clamp по признаку размера/флага или отдельному пути).
- **Шаг 3:** TexEnvMode в шейдере: uniform-режим на канал 1 (MODULATE/MULTIPLY2/ADD/REPLACE)
  — закрывает корректную яркость двухслойных поверхностей.

## 9. Корень «BSP без текстур» (подтверждено дампом кадра 2026-07-22)

Дамп `RIDDICK_DUMP_FRAME` (TheDream): 515 дроев. Кадр BSP2 = Z/ambient-препасс
(233 дроя, все Tex0=0, цвет = `m_UnifiedAmbience`) + depth-fog проход (233 дроя,
Tex0=`Special_DepthFogTable`, texgen не реализован) + ~49 текстурированных дроев
декалей/прочего. Текстур+света нет, потому что **shading-очередь мертва**:

- `CXR_Shader::Create` (XRShader.cpp:1252): ModesAvail собирается только из FP20/FP14
  caps (нужно 8 текстурных юнитов); TEXENVCOMBINE-ветка закомментирована. При наших caps
  ModesAvail=0 → `BitScanBwd32(0)` = -1 → `m_ShaderMode = 0xFFFFFFFF`.
- Все световые проходы только FP20: `CXR_Shader::RenderShading` (XRShader.cpp:1531)
  и `RenderShading_LightFieldMapping` (XRShader.cpp:1658) — switch по m_ShaderMode
  без case TEXENVCOMBINE → no-op. `RenderShading_TexEnvCombine`
  (XRShader_TexEnvCombine.cpp:415) существует, но со старой сигнатурой
  (CXR_VertexBuffer*, не Geometry) и НЕ вызывается нигде — мёртвый код эпохи Xbox/EFBB.
  В этой версии движка (Dark Athena) fixed-pipeline шейдинг вырезан, PC-retail
  требовал SM2.0-карту.
- Итог по дизайну: Z-препасс (ambient, без текстур) → LFM-проход (diffuse × 3 лайтмапы,
  FP20) → динамические point/spot (FP20) → fog. Без FP20 виден только ambient-препасс.

**Принятое решение (fallback, WBSP2Model.cpp):** при `m_ShaderMode == -1` все поверхности
shading-очереди гоняются через textured-ветку Z-препасса (ZAlphaQueue): на канал 0 биндится
`pSSP->m_lTextureIDs[XR_SHADERMAP_DIFFUSE]` + alpha-compare, цвет полный белый (вместо
Ambience). Мир становится текстурированным, но без света (fullbright). Дальше по шагам:

- **Шаг 4a (лайтмапа, следующий):** в том же fallback биндить `m_lLMTextureIDs[iLMTexture*4]`
  на канал 1 с TexCoordSet=3 (BSP2: TEXCOORD3 = лайтмап-UV) — наш шейдер уже делает
  tex0×tex1. Это приближение: настоящий LFM блендит 3 направленные лайтмапы по
  tangent-space осям; начать с [0], оценить визуально.
- **Шаг 4b (далёкий):** портировать `RenderShading_FP20_LFM` на наш GLSL-шейдер
  (per-pixel, 8 текстур, tangent-space) — по сути задача M4 (генератор шейдеров).
