# Отчёт: путь лайтмап BSP2 «данные из .XW → GPU → пиксель»

По спеке `Docs/Research_Lightmap.md`. Ветка `riddick-render`.
Исследование статическое (код + уже имеющиеся логи прогонов в корне репо);
новых прогонов не делалось — они запрошены в §6.

**Главный результат:** путь лайтмап BSP2 в коде **не оборван** — он работает
сквозно, что доказано логом `run_arena.log` (2026-07-29): LFM-draw'ы доходят
до бэкенда, принимаются и рисуются (`lfm=120` за 60 кадров, настоящие
атласные страницы `lm=[15915 15916 15917 15918]` и `[15919..15922]`).
Наблюдение спеки §2.5 (`fp20=1740 lfm=0`) объясняется не дефектом кода, а
env-окружением того прогона: в его строке запуска стоял
`RIDDICK_DIFFUSE_ONLY=1` (`Docs/Research_ShardedCharacters.md:39`), а
`TrySetupLFMProgram` отклоняет любой draw при DiffuseOnly **молча**, первой
же проверкой (`MDisplaySDL2.cpp:2793`).

---

## 1. Ответ на главный вопрос (§3 спеки): звенья пути и их вердикты

### 1.1 Данные → лоадер (§4.1 п.1) — ИСПРАВНО

- `LIGHTMAPINFO`/`LIGHTMAPINFO2` → `m_lLightMapInfo`
  (`WBSP2Loader.cpp:2684-2717`), `LIGHTMAPS3` → `m_spLMTC` +
  `m_lLMTextureIDs`/`m_lLMDimensions` по 4 страницы на кластер
  (`WBSP2Loader.cpp:2764-2859`) — код на месте, условия штатные.
