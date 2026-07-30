# Отчёт: NPC двигаются быстрее, чем играет их анимация

По спеке `Docs/Research_MoveSpeed.md`. Только исследование, фиксов нет.

**Короткий итог.** Загрузчик таймингов анимации с ретейлом совпадает
бит-в-бит — главный подозреваемый спеки (Q3) **оправдан**. Перемещение
NPC при обычной ходьбе ведёт **анимация** (root-motion из move-track'а),
velocity хранится в **units/tick** и интегрируется без dt. Найдены два
живых места с зашитой константой 20 Гц: `WFixedCharMovement.cpp:102`
(velocity MoveTarget'ов — даёт ровно 1.5x при 30 Гц) и хардкод
`GetGameTickTime() = 1/20` на клиенте (`WClient_Core.cpp:523-527`).
Шаги — ключи последовательности на оси времени анимации, расстояние в
цепочке не участвует вообще.

---

## Q1. Кто задаёт скорость перемещения за тик

Цепочка: данные персонажа → конверсия в units/tick → user-acceleration →
velocity → интеграция раз в тик **без** домножения на dt.

- Скорость — из ресурса персонажа (`SPEED_FORWARD` и пр.),
  `WObj_CharCreate.cpp:1645-1679`; дефолты `WObj_CharClientData.cpp:354-358`.
- Конверсия идёт **не** через мёртвый макрос из `WPhysState.h:25-41`, а через
  живые локальные define в каждом TU:
  `#define PHYSSTATE_CONVERTFROM20HZ(x) ((x) * 20.0f * GetGameTickTime())`
  (`WObj_CharCreate.cpp:1094`, `WObj_CharPhys.cpp:1923`,
  `WObj_CharClientData.cpp:353`). При tickTime=1/30 даёт `x*20/30` за тик —
  корректно.
- tickTime — data-driven: `WPhysState.cpp:76-77` (`SERVER_REFRESHRATE`,
  дефолт 30). **НО** клиентский `CWorld_ClientCore::GetGameTickTime()`
  захардкожен `return 1.0f/20.0f` — `Client/WClient_Core.cpp:523-527`.
- Применение: `WObj_CharPhys.cpp:824` (dV из `m_Control_Move` × speed),
  доп. `* 20.0f * GetGameTickTime()` в `Phys_GetUserAccelleration`
  (`WObj_CharPhys.cpp:2491`, комментарий «Convert from 20hz»).
- Интеграция: `WServer_Phys.cpp:203,452` — `GetMatrixRow(NewMat,3) +=
  GetMoveVelocity();` (клиент: `WClient_Obj.cpp:349-350`). Т.е. вся
  корректность держится на том, что продюсеры velocity конвертируют в
  units/tick.

**Зашитая 20 Гц в живом коде:**
- `WFixedCharMovement.cpp:102` — самое горячее:
  `m_Velocity = (m_Target - Start) / (20 * (m_TargetTime - StartTime).GetTime());`
  Velocity считается в units-per-1/20s, а применяется как units/tick
  (`Object_SetVelocity`, `:146`) при 30 тиках/с → **ровно 1.5x быстрее**.
  Это путь `CMoveTarget` — очереди MoveTarget из анимграфа
  (`WAG_ClientData_Game.cpp:1792 AddMoveTarget`), AG2-driven перемещения
  (action/ledge/fixed-path, controlmode'ы `WObj_CharPhys.cpp:1366,1426,1493`).
- Мелочь: `WObj_CharCreate.cpp:2078` (`*20.0f` тики транквилизатора),
  `WModel_InstantTrail.cpp:544` (рендер, не движение).

## Q2. Кто ведёт: анимация или AI

**Гибрид; для обычной ходьбы NPC ведёт АНИМАЦИЯ (root-motion).**

- Дефолт move-device: `CAI_Device_Move::Use(..., _bAnimControl=true)`
  (`AI_DeviceHandler.cpp:303,335-351`) → `PLAYER_CONTROLMODE_ANIMATION`
  (`WObj_CharMsg.cpp:6214-6228`).
- Каждый тик `Char_ControlMode_Anim2` → `GetAnimVelocity`
  (`WObj_CharControlModeAnim.cpp:66-80`): `WAG2I.cpp:975-995` сэмплирует
  `EvalTrack0` в двух точках (t и t+TimeSpan×TimeScale), дельта = velocity
  за тик. `m_TimeSpan = GetGameTickTime()` (`WAG2I_Context.cpp:31-32`) —
  привязка к тику, не к FPS. Скорость ходьбы зашита в данных анимации.
- Тарировка контента под 20 Гц — живая: `WObj_CharClientData.cpp:1194-1196`:
  `MoveVeloVectorWorld *= GetGameTicksPerSecond() / 20.0f;` с комментарием
  «Recalculate movevelovector world to 20hz speed». Это property в анимграф
  (выбор/масштаб walk-анимаций) — при расхождении тикрейта/тарировки рвётся
  синхрон «скорость объекта ↔ скорость проигрывания».
- Perfect movement (`m_bHasPerfectMovement`, `WAG2I_StateInst.cpp:846`) —
  третий режим: velocity = доводка до destination
  (`WAG2I.cpp:943-956`, ветка `iPerfectState != -1`).
- Обратная ветвь (animcontrol OFF): AI ведёт через `WObj_CharPhys.cpp:806-824`,
  анимграф лишь следует: `m_SyncAnimScale` (`WObj_CharClientData.cpp:1183`).
- `m_MoveScale` — статический global-scale модели (`XRSkeleton.cpp:643,2292-2294`,
  из `CXR_MODEL_PARAM_GLOBALSCALE`, `WObj_CharAnim.cpp:1241-1244`); к
  синхронизации скорости отношения не имеет, `GetAnimVelocity` его минует.

## Q3. Длительность последовательности (сверка с ретейлом)

**Укладка совпадает с ретейл-бинарём полностью. Подозрение снято.**

- PC-контент — `COMPRESSEDANIMATIONSET2`; тайминги читаются как готовый
  массив fp32 абсолютных времён, чанк `KEYFRAMETIMES`
  (`XRAnimCompressed.cpp:1944-1951`). Поля «fps» в формате нет.
- `m_Duration` = время последнего ключа: `XRAnimCompressed.cpp:1708-1718`,
  fallback 0.05f при <2 ключей; `GetFrameAbsTime` = `m_lKeyTimes[...]` (:374-377).
- Сверка по MXR_dll_decomp.c (якоря: `"COMPRESSEDANIMATIONSET2"` :456342,
  `"Unsupported sequence version %.4x"` :452970):
  - заголовок последовательности — 8 полей ширин 2,2,1,1,2,2,2,2
    (retail `FUN_102a0a80` :452930-452975) ≡ `XRAnimCompressed.h:27-34`;
  - `KEYFRAMETIMES` — сырой Read без пересчёта (:456568-456600) — идентично;
  - Initialize (`FUN_102a6e80` :457165-457195): `nKeys<2 → 0x3d4ccccd`
    (=0.05f), иначе последний keytime — бит-в-бит наша логика.
- Констант 30.0/0.0333 в анимационном коде MXR нет ни у нас, ни в ретейле.

## Q4. События шагов

**Ключи последовательности на оси времени анимации. Не расстояние.**

- Шаги — `CXR_Anim_DataKey` типа `ANIM_EVENT_TYPE_DIALOGUE`, диалоги 31..42
  (`WObj_Char.h:415-416`, `PLAYER_FOOTSTEP_START/ENDINDEX`).
- Скан окна `[LayerTime, LayerTime + m_TimeSpan*m_TimeScale]` каждый тик
  (`WAG2I.cpp:1733-1736` из `CheckAnimEvents`, `WObj_Char.cpp:2483-2486`);
  ключ срабатывает при пересечении `m_AbsTime` (`XRAnim.cpp:1674-1684`).
- Время слоя: `(GameTime - EnterTime) * TimeScale`, зациклено
  `Modulus(m_Duration)` (`WAG2I_StateInst.cpp:1941-1943`, `XRAnim.cpp:1744-1747`).
- Звук: `WObj_CharMsg.cpp:4543-4580` → `Char_PlayDialogue_Hash` по
  материалу пола (`WObj_CharDialogue.cpp:707-736`); антиспам 6 тиков
  (`WObj_CharMsg.cpp:4565-4573`).
- Отдельного пути шагов у игрока НЕТ — общий код, разница лишь в
  аттенюации 2D/3D (`WObj_CharMsg.cpp:4578`). `Char_SetFootstep` — мёртвый
  (`WObj_CharMechanics.cpp:8840`, первая строка `return;`).
- Частота шагов = nFootKeys / (m_Duration / TimeScale): прямо пропорциональна
  скорости продвижения sequence time. Дистанционной компенсации нет.

## Q5. Зонды для разделения «перемещение быстрое» vs «анимация короткая»

Duration доказанно верна (Q3), поэтому вопрос сводится к: **продвигается ли
sequence time 1:1 с реальным временем** (тогда виновато перемещение — путь
MoveTarget/20 Гц) **или быстрее** (тогда TimeScale/property-тарировка AG2).

Один прогон, два зонда, оба за env-флагом, лог раз в N тиков:

1. **Зонд скорости/проигрывания** — в `Char_ControlMode_Anim2` после
   `GetAnimVelocity` (`WObj_CharControlModeAnim.cpp:73-80`) или в самом
   `GetAnimVelocity` (`WAG2I.cpp:975-995`), для одного walking NPC:
   `|MoveVelocity|` (units/tick), `Layer.m_Time`, `Layer.m_TimeScale`,
   `m_spSequence->GetDuration()`, имя состояния AG2.
   Проверка: `|MoveVelocity| × 30` (units/с) против линейной скорости
   move-track'а `|dMove(track0)| / GetDuration()` — при здоровом синке равны;
   и `d(Layer.m_Time)/dt` против реального времени — должно быть TimeScale.
2. **Зонд шагов** — в обработчике `OBJMSG_CHAR_ONANIMEVENT`
   (`WObj_CharMsg.cpp:4543-4580`), ветка footstep: реальное время между
   срабатываниями + `GetDuration()` текущей последовательности + TimeScale.
   Ритейл-каденс при верном Duration известен из контента (ключи 31..42,
   обычно 2 ключа на цикл): dt_шагов ≈ Duration/2. Отклонение = прямое
   измерение ошибки playback rate.

Интерпретация: шаги в реальном времени частые при `d(Layer.m_Time)/dt == 1`
→ перемещение завышено (подозреваемый №1 — `WFixedCharMovement.cpp:102`);
шаги частые ровно во столько же, во сколько `d(Layer.m_Time)/dt > 1`
→ playback rate AG2 (TimeScale / 20-Гц-тарировка
`WObj_CharClientData.cpp:1194-1196`).

## Свода подозреваемых (по убыванию)

1. `WFixedCharMovement.cpp:102` — хардкод 20 в velocity MoveTarget → 1.5x.
2. `WClient_Core.cpp:523-527` — клиентский `GetGameTickTime()=1/20` при
   30-Гц сервере: все клиентские конверсии `PHYSSTATE_CONVERTFROM20HZ`
   дают ×1.0 вместо ×20/30.
3. 20-Гц-тарировка MoveVelocity-property в анимграф
   (`WObj_CharClientData.cpp:1194-1196`) — легальна, но любой потребитель
   property, трактующий его иначе, рвёт синк.

---

## ПОПРАВКА (проверено 2026-07-30, после сдачи отчёта)

**Оба названных «живых места с зашитой константой 20 Гц» на самом деле
закомментированы и в сборку не попадают.** Проверено вырезанием блочных
комментариев из файлов:

* `WFixedCharMovement.cpp:102` — внутри блока `/*CMoveTarget::CMoveTarget()`
  (строка 8) … `}*/` (строка 227). Весь `CMoveTarget::MoveTo` мёртв.
* `WClient_Core.cpp:523-527` (`GetGameTickTime()` с `return 1.0f/20.0f`) —
  внутри блока `/*` (строка 510) … `*/` (строка 542). Вместе с ним мертвы
  `GetTimeScale`, `GetGameTickRealTime`, `GetGameTick`, `GetGameTime`
  той же группы.
* Заодно: `CWorld_ServerCore::GetGameTickTime()` с `return SERVER_TIMEPERFRAME`
  (`WServer_Core.cpp:398`) — тоже в комментарии (`/*` на строке 381). Это
  объясняет, почему `SERVER_TIMEPERFRAME` нигде не определён (единственный
  `#define` — `WPhysState.h:40` — сам внутри комментария) и при этом всё
  собирается.

Следствие: и клиент, и сервер пользуются одним и тем же
`CWorld_PhysState::GetGameTickTime() const` (`WPhysState.h:527`), который
возвращает data-driven `m_TickTime` = 1/30. Рассогласования 20/30 между
клиентом и сервером нет, и множителя 1.5 из `CMoveTarget` тоже нет.

Что из отчёта остаётся в силе: раздел Q2 (для обычной ходьбы NPC ведёт
анимация, root-motion), Q1 в части «velocity хранится в units/tick и
интегрируется без dt», и Q3 (загрузчик таймингов анимации совпадает с
ретейлом).

Дальше — зонд `RIDDICK_DBG_MOVE=1` на живом пути
(`WObj_CharControlModeAnim.cpp`, сразу после `GetAnimVelocity`).

**Урок для следующих исследований:** в этом дереве очень много кода закрыто
блочными комментариями, в том числе целые определения методов. Прежде чем
называть место «живым», проверять его вырезанием комментариев, а не глазами.

---

## РЕЗУЛЬТАТ ЗОНДА `RIDDICK_DBG_MOVE` (i1_pigsville, 2026-07-30)

Лог: `run_pig.log` (прогон пользователя), 99 строк `[MOVE]` по 7 объектам.

**Интеграция velocity точна. Множителя нет.** Сверка `real(t)` (реально
пройденное за тик) с `anim(t−1)` (запрошенное анимацией в предыдущем тике —
именно оно интегрируется в конце того тика):

| obj | пар | медиана `real(t)/anim(t−1)` | `anim` max (units/тик) |
|---|---|---|---|
| 93  | 21 | 1.000 (min 0.007 — коллизия в ближнем бою) | 21.4 |
| 101 | 21 | 1.000 | 23.9 |
| 111 | 23 | 1.000 | 0.37 |
| 112 | 23 | 1.000 (min 0.93) | 7.98 |
| 113 | 23 | 1.000 | 25.9 |
| 127 | 25 | 1.000 | 0.48 |

Следствия:
1. Гипотезы «×1.5 из-за 20 Гц» (`CMoveTarget`, клиентский
   `GetGameTickTime`) закрыты окончательно: и код мёртв (см. ПОПРАВКУ
   выше), и измерение показывает ×1.000.
2. Часы симуляции идут в реальном времени (`[RATE]` ratio 0.98-1.05,
   30 тиков/с), значит и units/тик → units/с пересчитывается честно.
3. Значит рассинхрон «идёт быстрее, чем шагает» — не в перемещении, а в
   паре «величина move-track-дельты ↔ скорость продвижения времени слоя».

**Что теперь подозрительно — величина.** `anim` у бегущих охранников
доходит до 24-26 units/тик = 720-780 units/с при том, что физическая
скорость персонажа по данным — `m_Speed_Forward` = 12·0.5·20/30 = 4
units/тик = 120 units/с (`WObj_CharClientData.cpp:354`, дефолт; ключи
`SPEED_*` его лишь переопределяют тем же макросом). Разница 3-6× слишком
велика для «бег vs ходьба».

Где может теряться масштаб (в порядке проверки):
1. `GetAnimVelocity` (`WAG2I.cpp:975-995`) берёт дельту трека между
   `Layer.m_Time` и `GetLoopedTime(Layer.m_Time + TimeSpan·Layer.m_TimeScale)`.
   Если `Layer.m_TimeScale` завышен (тарировка `MoveVeloVectorWorld *=
   GetGameTicksPerSecond()/20` — `WObj_CharClientData.cpp:1194-1196`),
   дельта за тик растёт, а поза рисуется по своему времени слоя.
2. `GetLoopedTime` при переходе через конец последовательности: дельта
   `MoveB − MoveA` считается без компенсации wrap'а, и на цикличном
   move-track'е один тик из цикла даёт всплеск. Для «постоянно быстро» не
   годится, но объясняет разброс 0…26.
3. Масштаб самих ключей move-track'а в PC-контенте
   (`XRAnimCompressed.cpp`, распаковка track0) — сверять с ретейлом, как
   уже сверяли тайминги в Q3.

**Следующий зонд (в то же место, к тем же строкам):** `Layer.m_Time`,
`Layer.m_TimeScale`, `m_spSequence->GetDuration()`, полный путь трека
`|EvalTrack0(dur) − EvalTrack0(0)|` и `TimeSpan`. Ожидаемый шаг за тик =
путь/длительность · TimeSpan · TimeScale; сравнение с фактическим `anim`
разводит «врёт выборка» (шаг ≫ ожидаемого) и «врут данные трека»
(совпадает, но сам путь трека огромен).
