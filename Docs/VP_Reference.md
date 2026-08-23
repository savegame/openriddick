# Справочник по вершинному конвейеру движка (VP.xrg → GLSL ES 3.00)

Источник: `shaders/VP.xrg` (3896 строк, извлечён из `System/GL/` PC-версии игры) +
генератор `CRC_VPGenerator` (`Source/P5/Shared/MOS/Classes/Render/MRenderVPGen.cpp/.h`).
Цель документа — дать точную, проверяемую по ссылкам математику для GLSL ES
генератора нашего `CRC_GLES3`-бэкенда (`Source/P5/Shared/MOS/RenderContexts/GLES3/`).
Уже реализовано: `CRC_TEXGENMODE_TEXCOORD`, `CRC_TEXGENMODE_LINEAR`.

Важное наблюдение с самого начала: реальный движковый `#define`-набор для PC/PS3
никогда не включает `CRC_SUPPORTCUBEVP` и `CRC_QUATMATRIXPALETTE` (оба закомментированы
в `MRenderVPGen.h:19-20`, и `grep` по всему дереву не находит ни одного `#define`
этих макросов). Это значит, что **весь `CubeVec`/`CubeTex`/`MPQuat`-путь в `VP.xrg`
— мёртвый код** для нашей цели: он присутствует в шаблоне, но генератор никогда не
включает соответствующие `*if_`-блоки. Все ссылки на эти блоки ниже помечены "мертво".

---

## 1. Структура шаблона `VP.xrg`

### 1.1 Формат файла

Это XRG-registry-формат (тот же `CRegistry::XRG_Parse`, что и обычные `.xrg`
конфиги движка), с одним корневым блоком:

```
*VertexProgram
{
    *vpheader "@HEADER"
    *if_texcoordin0 "@TEXCOORDIN0"
    ...
}
```

Дерево детей обходится рекурсивно (`CRC_VPGenerator::GetKeyList_r`,
`Source/P5/Shared/MOS/Classes/Render/MRenderVPGen.cpp:74-121`). Листовые узлы —
строки в кавычках (`"..."`) — это буквальный ARB-vertex-program ассемблер;
не-листовые — либо условные фильтры, либо просто именующие группы.

### 1.2 Директивы

| Директива | Смысл | Где обрабатывается |
|---|---|---|
| `*if_X { ... }` / `*if_X "..."` | Включить поддерево, только если `X` есть в списке активных `#define` этого прохода генерации (регистр не важен, ищется как дочернее имя `IF_X`) | `GetKeyList_r`, `MRenderVPGen.cpp:83-93` |
| `*ifnot_X { ... }` | Включить, только если `X` **отсутствует** в списке define | `MRenderVPGen.cpp:94-104` |
| именующие узлы без `if_`/`ifnot_` (`*Pre`, `*Post`, `*doeet`, `*doit`, `*doit2`, `*store`, `*storecolor`, `*LightBegin`, `*clamplight`, `*copypos`, `*normal`, `*end` и т.п.) | Не являются условиями — просто группируют/именуют кусок кода для читаемости; обходятся безусловно, в порядке появления в файле | `GetKeyList_r` рекурсирует в любого ребёнка, фильтр применяется только к именам вида `IF_*`/`IFNOT_*` |
| `*vpheader "@HEADER"` | Голова программы — первая подставляемая строка | `VP.xrg:122-124` |
| `@NAME` | **Текстовый макрос платформы**, разворачивается **один раз при загрузке файла**, до XRG-парсинга, из второго файла — `"VPDefines_<Платформа>.xrg"` (на PS3: `System/GL/VPDefines_PS3.xrg`, вызов `Create("System/GL/VP.xrg", "System/GL/VPDefines_PS3.xrg", false, CRC_MAXTEXCOORDS)` в `Source/P5/Shared/MOS/RenderContexts/PS3GCM/MRenderPS3_VertexProgram.cpp:477`). Этого defines-файла **нет в нашем репозитории** (пользователь достал только `VP.xrg` из `System/GL/`) — но для порта на GLSL это и не нужно: `@V_POS`, `@V_NRML`, `@V_MI`, `@V_MW`, `@V_MI2`, `@V_MW2`, `@O_HPOS`, `@O_TEX0..7`, `@O_COL0`, `@O_FOG`, `@O_CLIPVERTEX`, `@O_CLIP0..5`, `@ARL`, `@MPSIGN`, `@VTX_TEX0/1`, `@HEADER`, `@END`, `@TEXCOORDIN%d`, `@TEXCOORDOUT%d` — это просто имена вход/выходных регистров и пары ассемблерных мнемоник, подставляемые как текст (реализация подстановки — `CRC_VPGenerator::Create`, `MRenderVPGen.cpp:403-497`, цикл по `@` на строках 458-487). В GLSL это будут: `@V_POS`→`in vec3 a_Pos`, `@V_NRML`→`in vec3 a_Normal`, `@O_HPOS`→`gl_Position`, `@O_TEXn`→`out vec4 v_TexN`, `@O_COL0`→`out vec4 v_Color`, `@ARL a0.x, X`→`int a0 = int(floor(X))` и т.д. |
| `$NAME[+N\|-N]` | **Динамическая константа**, разворачивается **на каждую генерацию программы** в `CRC_VPGenerator::SubstituteConstants` (`MRenderVPGen.cpp:270-395`). `$NAME` даёт базовый индекс регистра константы (см. §2), `+N`/`-N` — компайл-таймовое смещение, считанное сразу после имени (`SubstConstant`, `MRenderVPGen.cpp:127-179`) |
| `$vN[.xyzw]` | Подстановка входного вершинного регистра **по номеру потока**, используется только на Xenon (`ActiveVertices`-битовая маска); на PC/PS3 `ActiveVertices` жёстко `0xffffffff` (`MRenderVPGen.cpp:264-268`), поэтому всегда берёт `vN`, а не заглушку `c[8].y...y` | `SubstVertex`, `MRenderVPGen.cpp:181-246` |

### 1.3 Как define'ы формируются в C++ (`CRC_VPGenerator::CreateVP`, `MRenderVPGen.cpp:499-780`)

Список активных `#define` для конкретного вызова строится из `CRC_VPFormat` так:

1. `MWCOMP<n>` — число костей на вершину, `n = GetMWComp()` (см. §5).
2. `TEXCOORDIN<i>` / `TEXCOORDOUT<i>` для `i=0..7` — какие входные/выходные потоки существуют.
3. `MPQuat` — **мертво** (см. выше).
4. `LIGHTING` — если `FMTBF0_VERTEXLIGHTING` (только когда `CRC_SUPPORTVERTEXLIGHTING` определён — на PS3 **не** определён, т.к. `PLATFORM_CONSOLE` включён (`Target_PS3.h:63`) и макрос собирается только `#ifndef PLATFORM_CONSOLE` (`MRender_Classes_VPUShared.h:14-16`); на PC-таргете движка вершинное освещение реально работает — для нашего порта релевантно).
5. `COLORVERTEX` / `COLOROUTPUT` — инверсия флагов `FMTBF0_NOVERTEXCOLOR`/`FMTBF0_NOCOLOROUTPUT`.
6. `USENORMAL`, `USETANGENTS`, `NORMALIZENORMAL`, `FOGDEPTH`, `POSTRANS`, `CLIPPLANES` — по битам `FMTBF0_*`.
7. `CubeVec`/`CubeTex`/`CubeTexSclZ`/`CubeTexScl2` — **мертво** (`CRC_SUPPORTCUBEVP` не определён).
8. `LIGHT<i>_POINT`/`_AMBIENT`/`_PARALLELL` для каждого активного источника (см. §4).
9. Для каждого текстурного канала `i=0..MaxTexCoords-1`: `TEXGEN<i>_COMP<j>` (всегда все 4, `Components=0xf` захардкожено, `MRenderVPGen.cpp:630`), `TEXGEN<i>_<MODE>` (см. §3 — полная раскладка switch на `MRenderVPGen.cpp:640-746`), `TEXMATRIX<i>` если для канала привязана явная 4×4 текстурная матрица, `TEXTURETRANS<i>` если для канала включено input-scale (упакованные форматы вершин).
10. Каналы `MaxTexCoords..CRC_MAXTEXCOORDS-1` (на PC максимум и так 8, см. `CRC_MAXTEXCOORDS` ниже) получают принудительно `TEXGEN<i>_VOID`.

