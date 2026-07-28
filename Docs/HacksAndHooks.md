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

- **DBG/ЭКСПЕРИМЕНТАЛЬНЫЙ** `RIDDICK_FP20=<n>` (`GLES3_FP20Enabled` /
  `GLES3_FP20ModeOverride`, `MDisplaySDL2.cpp:~435-471`; caps в
  `CRC_GLES3::Create`, `~1731-1787`; лог в `SetupCommonUniforms` +
  `DbgLogFP20`, `~1377-1428`, `~2779-2796`) — оживляет шейдерную очередь
  движка (`CXR_Shader::PrepareFrame`, `XRShader.cpp:1258-1305`), которая
  сейчас мертва: без флага `ModesAvail == 0` (нет `CRC_CAPS_FLAGS_
  FRAGMENTPROGRAM20` и texture units < 8) → `m_ShaderMode == -1` → все
  `switch(m_ShaderMode)` в `RenderShading*` — no-op.

  Что делает флаг:
  1. Добавляет `CRC_CAPS_FLAGS_FRAGMENTPROGRAM20` к `m_Caps_Flags`,
     поднимает `m_Caps_nMultiTexture`/`m_Caps_nMultiTextureCoords` до 8
     (НЕ добавляет `MRT`/`FRAGMENTPROGRAM30`/`COPYDEPTH` — иначе движок
     полез бы в deferred-режимы с MRT G-буфером, которого у нас нет).
  2. Пишет `XR_SHADERMODE` в реестр окружения движка
     (`pSys->GetEnvironment()->SetValuei`), форсируя forward-режим
     `XR_SHADERMODE_FRAGMENTPROGRAM20` (=5, локальная константа
     `kXRShaderMode_FP20Forward` — enum не импортирован из `XRShader.h`
     в этот TU намеренно). Без этого AUTO-выбор взял бы старший бит
     `ModesAvail` и попал бы в `FRAGMENTPROGRAM20DEFMM` (deferred).
     `RIDDICK_FP20=1` → режим 5; `RIDDICK_FP20=<N>`, N>1 → кладёт N как
     есть (проба других `XR_SHADERMODE_*` без пересборки).
  3. При `RIDDICK_DBG_GL=1` логирует `[GLES3-FP] prog=... hash=... tex=[...]
     texgen=[...] flags=... blend=.../...` один раз на уникальный
     `m_ProgramNameHash` (кэш 64 записи) плюс первые 4 вектора параметров
     программы (`CRC_ExtAttributes_FragmentProgram20::m_pParams`) —
     собирает факты о том, что движок реально просит. Счётчик
     `m_DbgFPDraws` (draws с FP20 ext-attrib за интервал) добавлен в
     строку `[GL-DBG]` (`fp20=N`).

  **НИЧЕГО не рисуется по-новому** — сами FP20-программы не
  реализованы, draw идёт как раньше через `m_UIShader`/`m_3DShader`.

  **Побочный эффект, ожидаемый и важный**: как только `m_ShaderMode`
  перестаёт быть `-1`, автоматически **отключается** HACK-fallback
  `bNoShaderPipeline` в `WBSP2Model.cpp` (`~1973`, см. запись выше про
  BSP2) — тот, что сейчас подставляет диффуз-текстуру и включает
  color-write в Z-препассе взамен отсутствующего shading pipeline.
  С `RIDDICK_FP20` мировая геометрия временно станет **ещё темнее/более
  untextured**, чем сейчас — это ожидаемо на данном шаге (собираем
  список FP20-программ, не чиним рендер).

  По умолчанию (без `RIDDICK_FP20`) поведение не меняется байт-в-байт.
  → удалять/заменять реальной реализацией FP20 когда дойдём до
  соответствующей фазы (Phase B, см. "Игнорируемый light-pass" ниже).

- **KEEP** `RIDDICK_LFM=1` (`MDisplaySDL2.cpp`, `GLES3_NoLFM()`,
  `CRC_GLES3::TrySetupLFMProgram`) — **opt-in**, по умолчанию программа
  `XRShader_FP20_LFM` ВЫКЛЮЧЕНА. Причина (проверено на Pa1_Arrival
  2026-07-27, скриншоты): данные настоящие — кластеры BSP2 несут
  лайтмап-атласы (`LM0_0..LM0_3`, 512x64), отбор программы срабатывает
  (`[GLES3-LFM] uvset0=0 uvset1=3`), но у порта нет тангентного базиса,
  поэтому `n_ts` берётся прямо из нормал-мапы, а она сэмплится диффузным
  UV, который тайлится (у первой вершины стены `v = -7.375`). Веса
  базиса начинают меняться с частотой тайлинга диффуза, и поверхность
  покрывается высокочастотной рябью вместо мягкого запечённого света.
  Riddick освещается динамически (`Docs/FP_Reference.md` §4a), так что
  эта программа не на критическом пути — код остаётся для другого
  контента/движков, но по умолчанию не мешает.
  `RIDDICK_NO_LFM=1` продолжает работать как явное выключение.
  → включать после того, как будут проброшены тангенты.
- **DBG** `RIDDICK_DBG_LFM=uv|lm` (только вместе с `RIDDICK_LFM=1`) —
  `uv` рисует `fract(vUVLFM)` как RG (видно, вменяемый ли диапазон
  лайтмап-UV: плавный градиент на кластер против повторяющихся рамп
  тайлящегося диффузного UV), `lm` показывает восстановленный запечённый
  цвет без умножения на диффуз.
