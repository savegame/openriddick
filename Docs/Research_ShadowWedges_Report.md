# Отчёт по спеке «чёрные клинья вокруг персонажей»

Дата: 2026-07-29. Спека: `Docs/Research_ShadowWedges.md` (HEAD постановки `6d7cbbb`).
Метод: чтение кода (движок TriMesh + GLES3-бэкенд + эталоны RndrGL/PS3GCM/VPGen),
без прогонов. Все ссылки — `путь:строка` на HEAD `6d7cbbb`.

---

## 1. Ответ на §3 спеки

Клинья — **стенсильные теневые объёмы скиннед `CXR_Model_TriangleMesh`**
(путь `CTM_CLUSTERFLAGS_SHADOWVOLUMESOFTWARE`). Дефект двойной.

### 1a. Геометрия объёма сломана: индексы адресуют удвоенный VB, вершин — вдвое меньше

Силуэтные индексные листы строятся на CPU
(`ShadowProjectHardware_ThreadSafe`, `WTriMesh.cpp:3921`) и **намеренно
адресуют удвоенный VB**: первая половина — базовые позиции, вторая
(`+nRealVBV`) — «экструдируемые» копии:

- caps: `pPrim[iP1++] = iRealClusterOffset + Tri.m_iV[k] + nRealVBV`
  (`WTriMesh.cpp:4312-4314`) против `+0` (`:4315-4317`);
- edge-квады смешивают `iv0/iv1` и `iv0+nRealVBV/iv1+nRealVBV`
  (`:4336-4351`).

`nRealVBV` — число вершин VB рендер-меша (`:4300`,
`plnVBVertices[pRealC->m_iVB]`), т.е. рассогласования «теневой меш vs
рендер-меш» нет — индексы корректны ровно при условии, что VB содержит
**2·nV** вершин.

Удвоение задумано двумя способами, и оба у нас выключены:

1. **VBID/hardware-путь**: `Get(CRC_BuildVertexBuffer&)` под флагом
   `CTM_CLUSTERFLAGS_SHADOWVOLUMESOFTWARE` пишет две копии вершин и помечает
   вторую `pTex[i]=1.0` («Extrude») (`WTriMesh.cpp:8247-8306`, суммарно
   `m_nV = Len*2` на `:8360-8362`). Экструзия второй копии — в вершинной
   программе по texgen `CRC_TEXGENMODE_SHADOWVOLUME2`. Этот путь скиннед-
   модели не берут: `bHWAnim=false` (`WTriMesh.cpp:5443`, бэкенд не заявляет
   `CRC_CAPS_FLAGS_MATRIXPALETTE`) → `m_bRenderTempTLEnable=false`
   (`:5483-5484`).
2. **CPU-путь**: удвоение и CPU-экструзия **закомментированы**:
   `int nVAlloc = /*(bSHADOWVOLUME) ? 2*nV :*/ nV;` (`WTriMesh.cpp:5789`) и
   `// Cluster_ProjectVertices(&m_pRenderV[0], &m_pRenderV[nV], ...)`
   (`:5807-5808`, аналог `:5895`). Аллоцируется и скиннится
   (`Cluster_TransformBones_V_N_TgU_TgV`, `:5802`) ровно **nV** вершин.

Итог: цепочный VB теневого прохода (`VB = m_RenderVB` + `Alloc_VBChainCopy`,
`:2024-2026`) содержит nV вершин, а подставленные в него индексы
(`Render_IndexedTriangles(m_pppShadowPrimData[...], 0xffff)`, `:2035`,
`:2061`) бегут до ~2·nV. Бэкенд стримит ровно nVerts и сырой индексный
массив (`MDisplaySDL2.cpp:5919`, протокол `0xffff` поддержан `:6040-6050`)
→ **out-of-range vertex fetch** в `glDrawElements`. Вторая половина каждого
треугольника — мусор/нули (точка у origin модели), первая — валидные
скиннед-вершины. Это ровно наблюдаемая картина: узкие треугольники веером
из общей точки, привязанной к персонажу, движущиеся с анимацией.

### 1b. Почему это видно: не цвет, а stencil

Сам проход объёма в цвет писать не должен и не пишет:

