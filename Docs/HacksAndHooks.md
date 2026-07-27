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
  яркий magenta/cyan checkerboard 32x32 на unit 0 для solid‑world draw'ов,
  отключает ch1/lighting/alpha-test/dbg-mode. **Skip'ит:** 2D UI (чтобы
  HUD оставался читаемым) и BLEND‑проходы (пыль/спрайты/декали, e.g. пылевые
  облака в TheDream — они alpha‑blended на весь экран и без skip'а полностью
  перекрывали геометрию).

- **DBG** `RIDDICK_DBG_SHADER=uv|normal|worldpos` (`MDisplaySDL2.cpp:~1245`)
  — debug-режимы **только 3D-программы** (`kGLES3_3DFragSrc`, свой enum):
  - `uv` — `fract(vUV)` как RG (fract, чтобы тайлинг читался)
  - `normal` — world-normal `(N+1)*0.5`
  - `worldpos` — `fract(worldPos*0.01)` как RGB (повтор каждые 100 юнитов)

  ИЗМЕНЕНО 2026-07-25: раньше эта переменная управляла единым
  шейдером-всё-в-одном (enum 1..8). После разделения на две программы
  она отвечает только за 3D. Старые значения (`pos`, `no_tex`, `nrm_raw`,
  `pos_local`, `tex_only`, `tex_lod0`) живут в legacy-UI-шейдере под `#if 0`
  и в новом UI-шейдере не поддерживаются.

- **DBG** `RIDDICK_DBG_SHADERUI=uv|pos` (`MDisplaySDL2.cpp:~1230`) —
  debug-режимы **только UI-программы** (новый минимальный
  `kGLES3_UIFragSrc`): `uv` — `fract(vUV)` как RG, `pos` — сплошной
  красный. Остальные значения старого enum парсятся, но новым шейдером
  игнорируются (нет соответствующих веток).

- **DBG** `RIDDICK_FORCE_3D_SHADER=1` (`IsUIDraw`, `~2301`) — гоняет ВСЕ
  draw calls (включая UI) через 3D-программу. Для изолированной проверки
  3D-шейдера.

- **DBG** `RIDDICK_DBG_VP=1` (`Viewport_Update`, `~1968`) — лог `[VP]` на
  каждую смену вьюпорта: rect, сигнатура проекции (z→w, constW), состояние
  UI-хинта и итоговая классификация (`UI shader|3D shader`).

- **DBG** `RIDDICK_DBG_CLASSIFY=N` (`SetupCommonUniforms`, `~2323`) — лог
  `[CLS]` для первых N draw'ов: UI-хинт, тест проекции, класс модельной
  матрицы (ident/2dpat/3d), k00/k32, ZCmp/ZW/Blend, Tex0 и выбранная
  программа. Инструмент для отлова мисроутов UI↔3D по реальным данным.

- **DBG** `RIDDICK_AMBIENT_FLOOR=<f>` (дефолт 0, `SetupCommonUniforms`,
  `~2396`) — нижний «пол» для запечённого в вершины ambient (vCol) в
  3D-шейдере: `bake = max(vCol.rgb, uAmbientFloor)`. Карты с чёрной
  запечкой (Pit) без него рисуются чёрным — старый шейдер-всё-в-одном
  маскировал это динамическим светом + полом 0.2. `=0.2` воспроизводит
  старый пол без всякой обработки источников света.

- **KEEP** (hardcoded, `#ifdef PLATFORM_LINUX`, `XREngine.cpp:~1044`,
  `CXR_ViewContextImpl::Clear`) — сразу после `InverseOrthogonal(m_W2VMat)`
  негируется view‑space X‑колонка (и та же операция для `m_dW2VMat`). Правит
  и **CPU‑culling** (BSP portal, frustum, m_bIsMirrored), и **GPU render**
  одной точкой. Заменяет старый shader‑side `MIRROR_X`. Компенсация winding —
  `glFrontFace(GL_CW)` в GLES3 backend. Root cause: LH‑конвенция
  _CameraWMat (PS3) vs RH GL. Проверено 2026-07-24 — движение камеры,
  ориентация модели и culling сходятся.

