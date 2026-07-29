# Отчёт: меши персонажей рвутся в «осколки»

По спеке `Docs/Research_ShardedCharacters.md`. Ветка `riddick-render`,
HEAD постановки `3fc6f2e` (анализ на `02a9e8b`, только статический —
игра не собиралась и не запускалась).

---

## 1. Ответ на главный вопрос (§3 спеки)

**Ломается соответствие «индексы ↔ вершины»: движок передаёт в
`Render_IndexedTriangles` ЧИСЛО ИНДЕКСОВ (`m_nIBPrim`) там, где ожидается
ЧИСЛО ТРЕУГОЛЬНИКОВ. Бэкенд умножает его на 3, и draw читает в 3 раза
больше индексов, чем принадлежит кластеру, — сначала индексы соседних
кластеров того же меша, а в конце буфера — мусор за границей массива.**

Это оригинальный (дремлющий) баг движка в CPU-пути скиннинга, который на
штатных платформах не выполнялся (там брался HW/VPU-путь через VBID, а в
нём деление на 3 есть). У нас `bHWAnim = false`, поэтому персонажи идут
именно багнутым путём.

Четыре места с дефектом (все — CPU-ветки `else` рядом с корректными
VBID-ветками, где стоит `/ 3`):

| Файл:строка | Функция | Путь |
|---|---|---|
| `Source/P5/Shared/MOS/XRModels/Model_TriMesh/WTriMesh.cpp:1822` | `Cluster_RenderUnified` | **активный в прогоне** (unified) |
| `WTriMesh.cpp:1159` | `Cluster_Render` | не-unified |
| `WTriMesh.cpp:1414` | `Cluster_RenderProjLight` | projlight |
| `WTriMesh.cpp:2729` | `Cluster_RenderSingleColor` | bDrawFilled |

Во всех четырёх:
```cpp
_pRenderParams->m_RenderVB.Render_IndexedTriangles(
    pTIB->GetTriangles(this) + _pC->m_iIBOffset, _pC->m_nIBPrim);
```
а должно быть `_pC->m_nIBPrim / 3`.

### Почему симптом именно такой

- `m_nIBPrim` — длина диапазона кластера в общем индексном буфере (IB)
  **в индексах** (uint16), т.е. `3 × число треугольников`. Бэкенд делает
  `glDrawElements(..., _nTriangles * 3, ...)` — итого читается
  `m_nIBPrim * 3` индексов начиная с `m_iIBOffset`, т.е. **3× over-read**.
- Первая треть прочитанного — собственные треугольники кластера (меш
  частично узнаваем). Следующие две трети — продолжение того же массива
  `m_lTriangles` индексного буфера: индексы СОСЕДНИХ кластеров того же
  меша. Они валидны (адресуют тот же TVB, весь массив вершин которого
  заскиннен в `m_pRenderV`), поэтому рисуются куски соседних частей тела
  с материалом этого кластера → «месиво из полигонов с текстурами
  персонажа», «часть меша схлопнута».
- Если диапазон кластера стоит в КОНЦЕ массива (или IB выделен под один
  кластер: `m_iIBOffset = 0`, `m_nIBPrim = Len`), over-read сразу уходит
  за границу TArray → случайные uint16 как индексы. Часть из них
  `>= nV` → GL-фетч за концом стриминг-VBO (там соседняя геометрия
  кадра) → длинные узкие треугольники-веер из модели → «осколки».
  Часть `< nV`, но мусорные → случайные треугольники внутри облака
  вершин меша.
- «Осколки несут материалы персонажа» — потому что это ЕГО draw с ЕГО
  attrib/текстурой, просто с левыми индексами. «Меньше персонажей —
  меньше багов» — меньше кластеров с over-read и меньше чтений за
  границу. «Часть персонажей целые» — кластеры с `m_nIBPrim == 0` идут
  корректной веткой `Render_IndexedTriangles(pTVB->GetTriangles(this),
  pTVB->GetNumTriangles(this))` (`WTriMesh.cpp:1825`), где счётчик —
  настоящее число треугольников.

---

## 2. Доказательства

### 2.1 Семантика `m_nIBPrim` = число индексов (не треугольников)