- атрибут объёма: `ColorWriteDisable = CRC_FLAGS_COLORWRITE`
  (`WTriMesh.cpp:5091`, debug-бит 12 не выставлен — `m_DebugFlags=0`,
  `XREngine.cpp:36`, сеттер только через консоль `xr_debugflags`,
  `XREngine.cpp:5967`), далее `Attrib_Disable(ColorWriteDisable|ALPHAWRITE|ZWRITE)`
  (`:5116`), `Attrib_RasterMode(CRC_RASTERMODE_ADD)` (`:5117`),
  separate-stencil ветка `:5132-5148` (мы заявляем
  `CRC_CAPS_FLAGS_SEPARATESTENCIL`);
- атрибут присваивается VB (`VB.m_pAttrib = pA`, `:2100`), построение
  (`OnRender_Scissors`, вызов `:5610`) и культинг симметричны рендеру
  (`m_plLightCulled[iL]=1` на `:4951` → `continue` в `VB_RenderUnified`
  на `:2021`);
- бэкенд: `glColorMask` выставляется **всегда**, когда вызван
  `ApplyAttribs` (`MDisplaySDL2.cpp:4198-4200`), flush атрибутов стоит во
  всех трёх путях draw (`:5648-5649` DrawIndexed, `:6290-6291`
  DrawUserVerts, `:5018-5019` DrawCachedVB); счётчик `cwoff>0` в логах
  подтверждает, что colorwrite-off атрибуты до GL доходят;
- `RIDDICK_DIFFUSE_ONLY` на эти проходы не влияет (нет FP20 ext-attrib:
  `SetDefault` обнуляет `m_pExtAttrib`, `MRender.cpp:861`; гейт
  `MDisplaySDL2.cpp:4182-4184`).

То есть мусорная геометрия портит **не цветовой буфер, а stencil**: весь
смысл прохода — `INCWRAP/DECWRAP` по обеим граням (`:5140-5147`,
`StencilRef(1,255)`). Мусорные веерные треугольники оставляют в stencil
экранную веерную область со значением ≠0. Дальше per-light shading-проходы
обычных кластеров для тенеотбрасывающих источников идут **со stencil-test**
(`XR_SHADERFLAGS_NOSTENCILTEST` сбрасывается, `WTriMesh.cpp:2130-2133`) и в
этих областях маскируются → экранные веерные зоны теряют динамический свет
→ **чёрные клинья** (база в Riddick почти чёрная). Тот же испорченный
stencil (общий буфер, тот же ref=1) ломает и BSP2-проходы света
(`CXR_VirtualAttributes_BSP2ShadowVolumeFront`, `WBSP2Model.cpp:96-142`).

Замечание: даже если бы удвоение было включено, экструзии всё равно нет —
texgen `SHADOWVOLUME2` в GLES3-бэкенде игнорируется (0 упоминаний
`SHADOWVOLUME` в `RenderContexts/GLES3/`; неизвестные моды попадают в
`DbgNoteTexGenMode`, `MDisplaySDL2.cpp:3048-3051`). Тогда объёмы были бы
дегенератами (невидимы, теней нет). Полный фикс = удвоение + экструзия
(CPU-помощник `Cluster_ProjectVertices` существует, `WTriMesh.cpp:3907`;
либо шейдерная экструзия по эталону `shaders/VP.xrg:737-764`).

### 1c. Оранжевые полосы

Отдельный артефакт, вероятно `CXR_Model_LaserBeam` (присутствует в
`[MODEL]`-логе) — гипотеза 4.4 спеки не закрыта, к клиньям отношения не
имеет.

---

## 2. Доказательства (сводка цитат)