### 1.4 Полный список `*if_`/`*ifnot_` блоков в `VP.xrg`

Блоки повторяются почти один-в-один для каждого текстурного канала `N=0..7`
(`shaders/VP.xrg` grep даёт >250 вхождений `*if_texgenN_*`) — здесь они сведены в
семьи с одной строкой на семью; отличия по каналам — в примечании и в §3.

**Скиннинг / позиция** (`shaders/VP.xrg:147-683`):

| Блок | Назначение |
|---|---|
| `CubeVec` / `ifnot_CubeVec` | **Мертво.** Альтернативный "cube special" путь трансформации позиции |
| `PosTrans` | Позиция — упакованный формат (нужен MAD scale+offset) vs уже float |
| `usenormal` / `normal` | Есть входной поток нормали и он используется |
| `normalizenormal` | Нормализовать нормаль после блендинга костей |
| `MWComp0`..`MWComp8` | Число влияющих костей на вершину: `0`=без скининга (просто позиция), `1`=одна кость без веса, `2..8` = блендинг N костей с весами (см. §5) |
| `MPQuat` / `ifnot_MPQuat` | **Мертво.** Кватернионное представление кости вместо 4×3-матрицы |
| `CubeTex`, `CubeTexScl2`, `CubeTexSclZ` | **Мертво.** Спецмасштабирование UV для cube-пути |

**Тени / туман / освещение** (`shaders/VP.xrg:690-1131`):

| Блок | Назначение |
|---|---|
| `texgen0_shadowvolume` | Экструзия позиции по силуэту (стенсильные тени), только канал 0 |
| `texgen0_v_linear_texture`, `texgen1_n_linear_texture` | Явная выборка текстуры прямо в VP через линейный texgen — древний fixed-pipeline пережиток, каналы 0/1 |
| `texgen0_shadowvolume2` | Альтернативная "полу-аппаратная" экструзия с per-vertex весом из потока UV0, только канал 0 |
| `fogdepth` | Вычислить множитель тумана по глубине и свернуть в альфу цвета |
| `lighting` / `ifnot_lighting` | Включена ли вершинная накопительная модель освещения вообще |
| `Light0..7_Point` / `_Ambient` / `_Parallell` | Диспетчер типа источника на слот (см. §4) |
| `colorvertex` | Читать ли входной цвет вершины |
| `coloroutput` | Писать ли выходной цвет вообще (иначе цвет не пишется в варьинг) |
| `clipplanes` / `clipvertex` | Пользовательские плоскости отсечения: через `CLIPVERTEX` или через 6 отдельных `CLIP0..5` |

**Тангенты** (`shaders/VP.xrg:1138-1221`):

| Блок | Назначение |
|---|---|
| `usetangents` | Подготовить TangentU/TangentV (для bumpcubeenv/tsreflection/tslv/decaltstransform/depthoffset/pixelinfo/tang_u/tang_v); при активном скининге тангенты дополнительно вращаются той же матрицей кости, что и позиция/нормаль |
| `TextureTrans2`/`TextureTrans3` | Входные потоки тангентов упакованы (нужен scale+offset) — используют регистры канала 2/3 как хранилище сырых тангентов, независимо от того, какой texgen реально включён на этих каналах |

**На каждый канал `N=0..7`** (`shaders/VP.xrg:1226-3894`, конкретные строки — в §3):

`texgenN_void` (внешний гейт — канал вообще не пишется), `texgenN_null` (пишется 0),
`texgenN_texcoord` (+ `texmatrixN`, `TextureTransN`), `texgenN_linear`,
`texgenN_linear_ms` (**мертво**, ниже), `texgenN_screen` (условно мертво, ниже),
`texgenN_normalmap`, `texgenN_tang_u`, `texgenN_tang_v`, `texgenN_reflection`,
`texgenN_env`, `texgenN_Lighting`, `texgenN_Lighting_Nonormal`, `texgenN_LightField`,
`texgenN_tslv`, `texgenN_pixelinfo` (+`texcoordoutN+1`/`+2`), `texgenN_depthoffset`,
`texgenN_mspos`, `texgenN_vspos`, `texgenN_constant`. Только на части каналов:
`texgenN_linearnhf`, `texgenN_boxnhf` (только N=0), `texgenN_tsreflection` (N=0..3),
`texgenN_bumpcubeenv` (N=1,2,5), `texgenN_decaltstransform` (только N=2),
`texgenN_wspos` (только N=1, нигде не генерируется — см. ниже).

**Мёртвые/недостижимые блоки** (define, который их включает, никогда не генерируется
в `CreateVP`, либо генерируется, но использует нерелевантный для нас ассемблер):

- `texgenN_linear_ms` — генератор никогда не эмиттирует строку `TEXGENn_LINEAR_MS`
  (в `switch` на `MRenderVPGen.cpp:640-746` такого `case` нет вообще) — блок недостижим.
- `texgenN_screen` — **достижим** (`CRC_TEXGENMODE_SCREEN`, `case` на
  `MRenderVPGen.cpp:730-732`, реально используется старым NV20/register-combiner
  путём — `Source/P5/Shared/MOS/XR/XRShader_NV20.cpp:302,378,770,1235,1334`), но
  содержимое блока — буквальный GameCube-ассемблер (`r92`,`r93`,`r12` — регистры,
  которых нет в общей модели, см. `shaders/VP.xrg:1380-1384`) — для порта
  бесполезен, писать с нуля по смыслу (`CRC_TEXGENMODE_SCREEN` в комментарии:
  "GAMECUBE SPECIFIC TEXGENMODE", `MRender_Classes.h:535`).
- `CRC_TEXGENMODE_ENV2` используется движковым кодом
  (`Source/P5/Shared/MOS/XR/XRVBOperators.cpp:690`), но в `CreateVP` для него нет
  `case` вообще — попадает в `default:` → эмиттит `TEXGENi_NONE`, для которого в
  `VP.xrg` тоже нет блока → канал молча не пишется. Для порта нужен отдельный
  новый режим, в шаблоне референса нет.
- `wspos` встречается только как `texgen1_wspos` (`shaders/VP.xrg:2061-2067`) —
  в `CProgramFormat`/`CreateVP` нет генерации строки `TEXGEN1_WSPOS` — блок мёртв
  (артефакт другой платформы/более старой версии шаблона).

---

## 2. Раскладка констант

### 2.1 Статический префикс (перед `$BASE`, всегда один и тот же формат)

Из комментария в шапке файла (`shaders/VP.xrg:32-68`) и `CRC_VPFormat::SetRegisters_Init`
(`Source/P5/Shared/MOS/Classes/Render/MRenderVPGen.cpp:13-32`). Обратите внимание:
в шаблоне везде используется `c[$BASE+N]`, т.е. индексы из комментария в шапке
(`c[0..3]`,...) нужно понимать как индексы **относительно `$BASE`**
(`m_iConstant_Base` — параметр `CRC_VPFormat::Create(_iConstant_Base=0)`,
`MRenderVPGen.h:253-261`, на практике почти всегда 0):

| Регистр | Содержимое | Заполняется |
|---|---|---|
| `c[BASE+0..3]` | `Model*Projection` (модель → clip-space), 4 строки для `DP4` | движком, не через `SetRegisters_*` этого файла |
| `c[BASE+4..6]` | Поворот модели 3×3 (модель → view-space), 3 строки для `DP3`/`DP4` | движком |
| `c[BASE+7]` | Перенос модели (модель → view-space) | движком |
| `c[BASE+8]` | `x=0.0, y=1.0, z=0.5, w=(2 или 3)×255.0001` (`w` — множитель распаковки индекса кости: `2×` для мёртвого quat-варианта, `3×` для обычной 4×3-матрицы) | `SetRegisters_Init`, `MRenderVPGen.cpp:13-22` |
| `c[BASE+9]` | `x=2.0, y=255.0001, z=1/20, w=2/512` — **все 4 компоненты используются только мёртвым `CubeVec`/`CubeTex`-путём** | `SetRegisters_Init`, `MRenderVPGen.cpp:24-29` |

