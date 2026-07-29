# Справочник по фрагментным программам (`shaders/` → GLSL ES 3.00)

Этот документ — разбор оригинального фрагментно-шейдерного корпуса движка Starbreeze
P5, извлечённого из установленных PC-версий *Escape from Butcher Bay* (EFBB) и
*Assault on Dark Athena* (DA, файлы без суффикса `_EFBB`). Вершинная часть конвейера
уже разобрана в `Docs/VP_Reference.md` (шаблон `shaders/VP.xrg`, все `CRC_TEXGENMODE_*`,
раскладка констант, вершинное освещение) — здесь она не дублируется, только даются
ссылки на неё в местах пересечения (§3, §5).

Наш бэкенд (`Source/P5/Shared/MOS/RenderContexts/GLES3/`) сейчас имеет только один
GLSL ES 3.00 шейдер `vCol × texture` (см. `CRC_GLES3` в `MDisplaySDL2.cpp`, Фаза 4 M3
в `CLAUDE.md`). Полноценного фрагментного конвейера, генератора шейдеров по
`CRC_Attributes`/texenv-комбинациям и загрузчика `.fp`-файлов нет вообще.

---

## 0. Главный вывод: почему пяти «forward»-имён нет в данных

Движок (`Source/P5/Shared/MOS/XR/XRShader_FP20.cpp:2-23`) ожидает от бэкенда программы
с именами `XRShader_FP20_LFM/_LF/_NDS/_NDSP/_NDSEATP` (forward, non-deferred) и
`XRShader_FP20Def_*` / `XRShader_FP20DefLin_*` (deferred). Ни одного файла с этими
буквальными именами в извлечённом корпусе `shaders/` **нет** — и это ожидаемо, не баг
экстракции:

* PS3-бэкенд грузит fp-файл по буквальному имени программы:
  `Source/P5/Shared/MOS/RenderContexts/PS3GCM/MRenderPS3_FragmentProgram.cpp:554`:
  `CFStrF("System/GL/ARB_Fragment_Program/%s.fp", _pExtAttr->m_pProgramName)` — то есть
  имя из `SetProgram()` буквально равно имени файла. Этот же слой каталогов
  (`System/GL/ARB_Fragment_Program/...`) — это и есть структура, из которой пользователь
  вытащил `shaders/`.
* Выбор `m_ShaderMode` (`XRShader.cpp:1258-1310`): на не-Xenon платформах `AUTO` берёт
  **старший установленный бит** из `ModesAvail`, явно исключая
  `FRAGMENTPROGRAM20SS | FRAGMENTPROGRAM20DEF | FRAGMENTPROGRAM20DEFLIN`
  (`XRShader.cpp:1300`). Значения enum (`XRShader.h:106-121`):
  `FRAGMENTPROGRAM20=5, 20SS=6, 20DEF=7, 20DEFMRT=8, 20DEFMM=9, 20DEFLIN=10`.
  При исключении 6/7/10 старший доступный на реальном железе бит — **9 (`DEFMM`,
  зеркалит `DEF`) или 8 (`DEFMRT`)** — то есть **дефолт всегда деферред-рендер**.
  Чистый `XR_SHADERMODE_FRAGMENTPROGRAM20` (bit 5, единственный режим, который зовёт
  `RenderShading_FP20_NDS/NDSP/NDSEATP/LF/LFM` с буквальными именами из
  `ms_lProgramsShading20[]`) — это **резервный fallback** для GPU без MRT, который в
  ретейл-сборке PC/PS3 практически никогда не выбирается по умолчанию.
* `XR_SHADERMODE_FRAGMENTPROGRAM14` (ATI-регистровые комбайнеры, `RenderShading_FP14`)
  — ещё более старый путь, **явно закомментирован** в диспетчере
  (`XRShader.cpp:1549-1550`, `/* case XR_SHADERMODE_FRAGMENTPROGRAM14 : ... */`).

Итог: `ms_lProgramsShading20[]` — это позднейший (консольный?) рефакторинг таблицы
имён на короткие коды (`LFM/LF/NDS/NDSP/NDSEATP`), но реальный PC-корпус `.fp`,
который эти функции изначально грузили, называется описательно
(`XRShader_SinglePass_Dst2_SpecNormal.fp` и т.д. — заголовки этих файлов буквально
говорят `File: Program for CXR_Shader::RenderShading_FP20`, см. §2/§4) и никогда не
переименовывался под новую таблицу, потому что путь давно стал second-class fallback.
Deferred-семейство (`FP20Def_*`) содержательно соответствует комбинациям
`*generate` в `XRShader_BRDF3.fp` (см. §2, §3) — реальный шипованный рендер молний
не через `FP20Def_NDSEAP.fp`, а напрямую через `XRShader_BRDF3.fp` + G-buffer
(`XRShader_DeferredMRT.fp`, `XRShader_DeferredNormal.fp`).

---

## 1. Каталог корпуса

Диалекты: **ARB** = `ARB_Fragment_Program/` (ARB-ассемблер, некоторые файлы —
HLS-исходники `*type hls`, компилируемые в ARB), **NV** = `NV_Fragment_Program/`
(`OPTION NV_fragment_program` — расширенный ARB, больше регистров), **NV2** =
`NV_Fragment_Program2/` (`OPTION NV_fragment_program2` — циклы/ветвления, GeForce FX),
**ATI** = `ATI_Fragment_Shader/` (`sbzfp.1.4` — Starbreeze-ассемблер поверх
`GL_ATI_fragment_shader`, Radeon 8500/9000, 2-фазный регистровый комбайнер),
**HL** = `HL_Shading/` (высокоуровневый Starbreeze-язык `*_head_{*type hls}` —
секционный DSL, компилируется в HLSL/Cg/GLSL в зависимости от таргета; см. форматное
описание в §3.1), **GLSL100** = `EXT_Shading_Language_100/` (чистый GLSL 1.00,
fixed-function texenv эмуляция).

### 1.1 Per-light shading (forward, по одному источнику света за проход)

| База имени | Диалекты | Назначение |
|---|---|---|
| `XRShader_SinglePass_Dst2_SpecNormal` (+`_Proj_`) | ARB, NV | `RenderShading_FP20`: диффуз+спекуляр, спек-маска в альфе normal-map |
| `XRShader_SinglePass_Dst2_SpecDiffuse` (+`_Proj_`) | ARB, NV | то же, спек-маска в альфе diffuse-map |
| `XRShader_SinglePass_Dst2_SpecNormal`/`SpecDiffuse` (+`_Proj_`) | NV2 | то же, компактнее (циклы NV2), без soft-shadow |
| `XRShader_FP20SS_SpecNormal`/`SpecDiffuse` (+`Proj`) | NV2 | то же + 16-tap dithered PCF soft-shadow (depth-aware, `SMOfs0..15`) поверх результата |
| `XRShader_SpecNormal`/`SpecDiffuse` (+`Proj_`) | ATI | `RenderShading_FP14`: тот же алгоритм на 2-фазном ATI-комбайнере (устаревший fallback, код диспетчера закомментирован) |

Все шесть комбинаций (ARB/NV/NV2 × Normal/Diffuse, ×Proj) — переводы **одной и той
же** математики под разное железо; различия разобраны в §4.

### 1.2 Deferred / BRDF (G-buffer, основной путь Dark Athena)

