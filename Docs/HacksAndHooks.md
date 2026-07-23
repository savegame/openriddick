# GLES3 бэкенд — хаки, хуки и диагностика (потребует очистки)

Все изменения ниже — временные (диагностика или костыли-обходы). При
финальной сборке большая часть должна быть удалена. Каждое поле помечено:

- **HACK** — обход настоящей проблемы, требует правильного фикса.
- **DBG** — диагностика/env-var, можно снять после стабилизации.
- **KEEP** — оставить (реальная функциональность, просто под флагом).

Формат записи: где живёт (`file:approx-line`), env var / название,
что делает, зачем добавлено, когда можно удалить.

---

## Env-переменные

### Диагностика рендера

- **DBG** `RIDDICK_DBG_GL=1` (`MDisplaySDL2.cpp:~758`) — включает `[GL-DBG] 60f: ...`
  счётчики draws/verts/upload'ов раз в 60 кадров. Полезно для профайла.
  → удалять когда рендер стабилен.

- **HACK** BSP2 fallback (`WBSP2Model.cpp:~2340`): если
  `pSSP->m_lTextureIDs[XR_SHADERMAP_DIFFUSE]` пуст, скан по остальным
  слотам (NORMAL/SPECULAR/HEIGHT/...) — берём первый ненулевой в Tex0.
  Без этого стены арривала/пита выходят magenta (DIFFUSE не заполнен для
  многих surface'ов в BSP2, движок ждёт shader-pipeline который у нас нет).
  **DBG** `RIDDICK_DBG_SURF=1` — разовый лог `[BSP2-SSP] slots=[...] chose ...`
  для каждого уникального `CXR_SurfaceShaderParams` (cap 64). Убрать когда
  запилим полноценный shader-generator.

- **DBG** `RIDDICK_FORCE_TEX=1` (`SetupCommonUniforms`) — насильно биндит
  яркий magenta/cyan checkerboard 32x32 на unit 0 для каждого draw'а,
  отключает ch1/lighting/alpha-test/dbg-mode. Проверяет, доезжают ли
  world-пиксели до фрагмент-шейдера вообще (когда nrm_raw/pos_local
  дают «невидимо»).

- **DBG** `RIDDICK_DBG_SHADER=uv|pos|no_tex|normal|nrm_raw|pos_local`
  (`~763`) — переопределяет фрагмент-шейдер:
  - `uv` — vUV.xy как RG
  - `pos` — сплошной красный
  - `no_tex` — только vCol
  - `normal` — world-normal `(N+1)*0.5` (после `mat3(uModel)*aNormal`)
  - `nrm_raw` — сырой `aNormal` без transform/normalize (проверяет per-vertex
    plumbing атрибута 4)
  - `pos_local` — `fract(aPos*0.01)` (проверяет per-vertex plumbing атрибута 0)
  - `tex_only` — сырое `texture(uTex, vUV)` без vCol/light/fog/alpha; magenta
    если `uUseTexture=0`. Проверяет реальный диффуз без интерференции остальных
    факторов.
  - `tex_lod0` — то же, но `textureLod(uTex, vUV, 0.0)` — обходит mipmap chain
    (если высокие LOD пусты, а base OK, tex_only даст серый, tex_lod0 — детали).

- **KEEP** (hardcoded, `#ifdef PLATFORM_LINUX`, `XREngine.cpp:~1044`,
  `CXR_ViewContextImpl::Clear`) — сразу после `InverseOrthogonal(m_W2VMat)`
  негируется view‑space X‑колонка (и та же операция для `m_dW2VMat`). Правит
  и **CPU‑culling** (BSP portal, frustum, m_bIsMirrored), и **GPU render**
  одной точкой. Заменяет старый shader‑side `MIRROR_X`. Компенсация winding —
  `glFrontFace(GL_CW)` в GLES3 backend. Root cause: LH‑конвенция
  _CameraWMat (PS3) vs RH GL. Проверено 2026-07-24 — движение камеры,
  ориентация модели и culling сходятся.

- **DBG** `RIDDICK_NO_LIGHT=1` (`PushLightUniforms`) — форсит `uLightingMode=0`
  для всех draws. Диагностика «уровень чёрный»: если под этим видим
  диффуз — наша modulate-lighting перебарщивает (ambient=0 → `c *= vec3(0)`).

- **DBG** `RIDDICK_NO_MIPMAP=1` (`GLES3_Texture.cpp`) — форсит
  `GL_TEXTURE_MIN_FILTER=GL_LINEAR` (без mipmap sampling) во всех аплоадах.
  Диагностика «хром на стенах»: если под этим стены оказываются с
  видимой текстурой — `glGenerateMipmap` тихо провалил цепочку и sampler
  ловит incomplete texture. Также лог `[GLES3-TEX] glGenerateMipmap failed err=...`.

- **DBG** `RIDDICK_DUMP_GL_TEX=<W>` (`GLES3_Texture.cpp`) — при аплоаде
  первых 8 текстур ширины `W` (или любых, если `=0`) дампит *post-swizzle*
  CPU‑буфер в `/tmp/openriddick_tex_<W>x<H>_<n>.ppm`. Проверяет, что
  реально уходит в `glTexImage2D`.

- **DBG** F10 (`DbgDumpTick`, edge-detect) — армит два лога на следующие
  32 draw'а: `[VBB] ...` (регистр-wiring через `BuildVertsFromVBB`) и
  `[DRAW] nInd pCurAttrib tex=[..] Flags` (реальный `m_pCurAttrib` в
  `DrawIndexed` для world-sized `_nInd>=300`). Ловит игру в нужный момент
  (не меню/загрузка). Аналогично F9 (frame dump / MTX log rearm).

- **DBG** `RIDDICK_DBG_RTT=1` (`RTTOverlay.InitFromEnv()`) — рисует
  thumbnails всех активных RTT-текстур в углу экрана.

- **DBG** `RIDDICK_DBG_MTX=1` (`~2094, ~2360`) — на F9 сбрасывает счётчик
  и логирует Model/Proj/MVP + vertex[0]→NDC для следующих 10 non-UI world
  drawcall'ов. Используется для проверки матриц.

- **DBG** `RIDDICK_DUMP_FRAME=N` (`~741`) — в кадре N дампит все draws
  в `/tmp/openriddick_frame.txt`.

- **DBG** `RIDDICK_DUMP_OBJ=<dir>` (`~757`) — дампит каждый уникальный
  меш в OBJ файлы для просмотра в Blender.

- **DBG** `RIDDICK_OBJ_APPLY_MVP=1` (`DumpGeomOBJ`) — вершины в OBJ
  трансформируются MVP + деление на w, показывает что видит GL.

- **DBG** `RIDDICK_DUMP_BSP=<dir>` (`WBSP2Loader.cpp:1216`) — при загрузке
  BSP2-карты дампит raw m_lVertices + m_lFaces в OBJ. Отвязано от рендера.
  Верифицирует loader.

### Debug-оверрайды GL-стейта

- **DBG** `RIDDICK_NO_DEPTH=1` (`~742, ~1443`) — принудительно выключает depth-test/write.
- **DBG** `RIDDICK_NO_CULL=1` (`~743, ~1431`) — принудительно выключает face-culling.
- **DBG** `RIDDICK_NO_BLEND=1` (`~744, ~1450`) — принудительно выключает blend.
- **DBG** `RIDDICK_NO_ALPHA=1` (`~745`) — выключает alpha-test в шейдере.
- **DBG** `RIDDICK_FORCE_WIRE=1` (`~746`) — вместо треугольников рисует
  GL_LINE_STRIP.
- **DBG** `RIDDICK_TEST_TRI=1|2` (`DrawIndexed`) — вместо реальной геометрии
  рисует RGB-тестовый треугольник. Mode 1 = identity MVP, mode 2 = engine
  MVP. Bisect диагностика: pipeline vs data.

- ~~**DBG** `RIDDICK_MIRROR_X`~~ — удалён 2026-07-24, заменён на unconditional
  W2V.X negate под `PLATFORM_LINUX` (см. запись выше).

### Skip-фильтры (для изоляции проблем)

- **HACK** `RIDDICK_SKIP_SKINNED=1` (`BuildVertsFromVBB`, `BuildInterleavedVerts`)
  — скипает draws с CRC_VREG_MI0/MW0/MI1/MW1 или m_Geom.m_pMI/pMW/nMWComp.
  Character-меши не рендерятся т.к. skinning pipeline не реализован.
  → **надо реализовать skinning** и удалить.

- **HACK** `RIDDICK_SKIP_CHARS=1`, `RIDDICK_SKIP_PROPS=1`,
  `RIDDICK_SKIP_SPRITES=1`, `RIDDICK_SKIP_SPOTVOL=1` (`XREngine.cpp:RenderModel`) —
  выключают классы моделей на уровне engine (через TDynamicCast).
  Изоляционная диагностика для сужения источника артефактов.
  → удалить когда все классы рендерятся правильно.

- **HACK** `RIDDICK_SKIP_SKY=1` (`XREngine.cpp:Engine_RVC_RenderSky`) —
  выключает skybox render.
- **HACK** `RIDDICK_SKIP_PARTICLES=1` (`XRPContainer.cpp`) —
  гейтит `CXR_ParticleContainer::OnRender`.

- **HACK** `RIDDICK_ONLY_BSP=1` (`DrawIndexed`, `DrawUserVerts`) — рисует
  только draws с nV >= 100 (BSP2 world clusters). Всё остальное режется.
  Изоляционная диагностика.
  → удалить после стабилизации.

- **HACK** `RIDDICK_DIRECT_RENDER=1` (`RenderTarget_SetRenderTarget`,
  `RenderTarget_CopyToTexture`, `DrawIndexed`) — bypass'ит RTT-плюминг:
  все draws в screen FBO, CopyToTexture — no-op, draws с RTT-slot-текстурой
  на входе (post-process, envmap, deferred resolve) скипаются.
  Изоляционная диагностика. → удалить или сделать permanent когда
  RTT-плюминг зафиксирован.

- **DBG** `RIDDICK_COPYTEX_FLIP=0|1` (`RenderTarget_CopyToTexture`) — A/B
  переключение Y-flip'а при копировании backbuffer → RTT. Legacy debug.

### Экранное отображение

- **KEEP** `RIDDICK_ROTATE=0|90|180|270` — поворот финальной картинки в окне.
- **KEEP** `RIDDICK_WINSIZE=WxH` — физический размер окна.
- **KEEP** `RIDDICK_FBOSIZE=WxH` — логическое разрешение движка.
- **DBG** `RIDDICK_ASSERT_FATAL=0` (env, читается в CSystemLinux) — не
  падать на assert'ах.

---

## Костыли внутри рендера

### Placeholder textures

- **HACK** `GetPlaceholderTex()` (`~380`) — magenta 1×1 текстура,
  выдаётся когда реальная не загрузилась. Не логгируется как ошибка
  → тексты меню/иконки могут молча стать magenta.
  → правильно: логировать все failed uploads с деталями.

### RTT clear-to-black

- **HACK** `EnsureFBOFor` (`~460-478`) — сразу после создания RTT FBO
  делаем `glClear` в чёрный + verify через `glReadPixels`. Раньше
  недоинициализированные RTT-текстуры давали белый мусор на Mesa.
  → verify readback можно удалить, clear-to-black оставить.

### NDC.z remap в шейдере

- **KEEP** `gl_Position.z = 2.0 * gl_Position.z - gl_Position.w`
  (`kGLES3_UIVertSrc`, `~57`) — компенсация engine [0..1] NDC.z vs
  GL [-1..+1]. **Оставить** — правильный фикс.

### Winding fix (final, повторно проверен пользователем)

- **KEEP** `glFrontFace(GL_CCW); glCullFace(CULLCW ? GL_BACK : GL_FRONT)`
  (`~1440-1442`) — правильный retail-mapping (c9d6e2d). Мой предыдущий
  вариант (`CULLCW ? GL_FRONT : GL_BACK`) был поведенческий no-op
  относительно оригинала — резал те же треугольники. Пользователь сверил
  вручную по RndrGL:77659-77666 и подтвердил обратный mapping.
  Оставить.

### Wrap mode = REPEAT для всех текстур

- **KEEP** `GLES3_Texture.cpp:275-276` — GL_REPEAT (было CLAMP_TO_EDGE)
  для uncompressed upload path. Мировые wall textures используют UV
  вне [0..1] для тайлинга (Aguerra06 V ≈ 7.5); CLAMP давал плоскую
  edge-row вместо тайла. Compressed-DXT путь (:179) уже юзал REPEAT.
  Согласовано.

### VBB transform

- **KEEP** `CRC_VRegTransform` scale+offset (`BuildVertsFromVBB`, `~2290+`)
  — packed вершины (I16/NS/NU) хранят raw*Scale+Offset. Правильный фикс.
  Оставить.

### Fallback в BuildInterleavedVerts

- **HACK** `BuildInterleavedVerts` (`~1560`) — если m_GeomVBID != 0,
  дёргаем VB_Get каждый draw. Медленно (стриминг вершин каждый кадр
  через malloc+free+push). **Заменить на GPU-side VBO cache**.
  → удалить malloc-путь после кэширования VB на GPU.

### Diagnostic per-draw dumps

- **DBG** `DbgDumpDraw` (`~2115`), `DumpGeomOBJ` (`~2170`),
  `DbgDumpTick` (`~880`) — вся инфраструктура F9 frame-dump'а +
  RTT-verify логи. Оставить пока рендер нестабилен, потом удалить.

### Mouse pitch — убрана PS3-негация Y

- **KEEP** `WClientMod.cpp` — на не-Win ветке убрана автоматическая
  инверсия Y-mouse. Теперь инверсия только под `CONTROLLER_INVERTYAXIS=1`,
  как в PC-retail (c9d6e2d).

### Guard в WClient_Core

- **KEEP** `WClient_Core.cpp` — проверка pObj != 0 после iterator
  advance, потому что объект может удалить сам себя в OnClientRefresh
  (SIGSEGV на Pa1_Pit) (c9d6e2d).

### BSP2 PVS диагностика

- **DBG** `WBSP2Loader.cpp` — разовый log `[BSP2] PVS entries: N`. Если
  N=0, движок рисует ВСЕ листы (тормоза при вращении камеры).

### VPU sync fallback

- **HACK** `MRTC_VPUManager.cpp:153-170` — при переполненной очереди
  выполняем job'у синхронно вместо M_BREAKPOINT (SIGILL на Linux).
  → удалить когда реализуем настоящий async VPU worker pool.

### 32-битный ChildNodeStart в CRegistry_Compiled

- **HACK** `MRegistry_Compiled.cpp` — расширил 16-битный ChildNodeStart
  до 32 бит (для Pa1_TheDream registry который > 65536 узлов).
  Не совместимо с оригинальным layout'ом.
  → правильно: тот же on-disk формат, только внутренняя память шире.

### Игнорируемые FP20-эффекты

- **HACK** GLES3 бэкенд игнорирует `CXR_VBOperator_*` warnings
  (fresnelgenenv, NMRimLight, Electric, RGBToGrayscale, ghostdrone,
  fp20_cubewater). Просто дропаются с warning'ом в console.
  → правильно: реализовать эквиваленты в GLSL ES.

### Игнорируемый light-pass (Phase A)

- **HACK** `Attrib_Lights` override + Lambert в шейдере — включаются
  только при `CRC_FLAGS_LIGHTING` или ONE/ONE blend. Реальный PC-путь
  использует FP20 ext-attributes которые мы полностью игнорим.
  → Phase B: реализовать FP20 additive light passes через
  CRC_ExtAttributes_FragmentProgram20.

### Skinning не реализован

- **HACK** `BuildVertsFromVBB` возвращает NULL если m_lpVReg[MI0/MW0] есть
  (когда RIDDICK_SKIP_SKINNED=1). Иначе character-меши рендерятся с
  bone-local позициями = «шипы».
  → реализовать vertex skinning с matrix palette.

### Missing user clip planes

- **HACK** Ничего не делаем с CRC_FLAGS_CLIP / Clip_Set (Research §7-H1).
  Portal/mirror sub-views рисуются не clipped → могут заливать всё в
  своей RTT-текстуре. Основной кадр не страдает, но portal/mirror
  контент кривой.
  → реализовать через `gl_ClipDistance` в шейдере.

---

## Инфраструктура которую можно оставить

- **KEEP** `MRTC_System_Linux.cpp` — POSIX-порт core-системы.
- **KEEP** `MSystem_Linux.cpp`, `MMain_Linux.cpp` — SDL2 главный цикл.
- **KEEP** `MDisplaySDL2.cpp` окружение — CDisplayContextSDL2,
  CInputContext_SDL2, CRC_GLES3 базовые методы.
- **KEEP** `Sound/SDL2/MSound_SDL2` — audio backend.
- **KEEP** RTT-composite шейдер + rotate/scale/present инфраструктура.

---

## Очистка перед мержем

Порядок:

1. Стабилизировать рендер (winding, portal clip, skinning, FP20).
2. Убрать все `HACK` из списка выше.
3. Убрать все `DBG` env vars, оставив 2-3 самых полезных под #ifdef DEBUG.
4. Удалить diagnostic-код (`DbgDumpDraw`, `DumpGeomOBJ`, `DbgDumpTick`,
   MTX-логи).
5. Заменить `BuildInterleavedVerts` malloc-путь на VBO cache.
6. Удалить `RIDDICK_COPYTEX_FLIP` A/B (закрепить рабочий вариант).
7. Проверить `Placeholder` — либо loud-fail либо графическая индикация.

## Меньшие детали

- `SUIVert` расширен до 44 байт (позиция, UV, UV1, col, normal). При
  вводе GPU-side VBO cache — нужно тщательно ре-разметить.
- Placeholder texture создаётся раз на процесс, никогда не пересоздаётся.
- Screen FBO зелёный debug-clear можно вернуть в чёрный.
- `sFlip = -1` static'и во многих местах — не сбрасываются между
  сессиями (не важно в single-process, но грязно).
