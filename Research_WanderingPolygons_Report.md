# Отчёт по спеке Research_WanderingPolygons.md: «полигоны, растянутые через весь мир»

Дата: 2026-07-28. Ветка `riddick-render` (HEAD на момент исследования —
`ea6fcb2`). Исследование статическое (код + имеющиеся логи), прогонов не
было — см. §5.

---

## 1. Ответ на §3 — что рисует ленты

**Ответ (сильная гипотеза, дефект подтверждён кодом на 100%):**

Ленты — это **запечённые в уровень BSP2 теневые объёмы (per-light shadow
volume меши)**, которые рисуются видимыми из-за дефекта GLES3-бэкенда:
**`DrawCachedVB` не применяет ни атрибуты, ни матрицы** — в нём нет
вызовов `Attrib_Update()`/`Matrix_Update()`, которые есть в двух других
draw-точках.

Цепочка:

- Движок сабмитит объёмы как VBID-chain VB:
  `WBSP2Model.cpp:4469` (`pVB->Render_VertexBuffer(pVBIds[iSV].m_VBID)`),
  цвет геометрии `0xff100010` (тёмно-малиновый, `:4460`), виртуальный
  атрибут `CXR_VirtualAttributes_BSP2ShadowVolumeFront` (`:4475-4479`),
  флаг `CXR_VBFLAGS_LIGHTSCISSOR` (`:4471`). Блок активен по умолчанию
  (`bShadows = !(DebugFlags & M_Bit(13))`, `:4417`; DebugFlags=0).
- VBM-флаш выставляет их атрибут **отложенно**: через
  `OnSetAttributes` в копию + `_pRC->Attrib_End(AttrChg)`
  (`XRVBManager.cpp:3433-3491`) — это только ставит биты в
  `m_AttribChanged`, в GL ничего не уходит.
- Реификация стейта в GL происходит только в точке draw:
  `if (m_AttribChanged) Attrib_Update(); if (m_MatrixChanged) Matrix_Update();`
  — эти две строки есть в `DrawIndexed` (`MDisplaySDL2.cpp:5606-5607`) и
  `DrawUserVerts` (`:6248-6249`), но **отсутствуют в `DrawCachedVB`
  (`:4975-4995`)**, через который идут `Render_VertexBuffer(VBID)`
  (`:6542` → `:6557`) и `Render_VertexBuffer_IndexBufferTriangles`
  (`:6608` → `:6626`).
- Итог: теневой объём рисуется со стейтом **предыдущего** draw'а —
  обычно это opaque-проход мира с `glColorMask(TRUE)`, без stencil, с
  чужой текстурой и чужими юниформами (`m_pCurAttrib` обновляется только
  в `ApplyAttribs`, `:4140`). Правильный атрибут объёма (COLORWRITE off,
  stencil INCR/DECR, ZWRITE off — `WBSP2Model.cpp:96-142`) до GL не
  доходит вообще.

Экструдированные «через весь мир» меши с почти чёрным цветом, видимые
из-за протухшего colorwrite — это в точности симптом из §1 спеки. Авторы
порта уже натолкнулись на эту пару «мир невидим / объёмы видны» раньше:
комментарий `MDisplaySDL2.cpp:6603-6607` прямо описывает симптом
TheDream (но тогда чинили пропавший override, а не стейт).

**Побочные следствия той же дыры (подтверждено кодом):**

- Stencil-буфер объёмами не пишется → свет течёт сквозь стены (согласуется
  с пометкой в AGENTS.md про `XR_STENCILSHADOWS`).
- SLC world-кластеры (`WBSP2Model.cpp:4182` →
  `Render_VertexBuffer_IndexBufferTriangles`) рисуются через ту же дыру;
  мир выглядит корректно, предположительно, потому что соседние draw'ы
  мира функционально одностейтные (opaque+zwrite), а матрицы в пределах
  кадра константны (это объяснение — гипотеза, не факт).

**Один эксперимент, разделяющий оставшиеся варианты:** патч §4.1
(две строки — flush в `DrawCachedVB`) + прогон TheDream. Если ленты
исчезли — источник подтверждён окончательно. Если остались — второй
кандидат, конусы `CXR_Model_SpotLightVolume` (§3.2), отделяется прогоном
с `RIDDICK_SKIP_SPOTVOL=1`.

---

## 2. Доказательства

### 2.1 Движок выключает COLORWRITE теневым объёмам корректно