| Файл | Диалект | Назначение |
|---|---|---|
| `XRShader_BRDF3.fp` | HL | «One shader to rule them all» — уберистая деферред BRDF-модель, лайтинг-резолв 1-3 источников + env-map + лайтфилд(-маппинг); разбор — §3 |
| `XRShader_Deferred.fp` | HL | комбинатор материала в G-buffer перед основным проходом (запись albedo/нормали + alpha-blend режим декалей) |
| `XRShader_DeferredMRT.fp` | ARB | запись G-buffer в 3 MRT-таргета разом (normal/diffuse/specular) |
| `XRShader_DeferredNormal.fp` | ARB | запись только нормали в G-buffer (одна цель, без MRT) |
| `XRShader_MotionMap.fp` | ARB, HL | доп. деферред-выход motion-vectors + DoF-маска (`RenderMotionVectorsAndDOF`) |
| `XRShader_DecalNormalTransform.fp`, `_TM.fp` | ARB | декали, пишущие только нормаль в деферред G-buffer (трансформация в базис поверхности, TM = с проекцией по плоскости/затуханием по расстоянию) |

### 1.3 Декали (не-деферред, forward)

| Файл | Диалект | Назначение |
|---|---|---|
| `XRShader_FP20_Decal.fp` | ARB | 680-строчный forward-декаль-шейдер (Normal/Diffuse/Specular карты) |
| `XRShader_DecalTM.fp` | ARB | простая декаль-текстура с альфой по расстоянию до плоскости проекции |
| `XRShader_DecalTMProj.fp` | ARB | декаль с плоской/сферической проекцией на поверхность (`tmCenter/tmNormal/tmTanU/tmTanV`), `KIL` за пределами прямоугольника |

### 1.4 Пост-процесс: цвет/тон/блюр/DoF/glow/motion-blur/SSAO/histogram

Назначение большинства файлов очевидно из имени; сгруппировано без подробностей
(см. §6, где детально нужен только конечный композит):

| Группа | Файлы |
|---|---|
| Финальный композит/цветокоррекция | `XREngine_Final5.fp` (HL, мотоблюр+DoF+экспозиция+ЦК+grain+виньетка), `XREngine_CCFuser.fp` (ARB, слияние LUT цветокоррекции), `XREngine_LinearLerp.fp` (HL, dual-texture lerp gamma↔linear) |
| Blur / Gauss | `XREngine_Blur0/1.fp`, `XREngine_BlurClamped.fp`, `XREngine_GaussClamped(Hurt).fp` (ARB); `XREngine_Blur0/1.fp` (ATI) |
| Radial/Motion blur | `XREngine_RadialBlur(Hurt/Invert).fp`, `XREngine_MotionBlur.fp`, `XRUtil_RestoreMotionMap.fp` (ARB) |
| Glow / DoF | `XREngine_Glow4.fp`, `XREngine_GlowPrep.fp`, `XREngine_GrowDoF.fp` (HL) |
| SSAO | `XREngine_SSAO.fp`, `XREngine_SSAO2.fp`, `XREngine_SSAOFilter.fp`, `XREngine_SSAOGauss.fp`, `XREngine_SSAOPoisson.fp` (HL) |
| Histogram / экспозиция | `XREngine_Histogram.fp` (ARB, HL) |
| Тени (мягкие, экранные) | `XREngine_ShadowProj.fp` (HL, проекция теневой карты по буферу глубины), `XREngine_SoftStencil.fp` (HL) |
| Depth utils | `XREngine_DepthFog.fp` (ARB, мультипасс-туман-обёртка), `XREngine_RenderDepth.fp`, `XREngine_DepthReload.fp`, `XREngine_DepthToAlpha.fp` (HL), `XR_FPDepth.fph` (HL, инклюд — `ConvertDepth`/`ConvertDepthUVToPos`) |
| Copy/mip/misc | `Rndr_CopyToTexture.fp`, `XREngine_CreateMip.fp`, `XREngine_BtoA.fp`, `XREngine_Clear.fp` (HL); `XREngine_MulFilter.fp`, `XREngine_TransparentMesh.fp` (ARB) |
| Debug-визуализация | `XREngine_VBEShowAlpha(Cube)/ShowStencil.fp` (ARB), `XREngine_VBEShowVolume.fp` (HL), `XRShader_ShowViewSpace.fp`, `XRShader_ReverseProjTest.fp` (ARB) |
| Fog include | `Include_XREngine_Fog.fp/.fph` (ARB), `Include_XREngine_Fog_HL.fph` (HL) — общая функция `DoFog()` |

### 1.5 Небо / океан

| Файл | Диалект | Назначение |
|---|---|---|
| `XR_Sky_Atmosphere.fp`, `XR_Sky_DepthAtmosphere.fp` | HL | рассеяние света в небе (атмосферное свечение), depth-вариант — с учётом буфера глубины |
| `XR_Ocean_Surface.fp` | HL | освещение поверхности/толщи океана |
| `XR_Ocean_WaveFunction1.fp`, `XR_Ocean_WaveNormals.fp` | ARB | генерация волн (высота) и их нормалей по вейв-функции (Герстнер/сумма синусоид) |
| `VBOp_FP20_Water.fp`/`Water2.fp`, `VBOp_FP20_CubeWater.fp`/`CubeWater2.fp` | ARB | шейдеры воды для VB-операторов (плоская/кубическая, с fog-вариантами) |
| `VBOp_FP20_Fog.fp` | ARB | экранный туман-оператор |

### 1.6 VB-операторы (`CXR_VBOperator_*`: fresnel/genenv/rimlight/эффекты)

