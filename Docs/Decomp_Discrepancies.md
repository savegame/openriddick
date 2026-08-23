# Расхождения PS3-снапшота с PC/Dark Athena retail (декомпил)

Проход, сфокусированный на живом дефекте: часть персонажей анимация ходьбы
"застыла" — скользят в bind-позе, `[GLES3-SKIN] no palette for a skinned draw`
доходит до 10000. Цель — сверить построчно с декомпилом ретейла пять мест,
названных владельцем задачи (`port_status.md`, замер 2026-08-08, третий заход).

Источники декомпила (только grep, узкий контекст — см. `AGENTS.md`):
`MXR_dll_decomp.c`, `GameClasses_Win32_x86_dll_decomp.c`,
`GameWorld_Win32_x86_dll_decomp.c`, `MSystem_dll_decomp.c`,
`RndrGL_dll_decomp.c`, `MCCDyn_dll_decomp.c`.

---

## Проверено, расхождений нет

### 1. `CTM_VertexBuffer::ReadOld` — установка `m_bHaveBones`/`m_bHaveBoneMatrixMap` (BDVertexInfo-путь)

- Наша сторона: `Source/P5/Shared/MOS/XRModels/Model_TriMesh/XMDCommn.cpp:3096-3120`
  ```cpp
  if (m_lBDVertexInfo.Len())
  {
      m_bHaveBones = true;
      if (_pC->m_lBDMatrixMap.Len())
          m_bHaveBoneMatrixMap = true;
  }
  ```
- Декомпил: `MXR_dll_decomp.c:260155-260165` (функция `FUN_10184e90`,
  `__thiscall`, начало на `MXR_dll_decomp.c:259754`; опознана по строкам
  `"TVERTEXFRAMES"`, `"BONEMATRIXMAP"`, `"BONEDEFORM"`, `"TRIANGLES"`,
  `"CTriangleMeshCore::Read"` — это `CTM_VertexBuffer::ReadOld`, тот же код,
  что и в `MXR.dll`, ранее сопоставлен в `Decomp_Map.md` как `FUN_10122f00`
  в GameClasses-копии):
  ```c
  if ((*(int *)((int)local_230 + 0x14) != 0) &&
     (*(int *)(*(int *)((int)local_230 + 0x14) + 4) != 0)) {
      *(uint *)((int)local_230 + 0x84) = *(uint *)((int)local_230 + 0x84) | 0x4000;
      if ((*(int **)(param_2 + 0x34) != (int *)0x0) && (**(int **)(param_2 + 0x34) != 0)) {
          *(uint *)((int)local_230 + 0x84) = *(uint *)((int)local_230 + 0x84) | 0x8000;
      }
  }
  ```
- Вывод: `(ptr!=0 && count!=0)` на `local_230+0x14` = наш `m_lBDVertexInfo.Len()`,
  флаг `0x4000` в поле `+0x84` = `m_bHaveBones`; вложенная проверка на
  `param_2+0x34` (указатель на `_pC->m_lBDMatrixMap`) + флаг `0x8000` =
  `m_bHaveBoneMatrixMap`. **Совпадает байт в байт**, включая порядок
  проверок и условие вложенности (bone-matrix-map флаг выставляется только
  если уже выставлен bones-флаг). **Подтверждено.**

### 2. `CTM_VertexBuffer::Read` (новый путь, `ReadBoneDeform_v2`) — установка `m_bHaveBones`

- Наша сторона: `XMDCommn.cpp:3746-3768` (функция `CTM_VertexBuffer::Read`,
  строка ошибки `"Error 3"` после `GetNext("TVERTEXFRAMES")`):
  ```cpp
  if (_pDFile->GetNext("BONEDEFORM"))
  {
      m_bHaveBones = true;
      m_FilePosBoneDeform = pF->Pos();
      if (!bDelayLoad)
          ReadBoneDeform_v2(pF);
  }
  ```
  (без работы с `BONEMATRIXMAP` вовсе — это отдельная от ReadOld функция).