- `CXR_VirtualAttributes_BSP2ShadowVolumeFront::PrepareFrame`
  (`WBSP2Model.cpp:96-142`): `Disable = ALPHAWRITE|ZWRITE`,
  `if(!_ColorWrite) Disable |= CRC_FLAGS_COLORWRITE` (`:128-129`);
  `_ColorWrite` — debug-бит 12 (`:4552-4555`), по умолчанию 0.
- Динамические объёмы (`Light_RenderDynamicLight`,
  `WBSP2Light.cpp:1684`): `Attrib_Disable(ColorWriteDisable|ALPHAWRITE|
  ZWRITE)` (`:1939-1942`), `ColorWriteDisable = CRC_FLAGS_COLORWRITE`.
- TriMesh-объёмы (`WTriMesh.cpp:5061-5155`): тот же паттерн.
- Веток `PLATFORM_LINUX`/`PLATFORM_CONSOLE`, снимающих disable, нет.

Т.е. «движок забыл выключить цвет» — НЕ причина.

### 2.2 Бэкенд теряет применение атрибута на кэш-пути

- `DrawCachedVB` целиком — `MDisplaySDL2.cpp:4975-4995`: бинд VBO,
  `SetupCommonUniforms(false)`, `glDrawElements`. Ни `Attrib_Update`,
  ни `Matrix_Update`.
- Контраст: `DrawIndexed` (`:5606-5607`) и `DrawUserVerts` (`:6248-6249`)
  flush'ат. `m_AttribChanged` липкий (только `|=` и `= -1`,
  `MRender.cpp:2774+`), поэтому flush в этих двух точках происходит
  фактически на каждый draw — но cached-draws между ними стейта не
  получают.
- Движковая сторона выставляет атрибут VB отложенно:
  `XRVBManager.cpp:3422-3491` (`Attrib_End(AttrChg)`) и
  `CXR_VertexBuffer::SetAttrib` (`XRVertexBuffer.cpp:229-234`,
  `Attrib_Set(const CRC_Attributes&)` — deferred-версия,
  `MRender.cpp:2676`).
- `m_pCurAttrib` (от него берутся текстуры/бленд/флаги в
  `SetupCommonUniforms`) присваивается только в `ApplyAttribs`
  (`MDisplaySDL2.cpp:4140`) → для cached-draws он тоже протухший.
- Счётчики лога согласуются: в игровом кадре `attr=19680` при
  ~36k draw'ов за 60 кадров (`run_dream.log`, строка `[GL-DBG]` с
  `VBID=16304`) — установок атрибутов меньше, чем draw'ов.

### 2.3 Лог run_dream.log (прогон из §1 спеки)

- **Лог снят ДО фикса линковки 3D-шейдера**: `link failed` присутствует
  (`run_dream.log:505`, ровно один раз — программа `m_3DShader`). Все
  выводы по этому логу — с поправкой спеки §2.2 (3D-draw'ы шли через
  fallback).
- Игровой кадр: `prim=15360 VBID=16304 fp20=20984 attr=19680
  cached=25964 streamed=0 vconv=2844232 vmemo=9921600
  skin=0 mi0=0 strmMI0=0 skip=0 lastFmt=-1`.
- 97 скиннед-VBID построено кэшем (`[GLES3-GEOM] VBID=N skinned:
  MI0=1 MW0=0 ...`), но **ни одного** скиннед-draw: `mi0=0`,
  `strmMI0=0`, `skin=0` во всех 201 строках `[GL-DBG]`. Перепроверка
  §2.1 спеки подтверждает: скиннед-геометрия precache'ится, но не
  рисуется вообще (см. открытый вопрос §6.1).
- `streamed=0` во всех игровых интервалах → легаси-стриминг по VBID
  в этом прогоне не использовался (важно для §3.3).
- `[BSP2] PVS entries: 11895` — PVS есть, «рисуются все листы» не
  причина.

### 2.4 Арифметика индексов VBID/IBID (гипотеза §4.4 спеки) — расхождений нет

- `_PrimOffset` — в 16-битных индексах (`WBSP2Loader.cpp:1133-1136`,
  пулы режутся по 0xffff — `:1120-1130`); `ByteOffset = _PrimOffset * 2`
  (`MDisplaySDL2.cpp:6625`) эквивалентно PS3
  `pIB->m_PrimOffset + (_PrimOffset << 1)` (`MRenderPS3_Render.cpp:263`).
- Base vertex не нужен ни там, ни там: индексы пула VB-локальные
  (`WBSP2Loader.cpp:561-576`), вершины заливаются с нулевой
  (`GLES3_Geometry.cpp:338-351`, PS3 — `MRenderPS3_Geometry.cpp:345+`).
