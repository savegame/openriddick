# Карта соответствий: декомпил ретейла ↔ исходники

Ghidra-декомпилы PC-версии лежат в корне репозитория и доступны только
грепом (файлы огромные, открывать целиком не нужно):

| Файл | Модуль |
|---|---|
| `GameWorld_Win32_x86_dll_decomp.c` | GameWorld + XR/AnimGraph2 + WData |
| `GameClasses_Win32_x86_dll_decomp.c` | игровой код Riddick |
| `MXR_dll_decomp.c` | XR (скелет, анимация, VB) |
| `MSystem_dll_decomp.c` | MSystem (текстуры, звук, файлы) |
| `RndrGL_dll_decomp.c` | OpenGL-рендерер PC |

Символов в декомпиле нет, имена вида `FUN_10390570`. Ниже — только те,
которые **опознаны и сверены** с исходником: по строковым литералам в
`Error_static`/`ConOut`, по порядку и размеру полевых чтений, по структуре
switch'ей. Догадки сюда не пишем; если соответствие частичное — так и
помечено.

Формат: адрес, файл декомпила, что это в исходниках, чем доказано.

---

## GameWorld: AnimGraph2

| FUN | Что это | Доказательство |
|---|---|---|
| `FUN_10390570` | `CXRAG2_AnimLayer::Read(CCFile*, int _Ver)` (`AnimGraph2_AnimLayer.cpp:35`) | `switch(param_2)` с case 3/4/5/6 и `default:` → `Error_static("CXRAG2_StateAnim::Read", "Unsupported version %.4x")` (строка на `:634186`). Порядок и размеры чтений совпадают с нашим v5/v6 |
| `FUN_103904d0` | `CXRAG2_AnimLayer::Clear()` (`:21`) | Обнуляет ровно те же поля перед `Read`; значения см. таблицу раскладки ниже |
| `FUN_10390500` | `CXRAG2_AnimLayer::Write(CCFile*)` (`:150`) | Те же смещения, что у `Read`, но через writer-примитивы |

### Раскладка `CXRAG2_AnimLayer` v5/v6 в ретейле

Из `FUN_10390570`, case 5/6 (порядок вызовов = порядок в файле):

| Смещение | Примитив | Размер | Наше поле | Default в `FUN_103904d0` |
|---|---|---|---|---|
| `+0x00` | `FUN_1007f010` | 4 (fp32) | `m_TimeOffset` | `0` |
| `+0x04` | `FUN_1007f010` | 4 (fp32) | `m_TimeScale` | `0x3f800000` = 1.0f |
| `+0x08` | `FUN_10069050` | 4 (uint32) | `m_AnimFlags` | `0` |
| `+0x0c` | `FUN_101a2560` | 2 (int16) | `m_iAnim` | `0xffff` = −1 |
| `+0x0e` | `FUN_101a1d40` | 1 | *(у нас `Unknown0E`)* | `0` |
| `+0x0f` | `FUN_101a2150` | 1 (uint8) | `m_iBaseJoint` | `0` |
| `+0x10` | `FUN_101a2150` | 1 (uint8) | `m_iMergeOperator` | `0xff` |
| `+0x11` | `FUN_101a2150` | 1 (uint8) | `m_iTimeControlProperty` | `0xff` |

Итого 18 байт — ровно тот stride, что измерен на файлах
(`[AG2FMT] FULLANIMLAYERS stride == consumed == 18`).

**Наше чтение совпадает с ретейлом байт в байт.** Единственное расхождение —
дефолт `m_iMergeOperator`: у нас `AG2_ANIMLAYER_MERGEOPERATOR_BLEND` = `0x01`,
в ретейле `0xFF` (то есть `AG2_PROPERTY_NULL`). На поля, читаемые из файла,
это не влияет, но подтверждает, что `m_iMergeOperator` в этом движке
семантически **номер свойства**, а не enum операторов смешивания — он и
используется так в двух местах (`WAG2I_StateInst.cpp`: VALUECOMPARE и
adaptive timescale).

### `CXRAG2_State` — сверено, расхождений нет

| FUN | Что это | Доказательство |
|---|---|---|
| `FUN_10390940` | `CXRAG2_State::Read(CCFile*, int _Ver)` (`AnimGraph2_State.cpp:30`) | `default:` → `Error_static("CXRAG2_State::Read", "Unsupported version %.4x")` (`:634291`) |
| `FUN_10390a90` | `CXRAG2_SwitchState::Read` | тот же литерал на `:634337`, другая раскладка |
| `FUN_103908d0` / `FUN_10390920` | `Write`-пары к ним | те же смещения через writer-примитивы |

Раскладка из `FUN_10390940`:

| Смещение | Примитив | Размер | Наше поле |
|---|---|---|---|
| `+0x00` | `FUN_10069050` | 4 (uint32) | `m_lFlags[0]` |
| `+0x04` | `FUN_10069050` | 4 (uint32) | `m_lFlags[1]` |
| `+0x08` | `FUN_101a2560` | 2 (int16) | `m_iBaseNode` |
| `+0x0a` | `FUN_101a2560` | 2 (int16) | `m_iBaseAction` |
| `+0x0c` | `FUN_101a2560` | 2 (int16) | `m_iBasePropertyParam` |
| `+0x0e` | `FUN_101a2560` | 2 (int16) | `m_iBaseConstant` |
| `+0x10` | `FUN_101a2560` | 2 (int16) | `m_iBaseAnimLayer` |
| `+0x12` | `FUN_101a2150` | 1 (uint8) | `m_nConstants` |
| `+0x13` | `FUN_101a2150` | 1 (uint8) | `m_nAnimLayers` |
| `+0x14` | `FUN_101a2150` | 1 (uint8) | `m_Priority` |

21 байт. **Совпадает с нашим чтением полностью.** Проверялось ради версии
«флаги состояния читаются неверно, и поэтому мы уходим в
adaptive-timescale там, где ретейл идёт по обычной ветке» — версия
**закрыта**: флаги состояний читаются правильно.

Единственная мелочь: ретейл принимает для состояния версии **3, 5, 6**
(`if (param_2 != 3 && (param_2 < 5 || 6 < param_2)) Error`), то есть
версию 4 НЕ принимает, а наш `switch` принимает и её. На PC-контенте
безразлично.

---

## MXR: скелет

| FUN | Что это | Доказательство |
|---|---|---|
| `FUN_101435b0` | `CXR_SkeletonNode::Read(CDataFile*)` (`XRSkeleton.cpp:39`) | проверка `local_11c[0] != 0x100` → `"Unsupported node version. (%.4x)"` (`MXR_dll_decomp.c:214429`) |

Порядок чтения в ретейле:

| Смещение | Примитив | Наше поле |
|---|---|---|
| `+0x00/04/08` | `FUN_10063aa0` ×3 (fp32) | `m_LocalCenter` |
| `+0x18` | `FUN_10062e70` | `m_Flags` |
| `+0x1a` | `FUN_10062a60` (int16) | `m_iiNodeChildren` |
| *(во временную)* | `FUN_10062a60` (int16) → байт в `+0x22` | `m_nChildren` — **читается словом, хранится байтом** |
| `+0x1c` | `FUN_10062a60` (int16) | `m_iNodeParent` |
| `+0x10` | `FUN_10063aa0` (fp32) | `m_RotationScale` |
| `+0x14` | `FUN_10063aa0` (fp32) | `m_MovementScale` |
| `+0x1e` | `FUN_10062a60` (int16) | `m_iRotationSlot` |
| `+0x20` | `FUN_10062a60` (int16) | `m_iMovementSlot` |

**Порядок совпадает с нашим полностью.** Единственная деталь: ретейл
усекает `nChildren` до байта — у нас поле шире, что не теряет данных.

### `CXR_Skeleton::Read` — NODES / NODEINDICES тоже совпадают

`FUN_10146470`-район (`MXR_dll_decomp.c:216670-216740`), опознан по
строкам `"No NODES entry."` и `"No NODEINDICES entry."`. Ретейл делает
ровно то же, что и мы: `GetNext("NODEINDICES")` → `GetUserData()` →
`Core_SetLen(count)` → `CCFile::ReadLE(file, ptr, count)`.

Проверялось ради версии «44 узла 120-костного скелета недостижимы, потому
что мы недочитываем массив детей». **Версия закрыта.** Замер `[SKELTREE]`
даёт `nodes=120 idxArray=75 sumChildren=75 reached=76 outOfRange=0` — то
есть в файле действительно 75 связей на 120 узлов, данные самосогласованы,
и ретейл прочитал бы их так же. Недостижимые узлы объявляют `par=0,
nCh=0` и просто не числятся ничьими детьми.

QNaN на них — следствие заливки в `CXR_Skeleton::EvalAnim` под
`#ifndef M_RTM`, которой в релизной сборке ретейла нет вовсе. То есть это
особенность отладочной сборки, а не дефект порта.

### Примитивы чтения `CCFile` (GameWorld)

| FUN | Что читает |
|---|---|
| `FUN_1007f010` | `ReadLE(fp32)` |
| `FUN_10069050` | `ReadLE(uint32)` |
| `FUN_101a2560` | `ReadLE(int16)` |
| `FUN_101a2150` | `ReadLE(uint8)` |
| `FUN_101a1d40` | 1 байт, но **другой** примитив, чем `FUN_101a2150` (вероятно `int8`) |

### Прочее в GameWorld

| FUN / строка | Что это | Доказательство |
|---|---|---|
| `:634186` | `CXRAG2_StateAnim::Read` — обработчик неподдерживаемой версии | строковый литерал |
| `:634068` | `CXRAG2_Reaction::Read` — то же | строковый литерал |
| `:103125` | строка `"Dialogues/All.xcd"` | общий контейнер диалогов (PS3-упаковка) |
| `:95143` | строка `"Dialogues/%s.XCD"` | пофайловый фолбэк по имени диалога |
| `:95420` | строка `"DIALOGUES/"` | префикс поиска внутри контейнера |
| `:261850` | `GetValuef("SERVER_REFRESHRATE", 30.0)` | подтверждает частоту сима 30 Гц |

---

## Как опознавать дальше

Что реально работает на этом декомпиле:

1. **Строковые литералы.** `Error_static`/`ConOut`/`ConOutL` сохраняют имя
   функции строкой — самый надёжный якорь. Грепать по имени класса с `::`.
2. **Ключи реестра и имена файлов** — `GetValue*("…")`, `"…/….xcd"`.
3. **Порядок и размеры полевых чтений** — как в таблице выше: сопоставляем
   последовательность вызовов ридеров со структурой в исходнике. Работает
   для всех `Read`/`Write` и хорошо ловит расхождения раскладки.
4. **Уникальные константы** — редкие float'ы, размеры массивов, версии
   формата.

Что НЕ работает: поиск по арифметике вида `* 20.0` — Ghidra часто выносит
константы в `_DAT_…`, а компилятор переставляет умножения и деления.
Попытка найти так `MoveVel * 20.0f / ANIMMOVELENGTH`
(`WObj_CharClientData.cpp:1237`) в `GameClasses` результата не дала.

---

## Focus-frame HUD (2026-08-03)

**`GameClasses_Win32_x86_dll_decomp.c:96628-96665`** — рендерер подсказки
над объектом в фокусе. В наших исходниках соответствующий блок **выключен**
(`WObj_CharRender.cpp:1004`, `#if 0`, «JK-NOTE: Focus frame is broken, do
not use without rewrite»), то есть в ретейле он был переписан после того
среза, с которого снят наш снапшот.

Опознан по связке из трёх признаков: чтение двух строк из client-data,
проверка префикса `0xA7 'L'` (это `§L`, маркер ключа локализации в этом
движке) и вызовы `Localize_KeyExists`/`Localize_Str` — такой пары нет
больше нигде в модуле.

Соответствие полей (порядок и типы сверены с `WObj_CharClientData.h`):

| Смещение | Поле |
|---|---|
| `param_2 + 0x20` | `m_FocusFrameUseText` |
| `param_2 + 0x24` | `m_FocusFrameDescText` |

Логика ретейла: снять префикс `§L`, обрезать строку на следующем символе
`§`, проверить ключ через `Localize_KeyExists`, и **при отсутствии ключа не
рисовать ничего**. Это же объясняет, почему в ретейле подсказка появляется
только у тех объектов, чьи ключи есть в таблице локализации.

---

## Диалоговые айтемы персонажа (2026-08-04)

Все опознания в этом разделе доказаны **обратным перебором djb2**: ветки в
декомпиле выбираются по хэшу имени ключа, а `StrHash` (`FUN_10003140`,
побайтово совпадает с `CStrBase::StrHash`, `MRTC_StrBase.cpp:1614`)
обратима перебором коротких строк.

| FUN / константа | Что это | Чем доказано |
|---|---|---|
| `FUN_10003140` | `CStrBase::StrHash` | djb2 ×0x21, свёртка регистра, ±0x1505 — код совпадает построчно |
| `FUN_102b5350` | `CCharDialogueItems::Parse` | шесть веток по хэшам ключей, см. таблицу ниже |
| `FUN_102e7e60` | `Char_GetDialogueApproachItem` | читает слоты 0x324/0x32c, порог приоритета 0x7f |
| `FUN_102eb4f0` | `Char_ActivateDialogueItem` | шлёт 0x1077 (prio 0xc0), 0x10ae, 0x101b — как в исходнике |
| `FUN_102b3d30` | `CCharDialogueItems::Override` | копирует 6 пар (хэш, флаг) при валидности |
| `0x2a0d0` | `IS_VALID_ITEMHASH` | это `StrHash("0")`; макрос в `WDataRes_Sound.h:123` исключает 0 и `MHASH1('0')` |

Ключи и слоты (`m_DialogueItems` = смещение **0x324** в персонаже):

| Хэш ключа | Имя (перебором) | Слот | Действие ретейла |
|---|---|---|---|
| `409274C7` | `approachdialogueitem` | +0 | **число**: `hash = StrHash("%d")`, `bIsPlayer = (value < 0)` |
| `5B0CA3E6` | `dialogueitem_approach` | +0 | строка, `bIsPlayer = 1` |
| `CB507677` | `dialogueitem_approach_scared` | +8 | строка, `bIsPlayer = 1` |
| `7FD54113` | `dialogueitem_threaten` | +0x10 | строка, `bIsPlayer = 1` |
| `9673EA1C` | `dialogueitem_ignore` | +0x18 | строка, `bIsPlayer = 1` |
| `B8471D3F` | `dialogueitem_timeout` | +0x20 | строка, `bIsPlayer = 1` |
| `93C266B2` | `dialogueitem_exit` | +0x28 | строка, `bIsPlayer = 1` |

Смещения слотов независимо подтверждает `Char_DebugDialogueInfo`
(`GameClasses_Win32_x86_dll_decomp.c:473986`), где рядом с ними лежат
литералы `"Approach"`, `"Scared"`, `"Threaten"`, `"Ignore"`, `"Timeout"`.

**Расхождение с нашим снапшотом — ровно одно, легаси-ключ
`approachdialogueitem`** (`WObj_CharCreate.cpp:46`): у нас
`m_Approach.Set(_KeyValue, false)`, то есть значение хэшируется как строка
и флаг всегда `false`. У остальных шести ключей поведение совпадает.

**Ещё одно расхождение, найденное попутно:** ретейловый
`Char_GetDialogueApproachItem` имеет третью ветку — при флаге AI-состояния
`0x400` он отдаёт слот `+0x10` (Threaten) и выставляет вызывающему флаги
`0x1800`. В нашем исходнике этой ветки нет вовсе, есть только проверка
приоритета.

---

## Известные различия исходников и шипнутого PC-билда

Снапшот исходников — конфигурация **PS3**, а декомпилы — от **PC**-релиза.
Совпадает не всё, и это важно при сверке:

* **`EnterState_AdaptiveTimeScale`.** В нашем исходнике длина корневого
  движения клипа пишется в свойство с номером `pLayer->GetMergeOperator()`.
  В PC-контенте это поле у всех locomotion-слоёв равно нулю (замер
  `[ADAPT] enter … prop=0(raw 0)`), а игровой код делит на свойство **8**
  (`PROPERTY_FLOAT_ANIMMOVELENGTH`), и больше никто в это свойство не пишет.
  Раскладку слоя мы читаем верно (см. выше), значит PC-билд писал длину в 8
  каким-то другим путём. Обходится через `RIDDICK_ADAPTPROP`
  (`Docs/HacksAndHooks.md`).
* Формат `.XW`, `.XTC2`, `CImage v0x0400`, AG2 v5/v6 — PC-специфичные, в
  PS3-исходнике их нет вовсе (см. `Docs/BSP_PC_Format.md` и журнал в
  `CLAUDE.md`).
* **Границы DLL не совпадают с границами директорий нашего снапшота** —
  например код с регистро-строками `GUI\VIDSEL\PIXELASPECT`/
  `VIDEO_DISPLAY_WIDTH` (у нас в `Projects/Main/Exe/XRApp.cpp` и
  `Projects/Main/GameWorld/WFrontEndMod_Menus.cpp`) в декомпиле похоже
  лежит в `GameClasses.dll`, а не в exe. Догадка, не сверено построчно.
  Подробности — `Docs/Decomp_Coverage.md`, §3.4/§5.

---

## Автоматическое сопоставление по якорям "Класс::Метод" (исследование покрытия, этап 2)

**Устарело/частично дублирует:** секция «Автосопоставление, этапы A/C» в
конце файла — полный список (в т.ч. эти 11 пар как подмножество) плюс
неоднозначные пары и ещё два типа якорей (classname/command). Эта секция
оставлена как есть для истории первого прохода.

Методика, полный отчёт и метрики — `Docs/Decomp_Coverage.md`. Получено
скриптами `Tools/decomp_inventory.py` + `Tools/decomp_match.py`: находится
строковый литерал `"Класс::Метод"` (обычно первый аргумент
`Error_static`-подобного вызова) одновременно в исходнике (как реальный
метод `Класс::Метод`) и в декомпиле (внутри конкретной `FUN_xxxxxxxx`).
Ниже — **только однозначные** пары (литерал встретился ровно в одном
месте исходника); неоднозначные (например тексты, продублированные в
`MRTC_System_Win32.cpp`/`MRTC_System_PS3.cpp`/`MRTC_System_Linux.cpp` под
разные платформы) в карту не включены — сам факт совпадения имени метода
там уже подтверждён, но привязка к конкретному `.cpp` неоднозначна.