Сам движок дважды делит его на 3, чтобы получить треугольники:

- `WTriMesh.cpp:318`:
  ```cpp
  int nTriangles = pC->m_nIBPrim?(pC->m_nIBPrim / 3):pVB->GetNumTriangles(this);
  ```
- `WTriMesh.cpp:7488-7491` (построение рёбер):
  ```cpp
  if(pC->m_nIBPrim)
  {
      nTriangles = pC->m_nIBPrim / 3;
      pTri = (CTM_Triangle *)(piPrim + pC->m_iIBOffset);
  }
  ```
  (`CTM_Triangle` = 3×uint16, `m_iIBOffset` — смещение в ИНДЕКСАХ.)

Все VBID-ветки тех же функций делят на 3 при передаче в
`Render_VertexBuffer_IndexBufferTriangles`: `WTriMesh.cpp:1144`,
`:1399`, `:1777`, `:1793`, `:2712`.

### 2.2 Семантика `Render_IndexedTriangles(_pTriVertIndices, _nTriangles)` = треугольники

- PS3-эталон `Source/P5/Shared/MOS/RenderContexts/PS3GCM/MRenderPS3_Render.cpp:70-76`:
  `gcmSetDrawIndexArray(CELL_GCM_PRIMITIVE_TRIANGLES, _nTriangles * 3, ...)`.
- Наш бэкенд `Source/P5/Shared/MOS/RenderContexts/GLES3/MDisplaySDL2.cpp:6069`:
  `DrawIndexed(GL_TRIANGLES, _pTriVertIndices, _nTriangles * 3);`
- Цепочка доставки: звено `CXR_VBChain::Render_IndexedTriangles`
  сохраняет `_nTriangles` в `m_nPrim`
  (`Source/P5/Shared/MOS/XR/XRVertexBuffer_VPUShared.h:47-52`), движок
  разворачивает цепочку и передаёт как есть:
  `Source/P5/Shared/MOS/XR/XRVertexBuffer.cpp:312` → `:337-339`.
- Корректная ветка того же кода передаёт `GetNumTriangles(this)` —
  число треугольников (`XMDCommn.cpp:5681-5693` возвращает
  `m_lTriangles.Len()`; `WTriMesh.cpp:7485-7498` итерирует его как
  массив `CTM_Triangle`).

Итого: на CPU-ветке с `m_nIBPrim != 0` в draw уходит
`m_nIBPrim * 3 = 9 × (число треугольников кластера)` индексов вместо
`3 ×`, читается `3×` от диапазона кластера.

### 2.3 Персонажи идут именно CPU-веткой

- `bHWAnim = false` (не заявляем `CRC_CAPS_FLAGS_MATRIXPALETTE`,
  `MDisplaySDL2.cpp:~3753`) → `m_bRenderTempTLEnable = false`
  (`WTriMesh.cpp:5483-5484`) → ветка `else` в `OnRender`
  (`WTriMesh.cpp:5807-5831`): аллокация `m_pRenderV/N(/TangU/V)` из VBM,
  `Cluster_TransformBones_V_N_TgU_TgV` (`:5823`), палитра обнуляется
  (`:5831`).
- `m_bUsePrimitives == false` всегда (`WTriMesh.cpp:6792` — безусловное
  присваивание), значит выбор только между `m_nIBPrim != 0` (баг) и
  `GetNumTriangles` (норма).
- Рендер: `Cluster_RenderUnified` (`WTriMesh.cpp:6011` → `:1750`),
  CPU-ветка `:1804-1827`, багнутая строка `:1822`.
- `m_nIBPrim`/`m_iIBOffset` читаются из файла модели
  (`XMDCommn.cpp:3486-3618`, IO-структуры v200/v201) — т.е.
  зависит от данных конкретной модели, отсюда «часть мешей целые».

### 2.4 Бэкенд не защищён от out-of-range индексов

`DrawIndexed` (`MDisplaySDL2.cpp:5652`) стримит вершины
(`PushVertices`, `:5886`) и индексы (`PushIndices`, `:5887`) как есть;
проверки `max(index) < nVerts` нет нигде на пути. Конвертер
`BuildInterleavedVerts` (`:4541-4642`) берёт `nV = m_Geom.m_nV`
(`:4568`) и не смотрит на индексы. Любой индекс `>= nV` читает соседние
данные стриминг-VBO.

