# Формат .XW миров PC-версии (Dark Athena remaster) — что накопано реверсом

Справка по расхождениям между мирами PC-версии (*Assault on Dark Athena*, 2009;
включает EFBB-кампанию) и тем, что ожидает загрузчик PS3-снапшота движка.
Реверс делался по реальному `Content/Worlds/Pa1_Intro.XW` (см. журнал в CLAUDE.md,
коммиты «BSP: …»). Все значения little-endian.

## 1. Контейнер: MOS DATAFILE2.0

`.XW` — это `CDataFile` v2 (`MCC/MDataFile.*`): дерево нод по **48 байт**:

```
char name[24]; int32 OffsetNext, OffsetSubDir, OffsetData, Size, UserData, UserData2;
```

- `UserData` — обычно число элементов чанка, `UserData2` — версия формата элемента.
- Корень: magic `"MOS DATAFILE2.0"`, в `UserData` — число нод (0 = посчитать обходом).

Дерево уровня (упрощённо): `BSPMODELS` → список `BSPMODEL` (первый — мир для
`CXR_Model_BSP2`, остальные — детальные/физические модели для BSP1/BSP3/BSP4);
внутри модели: `VERSION, BSPTYPE, PLANES, VERTICES, VERTEXINDICES, FACES,
FACEINDICES, [EDGES/EDGEFACES], BSPNODES, MAPPINGS, SURFACES, MEDIUMS, SOLIDS…,
LIGHTMAP*` и пр.

## 2. Версии

| Что | PS3/консоль | PC-ремастер | Примечание |
|---|---|---|---|
| `VERSION` (ud) файла модели | ≤ 0x0123 | **0x0124** | снапшот декларирует `XW_VERSION 0x0124`, но фактические PS3-структуры соответствуют 0x0123; ветки `if (XWVersion == 0x0123)` в лоадерах — 16-битные vertex-индексы |
| `FACES` (ud2) | 0x0203 | **0x0204** | см. §3 |
| `BSPNODES` (ud2) | 0x0202 | 0x0202 (!) | тег тот же, укладка другая — см. §4 |
| `CImage_FileHeader` (текстуры) | 0x0300 | 0x0400 | +m_ChunkSize/m_ChunkCount, fixup под флагом 0x4000 (сделано ранее, Ghidra MSystem.dll) |

## 3. FACES v0x0204 — 40 байт/фейс

Раскладка = **0x0203 (32 байта) + два хвостовых uint32**:

```
int32  iiEdges;            // == iiVertices при наличии рёбер; -1 если нет
uint32 VertexIO;           // iiVertices:24 | nVertices:8
uint32 iMapping;
uint16 iSurface;  uint16 Flags;
uint32 iPlane;
uint32 iiNormals;
uint16 iBackMedium; uint16 iFrontMedium;
uint32 LightInfoIO;        // iLightInfo:28 | nLightInfo:4
uint32 extra[2];           // НОВОЕ в 0x0204: индексы в LIGHTMAPRECTS /
                           // SPLINE_LIGHTMAPMAPPINGS (атлас лайтмап PC-рендера);
                           // заполнены у фейсов с флагом 0x1000; снапшотом не используются
```

Проверка полей по данным: `iiEdges == iiVertices`, `iSurface <` числа SURFACES
модели, счётчик в старших 4 битах LightInfoIO. Поддержка добавлена во все
пять ридеров: `CBSP_CoreFace::Read`, `CBSP2_CoreFace::Read`,
`CBSP3_CoreFace::Read`, `CBSP4_CoreFace::Read` (кейс 0x0204 = 0x0203 + skip 8),
а также `CXR_Model_BSP4Glass::ReadFaceMapping` (WBSP4GlassLoader.cpp) — glass
из FACES нужен только `m_iMapping`, хвост из двух uint32 скипается после
каждого фейса. Поведение подтверждено декомпилем MXR.dll (FUN_1019ec60,
`MXR_dll_decomp.c:277722`): case 0x204 читает 0x28 (40) байт/фейс, `iMapping`
(uint16) берётся по смещению 8, хвост игнорируется.

Попутный баг снапшота: ветки 0x0203/0x0202 в BSP1 не присваивали считанный
`m_iFrontMedium` — исправлено.

## 4. BSPNODES: PC пишет raw `CBSP2_Node` (24 байта), тег версии тот же 0x0202