Доказательство для всех строк ниже одно и то же: строка `Error_static`
(или аналог) со строго тем же текстом `"Класс::Метод"` встречается только
в указанном файле исходника, и найдена ровно в теле указанной
`FUN_xxxxxxxx` в декомпиле (проверено скриптом, не построчно вручную —
чуть слабее, чем ручная сверка выше, но строже, чем догадка).

| FUN | Декомпил | Класс::Метод | Файл:строка исходника |
|---|---|---|---|
| `FUN_102775e0` | GameClasses | `CAutoVarContainer::AutoVar_Read` | `Shared/MOS/Classes/GameWorld/WObjects/WObj_AutoVar.cpp:183` |
| `FUN_102774e0` | GameClasses | `CAutoVarContainer::AutoVar_Write` | `Shared/MOS/Classes/GameWorld/WObjects/WObj_AutoVar.cpp:161` |
| `FUN_1010cd70` | MSystem | `CSCC_Codec_RAW::AddData` | `Shared/MOS/MSystem/Sound/MSound_Codec.cpp:84` |
| `FUN_1010ce00` | MSystem | `CSCC_Codec_RAW::CreateDecoder` | `Shared/MOS/MSystem/Sound/MSound_Codec.cpp:90` |
| `FUN_1010cce0` | MSystem | `CSCC_Codec_RAW::CreateEncoder` | `Shared/MOS/MSystem/Sound/MSound_Codec.cpp:78` |
| `FUN_102843a0` | GameClasses | `CWObject_Character::OnClientPredict` | `Projects/Main/GameClasses/WObj_Char/WObj_CharDarkling.cpp:668` |
| `FUN_102ab770` | GameClasses | `CWObject_Character::OnClientRefresh` | `Projects/Main/GameClasses/WObj_Char.cpp:3557` |
| `FUN_10172950` | GameClasses | `CWObject_SoundVolume::OnCreateClientUpdate` | `Shared/MOS/Classes/GameWorld/WObjects/WObj_SoundVolume.cpp:126` |
| `FUN_10079600` | MXR | `CXR_EngineImpl::VBM_Begin` | `Shared/MOS/XR/XREngine.cpp:2531` |
| `FUN_10048d50` | MXR | `CXR_VBManager::Clip_Add` | `Shared/MOS/XR/XRVBManager.cpp:3980` |
| `FUN_10044c00` | MXR | `CXR_VBManager::Viewport_Add` | `Shared/MOS/XR/XRVBManager.cpp:3959` |

**Догадка (не сверено построчно):** `FUN_104bb830` в
`GameClasses_Win32_x86_dll_decomp.c` — вероятно логика видеонастроек
(`GUI\VIDSEL\PIXELASPECT`/`VIDEO_DISPLAY_WIDTH`, см. `Decomp_Coverage.md`
§3.4) — кандидат для будущих расследований `vid_pixelaspect`/`VIDEO_*`.

---

## Автосопоставление, этапы A/C (продолжение исследования покрытия)

Полная методика и метрики — `Docs/Decomp_Coverage.md`. Ниже —
**все** пары, которые скрипты нашли автоматически по трём типам
якорей (classmethod/classname/command), без ручной построчной
сверки тела функции — то есть это подтверждение **факта присутствия**
(имя метода/класса/команды опознано в конкретной `FUN_xxxxxxxx`),
не подтверждение идентичности логики.


### Автосопоставление по якорю "Класс::Метод" — однозначные (этап A/C)

Литерал встретился ровно в одном месте исходника — привязка к конкретному `.cpp:line` надёжна. Сгенерировано `Tools/decomp_match.py` + `Tools/decomp_map_append.py`, семантика тела функции НЕ сверялась построчно (см. `Docs/Decomp_Coverage.md`).

| FUN | Декомпил | Класс::Метод | Файл:строка исходника |
|---|---|---|---|
| `FUN_102775e0` | GameClasses | `CAutoVarContainer::AutoVar_Read` | `Shared/MOS/Classes/GameWorld/WObjects/WObj_AutoVar.cpp:183` |
| `FUN_102774e0` | GameClasses | `CAutoVarContainer::AutoVar_Write` | `Shared/MOS/Classes/GameWorld/WObjects/WObj_AutoVar.cpp:161` |
| `FUN_10054560` | GameWorld | `CConsole::ExecuteString` | `Shared/MOS/MSystem/Misc/MConsole.cpp:848` |
| `FUN_1047e7a0` | GameClasses | `CMFileContainer::GetFile` | `Shared/MOS/Classes/Miscellaneous/MFileContainer.cpp:171` |
| `FUN_104a4010` | GameClasses | `CMWnd_CubeMenu_Controller2::OnPaint` | `Projects/Main/GameWorld/WFrontEndMod_Menus.cpp:2787` |
| `FUN_100bccf0` | GameWorld | `CMWnd_CubeMenu_Controller2::OnPaint` | `Projects/Main/GameWorld/WFrontEndMod_Menus.cpp:2787` |
| `FUN_104a5460` | GameClasses | `CMWnd_ModGameMenu::OnMessage` | `Projects/Main/GameWorld/WFrontEndMod_Menus.cpp:2921` |
| `FUN_100be140` | GameWorld | `CMWnd_ModGameMenu::OnMessage` | `Projects/Main/GameWorld/WFrontEndMod_Menus.cpp:2921` |
| `FUN_104a0350` | GameClasses | `CMWnd_Timed::OnRefresh` | `Projects/Main/GameWorld/WFrontEndMod_Menus.cpp:139` |
| `FUN_100b8fb0` | GameWorld | `CMWnd_Timed::OnRefresh` | `Projects/Main/GameWorld/WFrontEndMod_Menus.cpp:139` |
| `FUN_103d95d0` | GameClasses | `CNetMsg::Read` | `Shared/MOS/Classes/GameWorld/WClass.cpp:352` |
| `FUN_103f7780` | GameClasses | `CRC_Util2D::GetAttrib` | `Shared/MOS/Classes/Win/MWinGrph.cpp:2047` |
| `FUN_1010cd70` | MSystem | `CSCC_Codec_RAW::AddData` | `Shared/MOS/MSystem/Sound/MSound_Codec.cpp:84` |
| `FUN_1010ce00` | MSystem | `CSCC_Codec_RAW::CreateDecoder` | `Shared/MOS/MSystem/Sound/MSound_Codec.cpp:90` |
| `FUN_1010cce0` | MSystem | `CSCC_Codec_RAW::CreateEncoder` | `Shared/MOS/MSystem/Sound/MSound_Codec.cpp:78` |
| `FUN_100658a0` | MXR | `CThinKeyContainer::Read` | `Shared/MOS/XR/XRSurf.cpp:4114` |
| `FUN_1005bcf0` | MXR | `CThinKeyContainer::SetKeyValue` | `Shared/MOS/XR/XRSurf.cpp:3983` |
| `FUN_10456e90` | GameClasses | `CWFrontEnd_Mod::Con_CG_Backout` | `Projects/Main/GameWorld/WFrontEndMod.cpp:1960` |
| `FUN_10456e10` | GameClasses | `CWFrontEnd_Mod::Con_DoCacheCommand` | `Projects/Main/GameWorld/WFrontEndMod.cpp:1954` |
| `FUN_10306d10` | GameClasses | `CWO_Character_ClientData::CreateClientObject` | `Projects/Main/GameClasses/WObj_CharClientData.cpp:833` |
| `FUN_103bd490` | GameClasses | `CWO_PhysicsState::AddPhysicsPrim` | `Shared/MOS/Classes/GameWorld/WObjCore.cpp:331` |
| `FUN_102843a0` | GameClasses | `CWObject_Character::OnClientPredict` | `Projects/Main/GameClasses/WObj_Char/WObj_CharDarkling.cpp:668` |
| `FUN_102ab770` | GameClasses | `CWObject_Character::OnClientRefresh` | `Projects/Main/GameClasses/WObj_Char.cpp:3557` |
| `FUN_102373b0` | GameClasses | `CWObject_GameCore::ResolveAnimHandle` | `Projects/Main/GameClasses/WObj_Game/WObj_GameCore.cpp:2485` |
| `FUN_10172b00` | GameClasses | `CWObject_SoundVolume::CWObject_SoundVolume` | `Shared/MOS/Classes/GameWorld/WObjects/WObj_SoundVolume.cpp:85` |
| `FUN_101734d0` | GameClasses | `CWObject_SoundVolume::CWObject_SoundVolume` | `Shared/MOS/Classes/GameWorld/WObjects/WObj_SoundVolume.cpp:85` |
| `FUN_10172950` | GameClasses | `CWObject_SoundVolume::OnCreateClientUpdate` | `Shared/MOS/Classes/GameWorld/WObjects/WObj_SoundVolume.cpp:126` |
| `FUN_1045b860` | GameClasses | `CWRes_Dialogue::AddString` | `Shared/MOS/Classes/GameWorld/WDataRes_Sound.cpp:1095` |
| `FUN_1045b960` | GameClasses | `CWRes_Dialogue::AddString` | `Shared/MOS/Classes/GameWorld/WDataRes_Sound.cpp:1095` |
| `FUN_10077060` | GameWorld | `CWRes_Dialogue::AddString` | `Shared/MOS/Classes/GameWorld/WDataRes_Sound.cpp:1095` |
| `FUN_10077160` | GameWorld | `CWRes_Dialogue::AddString` | `Shared/MOS/Classes/GameWorld/WDataRes_Sound.cpp:1095` |
| `FUN_1004ae30` | GameWorld | `CWorld_ServerCore::Render` | `Shared/MOS/Classes/GameWorld/Server/WServer_Core.cpp:1072` |
| `FUN_1046f310` | GameClasses | `CXR_Anim_DataKey_Edit::Read` | `Shared/MOS/XR/XRAnimDataKey.cpp:86` |
| `FUN_1046f4c0` | GameClasses | `CXR_Anim_DataKeys::Read` | `Shared/MOS/XR/XRAnimDataKey.cpp:154` |
| `FUN_1046fcd0` | GameClasses | `CXR_Anim_DataKeys_Edit::Read` | `Shared/MOS/XR/XRAnimDataKey.cpp:345` |
| `FUN_10435220` | GameClasses | `CXR_Anim_MoveKey::Read` | `Shared/MOS/XR/XRAnim.cpp:353` |
| `FUN_10154980` | MXR | `CXR_Anim_MoveKey::Read` | `Shared/MOS/XR/XRAnim.cpp:353` |
| `FUN_10434e60` | GameClasses | `CXR_Anim_RotKey::Read` | `Shared/MOS/XR/XRAnim.cpp:118` |
| `FUN_10153aa0` | MXR | `CXR_Anim_RotKey::Read` | `Shared/MOS/XR/XRAnim.cpp:118` |
| `FUN_10432c60` | GameClasses | `CXR_Anim_RotKey::Write` | `Shared/MOS/XR/XRAnim.cpp:236` |
| `FUN_10151490` | MXR | `CXR_Anim_RotKey::Write` | `Shared/MOS/XR/XRAnim.cpp:236` |
| `FUN_10436a00` | GameClasses | `CXR_Anim_SequenceTracks::ReadData` | `Shared/MOS/XR/XRAnim.cpp:3485` |
| `FUN_10155890` | MXR | `CXR_Anim_SequenceTracks::ReadData` | `Shared/MOS/XR/XRAnim.cpp:3485` |
| `FUN_103df1f0` | GameClasses | `CXR_Cloth::Write` | `Shared/MOS/XR/XRSkeleton.cpp:462` |
| `FUN_10140d50` | MXR | `CXR_Cloth::Write` | `Shared/MOS/XR/XRSkeleton.cpp:462` |
| `FUN_103e1340` | GameClasses | `CXR_ClothBoneWeights::Read` | `Shared/MOS/XR/XRSkeleton.cpp:170` |
| `FUN_10142d90` | MXR | `CXR_ClothBoneWeights::Read` | `Shared/MOS/XR/XRSkeleton.cpp:170` |
| `FUN_103e17b0` | GameClasses | `CXR_ClothConstraint::Read` | `Shared/MOS/XR/XRSkeleton.cpp:583` |
| `FUN_10143200` | MXR | `CXR_ClothConstraint::Read` | `Shared/MOS/XR/XRSkeleton.cpp:583` |
| `FUN_10079600` | MXR | `CXR_EngineImpl::VBM_Begin` | `Shared/MOS/XR/XREngine.cpp:2531` |
| `FUN_103e1aa0` | GameClasses | `CXR_SkeletonAttachPoint::Read` | `Shared/MOS/XR/XRSkeleton.cpp:104` |
| `FUN_10143770` | MXR | `CXR_SkeletonAttachPoint::Read` | `Shared/MOS/XR/XRSkeleton.cpp:104` |
| `FUN_10048d50` | MXR | `CXR_VBManager::Clip_Add` | `Shared/MOS/XR/XRVBManager.cpp:3980` |
| `FUN_10048e70` | MXR | `CXR_VBManager::Clip_Push` | `Shared/MOS/XR/XRVBManager.cpp:3996` |
| `FUN_10044c00` | MXR | `CXR_VBManager::Viewport_Add` | `Shared/MOS/XR/XRVBManager.cpp:3959` |
| `FUN_10044a90` | MXR | `CXR_VBManager::Viewport_Push` | `Shared/MOS/XR/XRVBManager.cpp:3923` |
| `FUN_10066f00` | MXR | `CXW_SurfaceLayer::Read` | `Shared/MOS/XR/XRSurf.cpp:433` |
| `FUN_100244d0` | GameClasses | `MRTC_CClassRegistry::LoadClassLibrary` | `Shared/MCC/Mrtc.cpp:1105` |
| `FUN_100244d0` | GameWorld | `MRTC_CClassRegistry::LoadClassLibrary` | `Shared/MCC/Mrtc.cpp:1105` |
| `FUN_10030780` | MCCDyn | `MRTC_CClassRegistry::LoadClassLibrary` | `Shared/MCC/Mrtc.cpp:1105` |
| `FUN_10036ac0` | MSystem | `MRTC_CClassRegistry::LoadClassLibrary` | `Shared/MCC/Mrtc.cpp:1105` |
| `FUN_100244d0` | MXR | `MRTC_CClassRegistry::LoadClassLibrary` | `Shared/MCC/Mrtc.cpp:1105` |
| `FUN_10024510` | RndrGL | `MRTC_CClassRegistry::LoadClassLibrary` | `Shared/MCC/Mrtc.cpp:1105` |
| `FUN_10012c80` | GameClasses | `MRTC_CClassRegistry::RemoveClassContainer` | `Shared/MCC/Mrtc.cpp:1043` |
| `FUN_10012c80` | GameWorld | `MRTC_CClassRegistry::RemoveClassContainer` | `Shared/MCC/Mrtc.cpp:1043` |
| `FUN_100162f0` | MCCDyn | `MRTC_CClassRegistry::RemoveClassContainer` | `Shared/MCC/Mrtc.cpp:1043` |
| `FUN_1001fe20` | MSystem | `MRTC_CClassRegistry::RemoveClassContainer` | `Shared/MCC/Mrtc.cpp:1043` |
| `FUN_10012c80` | MXR | `MRTC_CClassRegistry::RemoveClassContainer` | `Shared/MCC/Mrtc.cpp:1043` |
| `FUN_10012cc0` | RndrGL | `MRTC_CClassRegistry::RemoveClassContainer` | `Shared/MCC/Mrtc.cpp:1043` |
| `FUN_10024870` | GameClasses | `MRTC_CClassRegistry::UnloadClassLibrary` | `Shared/MCC/Mrtc.cpp:1184` |
| `FUN_10024870` | GameWorld | `MRTC_CClassRegistry::UnloadClassLibrary` | `Shared/MCC/Mrtc.cpp:1184` |
| `FUN_10030b20` | MCCDyn | `MRTC_CClassRegistry::UnloadClassLibrary` | `Shared/MCC/Mrtc.cpp:1184` |
| `FUN_10036e90` | MSystem | `MRTC_CClassRegistry::UnloadClassLibrary` | `Shared/MCC/Mrtc.cpp:1184` |
| `FUN_10024870` | MXR | `MRTC_CClassRegistry::UnloadClassLibrary` | `Shared/MCC/Mrtc.cpp:1184` |
| `FUN_100248b0` | RndrGL | `MRTC_CClassRegistry::UnloadClassLibrary` | `Shared/MCC/Mrtc.cpp:1184` |
| `FUN_10024fb0` | GameClasses | `MRTC_ObjectManager::UnregisterObject` | `Shared/MCC/Mrtc.cpp:2376` |
| `FUN_10024fb0` | GameWorld | `MRTC_ObjectManager::UnregisterObject` | `Shared/MCC/Mrtc.cpp:2376` |
| `FUN_10031260` | MCCDyn | `MRTC_ObjectManager::UnregisterObject` | `Shared/MCC/Mrtc.cpp:2376` |
| `FUN_1002df10` | MSystem | `MRTC_ObjectManager::UnregisterObject` | `Shared/MCC/Mrtc.cpp:2376` |
| `FUN_10024fb0` | MXR | `MRTC_ObjectManager::UnregisterObject` | `Shared/MCC/Mrtc.cpp:2376` |
| `FUN_10024ff0` | RndrGL | `MRTC_ObjectManager::UnregisterObject` | `Shared/MCC/Mrtc.cpp:2376` |
| `FUN_10145f70` | MSystem | `TSVQ::TwiddleImage` | `Shared/MOS/MSystem/Raster/MImageCompressVQ.cpp:813` |
| `FUN_101463b0` | MSystem | `TSVQ::TwiddleImage` | `Shared/MOS/MSystem/Raster/MImageCompressVQ.cpp:813` |
| `FUN_10146b30` | MSystem | `TSVQ::TwiddleImage` | `Shared/MOS/MSystem/Raster/MImageCompressVQ.cpp:813` |

### Автосопоставление по якорю "Класс::Метод" — неоднозначные (этап A/C)

Литерал совпадает с несколькими местами исходника (обычно один и тот же `Error_static("Класс::Метод", ...)`, продублированный под разные платформы — `MRTC_System_Win32.cpp`/`_PS3.cpp`/`_Linux.cpp`). Имя метода подтверждено, конкретный `.cpp` — нет.