- **HACK** DIRECT_RENDER 3D‑фильтр (`DrawIndexed`): под `RIDDICK_DIRECT_RENDER=1`
  скипает 3D‑дроу, где нет **и** COLORWRITE, **и** ZWRITE одновременно.
  Z‑only prepass'ы (для deferred/G-buffer) не нужны в single-pass mode.
  Alpha‑blend overlay'ы (без ZWRITE, e.g. BSP2 detail‑decals) — тоже,
  чтобы избежать двойного paint'а и z‑fight'а. UI обходит фильтр через
  проверку 2D-model matrix (диагональная, `k[2][2]==1`). Убрать когда
  вернём FBO с deferred pipeline.

- **DBG** `RIDDICK_NO_LIGHT=1` (`GLES3_NoLight()` — четыре точки:
  `BuildVertsFromVBB`, `BuildInterleavedVerts`, `SetupCommonUniforms`,
  `PushLightUniforms`) — **fullbright для 3D‑мира**: `vCol=white`
  (стирает vertex-baked ambient, из-за которого Pit — чёрный),
  `uFogEnable=0`, `uLightingMode=0`. Фрагмент коллапсирует в
  `c = texture(uTex,vUV)`. **UI не трогается** — authored per-vertex цвета
  (жёлтые надписи диалогов, полоса загрузки, ESRB) сохраняются.
  ОБНОВЛЕНО 2026-07-25: (а) дискриминатор 2D — теперь `ClassifyUI()`
  (engine-хинт `Render_SetUIPass` + эвристики, см. ниже), старый тест
  по model-matrix заменён; (б) в 3D-шейдере добавлен uniform `uNoLight`
  — шейдер сам игнорирует vCol независимо от host-side отбеливания
  (belt-and-braces).

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

- **DBG** `RIDDICK_NO_VBCACHE=1` (`GLES3_NoVBCache()`, `MDisplaySDL2.cpp`)
  — полностью выключает GPU-резидентный кэш геометрии по VBID
  (`GLES3_Geometry.h/.cpp`, `CGLES3GeometryCache`) и возвращает старый
  путь: `VB_Get` + скалярная конвертация через `BuildVertsFromVBB` +
  ре-аплоад через стриминг-VBO/IBO (`m_Streamer`) на КАЖДЫЙ draw.
  Держать как fallback для A/B-сравнения, пока кэш не обкатан на всех
  типах геометрии (skinned-меши и экзотические `CRC_RIP_*` в кэш не
  идут вообще — `m_bSkip`, всегда падают на старый путь независимо от
  этого флага). `[GL-DBG]`-строка печатает `vbCache{cached=.. streamed=.. built=.. bytesV=.. bytesI=..}`
  чтобы видеть долю кэшированных draw'ов и суммарный размер GPU-резидентных буферов.
  → удалить когда кэш подтверждён идентичным старому пути на всех картах.
  ОБНОВЛЕНО 2026-07-27: флаг гасит **оба** быстрых пути в `DrawIndexed`
  (VBID-кэш и мемо CPU-геометрии), т.е. это единый A/B-переключатель.

### Экранное отображение

- **KEEP** `RIDDICK_ROTATE=0|90|180|270` — поворот финальной картинки в окне.
- **KEEP** `RIDDICK_WINSIZE=WxH` — физический размер окна.
- **KEEP** `RIDDICK_FBOSIZE=WxH` — логическое разрешение движка.
- **DBG** `RIDDICK_ASSERT_FATAL=0` (env, читается в CSystemLinux) — не
  падать на assert'ах.

---

## Костыли внутри рендера

### Разделение UI/3D шейдеров (2026-07-25)

- **KEEP** Две шейдерные программы вместо одной «всё-в-одном»
  (`MDisplaySDL2.cpp`): `m_UIShader` (новый минимальный `kGLES3_UIVertSrc`
  + `kGLES3_UIFragSrc`: `vCol × texture`, debug `uv|pos`) для UI/2D и
  `m_3DShader` (`kGLES3_3DVertSrc/FragSrc`: `vCol × diffuse`, debug
  `uv|normal|worldpos`, `uNoLight`, `uAmbientFloor`) для мировой
  геометрии. Выбор — per-draw в `SetupCommonUniforms` через `IsUIDraw()`.
  Общий vertex layout (SUIVert, локации 0-4) — `SetVertexAttribPointers`
  общий. Заодно починен баг: `InitGLResources` раньше вызывал
  `m_UIShader.Build()` дважды, и второй вызов через `Destroy()` убивал
  UI-программу.