**Главная ловушка**: у консольных файлов `BSPNODES` v0x0202 — raw-дамп
"старой" ноды (`CBSP_Node`: plane первым, u16-линки); у PC — raw-дамп
**CBSP2_Node** (`XRModels/Model_BSP2/XW2Common.h`), при том же ud2=0x202:

```
uint32 iNodeFront / nFaces;    // union: node / leaf
uint32 iNodeBack  / iMedium;
uint32 iNodeParent;            // 0 = root
uint32 NodeIO;                 // iiFaces:24 | Flags:8  (Flags: 1=PORTAL, 2=STRUCTURE)
uint32 iPlane;                 // 0 => лист
uint16 iPortalLeaf; uint16 pad;
```

Валидировано на всех 52970 нодах мира Pa1_Intro (0 несоответствий: диапазоны
детей/родителя/плоскости; у листьев `iiFaces+nFaces` в пределах FACEINDICES,
`iMedium <` MEDIUMS).

Следствия по загрузчикам:
- **BSP2** (мир, `$WORLD:*`): его fast path читает raw `CBSP2_Node` — **работает как есть**;
- **BSP4**: всегда по-элементный `CBSP4_Node::Read`, его кейс 0x0202 читает
  ровно эту укладку — **работает как есть**;
- **BSP1, BSP3**: in-memory нода u16-полей с plane первым; их fast path читал
  raw мимо укладки → мусорные линки → бесконечная рекурсия
  (`ExpandFaceBoundBox_r`, цикл 1→13→…→1). Добавлен конвертер
  (при `XWVersion >= 0x0124`): W[0..5] → u16-поля, `iiFaces = W[3]&0xffffff`,
  `Flags = W[3]>>24`, `iPortalLeaf = W[5]&0xffff`. Bound-пары (`m_Bound`) в
  PC-файлах нет — остаётся 0.

Признак различения: **версия файла модели** (`VERSION` ud ≥ 0x0124 → PC-укладка),
т.к. ud2 чанка одинаковый.

## 4.1 Подтверждение декомпиляцией PC (MXR.dll, CXR_Model_BSP4::Create)

Декомпиляция PC-загрузчика BSP4 (FUN_101c4120 в MXR.dll) подтвердила:
- FACES и BSPNODES читаются **по-элементно версионными ридерами**
  (`Read(file, ud2)`), in-memory нода BSP4 = 16 байт (4×u32) — как у нас;
- ветка `XWVersion == 0x123` для 16-битных VERTEXINDICES/FACEINDICES — как у нас;
- lightmap-чанки в BSP4 PC тоже только предупреждение + пропуск
  («WARNING: CXR_Model_BSP4 with lightmap data» — штатно и в оригинале);
- **новое в PC**: опциональные чанки `OCTAAABBTREE` + `OCTAAABBHEADER` —
  квантованное AABB-октодерево (bounds + масштабы 65535/(max-min)) для
  ускорения коллизий. У нас не реализовано и не требуется: чанки опциональны,
  движок работает по обычному BSP-пути. Если займёмся — реверсить
  FUN_101c3a70 (чтение дерева) и FUN_100c1920 (заголовок).

## 5. Известные «не-BSP» расхождения PC-контента (для полноты)

- `Worlds/` PC-набора EFBB не содержит bootstrap-мира `campaign.xw` —
  кампания стартует сразу `Pa1_Intro` (`startnewcampaign` пробует кандидатов по
  `FileExists`).
- Видео — WMV9 (`.wmv`), Theora/Bink-контейнеров нет; наш снапшот видео пока не
  декодирует.
- Часть архивов текстур DA — XTC2 (`IMAGEDIRECTORY5`) — отдельный будущий парсер
  (см. CLAUDE.md).

## 5.1 POSHISTORY (engine-path'ы) — PC пишет версию 1002, снапшот знает только 1000/1001

**Это причина того, что в игре не открывается ни одна дверь.**

`CWO_PosHistory::LoadPath` (`WObj_PosHistory.cpp`) принимает только две версии:

| ID | Константа | Укладка |
|---|---|---|
| 1000 | `POSHISTORY_RESOURCEID` | `[nKeys][CFileKeyframe × nKeys]` — каждый кадр 32 байта (`fp32 time`, `CVec3Dfp32 pos`, `CQuatfp32 rot`) |
| 1001 | `POSHISTORY_PACKED_RESOURCEID` | `[nKeys][CFileKeyframe #0][CFileKeyframe #last][CFileKeyframe_Packed × (nKeys−2)]` — середина по 16 байт (`uint8 time[2]`, поз. 27+27+26 бит, кватернион 11+11+10 бит) |