| FUN | Декомпил | Класс::Метод | Кандидаты в исходнике |
|---|---|---|---|
| `FUN_104a0910` | GameClasses | `CMWnd_CubeMenu::OnPaint` | Projects/Main/GameWorld/WFrontEndMod_Menus.cpp:1265; Projects/Main/GameWorld/WFrontEndMod_Menus.cpp:1616; Projects/Main/GameWorld/WFrontEndMod_Menus.cpp:1748; Projects/Main/GameWorld/WFrontEndMod_Menus.cpp:1882; Projects/Main/GameWorld/WFrontEndMod_Menus.cpp:2091; Projects/Main/GameWorld/WFrontEndMod_Menus.cpp:2580; Projects/Main/GameWorld/WFrontEndMod_Menus.cpp:2793; Projects/Main/GameWorld/WFrontEndMod_Menus.cpp:3324; Projects/Main/GameWorld/WFrontEndMod_Menus.cpp:3791; Projects/Main/GameWorld/WFrontEndMod_Menus.cpp:3983; Projects/Main/GameWorld/WFrontEndMod_Menus.cpp:4167; Projects/Main/GameWorld/WFrontEndMod_Menus.cpp:4435; Projects/Main/GameWorld/WFrontEndMod_Menus.cpp:4686; Projects/Main/GameWorld/WFrontEndMod_Menus.cpp:487; Projects/Main/GameWorld/WFrontEndMod_Menus.cpp:5714; Projects/Main/GameWorld/WFrontEndMod_Menus.cpp:5840; Projects/Main/GameWorld/WFrontEndMod_Menus.cpp:6050; Projects/Main/GameWorld/WFrontEndMod_Menus.cpp:6450; Projects/Main/GameWorld/WFrontEndMod_Options.cpp:194; Projects/Main/GameWorld/WFrontEndMod_Options.cpp:385; Projects/Main/GameWorld/WFrontEndMod_Options.cpp:399; Projects/Main/GameWorld/WFrontEndMod_Options.cpp:484; Projects/Main/GameWorld/WFrontEndMod_Options.cpp:669; Projects/Main/GameWorld/WFrontEndMod_Options.cpp:72; Projects/Main/GameWorld/WFrontEndMod_Options.cpp:981; Projects/Main/GameWorld/WFrontEnd_Menues_P6.cpp:1085; Projects/Main/GameWorld/WFrontEnd_Menues_P6.cpp:334; Projects/Main/GameWorld/WFrontEnd_Menues_P6.cpp:67; Projects/Main/GameWorld/WFrontEnd_Menues_P6.cpp:690; Projects/Main/GameWorld/WFrontEnd_Menues_P6.cpp:94 |
| `FUN_100b9570` | GameWorld | `CMWnd_CubeMenu::OnPaint` | Projects/Main/GameWorld/WFrontEndMod_Menus.cpp:1265; Projects/Main/GameWorld/WFrontEndMod_Menus.cpp:1616; Projects/Main/GameWorld/WFrontEndMod_Menus.cpp:1748; Projects/Main/GameWorld/WFrontEndMod_Menus.cpp:1882; Projects/Main/GameWorld/WFrontEndMod_Menus.cpp:2091; Projects/Main/GameWorld/WFrontEndMod_Menus.cpp:2580; Projects/Main/GameWorld/WFrontEndMod_Menus.cpp:2793; Projects/Main/GameWorld/WFrontEndMod_Menus.cpp:3324; Projects/Main/GameWorld/WFrontEndMod_Menus.cpp:3791; Projects/Main/GameWorld/WFrontEndMod_Menus.cpp:3983; Projects/Main/GameWorld/WFrontEndMod_Menus.cpp:4167; Projects/Main/GameWorld/WFrontEndMod_Menus.cpp:4435; Projects/Main/GameWorld/WFrontEndMod_Menus.cpp:4686; Projects/Main/GameWorld/WFrontEndMod_Menus.cpp:487; Projects/Main/GameWorld/WFrontEndMod_Menus.cpp:5714; Projects/Main/GameWorld/WFrontEndMod_Menus.cpp:5840; Projects/Main/GameWorld/WFrontEndMod_Menus.cpp:6050; Projects/Main/GameWorld/WFrontEndMod_Menus.cpp:6450; Projects/Main/GameWorld/WFrontEndMod_Options.cpp:194; Projects/Main/GameWorld/WFrontEndMod_Options.cpp:385; Projects/Main/GameWorld/WFrontEndMod_Options.cpp:399; Projects/Main/GameWorld/WFrontEndMod_Options.cpp:484; Projects/Main/GameWorld/WFrontEndMod_Options.cpp:669; Projects/Main/GameWorld/WFrontEndMod_Options.cpp:72; Projects/Main/GameWorld/WFrontEndMod_Options.cpp:981; Projects/Main/GameWorld/WFrontEnd_Menues_P6.cpp:1085; Projects/Main/GameWorld/WFrontEnd_Menues_P6.cpp:334; Projects/Main/GameWorld/WFrontEnd_Menues_P6.cpp:67; Projects/Main/GameWorld/WFrontEnd_Menues_P6.cpp:690; Projects/Main/GameWorld/WFrontEnd_Menues_P6.cpp:94 |
| `FUN_104a0720` | GameClasses | `CMWnd_CubeMenu::OnRefresh` | Projects/Main/GameWorld/WFrontEndMod_Menus.cpp:3728; Projects/Main/GameWorld/WFrontEndMod_Menus.cpp:452 |
| `FUN_100b9380` | GameWorld | `CMWnd_CubeMenu::OnRefresh` | Projects/Main/GameWorld/WFrontEndMod_Menus.cpp:3728; Projects/Main/GameWorld/WFrontEndMod_Menus.cpp:452 |
| `FUN_103d8f80` | GameClasses | `CNetMsg::AddStr` | Shared/MOS/Classes/GameWorld/WClass.cpp:127; Shared/MOS/Classes/GameWorld/WClass.cpp:99 |
| `FUN_103d9100` | GameClasses | `CNetMsg::AddStr` | Shared/MOS/Classes/GameWorld/WClass.cpp:127; Shared/MOS/Classes/GameWorld/WClass.cpp:99 |
| `FUN_103e5a40` | GameClasses | `CNetTransferBufferBase::AddPacket` | Shared/MOS/Classes/GameWorld/Client/WClientClass.cpp:163; Shared/MOS/Classes/GameWorld/Client/WClientClass.cpp:314 |
| `FUN_1026e140` | GameClasses | `CRPG_Object::CreateObject` | Projects/Main/GameClasses/WObj_Char/WObj_CharAngelus.cpp:142; Projects/Main/GameClasses/WObj_CharCreate.cpp:1843; Projects/Main/GameClasses/WObj_CharCreate.cpp:2863; Projects/Main/GameClasses/WObj_CharCreate.cpp:2910; Projects/Main/GameClasses/WObj_CharMechanics.cpp:456; Projects/Main/GameClasses/WObj_CharMsg.cpp:3480; Projects/Main/GameClasses/WObj_CharMsg.cpp:7503; Projects/Main/GameClasses/WObj_CharSpectator.cpp:22; Projects/Main/GameClasses/WObj_Game/WObj_GameMod.cpp:6648; Projects/Main/GameClasses/WObj_Item.cpp:140; Projects/Main/GameClasses/WObj_Misc/WObj_ActionCutscene.cpp:3369; Projects/Main/GameClasses/WObj_Misc/WObj_ActionCutscenePickup.cpp:113; Projects/Main/GameClasses/WObj_Misc/WObj_AutoFire.cpp:85; Projects/Main/GameClasses/WObj_Misc/WObj_Model_Moth.cpp:1134; Projects/Main/GameClasses/WObj_Misc/WObj_Turret.cpp:83; Projects/Main/GameClasses/WObj_RPG.cpp:70; Projects/Main/GameClasses/WObj_RPG.cpp:705; Projects/Main/GameClasses/WObj_RPG.cpp:92; Projects/Main/GameClasses/WObj_Sys/WObj_Trigger.cpp:1559; Projects/Main/GameClasses/WObj_Sys/WObj_Trigger.cpp:2519; Projects/Main/GameClasses/WRPG/WRPGCore.cpp:202; Projects/Main/GameClasses/WRPG/WRPGCore.cpp:294; Projects/Main/GameClasses/WRPG/WRPGCore.cpp:312; Projects/Main/GameClasses/WRPG/WRPGCore.cpp:338; Projects/Main/GameClasses/WRPG/WRPGMiniGun.cpp:140 |
| `FUN_1026e9d0` | GameClasses | `CRPG_Object::CreateObject` | Projects/Main/GameClasses/WObj_Char/WObj_CharAngelus.cpp:142; Projects/Main/GameClasses/WObj_CharCreate.cpp:1843; Projects/Main/GameClasses/WObj_CharCreate.cpp:2863; Projects/Main/GameClasses/WObj_CharCreate.cpp:2910; Projects/Main/GameClasses/WObj_CharMechanics.cpp:456; Projects/Main/GameClasses/WObj_CharMsg.cpp:3480; Projects/Main/GameClasses/WObj_CharMsg.cpp:7503; Projects/Main/GameClasses/WObj_CharSpectator.cpp:22; Projects/Main/GameClasses/WObj_Game/WObj_GameMod.cpp:6648; Projects/Main/GameClasses/WObj_Item.cpp:140; Projects/Main/GameClasses/WObj_Misc/WObj_ActionCutscene.cpp:3369; Projects/Main/GameClasses/WObj_Misc/WObj_ActionCutscenePickup.cpp:113; Projects/Main/GameClasses/WObj_Misc/WObj_AutoFire.cpp:85; Projects/Main/GameClasses/WObj_Misc/WObj_Model_Moth.cpp:1134; Projects/Main/GameClasses/WObj_Misc/WObj_Turret.cpp:83; Projects/Main/GameClasses/WObj_RPG.cpp:70; Projects/Main/GameClasses/WObj_RPG.cpp:705; Projects/Main/GameClasses/WObj_RPG.cpp:92; Projects/Main/GameClasses/WObj_Sys/WObj_Trigger.cpp:1559; Projects/Main/GameClasses/WObj_Sys/WObj_Trigger.cpp:2519; Projects/Main/GameClasses/WRPG/WRPGCore.cpp:202; Projects/Main/GameClasses/WRPG/WRPGCore.cpp:294; Projects/Main/GameClasses/WRPG/WRPGCore.cpp:312; Projects/Main/GameClasses/WRPG/WRPGCore.cpp:338; Projects/Main/GameClasses/WRPG/WRPGMiniGun.cpp:140 |
| `FUN_10078500` | MSystem | `CRegistry_Dynamic::XRG_Parse` | Shared/MOS/MSystem/Misc/MRegistry_Dynamic.cpp:2252; Shared/MOS/MSystem/Misc/MRegistry_Dynamic.cpp:2257; Shared/MOS/MSystem/Misc/MRegistry_Dynamic.cpp:6180; Shared/MOS/MSystem/Misc/MRegistry_Dynamic.cpp:6185 |
| `FUN_100790a0` | MSystem | `CRegistry_Dynamic::XRG_Parse` | Shared/MOS/MSystem/Misc/MRegistry_Dynamic.cpp:2252; Shared/MOS/MSystem/Misc/MRegistry_Dynamic.cpp:2257; Shared/MOS/MSystem/Misc/MRegistry_Dynamic.cpp:6180; Shared/MOS/MSystem/Misc/MRegistry_Dynamic.cpp:6185 |
| `FUN_100257d0` | GameClasses | `CStrBase::CompareNoCase` | Projects/Main/GameClasses/WObj_AI/AICore.cpp:11517; Projects/Main/GameClasses/WObj_AI/AICore.cpp:11518; Projects/Main/GameClasses/WObj_AI/AI_KnowledgeBase.cpp:2378; Projects/Main/GameClasses/WObj_AI/AI_KnowledgeBase.cpp:2379; Projects/Main/GameClasses/WObj_AI/AI_KnowledgeBase.cpp:2441; Projects/Main/GameClasses/WObj_AI/AI_KnowledgeBase.cpp:2442; Projects/Main/GameClasses/WObj_AI/AI_KnowledgeBase.cpp:4323; Projects/Main/GameClasses/WObj_AI/AI_KnowledgeBase.cpp:4324; Projects/Main/GameClasses/WObj_CharMsg.cpp:2858; Projects/Main/GameClasses/WObj_CharMsg.cpp:3271; Projects/Main/GameClasses/WObj_CharMsg.cpp:7497; Projects/Main/GameClasses/WObj_CharMsg.cpp:756; Projects/Main/GameClasses/WObj_CharMsg.cpp:768; Projects/Main/GameClasses/WObj_Game/WObj_GameCore.cpp:1551; Projects/Main/GameClasses/WObj_Game/WObj_GameMod.cpp:828; Shared/MCC/MDA_Hash.cpp:303; Shared/MCC/MRTC_StrBase.cpp:857; Shared/MCC/MRTC_StrBase.cpp:878; Shared/MCC/MRTC_StrBase.cpp:890; Shared/MCC/MRTC_StrBase.cpp:902; Shared/MCC/MRTC_StrBase.cpp:923; Shared/MCC/MRTC_StrBase.cpp:932; Shared/MCC/MRTC_StrBase.cpp:941; Shared/MCC/MRTC_StrBase.cpp:979; Shared/MCC/MRTC_StrBase.cpp:985; Shared/MCC/MRTC_StrBase.cpp:991; Shared/MOS/Classes/GameWorld/WObjects/WObj_SimpleMessage.cpp:431; Shared/MOS/Classes/GameWorld/WObjects/WObj_SimpleMessage.cpp:434; Shared/MOS/Classes/GameWorld/WObjects/WObj_SimpleMessage.cpp:437; Shared/MOS/Classes/GameWorld/WObjects/WObj_SimpleMessage.cpp:440; Shared/MOS/Classes/GameWorld/WObjects/WObj_SimpleMessage.cpp:443; Shared/MOS/Classes/GameWorld/WObjects/WObj_SimpleMessage.cpp:446; Shared/MOS/MSystem/Misc/MRegistry_Static.cpp:411; Shared/MOS/MSystem/Misc/MRegistry_Static.cpp:425; Shared/MOS/MSystem/Misc/MRegistry_Static.cpp:442; Shared/MOS/MSystem/Misc/MRegistry_Static.cpp:456; Shared/MOS/MSystem/Misc/MRegistry_Static.cpp:470; Shared/MOS/MSystem/Misc/MRegistry_Static.cpp:488; Shared/MOS/MSystem/Raster/MTextureContainerXTC2.cpp:645; Shared/MOS/MSystem/Raster/MTextureContainers.cpp:5717; Shared/MOS/MSystem/Sound/MSound_WaveContainer_Plain.cpp:1722; Shared/MOS/MSystem/Sound/MSound_WaveContainer_Plain.cpp:1799; Shared/MOS/XR/XRSurf.cpp:2524; Shared/MOS/XR/XRSurf.cpp:2528; Shared/MOS/XR/XRSurf.cpp:2539; Shared/MOS/XR/XRSurf.cpp:2543 |
| `FUN_10025900` | GameClasses | `CStrBase::CompareNoCase` | Projects/Main/GameClasses/WObj_AI/AICore.cpp:11517; Projects/Main/GameClasses/WObj_AI/AICore.cpp:11518; Projects/Main/GameClasses/WObj_AI/AI_KnowledgeBase.cpp:2378; Projects/Main/GameClasses/WObj_AI/AI_KnowledgeBase.cpp:2379; Projects/Main/GameClasses/WObj_AI/AI_KnowledgeBase.cpp:2441; Projects/Main/GameClasses/WObj_AI/AI_KnowledgeBase.cpp:2442; Projects/Main/GameClasses/WObj_AI/AI_KnowledgeBase.cpp:4323; Projects/Main/GameClasses/WObj_AI/AI_KnowledgeBase.cpp:4324; Projects/Main/GameClasses/WObj_CharMsg.cpp:2858; Projects/Main/GameClasses/WObj_CharMsg.cpp:3271; Projects/Main/GameClasses/WObj_CharMsg.cpp:7497; Projects/Main/GameClasses/WObj_CharMsg.cpp:756; Projects/Main/GameClasses/WObj_CharMsg.cpp:768; Projects/Main/GameClasses/WObj_Game/WObj_GameCore.cpp:1551; Projects/Main/GameClasses/WObj_Game/WObj_GameMod.cpp:828; Shared/MCC/MDA_Hash.cpp:303; Shared/MCC/MRTC_StrBase.cpp:857; Shared/MCC/MRTC_StrBase.cpp:878; Shared/MCC/MRTC_StrBase.cpp:890; Shared/MCC/MRTC_StrBase.cpp:902; Shared/MCC/MRTC_StrBase.cpp:923; Shared/MCC/MRTC_StrBase.cpp:932; Shared/MCC/MRTC_StrBase.cpp:941; Shared/MCC/MRTC_StrBase.cpp:979; Shared/MCC/MRTC_StrBase.cpp:985; Shared/MCC/MRTC_StrBase.cpp:991; Shared/MOS/Classes/GameWorld/WObjects/WObj_SimpleMessage.cpp:431; Shared/MOS/Classes/GameWorld/WObjects/WObj_SimpleMessage.cpp:434; Shared/MOS/Classes/GameWorld/WObjects/WObj_SimpleMessage.cpp:437; Shared/MOS/Classes/GameWorld/WObjects/WObj_SimpleMessage.cpp:440; Shared/MOS/Classes/GameWorld/WObjects/WObj_SimpleMessage.cpp:443; Shared/MOS/Classes/GameWorld/WObjects/WObj_SimpleMessage.cpp:446; Shared/MOS/MSystem/Misc/MRegistry_Static.cpp:411; Shared/MOS/MSystem/Misc/MRegistry_Static.cpp:425; Shared/MOS/MSystem/Misc/MRegistry_Static.cpp:442; Shared/MOS/MSystem/Misc/MRegistry_Static.cpp:456; Shared/MOS/MSystem/Misc/MRegistry_Static.cpp:470; Shared/MOS/MSystem/Misc/MRegistry_Static.cpp:488; Shared/MOS/MSystem/Raster/MTextureContainerXTC2.cpp:645; Shared/MOS/MSystem/Raster/MTextureContainers.cpp:5717; Shared/MOS/MSystem/Sound/MSound_WaveContainer_Plain.cpp:1722; Shared/MOS/MSystem/Sound/MSound_WaveContainer_Plain.cpp:1799; Shared/MOS/XR/XRSurf.cpp:2524; Shared/MOS/XR/XRSurf.cpp:2528; Shared/MOS/XR/XRSurf.cpp:2539; Shared/MOS/XR/XRSurf.cpp:2543 |
| `FUN_100257d0` | GameWorld | `CStrBase::CompareNoCase` | Projects/Main/GameClasses/WObj_AI/AICore.cpp:11517; Projects/Main/GameClasses/WObj_AI/AICore.cpp:11518; Projects/Main/GameClasses/WObj_AI/AI_KnowledgeBase.cpp:2378; Projects/Main/GameClasses/WObj_AI/AI_KnowledgeBase.cpp:2379; Projects/Main/GameClasses/WObj_AI/AI_KnowledgeBase.cpp:2441; Projects/Main/GameClasses/WObj_AI/AI_KnowledgeBase.cpp:2442; Projects/Main/GameClasses/WObj_AI/AI_KnowledgeBase.cpp:4323; Projects/Main/GameClasses/WObj_AI/AI_KnowledgeBase.cpp:4324; Projects/Main/GameClasses/WObj_CharMsg.cpp:2858; Projects/Main/GameClasses/WObj_CharMsg.cpp:3271; Projects/Main/GameClasses/WObj_CharMsg.cpp:7497; Projects/Main/GameClasses/WObj_CharMsg.cpp:756; Projects/Main/GameClasses/WObj_CharMsg.cpp:768; Projects/Main/GameClasses/WObj_Game/WObj_GameCore.cpp:1551; Projects/Main/GameClasses/WObj_Game/WObj_GameMod.cpp:828; Shared/MCC/MDA_Hash.cpp:303; Shared/MCC/MRTC_StrBase.cpp:857; Shared/MCC/MRTC_StrBase.cpp:878; Shared/MCC/MRTC_StrBase.cpp:890; Shared/MCC/MRTC_StrBase.cpp:902; Shared/MCC/MRTC_StrBase.cpp:923; Shared/MCC/MRTC_StrBase.cpp:932; Shared/MCC/MRTC_StrBase.cpp:941; Shared/MCC/MRTC_StrBase.cpp:979; Shared/MCC/MRTC_StrBase.cpp:985; Shared/MCC/MRTC_StrBase.cpp:991; Shared/MOS/Classes/GameWorld/WObjects/WObj_SimpleMessage.cpp:431; Shared/MOS/Classes/GameWorld/WObjects/WObj_SimpleMessage.cpp:434; Shared/MOS/Classes/GameWorld/WObjects/WObj_SimpleMessage.cpp:437; Shared/MOS/Classes/GameWorld/WObjects/WObj_SimpleMessage.cpp:440; Shared/MOS/Classes/GameWorld/WObjects/WObj_SimpleMessage.cpp:443; Shared/MOS/Classes/GameWorld/WObjects/WObj_SimpleMessage.cpp:446; Shared/MOS/MSystem/Misc/MRegistry_Static.cpp:411; Shared/MOS/MSystem/Misc/MRegistry_Static.cpp:425; Shared/MOS/MSystem/Misc/MRegistry_Static.cpp:442; Shared/MOS/MSystem/Misc/MRegistry_Static.cpp:456; Shared/MOS/MSystem/Misc/MRegistry_Static.cpp:470; Shared/MOS/MSystem/Misc/MRegistry_Static.cpp:488; Shared/MOS/MSystem/Raster/MTextureContainerXTC2.cpp:645; Shared/MOS/MSystem/Raster/MTextureContainers.cpp:5717; Shared/MOS/MSystem/Sound/MSound_WaveContainer_Plain.cpp:1722; Shared/MOS/MSystem/Sound/MSound_WaveContainer_Plain.cpp:1799; Shared/MOS/XR/XRSurf.cpp:2524; Shared/MOS/XR/XRSurf.cpp:2528; Shared/MOS/XR/XRSurf.cpp:2539; Shared/MOS/XR/XRSurf.cpp:2543 |
| `FUN_10025900` | GameWorld | `CStrBase::CompareNoCase` | Projects/Main/GameClasses/WObj_AI/AICore.cpp:11517; Projects/Main/GameClasses/WObj_AI/AICore.cpp:11518; Projects/Main/GameClasses/WObj_AI/AI_KnowledgeBase.cpp:2378; Projects/Main/GameClasses/WObj_AI/AI_KnowledgeBase.cpp:2379; Projects/Main/GameClasses/WObj_AI/AI_KnowledgeBase.cpp:2441; Projects/Main/GameClasses/WObj_AI/AI_KnowledgeBase.cpp:2442; Projects/Main/GameClasses/WObj_AI/AI_KnowledgeBase.cpp:4323; Projects/Main/GameClasses/WObj_AI/AI_KnowledgeBase.cpp:4324; Projects/Main/GameClasses/WObj_CharMsg.cpp:2858; Projects/Main/GameClasses/WObj_CharMsg.cpp:3271; Projects/Main/GameClasses/WObj_CharMsg.cpp:7497; Projects/Main/GameClasses/WObj_CharMsg.cpp:756; Projects/Main/GameClasses/WObj_CharMsg.cpp:768; Projects/Main/GameClasses/WObj_Game/WObj_GameCore.cpp:1551; Projects/Main/GameClasses/WObj_Game/WObj_GameMod.cpp:828; Shared/MCC/MDA_Hash.cpp:303; Shared/MCC/MRTC_StrBase.cpp:857; Shared/MCC/MRTC_StrBase.cpp:878; Shared/MCC/MRTC_StrBase.cpp:890; Shared/MCC/MRTC_StrBase.cpp:902; Shared/MCC/MRTC_StrBase.cpp:923; Shared/MCC/MRTC_StrBase.cpp:932; Shared/MCC/MRTC_StrBase.cpp:941; Shared/MCC/MRTC_StrBase.cpp:979; Shared/MCC/MRTC_StrBase.cpp:985; Shared/MCC/MRTC_StrBase.cpp:991; Shared/MOS/Classes/GameWorld/WObjects/WObj_SimpleMessage.cpp:431; Shared/MOS/Classes/GameWorld/WObjects/WObj_SimpleMessage.cpp:434; Shared/MOS/Classes/GameWorld/WObjects/WObj_SimpleMessage.cpp:437; Shared/MOS/Classes/GameWorld/WObjects/WObj_SimpleMessage.cpp:440; Shared/MOS/Classes/GameWorld/WObjects/WObj_SimpleMessage.cpp:443; Shared/MOS/Classes/GameWorld/WObjects/WObj_SimpleMessage.cpp:446; Shared/MOS/MSystem/Misc/MRegistry_Static.cpp:411; Shared/MOS/MSystem/Misc/MRegistry_Static.cpp:425; Shared/MOS/MSystem/Misc/MRegistry_Static.cpp:442; Shared/MOS/MSystem/Misc/MRegistry_Static.cpp:456; Shared/MOS/MSystem/Misc/MRegistry_Static.cpp:470; Shared/MOS/MSystem/Misc/MRegistry_Static.cpp:488; Shared/MOS/MSystem/Raster/MTextureContainerXTC2.cpp:645; Shared/MOS/MSystem/Raster/MTextureContainers.cpp:5717; Shared/MOS/MSystem/Sound/MSound_WaveContainer_Plain.cpp:1722; Shared/MOS/MSystem/Sound/MSound_WaveContainer_Plain.cpp:1799; Shared/MOS/XR/XRSurf.cpp:2524; Shared/MOS/XR/XRSurf.cpp:2528; Shared/MOS/XR/XRSurf.cpp:2539; Shared/MOS/XR/XRSurf.cpp:2543 |
| `FUN_10031a80` | MCCDyn | `CStrBase::CompareNoCase` | Projects/Main/GameClasses/WObj_AI/AICore.cpp:11517; Projects/Main/GameClasses/WObj_AI/AICore.cpp:11518; Projects/Main/GameClasses/WObj_AI/AI_KnowledgeBase.cpp:2378; Projects/Main/GameClasses/WObj_AI/AI_KnowledgeBase.cpp:2379; Projects/Main/GameClasses/WObj_AI/AI_KnowledgeBase.cpp:2441; Projects/Main/GameClasses/WObj_AI/AI_KnowledgeBase.cpp:2442; Projects/Main/GameClasses/WObj_AI/AI_KnowledgeBase.cpp:4323; Projects/Main/GameClasses/WObj_AI/AI_KnowledgeBase.cpp:4324; Projects/Main/GameClasses/WObj_CharMsg.cpp:2858; Projects/Main/GameClasses/WObj_CharMsg.cpp:3271; Projects/Main/GameClasses/WObj_CharMsg.cpp:7497; Projects/Main/GameClasses/WObj_CharMsg.cpp:756; Projects/Main/GameClasses/WObj_CharMsg.cpp:768; Projects/Main/GameClasses/WObj_Game/WObj_GameCore.cpp:1551; Projects/Main/GameClasses/WObj_Game/WObj_GameMod.cpp:828; Shared/MCC/MDA_Hash.cpp:303; Shared/MCC/MRTC_StrBase.cpp:857; Shared/MCC/MRTC_StrBase.cpp:878; Shared/MCC/MRTC_StrBase.cpp:890; Shared/MCC/MRTC_StrBase.cpp:902; Shared/MCC/MRTC_StrBase.cpp:923; Shared/MCC/MRTC_StrBase.cpp:932; Shared/MCC/MRTC_StrBase.cpp:941; Shared/MCC/MRTC_StrBase.cpp:979; Shared/MCC/MRTC_StrBase.cpp:985; Shared/MCC/MRTC_StrBase.cpp:991; Shared/MOS/Classes/GameWorld/WObjects/WObj_SimpleMessage.cpp:431; Shared/MOS/Classes/GameWorld/WObjects/WObj_SimpleMessage.cpp:434; Shared/MOS/Classes/GameWorld/WObjects/WObj_SimpleMessage.cpp:437; Shared/MOS/Classes/GameWorld/WObjects/WObj_SimpleMessage.cpp:440; Shared/MOS/Classes/GameWorld/WObjects/WObj_SimpleMessage.cpp:443; Shared/MOS/Classes/GameWorld/WObjects/WObj_SimpleMessage.cpp:446; Shared/MOS/MSystem/Misc/MRegistry_Static.cpp:411; Shared/MOS/MSystem/Misc/MRegistry_Static.cpp:425; Shared/MOS/MSystem/Misc/MRegistry_Static.cpp:442; Shared/MOS/MSystem/Misc/MRegistry_Static.cpp:456; Shared/MOS/MSystem/Misc/MRegistry_Static.cpp:470; Shared/MOS/MSystem/Misc/MRegistry_Static.cpp:488; Shared/MOS/MSystem/Raster/MTextureContainerXTC2.cpp:645; Shared/MOS/MSystem/Raster/MTextureContainers.cpp:5717; Shared/MOS/MSystem/Sound/MSound_WaveContainer_Plain.cpp:1722; Shared/MOS/MSystem/Sound/MSound_WaveContainer_Plain.cpp:1799; Shared/MOS/XR/XRSurf.cpp:2524; Shared/MOS/XR/XRSurf.cpp:2528; Shared/MOS/XR/XRSurf.cpp:2539; Shared/MOS/XR/XRSurf.cpp:2543 |
| `FUN_10031bb0` | MCCDyn | `CStrBase::CompareNoCase` | Projects/Main/GameClasses/WObj_AI/AICore.cpp:11517; Projects/Main/GameClasses/WObj_AI/AICore.cpp:11518; Projects/Main/GameClasses/WObj_AI/AI_KnowledgeBase.cpp:2378; Projects/Main/GameClasses/WObj_AI/AI_KnowledgeBase.cpp:2379; Projects/Main/GameClasses/WObj_AI/AI_KnowledgeBase.cpp:2441; Projects/Main/GameClasses/WObj_AI/AI_KnowledgeBase.cpp:2442; Projects/Main/GameClasses/WObj_AI/AI_KnowledgeBase.cpp:4323; Projects/Main/GameClasses/WObj_AI/AI_KnowledgeBase.cpp:4324; Projects/Main/GameClasses/WObj_CharMsg.cpp:2858; Projects/Main/GameClasses/WObj_CharMsg.cpp:3271; Projects/Main/GameClasses/WObj_CharMsg.cpp:7497; Projects/Main/GameClasses/WObj_CharMsg.cpp:756; Projects/Main/GameClasses/WObj_CharMsg.cpp:768; Projects/Main/GameClasses/WObj_Game/WObj_GameCore.cpp:1551; Projects/Main/GameClasses/WObj_Game/WObj_GameMod.cpp:828; Shared/MCC/MDA_Hash.cpp:303; Shared/MCC/MRTC_StrBase.cpp:857; Shared/MCC/MRTC_StrBase.cpp:878; Shared/MCC/MRTC_StrBase.cpp:890; Shared/MCC/MRTC_StrBase.cpp:902; Shared/MCC/MRTC_StrBase.cpp:923; Shared/MCC/MRTC_StrBase.cpp:932; Shared/MCC/MRTC_StrBase.cpp:941; Shared/MCC/MRTC_StrBase.cpp:979; Shared/MCC/MRTC_StrBase.cpp:985; Shared/MCC/MRTC_StrBase.cpp:991; Shared/MOS/Classes/GameWorld/WObjects/WObj_SimpleMessage.cpp:431; Shared/MOS/Classes/GameWorld/WObjects/WObj_SimpleMessage.cpp:434; Shared/MOS/Classes/GameWorld/WObjects/WObj_SimpleMessage.cpp:437; Shared/MOS/Classes/GameWorld/WObjects/WObj_SimpleMessage.cpp:440; Shared/MOS/Classes/GameWorld/WObjects/WObj_SimpleMessage.cpp:443; Shared/MOS/Classes/GameWorld/WObjects/WObj_SimpleMessage.cpp:446; Shared/MOS/MSystem/Misc/MRegistry_Static.cpp:411; Shared/MOS/MSystem/Misc/MRegistry_Static.cpp:425; Shared/MOS/MSystem/Misc/MRegistry_Static.cpp:442; Shared/MOS/MSystem/Misc/MRegistry_Static.cpp:456; Shared/MOS/MSystem/Misc/MRegistry_Static.cpp:470; Shared/MOS/MSystem/Misc/MRegistry_Static.cpp:488; Shared/MOS/MSystem/Raster/MTextureContainerXTC2.cpp:645; Shared/MOS/MSystem/Raster/MTextureContainers.cpp:5717; Shared/MOS/MSystem/Sound/MSound_WaveContainer_Plain.cpp:1722; Shared/MOS/MSystem/Sound/MSound_WaveContainer_Plain.cpp:1799; Shared/MOS/XR/XRSurf.cpp:2524; Shared/MOS/XR/XRSurf.cpp:2528; Shared/MOS/XR/XRSurf.cpp:2539; Shared/MOS/XR/XRSurf.cpp:2543 |
| `FUN_100203c0` | MSystem | `CStrBase::CompareNoCase` | Projects/Main/GameClasses/WObj_AI/AICore.cpp:11517; Projects/Main/GameClasses/WObj_AI/AICore.cpp:11518; Projects/Main/GameClasses/WObj_AI/AI_KnowledgeBase.cpp:2378; Projects/Main/GameClasses/WObj_AI/AI_KnowledgeBase.cpp:2379; Projects/Main/GameClasses/WObj_AI/AI_KnowledgeBase.cpp:2441; Projects/Main/GameClasses/WObj_AI/AI_KnowledgeBase.cpp:2442; Projects/Main/GameClasses/WObj_AI/AI_KnowledgeBase.cpp:4323; Projects/Main/GameClasses/WObj_AI/AI_KnowledgeBase.cpp:4324; Projects/Main/GameClasses/WObj_CharMsg.cpp:2858; Projects/Main/GameClasses/WObj_CharMsg.cpp:3271; Projects/Main/GameClasses/WObj_CharMsg.cpp:7497; Projects/Main/GameClasses/WObj_CharMsg.cpp:756; Projects/Main/GameClasses/WObj_CharMsg.cpp:768; Projects/Main/GameClasses/WObj_Game/WObj_GameCore.cpp:1551; Projects/Main/GameClasses/WObj_Game/WObj_GameMod.cpp:828; Shared/MCC/MDA_Hash.cpp:303; Shared/MCC/MRTC_StrBase.cpp:857; Shared/MCC/MRTC_StrBase.cpp:878; Shared/MCC/MRTC_StrBase.cpp:890; Shared/MCC/MRTC_StrBase.cpp:902; Shared/MCC/MRTC_StrBase.cpp:923; Shared/MCC/MRTC_StrBase.cpp:932; Shared/MCC/MRTC_StrBase.cpp:941; Shared/MCC/MRTC_StrBase.cpp:979; Shared/MCC/MRTC_StrBase.cpp:985; Shared/MCC/MRTC_StrBase.cpp:991; Shared/MOS/Classes/GameWorld/WObjects/WObj_SimpleMessage.cpp:431; Shared/MOS/Classes/GameWorld/WObjects/WObj_SimpleMessage.cpp:434; Shared/MOS/Classes/GameWorld/WObjects/WObj_SimpleMessage.cpp:437; Shared/MOS/Classes/GameWorld/WObjects/WObj_SimpleMessage.cpp:440; Shared/MOS/Classes/GameWorld/WObjects/WObj_SimpleMessage.cpp:443; Shared/MOS/Classes/GameWorld/WObjects/WObj_SimpleMessage.cpp:446; Shared/MOS/MSystem/Misc/MRegistry_Static.cpp:411; Shared/MOS/MSystem/Misc/MRegistry_Static.cpp:425; Shared/MOS/MSystem/Misc/MRegistry_Static.cpp:442; Shared/MOS/MSystem/Misc/MRegistry_Static.cpp:456; Shared/MOS/MSystem/Misc/MRegistry_Static.cpp:470; Shared/MOS/MSystem/Misc/MRegistry_Static.cpp:488; Shared/MOS/MSystem/Raster/MTextureContainerXTC2.cpp:645; Shared/MOS/MSystem/Raster/MTextureContainers.cpp:5717; Shared/MOS/MSystem/Sound/MSound_WaveContainer_Plain.cpp:1722; Shared/MOS/MSystem/Sound/MSound_WaveContainer_Plain.cpp:1799; Shared/MOS/XR/XRSurf.cpp:2524; Shared/MOS/XR/XRSurf.cpp:2528; Shared/MOS/XR/XRSurf.cpp:2539; Shared/MOS/XR/XRSurf.cpp:2543 |
| `FUN_100204f0` | MSystem | `CStrBase::CompareNoCase` | Projects/Main/GameClasses/WObj_AI/AICore.cpp:11517; Projects/Main/GameClasses/WObj_AI/AICore.cpp:11518; Projects/Main/GameClasses/WObj_AI/AI_KnowledgeBase.cpp:2378; Projects/Main/GameClasses/WObj_AI/AI_KnowledgeBase.cpp:2379; Projects/Main/GameClasses/WObj_AI/AI_KnowledgeBase.cpp:2441; Projects/Main/GameClasses/WObj_AI/AI_KnowledgeBase.cpp:2442; Projects/Main/GameClasses/WObj_AI/AI_KnowledgeBase.cpp:4323; Projects/Main/GameClasses/WObj_AI/AI_KnowledgeBase.cpp:4324; Projects/Main/GameClasses/WObj_CharMsg.cpp:2858; Projects/Main/GameClasses/WObj_CharMsg.cpp:3271; Projects/Main/GameClasses/WObj_CharMsg.cpp:7497; Projects/Main/GameClasses/WObj_CharMsg.cpp:756; Projects/Main/GameClasses/WObj_CharMsg.cpp:768; Projects/Main/GameClasses/WObj_Game/WObj_GameCore.cpp:1551; Projects/Main/GameClasses/WObj_Game/WObj_GameMod.cpp:828; Shared/MCC/MDA_Hash.cpp:303; Shared/MCC/MRTC_StrBase.cpp:857; Shared/MCC/MRTC_StrBase.cpp:878; Shared/MCC/MRTC_StrBase.cpp:890; Shared/MCC/MRTC_StrBase.cpp:902; Shared/MCC/MRTC_StrBase.cpp:923; Shared/MCC/MRTC_StrBase.cpp:932; Shared/MCC/MRTC_StrBase.cpp:941; Shared/MCC/MRTC_StrBase.cpp:979; Shared/MCC/MRTC_StrBase.cpp:985; Shared/MCC/MRTC_StrBase.cpp:991; Shared/MOS/Classes/GameWorld/WObjects/WObj_SimpleMessage.cpp:431; Shared/MOS/Classes/GameWorld/WObjects/WObj_SimpleMessage.cpp:434; Shared/MOS/Classes/GameWorld/WObjects/WObj_SimpleMessage.cpp:437; Shared/MOS/Classes/GameWorld/WObjects/WObj_SimpleMessage.cpp:440; Shared/MOS/Classes/GameWorld/WObjects/WObj_SimpleMessage.cpp:443; Shared/MOS/Classes/GameWorld/WObjects/WObj_SimpleMessage.cpp:446; Shared/MOS/MSystem/Misc/MRegistry_Static.cpp:411; Shared/MOS/MSystem/Misc/MRegistry_Static.cpp:425; Shared/MOS/MSystem/Misc/MRegistry_Static.cpp:442; Shared/MOS/MSystem/Misc/MRegistry_Static.cpp:456; Shared/MOS/MSystem/Misc/MRegistry_Static.cpp:470; Shared/MOS/MSystem/Misc/MRegistry_Static.cpp:488; Shared/MOS/MSystem/Raster/MTextureContainerXTC2.cpp:645; Shared/MOS/MSystem/Raster/MTextureContainers.cpp:5717; Shared/MOS/MSystem/Sound/MSound_WaveContainer_Plain.cpp:1722; Shared/MOS/MSystem/Sound/MSound_WaveContainer_Plain.cpp:1799; Shared/MOS/XR/XRSurf.cpp:2524; Shared/MOS/XR/XRSurf.cpp:2528; Shared/MOS/XR/XRSurf.cpp:2539; Shared/MOS/XR/XRSurf.cpp:2543 |
| `FUN_100257d0` | MXR | `CStrBase::CompareNoCase` | Projects/Main/GameClasses/WObj_AI/AICore.cpp:11517; Projects/Main/GameClasses/WObj_AI/AICore.cpp:11518; Projects/Main/GameClasses/WObj_AI/AI_KnowledgeBase.cpp:2378; Projects/Main/GameClasses/WObj_AI/AI_KnowledgeBase.cpp:2379; Projects/Main/GameClasses/WObj_AI/AI_KnowledgeBase.cpp:2441; Projects/Main/GameClasses/WObj_AI/AI_KnowledgeBase.cpp:2442; Projects/Main/GameClasses/WObj_AI/AI_KnowledgeBase.cpp:4323; Projects/Main/GameClasses/WObj_AI/AI_KnowledgeBase.cpp:4324; Projects/Main/GameClasses/WObj_CharMsg.cpp:2858; Projects/Main/GameClasses/WObj_CharMsg.cpp:3271; Projects/Main/GameClasses/WObj_CharMsg.cpp:7497; Projects/Main/GameClasses/WObj_CharMsg.cpp:756; Projects/Main/GameClasses/WObj_CharMsg.cpp:768; Projects/Main/GameClasses/WObj_Game/WObj_GameCore.cpp:1551; Projects/Main/GameClasses/WObj_Game/WObj_GameMod.cpp:828; Shared/MCC/MDA_Hash.cpp:303; Shared/MCC/MRTC_StrBase.cpp:857; Shared/MCC/MRTC_StrBase.cpp:878; Shared/MCC/MRTC_StrBase.cpp:890; Shared/MCC/MRTC_StrBase.cpp:902; Shared/MCC/MRTC_StrBase.cpp:923; Shared/MCC/MRTC_StrBase.cpp:932; Shared/MCC/MRTC_StrBase.cpp:941; Shared/MCC/MRTC_StrBase.cpp:979; Shared/MCC/MRTC_StrBase.cpp:985; Shared/MCC/MRTC_StrBase.cpp:991; Shared/MOS/Classes/GameWorld/WObjects/WObj_SimpleMessage.cpp:431; Shared/MOS/Classes/GameWorld/WObjects/WObj_SimpleMessage.cpp:434; Shared/MOS/Classes/GameWorld/WObjects/WObj_SimpleMessage.cpp:437; Shared/MOS/Classes/GameWorld/WObjects/WObj_SimpleMessage.cpp:440; Shared/MOS/Classes/GameWorld/WObjects/WObj_SimpleMessage.cpp:443; Shared/MOS/Classes/GameWorld/WObjects/WObj_SimpleMessage.cpp:446; Shared/MOS/MSystem/Misc/MRegistry_Static.cpp:411; Shared/MOS/MSystem/Misc/MRegistry_Static.cpp:425; Shared/MOS/MSystem/Misc/MRegistry_Static.cpp:442; Shared/MOS/MSystem/Misc/MRegistry_Static.cpp:456; Shared/MOS/MSystem/Misc/MRegistry_Static.cpp:470; Shared/MOS/MSystem/Misc/MRegistry_Static.cpp:488; Shared/MOS/MSystem/Raster/MTextureContainerXTC2.cpp:645; Shared/MOS/MSystem/Raster/MTextureContainers.cpp:5717; Shared/MOS/MSystem/Sound/MSound_WaveContainer_Plain.cpp:1722; Shared/MOS/MSystem/Sound/MSound_WaveContainer_Plain.cpp:1799; Shared/MOS/XR/XRSurf.cpp:2524; Shared/MOS/XR/XRSurf.cpp:2528; Shared/MOS/XR/XRSurf.cpp:2539; Shared/MOS/XR/XRSurf.cpp:2543 |
| `FUN_10025900` | MXR | `CStrBase::CompareNoCase` | Projects/Main/GameClasses/WObj_AI/AICore.cpp:11517; Projects/Main/GameClasses/WObj_AI/AICore.cpp:11518; Projects/Main/GameClasses/WObj_AI/AI_KnowledgeBase.cpp:2378; Projects/Main/GameClasses/WObj_AI/AI_KnowledgeBase.cpp:2379; Projects/Main/GameClasses/WObj_AI/AI_KnowledgeBase.cpp:2441; Projects/Main/GameClasses/WObj_AI/AI_KnowledgeBase.cpp:2442; Projects/Main/GameClasses/WObj_AI/AI_KnowledgeBase.cpp:4323; Projects/Main/GameClasses/WObj_AI/AI_KnowledgeBase.cpp:4324; Projects/Main/GameClasses/WObj_CharMsg.cpp:2858; Projects/Main/GameClasses/WObj_CharMsg.cpp:3271; Projects/Main/GameClasses/WObj_CharMsg.cpp:7497; Projects/Main/GameClasses/WObj_CharMsg.cpp:756; Projects/Main/GameClasses/WObj_CharMsg.cpp:768; Projects/Main/GameClasses/WObj_Game/WObj_GameCore.cpp:1551; Projects/Main/GameClasses/WObj_Game/WObj_GameMod.cpp:828; Shared/MCC/MDA_Hash.cpp:303; Shared/MCC/MRTC_StrBase.cpp:857; Shared/MCC/MRTC_StrBase.cpp:878; Shared/MCC/MRTC_StrBase.cpp:890; Shared/MCC/MRTC_StrBase.cpp:902; Shared/MCC/MRTC_StrBase.cpp:923; Shared/MCC/MRTC_StrBase.cpp:932; Shared/MCC/MRTC_StrBase.cpp:941; Shared/MCC/MRTC_StrBase.cpp:979; Shared/MCC/MRTC_StrBase.cpp:985; Shared/MCC/MRTC_StrBase.cpp:991; Shared/MOS/Classes/GameWorld/WObjects/WObj_SimpleMessage.cpp:431; Shared/MOS/Classes/GameWorld/WObjects/WObj_SimpleMessage.cpp:434; Shared/MOS/Classes/GameWorld/WObjects/WObj_SimpleMessage.cpp:437; Shared/MOS/Classes/GameWorld/WObjects/WObj_SimpleMessage.cpp:440; Shared/MOS/Classes/GameWorld/WObjects/WObj_SimpleMessage.cpp:443; Shared/MOS/Classes/GameWorld/WObjects/WObj_SimpleMessage.cpp:446; Shared/MOS/MSystem/Misc/MRegistry_Static.cpp:411; Shared/MOS/MSystem/Misc/MRegistry_Static.cpp:425; Shared/MOS/MSystem/Misc/MRegistry_Static.cpp:442; Shared/MOS/MSystem/Misc/MRegistry_Static.cpp:456; Shared/MOS/MSystem/Misc/MRegistry_Static.cpp:470; Shared/MOS/MSystem/Misc/MRegistry_Static.cpp:488; Shared/MOS/MSystem/Raster/MTextureContainerXTC2.cpp:645; Shared/MOS/MSystem/Raster/MTextureContainers.cpp:5717; Shared/MOS/MSystem/Sound/MSound_WaveContainer_Plain.cpp:1722; Shared/MOS/MSystem/Sound/MSound_WaveContainer_Plain.cpp:1799; Shared/MOS/XR/XRSurf.cpp:2524; Shared/MOS/XR/XRSurf.cpp:2528; Shared/MOS/XR/XRSurf.cpp:2539; Shared/MOS/XR/XRSurf.cpp:2543 |
| `FUN_10025810` | RndrGL | `CStrBase::CompareNoCase` | Projects/Main/GameClasses/WObj_AI/AICore.cpp:11517; Projects/Main/GameClasses/WObj_AI/AICore.cpp:11518; Projects/Main/GameClasses/WObj_AI/AI_KnowledgeBase.cpp:2378; Projects/Main/GameClasses/WObj_AI/AI_KnowledgeBase.cpp:2379; Projects/Main/GameClasses/WObj_AI/AI_KnowledgeBase.cpp:2441; Projects/Main/GameClasses/WObj_AI/AI_KnowledgeBase.cpp:2442; Projects/Main/GameClasses/WObj_AI/AI_KnowledgeBase.cpp:4323; Projects/Main/GameClasses/WObj_AI/AI_KnowledgeBase.cpp:4324; Projects/Main/GameClasses/WObj_CharMsg.cpp:2858; Projects/Main/GameClasses/WObj_CharMsg.cpp:3271; Projects/Main/GameClasses/WObj_CharMsg.cpp:7497; Projects/Main/GameClasses/WObj_CharMsg.cpp:756; Projects/Main/GameClasses/WObj_CharMsg.cpp:768; Projects/Main/GameClasses/WObj_Game/WObj_GameCore.cpp:1551; Projects/Main/GameClasses/WObj_Game/WObj_GameMod.cpp:828; Shared/MCC/MDA_Hash.cpp:303; Shared/MCC/MRTC_StrBase.cpp:857; Shared/MCC/MRTC_StrBase.cpp:878; Shared/MCC/MRTC_StrBase.cpp:890; Shared/MCC/MRTC_StrBase.cpp:902; Shared/MCC/MRTC_StrBase.cpp:923; Shared/MCC/MRTC_StrBase.cpp:932; Shared/MCC/MRTC_StrBase.cpp:941; Shared/MCC/MRTC_StrBase.cpp:979; Shared/MCC/MRTC_StrBase.cpp:985; Shared/MCC/MRTC_StrBase.cpp:991; Shared/MOS/Classes/GameWorld/WObjects/WObj_SimpleMessage.cpp:431; Shared/MOS/Classes/GameWorld/WObjects/WObj_SimpleMessage.cpp:434; Shared/MOS/Classes/GameWorld/WObjects/WObj_SimpleMessage.cpp:437; Shared/MOS/Classes/GameWorld/WObjects/WObj_SimpleMessage.cpp:440; Shared/MOS/Classes/GameWorld/WObjects/WObj_SimpleMessage.cpp:443; Shared/MOS/Classes/GameWorld/WObjects/WObj_SimpleMessage.cpp:446; Shared/MOS/MSystem/Misc/MRegistry_Static.cpp:411; Shared/MOS/MSystem/Misc/MRegistry_Static.cpp:425; Shared/MOS/MSystem/Misc/MRegistry_Static.cpp:442; Shared/MOS/MSystem/Misc/MRegistry_Static.cpp:456; Shared/MOS/MSystem/Misc/MRegistry_Static.cpp:470; Shared/MOS/MSystem/Misc/MRegistry_Static.cpp:488; Shared/MOS/MSystem/Raster/MTextureContainerXTC2.cpp:645; Shared/MOS/MSystem/Raster/MTextureContainers.cpp:5717; Shared/MOS/MSystem/Sound/MSound_WaveContainer_Plain.cpp:1722; Shared/MOS/MSystem/Sound/MSound_WaveContainer_Plain.cpp:1799; Shared/MOS/XR/XRSurf.cpp:2524; Shared/MOS/XR/XRSurf.cpp:2528; Shared/MOS/XR/XRSurf.cpp:2539; Shared/MOS/XR/XRSurf.cpp:2543 |
| `FUN_10025940` | RndrGL | `CStrBase::CompareNoCase` | Projects/Main/GameClasses/WObj_AI/AICore.cpp:11517; Projects/Main/GameClasses/WObj_AI/AICore.cpp:11518; Projects/Main/GameClasses/WObj_AI/AI_KnowledgeBase.cpp:2378; Projects/Main/GameClasses/WObj_AI/AI_KnowledgeBase.cpp:2379; Projects/Main/GameClasses/WObj_AI/AI_KnowledgeBase.cpp:2441; Projects/Main/GameClasses/WObj_AI/AI_KnowledgeBase.cpp:2442; Projects/Main/GameClasses/WObj_AI/AI_KnowledgeBase.cpp:4323; Projects/Main/GameClasses/WObj_AI/AI_KnowledgeBase.cpp:4324; Projects/Main/GameClasses/WObj_CharMsg.cpp:2858; Projects/Main/GameClasses/WObj_CharMsg.cpp:3271; Projects/Main/GameClasses/WObj_CharMsg.cpp:7497; Projects/Main/GameClasses/WObj_CharMsg.cpp:756; Projects/Main/GameClasses/WObj_CharMsg.cpp:768; Projects/Main/GameClasses/WObj_Game/WObj_GameCore.cpp:1551; Projects/Main/GameClasses/WObj_Game/WObj_GameMod.cpp:828; Shared/MCC/MDA_Hash.cpp:303; Shared/MCC/MRTC_StrBase.cpp:857; Shared/MCC/MRTC_StrBase.cpp:878; Shared/MCC/MRTC_StrBase.cpp:890; Shared/MCC/MRTC_StrBase.cpp:902; Shared/MCC/MRTC_StrBase.cpp:923; Shared/MCC/MRTC_StrBase.cpp:932; Shared/MCC/MRTC_StrBase.cpp:941; Shared/MCC/MRTC_StrBase.cpp:979; Shared/MCC/MRTC_StrBase.cpp:985; Shared/MCC/MRTC_StrBase.cpp:991; Shared/MOS/Classes/GameWorld/WObjects/WObj_SimpleMessage.cpp:431; Shared/MOS/Classes/GameWorld/WObjects/WObj_SimpleMessage.cpp:434; Shared/MOS/Classes/GameWorld/WObjects/WObj_SimpleMessage.cpp:437; Shared/MOS/Classes/GameWorld/WObjects/WObj_SimpleMessage.cpp:440; Shared/MOS/Classes/GameWorld/WObjects/WObj_SimpleMessage.cpp:443; Shared/MOS/Classes/GameWorld/WObjects/WObj_SimpleMessage.cpp:446; Shared/MOS/MSystem/Misc/MRegistry_Static.cpp:411; Shared/MOS/MSystem/Misc/MRegistry_Static.cpp:425; Shared/MOS/MSystem/Misc/MRegistry_Static.cpp:442; Shared/MOS/MSystem/Misc/MRegistry_Static.cpp:456; Shared/MOS/MSystem/Misc/MRegistry_Static.cpp:470; Shared/MOS/MSystem/Misc/MRegistry_Static.cpp:488; Shared/MOS/MSystem/Raster/MTextureContainerXTC2.cpp:645; Shared/MOS/MSystem/Raster/MTextureContainers.cpp:5717; Shared/MOS/MSystem/Sound/MSound_WaveContainer_Plain.cpp:1722; Shared/MOS/MSystem/Sound/MSound_WaveContainer_Plain.cpp:1799; Shared/MOS/XR/XRSurf.cpp:2524; Shared/MOS/XR/XRSurf.cpp:2528; Shared/MOS/XR/XRSurf.cpp:2539; Shared/MOS/XR/XRSurf.cpp:2543 |
| `FUN_1000d710` | GameClasses | `CStrData::Create` | Shared/MCC/MRTC_Str.cpp:347; Shared/MCC/MRTC_Str.cpp:413; Shared/MCC/MRTC_Str.cpp:672; Shared/MCC/MRTC_Str.cpp:689; Shared/MCC/MRTC_Str.cpp:789; Shared/MCC/MRTC_Str.cpp:877 |
| `FUN_1000fba0` | GameClasses | `CStrData::Create` | Shared/MCC/MRTC_Str.cpp:347; Shared/MCC/MRTC_Str.cpp:413; Shared/MCC/MRTC_Str.cpp:672; Shared/MCC/MRTC_Str.cpp:689; Shared/MCC/MRTC_Str.cpp:789; Shared/MCC/MRTC_Str.cpp:877 |
| `FUN_1000d710` | GameWorld | `CStrData::Create` | Shared/MCC/MRTC_Str.cpp:347; Shared/MCC/MRTC_Str.cpp:413; Shared/MCC/MRTC_Str.cpp:672; Shared/MCC/MRTC_Str.cpp:689; Shared/MCC/MRTC_Str.cpp:789; Shared/MCC/MRTC_Str.cpp:877 |
| `FUN_1000fba0` | GameWorld | `CStrData::Create` | Shared/MCC/MRTC_Str.cpp:347; Shared/MCC/MRTC_Str.cpp:413; Shared/MCC/MRTC_Str.cpp:672; Shared/MCC/MRTC_Str.cpp:689; Shared/MCC/MRTC_Str.cpp:789; Shared/MCC/MRTC_Str.cpp:877 |
| `FUN_10010910` | MCCDyn | `CStrData::Create` | Shared/MCC/MRTC_Str.cpp:347; Shared/MCC/MRTC_Str.cpp:413; Shared/MCC/MRTC_Str.cpp:672; Shared/MCC/MRTC_Str.cpp:689; Shared/MCC/MRTC_Str.cpp:789; Shared/MCC/MRTC_Str.cpp:877 |
| `FUN_10013080` | MCCDyn | `CStrData::Create` | Shared/MCC/MRTC_Str.cpp:347; Shared/MCC/MRTC_Str.cpp:413; Shared/MCC/MRTC_Str.cpp:672; Shared/MCC/MRTC_Str.cpp:689; Shared/MCC/MRTC_Str.cpp:789; Shared/MCC/MRTC_Str.cpp:877 |
| `FUN_10021c70` | MSystem | `CStrData::Create` | Shared/MCC/MRTC_Str.cpp:347; Shared/MCC/MRTC_Str.cpp:413; Shared/MCC/MRTC_Str.cpp:672; Shared/MCC/MRTC_Str.cpp:689; Shared/MCC/MRTC_Str.cpp:789; Shared/MCC/MRTC_Str.cpp:877 |
| `FUN_10022fd0` | MSystem | `CStrData::Create` | Shared/MCC/MRTC_Str.cpp:347; Shared/MCC/MRTC_Str.cpp:413; Shared/MCC/MRTC_Str.cpp:672; Shared/MCC/MRTC_Str.cpp:689; Shared/MCC/MRTC_Str.cpp:789; Shared/MCC/MRTC_Str.cpp:877 |
| `FUN_1000d710` | MXR | `CStrData::Create` | Shared/MCC/MRTC_Str.cpp:347; Shared/MCC/MRTC_Str.cpp:413; Shared/MCC/MRTC_Str.cpp:672; Shared/MCC/MRTC_Str.cpp:689; Shared/MCC/MRTC_Str.cpp:789; Shared/MCC/MRTC_Str.cpp:877 |
| `FUN_1000fba0` | MXR | `CStrData::Create` | Shared/MCC/MRTC_Str.cpp:347; Shared/MCC/MRTC_Str.cpp:413; Shared/MCC/MRTC_Str.cpp:672; Shared/MCC/MRTC_Str.cpp:689; Shared/MCC/MRTC_Str.cpp:789; Shared/MCC/MRTC_Str.cpp:877 |
| `FUN_1000d750` | RndrGL | `CStrData::Create` | Shared/MCC/MRTC_Str.cpp:347; Shared/MCC/MRTC_Str.cpp:413; Shared/MCC/MRTC_Str.cpp:672; Shared/MCC/MRTC_Str.cpp:689; Shared/MCC/MRTC_Str.cpp:789; Shared/MCC/MRTC_Str.cpp:877 |
| `FUN_1000fbe0` | RndrGL | `CStrData::Create` | Shared/MCC/MRTC_Str.cpp:347; Shared/MCC/MRTC_Str.cpp:413; Shared/MCC/MRTC_Str.cpp:672; Shared/MCC/MRTC_Str.cpp:689; Shared/MCC/MRTC_Str.cpp:789; Shared/MCC/MRTC_Str.cpp:877 |
| `FUN_100cdd20` | MCCDyn | `CStream_SubFile::Open` | Shared/MCC/MFile_Stream_SubFile.cpp:59; Shared/MCC/MFile_Stream_SubFile.cpp:64 |
| `FUN_100cddd0` | MCCDyn | `CStream_SubFile::Open` | Shared/MCC/MFile_Stream_SubFile.cpp:59; Shared/MCC/MFile_Stream_SubFile.cpp:64 |
| `FUN_100cdfe0` | MCCDyn | `CStream_SubFile::Read` | Shared/MCC/MFile_Stream_SubFile.cpp:130; Shared/MCC/MFile_Stream_SubFile.cpp:93 |
| `FUN_100ce0c0` | MCCDyn | `CStream_SubFile::Read` | Shared/MCC/MFile_Stream_SubFile.cpp:130; Shared/MCC/MFile_Stream_SubFile.cpp:93 |
| `FUN_100ce1a0` | MCCDyn | `CStream_SubFile::Read` | Shared/MCC/MFile_Stream_SubFile.cpp:130; Shared/MCC/MFile_Stream_SubFile.cpp:93 |
| `FUN_100ce250` | MCCDyn | `CStream_SubFile::Read` | Shared/MCC/MFile_Stream_SubFile.cpp:130; Shared/MCC/MFile_Stream_SubFile.cpp:93 |
| `FUN_100ce300` | MCCDyn | `CStream_SubFile::Read` | Shared/MCC/MFile_Stream_SubFile.cpp:130; Shared/MCC/MFile_Stream_SubFile.cpp:93 |
| `FUN_100ce3b0` | MCCDyn | `CStream_SubFile::Read` | Shared/MCC/MFile_Stream_SubFile.cpp:130; Shared/MCC/MFile_Stream_SubFile.cpp:93 |
| `FUN_100ce450` | MCCDyn | `CStream_SubFile::Read` | Shared/MCC/MFile_Stream_SubFile.cpp:130; Shared/MCC/MFile_Stream_SubFile.cpp:93 |
| `FUN_100ce4f0` | MCCDyn | `CStream_SubFile::Read` | Shared/MCC/MFile_Stream_SubFile.cpp:130; Shared/MCC/MFile_Stream_SubFile.cpp:93 |
| `FUN_100ce590` | MCCDyn | `CStream_SubFile::Read` | Shared/MCC/MFile_Stream_SubFile.cpp:130; Shared/MCC/MFile_Stream_SubFile.cpp:93 |
| `FUN_1005bda0` | MXR | `CThinKeyContainer::DeleteKey` | Shared/MOS/XR/XRSurf.cpp:4031; Shared/MOS/XR/XRSurf.cpp:4039 |
| `FUN_1005be30` | MXR | `CThinKeyContainer::DeleteKey` | Shared/MOS/XR/XRSurf.cpp:4031; Shared/MOS/XR/XRSurf.cpp:4039 |
| `FUN_10122f00` | MXR | `CTriangleMeshCore::Read` | Shared/MOS/XRModels/Model_TriMesh/WTriMesh.cpp:751; Shared/MOS/XRModels/Model_TriMesh/XMDCommn.cpp:4887 |
| `FUN_1035fac0` | GameClasses | `CWObject_Character::OnClientUpdate` | Projects/Main/GameClasses/WObj_Char/WObj_CharShapeshifter.cpp:718; Projects/Main/GameClasses/WObj_Char/WObj_CharShapeshifter.cpp:726; Projects/Main/GameClasses/WObj_CharIO.cpp:1394 |
| `FUN_10190780` | GameClasses | `CWObject_Engine_Path::GetClientData` | Projects/Main/GameClasses/WObj_Misc/WObj_ActionCutscenecamera.cpp:1237; Projects/Main/GameClasses/WObj_Misc/WObj_ActionCutscenecamera.cpp:1511; Shared/MOS/Classes/GameWorld/WObjects/WObj_Hook.cpp:3083 |
| `FUN_10172820` | GameClasses | `CWObject_GameCore::GetClientData` | Projects/Main/GameClasses/WObj_Game/WObj_GameCore.cpp:418; Projects/Main/GameClasses/WObj_Game/WObj_GameCore.cpp:430; Projects/Main/GameClasses/WObj_Game/WObj_GameCore.cpp:435 |
| `FUN_102840e0` | GameClasses | `CWObject_Player::GetCD` | Projects/Main/GameClasses/WObj_CharClientData.cpp:861; Projects/Main/GameClasses/WObj_CharClientData.cpp:862; Projects/Main/GameClasses/WObj_Player.cpp:102; Projects/Main/GameClasses/WObj_Player.cpp:113 |
| `FUN_1042c820` | GameClasses | `CXR_Anim_SequenceTracks::GetFrame` | Shared/MOS/XR/XRAnim.cpp:3057; Shared/MOS/XR/XRAnim.cpp:3065 |
| `FUN_1014ac80` | MXR | `CXR_Anim_SequenceTracks::GetFrame` | Shared/MOS/XR/XRAnim.cpp:3057; Shared/MOS/XR/XRAnim.cpp:3065 |
| `FUN_100706d0` | MXR | `CXR_FogState::SetDepthFog` | Shared/MOS/XR/XRFog.cpp:523; Shared/MOS/XR/XRFog.cpp:559 |
| `FUN_10070820` | MXR | `CXR_FogState::SetDepthFogBlack` | Shared/MOS/XR/XRFog.cpp:547; Shared/MOS/XR/XRFog.cpp:593 |
| `FUN_10047060` | MXR | `CXR_VBManager::AddVB` | Shared/MOS/XR/XRVBManager.cpp:1307; Shared/MOS/XR/XRVBManager.cpp:1566 |
| `FUN_10047370` | MXR | `CXR_VBManager::AddVB` | Shared/MOS/XR/XRVBManager.cpp:1307; Shared/MOS/XR/XRVBManager.cpp:1566 |
| `FUN_10065350` | MXR | `CXW_Surface::Read` | Shared/MOS/XR/XRSurf.cpp:2043; Shared/MOS/XR/XRSurf.cpp:3390; Shared/MOS/XR/XRSurf.cpp:3393; Shared/MOS/XR/XRSurf.cpp:3441; Shared/MOS/XR/XRSurf.cpp:3444; Shared/MOS/XR/XRSurfaceContext.cpp:300; Shared/MOS/XR/XRSurfaceContext.cpp:339; Shared/MOS/XR/XRSurfaceContext.cpp:371; Shared/MOS/XRModels/Model_BSP/WBSPLoader.cpp:2045; Shared/MOS/XRModels/Model_BSP2/WBSP2Loader.cpp:2434; Shared/MOS/XRModels/Model_BSP3/WBSP3Loader.cpp:1863; Shared/MOS/XRModels/Model_BSP4/WBSP4Loader.cpp:235; Shared/MOS/XRModels/Model_TriMesh/XMDCommn.cpp:4947 |
| `FUN_10024b50` | GameClasses | `MRTC_ObjectManager::CreateObject` | Shared/MCC/Mrtc.cpp:2300; Shared/MCC/Mrtc.cpp:2306 |
| `FUN_10024da0` | GameClasses | `MRTC_ObjectManager::CreateObject` | Shared/MCC/Mrtc.cpp:2300; Shared/MCC/Mrtc.cpp:2306 |
| `FUN_10024b50` | GameWorld | `MRTC_ObjectManager::CreateObject` | Shared/MCC/Mrtc.cpp:2300; Shared/MCC/Mrtc.cpp:2306 |
| `FUN_10024da0` | GameWorld | `MRTC_ObjectManager::CreateObject` | Shared/MCC/Mrtc.cpp:2300; Shared/MCC/Mrtc.cpp:2306 |
| `FUN_10030e00` | MCCDyn | `MRTC_ObjectManager::CreateObject` | Shared/MCC/Mrtc.cpp:2300; Shared/MCC/Mrtc.cpp:2306 |
| `FUN_10031050` | MCCDyn | `MRTC_ObjectManager::CreateObject` | Shared/MCC/Mrtc.cpp:2300; Shared/MCC/Mrtc.cpp:2306 |
| `FUN_1003b820` | MSystem | `MRTC_ObjectManager::CreateObject` | Shared/MCC/Mrtc.cpp:2300; Shared/MCC/Mrtc.cpp:2306 |
| `FUN_1003ba70` | MSystem | `MRTC_ObjectManager::CreateObject` | Shared/MCC/Mrtc.cpp:2300; Shared/MCC/Mrtc.cpp:2306 |
| `FUN_10024b50` | MXR | `MRTC_ObjectManager::CreateObject` | Shared/MCC/Mrtc.cpp:2300; Shared/MCC/Mrtc.cpp:2306 |
| `FUN_10024da0` | MXR | `MRTC_ObjectManager::CreateObject` | Shared/MCC/Mrtc.cpp:2300; Shared/MCC/Mrtc.cpp:2306 |
| `FUN_10024b90` | RndrGL | `MRTC_ObjectManager::CreateObject` | Shared/MCC/Mrtc.cpp:2300; Shared/MCC/Mrtc.cpp:2306 |
| `FUN_10024de0` | RndrGL | `MRTC_ObjectManager::CreateObject` | Shared/MCC/Mrtc.cpp:2300; Shared/MCC/Mrtc.cpp:2306 |
| `FUN_100228f0` | GameClasses | `MRTC_SystemInfo::OS_FileSetFileSize` | Shared/MCC/MFile_Dolphin.cpp:531; Shared/MCC/MFile_StreamMgr.cpp:1677; Shared/MCC/MFile_StreamMgr.cpp:865; Shared/MCC/MRTC_System_Linux.cpp:872; Shared/MCC/MRTC_System_PS3.cpp:2425; Shared/MCC/MRTC_System_Win32.cpp:4916 |
| `FUN_100228f0` | GameWorld | `MRTC_SystemInfo::OS_FileSetFileSize` | Shared/MCC/MFile_Dolphin.cpp:531; Shared/MCC/MFile_StreamMgr.cpp:1677; Shared/MCC/MFile_StreamMgr.cpp:865; Shared/MCC/MRTC_System_Linux.cpp:872; Shared/MCC/MRTC_System_PS3.cpp:2425; Shared/MCC/MRTC_System_Win32.cpp:4916 |
| `FUN_100286e0` | MCCDyn | `MRTC_SystemInfo::OS_FileSetFileSize` | Shared/MCC/MFile_Dolphin.cpp:531; Shared/MCC/MFile_StreamMgr.cpp:1677; Shared/MCC/MFile_StreamMgr.cpp:865; Shared/MCC/MRTC_System_Linux.cpp:872; Shared/MCC/MRTC_System_PS3.cpp:2425; Shared/MCC/MRTC_System_Win32.cpp:4916 |
| `FUN_1001d680` | MSystem | `MRTC_SystemInfo::OS_FileSetFileSize` | Shared/MCC/MFile_Dolphin.cpp:531; Shared/MCC/MFile_StreamMgr.cpp:1677; Shared/MCC/MFile_StreamMgr.cpp:865; Shared/MCC/MRTC_System_Linux.cpp:872; Shared/MCC/MRTC_System_PS3.cpp:2425; Shared/MCC/MRTC_System_Win32.cpp:4916 |
| `FUN_100228f0` | MXR | `MRTC_SystemInfo::OS_FileSetFileSize` | Shared/MCC/MFile_Dolphin.cpp:531; Shared/MCC/MFile_StreamMgr.cpp:1677; Shared/MCC/MFile_StreamMgr.cpp:865; Shared/MCC/MRTC_System_Linux.cpp:872; Shared/MCC/MRTC_System_PS3.cpp:2425; Shared/MCC/MRTC_System_Win32.cpp:4916 |
| `FUN_10022930` | RndrGL | `MRTC_SystemInfo::OS_FileSetFileSize` | Shared/MCC/MFile_Dolphin.cpp:531; Shared/MCC/MFile_StreamMgr.cpp:1677; Shared/MCC/MFile_StreamMgr.cpp:865; Shared/MCC/MRTC_System_Linux.cpp:872; Shared/MCC/MRTC_System_PS3.cpp:2425; Shared/MCC/MRTC_System_Win32.cpp:4916 |
| `FUN_10022630` | GameClasses | `MRTC_SystemInfo::OS_FileSize` | Shared/MCC/MFile_DiskUtil.cpp:671; Shared/MCC/MFile_Dolphin.cpp:477; Shared/MCC/MFile_StreamMgr.cpp:698; Shared/MCC/MRTC_System_Linux.cpp:850; Shared/MCC/MRTC_System_PS3.cpp:2443; Shared/MCC/MRTC_System_Win32.cpp:4607 |
| `FUN_10022720` | GameClasses | `MRTC_SystemInfo::OS_FileSize` | Shared/MCC/MFile_DiskUtil.cpp:671; Shared/MCC/MFile_Dolphin.cpp:477; Shared/MCC/MFile_StreamMgr.cpp:698; Shared/MCC/MRTC_System_Linux.cpp:850; Shared/MCC/MRTC_System_PS3.cpp:2443; Shared/MCC/MRTC_System_Win32.cpp:4607 |
| `FUN_10022630` | GameWorld | `MRTC_SystemInfo::OS_FileSize` | Shared/MCC/MFile_DiskUtil.cpp:671; Shared/MCC/MFile_Dolphin.cpp:477; Shared/MCC/MFile_StreamMgr.cpp:698; Shared/MCC/MRTC_System_Linux.cpp:850; Shared/MCC/MRTC_System_PS3.cpp:2443; Shared/MCC/MRTC_System_Win32.cpp:4607 |
| `FUN_10022720` | GameWorld | `MRTC_SystemInfo::OS_FileSize` | Shared/MCC/MFile_DiskUtil.cpp:671; Shared/MCC/MFile_Dolphin.cpp:477; Shared/MCC/MFile_StreamMgr.cpp:698; Shared/MCC/MRTC_System_Linux.cpp:850; Shared/MCC/MRTC_System_PS3.cpp:2443; Shared/MCC/MRTC_System_Win32.cpp:4607 |
| `FUN_10028420` | MCCDyn | `MRTC_SystemInfo::OS_FileSize` | Shared/MCC/MFile_DiskUtil.cpp:671; Shared/MCC/MFile_Dolphin.cpp:477; Shared/MCC/MFile_StreamMgr.cpp:698; Shared/MCC/MRTC_System_Linux.cpp:850; Shared/MCC/MRTC_System_PS3.cpp:2443; Shared/MCC/MRTC_System_Win32.cpp:4607 |
| `FUN_10028510` | MCCDyn | `MRTC_SystemInfo::OS_FileSize` | Shared/MCC/MFile_DiskUtil.cpp:671; Shared/MCC/MFile_Dolphin.cpp:477; Shared/MCC/MFile_StreamMgr.cpp:698; Shared/MCC/MRTC_System_Linux.cpp:850; Shared/MCC/MRTC_System_PS3.cpp:2443; Shared/MCC/MRTC_System_Win32.cpp:4607 |
| `FUN_1001d3c0` | MSystem | `MRTC_SystemInfo::OS_FileSize` | Shared/MCC/MFile_DiskUtil.cpp:671; Shared/MCC/MFile_Dolphin.cpp:477; Shared/MCC/MFile_StreamMgr.cpp:698; Shared/MCC/MRTC_System_Linux.cpp:850; Shared/MCC/MRTC_System_PS3.cpp:2443; Shared/MCC/MRTC_System_Win32.cpp:4607 |
| `FUN_1001d4b0` | MSystem | `MRTC_SystemInfo::OS_FileSize` | Shared/MCC/MFile_DiskUtil.cpp:671; Shared/MCC/MFile_Dolphin.cpp:477; Shared/MCC/MFile_StreamMgr.cpp:698; Shared/MCC/MRTC_System_Linux.cpp:850; Shared/MCC/MRTC_System_PS3.cpp:2443; Shared/MCC/MRTC_System_Win32.cpp:4607 |
| `FUN_10022630` | MXR | `MRTC_SystemInfo::OS_FileSize` | Shared/MCC/MFile_DiskUtil.cpp:671; Shared/MCC/MFile_Dolphin.cpp:477; Shared/MCC/MFile_StreamMgr.cpp:698; Shared/MCC/MRTC_System_Linux.cpp:850; Shared/MCC/MRTC_System_PS3.cpp:2443; Shared/MCC/MRTC_System_Win32.cpp:4607 |
| `FUN_10022720` | MXR | `MRTC_SystemInfo::OS_FileSize` | Shared/MCC/MFile_DiskUtil.cpp:671; Shared/MCC/MFile_Dolphin.cpp:477; Shared/MCC/MFile_StreamMgr.cpp:698; Shared/MCC/MRTC_System_Linux.cpp:850; Shared/MCC/MRTC_System_PS3.cpp:2443; Shared/MCC/MRTC_System_Win32.cpp:4607 |
| `FUN_10022670` | RndrGL | `MRTC_SystemInfo::OS_FileSize` | Shared/MCC/MFile_DiskUtil.cpp:671; Shared/MCC/MFile_Dolphin.cpp:477; Shared/MCC/MFile_StreamMgr.cpp:698; Shared/MCC/MRTC_System_Linux.cpp:850; Shared/MCC/MRTC_System_PS3.cpp:2443; Shared/MCC/MRTC_System_Win32.cpp:4607 |
| `FUN_10022760` | RndrGL | `MRTC_SystemInfo::OS_FileSize` | Shared/MCC/MFile_DiskUtil.cpp:671; Shared/MCC/MFile_Dolphin.cpp:477; Shared/MCC/MFile_StreamMgr.cpp:698; Shared/MCC/MRTC_System_Linux.cpp:850; Shared/MCC/MRTC_System_PS3.cpp:2443; Shared/MCC/MRTC_System_Win32.cpp:4607 |
| `FUN_10022d90` | GameClasses | `MRTC_SystemInfo::OS_MemSize` | Shared/MCC/MRTC_System_Linux.cpp:197; Shared/MCC/MRTC_System_PS3.cpp:1296; Shared/MCC/MRTC_System_Win32.cpp:6029 |
| `FUN_10022d90` | GameWorld | `MRTC_SystemInfo::OS_MemSize` | Shared/MCC/MRTC_System_Linux.cpp:197; Shared/MCC/MRTC_System_PS3.cpp:1296; Shared/MCC/MRTC_System_Win32.cpp:6029 |
| `FUN_10028b80` | MCCDyn | `MRTC_SystemInfo::OS_MemSize` | Shared/MCC/MRTC_System_Linux.cpp:197; Shared/MCC/MRTC_System_PS3.cpp:1296; Shared/MCC/MRTC_System_Win32.cpp:6029 |
| `FUN_1001dad0` | MSystem | `MRTC_SystemInfo::OS_MemSize` | Shared/MCC/MRTC_System_Linux.cpp:197; Shared/MCC/MRTC_System_PS3.cpp:1296; Shared/MCC/MRTC_System_Win32.cpp:6029 |
| `FUN_10022d90` | MXR | `MRTC_SystemInfo::OS_MemSize` | Shared/MCC/MRTC_System_Linux.cpp:197; Shared/MCC/MRTC_System_PS3.cpp:1296; Shared/MCC/MRTC_System_Win32.cpp:6029 |
| `FUN_10022dd0` | RndrGL | `MRTC_SystemInfo::OS_MemSize` | Shared/MCC/MRTC_System_Linux.cpp:197; Shared/MCC/MRTC_System_PS3.cpp:1296; Shared/MCC/MRTC_System_Win32.cpp:6029 |
| `FUN_1000ab40` | GameClasses | `MRTC_SystemInfo::Thread_LocalAlloc` | Shared/MCC/MRTC_System_Linux.cpp:412; Shared/MCC/MRTC_System_PS3.cpp:1656; Shared/MCC/MRTC_System_Win32.cpp:5437; Shared/MCC/Mrtc.cpp:2168 |
| `FUN_1000ab40` | GameWorld | `MRTC_SystemInfo::Thread_LocalAlloc` | Shared/MCC/MRTC_System_Linux.cpp:412; Shared/MCC/MRTC_System_PS3.cpp:1656; Shared/MCC/MRTC_System_Win32.cpp:5437; Shared/MCC/Mrtc.cpp:2168 |
| `FUN_1000cfb0` | MCCDyn | `MRTC_SystemInfo::Thread_LocalAlloc` | Shared/MCC/MRTC_System_Linux.cpp:412; Shared/MCC/MRTC_System_PS3.cpp:1656; Shared/MCC/MRTC_System_Win32.cpp:5437; Shared/MCC/Mrtc.cpp:2168 |
| `FUN_1001da40` | MSystem | `MRTC_SystemInfo::Thread_LocalAlloc` | Shared/MCC/MRTC_System_Linux.cpp:412; Shared/MCC/MRTC_System_PS3.cpp:1656; Shared/MCC/MRTC_System_Win32.cpp:5437; Shared/MCC/Mrtc.cpp:2168 |
| `FUN_1000ab40` | MXR | `MRTC_SystemInfo::Thread_LocalAlloc` | Shared/MCC/MRTC_System_Linux.cpp:412; Shared/MCC/MRTC_System_PS3.cpp:1656; Shared/MCC/MRTC_System_Win32.cpp:5437; Shared/MCC/Mrtc.cpp:2168 |
| `FUN_1000ab80` | RndrGL | `MRTC_SystemInfo::Thread_LocalAlloc` | Shared/MCC/MRTC_System_Linux.cpp:412; Shared/MCC/MRTC_System_PS3.cpp:1656; Shared/MCC/MRTC_System_Win32.cpp:5437; Shared/MCC/Mrtc.cpp:2168 |
| `FUN_10147210` | MSystem | `TSVQ::TSVQ` | Shared/MOS/MSystem/Raster/MImageCompressVQ.cpp:138; Shared/MOS/MSystem/Raster/MImageCompressVQ.cpp:149; Shared/MOS/MSystem/Raster/MImageCompressVQ.cpp:157 |