- **DBG** Legacy-UI-фрагментник сохранён под `#if 0` как
  `kGLES3_UIFragSrc_Legacy` (`~89-175`) — для A/B-сравнения (свет, туман,
  alpha-test, UV1, 8 debug-режимов). **Удалить перед мержем.**

- **KEEP** Явный канал «сейчас рисуется UI»: виртуал
  `CRenderContext::Render_SetUIPass(bint)` (`MRender.h`), no-op тело
  `CRC_Core::Render_SetUIPass` (`MRender.cpp`), override в `CRC_GLES3`
  (флаг `m_bUIPass`, `~1909`). Движок выставляет в
  `CWFrontEnd::OnRender` (`WFrontEnd.cpp`) — `_pRC->Render_SetUIPass(true/false)`
  вокруг рендера интерфейса. ВНИМАНИЕ: виртуал меняет vtable
  `CRenderContext` — после правки заголовков обязательна полная
  пересборка всех модулей. TODO: добавить такую же скобку в точке
  рендера in-game HUD, если он идёт мимо `WFrontEnd::OnRender`.

- **HACK** Эвристический дискриминатор UI (`IsUI2DDraw`, `~2279`) —
  фолбэк для UI, идущего вне hinted-скобки (и для отложенных VBM-флашей).
  Три сигнала, любого достаточно: (а) ортографическая/2D проекция
  вьюпорта (`Is2DProjection`: z→w == 0, constW == 1); (б) model-matrix с
  паттерном `CRC_Viewport::Get2DMatrix` (диагональ, `k[2][2]==1`,
  НЕ identity — identity это мировая BSP с world-space вершинами);
  (в) identity model + `ZCOMPARE` и `ZWRITE` оба выключены (UI со
  шрифтовыми квадами в пиксельных координатах, пре-трансформированными
  на CPU; фронтенд явно гасит ZCOMPARE). `ClassifyUI()` = hint || эвристика.
  → правильно: покрыть хинтом ВСЕ точки входа UI (HUD, VBM-flush) и
  удалить эвристики.

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
  (`kGLES3_UIVertSrc`, `~57`; также `kGLES3_3DVertSrc`, `~210`) —
  компенсация engine [0..1] NDC.z vs GL [-1..+1]. Есть в обоих
  вершинных шейдерах. **Оставить** — правильный фикс.

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
- **KEEP** В GPU-кэше (`GLES3_Geometry.cpp`, `CGLES3GeometryCache::Build`)
  скейл/оффсет применять не нужно отдельно: destination-формат всегда
  F32, а движковый `ConvertToInterleaved` сам применяет source
  scale/offset при конвертации packed → float (см. `MRender.cpp:2210`).
  Поэтому `DestTransformEnable=0` и `DstScale` (Scale=1/Offset=0) в
  кэш-пути фактически no-op — это ожидаемо, а не баг.

### Fallback в BuildInterleavedVerts

- **HACK** `BuildInterleavedVerts` (`~1560`) — если m_GeomVBID != 0,
  дёргаем VB_Get каждый draw. Медленно (стриминг вершин каждый кадр
  через malloc+free+push).
  **2026-07-27: реализован GPU-side VBO/IBO кэш по VBID**
  (`GLES3_Geometry.h/.cpp`, `CGLES3GeometryCache`, интеграция в
  `Render_VertexBuffer`/`Render_VertexBuffer_IndexBufferTriangles`/
  `Geometry_Precache`/`Geometry_PrecacheFlush` в `MDisplaySDL2.cpp`).
  Кэш строит interleaved VBO + uint16 IBO один раз через engine'ский
  `CRC_BuildVertexBuffer::ConvertToInterleaved` (не наш скалярный
  `VRegFetch`) и переиспользует GL-буферы, пока движок не сбросит бит 0
  `CRC_VBIDInfo::m_Fresh` (`CXR_VBContext::VB_MakeDirty`). `BuildVertsFromVBB`
  / `BuildInterleavedVerts` остаются как fallback-путь: skinned-меши,
  `CRC_RIP_WIRES` и всё, что кэш не смог собрать (или что явно
  выключено через `RIDDICK_NO_VBCACHE=1`), по-прежнему идёт через
  malloc+VRegFetch+`m_Streamer` каждый кадр, без изменений в этой
  логике.
  → malloc-путь для остального (skinning, wires) убрать после
  реализации matrix-palette skinning в кэше.