Начиная с `c[BASE+10]` (или той точки, докуда дошёл предыдущий блок) — **вся
раскладка динамическая**: каждый `SetRegisters_*` получает текущий счётчик
регистра `iReg`, пишет туда сколько нужно `vec4` и возвращает их количество;
следующий вызов получает уже сдвинутый `iReg`. Никакого фиксированного слота
(кроме самого начала, `EStart=8`, `CRC_VPFormat::GetFirstFreeRegister()`,
`MRenderVPGen.h:600`) в реально используемом коде нет — enum `ERegisterStart`/
`EMaxSizes` (`MRenderVPGen.h:291-330`, с полями типа `EConstantColor`,
`ETexGenMatrix`, `EMatrixPalette`) **нигде не используется** (проверено: `grep`
по всему дереву не находит обращений к `EConstantColor`/`EMatrixPalette`/
`ETexGenMatrix`/`EAttrib`/`ETransform`/`EClipPlanes` вне самого объявления enum) —
это дохлая/устаревшая заготовка фиксированного бюджета констант, живой путь —
последовательный курсор `iReg`, см. конкретный порядок вызовов в
`Source/P5/Shared/MOS/RenderContexts/PS3GCM/MRenderPS3_Attrib.cpp:937-1004`:

```
iReg = 8 (GetFirstFreeRegister)
iReg += SetRegisters_Init(...)                    // 2 vec4              → база 8
iReg += SetRegisters_ConstantColor(...)           // 1 vec4              → база 10 (комментарий "must be fixed at base 10")
iReg += SetRegisters_TextureTransformFull(...)    // 2 + 8*2 = 18 vec4   → база 11
[iReg += SetRegisters_ClipPlanes(...)]            // 6 vec4, если включены clip-planes
iReg += SetRegisters_Attrib(...)                  // 0 или 1 vec4 (FOGDEPTH)
iReg += SetRegisters_TexGenMatrix(...)            // TEXMATRIXx (4 vec4 каждая) + TEXPARAMx (см. §2.3)
[iReg += SetRegisters_Lights(...)]                // 2 vec4 на источник, если лайтинг включён
[iReg += SetRegisters_MatrixPalette(...)]         // 3 vec4 на кость (или 2 vec4 — мёртвый quat-путь)
```

**Для порта на GLSL ES этот весь механизм линейных "constant registers" не нужен
вообще** — вместо единого массива `c[]` каждая логическая сущность
(`$CONSTANTCOLOR`, `$POSTRANS`, `$TEXPARAMx`, `$TEXMATRIXx`, `$TEXTURETRANSx`,
`$LIGHT`, `$MP`) становится **отдельным именованным uniform** (или полем UBO);
единственное место, где реально нужна индексация по массиву — матрицы палитры
костей (`$MP`, массив `mat4`/`mat4x3`, индексируемый декодированным
per-vertex индексом, см. §5) и массив источников света (`$LIGHT`, `vec4[16]`
или `struct Light[8]`).

### 2.2 Прочие динамические константы (не текстурные)

| Имя | Кол-во `vec4` | Заполняется | Содержимое |
|---|---|---|---|
| `$CONSTANTCOLOR` | 1 | `SetRegisters_ConstantColor`, `MRenderVPGen.h:567-578` | `RGBA / 255` (glModulate-цвет геометрии) |
| `$POSTRANS` | 2 (`Scale`,`Offset`) | `SetRegisters_VertexInputScale`, `MRenderVPGen.h:626-688`, только если `FMTBF0_VERTEXINPUTSCALE` | Деквантизация упакованной позиции: `pos = raw*Scale + Offset` |
| `$TEXTURETRANSx` | 2 на канал (`Scale`,`Offset`) | тот же вызов, по одному на каждый канал с включённым `TexCoordInputScale`-битом; **регистрируется на "переименованный" (rename) индекс, см. код на `MRenderVPGen.h:672-677`** — если канал переименован (`GetRename(i) != i`), присвоение `m_iConstant_TexTransform[j]` пропускается (только когда `OriginalCoord == i`) — при нетривиальном ремаппинге текстурных каналов эта константа для части каналов может остаться незаполненной; учитывать при портировании ремаппинга | Деквантизация упакованного UV: `uv = raw*Scale + Offset` |
| `$FOGDEPTH` | 1 (только если `FMTBF0_DEPTHFOG`) | `SetRegisters_Attrib`, `MRenderVPGen.h:581-598` | `x = fogEnd/(fogEnd-fogStart)`, `y = 1/(fogEnd-fogStart)` (на практике в шаблоне туман использует только `.y`, см. §4.2) |
| `$CLIPPLANES` | 6 (фикс. размер, только если `FMTBF0_CLIPPLANES`) | `SetRegisters_ClipPlanes`, `MRenderVPGen.h:603-623` | `{n.x,n.y,n.z,d}` на плоскость, в **view-space модели** (плоскости трансформируются `InverseOrthogonal(Model)` перед загрузкой, см. `MRenderPS3_Attrib.cpp:965-976`) |
| `$LIGHT` | `2 × nLights` | `SetRegisters_Lights`, `MRenderVPGen.h:1040-1136` (только если `CRC_SUPPORTVERTEXLIGHTING`) | см. §4 |
| `$MP` | `3 × nBones` (или `2 × nBones` — мёртвый quat) | `SetRegisters_MatrixPalette[_Quat]`, `MRenderVPGen.h:1177-1328` | см. §5 |

### 2.3 Текстурные константы (`$TEXPARAMx_y`, `$TEXMATRIXx`)

`SetRegisters_TexGenMatrix` (`Source/P5/Shared/MOS/Classes/Render/MRenderVPGen.h:711-1037`,
шаблонный метод, вызывается один раз на все 8 каналов сразу) для каждого канала `iTxt`:

1. Если каналу назначена явная текстурная матрица (`_pMatrix[iTxt] != null`,
   привязывается через `Attrib_SetTexMatrix`/`SetTexMatrix`) — пишет 4 `vec4`
   (**транспонированную** 4×4-матрицу, по строкам для `DP4`) в `$TEXMATRIXx`.
2. Затем, в зависимости от `texgen`-режима канала, дописывает `$TEXPARAMx_0..3`
   параметров, число которых равно `CRC_VPFormat::ms_lnTexGenParams[mode]`
   (`MRenderVPGen.h:211-238`, таблица размеров в `vec4`, инициализируется в
   `InitRegsNeeded`). Данные для этих регистров копируются из CPU-стороннего
   буфера атрибута `_pAttrib->m_pTexGenAttr` (тип `fp32*`, плоский массив
   float, "плотно упакованный" по всем каналам подряд — размер на канал
   даёт `CRC_Attributes::GetTexGenModeAttribSize` в **числе float**,
   `Source/P5/Shared/MOS/MSystem/Raster/MRender.cpp:1208-1248`; расхождение
   единиц с `ms_lnTexGenParams`, приведённых в `vec4`, нормально — сравнение
   ниже).

Таблица размеров по режиму (сверка `ms_lnTexGenParams` [vec4] vs
`GetTexGenModeAttribSize` [float] — значения согласованы, `float = vec4 × 4`,
кроме отмеченных):

| Режим | `vec4` (`ms_lnTexGenParams`) | float (`GetTexGenModeAttribSize`) | Комментарий |
|---|---|---|---|
| `LINEAR` | 4 | `nComp × 4` (обычно 16 = 4 vec4) | зависит от `TexGenComp`-маски (U/V/W/Q) |
| `LINEARNHF` | 3 | 12 | сходится |
| `BOXNHF` | 10 | 40 | сходится |
| `SHADOWVOLUME2` | 1 | не задан в `GetTexGenModeAttribSize` (0 по `default`) | см. ниже — расходится, но у SHADOWVOLUME2 источник данных — вершинный поток, не `m_pTexGenAttr` |
| `CONSTANT` | 1 | 4 | сходится |
| `SHADOWVOLUME` | 1 | 4 | сходится |
| `BUMPCUBEENV` | 8 | 32 | сходится |
| `TSREFLECTION` | 1 (в `SetRegisters_TexGenMatrix` копируется **2** `vec4`, см. `MRenderVPGen.h:940-958`) | 8 (=2 vec4) | **расхождение**: `ms_lnTexGenParams[TSREFLECTION]=1`, но код копирует 2 `vec4` и продвигает `iReg` на 2 — таблица размеров устарела относительно кода, при порте ориентируйтесь на код (`SetRegisters_TexGenMatrix`), не на таблицу |
| `LIGHTING` / `LIGHTING_NONORMAL` | 2 | 8 | сходится |
| `LIGHTFIELD` | 6 | 24 (нет явного `case`, используется `default: return 0` в `GetTexGenModeAttribSize` — **не заявлено**, но код `SetRegisters_TexGenMatrix` копирует 6 vec4) | `GetTexGenModeAttribSize` не знает про `LIGHTFIELD` — ещё одно расхождение таблицы/кода |
| `DECALTSTRANSFORM` | 4 (зарезервировано) | 12 (=3 vec4 источника) | код читает только **2** vec4 (`c[$TEXPARAM0+0]`,`c[$TEXPARAM0+1]`, см. §3.14) — 3 несовпадающих числа: 4 (резерв) / 3 (источник) / 2 (реально прочитано) |
| `TSLV` | 1 | 4 | сходится |
| `PIXELINFO` | 3 | 12 | сходится |
| `DEPTHOFFSET` | 2 | 8 | сходится |
| прочие (`TEXCOORD`,`NORMALMAP`,`REFLECTION`,`ENV`,`TANG_U/V`,`MSPOS`,`VSPOS`,`NULL`,`VOID`) | 0 | 0 | не используют `$TEXPARAM`, входные данные — уже имеющиеся регистры (`R8`/`R9`/`R0`/`R1`) или матрицы вида/проекции |