| Утверждение | Место |
|---|---|
| Индексы объёма адресуют удвоенный VB | `WTriMesh.cpp:4312-4317`, `:4336-4351` |
| `nRealVBV` — вершины рендер-VB | `WTriMesh.cpp:4300`, `:3973-3979` |
| Удвоение в VBID-пути (2 копии + метка extrude) | `WTriMesh.cpp:8247-8306`, `:8360-8362` |
| Удвоение/экструзия в CPU-пути закомментированы | `WTriMesh.cpp:5789`, `:5807-5808` |
| Скиннед-путь — только CPU (`bHWAnim=false`) | `WTriMesh.cpp:5443`, `:5483-5484` |
| Подмена индексов теневым листом | `WTriMesh.cpp:2035`, `:2061` |
| Теневой путь активен: `ENABLE_SHADOWVOLUME 1` | `WTriMesh.cpp:24` |
| `CTM_SHADOWVOLUMES` выключен → `bSHADOWVOLUME = unified` | `WTriMesh.cpp:4917`, `:5296-5307` |
| Генерация prim-data в unified-режиме без доп. гейтов | `WTriMesh.cpp:5618-5622`, вызов `:5680` |
| Атрибут: COLORWRITE/ALPHAWRITE/ZWRITE off, ADD, separate-stencil | `WTriMesh.cpp:5091`, `:5116-5117`, `:5132-5148` |
| `glColorMask` применяется всегда | `MDisplaySDL2.cpp:4198-4200` |
| Flush атрибутов во всех путях draw | `MDisplaySDL2.cpp:5648-5649`, `:6290-6291`, `:5018-5019` |
| Shading-проходы stencil-masked | `WTriMesh.cpp:2130-2133` |
| SHADOWVOLUME в бэкенде не реализован | grep `RenderContexts/GLES3/` = 0 |
| Эталон экструзии в шаблоне VP | `shaders/VP.xrg:690-701`, `:737-764`; `Docs/VP_Reference.md` §3.13 |

## 3. Опровергнутые гипотезы (§4 спеки)

- **4.3 (separate-stencil неверен)** — опровергнута. Маппинг op-констант
  1:1 (`MDisplaySDL2.cpp:4305-4308` против `MRender_Classes.h:643-650`),
  front/back — прямой (`:4319-4324`), `Attrib_Disable(CULL|CULLCW)` доходит
  до `glDisable(GL_CULL_FACE)` (`:4239-4242` + `MRender.cpp:2803-2815`).
- **4.5 (маппинг ADD / DIFFUSE_ONLY)** — опровергнута. ADD = `GL_ONE,GL_ONE`
  (`MRender.cpp:1250-1266` → `GLES3_MapBlend` `:1180-1197`),
  `glBlendEquation` не нужен (дефолт `GL_FUNC_ADD`). DIFFUSE_ONLY гасит
  бленд только у FP20-проходов (`:4182-4184`), теневых не касается.
- **4.2 (атрибут не доходит)** — как самостоятельная причина не
  подтвердилась: flush есть везде, `cwoff>0` в логах, построение/культинг
  атрибутов симметричны. Механизм видимости объяснён через stencil (§1b)
  без нарушения доставки атрибута.
- **4.1 (нет VP-экструзии)** — факт верен, но это НЕ причина клиньев: без
  экструзии объёмы были бы невидимыми дегенератами. Причина — OOB-индексы
  (§1a) + stencil (§1b). Экструзия нужна для полного фикса, не для
  объяснения симптома.
- **4.4 (не теневые объёмы)** — для оранжевых полос не опровергнута
  (LaserBeam); для чёрных клиньев объяснение §1 достаточно и лучше
  соответствует форме/привязке к персонажу.

## 4. Патчи диагностики (предложения, в код не внесены)

Оба — ASCII-only вставки (файлы CP1252, править байтово через python3 по
AGENTS.md), под env-флагом, выключены по умолчанию.

### 4.1 `RIDDICK_SKIP_SHADOWVOL=1` — выключить теневые объёмы TriMesh целиком

Якорь: `WTriMesh.cpp:5618` (условие генерации `m_pppShadowPrimData`).
Без prim-data `CullCluster` отсекает SW-теневые кластеры (`:4893-4894`) —
не рисуется ничего, и CPU-работа по силуэту не тратится. BSP2-тени не
затрагиваются (свой путь, `WBSP2Light.cpp`).

```cpp
	// --- RIDDICK_SKIP_SHADOWVOL: kill character stencil shadow volumes ---
	static int s_SkipShadowVol = -1;
	if (s_SkipShadowVol < 0)
	{
		const char* pE = getenv("RIDDICK_SKIP_SHADOWVOL");
		s_SkipShadowVol = (pE && pE[0] == '1') ? 1 : 0;
	}

	if (m_spShadowData &&
		!s_SkipShadowVol &&                      // <-- добавленная строка
		RenderParams.m_bRender_Unified &&
		!(RenderParams.m_RenderInfo.m_Flags & CXR_RENDERINFO_NOSHADOWVOLUMES) &&
		...(остальное условие без изменений)
```

Примечание: `XR_STENCILSHADOWS` для этого НЕ подходит — в TriMesh он гейтит
только ветку `#ifdef CTM_SHADOWVOLUMES` (`WTriMesh.cpp:5296-5298`), которая
выключена (`:4917`); у MultiTriMesh гейт есть
(`WModel_MultiTriMesh.cpp:306`), но у MTM генерация теней вырезана
(`:438-449`).