| Файл | Диалект | Назначение |
|---|---|---|
| `VBOp_Fresnel.fp` | ARB | Френель-затемнение по углу обзора |
| `VBOp_Genenv.fp`, `_Genenv2.fp`, `_Genenv_EFBB.fp` | ARB | бамп-environment-mapping (генерация env-координат по нормали) |
| `VBOp_GenEnv.fp` | ATI | тот же на ATI-комбайнере |
| `VBOp_FresnelGenEnv2.fp` | HL | объединение Fresnel+GenEnv в одном проходе (экономия pass'а из-за dest-alpha) |
| `VBOp_NMRimLight.fp` | ARB, HL | rim-light по normal map |
| `VBOP_FP20_Distort.fp` | ARB | экранное искажение по мешу (screen-space distortion) |
| `VBOP_GhostDrone.fp`, `VBOp_MechHood.fp` | ARB | композиция слоёв HUD-интерфейсов (drone-view/mech-hood) |
| `WModel_FXDepthLine.fp`, `FXHeatHaze(Mask)(Depth).fp`, `FXRenderSurface.fp`, `FXSCAR.fp` | HL | частные VFX моделей (линия сканера, тепловое марево с маской/учётом глубины, шрам/поверхность рендера) |

### 1.7 GUI / меню

| Файл | Диалект | Назначение |
|---|---|---|
| `CMWnd_CubeMenu_Blend.fp`, `_ScanLine.fp` | ARB(HL) | блендинг и скан-линия кубического меню |
| `CMWnd_ModTexture_PaintVideo_YUV2RGB.fp` (+`_EFBB`) | ARB, ATI | YUV→RGB конвертация видео для "paint video" виджета |
| `GUIFadeToWhite.fp`, `GUIRGB2Grey.fp`, `GUIRadialBlur.fp`/`2.fp` | ARB | фейд/ч-б/радиальный блюр GUI-эффектов |
| `WFrontEndMod_Mask.fp` (ARB), `WFrontEndMod_Blend.fp` (HL) | — | смешение не-постпроцессенного GUI-оверлея с маской поверх постпроцесса |
| `TexEnvProj1.fp` | ARB | простое `tex × vertexColor` для проективного texenv |
| `Default.fp`/`Default_EFBB.fp` | ARB, ATI | заглушки материала (константный/passthrough цвет) |
| `GLSL_TexEnv0..4.fp`, `GLSL_TexEnvAlpha.fp` | GLSL100 | fixed-function texenv эмуляция (modulate/decal, по числу активных стадий 0–4) — **прямой аналог нашего текущего мини-шейдера** |
| `WClientMod_GV.fp`, `_HazeDepth_0/1.fp`, `_Power.fp`, `_ScreenFX0..4.fp`(+`_EFBB`) | ARB, HL, ATI | клиентские пост-эффекты (game-view вставка, дымка по глубине, "power"-вспышка, экранные FX-пресеты COR:EFBB) |

### 1.8 Служебные инструменты (офлайн)

| Файл | Диалект | Назначение |
|---|---|---|
| `Tools/XWBSP_Radiosity*.fp` (6 файлов) | ARB (`NV_fragment_program2`) | офлайн-баунсинг радиосити для запекания лайтмапов BSP (Front/Left/Right/Up/Down — грани хемикуба; см. §5) |

---

## 2. Соответствие «имя, которое просит движок» → «что есть в данных»

Пройдено: `ms_lProgramsShading20[]` (`XRShader_FP20.cpp:2-23`) и все литералы
`SetProgram("...")` в `Source/P5/Shared/MOS/` (полный список см. вывод
`grep -rn 'SetProgram(' Source/P5/Shared/MOS/`).

### 2.1 Forward per-light (`RenderShading_FP20*`, `XRShader_FP20.cpp` / `XRShader_LightField.cpp`)

| Имя движка | Файл есть? | Аналог | Обоснование |
|---|---|---|---|
| `XRShader_FP20_LFM` | нет | нет прямого — см. §5 | лайтмап-маппинг для BSP2 не имеет отдельного forward-`.fp` в корпусе; логика есть только внутри `XRShader_BRDF3.fp` (`*lightfieldmapping`, деферred) |
| `XRShader_FP20_LF` | нет | нет прямого | «lightfield» = per-object ambient-cube (`CXR_ShaderParams_LightField::m_lLFAxes[6]`); в forward-варианте аналога нет, в BRDF3 — блок `*if_lightfield` (§3) |
| `XRShader_FP20_NDS` | нет | `XRShader_SinglePass_Dst2_SpecNormal.fp` / `_SpecDiffuse.fp` (ARB/NV) | оба явно помечены `File: Program for CXR_Shader::RenderShading_FP20`; NDS = Normal+Diffuse+Specular, без проекции — это ровно то, что шлёт `RenderShading_FP20_COREFBB` при `TextureIDProj == 0` (`XRShader_FP20.cpp:402`) |
| `XRShader_FP20_NDSP` | нет | `..._Proj_SpecNormal.fp` / `..._Proj_SpecDiffuse.fp` (ARB/NV) | тот же путь, `TextureIDProj != 0` (та же строка 402) |
| `XRShader_FP20_NDSEATP` | нет | концептуально — `XRShader_BRDF3.fp` (forward-эквивалента нет) | `RenderShading_FP20` (не-COREFBB, `XRShader_FP20.cpp:844`) всегда шлёт эту единственную уберпрограмму — 8 текстур (Diffuse/Specular/Normal/Attribute/Transmission/Proj1/Proj2/Environment, комментарий `XRShader_FP20.cpp:684-691`); в реальном шипе этот путь не используется (deferred по умолчанию, см. §0) — играет ту же концептуальную роль что и BRDF3 в forward-варианте, но **исходника для GLES3-порта нет вообще** |

Выбор между `_SpecNormal` и `_SpecDiffuse` не виден в `XRShader_FP20.cpp` (там всегда
одна и та же виртуальная атрибутика с текстурой Normal по слоту 2) — вероятно,
решался на уровне данных материала (в какой карте запечён альфа-канал спек-маски);
для GLES3-порта достаточно поддержать оба варианта как два пути одного шейдера
(`if (uHasNormalAlpha) ... else ...`), см. §4.

### 2.2 Deferred (`RenderShading_FP20Def*`, `XRShader_FP20Def.cpp` / `XRShader_FP20Def_LightField.cpp`)

| Имя движка | Файл есть? | Аналог |
|---|---|---|
| `XRShader_FP20Def_LFM` | нет | `XRShader_BRDF3.fp`, ветка `*if_deferredin` + `*if_lightfieldmapping` |
| `XRShader_FP20Def_LF` | нет | `XRShader_BRDF3.fp`, ветка `*if_deferredin` + `*if_lightfield` |
| `XRShader_FP20Def_NDS` | нет | `XRShader_BRDF3.fp`, `*gen deferredin + deferredfromdepth + light0` |
| `XRShader_FP20Def_NDSP` | нет | `*gen deferredin + deferredfromdepth + light0 + projmap0` |
| `XRShader_FP20Def_NDSE`/`NDSEP`/`NDSEAP` | нет | те же деферред-комбинации BRDF3 с `environmentmap`/`projmap0..2`/несколькими `light0..2` (полный список комбинаций — `XRShader_BRDF3.fp:63-121`, директива `*generate`) |
| `XRShader_FP20DefLin_*` (весь набор) | нет | те же, но с `XR_SHADERMODETRAIT_LINEARCOLORSPACE` (линейное цвет. пространство вместо гамма); в данных отдельного файла нет — предполагается тот же BRDF3, скомпилированный с другим таргет-флагом |

Вывод G-buffer, который читают все `FP20Def_*`/BRDF3-деферред-варианты, пишется
`XRShader_DeferredMRT.fp` (3 MRT: нормаль+альфа, диффуз+альфа, спекуляр+альфа) или
`XRShader_DeferredNormal.fp` (только нормаль, без MRT) — оба **есть** в корпусе и
математически совпадают с чтением в BRDF3 `*if_deferredin` (см. §3).

### 2.3 Прочие `SetProgram()` в движке — все находятся буквально

| Имя движка (`SetProgram`) | Место | Файл в данных |
|---|---|---|
| `XRShader_DeferredMRT` | `XRShader.cpp:1732`, `XRVBOperators.cpp` (deferred array) | `ARB_Fragment_Program/XRShader_DeferredMRT.fp` — есть |
| `XRShader_DeferredNormal` | `XRShader.cpp:1824,2067`, `WTriMesh.cpp:2262` | `ARB_Fragment_Program/XRShader_DeferredNormal.fp` — есть |
| `XRShader_DecalNormalTransform` | `XRShader.cpp:1833`, `XRVBOperators.cpp:1946` | `ARB_Fragment_Program/XRShader_DecalNormalTransform.fp` — есть |
| `XRShader_DecalNormalTransformTM` | `XRShader.cpp:1856,2099`, `WTriMesh.cpp:2652` | `ARB_Fragment_Program/XRShader_DecalNormalTransformTM.fp` — есть |
| `XRShader_DecalTM` | `XRShader.cpp:1857,2100`, `WTriMesh.cpp:2661` | `ARB_Fragment_Program/XRShader_DecalTM.fp` — есть |
| `XRShader_MotionMap` | `XRShader.cpp:2056,2192` | `ARB_Fragment_Program/XRShader_MotionMap.fp`, `HL_Shading/XRShader_MotionMap.fp` — есть (обе версии) |
| `TexEnvProj1` | `XREngine.cpp:3145`, `XRVBOperators.cpp:2263` | `ARB_Fragment_Program/TexEnvProj1.fp` — есть |
| `XREngine_MotionBlur` | `XREngine.cpp:4557` | `ARB_Fragment_Program/XREngine_MotionBlur.fp` — есть |
| `XREngine_Histogram` | `XREngine.cpp:4648` | `ARB_Fragment_Program/XREngine_Histogram.fp` (ARB), `HL_Shading/XREngine_Histogram.fp` — есть |
| `XREngine_FinalLinear`/`XREngine_Final`/`XREngine_FinalNoExposure` | `XREngine.cpp:4822-4828` | явных файлов с этими именами нет; вероятно варианты компиляции `HL_Shading/XREngine_Final5.fp` под разные флаги (линейный/без экспозиции) |
| `CMWnd_ModTexture_PaintVideo_YUV2RGB` | `XRVBOperators_NVidia.cpp:1347` | есть в ARB и ATI (+`_EFBB`) |
| `XRUtil_VL3P0..3_T1` | `XRUtil.cpp:255-258` | нет в корпусе (вершинное освещение утилит, не по этой части) |
| `WModel_DarklingEffect_FakeSpawn` | `XRVBOperators.cpp:1619` | нет в корпусе |
| `XREngine_GaussClamped` | `XRUtil.cpp:3162-3163` | `ARB_Fragment_Program/XREngine_GaussClamped.fp` — есть |
| `XREngine_RadialBlur` | `XRUtil.cpp:3400` | `ARB_Fragment_Program/XREngine_RadialBlur.fp` — есть |

Итог: **все не-FP20/FP20Def-программы движка резолвятся буквально** — расхождение
изолировано ровно в семье `RenderShading_FP20*`/`RenderShading_FP20Def*`, что
подтверждает вывод §0 (это единственный код-путь, чья таблица имён устарела
относительно реального содержимого `System/GL/`).

---

## 3. `XRShader_BRDF3.fp` — разбор

### 3.1 Синтаксис языка (HL_Shading DSL)

Файл — не GLSL, а Starbreeze-DSL "hls" (High-Level Shading), компилируемый в
целевой язык (Cg/HLSL/GLSL) их офлайн-компилятором. Структура:

* `*_head_ { *type hls; *flags ... }` — метаданные компиляции (`nodebug`,
  `dopreparse`).
* `*flags { *имя битовая_маска ... }` — булевы фичи-переключатели программы
  (`BRDF3.fp:36-61`, 22 флага, см. §3.2). Это ровно та же роль, что и `*if_*`/`*ifnot_*`
  директивы в `VP.xrg` (`Docs/VP_Reference.md` §1.2) — механизм генерации множества
  конкретных программ из одного шаблона по комбинации флагов.
* `*generate { *gen ...; *permute A+B+C { *gen ...} }` — явное перечисление, какие
  комбинации флагов реально компилируются в отдельные варианты программы (не все
  2^22 комбинаций, а осмысленное подмножество, `BRDF3.fp:63-122`). `*permute X`
  разворачивается в генерацию `*gen` для каждой комбинации бит из X (2^N вариантов),
  вложенные `*permute` дают декартово произведение.
* `*param { *ifX { *envX Имя } }` — список констант программы (`program.env[]`),
  условно включаемых по флагам; `*envX` — "выделить следующий свободный env-регистр
  под это имя".
* `*texture { *tex2D_N / *texCube_N samplerName }`, `*attrib { *texcoordN name }`,
  `*output { *color/*color1/*color2 name }` — привязка семплеров к текстурным юнитам,
  интерполянтов к texcoord-каналам, и выходных RT (MRT для `deferredmrt`).
* `*source { *INCLUDE "файл.fph"; *секция "код" }` — общие функции (см. §3.5),
  секции именованы произвольно (`*doeet`, `*dofres`, `*util`...) — это просто именованные
  куски исходника, вставляемые в порядке объявления.
* `*main { *if_X "код" }` — тело программы; `*do`/`*do1`/`*do2`/`*dovar`/`*doenvinit`
  и т.п. — синонимы "вставить код здесь" (имя блока не имеет семантики, кроме
  документирующей — историческая манера нарезки на именованные шаги).

Итого: флаги → `*generate` определяет набор реально скомпилированных вариантов →
`*param`/`*texture`/`*attrib` для каждого варианта включают только нужные ветки →
`*main` собирает финальный код условной вставкой `*if_/*ifnot_` блоков. GLSL-аналог
такого подхода в нашем порте — набор `#define`-переключателей + один uber-шейдер,
скомпилированный в несколько вариантов через препроцессор (см. §6).

### 3.2 Полный список флагов (`BRDF3.fp:36-61`)

| Флаг | Значение | Смысл |
|---|---|---|
| `anisotrophic` | 1 | анизотропный BRDF (направление анизотропии из текстуры) |
| `materialmask` | 2 | смешение до 3 материалов по маске (multi-material blend) |
| `environmentmap` | 4 | кубическая карта окружения (specular IBL) |
| `macronormalmap` | 8 | доп. "макро"-нормаль (крупномасштабная деталь поверх основной) |
| `light0`/`light1`/`light2` | 16/32/64 | до 3 динамических точечных/спот-источников в одном проходе (forward-стиль внутри деферред-резолва) |
| `projmap0..2` | 128/256/512 | проекционные (кубические) карты для соответствующих источников |
| `detailnormalmap` | 1024 | доп. детальная нормаль (тайлинг-микрорельеф) |
| `deferredalphaonly` | 0x800 | писать в G-buffer только альфа-канал (surface-normal Z компонента, для декалей поверх готового буфера) |
| `lightfield` | 8192 | per-object ambient-cube (`LF_Axis0..5`, вершинный аналог — `CRC_TEXGENMODE_LIGHTFIELD`, `VP_Reference.md` §3.12) |
| `lightfieldmapping` | 16384 | per-texel лайтмап BSP2 (4 текстуры `LFM0..3`, см. §5) |
| `deferredin` | 32768 | это лайтинг-резолв проход, читающий готовый G-buffer (а не запись материала) |
| `deferredout`/`_n`/`_d`/`_s` | 0x10000/0x20000/0x40000/0x80000 | запись материала в G-buffer: всё разом (MRT) или по одному каналу (normal/diffuse/specular) для железа без MRT |
| `deferredmrt` | 0x100000 | использовать 3 одновременных output вместо трёх отдельных проходов `_n/_d/_s` |
| `fixedtangentspace` | 0x200000 | TBN приходит как готовые константы (`e_TS2W_Mat_0..2`), а не интерполируется по вершинам — путь для деферред-резолва (полноэкранный quad без TBN на вершину) |
| `deferredfromdepth` | 0x400000 | восстанавливать позицию пикселя из буфера глубины (`ConvertDepth`/`ConvertDepthUVToPos`, `XR_FPDepth.fph`) вместо интерполированной world-position |
| `deferredalphamap` | 0x800000 | доп. текстура-маска для blend-декалей поверх G-buffer (`sampler_AlphaMap`) |

### 3.3 Входы

G-buffer (описан в шапке файла, `BRDF3.fp:10-18`, все компоненты в **мировом
пространстве**):

```
Map0 (sampler_Diffuse0 в *deferredin-чтении): N.x, N.y, N.z, SurfN.x
Map1 (sampler_Diffuse0 при записи /*MaterialSpecular0 при чтении — см. ниже): D.r, D.g, D.b, SurfN.y
Map2 (sampler_MaterialSpecular0): AO, Fresnel, log2(SpecIntensity)/16, SurfN.z
```
(`N` — bump-нормаль, `SurfN` — геометрическая нормаль поверхности без бампа —
используется отдельно для self-shadow члена, чтобы бамп не давал резких артефактов
на грани силуэта.)

Текстурные юниты (`*texture`, `BRDF3.fp:229-278`, полный уберackage):
`0`=Diffuse0, `1`=MaterialSpecular0, `2`=Normal0, `3`=AnisotropicVec, `4..6`=NormalDetail0..2
(если `detailnormalmap`+`materialmask`), `7`=NormalMacro (если `macronormalmap`),
`8`=Env0, `9`=Env1 (cube), `10`=Depth (если `deferredfromdepth`), `11`=AlphaMap,
`12..14`=ProjMap0..2 **или** LFM0..2 (те же слоты переиспользуются в зависимости от
`lightfieldmapping` vs `projmap*`), `15`=LFM3.

Интерполянты (`*attrib`): `texcoord0`=mapping UV; `texcoord1`=либо мировая позиция
пикселя (`fixedtangentspace`), либо строка 0 TBN-матрицы (иначе, а строки 1,2 — в
texcoord2/3); `texcoord5/6/7` = ProjMap UV **или** LFM UV + intensity-scale, в
зависимости от `lightfieldmapping`.

Константы (`*param`): цвета/фреснели диффуза-спекуляра (с материал-маской — по 3 на
канал), позиции/интенсивности до 3 источников (`LightPos0_w.w` = `1/Range²`),
6 `LF_Axis0..5` (ambient-cube для lightfield), `LFM_Scale` (интенсивность лайтмапа),
`VPParam/VPConst/VPScale/DepthScale/V2WMat_0..2` (реконструкция позиции из глубины,
см. `XR_FPDepth.fph`), `e_TS2W_Mat_0..2` (фиксированный TBN для деферред-резолва).

### 3.4 Общие функции (`XR_FPUtil.fph`, `XR_FPDepth.fph`, разбор для BRDF3)

* `FresnelBaseDielectric(cosi)` — аппроксимация Френеля для диэлектрика двумя
  экспонентами по `log2` (быстрая замена честной формулы), используется в
  `Fresnel(cosi, fresparams)` (`BRDF3.fp:349-355`): подмешивает `_fresparams.rgb`
  (цвет металла на грани) при `_fresparams.w`≈"металличность"-подобный параметр.
* `ConvertNormalTexel(texel)` / `ConvertNormalTexel_NoRemap(texel)` — 2-компонентная
  упаковка нормали: хранятся только `g,a` каналы текстуры, `x`-компонента
  восстанавливается как `sqrt(1 - g² - a²)` (стандартная "derive Z" упаковка,
  экономит канал под другие данные — здесь `.b`/`.r` заняты spec/height).
* `ConvertDepth(depthtexel, VPConst)` — раскодирует упакованный в RGB буфер глубины
  (24-бит fixed-point через `dot(..., {1/256,1/256²,1/256³})`) в view-space Z, затем
  `-1/(depth*VPConst.w - VPConst.x) * VPConst.z` — обратное проективное преобразование.
* `ConvertDepthUVToPos(depth, uv, DepthScale)` — по view-space Z и UV восстанавливает
  view-space XYZ (`pos.xy = (uv*scale+offset)*depth`), т.е. classic deferred
  position-from-depth без отдельного G-buffer-канала под позицию.
* `SafeNormalize(v, thres)` — `normalize()` с защитой от деления на почти-ноль
  (лайтмап-направление `l_ts` может выродиться при плоском освещении).
* `Attenuation(pos, light, rangeInvSqr)` (`BRDF3.fp:333-339`) — квадратичное
  затухание `sqr(saturate(1 - lensqr*rangeInvSqr))` — **тот же паттерн затухания**,
  что и в ARB per-light шейдерах §4 (`r1.w = sqr(saturate(1 - distSq/range²))`), просто
  записан на HL вместо ARB-ассемблера.
* `SelfShadow(cosi, k1, k2)` — `saturate((k1+cosi)*k2)` — плавное само-затенение
  у горизонта (не даёт резкого обрыва света на терминаторе).

`__BRDF3_TestStuff.txt` содержит **не используемые в финале** альтернативные версии
BRDF (`BRDF1`, `BRDF2`, экспериментальный `BRDF3` с честным Cook-Torrance
geometry-термом `Gmask/Gshadow`) — черновики итераций дизайна модели; полезны как
референс "откуда взялась" финальная упрощённая формула в `BRDF()` самого `.fp`
(упрощение: `Gmask` зафиксирован в `1.0`, честный geometry-term выброшен ради
производительности).

### 3.5 Модель освещения — псевдо-GLSL

Основной BRDF (`BRDF3.fp:401-434`, без анизотропии):

```glsl
vec3 BRDF(vec3 n, vec3 nsurf, vec3 e, vec3 l,
          vec4 kdiff, vec4 kspec /* rgb=цвет, a=power */,
          vec4 fresparams, vec3 envmap)
{
    kspec.a = max(1.0, kspec.a * kspec.r);
    vec3 h = normalize(e + l);
    float cosi = saturate(dot(n, l));

    vec3 fres = FresnelApprox(cosi, fresparams) * saturate(kspec.a * 0.125);
    vec3 frescolor = mix(fresparams.rgb, vec3(1.0), fres);

    float specweight1 = 1.0 - inversesqrt(kspec.a);        // растёт с "блеском"
    float specweight2 = max(fres.r, max(fres.g, fres.b));    // Френель на грани

    float speclobe   = pow(saturate(dot(n, h)), kspec.a);    // Phong (не Blinn — h, но экспонента как у Blinn-Phong)
    float ispecular  = (kspec.a + 3.4515) * speclobe * 0.038969686; // нормировка лепестка
    float idiffuse   = 1.0 / (2.0 * PI);
    vec3  diffuse    = idiffuse * kdiff.rgb;
    float specular   = ispecular / max(1e-6, dot(e, h));

    float cosisurf   = dot(nsurf, l);
    float selfshadow = SelfShadow(cosisurf, 0.0, 8.0);

    return (envmap * specweight1
            + mix(diffuse, frescolor * specular, specweight1 * specweight2))
           * (cosi * selfshadow);
}
```

Итоговое освещение на пиксель (`*dolight`, `BRDF3.fp:806-980`):

```glsl
vec4 result;
result.rgb = 0.0; result.a = DiffuseColor.a;
if (light0) result.rgb += BRDF(..., l0_w, ...) * attn0;
if (light1) result.rgb += BRDF(..., l1_w, ...) * attn1;
if (light2) result.rgb += BRDF(..., l2_w, ...) * attn2;
if (lightfield)        result.rgb += /* ambient-cube из 6 BRDF-вызовов по осям, см. ниже */;
if (lightfieldmapping) result.rgb += BRDF(..., l_w_from_LFM, ...) * attn_lfm;
result.rgb *= 4.0;   // компенсация нормировки диффуза/спекуляра
oCol0 = result;
```

**Проекционные карты** (`projmap0..2`) применяются как маска затухания источника
*до* вызова BRDF: `attnN *= textureCube(sampler_ProjMapN, tc).rgb` (§3.3, texcoord из
той же матрицы проекции света, что и в ARB-шейдерах §4 — разница лишь в том, что
здесь заранее прожекция читается как RGB-маска, а не альфа).

**Environment map**: только когда не `deferredin` (т.е. в чистом forward или уже
раскрытом виде) — `r_w = reflect(-e_w, n_w)`, лод по блеску специалярности
(`envlod = EnvColor.w - log2(specexp) + cosi`), домножается на тот же Френель, что
и спекуляр, и на `SelfShadow` по геометрической нормали.

**AO** в этой версии шейдера **не применяется к финальному цвету** (`ao_out = 0.0`
жёстко — заготовка для будущего SSAO-входа, реально читается только `FresParams.w`/
`spec_out` из Map2; SSAO комбинируется отдельно, см. `XREngine_SSAO*.fp` §1.4, вне
BRDF3).

**Lightfield (ambient-cube, объектный)**: 6 констант `LF_Axis0..5` — цвет освещения
по −X,+X,−Y,+Y,−Z,+Z полусферам нормали (тот же принцип, что и вершинный
`CRC_TEXGENMODE_LIGHTFIELD`, `VP_Reference.md` §3.12, только здесь честно вызывается
`BRDF()` 6 раз с фиксированными направлениями света вместо диффузного дот-произведения).
**Lightfieldmapping** — см. §5.

### 3.6 Что нужно для forward мировой геометрии, а что только для deferred

| Часть | Forward (мировая геометрия, наш приоритет) | Только deferred |
|---|---|---|
| `BRDF()` основная формула, `Fresnel`, `SelfShadow`, `Attenuation` | нужны | — |
| Чтение Diffuse0/MaterialSpecular0/Normal0 **как обычных материальных карт** (не G-buffer) | нужно (форвард читает их напрямую по UV, не по экрану) | — |
| `light0` (1 источник за проход, forward per-light как в §4) | нужно | — |
| `light1`/`light2` в одном проходе | не нужно (в forward это просто ещё один проход per-light) | нужно (экономия проходов деферред-резолва) |
| `projmap0..2`, `environmentmap`, `lightfield`, `lightfieldmapping` | нужны (материал/лайтмап не зависят от rendering path) | — |
| `deferredin`, `deferredfromdepth`, `deferredout*`, `deferredmrt`, `deferredalphamap`, `fixedtangentspace` | не нужны | нужны только если решим делать деферред-рендер позже |
| `materialmask`, `anisotrophic`, `macronormalmap`, `detailnormalmap` | опциональные материальные фичи — не критичны для базового освещённого BSP2 | то же |

---

## 4. Per-light shading EFBB (`XRShader_SinglePass_Dst2_Spec{Normal,Diffuse}(_Proj)`)

Обе программы (в любом диалекте) реализуют **один и тот же алгоритм**; различаются
только источником спек-маски (см. ниже) и наличием проекционной карты. Полный текст
разобран построчно из `ARB_Fragment_Program/XRShader_SinglePass_Dst2_Spec{Normal,Diffuse}(_Proj)?.fp`.

### 4.1 Входы

Текстуры: `texture[0]` = Diffuse Map, `texture[1]` = Projection Map (**CUBE**, только
`_Proj_`-вариант), `texture[2]` = Normal+Specular map (RGB=нормаль тангентного
пространства, A=спек-маска — только в `SpecNormal`-варианте; в `SpecDiffuse` спек-маска
берётся из `texture[0].a`, т.е. Diffuse Map). Texcoord'ы (сверено с генератором
`VP.xrg`/`Docs/VP_Reference.md` §3): `texcoord[0]` — mapping (обычный UV),
`texcoord[1]` — animated model-space позиция пикселя (`CRC_TEXGENMODE_PIXELINFO`,
`VP_Reference.md` §3.2), `texcoord[3]` — **IPTSLV**, interpolated tangent-space
light vector (`CRC_TEXGENMODE_TSLV`, `VP_Reference.md` §3.1 — именно этот
texgen-режим питает данный шейдер), `texcoord[4]` — **IPTSEV**, tangent-space eye
vector (второй TSLV-вызов с позицией глаза вместо позиции света — тот же
`CRC_TEXGENMODE_TSLV`, но `pos=eye`), `texcoord[7]` — координата проекции (только
`_Proj_`).