*(классметод: 84 однозначных пар, 103 неоднозначных)*


### Автосопоставление по якорю "имя класса" (MRTC_IMPLEMENT_*)

Литерал — первый аргумент `MRTC_IMPLEMENT_DYNAMIC`/`MRTC_IMPLEMENT_SERIAL_WOBJECT`/аналогов. Совпадение имени класса в декомпиле — как правило, это конструктор/RTTI-thunk этого класса, не факт что FUN == сам конструктор один в один (не сверено вручную).

| Литерал | Декомпил:FUN | Файл:строка исходника |
|---|---|---|
| `CTextureContainer_Video_Theora` | GameWorld:`FUN_10097bf0` | Shared/MOS/MSystem/Raster/MTextureContainerTheora.cpp:1706 |
| `CWObject_GameCore` | GameClasses:`FUN_10247da0` | Projects/Main/GameClasses/WObj_Game/WObj_GameCore.cpp:18 |
| `CWRes_Anim` | GameWorld:`FUN_10097bf0` | Shared/MOS/Classes/GameWorld/WDataRes_Anim.cpp:7 |
| `CWRes_AnimGraph2` | GameWorld:`FUN_10097bf0` | Shared/MOS/Classes/GameWorld/WDataRes_AnimGraph2.cpp:96 |
| `CWRes_Class` | GameWorld:`FUN_10097bf0` | Shared/MOS/Classes/GameWorld/WDataRes_Core.cpp:12 |
| `CWRes_DLL` | GameWorld:`FUN_10097bf0` | Shared/MOS/Classes/GameWorld/WDataRes_Core.cpp:8 |
| `CWRes_Dialogue` | GameWorld:`FUN_10097bf0` | Shared/MOS/Classes/GameWorld/WDataRes_Sound.cpp:15 |
| `CWRes_FacialSetup` | GameWorld:`FUN_10097bf0` | Shared/MOS/Classes/GameWorld/WDataRes_FacialSetup.cpp:18 |
| `CWRes_Model_Custom` | GameWorld:`FUN_10097bf0` | Shared/MOS/Classes/GameWorld/WDataRes_Models.cpp:13 |
| `CWRes_Model_Custom_File` | GameWorld:`FUN_10097bf0` | Shared/MOS/Classes/GameWorld/WDataRes_Models.cpp:14 |
| `CWRes_Model_Glass` | GameWorld:`FUN_10097bf0` | Shared/MOS/Classes/GameWorld/WDataRes_XW.cpp:20 |
| `CWRes_Model_XMD` | GameWorld:`FUN_10097bf0` | Shared/MOS/Classes/GameWorld/WDataRes_Models.cpp:12 |
| `CWRes_Model_XW` | GameWorld:`FUN_10097bf0` | Shared/MOS/Classes/GameWorld/WDataRes_XW.cpp:16 |
| `CWRes_Model_XW2` | GameWorld:`FUN_10097bf0` | Shared/MOS/Classes/GameWorld/WDataRes_XW.cpp:17 |
| `CWRes_Model_XW3` | GameWorld:`FUN_10097bf0` | Shared/MOS/Classes/GameWorld/WDataRes_XW.cpp:18 |
| `CWRes_Model_XW4` | GameWorld:`FUN_10097bf0` | Shared/MOS/Classes/GameWorld/WDataRes_XW.cpp:19 |
| `CWRes_ObjectAttribs` | GameWorld:`FUN_10097bf0` | Shared/MOS/Classes/GameWorld/WDataRes_XW.cpp:25 |
| `CWRes_Registry` | GameWorld:`FUN_10097bf0` | Shared/MOS/Classes/GameWorld/WDataRes_Core.cpp:9 |
| `CWRes_Sound` | GameWorld:`FUN_10097bf0` | Shared/MOS/Classes/GameWorld/WDataRes_Sound.cpp:13 |
| `CWRes_Surface` | GameWorld:`FUN_10097bf0` | Shared/MOS/Classes/GameWorld/WDataRes_MiscMedia.cpp:6 |
| `CWRes_Template` | GameWorld:`FUN_10097bf0` | Shared/MOS/Classes/GameWorld/WDataRes_Core.cpp:13 |
| `CWRes_Wave` | GameWorld:`FUN_10097bf0` | Shared/MOS/Classes/GameWorld/WDataRes_Sound.cpp:14 |
| `CWRes_XFC` | GameWorld:`FUN_10097bf0` | Shared/MOS/Classes/GameWorld/WDataRes_MiscMedia.cpp:7 |
| `CWRes_XWIndex` | GameWorld:`FUN_10097bf0` | Shared/MOS/Classes/GameWorld/WDataRes_XW.cpp:15 |
| `CWRes_XWNavGraph` | GameWorld:`FUN_10097bf0` | Shared/MOS/Classes/GameWorld/WDataRes_XW.cpp:23 |
| `CWRes_XWNavGrid` | GameWorld:`FUN_10097bf0` | Shared/MOS/Classes/GameWorld/WDataRes_XW.cpp:22 |
| `CWRes_XWResource` | GameWorld:`FUN_10097bf0` | Shared/MOS/Classes/GameWorld/WDataRes_XW.cpp:21 |
| `CWorld_NavGraph_Path` | GameClasses:`FUN_100f3b30`, GameClasses:`FUN_100f52d0`, GameClasses:`FUN_100f7160` | Shared/MOS/Classes/GameWorld/WNavGraph/WNavGraph_PathFinder.cpp:964 |
| `CXBN_SearchResult` | GameClasses:`FUN_100f3b30`, GameClasses:`FUN_100f52d0`, GameClasses:`FUN_100f7160` | Shared/MOS/XR/XRBlockNavResult.inl:212 |
| `CXR_Anim_Base` | GameWorld:`FUN_10088c60` | Shared/MOS/XR/XRAnim.cpp:15 |
| `CXR_Model_BSP` | GameWorld:`FUN_1006e700` | Shared/MOS/XRModels/Model_BSP/WBSPModel.cpp:174 |
| `CXR_Model_BSP2` | GameWorld:`FUN_100664e0`, GameWorld:`FUN_1006e7a0` | Shared/MOS/XRModels/Model_BSP2/WBSP2Model.cpp:275 |
| `CXR_Model_BSP3` | GameWorld:`FUN_1006e810` | Shared/MOS/XRModels/Model_BSP3/WBSP3Model.cpp:177 |
| `CXR_Model_BSP4` | GameWorld:`FUN_1006e870` | Shared/MOS/XRModels/Model_BSP4/WBSP4Model.cpp:20 |
| `CXR_Model_BSP4Glass` | GameWorld:`FUN_1006e8b0` | Shared/MOS/XRModels/Model_BSP4Glass/WBSP4Glass.cpp:16 |
| `CXR_Model_TriangleMesh` | GameClasses:`FUN_10164590` | Shared/MOS/XRModels/Model_TriMesh/WTriMesh.cpp:102 |
| `CXR_VBManager` | MXR:`FUN_1003ff90` | Shared/MOS/XR/XRVBManager.cpp:272 |
| `MRTC_TaskCompressTexture` | MSystem:`FUN_100ecb30` | Shared/MOS/MSystem/Raster/MTextureContainers.cpp:4618 |
| `MRTC_TaskCompressTextureContainer` | MSystem:`FUN_100e9618` | Shared/MOS/MSystem/Raster/MTextureContainers.cpp:4620 |
| `MRTC_TaskConvertTexture` | MSystem:`FUN_100e4a00` | Shared/MOS/MSystem/Raster/MTextureContainers.cpp:4619 |
| `MRTC_TaskConvertTextureContainer` | MSystem:`FUN_100e9618` | Shared/MOS/MSystem/Raster/MTextureContainers.cpp:4621 |
| `MRTC_TaskGaussFilterCubemap` | MSystem:`FUN_100e9618` | Shared/MOS/MSystem/Raster/MTextureContainers.cpp:4622 |
| `MRTC_TaskHostCompressTexture` | MSystem:`FUN_100e47e0` | Shared/MOS/MSystem/Raster/MTextureContainers.cpp:4626 |
| `MRTC_TaskSmoothCubemapEdges` | MSystem:`FUN_100e9618` | Shared/MOS/MSystem/Raster/MTextureContainers.cpp:4623 |
| `MRTC_TaskTexturePostFilter` | MSystem:`FUN_100e9618` | Shared/MOS/MSystem/Raster/MTextureContainers.cpp:4625 |
| `MRTC_TaskTexturePow2_Process` | MSystem:`FUN_100e8430`, MSystem:`FUN_100e86f3` | Shared/MOS/MSystem/Raster/MTextureContainers.cpp:3080 |