- **KEEP** `RIDDICK_LFM_SCALE=<f>` (default `4.0`, `GLES3_LFMScale()`) —
  множитель яркости запечённого света LFM-программы (`uLFMScale`).
  Заменяет непроброшенный `4.0 * lmIntensityScale * LFM_Scale.rgb`
  оригинала одним скаляром — см. TODO в `kGLES3_LFMFragSrc`.

- **KEEP** `RIDDICK_NDS=1` (`MDisplaySDL2.cpp`, `GLES3_NDSEnabled()`,
  `CRC_GLES3::TrySetupNDSProgram`/`TrySetupNDSPProgram`/`TrySetupLFProgram`)
  — **opt-in** (в отличие от LFM, по умолчанию ВЫКЛЮЧЕНО, не наоборот).
  Один флаг включает ВСЮ семью FP20 forward-программ: три однопроходных
  динамических источника света (diffuse + normal map + Phong-спекуляр):
  `XRShader_FP20_NDS` (свой GL-шейдер `m_NDSShader`) и `XRShader_FP20_NDSP`/
  `XRShader_FP20_NDSEATP` (общий GL-шейдер `m_NDSPShader` с проекционной
  картой(-ами), см. раздел ниже) — критический путь освещения Riddick
  (весь видимый динамический свет в игре — эти проходы); плюс
  `XRShader_FP20_LF` (`m_LFShader`, см. раздел ниже) — объектный ambient
  (свет там, куда ни один точечный источник не достаёт, иначе комнаты
  чёрные даже с рабочим NDS). NDS/NDSP требуют тангентного базиса (локации
  5/6, только кэш-путь геометрии) — см. раздел ниже; LF тангентов не
  требует вовсе (свой, объектный контракт атрибутов — см. раздел). Все
  четыре программы выключены по умолчанию одним и тем же флагом, пока не
  подтверждены на реальном прогоне с реальными нормал-мапами/лайтфилдами.
  `RIDDICK_NO_LF=1` — гасит только `XRShader_FP20_LF`, оставляя
  NDS/NDSP/LFM работать (см. `GLES3_NoLF()`); отдельного `RIDDICK_LF=1`
  переключателя нет — задача явно просит один общий флаг на всё семейство.
  → включать после проверки на реальной карте с материалами, у которых
  есть normal map/лайтфилд.
- **DBG** `RIDDICK_DBG_NDS=tslv|normal|diffuse|spec|atten|proj|lf` (только
  вместе с `RIDDICK_NDS=1`, общий для всех четырёх программ NDS/NDSP/
  NDSEATP/LF) —
  `tslv`: нормализованный tangent-space light vector как RGB; `normal`:
  декодированная нормаль из normal-мапы; `diffuse`: только диффузный
  член; `spec`: только спекулярный член; `atten`: затухание по расстоянию
  `(1 - saturate(distSq * uLightRange.z))^2` (до умножения на self-shadow)
  как оттенки серого — видно, добивает ли источник до поверхности вообще,
  без гадания по итоговому освещённому результату; `proj` (только
  NDSP/NDSEATP): комбинированный множитель проекционной карты(-т) один,
  до умножения в attn — изолирует форму «печенья»/cookie от затухания по
  расстоянию; `lf` (только LF): вклад ambient-cube один, без умножения на
  диффузную текстуру/цвет материала — та же идея изоляции терма, что и
  `atten`/`proj`, но для шестой (объектно-амбиентной) программы. Без
  debug-режимов отладка нового шейдера "вслепую" почти невозможна.

- **DBG** `RIDDICK_ZEQ_LEQUAL=1` (`GLES3_ZEqualToLEqual()`, применяется в
  `ApplyAttribs`) — заменяет `ZCompare EQUAL` на `LESSEQUAL` для всех
  проходов. FP20-проходы света/лайтмапы рисуются аддитивно поверх
  глубины, записанной более ранним проходом, и требуют ТОЧНОГО равенства
  глубин. У нас два пути подачи вершин (GPU-кэш геометрии и старый
  скалярный стример) могут дать побитово разные позиции для одной и той
  же поверхности — тогда light-проход отваливается целиком. Это ровно
  то, что видно на скриншотах: освещение сработало лишь на части
  полигонов. Флаг — диагностический: если под ним мир освещается, то
  настоящий фикс — привести оба пути к побитово одинаковым позициям.

- **KEEP** двухканальные нормал-мапы (`uNormalTwoCh` в NDS/LFM-программах,
  `TextureID_IsTwoChannel`, таблица `m_lTexFmt`) — нормал-мапы игры лежат
  в `IMAGE_FORMAT_I8A8` (`MImage.h:63`), это 43 из 44 на Pa1_Arrival: то
  есть раскладка 3DC/BC5, где хранятся X и Y, а Z восстанавливается.
  Наш аплоадер отдаёт их как `GL_RG8` со свизлом (R,R,R,G), поэтому X
  приходит в `.r`, Y в `.a`. Прежний декод `texture(...).xyz * 2 - 1`
  давал вектор `(i,i,i)`, вырождающийся в ноль при i≈0.5, а `normalize`
  усиливал шум — отсюда дизерная «грязь» на скриншотах 2026-07-27.
  Формат запоминается на аплоаде и подаётся в шейдер юниформом.