- Декомпил: `MXR_dll_decomp.c:260486-260544` — опознана буквальным попаданием
  строки `"CTM_VertexBuffer::Read"` (`:260494`, `Error 3`) как ошибки:
  ```c
  bVar3 = CDataFile::GetNext(param_2,"BONEDEFORM");
  if (bVar3) {
      *(uint *)((int)local_224 + 0x84) = *(uint *)((int)local_224 + 0x84) | 0x4000;
      _Var14 = CCFile::Pos(this_00);
      *(int *)((int)this_01 + 0x6c) = (int)_Var14;
      if ((char)local_234 == '\0') {
          FUN_1017dd80(this_01,this_00);   // ReadBoneDeform_v2
      }
  }
  ```
  Никакого чтения/установки `BONEMATRIXMAP`-флага в этой функции нет — так же,
  как и у нас. **Подтверждено, расхождений нет.**

### 3. `CTriangleMeshCore::Read` — вывод `m_bVertexAnim`/`m_bTVertexAnim`/`m_bMatrixPalette` из VB

- Наша сторона: `XMDCommn.cpp:5352-5372`:
  ```cpp
  m_bVertexAnim = false; m_bTVertexAnim = false; m_bMatrixPalette = false;
  for (int i = 0; i < nVB; ++i) {
      CTM_VertexBuffer *pVB = GetVertexBuffer(i);
      if (pVB->m_lVFrames.Len() > 1)  m_bVertexAnim = true;
      if (pVB->m_lTVFrames.Len() > 1) m_bTVertexAnim = true;
      if (pVB->m_bHaveBones)          m_bMatrixPalette = true;
  }
  ```
- Декомпил: `MXR_dll_decomp.c:262518-262552` (сразу после блока `"SHADOWDATA"`,
  `:262479`, той же функции `CTriangleMeshCore::Read`):
  ```c
  *(uint *)(pCVar20 + 0x50) = *(uint *)(pCVar20 + 0x50) & 0xffffff80;   // сброс битов 0..6
  ... for каждого VB (iVar16) ...
      if ((*(int **)(iVar16 + 8) != NULL) && (1 < **(int**)(iVar16+8)))
          flags |= 1;                       // VFrames.Len() > 1  -> m_bVertexAnim
      if ((*(int **)(iVar16 + 0xc) != NULL) && (1 < **(int**)(iVar16+0xc)))
          flags |= 2;                       // TVFrames.Len() > 1 -> m_bTVertexAnim
      if ((*(uint *)(iVar16 + 0x84) & 0x4000) != 0)
          flags |= 4;                       // m_bHaveBones (тот же бит 0x4000) -> m_bMatrixPalette
  ```
  Три условия, три бита, тот же порядок, тот же источник (`m_bHaveBones` —
  ОДИН бит `0x4000` что и в п.1/п.2 — переиспользуется как есть, никакого
  дополнительного условия на LOD/кластер нет). **Подтверждено, расхождений
  нет.**

  **Важно для живого бага:** это значит, что `m_bMatrixPalette` в ретейле
  выводится **точно так же, для КАЖДОГО `CXR_Model_TriangleMesh` независимо**
  (в т.ч. LOD-меша — LOD-меш является отдельным `CTriangleMeshCore`/объектом
  со своим вызовом `Read`). Ретейл не делает ничего специального для
  LOD-мешей — если LOD-геометрия в данных не несёт `BONEDEFORM`/bone-инфо,
  её `m_bMatrixPalette` будет `false` и в ретейле тоже. Разбор кода не
  выявил здесь порт-специфичного расхождения — если "застывшие" персонажи
  оказываются LOD-мешами без костных данных, это следствие содержимого
  ассетов/выбора LOD, а не бага чтения.

### 4. `CXR_Skeleton::EvalAnim` — гейт `bFullLayerFound` и QNaN-заливка