Заголовок ресурса: необязательный тег `'PATH'` (его пишет XWC, чтобы узнавать
пути в resource-data), затем слово версии (`ver & 0xffff` = ID, бит `0x10000` =
`POSHISTORY_FLAGS`, тогда после кадров идёт по байту флагов на кадр,
выровненных до слова), затем `nSeq`, затем последовательности.

Всё остальное `LoadPath` **отбрасывает молча** — ни исключения, ни сообщения.
Результат: `m_lSequences` пустой → `IsValid()` false → `GetDuration()` = −1 →
`CWObject_Attach::OnRefresh` получает от `GetRenderMatrix` константу, условие
`!Mat.AlmostEqual(Old)` никогда не выполняется, и мувер не двигается. При этом
скрипты, таймеры и timed-сообщения пути работают штатно, поэтому симптом
выглядит как «сломанные скрипты», хотя скрипты ни при чём.

Прогон `Pa1_Pit` 2026-07-29 (`RIDDICK_DBG_PATH=1`):

```
[PATH] LoadPath: unknown poshistory version 0x3ea (id=1002, expected 1000 or 1001)
[PATH] run obj=95 'XTRAGATE' iAnim0=12 nSeq=0 dur=-1.000 travel=-1.00 cflags=0xb8010
[PATH] run obj=462 'CHAIN1'  iAnim0=97 nSeq=0 dur=-1.000 travel=-1.00 cflags=0x98010
[PATH] run obj=456 'CHAIN2'  iAnim0=94 nSeq=0 dur=-1.000 travel=-1.00 cflags=0x98010
```

`nSeq=0` у **всех** путей карты, включая AI-пути (их же читают
`AI_Behaviour_Patrol`/`_Lead` через `OBJMSG_HOOK_GETRENDERMATRIX`) — то есть
это же расхождение объясняет и странное поведение NPC.

### Укладка 1002 — подтверждена (2026-07-29)

**1002 — это ровно укладка 1000 под новым номером версии, плюс одно лишнее
32-битное слово в самом конце ресурса.** Тега `'PATH'` в PC-данных нет,
версия лежит в `w0`.

```
w0            версия (0x3ea = 1002, бит 0x10000 = POSHISTORY_FLAGS)
w1            nSeq
для каждой последовательности:
   nKeys
   CFileKeyframe × nKeys      -- 32 байта: fp32 time, CVec3Dfp32 pos, CQuatfp32 rot
<одно слово = 0>              -- хвост, назначение неизвестно, во всех образцах ноль
```

Проверено на всех шести ресурсах из дампа `Pa1_Pit` — не на паре примеров, а по
инвариантам на каждом кадре: все кватернионы **точно** единичной длины
(|q| = 1.00000), времена монотонны, и потреблённая длина каждый раз равна
`len − 4`:

| obj | iRes | len | nSeq | кадры | потреблено | хвост |
|---|---|---|---|---|---|---|
| 26 | 5 | 80 | 1 | 2 | 76 | 4 (0x0) |
| 28 | 6 | 80 | 1 | 2 | 76 | 4 (0x0) |
| 30 | 8 | 80 | 1 | 2 | 76 | 4 (0x0) |
| 31 | 9 | 80 | 1 | 2 | 76 | 4 (0x0) |
| 41 | 10 | 80 | 1 | 2 | 76 | 4 (0x0) |
| 95 | 12 | 148 | 2 | 3 + 1 | 144 | 4 (0x0) |

Содержимое читается осмысленно, что и есть окончательное подтверждение:

- `obj=95 'XTRAGATE'`, seq0: `t=0 (0,0,0)` → `t=15 (0,0,96)` → `t=20 (0,0,0)` —
  решётка поднимается на 96 юнитов и опускается; seq1 — один кадр в нуле.
- `obj=41`: `t=0.2 (0,0,−72)` — опускается на 72 юнита за 0.2 с.
- `obj=28`: `t=0.35 (112, −0.003, 0)` — сдвиг на 112 юнитов по X.
- `obj=26/30/31`: два кадра в нуле с длительностями 20 / 0.2 / 0.2 с — это
  EP-объекты, используемые как чистые скриптовые таймеры (`EP_MASTER_ON/OFF`,
  `DWELLCHECK*`), им движение и не нужно.