Константы (`program.env[]`): `[0]` LightPosition (world xyz), `[1]` LightRange
`{1/R, R, 1/R², R²}`, `[2]` LightColor (0-2 диапазон, HDR-подобный овербрайт), `[3]`
SpecColor+Power (`.a` = экспонента Phong).

### 4.2 Формула цвета — псевдо-GLSL

```glsl
// Затухание (квадратичное по расстоянию до источника в модельном пространстве)
vec3  toLight   = LightPosition.xyz - PixelPosition.xyz;
float distSq    = dot(toLight, toLight);
float attnLin   = saturate(distSq * LightRange.z /* = 1/R² */);
float attn      = sqr(1.0 - attnLin);                 // (1 - d²/R²)² — сфера затухания

if (Proj) attn *= textureCube(ProjMap, ProjMapTexCoord).a;  // маска прожектора/фонарика

// Нормаль и векторы (все уже в тангентном пространстве, только нормализовать)
vec3 N   = normalize(NormalSpecTex.rgb * 2.0 - 1.0);   // нормаль из карты
vec3 L   = normalize(IPTSLV);                          // к источнику
vec3 E   = normalize(IPTSEV);                          // к глазу
vec3 R   = 2.0 * dot(N, E) * N - E;                    // отражённый (Phong, не half-vector!)

// Самозатенение по касательной X-компоненте TSLV (аппроксимация terminator'а
// без реального dot(N_geom, L) — использует X-компоненту нетронутого
// интерполированного (не нормализованного!) IPTSLV как прокси-косинус)
float selfShadow = saturate((0.25 + IPTSLV.x) * 4.0);
attn *= selfShadow;

// Диффуз
vec3 diffuse = LightColor.rgb * DiffuseTex.rgb * 2.0 * saturate(dot(N, L));

// Спекуляр (Phong: (R·L)^power, замаскированный альфой из normal- либо diffuse-карты)
float specMask = SpecNormalVariant ? NormalSpecTex.a : DiffuseTex.a;
vec3  specular = SpecColor.rgb * pow(saturate(dot(L, R)), SpecColor.a) * specMask;

vec3 result = (diffuse + specular) * attn;
```