Вывод: **для GLSL-порта не полагайтесь на `ms_lnTexGenParams`/`GetTexGenModeAttribSize`
как на источник истины по размеру uniform-а** — берите фактическое число
`vec4`, реально читаемых в `SetRegisters_TexGenMatrix` (`MRenderVPGen.h:763-1032`),
оно и есть контракт с шаблоном `VP.xrg`.

---

## 3. Математика по TEXGEN-режимам

Обозначения (соответствуют регистрам ARB-кода):
- `P` — позиция вершины в **анимированном модельном пространстве** после
  скининга и `PosTrans` (регистр `R8`, `.w=1`).
- `N` — нормаль там же (регистр `R9`, нормализована, если включён `normalizenormal`).
- `TU`, `TV` — тангенты U/V там же, после скининга той же матрицей, что и
  позиция/нормаль (регистры `R0`/`R1`, см. §1, блок `usetangents`).
- `ToView(p)` — переход модель→view, встречается почти во всех режимах,
  использующих камеру: `p' = p + c[BASE+7]; view = vec3(dot(p',c[BASE+4]), dot(p',c[BASE+5]), dot(p',c[BASE+6]))`.
  Псевдо-GLSL: `vec3 ToView(vec3 p) { vec4 p1 = vec4(p + uModelTranslate, 1.0); return vec3(dot(p1,uViewRow0), dot(p1,uViewRow1), dot(p1,uViewRow2)); }`
- `ToView3x3(v)` — то же самое, но без переноса (`DP3`, для направлений):
  `vec3 ToView3x3(vec3 v) { return vec3(dot(v,uViewRow0.xyz), dot(v,uViewRow1.xyz), dot(v,uViewRow2.xyz)); }`

Приоритетные режимы — сначала.

### 3.1 `CRC_TEXGENMODE_TSLV` (tangent-space light vector)

`shaders/VP.xrg:1565-1573`. Вход: `P`, `N`, `TU`, `TV`. Пространство: **касательное**
(результат — компоненты вектора "к свету минус к глазу" в базисе TU/TV/N).
Параметры: 1 `vec4` (`$TEXPARAM0+0` = `LightPos.xyz`, `.w` = дополнительный
масштаб — множится на весь результат).

```glsl
vec3 L = uTexParam0.xyz - P;      // ADD R4, c[$TEXPARAM0+0], -R8
float x = dot(N,  L);
float y = dot(TV, L);
float z = dot(TU, L);
vec3 uv = vec3(x, y, z) * uTexParam0.w;
// O_TEXn = vec4(uv, 0)
```
Внимание на порядок: в ассемблере `R3.x=dot(R9,R4)` (N), `R3.y=dot(R1,R4)` (TV),
`R3.z=dot(R0,R4)` (TU) — т.е. компонента **x результата — это проекция на нормаль**,
не на тангенс. Комментарий в enum (`MRender_Classes.h:552-555`) подтверждает:
`x=(L-V)·TangU`? Нет — фактический код кладёт `dot(N,L)` в `.x`, `dot(TV,L)` в `.y`,
`dot(TU,L)` в `.z`; порядок в комментарии enum (`x=TangU`,`y=TangV`,`z=N`) **не
совпадает с реальным кодом шаблона** — транслируйте по коду, не по комментарию
enum.

### 3.2 `CRC_TEXGENMODE_PIXELINFO`

`shaders/VP.xrg:1596-1616` (канал 0, аналогично на 1/2/3/4/5, с сдвигом выходных
каналов). Вход: `P`,`N`,`TU`,`TV`. Пространство результата — задаётся тремя
переданными строками матрицы (`$TEXPARAM0+0..2`), обычно модель→мир. Параметры:
3 `vec4` (строки матрицы). Пишет **три последовательных выходных texcoord'а**
(текущий канал `N` получает первую строку через основной хвост генератора, `N+1`
и `N+2` — прямо из этого блока, только если соответствующий `texcoordoutN+1/+2`
активен):

```glsl
// row0 = uTexParam0[0], row1 = uTexParam0[1], row2 = uTexParam0[2]
vec4 outN   = vec4(dot(N,row0), dot(TV,row0), dot(TU,row0), dot(P,row0.xyz)+row0.w /*DP4 с P.w=1*/);
vec4 outN1  = vec4(dot(N,row1), dot(TV,row1), dot(TU,row1), dot(vec4(P,1),row1));
vec4 outN2  = vec4(dot(N,row2), dot(TV,row2), dot(TU,row2), dot(vec4(P,1),row2));
// v_TexN   = outN   (дописывается общим хвостом ifnot_texgenN_constant/mspos)
// v_TexN+1 = outN1
// v_TexN+2 = outN2
```
Опять тот же порядок компонент, что в TSLV: `.x`←N, `.y`←TV, `.z`←TU, `.w`←P
(проекция позиции через `DP4`, т.е. включает перенос строки).

### 3.3 `CRC_TEXGENMODE_NORMALMAP`

`shaders/VP.xrg:1451-1454` (и по одному блоку на каждый канал). Вход: `N`.
Пространство: модель. Параметров: 0.
```glsl
uv.xyz = N;  // MOV R3, R9 — далее общий хвост копирует R3 в O_TEXn (см. §3.16)
```

### 3.4 `CRC_TEXGENMODE_REFLECTION`

`shaders/VP.xrg:1467-1483`. Вход: `P`,`N`. Пространство: **view-space**.
Параметров: 0.
```glsl
vec3 V = ToView(P);            // НЕ нормализуется! это view-space позиция, не eye-vector
vec3 Nv = ToView3x3(N);
float d = dot(-V, Nv) * 2.0;
vec3 R = V + Nv * d;           // классический reflect(), но на непронормированном V
// uv.xyz = R (через хвост §3.16)
```
Замечание: `V` не нормализован (это дословно view-space координата вершины,
а не единичный вектор взгляда) — расхождение с "учебной" формулой отражения;
переносите как есть, не "исправляйте" на `normalize(viewPos)`, чтобы сохранить
визуальное поведение оригинала.

### 3.5 `CRC_TEXGENMODE_ENV`

`shaders/VP.xrg:1485-1503`. Вход: `N` (+ опционально исходный UV канала через
`TextureTrans0`, но результат тут же перезаписывается — т.е. TextureTrans-ветка
фактически не влияет на итог, читайте `*Post`). Пространство: view-space.
Параметров: 0 (кроме неиспользуемого TextureTrans-эффекта).
```glsl
vec3 Nv = ToView3x3(N);
uv.x = Nv.x * 0.5 + 0.5;
uv.y = -Nv.y * 0.5 + 0.5;
// uv.z, uv.w не трогаются этим блоком (остаются от TextureTrans-ветки/предыдущего состояния R3)
```
Классический dual-paraboloid/sphere-map fixed-function envmap (`N.xy * 0.5 + 0.5`,
Y инвертирован).

### 3.6 `CRC_TEXGENMODE_LIGHTING`