### 2.5 Код — оригинальный, не портовый

`git log -L` по `WTriMesh.cpp:1815-1826`: строки пришли коммитом
`ca961cf "Add Sources of SBEngine"` (начальный импорт), не правились.

---

## 3. Опровергнутые гипотезы §4 спеки

- **4.1 (слияние кластеров / протухший кэш бэкенда) — ОПРОВЕРГНУТО.**
  На CPU-пути слияния кластеров НЕТ вообще: цикл слияния
  (`CTM_MAX_MERGECLUSTERS`, `WTriMesh.cpp:5841`/`:5935`) стоит в ветке
  `m_bRenderTempTLEnable == true` (HW/VBID), которая у нас не
  выполняется. (Уточнение к §2.3 спеки: формулировка «кластеры сливаются
  в цепочку» неверна для нашего пути — на CPU-пути каждый кластер =
  цепочка из ОДНОГО звена.) Мемо бэкенда инвалидируется каждым
  `Geometry_VertexBuffer` (`MDisplaySDL2.cpp:6173-6187`), т.е. на каждое
  звено; ключ мемо — указатели/счётчики, внутри кадра арена VBM
  append-only, протухания нет.
- **4.2 (индексы кластера глобальные) — ОПРОВЕРГНУТО в исходной форме,
  но ближайшая к правде.** Индексы действительно TVB-глобальные, и это
  КОРРЕКТНО: `m_pRenderV` содержит все `nV = GetNumVertices()` вершин
  TVB (`WTriMesh.cpp:5809`, `:5823`, `:1808-1809`), скиннинг идёт по
  всему TVB (`:3731`, цикл `v < nV` `:3770`). Ломается не база индексов,
  а их КОЛИЧЕСТВО (§1).
- **4.3 (`nVAlloc` без удвоения) — ОПРОВЕРГНУТО для рендер-пути.**
  Удвоение нужно только теневым объёмам; они выключены
  (`RIDDICK_SKIP_SHADOWVOL=1`, `svol=0`). Рендер-путь адресует только
  `[0, nV)`.
- **4.4 (`Cluster_TransformBones_V_N_TgU_TgV`) — ОПРОВЕРГНУТО.**
  Цикл `0..nV` (`WTriMesh.cpp:3770-3903`), все три ветки (1/2/n костей)
  пишут V, N, TangU, TangV для КАЖДОЙ вершины. Ранние выходы:
  `:3736-3737` (без лога, но входы гарантированы вызывающим,
  `:5820-5822`), `:3752-3760` (с ConOut-WARNING — в логе прогона их нет,
  зеркало `[CON]` работает).
- **4.5 (порча VBM) — ОПРОВЕРГНУТО как причина.** `Alloc` — строгий
  bump-аллокатор (`XRVBManager.cpp:551`, при нехватке — NULL + трейс
  `Out of VB memory!` `:618-623`, перезаписи чужих блоков нет). NULL →
  `continue` (`WTriMesh.cpp:5816`/`:5822`) = кластер пропадает ЦЕЛИКОМ,
  а не рвётся. `Alloc_VBChainCopy` (`XRVBManager.cpp:812-877`) копирует
  звенья одним аллоком, частичных цепочек не остаётся; вершинные данные
  разделяются read-only в пределах кадра.

### Побочные находки (не причина осколков, отдельные дефекты)

1. **Шейдинг персонажей идёт без нормалей и тангентов.** В
   `Cluster_RenderUnified` на цепочку вешаются только V и TV0
   (`WTriMesh.cpp:1809-1810`); `m_pN` звена присваивается ПОСЛЕ всех
   `RenderShading` (`WTriMesh.cpp:2478-2479`), тангенты не привязываются
   нигде. Для вида «шарды» безразлично, но FP20-освещение персонажей
   работает на неполной геометрии.
2. **`ADDVERTEX` без bounds-check** в `CXR_VBChain::BuildVertexUsage`
   (`XRVertexBuffer.cpp:399`): индекс `>= m_nV` пишет за границу
   битмапы использования вершин (OOB-запись в кучу). Путь — VB-операторы
   / туман / `XRUtilRS.cpp:1932`; на наших флагах не активен, но после
   включения соответствующих фич станет источником порчи чужих данных.