Итог: **Phong-затенение** (не Blinn-Phong — используется явный отражённый вектор
`R`, а не полу-вектор `H`), спекуляр-маска берётся из альфы одной из двух карт в
зависимости от того, что доступно у материала (нет ни одного явного switch'а в
самом коде — выбор какой из двух `.fp`-файлов используется, происходит на уровне
выбора имени программы движком/данными, не показан в разобранном исходнике).

### 4.3 Отличия по железу/диалектам

* **ARB vs NV** (`NV_Fragment_Program/`) — математически **идентичны**; NV-вариант
  просто добавляет `OPTION NV_fragment_program;` (больше temp-регистров/инструкций,
  снятие лимитов ARBfp1.0), код 1-в-1.
* **NV2** (`NV_Fragment_Program2/XRShader_SinglePass_Dst2_*`) — тот же алгоритм, но
  компактнее записан (доступны `NRMH` для нормализации одной инструкцией вместо
  `DP3+RSQ+MUL`); функционально идентичен.
  `XRShader_FP20SS_Spec{Normal,Diffuse}(Proj).fp` в этой же папке — **другая
  программа** с тем же базовым освещением ("SS" = soft-shadow): добавляет 16-tap
  дизеринг-PCF выборку из shadow-mask + depth-stencil буфера (`texture[5]`/`[6]`,
  смещения `SMOfs0..15`, глубинный тест `|depth_neighbor - depth_center| < tolerance`
  перед усреднением маски), результат которого домножает финальный цвет —
  это forward-эквивалент проецируемых теней (`RenderShading_FP20SS`, режим
  `XR_SHADERMODE_FRAGMENTPROGRAM20SS`, требует FP30+CopyDepth caps).