- Наша сторона: `Source/P5/Shared/MOS/XR/XRSkeleton.cpp:2526-2587`:
  ```cpp
  bool bFullLayerFound = false;
  for(int i = _nLayers-1; i >= 0; i--)
      if (!_pLayers[i].m_iBlendBaseNode && (_pLayers[i].m_Blend > 0.999f))
          bFullLayerFound = true;
  if (!bFullLayerFound) {
      MemSetD(_pSkelInst->m_pBoneLocalPos, 0x7Fc00000, _pSkelInst->m_nBoneLocalPos * sizeof(CMat4Dfp32) >> 2);
      MemSetD(_pSkelInst->m_pBoneTransform, 0x7Fc00000, _pSkelInst->m_nBoneTransform * sizeof(CMat4Dfp32) >> 2);
      return;
  }
  if (INVALID_V3(row0)||INVALID_V3(row1)||INVALID_V3(row2)||INVALID_V3(row3)) { ...; return; }
  ```
- Декомпил: `MXR_dll_decomp.c:216397-216460` (внутри функции с телом
  ~1300 строк, начало сигнатуры `MXR_dll_decomp.c:215351`; опознана по
  константе `0x7fc00000` — QNaN-заливка, встречается всего 2 раза в файле):
  ```c
  // unrolled-loop эквивалент "for(i=_nLayers-1;i>=0;i--) if(!BlendBaseNode && Blend>0.999) bVar19=true"
  if ((*(short *)(pfVar24 + 5) == 0) && (0.999 < *pfVar24)) bVar19 = true;
  ...
  if (!bVar19) {
      MemSetD(*(undefined4*)(param_3+0x20), 0x7fc00000, (ushort)*(param_3+0x3a) << 4);
      MemSetD(*(undefined4*)(param_3+0x24), 0x7fc00000, (ushort)*(param_3+0x3c) << 4);
      return ...;
  }
  // 16-кратная проверка (uint)param_4[k] & 0x7f800000 != 0x7f800000 для k=0..15
  // (эквивалент 4x INVALID_V3 на 4 строки _WMat) -- если ВСЕ валидны, вызов
  // FUN_10144ae0 (EvalTracks) и продолжение; иначе функция завершается без
  // вызова EvalTracks -- тот же ранний return, что и у нас.
  ```
  `<< 4` = умножение на 16 dword = 64 байта = `sizeof(CMat4Dfp32)`, то есть
  тот же расчёт длины, что и наш `* sizeof(CMat4Dfp32) >> 2`. Порядок
  (сначала гейт `bFullLayerFound`+QNaN-заливка+return, потом проверка
  валидности `_WMat` без повторной заливки) **совпадает**. **Подтверждено,
  расхождений нет.**

  *Оговорка по уверенности:* сама функция в декомпиле — гигантский слитый
  блок (компилятор объединил в один `FUN`, вероятно `EvalAnim`+часть
  `EvalTracks`/применения трансформа), однозначно её единственное имя не
  восстановлено. Совпадение проверено **по значениям констант и порядку
  операций**, не по имени функции — это чуть ниже "построчно опознанной
  функции", но достаточно надёжно (константа `0x7fc00000` + сдвиг `<<4` +
  структура условий совпадают избыточно точно для совпадения случайно).

---

## Найденные расхождения

### R1. Набор ключей `XR_*` в реестре окружения различается (ПОДТВЕРЖДЕНО)

* наша сторона: `Source/P5/Shared/MOS/XR/XREngine.cpp:1372-1392`
  (`CXR_Engine::Create`, блок `MACRO_GetSystemEnvironment(pReg)`);
* сторона ретейла: `MXR_dll_decomp.c`, все строковые литералы `"XR_*"`.

Метод: `grep -o '"XR_[A-Z0-9_]*"' MXR_dll_decomp.c | sort -u` — полный
список ключей, которые ретейл вообще упоминает строкой.