- `m_nPrim` для RIP_TRIANGLES = число треугольников с обеих сторон
  (`WBSP2Misc.cpp:86-90`, `XMDCommn.cpp:5681-5693`;
  кэш `nIdx = m_nPrim*3` — `GLES3_Geometry.cpp:386`, PS3 —
  `MRenderPS3_Geometry.cpp:820-822`).
- Диагностика `[GLES3-VBIT]` в `run_dream_novbcache.log:1456+` показывает
  `off` и `range` в пределах пула — выхода индексов за nV не видно.

---

## 3. Опровергнутые гипотезы (§4 спеки)

### 3.1 §4.1 в исходной формулировке — уточнена, не опровергнута

Вопрос был «доходит ли до GL выключение записи цвета». Ответ: **не
доходит**, но не из-за потери `m_pAttrib` и не из-за редкого
`ApplyAttribs` — а потому что целый draw-путь (`DrawCachedVB`) не имеет
точки применения стейта. Дефект подтверждён, механизм другой.

### 3.2 §4.2 SpotLightVolume — НЕ опровергнута, но понижена

- `CXR_Model_SpotLightVolume` (`WModel_Flare.cpp:586-783`) — не конус-меш,
  а цепочка из 8 view-aligned билбордов 16–58 ед. диаметром вдоль оси
  света (`:692-708`) + флейр-квад. Форма («лента от источника, всегда к
  камере») структурно совместима с симптомом.
- Но: идёт через `Render_Surface` → CPU VBCHAIN → `DrawIndexed`
  (`XRVertexBuffer.cpp:312-333`) — путь **с** flush'ем атрибутов, дыра
  §2.2 его не касается.
- `RIDDICK_DIFFUSE_ONLY` его бленд НЕ гасит: подавление бленда гейтится
  на FP20 ext-attrib (`MDisplaySDL2.cpp:4174-4176`), а к VB конуса
  FP20-attrib не прицепляется никогда (`_pParams=NULL`,
  `WModel_Flare.cpp:723-724`).
- «Тёмность» зависит от данных поверхности `SPECIAL_LCV`: если её нет в
  EFBB-ресурсах, `GetSurfaceID` молча отдаёт дефолтную opaque-поверхность
  (`XRSurfaceContext.cpp:471-487`) — тогда 8 непрозрачных квадов с
  ZWrite. Из кода не проверить. Дёшево отделяется прогоном
  `RIDDICK_SKIP_SPOTVOL=1`.

### 3.3 §4.3 легаси-пути — в проблемном прогоне НЕ активны (опровергнуто для этого лога), но найден латентный баг

- `streamed=0` во всех игровых интервалах run_dream.log → стриминг по
  VBID не работал; `skip=0 lastFmt=-1` → форматных пропусков нет;
  неподдержанный формат позиции скипает меш целиком
  (`MDisplaySDL2.cpp:6405-6410`), мусорных вершин не даёт.
- `vconv=2.8M/60 кадров` — это DrawIndexed-fallback (`:5735`), полные
  VB/кластера на draw, в основном CPU `m_Geom`-draw'ы (партиклы, туман
  NHF, UI) — мемо инвалидируется каждым `Geometry_VertexBuffer`
  (`:6115-6129`). Аномалии не подтверждается.
- **Латентный баг (подтверждён кодом, в прогоне не стрелял):** стриминг-
  фолбэк `Render_VertexBuffer` игнорирует `VBB.m_PrimType`
  (`MDisplaySDL2.cpp:6574-6592`): сырой TRIANGLES-массив TriMesh
  (`WTriMesh.cpp:8481-8488`) парсится как типизированный поток →
  мусорный `nInd`/индексы → ровно «ленты через мир». Эталон PS3 ветвится
  по типу (`MRenderPS3_Geometry.cpp:786-822`), наш кэш тоже
  (`GLES3_Geometry.cpp:382-439`) — сломан только фолбэк. Стрельнёт при
  любом `RIDDICK_NO_VBCACHE=1` / скиннед без `RIDDICK_SKINNING` / WIRES.
  Патч — §4.3.

### 3.4 §4.4 индексы за пределами VB — опровергнута (§2.4 выше)

### 3.5 §4.5 частицы/спрайты — опровергнута

Частицы (`XRPContainer.cpp:25-76`) — мелкие alpha-blend квады через
`DrawIndexed` (flush есть); спрайт (`WModel_Flare.cpp:351-417`) —
одиночный билборд. Ленту через мир дать не могут при корректных данных;
мусорных источников координат в их путях не найдено.