`shaders/VP.xrg:1533-1548`. Вход: `P`,`N`. Пространство: модель (для `P`,`N`),
свет — тоже в модельном пространстве (`$TEXPARAM0+0.xyz`). Параметров: 2 `vec4`
(`+0`=`LightPos.xyz,invRange`, `+1`=`Color.rgb`).
```glsl
vec3 Lraw = uTexParam0_0.xyz - P;
float dist = length(Lraw);
float atten = max(1.0 - uTexParam0_0.w * dist, 0.0);   // линейное затухание по invRange
vec3 L = Lraw * (dist * atten);                         // ВНИМАНИЕ: домножение на dist, не 1/dist — транслитерация буквальная, см. §4.2 про то же в *if_lighting
float ndotl = max(dot(N, L), 0.0);
vec3 uv_rgb = ndotl * uTexParam0_1.rgb;
// uv.w = 1.0
```
Это "запечь диффуз в текстурную координату" вариант того же самого закона
затухания, что в §4 (обычное вершинное освещение) — см. там же предупреждение
про нетривиальную (не honest inverse-square) модель.

### 3.7 `CRC_TEXGENMODE_LIGHTING_NONORMAL`

`shaders/VP.xrg:1521-1531`. То же, что LIGHTING, но **без** финального
`dot(N,·)` — используется для биллбордов, у которых нет осмысленной нормали:
```glsl
vec3 Lraw = uTexParam0_0.xyz - P;
float dist = length(Lraw);
float atten = max(1.0 - uTexParam0_0.w * dist, 0.0);
vec3 uv_rgb = uTexParam0_1.rgb * atten;   // без dot(N,L) и без домножения на dist
```

### 3.8 `CRC_TEXGENMODE_CONSTANT`

`shaders/VP.xrg:1659-1662`. Параметров: 1 `vec4`, читается напрямую:
```glsl
v_TexN = uTexParamN_0;   // MOV @O_TEXn, c[$TEXPARAMn+0] — единственный режим, не проходящий через общий хвост §3.16
```

### 3.9 `CRC_TEXGENMODE_VOID`

Внешний гейт `*ifnot_texgenN_void { ... }` (например `shaders/VP.xrg:1230,1682`
для N=0/1) — если режим VOID, то **весь внутренний блок для этого канала не
вставляется вообще**: выходная переменная `v_TexN` просто не пишется в этом
draw-call (в GLSL-порте эквивалент — не иметь этот varying/не писать в него;
поведение "оставить как есть" не воспроизводимо в GLSL и не нужно — если канал
`VOID`, он в принципе не используется дальше по конвейеру).

### 3.10 `CRC_TEXGENMODE_LINEARNHF`

`shaders/VP.xrg:1386-1416`, только канал 0. Параметров: 3 `vec4`. **Важно**:
итог пишется не в `O_TEX0`, а в `O_COL0` (`shaders/VP.xrg:1415`) — это не
генератор текстурных координат, а способ запечь приближённое освещение прямо
в цвет вершины, использующий "texgen"-слот как транспорт для данных. Вычисленный
промежуточный фактор (`R3.x`) в показанном коде не участвует в итоговом
`MOV @O_COL0, c[$TEXPARAM0+2]` — итоговый цвет **константа** (`$TEXPARAM0+2`).
Похоже на недоделанную/сокращённую фичу — при портировании воспроизводите
буквально (константный цвет), не пытайтесь "оживить" неиспользуемый расчёт.

### 3.11 `CRC_TEXGENMODE_BOXNHF`

`shaders/VP.xrg:1418-1449`, только канал 0. Параметров: 10 `vec4`
(`+0..+7` = 8 цветов углов "коробки" точечного пробника освещения, `+8` = мин.
угол коробки в модельном пространстве, `+9` = обратный размер коробки). Тоже
пишет в `O_COL0`, не в текстурную координату:
```glsl
vec3 t = clamp((P - uTexParam0_8.xyz) * uTexParam0_9.xyz, 0.0, 1.0); // локальные UVW внутри бокса
vec4 c00 = mix(uTexParam0_0, uTexParam0_1, t.x);
vec4 c10 = mix(uTexParam0_2, uTexParam0_3, t.x);
vec4 c0  = mix(c00, c10, t.y);
vec4 c01 = mix(uTexParam0_4, uTexParam0_5, t.x);
vec4 c11 = mix(uTexParam0_6, uTexParam0_7, t.x);
vec4 c1  = mix(c01, c11, t.y);
vec4 result = mix(c0, c1, t.z);
// O_COL0 = result;  (перезаписывает то, что записала секция *if_lighting/*if_coloroutput!)
```
Трилинейная интерполяция по 8 угловым "пробникам" (как мини light-probe box).
И `LINEARNHF`, и `BOXNHF` **исполняются в файле после блока освещения** — если
оба активны одновременно, они безусловно затирают `O_COL0` (см. §7, риск).

### 3.12 `CRC_TEXGENMODE_LIGHTFIELD`

`shaders/VP.xrg:1550-1563` (канал 0), 6 `vec4` (`+0..+5` = цвета для −X,+X,−Y,+Y,−Z,+Z
полусфер по нормали). Пишет в обычный `O_TEXn` (не в цвет):
```glsl
vec3 pos3 = max(N, 0.0);
vec3 neg3 = max(-N, 0.0);
vec3 rgb = neg3.x*uTexParamN_0.rgb + pos3.x*uTexParamN_1.rgb
         + neg3.y*uTexParamN_2.rgb + pos3.y*uTexParamN_3.rgb
         + neg3.z*uTexParamN_4.rgb + pos3.z*uTexParamN_5.rgb;
rgb *= 2.0;
v_TexN = vec4(rgb, 1.0);
```
Классический ambient-cube (6 граней), взвешенный компонентами нормали.

### 3.13 `CRC_TEXGENMODE_SHADOWVOLUME` / `SHADOWVOLUME2`

Оба — только канал 0, оба **модифицируют саму позицию `P` (R8)**, а не текстурную
координату — это единственные "texgen"-режимы, которые физически двигают
геометрию (экструзия стенсильного объёма тени).

`SHADOWVOLUME` (`shaders/VP.xrg:690-701`), 1 `vec4` (`LightPos.xyz`, `.w`=дистанция экструзии):
```glsl
vec3 dir = normalize(P - uTexParam0.xyz);
float facing = (dot(dir, N) < 0.0) ? 1.0 : 0.0;   // силуэт: грань не смотрит на свет
P += dir * facing * uTexParam0.w;
```

`SHADOWVOLUME2` (`shaders/VP.xrg:737-764`), 1 `vec4` (`HingePos.xyz`,`.w`=макс.дистанция затухания),
плюс модулируется потоковым скаляром `TEXINPUT0` (авторская "сила экструзии на вершину"):
```glsl
vec3 dir = P - uTexParam0.xyz;
float len = length(dir);
dir = normalize(dir);
float falloff = max(uTexParam0.w - len, 0.0);
float weight = TextureTrans0 ? (inUV0.x * transScale0.x + transOffset0.x) : inUV0.x;
P += dir * (falloff * weight);
```

### 3.14 `CRC_TEXGENMODE_DECALTSTRANSFORM`