- **KEEP** клампинг UV для `CRC_TEXGENMODE_LINEAR` (вершинные шейдеры UI и
  3D) — таблица тумана `Special_DepthFogTable` это текстура **8x1**, а
  генерируемая координата `u = (depth - FogStart)/(FogEnd - FogStart)`
  уходит в минус ближе FogStart и за единицу дальше FogEnd. Наш аплоадер
  биндит ВСЕ текстуры с `GL_REPEAT`, поэтому такие значения заворачивались:
  прямо перед камерой получался полный туман, а вдали — никакого, то есть
  туман работал наоборот (наблюдалось 2026-07-27: ближние поверхности
  залиты серым, даль чёрная, рельеф от нормал-мап пропадал в ближнем
  радиусе). Кламп в шейдере — эквивалент `CLAMP_TO_EDGE` на конкретную
  выборку, не трогающий общий стейт сэмплера, от которого зависят
  тайлящиеся мировые текстуры.
  → правильный фикс в будущем: пробрасывать per-texture wrap из движкового
  `m_TexSamplerMode`, тогда кламп можно убрать.

- **KEEP** 1D-LUT текстуры без мипов и с CLAMP (`GLES3_Texture.cpp`,
  условие `W <= 1 || H <= 1`) — таблица тумана `Special_DepthFogTable`
  имеет размер 8x1. Выборка из рампы пробегает весь диапазон 0..1 по
  поверхности, то есть производная UV огромная, и сэмплер сваливается на
  грубый мип — а для текстуры 8x1 это усреднение всей рампы в одно
  плоское значение. На экране это читается как равномерная белёсая
  засветка независимо от расстояния (наблюдалось на TheDream 2026-07-27:
  дверь пропадает на средней дистанции и белеет вблизи). Для LUT нужен
  ровно `GL_LINEAR` без мипов и `CLAMP_TO_EDGE`; общий `GL_REPEAT`,
  нужный тайлящимся мировым текстурам, на такие текстуры больше не
  накладывается.

- **DBG** `RIDDICK_DBG_SHADER=foguv` — изолирует проходы с
  `CRC_TEXGENMODE_LINEAR` (на практике — depth-fog): такие дрои
  закрашиваются сгенерированной координатой рампы в красный канал,
  остальные рисуются как обычно. Позволяет прочитать плотность тумана
  прямо с экрана, вместо того чтобы угадывать её по композиту.

- **KEEP/HACK** `RIDDICK_ALPHA_REF=<f>` (дефолт `0.5`, `TrySetupNDSPProgram`)
  — принудительный alpha-test для варианта `XRShader_FP20_NDSEATP`.
  Буква **A** в имени программы это AlphaTest, но движок его в атрибут НЕ
  кладёт: базовый FP20-атрибут (`XRShader_FP20.cpp:100-120`) не вызывает
  `Attrib_AlphaCompare` вообще, поэтому `m_AlphaCompare` остаётся `ALWAYS`
  и общий путь тест выключает. Вырез — работа самой программы (в оригинале
  это делал бы `KIL` в ARB-ассемблере). Форсим `GREATEREQUAL` по альфе
  диффузной текстуры; порог вынесен в env, потому что оригинальной
  константы в данных нет — файла `XRShader_FP20_NDSEATP.fp` не существует.
  → уточнить порог, когда/если найдётся эталон.

- **DBG** `RIDDICK_NO_FOG=1` (`DrawIndexed_ShouldSkip`) — полностью
  выбрасывает проходы глубинного тумана. Они alpha-blend'ятся поверх всего
  в конце кадра и, пока ведут себя неправильно, забеливают картинку так,
  что любая другая визуальная проверка становится нечитаемой: белёсую
  дымку легко принять за результат светового прохода (я на этом ошибся
  при чтении скриншота `RIDDICK_DBG_NDS=atten`, 2026-07-27). Признак
  fog-прохода однозначный: texgen-режим `CRC_TEXGENMODE_LINEAR` на
  texcoord-канале 0 — его выставляет единственное место в движке
  (`WBSP2Model.cpp:1927-1929`), больше в кадре его никто не использует.
  → держать до тех пор, пока туман не станет корректным; для проверок
  освещения запускать всегда с ним.

- **DBG** `RIDDICK_NO_SCISSOR=1` (`ApplyAttribs`) — игнорировать
  движковый scissor-прямоугольник. Световые проходы несут
  `CXR_VBFLAGS_LIGHTSCISSOR`: движок обрезает каждый аддитивный проход
  экранным bounding box'ом источника — это оптимизация fill rate, и при
  ПРАВИЛЬНОМ прямоугольнике она не может убрать ни одного видимого
  пикселя света. Если же прямоугольник посчитан не в том пространстве
  (viewport-relative против target-absolute, либо не та высота для
  Y-флипа), свет обрезается жёсткой прямоугольной границей, которая ездит
  вместе с камерой — ровно это видно на скриншоте `atten` от 2026-07-27:
  белое пятно на воротах обрывается вертикальной линией.
  Флаг диагностический: если под ним свет разливается по всей сцене —
  причина именно в вычислении rect'а, и чинить надо его, а не свет.

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

