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

## 6. Как дебажили (процесс)

- gdb `bt` от пользователя — основной инструмент локализации (глубина стека
  2474 фрейма сразу выдала рекурсию по нодам).
- Реверс полей: hex-дамп чанка из .XW + проверка гипотез скриптом по инвариантам
  на *всех* элементах (не по паре примеров) — только после нулевого числа
  несоответствий укладка считается подтверждённой.
- `[CON]`-зеркало консоли движка в stderr и трейсы `(Command_ChangeMap)` — чтобы
  сообщения загрузчика попадали в run-лог.