**Есть у ретейла, нет у нас:** `XR_SSAO`, `XR_MOTIONBLUR_DOF`,
`XR_SOFTSTENCIL`, `XR_CHARSHADOWS`, `XR_FORCE_POT_TEXTURES`,
`XR_MAXVBCOUNT`, `XR_SPLINETESSLEVEL`, `XR_SHADERMODE*`, `XR_MODE*`.
Это поздние PC/Dark Athena добавления — ожидаемо для более нового билда.

**Есть у нас, нет у ретейла:** `XR_LODOFFSET`, `XR_LODSCALE`,
`XR_DLIGHT`, `XR_FASTLIGHT`, `XR_SHADOWDECALS`, `XR_ZFOG`, `XR_VFOG`,
`XR_NHFOG`, `XR_STENCILSHADOWS`, `XR_PORTALTEXTURESIZE`,
`XR_SHADOWDECALTEXTURESIZE`.

Отсутствие строки само по себе не доказывает отсутствие механизма (ключ
мог быть переименован или собираться в рантайме), но здесь вывод сильнее
обычного: **соседние ключи из ТОГО ЖЕ блока чтения** (`XR_FLARES`,
`XR_WALLMARKS`, `XR_SURFOPTIONS`) в декомпиле присутствуют, а эти — нет.
Значит блок в ретейле переписан, а не просто иначе отрендерен Ghidra.

Показательная пара: у нас `XR_STENCILSHADOWS`, у ретейла на его месте
`XR_CHARSHADOWS` + `XR_SOFTSTENCIL` — тени персонажей вынесены в
отдельный переключатель.

**Последствие для порта: влияет, и сразу в двух местах.**
1. `XR_LODOFFSET`/`XR_LODSCALE` в ретейле не читаются — значит выбор LOD
   там управляется иначе (или не управляется вовсе). Для разбора живого
   дефекта «застывшая ходьба» это важно: наш форсирующий LOD 0 приём
   `RIDDICK_ENV="XR_LODOFFSET=-1000000"` — это рычаг НАШЕГО снапшота, а
   не воспроизведение ретейла, и вывод «в ретейле LOD ведёт себя так же»
   из него делать нельзя.
2. Ключи теней разные, что стоит помнить при следующем заходе на
   стенсильные тени персонажей.

---

## Не удалось проверить в рамках бюджета (только grep, без адресов)

**Обновление 2026-08-08 (проход "граф вызовов"):** новый заход раскрутил
граф от подтверждённого `CTriangleMeshCore::Read` и нашёл точный кластер
адресов вокруг него — `CXR_Model_TriangleMesh::Read` (`FUN_10139af0`),
конструктор `CTriangleMeshCore` (`FUN_10189d90`), деструктор
(`FUN_10189e90`), парный `Write` LOD-секции (`FUN_10179c90`, даёт смещение
LOD-массива `+8` байт от объекта) — подробности и адреса в
`Docs/Decomp_Map.md` («Граф вызовов: выведенные пары…»). Заодно нашёлся и
устранён баг инструмента (`Tools/decomp_inventory.py` неправильно считал
границы функций из-за char-литералов `'{'`/`'}'` — молчал на 42-80% файла
в 4 из 6 декомпилов; см. `Docs/Decomp_Coverage.md` §7.1), что подняло общее
число размеченных `FUN_` с 28 745 до 49 236. **`GetLOD` сам, тем не менее,
по-прежнему не найден** — виртуальные методы не гарантированно лежат рядом
с `Read`/ctor/dtor в скомпилированном бинарнике, а `CVec3Dfp32::Length`
(нужен GetLOD) — inline-класс без RTTI, невозможно найти по имени. Три
цели ниже (2, 3 и `CalculateLocalMatrices`) остаются в статусе
"неизвестно" — см. также резюме в конце файла и обновлённую карту соседей
в `Decomp_Map.md`.

Следующие три места — **чистая логика без строковых литералов и без
уникальных числовых констант**, и при этом:
- находятся в функциях-членах `CXR_Model_TriangleMesh`, которые компилятор
  раскладывает по декомпилу как сотни соседних `FUN_xxxxxxxx` без имён;