### 3.6 Скиннинг (перепроверка §2.1 спеки) — ленты НЕ персонажи

`skin=0 mi0=0 strmMI0=0` при 97 построенных VBID → скиннед-геометрия не
доходит до draw вообще. Bone-local вершины на экране отсутствуют.
(Почему персонажи не рисуются — отдельный открытый вопрос §6.1.)

---

## 4. Патчи диагностики (готовые фрагменты, точки вставки)

### 4.1 FIX-кандидат: flush стейта в DrawCachedVB (2 строки)

`Source/P5/Shared/MOS/RenderContexts/GLES3/MDisplaySDL2.cpp:4977`,
после строки `if (!_E.m_VBO || !_IB.m_IBO || _nIdx <= 0) return false;`:

```cpp
		// VBID draw-paths (Render_VertexBuffer[_IndexBufferTriangles]) reach
		// here without the Attrib/Matrix flush that DrawIndexed:5606 and
		// DrawUserVerts:6248 do. Without it cached draws inherit the
		// previous draw's GL state -- precomputed BSP2 shadow volumes
		// became visible (colour-write left on) and never wrote stencil.
		if (m_AttribChanged) Attrib_Update();
		if (m_MatrixChanged) Matrix_Update();
```

Это ровно тот же паттерн, что в двух других draw-точках; семантика
`Attrib_Update`/`Matrix_Update` — `MRender.cpp:2636-2640` и
`:3383-3440`. Побочный эффект по замыслу: теневые объёмы начнут писать
stencil (свет перестанет течь сквозь стены) и станут невидимыми.

### 4.2 Проверка срабатывания дыры до фикса (опционально, если нужен лог-доказательство)

Временно, ДО патча 4.1, в начало `DrawCachedVB` (тот же якорь):

```cpp
		static int s_nCachedStale = 0;
		if (m_AttribChanged && s_nCachedStale < 40)
		{
			++s_nCachedStale;
			const CRC_Attributes& A = m_lAttribStack[m_iAttribStack];
			ConOutL(CStrF("[CACHED-STALE] nIdx=%d stackFlags=%08x appliedTex0=%d",
				_nIdx, A.m_Flags,
				m_pCurAttrib ? (int)m_pCurAttrib->m_TextureID[0] : -1));
		}
```

Строки `[CACHED-STALE]` с несовпадающими флагами — прямое доказательство,
что кэш-draws идут с невыставленным атрибутом. После фикса удалить.

### 4.3 FIX латентного бага: ветвление по m_PrimType в streaming Render_VertexBuffer

`MDisplaySDL2.cpp:6574`, заменить безусловный
`CRCPrimStreamIterator It(VBB.m_piPrim, VBB.m_nPrim);` блок на:

```cpp
		if (VBB.m_PrimType == CRC_RIP_TRIANGLES)
		{
			// Raw index list, no stream headers (TriMesh default,
			// WTriMesh.cpp:8481-8488). PS3: MRenderPS3_Geometry.cpp:786-822.
			DrawUserVerts(GL_TRIANGLES, pVerts, nV, VBB.m_piPrim, VBB.m_nPrim * 3);
		}
		else if (VBB.m_PrimType == CRC_RIP_STREAM)
		{
			CRCPrimStreamIterator It(VBB.m_piPrim, VBB.m_nPrim);
			// ... существующий цикл do/while без изменений ...
		}
		else
		{
			static int s_nBadPrim = 0;
			if (s_nBadPrim++ < 8)
				ConOutL(CStrF("[GLES3-VB] unsupported PrimType %d in streamed VB", (int)VBB.m_PrimType));
		}
```

### 4.4 `RIDDICK_DBG_HUGE=<единиц>` — AABB-детектор «через весь мир» (из §5.1 спеки)

- В `GLES3_Geometry.cpp::Build` после интерливинга: один проход по
  позициям (смещение `m_lRegOffset[CRC_VREG_POSITION]`, stride из
  entry), сохранить `m_AABBMin/Max` в `SGLES3GeomEntry`.
- В `DrawCachedVB` и в fallback-ветке `DrawIndexed` (`MDisplaySDL2.cpp:
  5732-5735`): если диагональ AABB > порога — лог (кап 40): путь
  (cached/streamed/geom), VBID/IBID, nV, nIdx, `m_Flags` текущего стека,
  тип ext-attrib, TextureID[0], диагональ. Для кэш-пути первые вершины
  недоступны (CPU-копии нет) — AABB достаточно.