Позиции в кадрах — смещения относительно начала объекта, движок накладывает их
через `m_Transform` / `m_PathRelMat`.

**Фикс:** `POSHISTORY_PC_RESOURCEID = 1002` в перечислении, и `LoadPath`
(а также `CPosHistory_EditData::Load`) переписывает ID на
`POSHISTORY_RESOURCEID` сразу после проверки версии, сохраняя бит флагов.
Так все двенадцать сравнений `GetVersion() == POSHISTORY_RESOURCEID` внутри
`CSequence` продолжают работать без правок. Хвостовое слово игнорируется —
последовательности идут перед ним, поэтому на разбор оно не влияет.

## 5.2 AnimGraph2 v5/v6 (PC) — укладка `CXRAG2_AnimLayer`, снятая с бинаря

**Это причина того, что скелеты персонажей целиком заливались QNaN.**

Взято не гаданием, а из `GameWorld_Win32_x86_dll_decomp.c` — функция,
заканчивающаяся на
`Error_static("CXRAG2_StateAnim::Read", "Unsupported version %.4x")`, это и есть
`CXRAG2_AnimLayer::Read` ретейл-сборки. Ширины чтений определены по
`CByteStream::Read(..., N)` внутри инлайненных `ReadLE`:

| функция в дампе | N | тип |
|---|---|---|
| `FUN_1007f010` | 4 | fp32 |
| `FUN_10069050` | 4 | uint32 |
| `FUN_101a2560` | 2 | int16 |
| `FUN_101a2150` | 1 | uint8 |
| `FUN_101a1d40` | 1 | uint8 |

Ветка `case 5: case 6:` читает по порядку:

```
+0x00  4  TimeOffset            fp32
+0x04  4  TimeScale             fp32
+0x08  4  AnimFlags             uint32     <-- 32 бита, не байт
+0x0c  2  iAnim                 int16
+0x0e  1  <поле, которое PC держит по +0x0e; у нас члена нет>
+0x0f  1  iBaseJoint            uint8
+0x10  1  iMergeOperator        uint8
+0x11  1  iTimeControlProperty  uint8
                                = 18 байт
```

18 — ровно тот шаг, что измерен из файла (`[AG2FMT] FULLANIMLAYERS
stride == consumed == 18`, остаток 0 на всех графах). Совпадение независимо
измеренного шага с суммой ширин из бинаря — и есть подтверждение.

**Opacity в v5/v6 не хранится вообще.** В ветке `case 5/6` третьего float
просто нет, а в `case 4` PC читает его в **выбрасываемую стековую
локаль** (`local_110`) — то есть в PC-структуре члена `m_Opacity` нет.
Там же видно, что в `case 3/4` один байт тоже читается в выбрасываемую
локаль.

Что было сломано у нас и к чему это приводило:

* байты 8..11 читались как float в `m_Opacity`. На самом деле это
  `AnimFlags`, у большинства слоёв нулевой → **opacity 0.0 у всех слоёв**.
  `CWAG2I_StateInstance::GetAnimLayers` считает
  `LayerBlend = Blend * GetOpacity()` и выбрасывает слой при нулевом
  произведении → ни один слой не выживал → `CXR_Skeleton::EvalAnim` не
  находил полнотелого слоя (`base == 0 && blend > 0.999`), обрывался и
  заливал все 120 костей QNaN. Отсюда `Aborting EvalAnim` 650 раз за
  прогон, `Fucked up camera rottrack` 324 раза и камера в координатах
  ступней;
* `AnimFlags` брался из одного байта по +0x0e, то есть настоящие флаги
  (`CXR_ANIMLAYER_VALUECOMPARE` и прочие) терялись полностью.

Осталось неизвестным: назначение байта +0x0e. Порядок последних двух байт
(`iMergeOperator`, `iTimeControlProperty`) сохранён по аналогии с v3/v4; во
всех снятых образцах они нулевые, поэтому эмпирически не различимы.

## 6. Как дебажили (процесс)

- gdb `bt` от пользователя — основной инструмент локализации (глубина стека
  2474 фрейма сразу выдала рекурсию по нодам).
- Реверс полей: hex-дамп чанка из .XW + проверка гипотез скриптом по инвариантам
  на *всех* элементах (не по паре примеров) — только после нулевого числа
  несоответствий укладка считается подтверждённой.
- `[CON]`-зеркало консоли движка в stderr и трейсы `(Command_ChangeMap)` — чтобы
  сообщения загрузчика попадали в run-лог.