* **ATI** (`XRShader_{Spec,Proj_Spec}{Normal,Diffuse}.fp`, `RenderShading_FP14`) —
  тот же алгоритм на 2-фазном register-combiner ассемблере (`sbzfp.1.4`): фаза 1
  трансформирует нормаль в half-angle-пространство для табличного `(N·H)^k`
  lookup (`texld r0, r1, str` — фактически чтение из кубической текстуры-таблицы
  степенной функции вместо `POW`), фаза 2 читает диффуз/спек/проекцию и собирает
  финальный цвет; математически то же самое, но `pow()` заменён на текстурный
  lookup (типичное ограничение ps.1.4-класса железа — нет general `POW`).
  Диспетчер этого пути (`XR_SHADERMODE_FRAGMENTPROGRAM14`) закомментирован в
  `XRShader.cpp:1549` — считать полностью legacy, не переносить.

---

## 4a. Лайтмапы В ИГРЕ ЕСТЬ (исправлено 2026-07-29)

> **ОТМЕНЯЕТ прежнюю редакцию этого раздела.** С 2026-07-27 здесь стояло
> утверждение «в самом Riddick лайтмапов визуально нет, весь свет
> динамический». Владелец перепроверил это на оригинальной игре 2026-07-29:
> **лайтмапы есть и дают заметный вклад**. Прежнее утверждение было ошибочным,
> и все выводы, которые из него следовали, недействительны.