### 4.5 Счётчик `cwoff=N` в `[GL-DBG]` (из §5.3 спеки)

В `ApplyAttribs` (`MDisplaySDL2.cpp:4137+`, рядом с `glColorMask` на
`:4192`): `if (!(A.m_Flags & CRC_FLAGS_COLORWRITE)) ++m_DbgColorWriteOff;`
— печатать в строке `[GL-DBG]`. Одно число покажет, применяются ли
вообще атрибуты с выключенной записью цвета (после фикса 4.1 должно
стать >0 на картах с тенями).

### 4.6 `[MODEL]` + bounding box (из §5.4 спеки)

В `CXR_EngineImpl::RenderModel` (`XREngine.cpp:~1598`, рядом с
существующим `[MODEL]` под `RIDDICK_DBG_MODELS=1`) добавить в строку
мировой AABB модели — сразу виден объект абсурдного размера.

Все новые флаги — задокументировать в `Docs/HacksAndHooks.md` (п.6
ограничений спеки).

---

## 5. Что запросить у владельца

Прогон А (верификация основной гипотезы), после применения патча 4.1
(опционально 4.2/4.5 для логов), на коммите `a0569c7` или новее:

```
RIDDICK_SKINNING=1 RIDDICK_DIFFUSE_ONLY=1 RIDDICK_NO_FOG=1 \
RIDDICK_NDS=1 RIDDICK_LFM=1 RIDDICK_LFM_SCALE=8 RIDDICK_FP20=1 \
RIDDICK_DBG_GL=1 RIDDICK_DBG_MODELS=1 \
RIDDICK_DIRECT_RENDER=1 RIDDICK_STARTMAP=Pa1_TheDream \
./build/desktop-x86_64/bin/openriddick -datapath /mnt/data_storage/sashikknox/Games/Riddick
```

Смотреть: (1) исчезли ли ленты визуально; (2) в логе нет `link failed`;
(3) `[CACHED-STALE]`-строки до фикса / `cwoff>0` после; (4) свет больше
не течёт сквозь стены (stencil заработал).

Прогон Б (только если ленты остались): то же + `RIDDICK_SKIP_SPOTVOL=1`.
Если ленты уходят — источник конусы прожекторов (§3.2), дальше копать
поверхность `SPECIAL_LCV` в данных.

Дополнительно (независимо): прогон с `RIDDICK_DBG_HUGE=200` (патч 4.4) —
даст прямой список «гигантских» draw'ов, если после 4.1 останется
второй источник.

---

## 6. Открытые вопросы

1. **Почему персонажи не рисуются вообще.** 97 скиннед-VBID построены
   (`[GLES3-GEOM]`), но ни один draw их не несёт (`mi0=0 strmMI0=0
   skin=0`, `streamed=0`). Значит VBID'ы персонажей не доходят до
   `Render_VertexBuffer*` — отсекаются выше (RenderModel/VBM/анимации).
   Это отдельная задача («моделей на экране нет»), не связанная с лентами
   напрямую; искать в `XREngine.cpp::RenderModel` → VBM или в AG2-гейтах.
2. Почему мир визуально корректен при той же дыре в `DrawCachedVB`
   (SLC-кластеры тоже без flush'а) — предположительно одностейтность
   соседних world-draw'ов; после патча 4.1 проверить, что визуал мира не
   изменился в худшую сторону.
3. Содержимое поверхности `SPECIAL_LCV` в EFBB-данных (определяет
   «тёмность» конусов §3.2) — из кода не видно; нужен лог miss'а в
   `CXR_SurfaceContext::GetSurfaceID` (`XRSurfaceContext.cpp:481-484`).
4. Побочные находки (не ленты, зафиксировать на будущее):
   - стриминг-пути GLES3 нигде не вызывают `VB_Release` (рост памяти,
     спам «Temp data not released», `WBSP2Misc.cpp:66-69`);
   - `VRegFetch` нормализует NS_I16 как 1/32767 (`MDisplaySDL2.cpp:6351`)
     против 1/32768 в эталонном `ConvertRegisterFormat`
     (`MRender.cpp:1476-1489`);
   - `ConvertRegisterFormat` не покрывает `NS1_I16`/`NU*_I16`
     (`MRender.cpp:1419-1553`) — кэш-путь упадёт `Error_static`, если
     такой формат встретится;
   - `m_GeomColor` не входит в ключ мемо `m_GeomMemo` — устаревший
     константный цвет при мультипассе одной геометрии;
   - `WBSP2StencilLight.cpp` не входит в сборку (мёртвый код).
