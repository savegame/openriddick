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
четыре ридера: `CBSP_CoreFace::Read`, `CBSP2_CoreFace::Read`,
`CBSP3_CoreFace::Read`, `CBSP4_CoreFace::Read` (кейс 0x0204 = 0x0203 + skip 8).

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

## 5. Известные «не-BSP» расхождения PC-контента (для полноты)

- `Worlds/` PC-набора EFBB не содержит bootstrap-мира `campaign.xw` —
  кампания стартует сразу `Pa1_Intro` (`startnewcampaign` пробует кандидатов по
  `FileExists`).
- Видео — WMV9 (`.wmv`), Theora/Bink-контейнеров нет; наш снапшот видео пока не
  декодирует.
- Часть архивов текстур DA — XTC2 (`IMAGEDIRECTORY5`) — отдельный будущий парсер
  (см. CLAUDE.md).

## 6. Как дебажили (процесс)

- gdb `bt` от пользователя — основной инструмент локализации (глубина стека
  2474 фрейма сразу выдала рекурсию по нодам).
- Реверс полей: hex-дамп чанка из .XW + проверка гипотез скриптом по инвариантам
  на *всех* элементах (не по паре примеров) — только после нулевого числа
  несоответствий укладка считается подтверждённой.
- `[CON]`-зеркало консоли движка в stderr и трейсы `(Command_ChangeMap)` — чтобы
  сообщения загрузчика попадали в run-лог.