### Горячий путь DrawIndexed (2026-07-27)

Профиль на Pa1_Arrival показал ~**3 млн конвертаций вершин на кадр** при
всего ~81 тыс. нарисованных индексов. Причина: движок ставит геометрию
один раз (`Geometry_VertexBuffer` -> `m_Geom` или `m_GeomVBID`), а затем
шлёт поток примитивов, и КАЖДЫЙ `DrawIndexed` заново пересобирал весь
вершинный массив кластера (в VBID-ветке — ещё и `VB_Get` + `malloc`).
Счётчики `Render_VertexBuffer*` при этом были нулевые (`VBID=0`) — кэш из
первого коммита к игровому кадру просто не подключался.

Три правки в `MDisplaySDL2.cpp`:

- **KEEP** Fast path A: при `m_GeomVBID != 0` дроу идёт через
  `CGLES3GeometryCache::Ensure` — биндится постоянный VBO, стримятся
  только индексы. Вершинной работы ноль.
- **KEEP** Fast path B (мемо `m_GeomMemo`): для CPU-геометрии (`m_Geom`)
  результат конвертации и его смещение в стриминг-VBO переиспользуются
  следующими дроями. Инвалидация: (а) оверрайды `Geometry_VertexBuffer`
  (обе перегрузки) и `Geometry_Clear` — авторитетный сигнал «данные
  сменились», нужен потому что VBM-скретч может отдать тот же адрес под
  другое содержимое; (б) счётчик поколений вершинного кольца
  (`CGLES3VBOStreamer::GetVBGeneration`, инкремент при orphan);
  (в) смена UV-сетов / флага fullbright-отбеливания.
  **Известный риск:** если движок правит вершины ПО МЕСТУ, не вызывая
  сеттер геометрии, мемо этого не увидит. Симптом — «замерзшая»
  анимированная геометрия; проверка — `RIDDICK_NO_VBCACHE=1`.
- **KEEP** `Render_IndexedPrimitives` сворачивает весь поток примитивов в
  ОДИН `GL_TRIANGLES`-дроу через `CRC_Core::Geometry_BuildTriangleListFromPrimitives*`
  (как PS3 делает на этапе Build), если все типы в потоке —
  TRIANGLES/TRISTRIP/TRIFAN. Иначе остаётся старый поэлементный цикл.

Общие фильтры дроу вынесены в `DrawIndexed_ShouldSkip(nVerts)`
(`RIDDICK_ONLY_BSP`, DIRECT_RENDER RTT-skip и `RIDDICK_DIRECT_PASS`),
чтобы все три пути применяли одинаковые правила.

**Известное ограничение:** `DbgDumpDraw`/`DumpGeomOBJ`/`RIDDICK_DBG_MTX`
в VBID-кэш-ветке не пишутся (CPU-вершин на руках нет) — для дампов
запускать с `RIDDICK_NO_VBCACHE=1`.

Диагностика: в `[GL-DBG]` добавлены `vconv` (реально сконвертировано
вершин за интервал) и `vmemo` (сколько конвертаций пропущено).

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
8. Удалить `kGLES3_UIFragSrc_Legacy` (`#if 0`-блок).
9. Покрыть `Render_SetUIPass`-скобками все точки входа UI (in-game HUD,
   отложенные VBM-флаши) и удалить эвристики `IsUI2DDraw`/`Is2DProjection`.

## Меньшие детали

- `SUIVert` расширен до 44 байт (позиция, UV, UV1, col, normal). При
  вводе GPU-side VBO cache — нужно тщательно ре-разметить.
- Placeholder texture создаётся раз на процесс, никогда не пересоздаётся.
- Screen FBO зелёный debug-clear можно вернуть в чёрный.
- `sFlip = -1` static'и во многих местах — не сбрасываются между
  сессиями (не важно в single-process, но грязно).