*(classname: 46 пар)*


### Автосопоставление по якорю "консольная команда" (RegFunction)

Литерал — имя консольной команды из `RegFunction("...")`. Многие совпадения — `fun=?` (строка вне обнаруженной функции — похоже, статическая таблица команд в `.rdata`, а не код), такие строки в таблицу ниже не попали (нет привязки к FUN).

| Литерал | Декомпил:FUN | Файл:строка исходника |
|---|---|---|
| `cachecommand` | GameClasses:`FUN_10458640` | Projects/Main/GameWorld/WFrontEndMod.cpp:1999 |
| `cg_backout` | GameClasses:`FUN_10458640` | Projects/Main/GameWorld/WFrontEndMod.cpp:1996 |
| `cg_blackloading` | GameClasses:`FUN_10458640` | Projects/Main/GameWorld/WFrontEndMod.cpp:1997 |
| `cg_clearmenus` | GameClasses:`FUN_10466980` | Shared/MOS/Classes/GameWorld/FrontEnd/WFrontEnd.cpp:575 |
| `cg_cuberesetwindow` | GameClasses:`FUN_10458640` | Projects/Main/GameWorld/WFrontEndMod.cpp:2002 |
| `cg_cubeseq` | GameClasses:`FUN_10458640` | Projects/Main/GameWorld/WFrontEndMod.cpp:2005 |
| `cg_cubeseqforce` | GameClasses:`FUN_10458640` | Projects/Main/GameWorld/WFrontEndMod.cpp:2006 |
| `cg_cubeside` | GameClasses:`FUN_10458640` | Projects/Main/GameWorld/WFrontEndMod.cpp:2004 |
| `cg_dowindowswitch` | GameClasses:`FUN_10466980` | Shared/MOS/Classes/GameWorld/FrontEnd/WFrontEnd.cpp:581 |
| `cg_dumpwnd` | GameClasses:`FUN_10466980` | Shared/MOS/Classes/GameWorld/FrontEnd/WFrontEnd.cpp:585 |
| `cg_grabscreen` | GameClasses:`FUN_10458640` | Projects/Main/GameWorld/WFrontEndMod.cpp:2000 |
| `cg_loadlastvalidprofile` | GameClasses:`FUN_10466980` | Shared/MOS/Classes/GameWorld/FrontEnd/WFrontEnd.cpp:590 |
| `cg_menu` | GameClasses:`FUN_10466980` | Shared/MOS/Classes/GameWorld/FrontEnd/WFrontEnd.cpp:584 |
| `cg_menuimpulse` | GameClasses:`FUN_10466980` | Shared/MOS/Classes/GameWorld/FrontEnd/WFrontEnd.cpp:580 |
| `cg_newgame` | GameClasses:`FUN_10466980` | Shared/MOS/Classes/GameWorld/FrontEnd/WFrontEnd.cpp:583 |
| `cg_playrandommusic` | GameClasses:`FUN_10458640` | Projects/Main/GameWorld/WFrontEndMod.cpp:2010 |
| `cg_playsound` | GameClasses:`FUN_10466980` | Shared/MOS/Classes/GameWorld/FrontEnd/WFrontEnd.cpp:587 |
| `cg_playsound_sfxvol` | GameClasses:`FUN_10466980` | Shared/MOS/Classes/GameWorld/FrontEnd/WFrontEnd.cpp:588 |
| `cg_prevmenu` | GameClasses:`FUN_10466980` | Shared/MOS/Classes/GameWorld/FrontEnd/WFrontEnd.cpp:576 |
| `cg_prevmenu2` | GameClasses:`FUN_10466980` | Shared/MOS/Classes/GameWorld/FrontEnd/WFrontEnd.cpp:577 |
| `cg_reloaduiregistry` | GameClasses:`FUN_10458640` | Projects/Main/GameWorld/WFrontEndMod.cpp:2008 |
| `cg_rootmenu` | GameClasses:`FUN_10466980` | Shared/MOS/Classes/GameWorld/FrontEnd/WFrontEnd.cpp:578 |
| `cg_rootmenu_ingame` | GameClasses:`FUN_10466980` | Shared/MOS/Classes/GameWorld/FrontEnd/WFrontEnd.cpp:579 |
| `cg_savefileremovenewrootmenu` | GameClasses:`FUN_10458640` | Projects/Main/Exe/XRApp.cpp:7197 |
| `cg_submenu` | GameClasses:`FUN_10466980` | Shared/MOS/Classes/GameWorld/FrontEnd/WFrontEnd.cpp:574 |
| `cg_switchmenu` | GameClasses:`FUN_10466980` | Shared/MOS/Classes/GameWorld/FrontEnd/WFrontEnd.cpp:573 |
| `docachedcommand` | GameClasses:`FUN_10458640` | Projects/Main/GameWorld/WFrontEndMod.cpp:1998 |
| `doprecache` | GameClasses:`FUN_10458640` | Projects/Main/Exe/XRApp.cpp:7210 |
| `floor` | MSystem:`FUN_10137040` | Shared/MOS/MSystem/Script/MScript_Language.cpp:150 |
| `log10` | MSystem:`FUN_10137040` | Shared/MOS/MSystem/Script/MScript_Language.cpp:149 |
| `r_anisotropy` | RndrGL:`FUN_1005c9f0` | Projects/Main/Exe/XRApp.cpp:7175; Shared/MOS/RenderContexts/PS3GCM/MRenderPS3_Cmd.cpp:93 |
| `r_antialias` | RndrGL:`FUN_1005c9f0` | Projects/Main/Exe/XRApp.cpp:7177; Shared/MOS/RenderContexts/PS3GCM/MRenderPS3_Cmd.cpp:91 |
| `r_backbufferformat` | RndrGL:`FUN_1005c9f0` | Projects/Main/Exe/XRApp.cpp:7178; Shared/MOS/RenderContexts/PS3GCM/MRenderPS3_Cmd.cpp:92 |
| `r_picmip` | MXR:`FUN_100b8bf0` | Projects/Main/Exe/XRApp.cpp:7174; Shared/MOS/RenderContexts/PS3GCM/MRenderPS3_Cmd.cpp:96 |
| `r_vsync` | RndrGL:`FUN_1005c9f0` | Projects/Main/Exe/XRApp.cpp:7176; Shared/MOS/RenderContexts/PS3GCM/MRenderPS3_Cmd.cpp:94 |
| `rs_asynccacheblock` | GameWorld:`FUN_1009eb00` | Shared/MOS/Classes/GameWorld/WDataCore.cpp:3061 |
| `rs_classes` | GameWorld:`FUN_1009eb00` | Shared/MOS/Classes/GameWorld/WDataCore.cpp:3057 |
| `rs_resources` | GameWorld:`FUN_1009eb00` | Shared/MOS/Classes/GameWorld/WDataCore.cpp:3058 |
| `rs_resourcesbyclass` | GameWorld:`FUN_1009eb00` | Shared/MOS/Classes/GameWorld/WDataCore.cpp:3059 |
| `rs_resourcessorted` | GameWorld:`FUN_1009eb00` | Shared/MOS/Classes/GameWorld/WDataCore.cpp:3060 |
| `rs_updateanims` | GameWorld:`FUN_1009eb00` | Shared/MOS/Classes/GameWorld/WDataCore.cpp:3054 |
| `rs_updatemodels` | GameWorld:`FUN_1009eb00` | Shared/MOS/Classes/GameWorld/WDataCore.cpp:3055 |
| `rs_updateregistry` | GameWorld:`FUN_1009eb00` | Shared/MOS/Classes/GameWorld/WDataCore.cpp:3056 |
| `rs_updatesurfaces` | GameWorld:`FUN_1009eb00` | Shared/MOS/Classes/GameWorld/WDataCore.cpp:3053 |
| `srand` | MSystem:`FUN_10137040` | Shared/MOS/MSystem/Script/MScript_Language.cpp:153 |
| `sv_dialogueinfo` | GameWorld:`FUN_100abb50` | Projects/Main/GameWorld/WServerMod.cpp:428 |
| `sv_executetestrun` | GameWorld:`FUN_100abb50` | Projects/Main/GameWorld/WServerMod.cpp:429 |
| `vp_aspectratio` | GameClasses:`FUN_1044bca0` | Shared/MOS/Classes/Render/MRenderUtil.cpp:125 |
| `vp_backplane` | GameClasses:`FUN_1044bca0` | Shared/MOS/Classes/Render/MRenderUtil.cpp:127 |
| `vp_fov` | GameClasses:`FUN_1044bca0` | Shared/MOS/Classes/Render/MRenderUtil.cpp:123 |
| `vp_frontplane` | GameClasses:`FUN_1044bca0` | Shared/MOS/Classes/Render/MRenderUtil.cpp:126 |
| `vp_zoom` | GameClasses:`FUN_1044bca0` | Shared/MOS/Classes/Render/MRenderUtil.cpp:124 |
| `vwinsize` | RndrGL:`FUN_10046930` | Projects/Main/Exe/XRApp.cpp:7179 |
| `xr_bspdebugflags` | MXR:`FUN_100b8bf0` | Shared/MOS/XR/XREngine.cpp:6041 |
| `xr_bsptoggledebugflags` | MXR:`FUN_100b8bf0` | Shared/MOS/XR/XREngine.cpp:6042 |
| `xr_debugflags` | MXR:`FUN_100b8bf0` | Shared/MOS/XR/XREngine.cpp:5985 |
| `xr_flares` | MXR:`FUN_100b8bf0` | Shared/MOS/XR/XREngine.cpp:5987 |
| `xr_fogculloffset` | MXR:`FUN_100b8bf0` | Shared/MOS/XR/XREngine.cpp:6019 |
| `xr_lightdebugflags` | MXR:`FUN_100b8bf0` | Shared/MOS/XR/XREngine.cpp:5986 |
| `xr_lodoffset` | MXR:`FUN_100b8bf0` | Shared/MOS/XR/XREngine.cpp:5992 |
| `xr_lodscale` | MXR:`FUN_100b8bf0` | Shared/MOS/XR/XREngine.cpp:5993 |
| `xr_modeoverride` | MXR:`FUN_100b8bf0` | Shared/MOS/XR/XREngine.cpp:6020 |
| `xr_objectsonly` | MXR:`FUN_100b8bf0` | Shared/MOS/XR/XREngine.cpp:5996 |
| `xr_portalsonly` | MXR:`FUN_100b8bf0` | Shared/MOS/XR/XREngine.cpp:5997 |
| `xr_ppdebugflags` | MXR:`FUN_100b8bf0` | Shared/MOS/XR/XREngine.cpp:6023 |
| `xr_ppexposureblacklevel` | MXR:`FUN_100b8bf0` | Shared/MOS/XR/XREngine.cpp:6033 |
| `xr_ppexposurecontrast` | MXR:`FUN_100b8bf0` | Shared/MOS/XR/XREngine.cpp:6031 |
| `xr_ppexposuredebug` | MXR:`FUN_100b8bf0` | Shared/MOS/XR/XREngine.cpp:6036 |
| `xr_ppexposureexp` | MXR:`FUN_100b8bf0` | Shared/MOS/XR/XREngine.cpp:6029 |
| `xr_ppexposuresaturation` | MXR:`FUN_100b8bf0` | Shared/MOS/XR/XREngine.cpp:6032 |
| `xr_ppexposurescale` | MXR:`FUN_100b8bf0` | Shared/MOS/XR/XREngine.cpp:6030 |
| `xr_ppglowbias` | MXR:`FUN_100b8bf0` | Shared/MOS/XR/XREngine.cpp:6025 |
| `xr_ppglowcenter` | MXR:`FUN_100b8bf0` | Shared/MOS/XR/XREngine.cpp:6028 |
| `xr_ppglowexp` | MXR:`FUN_100b8bf0` | Shared/MOS/XR/XREngine.cpp:6024 |
| `xr_ppglowgamma` | MXR:`FUN_100b8bf0` | Shared/MOS/XR/XREngine.cpp:6027 |
| `xr_ppglowscale` | MXR:`FUN_100b8bf0` | Shared/MOS/XR/XREngine.cpp:6026 |
| `xr_ppmbdebugflags` | MXR:`FUN_100b8bf0` | Shared/MOS/XR/XREngine.cpp:6039 |
| `xr_ppmbmaxradius` | MXR:`FUN_100b8bf0` | Shared/MOS/XR/XREngine.cpp:6038 |
| `xr_pptoggledynamicexposure` | MXR:`FUN_100b8bf0` | Shared/MOS/XR/XREngine.cpp:6035 |
| `xr_pptoggleexposuredebug` | MXR:`FUN_100b8bf0` | Shared/MOS/XR/XREngine.cpp:6034 |
| `xr_showbounding` | MXR:`FUN_100b8bf0` | Shared/MOS/XR/XREngine.cpp:5994 |
| `xr_showfoginfo` | MXR:`FUN_100b8bf0` | Shared/MOS/XR/XREngine.cpp:6001 |
| `xr_showportalfence` | MXR:`FUN_100b8bf0` | Shared/MOS/XR/XREngine.cpp:6003 |
| `xr_showrecurse` | MXR:`FUN_100b8bf0` | Shared/MOS/XR/XREngine.cpp:6002 |
| `xr_showtiming` | MXR:`FUN_100b8bf0` | Shared/MOS/XR/XREngine.cpp:5999 |
| `xr_showvbtime` | MXR:`FUN_100b8bf0` | Shared/MOS/XR/XREngine.cpp:6000 |
| `xr_showvbtypes` | MXR:`FUN_100b8bf0` | Shared/MOS/XR/XREngine.cpp:6014 |
| `xr_sky` | MXR:`FUN_100b8bf0` | Shared/MOS/XR/XREngine.cpp:5998 |
| `xr_surfoptions` | MXR:`FUN_100b8bf0` | Shared/MOS/XR/XREngine.cpp:6007 |
| `xr_synconrender` | MXR:`FUN_100b8bf0` | Shared/MOS/XR/XREngine.cpp:6021 |
| `xr_wallmarks` | MXR:`FUN_100b8bf0` | Shared/MOS/XR/XREngine.cpp:5990 |
| `xr_worldonly` | MXR:`FUN_100b8bf0` | Shared/MOS/XR/XREngine.cpp:5995 |
| `xr_zfog` | MXR:`FUN_100b8bf0` | Shared/MOS/XR/XREngine.cpp:6005 |