3. Спящий дефект теневых объёмов (удвоение VB) — уже описан в
   `Docs/Research_ShadowWedges_Report.md`, остаётся.

---

## 4. Патчи (в код НЕ внесены)

### 4.1 Фикс-кандидат (решающий A/B-эксперимент)

Четыре правки, по одной строке. Якоря — функция + текущая строка:

```cpp
// WTriMesh.cpp, Cluster_RenderUnified, сейчас строка 1822:
- _pRenderParams->m_RenderVB.Render_IndexedTriangles(pTIB->GetTriangles(this) + _pC->m_iIBOffset, _pC->m_nIBPrim);
+ _pRenderParams->m_RenderVB.Render_IndexedTriangles(pTIB->GetTriangles(this) + _pC->m_iIBOffset, _pC->m_nIBPrim / 3);

// WTriMesh.cpp, Cluster_Render, сейчас строка 1159: та же замена.
// WTriMesh.cpp, Cluster_RenderProjLight, сейчас строка 1414: та же замена.
// WTriMesh.cpp, Cluster_RenderSingleColor, сейчас строка 2729: та же замена.
```

Это не «диагностика», а кандидат на фикс; по ТЗ не внесён. Если владелец
предпочтёт сначала чистое подтверждение — см. 4.2/4.3.

### 4.2 `RIDDICK_DBG_IBRANGE=1` (engine-side, прямое измерение over-read)

В `Cluster_RenderUnified`, сразу после `WTriMesh.cpp:1808`
(`int nV = pTVB->GetNumVertices(this);`), в ветке `else if(_pC->m_nIBPrim)`:

```cpp
else if(_pC->m_nIBPrim)
{
    CTM_VertexBuffer* pTIB = GetVertexBuffer(_pC->m_iIB);
#ifdef PLATFORM_LINUX  // или без гейта, env-чтение дешёвое в static
    {
        static int sDbgIB = -1;
        if (sDbgIB < 0) { const char* e = getenv("RIDDICK_DBG_IBRANGE"); sDbgIB = (e && *e && *e != '0') ? 200 : 0; }
        if (sDbgIB > 0)
        {
            --sDbgIB;
            int lenIdx = pTIB->GetNumTriangles(this) * 3;   // длина массива индексов IB
            int readEnd = _pC->m_iIBOffset + _pC->m_nIBPrim * 3; // конец чтения с текущим кодом
            ConOut(CStrF("[IBRANGE] cl=%d iIBOff=%d nIBPrim=%d lenIdx=%d validEnd=%d over=%d",
                _iCluster, _pC->m_iIBOffset, _pC->m_nIBPrim, lenIdx,
                _pC->m_iIBOffset + _pC->m_nIBPrim, readEnd - lenIdx));
        }
    }
#endif
    _pRenderParams->m_RenderVB.Render_IndexedTriangles(pTIB->GetTriangles(this) + _pC->m_iIBOffset, _pC->m_nIBPrim);
}
```

Ожидаемый результат при подтверждении: у всех кластеров
`validEnd == iIBOff + nIBPrim` (данные консистентны) и `over > 0` у
большинства; у «разорванных» мешей — большой `over` (чтение далеко за
конец IB). Аналогичный блок можно поставить в `:1156`, `:1411`, `:2726`.

### 4.3 `RIDDICK_DBG_GEOMRANGE=1` + счётчик `oob=` (backend, из §5 спеки)

В `DrawIndexed` (`MDisplaySDL2.cpp:5652`), сразу после
`iRes = m_Streamer.PushIndices(...)` (`:5887`), перед
`glDrawElements` (`:5737` для кэш-пути / общий draw для path B):