- **DBG** `RIDDICK_NO_TEXGEN=1` (`GLES3_NoTexGen()`, `PushTexGenUniforms`,
  `MDisplaySDL2.cpp`) — форсит `uTexGenMode0/1=0` на всех draw'ах, т.е.
  UV всегда берётся из вершинного регистра (поведение до реализации
  TexGen). A/B-проверка, что реализация TexGen ничего не сломала в
  UI/спрайтах/декалях/мировой геометрии. См. раздел «TexGen» ниже.

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
  ОБНОВЛЕНО 2026-07-28: alpha-test (`uAlphaFunc`/`uAlphaRef`, тот же
  `CRC_COMPARE_*`-код что и раньше) теперь реализован во ВСЕХ четырёх
  программах (`kGLES3_UIFragSrc`, `kGLES3_3DFragSrc`, `kGLES3_NDSFragSrc`,
  `kGLES3_LFMFragSrc`), а не только в UI — раньше именно из-за этого
  alpha-cutout геометрия (сетка забора/колючая проволока) на мировых
  проходах рисовалась сплошной непрозрачной чёрной плашкой вместо дырчатой
  текстуры. Пара (func, ref) считается общим хелпером
  `CRC_GLES3::GetAlphaTestParams` (`MDisplaySDL2.cpp`) — используется всеми
  четырьмя программами, `RIDDICK_NO_ALPHA` продолжает отключать тест
  везде разом. В NDS/LFM (аддитивные ONE/ONE-проходы) тест сделан по
  альфе ИМЕННО диффузной текстуры (`diffuseTexel.a` / `diff.a`), а не по
  итоговой альфе фрагмента — у NDS итоговая альфа всегда форсится в 1.0
  (проход не пишет альфу), так что тест по ней никогда бы не сработал.
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

### TexGen (2026-07-27)

- **KEEP** Вершинные шейдеры (`kGLES3_UIVertSrc`, `kGLES3_3DVertSrc`)
  умеют вычислять UV в шейдере вместо чтения из вершинного регистра,
  когда `CRC_Attributes::m_lTexGenMode[iTxt]` этого требует. Разбор
  атрибута — `CRC_GLES3::PushTexGenUniforms(bool _bUI)` в
  `MDisplaySDL2.cpp`, вызывается из `SetupCommonUniforms` для ОБЕИХ
  программ на каждый draw. Разбор `m_pTexGenAttr` повторяет один в один
  движковый декодер `Classes/Render/MRenderVPGen.h::SetRegisters_TexGenMatrix`
  (порядок каналов `iTxt = 0..CRC_MAXTEXCOORDS-1`, порядок компонент
  U/V/W/Q, шаг указателя только на взведённых битах `GetTexGenComp`,
  смещение между каналами — `CRC_Attributes::GetTexGenModeAttribSize`).
  Поддержаны только два режима:
  - `CRC_TEXGENMODE_TEXCOORD` (0) — текущее поведение, UV из `aUV`/`aUV1`;
  - `CRC_TEXGENMODE_LINEAR` (1) — `uv = dot(vec4(aPos,1), U); dot(vec4(aPos,1), V)`,
    `aPos` — модельное пространство, ДО `uModel`. Матрица `uTexMat`/`uTexMat1`
    применяется к результату texgen так же, как раньше применялась к
    вершинным UV (порядок в шейдере не менялся).
  Юниформы `uTexGenMode0/1`, `uTexGenU0/V0/U1/V1` — только канал 0 и 1
  (единственные, которые сэмплят оба шейдера); 3D-шейдер имеет только
  канал 0 (у него нет `aUV1`/`vUV1` вообще). Локации кэшируются один раз
  в `InitGLResources`, как и остальные юниформы этих программ.
  Чинит депт-фог BSP2 (см. `Docs/Render_Strategy.md` §1): проход, который
  раньше сэмплил `SPECIAL_DEPTHFOGTABLE` по diffuse-UV стены (плоский
  градиент чёрное→белое поверх геометрии), теперь получает честную
  нормированную глубину по взгляду через LINEAR texgen.

- **DBG** Любой режим кроме `TEXCOORD`/`LINEAR` трактуется как `TEXCOORD`
  (текущее поведение) и логируется один раз на уникальную пару
  (режим, канал) под `RIDDICK_DBG_GL=1`: `[GLES3-TEXGEN] unsupported
  mode=%d on channel %d`. Ожидаемые кандидаты по коду движка:
  `LIGHTING`/`LIGHTING_NONORMAL` (запечённый свет), `REFLECTION`/`ENV`
  (env-мапы), `TSLV`/`TSREFLECTION` (bump/tangent-space), проективные
  источники света. Смотреть эти логи, чтобы расставить приоритеты
  следующей реализации.
  → удалить лог (оставить тихий фолбэк) когда все встречающиеся в игре
  режимы или реализованы, или сознательно списаны.

- **DBG** `RIDDICK_NO_TEXGEN=1` — см. запись в «Диагностика рендера» выше.

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

### FP20 LFM program (2026-07-27) — первая настоящая Phase B программа

Первая реальная (не диагностическая) реализация FP20-программы движка:
`XRShader_FP20_LFM` — запечённый статический свет BSP2-геометрии
(directional/radiosity-normal-map lightmaps, 4+1 базисных направления).
Всё в `MDisplaySDL2.cpp`, разбор — `XRShader_LightField.cpp:511-545`,
математика — `Docs/FP_Reference.md` §5.3 (портирована дословно из
`shaders/HL_Shading/XRShader_BRDF3.fp:939-969`).

- **KEEP** Третья GLSL ES 3.00 программа `m_LFMShader`
  (`kGLES3_LFMVertSrc`/`kGLES3_LFMFragSrc`) — тот же vertex-layout, что у
  `m_UIShader`/`m_3DShader` (локации 0=pos,1=uv0,2=col,3=uv1,4=normal,
  общий `SetVertexAttribPointersFromEntry`); `aUV1` здесь несёт
  **LFM-UV** (texcoord-сет 1), а не lightmap-modulate UV как у UI-шейдера.
  Фрагмент реализует формулу из задания один в один: 4 сэмпла LFM0..3,
  шестое направление `lfm4` из альфа-каналов LFM1..3, `nSat0/nSat1` от
  `n_ts`, взвешенная сумма × `uLFMScale`, умножение на диффуз.