### 4.2 Счётчик `svol=N` в `[GL-DBG]` — сколько draw'ов с texgen SHADOWVOLUME*

Якорь: `DecodeTexGenChannels`, `MDisplaySDL2.cpp:3048-3051` (ветка
`DbgNoteTexGenMode`): при `iTxt==0 && (RawMode==CRC_TEXGENMODE_SHADOWVOLUME
|| RawMode==CRC_TEXGENMODE_SHADOWVOLUME2)` инкрементить счётчик; печатать в
`[GL-DBG]` рядом с `cwoff` (`:2626-2632`), сброс в `DbgResetCounters`
(`:2539`). Прямой ответ «рисуются ли объёмы вообще» в любом прогоне.

### 4.3 (опционально) `RIDDICK_SKIP_LASER=1`

По образцу `RIDDICK_SKIP_CHARS/PROPS` в `CXR_EngineImpl::RenderModel`
(`XREngine.cpp:~1598`): скип `CXR_Model_LaserBeam`. Закрывает вопрос
оранжевых полос одним прогоном.

## 5. Что запросить у владельца

По одному прогону, смотреть на наличие/отсутствие клиньев на тех же ракурсах.

**Прогон 1** (флаги уже есть, самый дешёвый):

```
RIDDICK_DBG_MODELS=1 RIDDICK_SKINNING=1 RIDDICK_DIFFUSE_ONLY=1 \
RIDDICK_NO_SCISSOR=1 RIDDICK_NO_FOG=1 RIDDICK_DBG_NDS=0 RIDDICK_NDS=1 \
RIDDICK_LFM=1 RIDDICK_LFM_SCALE=8 RIDDICK_FP20=1 RIDDICK_DBG_GL=1 \
RIDDICK_DBG_SHADER=0 RIDDICK_SKIP_PROPS=1 RIDDICK_SKIP_CHARS=1 \
RIDDICK_DIRECT_RENDER=1 RIDDICK_STARTMAP=Pa1_TheDream
```

Ожидание: клинья исчезнут вместе с персонажами (они и их объёмы в одной
ветке). Если останутся — источник не скиннед-ветка, идти по 4.4
(LaserBeam/CUSTOM).

**Прогон 2** (после внесения патчей 4.1+4.2): та же строка, но
`RIDDICK_SKIP_CHARS=0 RIDDICK_SKIP_SHADOWVOL=1`. Ожидание: клинья исчезают
при живых персонажах; в `[GL-DBG]` рост `svol` в обычном прогоне и
`svol=0` с флагом. Это окончательное подтверждение §1.

## 6. Открытые вопросы

1. Оранжевые полосы — LaserBeam или нет (патч 4.3 / `[MODEL]`-лог).
2. Какие именно проходы реально потребляют испорченный stencil в режиме
   `RIDDICK_DIFFUSE_ONLY=1` (FP20-проходы рисуются, но проваливаются в
   обычный шейдер — `MDisplaySDL2.cpp:2736,2873,3107,3259`). Если в этом
   режиме stencil-test не делает никто, механизм §1b слабеет, и вес
   получает вариант «клинья — иной draw с colormask on». Прогон 2
   различает это напрямую.
3. Латентный риск MultiTriMesh: `OnRender_Scissors` там не вызывается,
   `m_lpShadowLightAttributes[]` не инициализирован
   (`WTriMeshRIP.h:98-100`, ctor `:105-152` не обнуляет); hardware-
   SHADOWVOLUME-кластер MTM при мусорном `m_plLightCulled[iL]==0` уйдёт в
   `VB_RenderUnified` с мусорным указателем атрибута. Сейчас, видимо,
   не стреляет (генерация вырезана, `WModel_MultiTriMesh.cpp:438-449`),
   но при оживлении теней MTM — первый кандидат.
4. Почему удвоение закомментировано в оригинале (`:5789`, `:5807-5808`) —
   похоже на недоведённую оптимизацию эпохи Mondelore; на PS3 путь не
   брался (VPU/`ShadowProjectHardware_ThreadSafe_VPU`, `:5663`). На вывод
   не влияет, но при реализации фикса стоит восстановить обе строки и
   сверить с `Cluster_ProjectVertices` (`:3907`).