`shaders/VP.xrg:2252-2273`, только канал 2. **Аномалия**: несмотря на то, что
это единственный texgen-режим канала 2, код читает `c[$TEXPARAM0+0]` и
`c[$TEXPARAM0+1]` — **константы канала 0**, а не `$TEXPARAM2` (опечатка/легаси
в шаблоне, либо намеренное разделяемое хранилище — не проверено, но
воспроизводить буквально при 1:1 порте). Параметры фактически прочитанные: 2
`vec4` (по комментарию enum, `MRender_Classes.h:568`: "Texparam0..1 are
transformed to tangent-space and used as tangents"). Вход: `TU`,`TV`.
```glsl
vec3 rowA = uTexParam0_0.xyz;  // да, канал 0, не 2 — см. примечание выше
vec3 rowB = uTexParam0_1.xyz;
vec3 t3 = -vec3(dot(rowB, TU), dot(rowA, TU), 0.0);
t3 = normalize(t3);
vec3 t4 = vec3(dot(rowB, TV), dot(rowA, TV), 0.0);
t4 = normalize(t4);
// v_Tex3 = vec4(t4, ?)   -- пишет в канал 3 (O_TEX3), не в свой собственный канал 2!
```
Ещё одна аномалия: результат уходит в `O_TEX3`, а не `O_TEX2` — блок целиком
не пишет в "свой" канал. Транслируйте побайтово при первой реализации, чтобы
не гадать — это явно нестандартный, возможно, полу-хардкодный кусок под
конкретный game-side шейдер декалей.

### 3.15 `CRC_TEXGENMODE_TSREFLECTION`

Канал 0 (`shaders/VP.xrg:1576-1595`, помечено `// Not tested, only on texcoord 0`)
и каналы 1/2/3 (например `shaders/VP.xrg:2205-2220` для канала 2 — другая
формула, см. ниже). Т.е. **две разные реализации под одним именем режима** в
зависимости от канала:

Канал 0 (view-space reflection, спроецированный в касательный базис):
```glsl
vec3 V = ToView(P); vec3 Nv = ToView3x3(N);
float d = dot(-V, Nv) * 2.0;
vec3 Rr = V + Nv * d;                  // как в §3.4, но дополнительно проецируем в TS:
vec3 uv = vec3(dot(TU, Rr), dot(TV, Rr), dot(N, Rr));
```
Каналы 1/2/3 (модельно-пространственная разница двух точек параметра, без вида камеры):
```glsl
vec3 d0 = normalize(uTexParamN_0.xyz - P);
vec3 d1 = normalize(uTexParamN_1.xyz - P);
vec3 sum = d0 + d1;
vec3 uv = vec3(dot(TU, sum), dot(TV, sum), dot(N, sum));
```
Параметров: 2 `vec4` (см. расхождение с таблицей размеров, §2.3).

### 3.16 `CRC_TEXGENMODE_BUMPCUBEENV`

Каналы 1, 2, 5 (`shaders/VP.xrg:1965-1998` пример для канала 1). Вход: `P`,`N`,`TU`,`TV`.
Параметров: 8 `vec4` (`+0..+2` = 3 строки плоскости/куб-проекции, `+3` = масштаб,
`+7` = опорная точка; `+4..+6` не используются кодом — вероятно padding/выравнивание).
```glsl
vec3 d = vec3(dot(uTexParamN_0, vec4(P,1)),
              dot(uTexParamN_1, vec4(P,1)),
              dot(uTexParamN_2, vec4(P,1)));
d = uTexParamN_7.xyz - d;
// далее спроецировать d в касательный базис по трём строкам ещё раз:
vec3 a = vec3(dot(N,  uTexParamN_0.xyz), dot(TV, uTexParamN_0.xyz), dot(TU, uTexParamN_0.xyz));
vec3 b = vec3(dot(N,  uTexParamN_1.xyz), dot(TV, uTexParamN_1.xyz), dot(TU, uTexParamN_1.xyz));
vec3 c = vec3(dot(N,  uTexParamN_2.xyz), dot(TV, uTexParamN_2.xyz), dot(TU, uTexParamN_2.xyz));
a *= uTexParamN_3.xyz; b *= uTexParamN_3.xyz; c *= uTexParamN_3.xyz;
// v_TexN+1 = vec4(b, ?)
// v_TexN+2 = vec4(c, ?)
```
(Полная реконструкция w-компонент из `.w = d.x/y/z` компонент неочевидна из
куска ассемблера — при реализации сверяться с точной последовательностью в
`shaders/VP.xrg:1965-1998`/`2221-2250`/для канала 5.) Используется совместно с
кубической текстурой, спроецированной параллакс-коррекцией по позиции
относительно опорной точки `+7`.

### 3.17 `CRC_TEXGENMODE_MSPOS`

`shaders/VP.xrg:1644-1647` (и на каждый канал). Тривиально:
```glsl
v_TexN = vec4(P, 1.0);   // модельное пространство "как есть"
```

### 3.18 Остальные режимы (кратко)

- **`TEXCOORD`** (0) — passthrough входного UV канала, опционально через
  `$TEXTURETRANSx` (scale+offset для упакованных форматов) и/или `$TEXMATRIXx`
  (полная 4×4). `shaders/VP.xrg:1237-1346`.
- **`LINEAR`** (1) — `uv = TexParam-матрица (4×4) × vec4(P,1)` (4 `DP4`).
  `shaders/VP.xrg:1351-1357`. Уже реализовано в бэкенде.
- **`SCREEN`** — недостижимый по содержимому (GameCube-мусор), см. §1.4.
- **`TANG_U`/`TANG_V`** — `uv.xyz = TU` / `uv.xyz = TV`. `shaders/VP.xrg:1456-1464`.
- **`NULL`** — `v_TexN.x = 0.0` (явный ноль, канал при этом присутствует и
  пишется, в отличие от `VOID`). `shaders/VP.xrg:1232-1235`.
- **`DEPTHOFFSET`** — параллакс-подобное смещение UV вдоль направления на
  камеру, спроецированного в касательный базис (2 `vec4`: позиция источника
  смещения и `(offsetU,offsetV)`; формула — `shaders/VP.xrg:1618-1641`):
  ```glsl
  vec3 toEye = normalize(uTexParamN_0.xyz - P);
  vec2 e = vec2(dot(toEye, TU), dot(toEye, TV));
  float invN = 1.0 / dot(toEye, N);
  vec2 offsetUV = e * uTexParamN_1.xy;
  uv = baseUV + offsetUV * invN;   // baseUV = TextureTransN(inUV) или inUV напрямую
  ```
- **`VSPOS`** — `uv.xyz = ToView(P)`, `.w = 1`. `shaders/VP.xrg:1650-1657`.
- **`WSPOS`** — мёртв (см. §1.4).

---

## 4. Вершинное освещение (`*if_lighting`)

Блок `shaders/VP.xrg:793-1061`. Активен только при определённом
`CRC_SUPPORTVERTEXLIGHTING` (не на PS3, актуально для PC/нашего порта) и
установленном `FMTBF0_VERTEXLIGHTING`.

### 4.1 Раскладка констант на источник — **2 `vec4` на источник**, подтверждено кодом

`SetRegisters_Lights`, `Source/P5/Shared/MOS/Classes/Render/MRenderVPGen.h:1040-1136`,
`nRegsNeeded = _nLights*2`. Слот `i` занимает `$LIGHT+2i` / `$LIGHT+2i+1`:

| Тип (`L.m_Type`) | `$LIGHT+2i` (слот 0) | `$LIGHT+2i+1` (слот 1) |
|---|---|---|
| `CRC_LIGHTTYPE_POINT` | `Pos.xyz`, `.w = InvRange` (`m_Attenuation[1]`) | `Color.rgba / 255` |
| `CRC_LIGHTTYPE_PARALLELL` | `Dir.xyz`, `.w = 0` | `Color.rgba / 255` |
| Ambient (`type==3`) | `(0,0,0,0)` — не используется | `Color.rgba / 255` |

Максимум источников — `CRC_MAXLIGHTS = 8` (`MRender_Classes_VPUShared.h:29`).
Тип каждого слота **дополнительно** кодируется в самом define
(`LIGHT<i>_POINT`/`_AMBIENT`/`_PARALLELL`, `CreateVP`, `MRenderVPGen.cpp:596-619`),
т.е. в отличие от фрагментного шейдера с рантайм-веткой, здесь **под каждую
комбинацию типов генерируется свой вариант программы** (в GLSL-порте можно либо
тоже специализировать по define'ам/veriant-ключу, либо (проще для GLES3) сделать
единый `uniform int uLightType[8]` и ветвиться в цикле — ценой немного другой
модели кэширования шейдеров, чем в оригинале).

### 4.2 Формула накопления (транслитерация буквально, не "исправлять")

`shaders/VP.xrg:793-1037`. `R10` — аккумулятор, старт `(0,0,1,1)`... на деле
`MOV R10.xyzw, c[$BASE+8].xxxy` → `R10 = (0,0,0,1)` (`c8.x=0,c8.y=1`, маска
`.xxxy` берёт `x,x,x,y` = `0,0,0,1`). Источник 0 **перезаписывает** `R10.rgb`
(`MOV`), источники 1-7 **накапливают** (`MAD`/`ADD`).

Point-light (пример источник 0, `shaders/VP.xrg:802-816`; 1-7 идентичны с
поправкой на индекс регистра):
```glsl
vec3 Lraw   = uLight[2*i].xyz - P;
float dist2 = dot(Lraw, Lraw);
float invDist = inversesqrt(dist2);
float dist  = dist2 * invDist;                    // == 1/invDist, транслитерация ARB DST
float atten = max(1.0 - uLight[2*i].w * dist, 0.0); // линейное (не квадратичное!) затухание по InvRange
float scale = dist2 * atten;                        // ВНИМАНИЕ: буквально dist², не 1/dist² — так в оригинале
vec3 Lscaled = Lraw * scale;
float ndotl = max(dot(N, Lscaled), 0.0);
diffuse = ndotl * uLight[2*i+1].rgb;   // MOV для i==0, MAD (+=) для i>0
```
Это **не** канонiчная physically-based формула (нет чистого `1/dist²`) —
переносите как есть, чтобы сохранить визуальный вид оригинала; при желании
"починить" — только по явному запросу и со сравнением скриншотов.

Ambient (`shaders/VP.xrg:821-824`, `954-987`): `diffuse (+)= uLight[2*i+1].rgb`
(без нормали и без position/direction).

Parallel (`shaders/VP.xrg:829-833`, `992-1032`): `diffuse (+)= max(dot(N, uLight[2*i].xyz),0)`... — **внимание**, в шаблоне здесь нет `MAX` перед умножением на цвет (в отличие от point!), т.е. буквально:
```glsl
float ndotl = dot(N, uLight[2*i].xyz);   // без clamp к [0..)
diffuse = ndotl * uLight[2*i+1].rgb;     // может уйти в отрицательный вклад при MAD-накоплении
```
Финальный клэмп всего аккумулятора (`*clamplight`, `shaders/VP.xrg:1034-1037`):
`diffuse = max(diffuse, 0.0)` — применяется **один раз в конце**, после сложения
всех источников, так что отрицательный вклад одного parallel-источника способен
"съесть" положительный вклад других до финального клэмпа — переносить как есть.

### 4.3 Цвет вершины, фог, итоговый вывод

`shaders/VP.xrg:1039-1093`:
```glsl
if (colorvertex) diffuse *= inColor;              // *if_colorvertex, только внутри if_lighting/coloroutput
if (fogdepth) {
    float viewZ = ToView(P).z;                     // DP4 c[BASE+6]
    float f = viewZ * uFogDepth.y - uFogDepth.y;    // буквально так — .x (fogEnd*invRange) не используется в этой ветке, см. примечание ниже
    f = clamp(f, 0.0, 1.0);
    f = 1.0 - f;
    diffuse.a *= f;                                 // фог модулирует только альфу, не RGB!
}
outColor = diffuse * uConstantColor;                // если lighting
// или (если !lighting):
outColor = (colorvertex ? inColor * uConstantColor : uConstantColor);
// + тот же fog-блок на alpha
// если !coloroutput — вообще ничего не пишется в выходной цвет
```
Примечание про фог: код использует только `$FOGDEPTH.y` (`invRange`), **не**
`$FOGDEPTH.x` (`fogEnd*invRange`), хотя последний специально считается на
CPU-стороне (`SetRegisters_Attrib`, `MRenderVPGen.h:589-590`). Формула по факту
эквивалентна `f = invRange*(viewZ - 1)`, что не похоже на стандартное линейное
`(fogEnd - viewZ)*invRange`, если не считать это намеренной оптимизацией под
специфическую систему координат. Транслитерировать буквально и сверить визуально
при появлении первого тумана в игре — вероятная точка расхождения с оригиналом
при "интуитивном" переписывании формулы.

---

## 5. Matrix palette (скиннинг)

### 5.1 Форматы входных вершинных потоков

Подтверждено в `Source/P5/Shared/MOS/MSystem/Raster/MRender_Classes.h:1150-1161`:

| Поток | Формат | Регистр `@V_*` |
|---|---|---|
| `MatrixIndex0` | `CRC_VREGFMT_N4_UI8_P32_NORM` — 4 байта, каждый нормализован в `[0..1]` (т.е. `byte/255`) | `@V_MI` (`CRC_VREG_MI0=12`) |
| `MatrixWeight0` | до 4 `float` (`V1..V4_F32`) | `@V_MW` (`CRC_VREG_MW0=13`) |
| `MatrixIndex1` | тот же формат, для костей 4-7 | `@V_MI2` (`CRC_VREG_MI1=14`) |
| `MatrixWeight1` | до 4 `float`, для костей 4-7 | `@V_MW2` (`CRC_VREG_MW1=15`) |

### 5.2 Декодирование индекса кости в номер регистра константы

`shaders/VP.xrg:214-240` (пример `MWComp1`):
```
MUL R3, @V_MI, c[$BASE+8].w;   // c8.w = 3×255.0001 (не-quat путь) — байт [0..1] × 3×255 ≈ индекс кости × 3
@ARL A0.x, R3.x;               // округление вниз до int → адрес регистра = boneIndex*3
MOV r4, c[A0.x+$MP+0]; ...     // читаем 3 строки 4×3-матрицы кости начиная с $MP + A0.x
```
Т.е. байтовый индекс кости `b ∈ [0..255]` кодируется как нормализованный float
`b/255`, а множитель `c8.w = 3×255.0001` (с запасом `0.0001`, чтобы после
округления `floor()` не терять последнюю кость из-за погрешности float) сразу
даёт **адрес регистра** (`boneIndex × 3`, т.к. каждая матрица кости занимает 3
`vec4`-регистра — 4×3 транспонированная матрица, без последней строки
`(0,0,0,1)`). GLSL-эквивалент:
```glsl
int boneReg = int(floor(a_MatrixIndex0.x * 255.0 + 0.5)) * 3;  // либо: используйте отдельный
                                                                 // немасштабированный int-атрибут индекса
                                                                 // (избегайте round-trip через norm-byte в GLSL)
mat4x3 M = mat4x3(uMP[boneReg], uMP[boneReg+1], uMP[boneReg+2]); // или напрямую uBoneMatrices[boneIndex]
```
**Для порта на GLSL ES естественнее не воспроизводить byte-normalized round-trip
и плоскую индексацию по `c[]`, а завести обычный `int`/`uint` атрибут индекса
кости и `uniform mat4 uBoneMatrices[MAX_BONES]` (или `mat4x3`, если хотим
экономить)** — арифметика `A0.x + $MP + n` из ARB — это подгонка под плоский
константный файл, не обязательное свойство алгоритма.

### 5.3 Число костей на вершину — `MWComp0..8`

`GetMWComp()` — 4 бита из `FMTBF0` (`MRenderVPGen.h:156-159`), задаётся вызовом
`CRC_VPFormat::SetMWComp` (`MRenderVPGen.h:531-534`), реально видно как
`VPFormat.SetMWComp(m_ContextGeometry.GetMWComp())` в
`Source/P5/Shared/MOS/RenderContexts/PS3GCM/MRenderPS3_Attrib.cpp:916`.

| `MWComp` | Что происходит (`shaders/VP.xrg`) | Веса |
|---|---|---|
| `0` | Нет скининга вообще, `P = PosTrans(rawPos)` напрямую (:186-212) | — |
| `1` | 1 кость, вес не нужен (вес=1 неявно) — не найден отдельным блоком `MWComp1` с одной костью явно (см. код :214-240, содержит `V_MW.x` — т.е. даже "1" реально уже использует вес из потока; иначе был бы отдельный особый случай) | `V_MW.x` |
| `2` | 2 кости, веса `V_MW.x`,`V_MW.y`, накопление `MUL`+`MAD` (:242-272) | `V_MW.xy` |
| `3` | 3 кости (:274-409, с ответвлением на мёртвый `MPQuat`) | `V_MW.xyz` |
| `4` | 4 кости, все веса из первого потока (:411-449) | `V_MW.xyzw` |
| `5` | 5 костей — веса первых 4 из `V_MW`, 5-я — из **второго** индекса/веса потока `V_MI2`/`V_MW2.x` (:451-495) | `V_MW.xyzw` + `V_MW2.x` |
| `6` | 6 костей — `V_MW2.xy` (:497-545) |
| `7` | 7 костей — `V_MW2.xyz` (:547-599) |
| `8` | 8 костей — `V_MW2.xyzw`, оба потока индексов/весов полностью (:601-657) |

Во всех случаях (кроме `0`) каждая дополнительная кость декодируется той же
схемой `ARL(V_MI[.x/.y/.z/.w] × c8.w) → адрес → c[A0.x+$MP+0..2]`, затем
`MUL`/`MAD` с соответствующим весом из `V_MW`/`V_MW2`, результат накапливается в
регистрах `R0,R1,R2` (три строки итоговой смешанной 4×3-матрицы), которые потом
применяются к позиции (`DP4 R8.{xyz}, R{0,1,2}, R4_pos`) и (если `usenormal`) к
нормали (`DP3`, `shaders/VP.xrg:659-682`) и к тангентам (`shaders/VP.xrg:1184-1219`).
**Веса не нормализуются движковым VP-кодом** (нет деления суммы весов на 1) —
предполагается, что автор контента уже поставляет нормализованные веса.

Квaтернионный вариант (`MPQuat`, `shaders/VP.xrg:276-353`) — **мёртв** для нас
(см. вступление), хранит на кость `2 vec4`: `k[0..3]` кватернион вращения +
`Mat.k[3][0..3]` перенос; разворачивается в матрицу прямо в шейдере формулой
из комментария в шапке файла (`shaders/VP.xrg:72-111`, стандартный quat→mat3).
Не реализовывать, если не появится конкретная необходимость (see §7).

---

## 6. Трансформы позиции и UV (`$POSTRANS`, `$TEXTURETRANSx`)

Оба используют один и тот же примитив: `raw × Scale + Offset`, `Scale`/`Offset`
— по `vec4` каждый (`CRC_VRegTransform`, `Source/P5/Shared/MOS/MSystem/Raster/MRender_Classes.h:1075-1080`).
Включается битом `FMTBF0_VERTEXINPUTSCALE` (позиция) / `FMTBF0_TEXCOORDINPUTSCALE`-маской
по каналу (UV), выставляется через `SetVertexTransform()`/`SetTexCoordInputScale(i)`
(`MRenderVPGen.h:477-485`), нужен когда вершинный формат хранит позицию/UV в
компактном целочисленном виде (например `N4_UI8_P32_NORM`, `I16` и т.п. — см.
`CRC_VREGFMT_*`, `MRender_Classes.h:994+`) вместо `f32`, и требуется деквантизация
на GPU:

```glsl
// Позиция (POSTRANS), shaders/VP.xrg:151-152 (пример под CubeVec, идентично во всех MWCompN)
vec3 P_raw = a_Position;                 // может быть I16/N4_UI8 и т.п., уже приведено к float атрибутом
vec3 P = P_raw * uPosTrans.Scale.xyz + uPosTrans.Offset.xyz;

// UV (TEXTURETRANSx), например shaders/VP.xrg:1304-1305 (texcoord, без texmatrix)
vec4 uv_raw = a_UVn;
vec4 uv = uv_raw * uTexTransN.Scale + uTexTransN.Offset;
```
Если и `$TEXMATRIXx`, и `$TEXTURETRANSx` активны одновременно — сначала
scale+offset, потом умножение на 4×4 матрицу (`shaders/VP.xrg:1241-1251`):
`uv = TexMatrixN × (uv_raw × Scale + Offset)`.

Особый случай — `CubeTex`-путь (`shaders/VP.xrg:1266-1340`, **мёртвый**, см.
§1) использует `TextureTrans0` вместе с данными из матрицы палитры (`c[A0.x+$MP+1]`)
для проекции UV на грань куба с адаптивным масштабом (`CubeTexScl2`/`CubeTexSclZ`)
— не переносить без явной необходимости.

---

## 7. Итог: приоритет портирования

| Фича | Сложность на GLSL ES | Блокирует |
|---|---|---|
| `TEXCOORD`, `LINEAR` | Готово | — (уже реализовано) |
| `NULL`, `VOID`, `CONSTANT`, `MSPOS`, `VSPOS` | Тривиальная (0-1 строка) | Ничего — быстрые победы, покрывают много статичного геометрического контента |
| `NORMALMAP`, `TANG_U`, `TANG_V` | Тривиальная | Материалы с bump/normal-mapping без дополнительных трансформов |
| Matrix palette (`MWComp1..8`) | Средняя — переиндексировать байтовый normalized-индекс в `int`/`uint` атрибут + `uniform mat4[]`/UBO костей; веса не нормализуются, переносить как есть | **Блокирует всю анимированную геометрию персонажей/оружия** (`RIDDICK_SKIP_SKINNED` в текущем коде) — самый высокий приоритет после базовых режимов |
| `ENV`, `REFLECTION` | Средняя — нужен `ToView()`-хелпер (по сути дублирует существующую матрицу вида) | Материалы окружения/хром на оружии и BSP-декорациях |
| Вершинное освещение (`*if_lighting`, все `Light<i>_*`) | Средняя-высокая — нужно решить, специализировать ли шейдер по комбинации типов света (как оригинал) или писать общий цикл по `uniform`-массиву; нетривиальная (не физическая) формула затухания требует точной транслитерации, не "исправления" | Статичная геометрия без lightmaps/предпросчитанного освещения (часть BSP-объектов, динамические пропы) |
| `LIGHTING`/`LIGHTING_NONORMAL` (texgen-вариант) | Низкая-средняя (переиспользует ту же формулу затухания, что и §4) | Отдельные материалы, помечающие диффуз как "текстурную координату" — вероятно старый/редкий путь |
| `TSLV` | Средняя — нужен корректный касательный базис (see `usetangents`) с учётом скининга тангентов | **FP20-освещение фрагментного шейдера завязано на TSLV** (по брифу задачи) — второй по приоритету после скининга, если материалы используют bump+point light |
| `PIXELINFO` | Средняя — пишет в 3 последовательных texcoord-канала, нужно синхронизировать номера `varying` с генератором | Материалы, использующие полный tangent-space пиксельный пайплайн (декали/parallax) |
| `DEPTHOFFSET` | Средняя | Параллакс-материалы (вероятно немного контента) |
| `LIGHTFIELD` | Средняя (ambient cube, 6 параметров) | Контент со "light probe" освещением вместо реальных источников |
| `LINEARNHF`, `BOXNHF` | Низкая по формуле, но **пишут в цвет вершины, а не в UV**, и в LINEARNHF финальный расчёт похоже не используется (см. §3.10) — риск, что это недоделанная/мёртвая в геймплее фича; проверить, встречается ли она вообще в реальном контенте прежде чем вкладываться | Низкий — вероятно единичные легаси-материалы |
| `TSREFLECTION`, `BUMPCUBEENV` | Высокая — две разные формулы под одним именем в зависимости от канала (`TSREFLECTION`), неполные данные по `.w`-компонентам (`BUMPCUBEENV`) реконструированы приблизительно, требуют сверки на реальном материале с известным результатом | Специфичные chrome/bump-cube материалы, вероятно немного объектов |
| `SHADOWVOLUME`, `SHADOWVOLUME2` | Средняя, но это **не texgen**, а экструзия геометрии — нужно решить архитектурно, где в GLES3-конвейере это делать (отдельный проход рендера теней, не обычный материал) | Стенсильные тени — согласно `CLAUDE.md`, "критичная фича Riddick"; закладывать архитектурно заранее, реализовывать после базового освещения |
| `DECALTSTRANSFORM` | Средняя, но **со странностями в индексации регистров/выходного канала** (читает `TEXPARAM0`, пишет `O_TEX3` при заявленном канале 2) — обязательно сверить с дампом реального шейдера/скриншотом перед доверием этой транскрипции | Декальные материалы, вероятно небольшой объём контента |
| `SCREEN`, `ENV2`, `LINEAR_MS`, `WSPOS` | Не применимо — мёртвый/недостижимый код в шаблоне (см. §1.4) | Ничего — не реализовывать, только если найдётся живой материал, ссылающийся на `ENV2` (тогда нужен новый код, не портирование существующего) |
| `CubeVec`/`CubeTex`/`MPQuat` (весь семейство) | Не применимо — недостижимо (`CRC_SUPPORTCUBEVP`/`CRC_QUATMATRIXPALETTE` никогда не определены) | Ничего |

### Общее наблюдение по риску

Формат констант в `VP.xrg`/`CRC_VPGenerator` — это плоский ARB-регистровый файл
с ручной адресной арифметикой (`$BASE`, `$MP`, `A0.x`), заточенный под старые
GPU с ограниченным числом константных регистров. Для GLES3 естественнее **не**
пытаться воспроизводить это как единый `c[]`-массив, а превратить каждую
логическую сущность в свой `uniform`/UBO-член — единственные места, где
индексация по массиву действительно нужна семантически, это матрицы палитры
костей (§5) и массив источников света (§4). Самый большой источник риска
регрессии — не сама математика (она вся вычислена и процитирована выше), а
**молчаливые нестыковки в оригинальном шаблоне** (три показанных выше: TSLV
порядок компонент расходится с комментарием enum; DECALTSTRANSFORM читает не
свой канал параметров и пишет не в свой выходной канал; таблицы размеров
`ms_lnTexGenParams`/`GetTexGenModeAttribSize` расходятся с фактическим кодом
для `TSREFLECTION`/`LIGHTFIELD`) — их следует переносить **буквально**, а не
"чинить", чтобы не разойтись с визуальным видом оригинальной PC/PS3-версии.