*(command: 93 пар)*


---

## Граф вызовов: выведенные пары + разбор CXR_Model_TriangleMesh (2026-08-08)

Контекст и методика — `Docs/Decomp_Coverage.md` §7. Границы функций декомпила
после фикса бага в `decomp_inventory.py` (см. там же §7.1) пересчитаны заново
— `Tools/out/decomp_funcs.tsv` вырос с 28 745 до 49 236 записей; все
FUN-адреса ниже опираются на новую (исправленную) разметку.

### Выведено по графу (этап 2, ОДНОЗНАЧНО — единственный известный вызывающий
### + единственный неопознанный вызов на стороне исходника)

| FUN | Класс::Метод | Доказательство |
|---|---|---|
| `GameClasses:FUN_105534d0` | `CWObject_SwingDoorParent::OnCreateClientUpdate` (`Projects/Main/GameClasses/WObj_Misc/WObj_SwingDoor.cpp:1062`) | единственный вызывающий в файле — опознанный `FUN_10553c70` = `CWObject_SwingDoor::OnCreateClientUpdate`, который в исходнике вызывает ровно один ещё неопознанный метод (базовую реализацию у родителя) |
| `GameClasses:FUN_1057e3f0` | `CWObject_Model_Anim_Parent::OnCreateClientUpdate` (`Projects/Main/GameClasses/WObj_Misc/WObj_Model_Anim.cpp:125`) | тот же паттерн: единственный вызывающий `FUN_1057ec70` = `CWObject_Model_Anim::OnCreateClientUpdate` |