- **Доказано логом**: на Pa1_TheDream, Pa1_Arrival и Arena_01 движок
  присылает draw'ы с программой `XRShader_FP20_LFM` (строки `[GLES3-FP]
  prog='XRShader_FP20_LFM'` в `run_dream.log`, `run_arrival.log`,
  `run_arena.log`) — это возможно только при заполненных
  `m_lLightMapInfo`/`m_lLMTextureIDs` и `m_iLMTexture >= 0` у очередей.
  Значит, в PC .XW легаси-данные лайтмап **присутствуют и парсятся** —
  это опровергает «пустые легаси-данные» из гипотезы §5.7.
- Дефект не блокирующий, но реальный: при отсутствии LIGHTMAPINFO* весь
  блок LIGHTMAPS3 скипается **молча** (`WBSP2Loader.cpp:2764`, фатальный
  `Error` на `:2867` внутри того же `if`) — в логе такая карта ничем не
  отличится от рабочей. Логирования числа лайтмап-данных в лоадере НЕТ
  вообще (живые принты рядом: только `[BSP2] PVS entries`,
  `WBSP2Loader.cpp:2566`). Патч — §5.1.

### 1.2 Лоадер → очереди (m_iLMTexture) — ИСПРАВНО, с оговоркой

- `iLMC` вычисляется в `PrepareVertexBuffer` (`WBSP2Loader.cpp:466-471`):
  требуется `m_spLMTC && m_lLMDimensions.Len() && m_lLMTextureIDs.Len()`
  **и флаг `XW_FACE_LIGHTMAP` (=4, `XW2Common.h:67`) на ПЕРВОМ фейсе VB**
  (`:469`). Очередь наследует `m_iLMTexture = iLMC` (`:484`, `:494`).
- Оговорка (тихий дефект, не блокер): VB группируется по поверхности, и
  если первый фейс VB без флага лайтмапы, весь VB остаётся с
  `m_iLMTexture = -1`, даже если остальные фейсы флаг имеют — их LM-UV
  молча не строятся (компонент 2 VB ставится только при `iLMC >= 0`,
  `:513-514` → `m_lTV2` не аллоцируется, `WBSP2Model.h:716`). Часть
  лайтмап-фейсов может теряться; массовой потери нет (иначе lfm=120 в
  run_arena не было бы).

### 1.3 Очереди → CreateLFM → движковый LFM-проход (§4.1 п.2) — ИСПРАВНО

- Развилка LF/LFM — `WBSP2Model.cpp:2419`: `m_pLightVolume != NULL`
  (энтити) → LF; мир рендерится с `_pViewClip == NULL`
  (`XREngine.cpp:3678`) → ветка World (`WBSP2Model.cpp:5113-5120`) →
  `m_pLightVolume = NULL` → LFM-батчинг (`:2437-2472`).
- Вход в `CreateLFM` — `WBSP2Model.cpp:2447` (`iLMTexture != -1`),
  `CreateLFM(..., &m_lLMTextureIDs[iLMTexture*4], 3, 4)` (`:2450`),
  `RenderShading_LightFieldMapping` (`:2472`). Прочитано и сверено по
  коду напрямую.
- `m_ShaderMode`: при `RIDDICK_FP20=1` caps форсируются
  (`MDisplaySDL2.cpp:3950-3954`), условие `XRShader.cpp:1268` проходит,
  и пин `XR_SHADERMODE=5` (`MDisplaySDL2.cpp:3987-3998`, ретрай
  `PinShaderModeIfNeeded` `:4472-4487`) даёт forward FP20. **Доказано
  логом**: `[GLES3-FP] XR_SHADERMODE pinned to 5` есть в run_dream.log и
  run_arrival.log. Без пина AUTO выбрал бы deferred DEFMM (бит 9:
  маска AUTO на `XRShader.cpp:1300` снимает SS/DEF/DEFLIN, но НЕ DEFMM,
  который derive'ится на `:1283-1284`) → LFM ушёл бы под именем
  `XRShader_FP20Def_LFM`, которое бэкенд не матчит. Этот риск закрыт
  пином.

### 1.4 Движок → бэкенд: точка обрыва №1 — НАЙДЕНА, это env-гейт, не код

Цепочка отбора в `TrySetupLFMProgram` (`MDisplaySDL2.cpp:2791-2906`,
прочитана целиком). Полный список отказов:

| # | Проверка | Строка | Лог при отказе |
|---|---|---|---|
| 1 | `GLES3_DiffuseOnly()` | `:2793` | **НЕТ** |
| 2 | `GLES3_NoLFM()` (дефолт ON) / шейдер невалиден | `:2794` | **НЕТ** (фейл компиляции шейдера печатается отдельно) |
| 3 | нет ext-attrib / тип ≠ FP20 | `:2795-2797` | НЕТ (норма для не-FP20 draw'ов) |
| 4 | хэш имени ≠ LFM | `:2808-2809` | НЕТ (норма: другие FP20-программы) |
| 5 | `strcmp` имени | `:2810-2811` | НЕТ |
| 6 | `m_TextureID[10..13]` не все ≠ 0 | `:2819-2823` | `DbgLogLFMFallback`, кап 4 |
| 7 | фейл аплоада LFM0..3 | `:2826-2834` | `DbgLogLFMFallback`, тот же кап 4 |

Вердикт по наблюдению §2.5 (`fp20=1740 lfm=0`, ни одной строки
`[GLES3-LFM]`):

- `[GLES3-FP] prog='XRShader_FP20_LFM'` в том прогоне **был** (логируется
  `DbgLogFP20` до отбора, `MDisplaySDL2.cpp:5352-5354`) → движок LFM-draw'ы
  присылал, имя/хэш корректны.
- Отказ после матча имени (причины 6/7) оставил бы до 4 строк
  `[GLES3-LFM] falling back...` — их **нет** в run_dream.log /
  run_arrival.log вообще.
- Значит, отказ произошёл на гейтах 1 или 2 — молчачих. В строке запуска
  того прогона (`Docs/Research_ShardedCharacters.md:38-43`) стоит
  **`RIDDICK_DIFFUSE_ONLY=1`** (строка 39) → отказ на `:2793`.
  (`RIDDICK_LFM=1` там был, строка 40 — гейт 2 пропустил бы.)

**Вывод: обрыва кода нет; есть молчаливый env-гейт.** Лечится
диагностикой (§5.2) и корректной строкой запуска (§6).

### 1.5 Бэкенд → GPU: принятие draw'ов — ИСПРАВНО (доказано run_arena)

`run_arena.log` (Arena_01, 2026-07-29, без DIFFUSE_ONLY, с LFM=1):

```
[GLES3-LFM] diffuse=13220 normal=13221 lfm=[15915 15916 15917 15918] uvset0=0 uvset1=3 scale=4.000000
[GLES3-LFM] diffuse=9661  normal=9663  lfm=[15919 15920 15921 15922] uvset0=0 uvset1=3 scale=4.000000
[GLES3-LFM] falling back to legacy shader: CRC_Attributes::m_TextureID[10..13] not all populated   (×4, кап)
[GL-DBG] ... fp20=48420 lfm=120 lf=660 ...
```

- 42 из 93 строк `[GL-DBG]` с `lfm=120` (+ `lfm=24`, `lfm=98`) — LFM-draw'ы
  принимаются стабильно, 2/кадр. ID 15915-15922 — последовательные
  страницы атласа (контракт §2.3 подтверждён: `uvset0=0 uvset1=3`).
- 4 фолбэка «10..13 not all populated» — меньшинство draw'ов (иначе кап 4
  был бы съеден мгновенно, а lfm не был бы 120/60ф). Источник этих
  draw'ов не идентифицирован (ранее в похожей ситуации в каналах 10..13
  оказывались обычные материальные текстуры — комментарий
  `MDisplaySDL2.cpp:2845-2849`); кап 4 общий для двух причин —
  диагностику расширить (§5.2). Блокером не является.

### 1.6 Пиксель (§4.2) — частично сломано, дефекты известны

- **Тангентный базис не проброшен** (задокументировано при коммите
  49dce52): `kGLES3_LFMVertSrc` (`MDisplaySDL2.cpp:477-500`) объявляет
  только aPos/aUV/aUV1/aCol/aNormal; нормаль сэмплится из нормал-мапы по
  тайлящемуся diffuse-UV как есть → рябь с частотой тайлинга
  (`Docs/HacksAndHooks.md:69-88`). Тангенты в данных есть
  (`WBSP2Model.cpp:1036-1070`, каналы TEXCOORD1/2 → сеты 2/3 контракта).
  Образец проброса — NDS (кэш-путь: loc 5/6 из `m_iTexCoordSet[2]/[3]`,
  `MDisplaySDL2.cpp:4873-4874`; стример сейчас глушит тангенты
  константами, `:4745-4750` — для LFM стример надо научить фетчу).
- **Per-vertex интенсивность не читается**: сет 4 (`m_lLMScale`,
  `WBSP2Model.cpp:3174-3176`, считается в тесселяции `:973-975`) в
  шейдер не заведён; вместо него uniform `RIDDICK_LFM_SCALE` (дефолт
  4.0). Поправка к §4.2 спеки: FP20-параметра `LFM_Scale` **не
  существует** — LFM-проход шлёт ровно 7 констант
  (`XRShader_LightField.cpp:660,744-784`: LightPos/LightRange/Diffuse/
  Spec/AttribScale/Env/Transmission), масштаб лайтмапы идёт только
  per-vertex сетом 4. Чинить = читать сет 4, а не `m_pParams`.
- **ZCompare EQUAL**: LFM-проход делает `CopyVBChain(_pVB)`
  (`XRShader_LightField.cpp:680`) — тот же VBID и тот же путь подачи
  вершин, что у базового прохода, поэтому побитовое равенство позиций в
  штатном случае есть. Риск только при вытеснении/ребилде кэша между
  проходами и не-identity `m_lTransform` (стример применяет per-register
  transform, `MDisplaySDL2.cpp:6629-6655`; кэш-путь игнорирует,
  `GLES3_Geometry.cpp:345-351`). Костыль `RIDDICK_ZEQ_LEQUAL=1`
  (`MDisplaySDL2.cpp:1385-1394`) на месте. Дефектом не считать до A/B.
- **Текстуры**: аплоад страниц атласа работает — в run_arena все 8 ID
  прошли `TextureID_EnsureUploaded` (иначе был бы фолбэк №7), фейлов
  `[GLES3-TEX-FAIL]` под них в логе нет. Placeholder-magenta draw **не
  отклоняет** (EnsureUploaded возвращает placeholder,
  `MDisplaySDL2.cpp:3848`) — `lfm>0` сам по себе не гарантирует реальные
  данные; проверять по `[GLES3-TEX-FAIL] ... -> placeholder` (`:3840`).

### 1.7 Минимальный список правок до «правильного пикселя»

Первая видимая лайтмапа, строго говоря, уже есть (run_arena, lfm=120) —
но с рябью и без per-vertex интенсивности. Минимум до корректной
картинки:

1. **Проброс тангентов в LFM-шейдер** (главная правка):
   `kGLES3_LFMVertSrc/kGLES3_LFMFragSrc` (`MDisplaySDL2.cpp:467-597`) —
   добавить атрибуты TangU/TangV (локации 5/6 по образцу NDS,
   `:4873-4874`) и преобразование нормали из нормал-мапы в тангентный
   базис по формуле `Docs/FP_Reference.md` §5.3; стример
   (`BuildVertsFromVBB`, фетч в `:6582-6586`/`:6670-6671` по образцу
   UVSet1) — честный фетч сетов 2/3 вместо констант `:4745-4750`.
2. **Per-vertex lmIntensityScale**: завести сет 4 (`aLMScale`) в
   LFM-вершинник на обоих путях подачи вершин (источник —
   `m_lLMScale`, канал TEXCOORD4, `WBSP2Model.cpp:3088-3097`), заменить
   скаляр `RIDDICK_LFM_SCALE` на `uScale * aLMScale`.
3. **Диагностика env-гейтов** (§5.2) — чтобы ловушка §2.5 не
   повторялась.
4. (По результатам прогона 2, если рябь останется) — A/B
   `RIDDICK_ZEQ_LEQUAL=1`.

---

## 2. Доказательная база

- Код: все якоря §1 прочитаны напрямую или через суб-агентов и
  спотчеканы: `MDisplaySDL2.cpp:1415-1426` (NoLFM, дефолт off),
  `:2791-2906` (TrySetupLFMProgram целиком), `WBSP2Model.cpp:2415-2479`
  (LF/LFM-развилка и CreateLFM), `WBSP2Loader.cpp:455-519` (iLMC по
  первому фейсу), `:2764-2794` (гейт LIGHTMAPS3), `XRShader.cpp:1258-1315`
  (ModesAvail/AUTO/default).
- Логи (уже лежали в репо, новых прогонов не было):
  - `run_arena.log` — `lfm=120` ×42 строки GL-DBG, 8 строк `[GLES3-LFM]`
    с атласными ID, 4 фолбэка «10..13 not all populated».
  - `run_dream.log`, `run_arrival.log` — `[GLES3-FP]
    prog='XRShader_FP20_LFM'` присутствует, `[GLES3-FP] XR_SHADERMODE
    pinned to 5` присутствует, строк `[GLES3-LFM]` нет, `lfm=0` →
    молчаливый отказ на гейтах `:2793/:2794`.
  - Строка запуска прогона §2.5 — `Docs/Research_ShardedCharacters.md:38-43`:
    `RIDDICK_DIFFUSE_ONLY=1` (:39) + `RIDDICK_LFM=1` (:40).

---

## 3. Опровергнутые/подтверждённые гипотезы §5 спеки

1. **«LFM-draw'ы отклоняются/не порождаются»** — уточнено и закрыто:
   порождаются и доходят до бэкенда (доказано `[GLES3-FP]`); отклонялись
   молча env-гейтом `RIDDICK_DIFFUSE_ONLY=1` (`MDisplaySDL2.cpp:2793`) в
   прогоне §2.5. При чистом env принимаются (run_arena). **Не дефект
   кода.**
2. **ZCompare EQUAL** — не подтверждена и не опровергнута; штатно позиции
   побитово равны (CopyVBChain). Отложена до прогона 3.
3. **Тангентный базис отсутствует** — **подтверждена кодом**
   (`MDisplaySDL2.cpp:477-500`, тангентов в LFM-шейдере нет); это
   главный кандидат на правку №1.
4. **Лайтмап-UV невалидны** — не подтверждена: `uvset1=3` в логе,
   тесселяция цела; верификация визуально — прогон 2 (`DBG_LFM=uv`).
5. **Страницы атласа не аплоадятся** — **опровергнута** (run_arena:
   аплоад прошёл, фолбэков №7 нет).
6. **Яркость** — **подтверждена частично**: per-vertex сет 4 не читается;
   поправка: FP20-параметра `LFM_Scale` нет вовсе
   (`XRShader_LightField.cpp:660,744-784`), чинить через сет 4.
7. **PC-атлас** — **опровергнута в постановке «легаси-данные пусты»**:
   легаси-данные в PC .XW есть и работают (§1.1). Чанки
   `LIGHTMAPRECTS`/`SPLINE_LIGHTMAPMAPPINGS` по-прежнему не найдены ни в
   одном декомпиле — проверка их наличия в файлах остаётся запросом к
   владельцу (§6), но критичности уже нет. Попутная находка: якорь спеки
   «FACES 0x204, MXR:277722» — это читатель **BSP4Glass**
   (`FUN_1019ec60`, GLASS-лоадер), не BSP2; retail и наш
   `WBSP4GlassLoader.cpp:351-388` хвост из двух uint32 одинаково
   выбрасывают.

---

## 4. Второстепенные вопросы (§3 спеки)

### 4.1 BSP1/сплайн-кисти: движок цел, обрыв точечный в бэкенде

- Лоадер полный аналог retail: LIGHTMAPINFO*/LIGHTMAPS3/
  LIGHTMAPCLUSTERS2 (`WBSPLoader.cpp:2250-2339`), пересчёт LM-UV в TV[1]
  (`:2592-2637`).
- TextureID доносится: `m_spLMTC->GetTextureID(...)` →
  `Params.m_TextureIDLightMap` (`WBSPSpline.cpp:295`, `WBSPModel.cpp:1662`,
  `:1344`) → single-pass ветка `XRUtilRS.cpp:1564-1565` кладёт лайтмапу
  на **канал 1** атрибутов.
- **Сломано в `MDisplaySDL2.cpp:5541`**: канал 1 биндится только для UI
  (`int Tex1 = bUI ? m_TextureID[1] : 0;`, комментарий `:5535-5536`);
  3D-шейдер однотекстурный. Фикс: второй сэмплер (uTex1 × vUV1,
  модуляция) в 3D-программе по образцу UI (`MDisplaySDL2.cpp:266`).
  Дешевле LFM-тангентов и даст свет на BSP1-объектах сразу.

### 4.2 BSP3: чтение и рендер-путь в дереве есть; данных нет

- `CBSP3_LightData::ReadLightmap` (`XW3Common.cpp:1078-1169`, вызов
  `WBSP3Loader.cpp:2153-2158`): чанк LIGHTMAP3 → m_spLMTC + готовые
  `CRC_Attributes` (RASTERMODE_ADD, ZWRITE off, ZCOMPARE EQUAL,
  лайтмапа на канале 0).
- Рендер — отдельный аддитивный проход по shadow-volume'ам:
  `RenderPortalLeaf` (`WBSP3Model.cpp:2392-2445`), VB с лайтмап-UV в TV0,
  приоритет `CXR_VBPRIORITY_LIGHTMAP`. С GLES3-бэкендом совместимо по
  коду (VBID-путь есть, канал 0 = «diffuse» generic-шейдера, EQUAL
  мапится).
- Вердикт: **должно работать через generic-путь, не проверяемо** — миров
  BSP3 в PC-данных Riddick нет.

### 4.3 PC-атлас

- В коде retail подтверждена **другая** триада чанков:
  `LIGHTMAPTVERTICES`/`LIGHTMAPTVERTEXINDICES`/`LIGHTMAPFACETVINFO`
  читаются BSP2-читателем retail (`FUN_102284b0`,
  `MXR_dll_decomp.c:370168-370241`): TVector2f-массив + uint16-индексы +
  per-face старты — **по-вершинно хранимые лайтмап-UV**, альтернатива
  вычисляемым тесселяцией. У нас триада **отсутствует** (grep по
  `Source/` пуст; поля закомментированы в `XW2Common_VPUShared.h:43-55`,
  тесселяция считает UV из LMI, `WBSP2Model.cpp:905-975`). Если в
  PC-файлах триада есть — retail брал UV из неё; наши вычисленные UV
  штатно эквивалентны, но это открытый вопрос (§7).
- `LIGHTMAPCLUSTERS2`: retail MXR **не знает** этого чанка (grep по
  MXR_dll_decomp.c пуст) → наш закомментированный блок в BSP2-лоадере
  (`WBSP2Loader.cpp:2870-2885`) соответствует retail; задачу снять.
- Расхождение версий LIGHTMAPINFO2: retail raw-dump при версии **5**
  (`MXR_dll_decomp.c:371330`), у нас при `XW_LIGHTMAPINFO_VERSION == 4`
  (`XW2Common.h:312`, `WBSP2Loader.cpp:2703`), а
  `CBSP2_LightMapInfo::Read` (`XW2Common.cpp:345-414`) знает версии 1-4 —
  для 5 молча не читает ничего. На Arena_01 парсинг штатный (LFM
  работает) → там версия ≤ 4; версию в Pa1_*-файлах подтвердить дампом
  (§6).

---

## 5. Патчи диагностики (предлагаются; в код не внесены)

### 5.1 Лоадер BSP2 (разовые логи при загрузке мира)

- Якорь 1: `WBSP2Loader.cpp:2717` (после чтения LIGHTMAPINFO*) и
  else-ветка `:2764` — печать:
  ```cpp
  // после блока if/else LIGHTMAPINFO/LIGHTMAPINFO2, перед if (m_lLightMapInfo.Len())
  ConOutL(CStrF("[BSP2-LM] LightMapInfo entries: %d", m_lLightMapInfo.Len()));
  ```
- Якорь 2: `WBSP2Loader.cpp:2859` (после заполнения m_lLMTextureIDs) —
  ```cpp
  ConOutL(CStrF("[BSP2-LM] LM pages: %d (%d clusters), LMTC=%s",
      m_lLMTextureIDs.Len(), m_lLMTextureIDs.Len()/4,
      m_spLMTC ? "ok" : "NULL"));
  ```
- Якорь 3: конец `PrepareVertexBuffer`-прохода (после построения
  `m_lSurfQueues`, ~`WBSP2Loader.cpp:1580`) — сводка очередей с
  `m_iLMTexture >= 0` / всего. Выводит и потери по «первому фейсу»
  (§1.2).

### 5.2 Бэкенд: закрыть молчаливые гейты (ловушка §2.5)

- Якорь: `MDisplaySDL2.cpp:2791-2797`, `TrySetupLFMProgram`. Перенести
  блок ext-attrib/хэша (`:2795-2811`) **перед** env-гейтами `:2793-2794`
  и добавить разовые (static counter) принты:
  ```cpp
  if (bIsLFM && GLES3_DiffuseOnly())
      { static int n = 0; if (n++ < 2)
          fprintf(stderr, "[GLES3-LFM] LFM draws present but rejected: RIDDICK_DIFFUSE_ONLY=1\n"); return false; }
  if (bIsLFM && (GLES3_NoLFM() || !m_LFMShader.IsValid()))
      { static int n = 0; if (n++ < 2)
          fprintf(stderr, "[GLES3-LFM] LFM draws present but rejected: NoLFM (set RIDDICK_LFM=1) or shader invalid\n"); return false; }
  ```
- Якорь: `MDisplaySDL2.cpp:2769-2776` — разделить кап `DbgLogLFMFallback`
  на два счётчика (по одному на причину), сейчас 4 строки общие.
- Якорь: `WBSP2Model.cpp:2443` (вход в LFM-батч) — разовый дамп
  `nShadingQueue` и гистограммы `m_iLMTexture` (сколько -1), плюс ветка
  `:2419` (LF или LFM, значение m_pLightVolume).

### 5.3 Новые env-флаги

Все логи §5.1–5.2 — под общий `RIDDICK_DBG_LM=1` (дефолт выкл.),
задокументировать в `Docs/HacksAndHooks.md`. Существующие
(`RIDDICK_LFM`, `RIDDICK_DBG_LFM=uv|lm`, `RIDDICK_LFM_SCALE`,
`RIDDICK_ZEQ_LEQUAL`) не меняются.

---

## 6. Что запросить у владельца

### 6.1 Прогон 1 (верификация §1.4 на Pa1_Arrival + состояние пикселя)

```
RIDDICK_FP20=1 RIDDICK_LFM=1 RIDDICK_DBG_GL=1 RIDDICK_DBG_LFM=lm \
RIDDICK_DIRECT_RENDER=1 RIDDICK_STARTMAP=Pa1_Arrival \
./build/desktop-x86_64/bin/openriddick -datapath /mnt/data_storage/sashikknox/Games/Riddick
```

Смотреть: `[GL-DBG]` — ожидаем `lfm>0`; `[GLES3-LFM]` — ID атласов и
фолбэки; скриншот — вклад запечённого света без диффуза (гладкий или
рябь). Затем вторым заходом то же с `RIDDICK_DBG_LFM=uv` — градиент
лайтмап-UV (верификация гипотезы §5.4). **Без RIDDICK_DIFFUSE_ONLY.**

### 6.2 Данные .XW (не прогон)

По `Pa1_Arrival.XW` (структура DATAFILE2.0 — `Docs/BSP_PC_Format.md` §1):

1. Есть ли ноды `LIGHTMAPRECTS` / `SPLINE_LIGHTMAPMAPPINGS`
   (ожидание: нет — тогда §2.7 закрыть окончательно).
2. Есть ли нода `LIGHTMAPTVERTICES` (+ INDICES/FACETVINFO) в BSP2-модели
   (если да — retail брал UV из неё, см. §4.3).
3. Версия чанка `LIGHTMAPINFO2` (4 или 5 — см. расхождение §4.3).
4. Хвостовые два uint32 у FACES 0x0204 у лайтмап-фейсов (нулевые?) и
   реальное значение `m_Flags` у них (бит 4 = XW_FACE_LIGHTMAP или
   0x1000 из документа).

### 6.3 Прогон 2 (после правки тангентов, отдельным этапом)

A/B скриншоты до/после; при неизменной картинке — `RIDDICK_ZEQ_LEQUAL=1`.

---

## 7. Открытые вопросы

1. Источник ~4 draw'ов с именем `XRShader_FP20_LFM`, у которых каналы
   10..13 не заполнены (run_arena) — кто их шлёт? После снятия общего
   капа 4 (§5.2) станет видно масштаб.
2. Потеря лайтмап-фейсов по «первому фейсу VB» (§1.2): массовая или
   единичная — покажет сводка §5.1 якорь 3.
3. Есть ли в PC .XW триада LIGHTMAPTVERTICES* и отличаются ли хранимые
   UV от вычисляемых (§4.3) — после ответа владельца п.6.2.
4. Версия LIGHTMAPINFO2 в Pa1_* (если 5 — наш лоадер молча не читает
   LMI, `XW2Common.cpp:345-414` vs `MXR_dll_decomp.c:371330`).
5. Формат/живость per-vertex `m_lLMScale` в PC-данных (значения могут
   быть вырождены — увидим после правки §1.7.2).

---

## Дополнение 2026-07-30: крэш Pa1_Arrival и фикс

Прогон владельца по §6.1 (`RIDDICK_FP20=1 RIDDICK_LFM=1 RIDDICK_DBG_LFM=lm`,
Pa1_Arrival) упал: `Exception! Location: GetTexture, Message: Invalid
TextureID (ID 60692, ...)` → signal 4; стек `TrySetupLFMProgram` →
`TextureID_EnsureUploaded` → `CTextureContext::GetTexture` (лог
`run_arrival.log`).

**Причина.** PC-файлы несут `LIGHTMAPINFO2` **версии 5**, а наш лоадер
raw-dump'ил только версию 4 (`WBSP2Loader.cpp`, условие
`Version == XW_LIGHTMAPINFO_VERSION`); поэлементный
`CBSP2_LightMapInfo::Read` знает версии 1-4. В итоге массив
`m_lLightMapInfo` оставался мусором → мусорный `m_iLMC` → чтение
`m_lLMTextureIDs` за границей → мусорные TextureID в каналах 10..13
(`lfm=[22 6 7 12]` — Console_02, `[25287 ...]` — FontTexture). Retail MXR
raw-dump'ит именно v5 (`MXR_dll_decomp.c:371330`), layout v5 == v4.
Недетерминизм падения (первый прогон упал, второй нет) — следствие
неинициализированной памяти.

**Следствие для визуальной оценки.** Второй (неупавший) прогон рисовал
лайтмапы из мусорных текстур (`run_arrival2.log`: `lfm=[19 25 19 28]`,
`[148 135 256 35]`, `lfm=360/60f`), поэтому пересветы снаружи на скриншоте
владельца НЕ являются вердиктом шейдеру — оценка пикселя переносится на
прогон после фикса. Внутри помещения тёмные/светлые участки видны даже на
мусоре.

**Фикс (4 файла).**
- `WBSP2Loader.cpp`: условие raw-dump ослаблено до
  `Version >= XW_LIGHTMAPINFO_VERSION` (принимает v5 как v4).
- `WBSPLoader.cpp`: то же ослабление для BSP1.
- `WBSP2Model.cpp`: валидация `m_iLightInfo`/`m_iLMC` с разовым логом
  `[BSP2-LM]` в `PrepareVertexBuffer`; per-face clamp в `Tesselate`;
  гейт перед `CreateLFM`.
- `MDisplaySDL2.cpp`: небросающий `TextureID_IsValidTC` + гейт в
  `TrySetupLFMProgram` (мусорный ID больше не роняет кадр).

Открытый вопрос №4 из §7 закрыт выводом «версия 5» (выведено из
retail-кода; факт версии в файле подтвердит дамп владельца — запрос §6.2
остаётся в силе).

**Следующий прогон для владельца:** та же строка, что в §6.1. Ожидание:
`[GLES3-LFM]` с последовательными LM-страницами атласа (как 15915-15922 на
Arena_01), крэша нет; далее оценка `DBG_LFM=lm/uv` по скриншотам.