Практические следствия — ровно обратные тому, что стояло раньше:

- **восстановление лайтмапов это приоритетная часть работы, а не побочная**;
- слабый или нулевой визуальный вклад `XRShader_FP20_LFM` — **признак ошибки
  в нашей реализации**, а не ожидаемое поведение. Раньше он списывался на
  «в игре их и нет» — так делать нельзя;
- аддитивные per-light проходы `XRShader_FP20_NDS` / `_NDSP` (§4) остаются
  важны, но они дополняют лайтмап-базу, а не заменяют её;
- тёмные стены, скорее всего, объясняются именно отсутствующей/сломанной
  лайтмап-базой, а не только нехваткой per-light проходов.

Механика лайтмапов BSP2 (directional / radiosity-normal-map, 4 текстуры
`LFM0..3`) разобрана в §5 — этот разбор верен и остаётся в силе, менялась
только оценка их значимости для картинки.

## 5. Лайтмапы (Light Field Mapping, BSP2)

### 5.1 В корпусе `.fp` — ничего похожего

Ни один `.fp`-файл не реализует форвард light-field-mapping для BSP2 напрямую.
`XRShader_FP20_LFM` (запрашиваемое имя, `XRShader_FP20.cpp:4`) не имеет файла (§2.1).
Единственное место с реальной математикой LFM — деферред-путь внутри
`XRShader_BRDF3.fp` (`*if_lightfieldmapping`, `BRDF3.fp:939-969`), разобранный ниже.

### 5.2 Как устроены данные

`Source/P5/Shared/MOS/XRModels/Model_BSP2/WBSP2Model.cpp:2417-2425`:

```cpp
uint iLMTexture = pQueues[...].m_iLMTexture;                 // индекс лайтмап-кластера поверхности
CXR_ShaderParams_LightFieldMapping LFMParams;
LFMParams.CreateLFM(M2W, W2V, &m_lLMTextureIDs[iLMTexture*4], 3, 4);  // 4 текстуры на кластер!
pShader->RenderShading_LightFieldMapping(&lVB[iInner], &LFMParams, &lpSSP[iInner]);
```

`m_lLMTextureIDs` (`WBSP2Model.h:1353`, `TArray<uint16>`) — плоский массив ID текстур,
**по 4 подряд на каждый уникальный лайтмап-кластер** поверхности BSP2. Именно эти
4 ID и есть `LFM0..3` из `CXR_ShaderParams_LightFieldMapping::m_lLFMTextureID[4]`
(`XRShader.h:326`), которые дальше в `CXR_VirtualAttributes_ShaderFP20_LFM::Create`
(`XRShader_LightField.cpp:481-484`) кладутся в текстурные слоты 10-13 (`Attrib_TextureID(10..13, ...)`,
`XRShader_LightField.cpp:542-545`) — те же самые слоты 12-15 (со сдвигом за счёт
проекционных карт), что и `sampler_LFM0..3` в BRDF3 (`BRDF3.fp:267-273`).

### 5.3 Математика (`BRDF3.fp:939-969`, псевдо-GLSL)

```glsl
vec4 lfm0 = texture(sampler_LFM0, tcLFM);
vec4 lfm1 = texture(sampler_LFM1, tcLFM);
vec4 lfm2 = texture(sampler_LFM2, tcLFM);
vec4 lfm3 = texture(sampler_LFM3, tcLFM);
vec4 lfm4 = vec4(lfm1.a, lfm2.a, lfm3.a, 0.0);   // 6-е направление "спрятано" в альфа-каналах 1..3

vec3 nSat0 = saturate( n_ts);   // положительные компоненты нормали в тангентном пр-ве
vec3 nSat1 = saturate(-n_ts);

// Взвешенная сумма 6 направленных "ambient cube" базисов лайтмапа по компонентам нормали
vec3 lfmColor = lfm0.rgb * nSat0.r     // +X
              + lfm1.rgb * nSat1.b     // -Z (переставлено!)
              + lfm2.rgb * nSat1.g     // -Y
              + lfm3.rgb * nSat0.b     // +Z
              + lfm4.rgb * nSat0.g;    // +Y  (лежит в альфа-каналах lfm1..3)
lfmColor *= 4.0 * lmIntensityScale * LFM_Scale.rgb;

// Направление на "виртуальный" источник восстанавливается из тех же 4 текстур
// (нужно только для спекуляра — диффуз уже посчитан выше как сумма по базису)
vec3 lTs = vec3(dot(lfm0.rgb, vec3(1,2,1)),
                dot(lfm4.rgb, vec3(1,2,1)) - dot(lfm2.rgb, vec3(1,2,1)),
                dot(lfm3.rgb, vec3(1,2,1)) - dot(lfm1.rgb, vec3(1,2,1)));
vec3 lW = SafeNormalize(TS_to_W(lTs), 0.01);

result.rgb += BRDF(matweights, n_w, nsurf_w, e_w, lW, DiffuseColor, SpecularColor, FresParams, anisodir_w, envmap.rgb) * lfmColor;
```