### Найдено вручную при раскрутке от `CTriangleMeshCore::Read` (задача этапа 3)

Строковый якорь `"CTriangleMeshCore::Read"` (после фикса границ) теперь
резолвится в **4 разных FUN_** в `MXR_dll_decomp.c` (несколько версий/веток
Read, ранее часть была в "мёртвой зоне" и не резолвилась вовсе):
`FUN_10122f00`, `FUN_10184e90`, `FUN_10185d80`, `FUN_101878c0`.

`FUN_101878c0` (строки 261338-262565) — основная/полная версия: содержит
чтение `LODBIAS` (`CDataFile::GetNext(param_3,"LODBIAS")`, строка 262439),
что соответствует полю `m_lLODBias` (`WTriMesh.cpp`). Сразу за ней по
адресам располагается кластер конструктора/деструктора `CTriangleMeshCore`
(подтверждено присвоением `*param_1 = CTriangleMeshCore::vftable;`):
`FUN_10189d90` (конструктор, 262581-262629), `FUN_10189e90` (деструктор,
262633-262696).

Найден и парный **writer**: `FUN_10179c90` (строки 252629-253025) пишет ту
же секцию `"LODBIAS"` (`CDataFile::BeginEntry` + цикл по `*(int*)((int)this+8)`
— TArray-заголовок массива по смещению **+8 байт** от начала объекта). Это
даёт точное смещение LOD-массива в раскладке `CTriangleMeshCore` для
будущей сверки — само по себе не привязано к конкретному методу исходника
построчно, статус **не подтверждено окончательно** (какой именно метод —
`Write` целиком или его часть — не сверено построчно), но адрес и смещение
надёжны (видно напрямую в тексте декомпила).