- **KEEP** Выбор программы — `CRC_GLES3::TrySetupLFMProgram()`, вызывается
  в начале `SetupCommonUniforms` (после диагностики `RIDDICK_FP20`, до
  выбора `m_UIShader`/`m_3DShader`). Условие: ext-attrib типа
  `CRC_ATTRIBTYPE_FP20`, хэш `m_ProgramNameHash` совпал с
  `StringToHash("XRShader_FP20_LFM")` (посчитан из литерала при первом
  вызове, НЕ хардкод константы из лога), имя подтверждено `strcmp`, и все
  четыре текстуры каналов 10..13 присутствуют в атрибуте и успешно
  аплоадятся через общий `TextureID_EnsureUploaded`. При успехе функция
  сама делает `Use()` + все uniform'ы + все текстурные бинды и
  возвращает `true` — `SetupCommonUniforms` тут же `return`, весь
  обычный путь (UI/3D выбор, TexGen, alpha-test, fog, `RIDDICK_FORCE_TEX`
  и т.д.) для этого draw'а не выполняется. Любой другой draw (не-LFM
  FP20, LFM без одной из текстур, `RIDDICK_NO_LFM=1`) проваливается через
  `TrySetupLFMProgram() == false` и рендерится байт-в-байт как раньше.
- **KEEP** Биндинг юнитов: 0=diffuse (канал 0), 1=normal (канал 2, если
  есть), 2..5=LFM0..3 (каналы 10..13). После всех бинов активный юнит
  возвращается на `GL_TEXTURE0` (см. комментарий в
  `TrySetupLFMProgram` — сознательно не повторяет старый недочёт, когда
  активный юнит оставался ненулевым после мультитекстурного бинда).
- **DBG** Лог `[GLES3-LFM] diffuse=.. normal=.. lfm=[.. .. .. ..]
  uvset0=.. uvset1=.. scale=..` — один раз за сессию, при первом
  успешном применении программы. Счётчик `lfm=N` в строке `[GL-DBG]` —
  сколько draw'ов за 60-кадровый интервал ушло в LFM-программу.
- **HACK** `[GLES3-LFM] falling back to legacy shader: ...` — если
  канал 10..13 не заполнен или любая из четырёх LFM-текстур не
  аплоадится, программа НЕ используется (чтобы не рисовать чёрным):
  тихий откат на `m_UIShader`/`m_3DShader`, причина логируется (капа
  4 раза за сессию, не флудит).

**Упрощения относительно оригинальной формулы (сознательно, см. TODO в
`kGLES3_LFMFragSrc`):**

1. **Тангенты не проброшены.** `n_ts` берётся из normal-мапы (канал 2,
   диффузным UV) как если бы она уже была в нужном базисе, либо
   `(0,0,1)` (плоская нормаль, деградация до +Z-направления LFM) —
   настоящего tangent-space преобразования (tangentU/tangentV,
   texcoord-сеты [2]/[3] контракта) нет.
2. **`uLFMScale` — единственный скаляр** (`RIDDICK_LFM_SCALE`,
   default 4.0) вместо `4.0 * lmIntensityScale * LFM_Scale.rgb`:
   per-vertex `lmIntensityScale` (texcoord-сет [4]) и параметр
   FP20-программы `LFM_Scale` (`CRC_ExtAttributes_FragmentProgram20::
   m_pParams`) не читаются вообще.
3. **Спекуляр и восстановление направления `lW`** (упомянутые в
   оригинальной BRDF3-программе вокруг этого блока) не реализованы —
   портирован только сам lightmap-блендинг (строки 939-969).
4. **Параметры FP20-программы** (`m_pParams`/`m_nParams`) читаются
   только диагностикой `DbgLogFP20` (первые 4 вектора, для справки) —
   `TrySetupLFMProgram` их не использует вовсе.