- это виртуальные методы (`GetLOD`, `OnRender2`-путь) или мелкие приватные
  хелперы (`Cluster_SetMatrixPalette`) — не опознаются ни по RTTI-имени
  класса (это совпадение сработало один раз, на регистрации класса, не на
  методе), ни по вызову с уникальной строкой;
- перебор по границам функций / размеру тела (сотни кандидатов в диапазоне,
  где физически лежит код `Model_TriMesh`) не окупается в оставшемся
  бюджете вызовов инструмента.

Согласно `AGENTS.md` ("если нужен декомпайл конкретной функции — попросить
пользователя, он выгрузит из Ghidra быстрее") — здесь нужен точный адрес
`FUN_xxxxxxxx` от пользователя (например, через поиск по вызову
`GetLOD`/`Cluster_SetMatrixPalette` в Ghidra по перекрёстным ссылкам,
которые недоступны через текстовый grep).

1. **`WTriMesh.cpp:5807`** — гейт `if (pSkelInstance && bAnim && !m_bMatrixPalette) bAnim = false;`
   (окружение `OnRender2`, строки ~5620-5810). Не локализовано в декомпиле.
   Статус: **неизвестно**. Косвенно относится п.3 выше (вывод
   `m_bMatrixPalette`, подтверждён) — сам гейт использует уже проверенное
   поле, но само условие гейта (именно `pSkelInstance && bAnim &&
   !m_bMatrixPalette`, а не что-то ещё, например явная проверка LOD) не
   сверено.
2. **`WTriMesh.cpp:514` `CXR_Model_TriangleMesh::GetLOD`** — формула
   `Dist += m_LODOffset; Dist *= m_LODScale;` и цикл сравнения с
   `m_lLODBias[i]` по убыванию `i`. Не локализовано. Статус: **неизвестно**.
3. **`WTriMesh.cpp:1123` `Cluster_SetMatrixPalette`** — ремап
   `m_piMatrices`/`BONEMATRIXMAP`, поведение при `pVBM->Alloc()==NULL`
   (наш код возвращает `false` при нехватке VB-арены — это порт-специфичный
   путь, retail на PC не использует ту же VB-арену/бэкенд, так что здесь
   вероятнее всего **нет прямого аналога для сравнения** — PC-рендерер
   строит палитру для GPU-скиннинга другим бэкендом (`RndrGL_dll_decomp.c`,
   0 совпадений `0x7fc00000`/др. якорей в нём для этой функции в рамках
   проверенного). Статус: **неизвестно**, требует адрес от пользователя.
4. **`XRSkeleton.cpp:2668,2699` `CalculateLocalMatrices`** — граница
   `Min(m_lNodes.Len(), (int)_pSkelInstance->m_nBoneTransform)`. Не
   локализовано (нет уникальной строки/константы рядом — `InverseOrthogonal`
   и локальная геометрия узлов не дают текстового якоря). Статус:
   **неизвестно**.

---

## Резюме по приоритетным целям (1-5 из задания)

| # | Место | Результат |
|---|---|---|
| 1 | `WTriMesh.cpp:5751-5810` гейт `bAnim` в `OnRender2` | не проверено (нет якоря) |
| 2 | `WTriMesh.cpp:514` `GetLOD` | не проверено (нет якоря) |
| 3 | `WTriMesh.cpp:1123` `Cluster_SetMatrixPalette` | не проверено (нет якоря; вероятно порт-специфичный код без прямого аналога) |
| 4 | `XMDCommn.cpp:3096-3120,3760,5352-5372` (`m_bHaveBones`/`m_bMatrixPalette`) | **подтверждено, расхождений нет** (3/3 подпункта) |
| 5 | `XRSkeleton.cpp:2374/2460` (QNaN-заливка, граница `Min(...)`) | заливка QNaN и гейт `bFullLayerFound` — **подтверждено, расхождений нет**; граница `Min()` в `CalculateLocalMatrices` — не проверено |

Значимый вывод для живого бага: код, отвечающий за то, ЧТО читается из
файла и КАК из этого выводится `m_bMatrixPalette` (цель 4, плюс гейт
`bFullLayerFound`/QNaN из цели 5), **идентичен ретейлу**. Расхождения в
порте здесь не найдено. Если гипотеза "LOD-меш без костных данных" верна,
то это, по построчной сверке форматов чтения, **не баг чтения/порта, а
следствие содержимого ассетов** (либо ассет реально не несёт кости на этом
LOD и в ретейле — тогда застывание должно происходить и там; стоит
перепроверить это заявление на реальных PC-данных, если есть с чем
сравнить визуально) — либо (более вероятно, раз симптом только у порта)
дефект нужно искать не в чтении/выводе флага, а в невыверенных целях 1-3
(сам гейт `bAnim`, выбор LOD, или ремап палитры), для которых понадобится
точный адрес `FUN_xxxxxxxx` из Ghidra.

---

## Диалоговая система: расхождения снапшота и ретейла (2026-08-21)

Разбор целиком — `Docs/Research_Scripts_Dialogue.md` §34. Здесь только сами
расхождения, чтобы их не искали заново.

| # | Место в снапшоте | Ретейл | Статус |
|---|---|---|---|
| 1 | `Char_BeginDialogue` (`WObj_CharDialogue.cpp:165-169`): после `PlayDialogue_Hash` ничего | `GameClasses_decomp:475718-475740`: `if (pCD->m_3PI_NoCamera == 0 && pSpeakerCD->m_3PI_NoCamera == 0) m_ClientFlags |= 0x48600000` (NOMOVE\|NOLOOK\|PLAYERSPEAK\|NOCROUCH). В снапшоте эта строка есть рядом, но **закомментирована** (`:89`) | **портировано** (флаг `RIDDICK_DLG_LOCK`, деф. ON) |
| 2 | `OnRefresh` (`WObj_Char.cpp:3000-3006`): снимает 3 бита, переустановки нет | `:445009-445040`: снимает `& 0xb79fffff` (все 4 бита), а пока реплика играет — ставит заново каждый тик | **портировано** (тот же флаг) |
| 3 | `EvalDialogueLink`, ветка `"player"` (`WObj_CharDialogue.cpp:1205-1225`): наполнить список и выйти | `FUN_102ebe50:477493-477596`: плюс обрыв разговора при пустом списке, плюс при единственном варианте и игроке в 3PI — `EQUIPITEMTYPE(0)`, `Char_ActivateDialogueItem(choice[0], игрок)` и тот же `0x48600000` | **портировано частично, по умолчанию ВЫКЛ** (`RIDDICK_DLG_AUTOSINGLE=1`); `EQUIPITEMTYPE` не портирован сознательно (риск не вернуть оружие) |
| 4 | `Player_SetObject` (`WObj_Game.cpp:191-205`) шлёт объекту ключ `PLAYERNR` | `FUN_1019ed50`: шлёт сообщение `0x10df`; хэшей `"PLAYERNR"`/`"$PLAYER"` в декомпилах нет вообще | расхождение зафиксировано, портировать не требуется (наш путь эквивалентен, но именно он раньше стирал имя игрока — §30) |
| 5 | имя игрока | ни один из 21 вызова `Object_SetName` в ретейле не именует игрока в SP; `"Riddick"` присваивается только в мультиплеерном game-mod'е (`FUN_102095b0`) | в SP ретейл берёт имя из данных (`TARGETNAME` шаблона `player_<мир>`); у нас его нет → имя ставим сами (`WObj_CharCreate.cpp`, `PLAYERNR`). Открыто: не теряем ли мы `TARGETNAME` шаблона |
| 6 | биты клиент-даты `+0x1ffc |= 0x80` и `|= 8`, которые ретейл ставит рядом с замком | — | **не портировано**: смещение не сопоставлено с полем, гадать не стали |