Раскрутка до вызывающего `CTriangleMeshCore::Read` дала обёртку уровня
модели:

| FUN | Что это | Доказательство |
|---|---|---|
| `MXR:FUN_10139af0` | `CXR_Model_TriangleMesh::Read(CDataFile*, ...)` (`WTriMesh.cpp`, район :747, `Error_static` на "Can't find EXTENDEDMODEL"/"Corrupt model. (1)") | читает `"EXTENDEDMODEL"` (`CDataFile::GetNext(param_1,"EXTENDEDMODEL")`, единственный вызывающий кода — соответствует `WTriMesh.cpp:747: if (!_pDFile->GetNext("EXTENDEDMODEL")) Error("Read", ...)`), затем вызывает `FUN_101878c0` (=`CTriangleMeshCore::Read`) по смещению `this+0x10` |
| `MXR:FUN_101379e0` | `CXR_Model_TriangleMesh`-конструктор | присвоение `*param_1 = CXR_Model_TriangleMesh::vftable;` (RTTI-имя резолвится напрямую, строка 206146) |

### GetLOD / OnRender2-гейт / Cluster_SetMatrixPalette / CalculateLocalMatrices — по-прежнему НЕ найдены

Несмотря на точную локализацию всего кластера
`Read`/`Write`/ctor/dtor `CTriangleMeshCore` и обёртки
`CXR_Model_TriangleMesh::Read` (выше), сам `GetLOD` (виртуальный метод,
`WTriMesh.cpp:514`) НЕ найден:
- соседние по адресам мелкие функции после `Read`-обёртки
  (`FUN_10139ef0`..`FUN_1013a000` и далее) проверены вручную — это
  аллокатор/thunk-деструктор/другие сервисные методы, не `GetLOD`
  (нет ни сравнения с массивом по убывающему индексу, ни двойного
  `return`-паттерна "элемент LOD" / "this");
- `CVec3Dfp32::Length` (используется в `GetLOD` для вычисления `Dist`) —
  **не резолвится как именованный символ вообще** (0 вхождений в
  `MXR_dll_decomp.c`) — это невиртуальный inline-класс без RTTI, Ghidra
  не восстанавливает для него имя, вызов виден только как сырая
  sqrt-математика — нет якоря, чтобы искать по имени;
  для этого выполнено меньше проверок, времени в рамках заявленного
  задания не хватило и на её (VFTABLE-content) точную идентификацию —
  не выполнено.
- Виртуальные методы в общем случае НЕ гарантированно лежат по соседству
  с `Read`/`Write`/ctor в скомпилированном бинарнике (порядок в TU не
  равен порядку в vtable) — гипотеза "соседние адреса" подтвердилась для
  ctor/dtor `CTriangleMeshCore`, но не сработала для `GetLOD`.

**Статус остаётся "неизвестно"** для всех четырёх целей этапа 3 из задания,
как и в прошлом проходе — но теперь с гораздо более точной картой соседей
(таблицы выше) и с исправленным инструментом, так что следующий заход может
начать не с нуля, а с этого кластера адресов + смещения `+8` для
LOD-массива.