**Не уверен / стоит перепроверить:**
- Перестановка каналов `lfm1.rgb * nSat1.b` (не `.g`) и `lfm2.rgb *
  nSat1.g` (не `.b`) взята из задания как есть ("перестановка именно
  такая") — сверить с `XRShader_BRDF3.fp:939-969`, если яркость/цвет
  на стенах будет выглядеть систематически перепутанным по осям.
- `oColor.a = diff.a` (альфа диффуза) — блендинг прохода ONE/ONE
  (аддитивный), альфа результата, скорее всего, не читается растровым
  конвейером, но это не проверено на реальном кадре.

### Тангентный базис (локации 5/6) — предпосылка для NDS и (позже) LFM

Вершинный layout расширен с 5 до 7 локаций: `0=pos, 1=uv0, 2=col, 3=uv1,
4=normal`, добавлены **`5=TangentU, 6=TangentV`**. Источник — регистры
`CRC_VREG_TEXCOORD0 + m_iTexCoordSet[2]`/`[3]` (та же индирекция, что у
mapping-UV через `m_iTexCoordSet[0]`/`[1]`), контракт — engine-код
`CXR_VirtualAttributes_ShaderFP20_COREFBB::OnSetAttributes`
(`XRShader_FP20.cpp`).

- **KEEP** Кэш-путь (`SetVertexAttribPointersFromEntry`/`BindEntryAttrib`,
  `MDisplaySDL2.cpp`) — тангенты биндятся из
  `CGLES3GeometryCache::Ensure(...)`'s `m_lRegOffset`/`m_lRegFormat`,
  которые уже generic по регистрам (`GLES3_Geometry.cpp::Build` копирует
  ЛЮБОЙ регистр, который есть у исходной геометрии, не фиксированный
  список) — если движковая геометрия реально несёт тангенты, они
  подхватываются без доп. правок в `GLES3_Geometry.cpp`. Если регистра
  нет — константный фолбэк `(1,0,0)`/`(0,1,0)` (тот же дух, что дефолт
  нормали `(0,0,1)`). `BindEntryAttrib` теперь возвращает `bool`
  (реальный регистр vs фолбэк); `SetVertexAttribPointersFromEntry`
  сохраняет это в `m_bTangentUReal`/`m_bTangentVReal` — на этот флаг
  проверяется `TrySetupNDSProgram`.
- **HACK/ограничение** Старый стриминг-путь (`SUIVert`/
  `BuildVertsFromVBB`/`BuildInterleavedVerts`/`DrawUserVerts`/
  `SetVertexAttribPointers`) тангенты **НЕ несёт вообще** — `SUIVert`
  сознательно не расширен (см. следующий раздел, почему). Локации 5/6 там
  просто дизейблятся с константным фолбэком (те же `(1,0,0)`/`(0,1,0)`) и
  `m_bTangentUReal/VReal = false` — так что любой draw через этот путь
  автоматически проваливает гейт `TrySetupNDSProgram` и рисуется старым
  шейдером.
- **KEEP** `DisableVertexAttribPointers` дизейблит и локации 5/6 (симметрично
  с 0-4).

### FP20 NDS program (2026-07-28) — критический путь освещения Riddick

`XRShader_FP20_NDS` — однопроходный динамический свет (diffuse + normal
map + Phong-спекуляр), аддитивный (`ONE/ONE`, `ZCompare EQUAL`). Разбор
контракта — `Source/P5/Shared/MOS/XR/XRShader_FP20.cpp:77-436`
(`CXR_VirtualAttributes_ShaderFP20_COREFBB` + `RenderShading_FP20_COREFBB`),
математика — портирована дословно из
`shaders/ARB_Fragment_Program/XRShader_SinglePass_Dst2_SpecNormal.fp`
(133 строки ARB-ассемблера, прочитан целиком — не только псевдо-GLSL из
`Docs/FP_Reference.md` §4, см. расхождение ниже).

- **KEEP** Четвёртая GLSL ES 3.00 программа `m_NDSShader`
  (`kGLES3_NDSVertSrc`/`kGLES3_NDSFragSrc`). Вершинный шейдер добавляет
  `aTangentU`/`aTangentV` (локации 5/6) и считает `CRC_TEXGENMODE_TSLV`
  прямо в VS для двух наборов (канал 3 = к источнику света, канал 4 = к
  глазу) — `vec3(dot(N,L), dot(TV,L), dot(TU,L)) * scale`, порядок
  компонент сверен с шаблоном `VP.xrg:1565-1573`
  (`Docs/VP_Reference.md` §3.1), НЕ с комментарием enum
  `CRC_TEXGENMODE_TSLV` (тот утверждает `x=TangU`, реальный код — иначе).
  `MSPOS` (канал 1, модельная позиция) вообще не требует отдельного
  регистра — это буквально `aPos`, поэтому передан как `vPosMS = aPos`.
- **KEEP** Выбор программы — `CRC_GLES3::TrySetupNDSProgram()`, вызывается
  в `SetupCommonUniforms` сразу после `TrySetupLFMProgram()` (по тому же
  контракту: хэш `StringToHash("XRShader_FP20_NDS")` посчитан из
  литерала, не хардкод; имя подтверждено `strcmp`). Дополнительные условия
  качества (в отличие от LFM): реальный тангентный базис
  (`m_bTangentUReal && m_bTangentVReal`), непустой normal map
  (`m_TextureID[2]`), и оба TSLV-канала (3 и 4) реально присутствуют в
  `m_pTexGenAttr` (`DecodeTexGenChannels`, см. ниже). Любое несоответствие
  — тихий откат на `m_UIShader`/`m_3DShader` с логом причины
  (`DbgLogNDSFallback`, капа 4 раза за сессию).
- **KEEP** `DecodeTexGenChannels` — общий разбор `m_pCurAttrib->
  m_pTexGenAttr` вынесен из `PushTexGenUniforms` в отдельный метод (тот же
  цикл по всем `CRC_MAXTEXCOORDS` каналам с тем же продвижением указателя
  через `GetTexGenModeAttribSize`), расширен так, чтобы параллельно с
  `LINEAR` (для UI/3D-шейдеров) отдавать и сырой `vec4` для любого канала
  в режиме `TSLV`. Чистый рефакторинг + добавление — поведение
  `PushTexGenUniforms` для существующих UI/3D-путей не изменилось.
- **DBG** Лог `[GLES3-NDS] diffuse=.. normal=.. uvset0=.. tuset=.. tvset=..
  nParams=..` + до 6 векторов параметров — один раз за сессию при первом
  успешном применении. Счётчик `nds=N` в строке `[GL-DBG]`.
- **HACK** `[GLES3-NDS] falling back to legacy shader: ...` — см. условия
  качества выше; капа 4 раза за сессию.

**Назначение 6 параметров `CRC_ExtAttributes_FragmentProgram20::m_pParams`**
(сверено с `RenderShading_FP20_COREFBB`, `XRShader_FP20.cpp:376-400`):

| # | Имя в движке | `program.env[]` | Что содержит | Читается ли ARB-программой |
|---|---|---|---|---|
| 0 | `LightPos` | `[0]` | позиция источника, model space, `.w=1` | да — `SUB r1, LightPosition, PixelPosition` (attenuation) |
| 1 | `LightRange` | `[1]` | `{1/R, R, 1/R², R²}` | да — `.z` (`1/R²`) в attenuation |
| 2 | `LightColor` | `[2]` | intensity×diffuseScale×diffuseColor, `.a`=SpecularAnisotrophy (не используется этой программой) | да — `MUL r1.rgb, LightColor, DiffuseTexel` |
| 3 | `SpecColor` | `[3]` | intensity×specularScale×specColor, `.a`=Phong-степень (`1+(specAlpha-1)*0.5`, либо `m_SpecularForcePower`) | да — `POW ...SpecColor1.a`, `MAD r0.rgb, SpecColor1, r1, r0` |
| 4 | `EyePos` | `[4]` (закомментирован в `.fp`!) | позиция глаза, model space | **нет** — ARB-исходник имеет `#PARAM EyePosition = program.env[4]` буквально закомментированным; глаз попадает в фрагмент иначе — через `vTSEV`, который VS считает из ТОГО ЖЕ значения Eye, переданного как texgen-параметр канала 4 (TSLV), а не как fragment-uniform |
| 5 | `NoiseOffset` | — | всегда `(0,0,0,0)`, кроме одного хардкод-light-GUID (`0x2346`) в движке | **нет** — ARB-исходник вообще не объявляет такой `PARAM`; мёртвый слот для этой конкретной программы (возможно, используется другим шейдер-вариантом с той же раскладкой struct) |

**Расхождения ассемблера с `Docs/FP_Reference.md` §4.2 (доверять
ассемблеру, см. код `kGLES3_NDSFragSrc`):**

1. **Self-shadow использует НОРМАЛИЗОВАННЫЙ `TSLV.x`**, а не сырой
   `IPTSLV.x`, как написано в §4.2. В ARB-файле блок "Normalize TSLV"
   (`MUL TSLV.xyz, IPTSLV, TSLV.a`) идёт РАНЬШЕ блока self-shadow
   (`SUB r0.a, const_val2.y, -TSLV.x`), и оба читают один и тот же temp-
   регистр `TSLV` — т.е. self-shadow видит уже нормализованный вектор.
   Мелкая, но не нулевая разница в весе (нормализация меняет `.x` в общем
   случае).
2. Остальная математика (attenuation, нормализация нормали/TSLV/TSEV,
   reflection, диффуз, спекуляр, финальное умножение на attn) совпадает
   с §4.2 один-в-один — расхождений не найдено.

**Не реализовано / упрощено (для `XRShader_FP20_NDS`):**

- Дефолтная normal-мапа при отсутствующей текстуре — константа
  `(0.5,0.5,1.0,1.0)` (плоская +Z-нормаль после анпака), а не настоящий
  `m_TextureID_DefaultNormal` движка (мы не знаем его точное содержимое
  без реального прогона).
- Как и у LFM, `oColor.a` не воспроизводит буквальное содержимое ARB
  `r0.a` (мусорный остаток от reflection-расчёта) — альфа-запись всё
  равно отключена движком (`Attrib_Disable(CRC_FLAGS_ALPHAWRITE)`), так
  что это не наблюдаемая разница.

### FP20 NDSP / NDSEATP programs (2026-07-28) — проекционные варианты NDS

`XRShader_FP20_NDSP` и `XRShader_FP20_NDSEATP` — тот же однопроходный
динамический свет, что и `XRShader_FP20_NDS`, плюс одна или две
проекционные текстуры (spotlight cookie), умножающие затухание света.
Реализованы ОДНОЙ GL-программой `m_NDSPShader`
(`kGLES3_NDSPVertSrc`/`kGLES3_NDSPFragSrc`) с юниформом-переключателем
`uUseProj2` вместо трёх похожих шейдеров — по прямому указанию задачи.
Селектор `CRC_GLES3::TrySetupNDSPProgram()` матчит ОБА имени по хэшу +
`strcmp` (тот же контракт, что `TrySetupNDSProgram`) и определяет
`bEATP`, откуда берутся текстурные каналы и texgen-каналы.

**Контракт «канал → карта»:**

| | `XRShader_FP20_NDSP` (COREFBB, `XRShader_FP20.cpp:77-289/298-436`) | `XRShader_FP20_NDSEATP` (класс `CXR_VirtualAttributes_ShaderFP20`, ibid:441-639/643-862+) |
|---|---|---|
| Diffuse | `m_TextureID[0]` | `m_TextureID[0]` |
| Normal(+Specular в alpha) | `m_TextureID[2]` | `m_TextureID[2]` |
| Attribute | — | `m_TextureID[3]` — **не читается** ни одним найденным `.fp` для этой программы, не сэмплируется |
| Transmission | — | `m_TextureID[4]` — **не читается**, `TransmissionColor`-параметр существует (`pParams[6]`), но ни один файл в `shaders/` его не использует; не сэмплируется |
| Projection 1 | `m_TextureID[4]`, UV из texgen-канала **7** (`LINEAR U\|V\|W`, `CreateProjMapTexGenAttr`, ibid:52-72) | `m_TextureID[5]`, UV из texgen-канала **4** (`LINEAR U\|V\|W`, `RenderShading_FP20:749-767`) |
| Projection 2 | — | `m_TextureID[6]` — **тот же движковый TextureID**, что Projection1 на этом call site (`RenderShading_FP20:855` передаёт `TextureIDProj` в оба аргумента `Create()`), тот же texgen-канал 4 |
| Environment | — | `m_TextureID[7]`, texgen-канал 5 (`BUMPCUBEENV`) — **не читается**, вклад = 0 (явно разрешено заданием) |
| TSLV → свет | texgen-канал 3 | texgen-канал **2** (другая нумерация!) |
| TSLV → глаз | texgen-канал 4 | texgen-канал **3** |
| Mapping UV / TangentU / TangentV | `m_iTexCoordSet[0]`/`[2]`/`[3]`, как у NDS | то же |

Номер texgen-канала для TSLV/проекции у NDSEATP выведен не из
комментария (там написано «TexCoord2=IPTSLV, TexCoord3=IPTSEV,
TexCoord4=ProjMap» — и это совпало), а перепроверен по порядку
последовательной записи в `pTexGenAttr` в `RenderShading_FP20`
(`nTexGenPos` растёт по буферу, `GetTexGenModeAttribSize` даёт 0 для
каналов 0/1 — TEXCOORD/MSPOS — так что первый блок TSLV ложится в канал
2, не в 0/1).

**Откуда взята математика:**

- Diffuse+Normal+Specular+Attenuation+Self-shadow — дословно тот же
  `shaders/ARB_Fragment_Program/XRShader_SinglePass_Dst2_SpecNormal.fp`,
  что и у обычного NDS (см. секцию выше).
- Умножение затухания на альфу проекционной карты — дословно из
  `shaders/ARB_Fragment_Program/XRShader_SinglePass_Dst2_Proj_SpecNormal.fp`
  (тот же файл + `TEX ProjMapTexel, ProjMapTexCoord, texture[1], CUBE;` /
  `MUL r1.w, r1.w, ProjMapTexel.a;`, между attenuation и self-shadow —
  порядок сохранён). Также сверено с общей схемой `attn *= textureCube(
  proj).rgb` в `shaders/HL_Shading/XRShader_BRDF3.fp:789-803` (другая,
  более поздняя deferred/materialmask-система — не транскрибирован
  дословно, использован только для проверки паттерна).
- Для `XRShader_FP20_NDSEATP` дословного `.fp`-аналога в дереве нет
  (задание это допускало) — контракт «канал → карта» восстановлен
  напрямую из `CXR_VirtualAttributes_ShaderFP20`/`RenderShading_FP20`
  (C++ — тоже источник истины, не только `.fp`), математика лампинга
  переиспользована как у NDS/NDSP.

**Упрощено / не восстановлено:**

- **Проекционная карта как CUBEMAP.** Оригинал сэмплирует её как
  настоящий `CUBE` (направление, не делённое на глубину). Наш
  `GLES3_Texture.{h,cpp}` (вне периметра этой задачи — редактировать
  нельзя) реализует только `Upload2D`, кубической загрузки нет. Замена:
  те же 3 плоскости `LINEAR U|V|W` (`CreateProjMapTexGenAttr`) — это
  классическая однородная проективная текстурная координата (U/V уже
  промасштабированы на `1/SpotWidth,1/SpotHeight`, W — сырая глубина в
  пространстве света), а `textureProj(sampler2D, vec3)` в GLSL ES 3.00
  считает ровно `texture(sampler, P.xy/P.z)` — стандартный 2D-вырожденный
  случай той же плоскостной математики для переднего полупространства
  (spotlight cookie), т.е. именно то, для чего используется
  `m_TextureID_DefaultLens`/projmap-фоллбэк на практике. Осознанная,
  задокументированная замена, а не догадка — но не воспроизведёт
  боковые/обратные кубические выборки, которые дал бы настоящий cubemap.
- **Environment (канал 7 у NDSEATP), Attribute (канал 3), Transmission
  (канал 4)** — НЕ сэмплируются вообще: ни один `.fp` в `shaders/` не
  ссылается на `XRShader_FP20_NDSEATP` по имени и не читает
  `TransmissionColor`/атрибут-текстуру для именно этой программы,
  восстановить надёжно нельзя — оставлено нейтральным (вклад 0), как
  прямо разрешено заданием, вместо того чтобы выдумывать формулу.
- Общий с NDS список (дефолтная normal-мапа, `oColor.a`) — см. выше.

- **KEEP** Диагностика — разовый лог `[GLES3-NDSP] prog=... diffuse=..
  normal=.. proj1=.. proj2=.. uvset0=.. tuset=.. tvset=.. lightCh=..
  eyeCh=.. projCh=..` один раз за сессию НА КАЖДОЕ из двух имён программ
  (`m_bDbgNDSPLogged`/`m_bDbgNDSEATPLogged`, независимые флаги — так в
  логе видна разбивка по программам). Счётчик кадров общий с обычным NDS
  (`nds=N` в `[GL-DBG]`), как и просило задание (один общий счётчик,
  разбивка — в разовых логах).
- **HACK** `[GLES3-NDSP] falling back to legacy shader: ...` — тот же
  контракт условий качества, что у NDS (тангентный базис, normal map,
  проекционная текстура, нужные texgen-каналы), капа 4 раза за сессию.

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