Это классическая схема **directional/radiosity-normal-map лайтмапа** (тот же
принцип, что Half-Life 2 Radiosity Normal Mapping / ambient cube): 6 базисных
направлений (±X,±Y,±Z в тангентном пространстве поверхности), запечённых в 4
RGBA-текстурах (RGB×4 + A×3 = 15 из 16 нужных float-каналов на пиксель лайтмапа,
шестое направление избыточно/выведено, либо одна из комбинаций не хранится
раздельно — в любом случае 4 текстуры на кластер, что и подтверждается кодом
BSP2-модели). Дополнительно синтезируется приближённое направление на "эквивалентный"
источник света `lW` (по разнице базисов) — используется только для спекулярного
члена BRDF, диффузная часть считается напрямую суммой базисов, без вызова BRDF.

Объектный (не per-texel) аналог — `*if_lightfield` (§3.5): те же 6 направлений, но
всего 6 констант `LF_Axis0..5` на весь объект вместо текстуры на пиксель — это
вершинный `CRC_TEXGENMODE_LIGHTFIELD` (`VP_Reference.md` §3.12) продолженный во
фрагментный шейдер вызовом `BRDF()` 6 раз с осевыми направлениями вместо одного
диффузного дот-произведения.

### 5.4 Рекомендация: из чего собирать LFM для GLES3

Готового `.fp`-референса нет — собирать по формуле §5.3 (она полностью
самодостаточна и уже адаптирована под HL/GLSL-подобный синтаксис, переносится
почти дословно). Понадобится:

1. Загрузчик, который для каждого `iLMTexture`-кластера BSP2 берёт 4 ID из
   `m_lLMTextureIDs` и биндит их на 4 текстурных юнита (в нашем рендере — с учётом
   ограничения "пока 2 юнита в caps", см. §6, для лайтмапов это отдельный проход/
   отдельные caps).
2. Реализация `lfmColor`/`lW` формулы (§5.3) один в один — не требует BRDF-модели
   целиком, диффузную часть можно взять напрямую как `lfmColor` (без вызова полного
   `BRDF()`), это уже физически корректный запечённый диффуз; спекуляр по `lW`
   можно отложить на потом (визуально малозначим для запечённого света).
3. Текстурный формат LFM0-3 — обычные 2D RGBA8 (та же инфраструктура, что уже есть
   для диффузных текстур в `GLES3_Texture`), только 4 штуки на кластер вместо одной
   на материал.

---

## 6. Минимальный набор для освещённых стен BSP2 (GLSL ES 3.00)

Контекст: у нас пока **2 текстурных юнита** в caps и **нет шейдер-генератора** (один
статический шейдер `vCol × texture`, `CRC_GLES3`). Реалистичный путь — не
переносить систему флагов BRDF3 целиком, а сделать 2-3 **отдельных статических
шейдера**, включаемых по наличию контента (лайтмап-текстуры есть → шейдер LFM;
динамический свет есть → шейдер per-light), в порядке возрастания сложности:

### Приоритет 1 — лайтмап-освещённые стены (диффуз, без спекуляра)

* **Вход**: Diffuse texture (юнит 0) + 4×LFM (нужно расширить caps минимум до 5
  юнитов, либо на первом шаге держать лайтмап-проход как отдельный multiply-pass
  поверх обычного diffuse-прохода — 2 юнита хватает: юнит0=diffuse текущего
  прохода, юнит1=упакованный "уже посчитанный на CPU/в отдельном пассе" lightmap;
  но честная реализация всё же требует 4 LFM-юнита одновременно).
* **Формула**: диффузная часть §5.3 (`lfmColor`), без BRDF/спекуляра/Френеля.
* **Сложность**: низкая — по сути ambient-cube взвешивание, ~15 арифметических
  инструкций + 4 текстур-выборки, никакой динамики (all-constant per-cluster кроме
  UV).
* **Даёт визуально**: корректно освещённые (запечённым статическим светом) стены
  BSP2 с направленным светотенением от нормали — именно то, чего сейчас не хватает
  ("градиенты на стенах", `Docs/Render_Strategy.md`), без единого динамического
  источника.

### Приоритет 2 — один динамический источник по стенам/пропсам (per-light forward)

* **Вход**: Diffuse (юнит0) + Normal+Spec (юнит1) — ровно 2 юнита, укладывается в
  текущие caps. Uniform-блок: `LightPos, LightRange, LightColor, SpecColor+Power`
  (как §4.1).
* **Формула**: §4.2 один-в-один (Phong, самозатенение по `IPTSLV.x`, квадратичное
  затухание) — переносится почти дословно из ARB-ассемблера в GLSL, addition-only
  проход (`GL_ONE, GL_ONE` blend, как у оригинала — `Attrib_SourceBlend(ONE)/DestBlend(ONE)`
  в C++, `XRShader_FP20.cpp:114-115`), по одному проходу на источник в радиусе
  видимости (как в оригинальном движке — стенсиль-scissor по объёму света,
  `CXR_VBFLAGS_LIGHTSCISSOR`).
* **Сложность**: низкая-средняя — требует передачи TSLV/TSEV per-vertex (уже есть
  как часть VP-генератора, `VP_Reference.md` §3.1/§4) плюс сам фрагментный расчёт.
* **Даёт визуально**: динамические точечные/споты (фонарик игрока, лампы) с
  нормал-мэппингом и спекуляром поверх лайтмап-базы приоритета 1 (аддитивно).

### Приоритет 3 — проекционная карта источника (фонарик/прожектор)

* Добавка к приоритету 2: третий текстурный юнит (cube) с `ProjMapTexCoord`,
  домножение затухания на `.a` (§4.2, `_Proj_`-вариант). Требует расширения caps
  до 3 юнитов для этого прохода.
* **Сложность**: тривиальная поверх приоритета 2 (одна доп. текстур-выборка).
* **Даёт визуально**: конус фонарика игрока с реальной маской вместо равномерного
  конуса — важная фича Riddick (stealth/fonарик — центральная механика).

### Не первоочередное (годится позже/по мере надобности)

* **BRDF3-уровень качества** (Френель, спекулярный лепесток с exp2/`FresnelBaseDielectric`,
  environment map, materialmask, anisotrophic) — визуально заметно только на
  металле/влажных поверхностях, не критично для "стены с текстурой и светом"; и
  требует полноценного шейдер-генератора с `#define`-флагами (см. §3.1) — большая
  разовая инвестиция, которую разумно делать после того, как приоритеты 1-2 дадут
  визуально играбельный результат.
* **Soft-shadow (`FP20SS`, 16-tap PCF)** — стенсильные тени движка (упомянуты в
  плане как "критичная фича Riddick") реализуются отдельным механизмом
  (`Docs/Render_Strategy.md` — не через эту 16-tap PCF-маску, которая специфична
  для NV2/FP30-путей; тени — отдельная задача, вне рамок этого документа).
* **Деферред G-buffer путь целиком** (`DeferredMRT`/`DeferredNormal`/`BRDF3`
  deferred-ветки) — не нужен, пока рендер ведёт один forward-проход на объект;
  имеет смысл только если/когда понадобится десятки динамических источников
  одновременно на сложных сценах.