```cpp
static int sDbgGR = -1;
if (sDbgGR < 0) { const char* e = getenv("RIDDICK_DBG_GEOMRANGE"); sDbgGR = (e && *e && *e != '0') ? 40 : 0; }
int maxIdx = 0;
for (int k = 0; k < _nInd; ++k) if (_pInd[k] > maxIdx) maxIdx = _pInd[k];
if (maxIdx >= nVerts)
{
    ++m_DbgDrawOOB;   // новый счётчик, печатать в [GL-DBG] как oob=N
    if (sDbgGR > 0)
    {
        --sDbgGR;
        fprintf(stderr, "[GEOMRANGE] nV=%d nInd=%d max=%d prim=0x%x pV=%p idx0=[%u %u %u]\n",
            nVerts, _nInd, maxIdx, (unsigned)_GLPrim, (void*)m_Geom.m_pV,
            (unsigned)_pInd[0], (unsigned)_pInd[1], (unsigned)_pInd[2]);
    }
}
```

(Для fast path A/B вставить в обе точки draw; `m_DbgDrawOOB` добавить в
строку `[GL-DBG]`.) При нашем баге: `oob > 0` на кластерах, чей over-read
вышел за границу IB; при гипотезе «вершины битые» (4.4/4.5) — `oob == 0`,
и это тоже информативно.

### 4.4 Флаги из §5 спеки, которые больше не нужны

`RIDDICK_DBG_CHAIN` (цепочки) и `RIDDICK_ONE_CHAR` — не предлагаются:
слияния кластеров на CPU-пути нет (4.1 опровергнута статически), а
зависимость от числа персонажей объясняется количеством over-read'ов.

---

## 5. Что запросить у владельца

**Вариант A (рекомендуется, один прогон):** применить 4.1 (4 строки) и
прогнать той же строкой:

```
RIDDICK_SKIP_SHADOWVOL=1 RIDDICK_DBG_MODELS=1 RIDDICK_SKINNING=1 \
RIDDICK_DIFFUSE_ONLY=1 RIDDICK_NO_SCISSOR=0 RIDDICK_NO_FOG=1 \
RIDDICK_DBG_NDS=0 RIDDICK_NDS=1 RIDDICK_LFM=1 RIDDICK_LFM_SCALE=8 \
RIDDICK_FP20=1 RIDDICK_DBG_GL=1 RIDDICK_DBG_SHADER=0 \
RIDDICK_SKIP_PROPS=1 RIDDICK_SKIP_CHARS=0 \
RIDDICK_DIRECT_RENDER=1 RIDDICK_STARTMAP=Pa1_TheDream
```

Смотреть: исчезли ли осколки/клинья у персонажей (два тех же ракурса,
что в постановке). Если да — причина доказана, патч оформить как фикс
(4 места) и закрыть задачу.

**Вариант B (если нужно количественное подтверждение до фикса):**
внести 4.2 + 4.3, прогнать той же строкой +
`RIDDICK_DBG_IBRANGE=1 RIDDICK_DBG_GEOMRANGE=1`. Смотреть:
- `[IBRANGE] ... over=<N>` — ожидается `over > 0` у кластеров с
  `m_nIBPrim != 0`; у шарденых мешей — большие `over`;
- `oob=` в `[GL-DBG]` — ожидается `oob > 0`;
- `[GEOMRANGE] nV=... max=...` — `max >= nV` на тех же draw'ах.

---

## 6. Открытые вопросы

1. Почему баг дошёл до нас живым: на PC-retail (RndrGL) и PS3 CPU-ветка
   с `m_nIBPrim` предположительно не выполнялась (HW matrix palette /
   VPU-скиннинг → VBID-путь с `/3`). Подтвердить можно было бы
   декомпайлом RndrGL (семантика его `Render_IndexedTriangles`), но
   внутренние доказательства (§2.1/§2.2) самодостаточны.
2. Побочный дефект освещения (§3, п.1): нормали на звене появляются
   после shading-проходов, тангенты не привязываются — после фикса
   осколков оценить визуально, нужен ли отдельный ресёрч по FP20 для
   персонажей.
3. `Cluster_RenderSingleColor` (`:2729`) идёт тем же багом, но путь
   `bDrawFilled` в текущих прогонах вроде бы не активен — править заодно
   (та же строка).
4. После возврата теневых объёмов (снятия `RIDDICK_SKIP_SHADOWVOL`)
   вернуться к спящему дефекту удвоения VB — см.
   `Docs/Research_ShadowWedges_Report.md` §1a.
