# AGENTS.md

## Правила работы
- не читай ничего в папке build и подпапках, это папка сборки
- пиши код совместимый с C++17, не выше.
- **НЕ билдить и НЕ запускать игру самостоятельно.** Только писать код; сборку и запуск делает пользователь, результаты присылает логом (обычно `run.log`).
- **Исходники в CP1252 (часть с CRLF) — НЕ конвертировать.** Комментарии с русским текстом в CP1252. Read/Edit могут отказать из-за non-UTF8 байт — тогда править байтово через `python3` (читать/писать в `'rb'`/`'wb'`, матчить по ASCII-фрагментам, сохранять `\r\n`). Конвертация всех исходников — отдельная отложенная задача, не предлагать её.
- Коммиты: ветка `claude/gles3-sdl2-fbo-crash-2w2izj`, сообщения по-русски, стиль как в `git log`. Перед коммитом — спросить пользователя.
- Файлы `*_decomp.c` и `MOVETOKENS_calls.txt` в корне — огромные (сотни тысяч строк), **только grep по ключевым словам**, не читать целиком. Если нужен декомпайл конкретной функции (`FUN_xxxxxxxx`) — попросить пользователя, он выгрузит из Ghidra быстрее.
- Для исследования крупных файлов использовать суб-агентов (explore), чтобы не раздувать контекст.

## Сборка и запуск
- CMake, build-директория `build/desktop-x86_64/`, бинарь `build/desktop-x86_64/bin/openriddick`.
- Запуск (из корня репо): `./build/desktop-x86_64/bin/openriddick -datapath /mnt/data_storage/sashikknox/Games/Riddick` — PC-ресурсы Riddick (XDF-архивы, миры Pa1_Intro и пр.).
- Полезные env-переменные порта:
  - `RIDDICK_DBG_SURF=1` — отладочный вывод по поверхностям/рендеру;
  - `RIDDICK_DBG_GL=1` — ценз текстур (`[GL-TEXREQ]`), RT-переключения (`[GLES3-RT]`);
  - `RIDDICK_ASSERT_FATAL=1` — вернуть жёсткий останов на M_ASSERT (по умолчанию ассерты log-and-continue, как в retail M_RTM).
- Пользователь гоняет gdb/valgrind сам; типовой bt — в `run.log`.

## Реверс-ресурсы (декомпиляции Ghidra, корень репо)
- `GameWorld_Win32_x86_dll_decomp.c` (~778k строк) — GameWorld DLL: геймплей, AG2 (MOVETOKENS/GRAPHBLOCKS/FULLSTATES), форматы записей каталога.
- `MXR_dll_decomp.c` — оригинальная библиотека загрузки BSP-уровней.
- `MSystem_dll_decomp.c` — чтение файлов уровней, формат архива и пр. (разобрано частично).
- `MOVETOKENS_calls.txt` — выборка Ghidra-функций вокруг MOVETOKENS/GRAPHBLOCKS (CXRAG2 = анимации персонажей, графы анимаций).
- `Docs/` — заметки по форматам (BSP_PC_Format.md и др.).

## Состояние порта (на 2026-07-20)
- Движок стартует и работает **без падений**: доходит до рут-меню (`cg_rootmenu('legal'/'kiosk'/'esrb')`), мир Pa1_Intro грузится до конца (прекэш, Simulate_Resume).
- Загрузка AG2 v6 (PC-графы анимаций) реализована в `Source/P5/Shared/MOS/XR/XRAnimGraph2/` — версии 3/4/5/6; guard'ы от спецзначений target-state (TERMINATE/STARTAG) в `WAG2I_Resources.cpp`.
- **Игровой графики на экране нет (чёрный экран)** — текущая активная задача.
  Уточнение по текстурам: `s3tcSub=0/4` — это **DXT1/DXT5** (enum
  `IMAGE_COMPRESSTYPE_S3TC_DXT1 = 0`), и они, как и форматы
  0x40/0x800/0x20000 (BGRX8/BGRA8/I8A8), ПОДДЕРЖАНЫ аплоадером
  (`GLES3_Texture.cpp: MapFormat` + DXT-декодеры). Фейлы происходят в
  молчаливых ветках (`Lock()/LockCompressed()==NULL` или `glGenTextures==0`
  — возможно, вызов не из GL-потока при прекэше) — в эти ветки добавлены
  диагностические принты, следующий прогон назовёт точную причину.
  Также снят вечный латч на placeholder: неудачный аплоад ретраится при
  каждом запросе (лог — один раз).
  Вторая ветвь гипотезы (закоммичено, e70ec34): caps рендерера были -1 —
  движок включал occlusion-query-отсечение (наши заглушки отвечают «не
  видно» -> мир отсекается целиком) и FP20-шейдерные слои; теперь caps
  честные (HWAPI|ARBITRARY_TEXTURE_SIZE|SEPARATESTENCIL, 2 текстурных юнита).
- Латентная порча кучи (`munmap_chunk` в разных местах: radeonsi, OS_FileAsyncClose) — в последних прогонах не воспроизводится; при рецидиве — valgrind-прогон (команда зафиксирована в переписке 2026-07-19).
- Ветка `kimi_fixes` — устаревший срез (откат caps/texture-правок), полезного не содержит.

## Архитектурные решения, стабы и обходы (полная карта для агентов)

### Политика ошибок
- **M_ASSERT = log-and-continue** (`MRTC_System_Linux.cpp: OS_Assert`):
  печатает `ASSERT: ...` в stderr и продолжает — поведение retail-сборок
  (M_RTM вычеркивал ассерты; игра шипилась с данными, на которых они
  срабатывают). `RIDDICK_ASSERT_FATAL=1` возвращает жёсткий стоп для gdb.
  Следствие: после пропущенного ассерта возможен SIGSEGV в точном месте —
  это осознанно (bt точнее).
- Фатальные сигналы (SEGV/BUS/FPE/ILL/ABRT) печатают backtrace в stderr
  (`MMain_Linux.cpp: Linux_FatalSignal`, exe слинкован с `-rdynamic`).
  ВАЖНО: при переполнении стека хэндлер не сработает (sigaltstack не
  ставится — было отклонено, см. git log 42e4916^..42e4916 в истории) —
  тогда тихая смерть, брать bt из gdb.
- Движковые исключения (`Error_static`/CCException) работают штатно,
  печатаются как `Exception! Location: ...`.

### Консоль и логи (маркеры в stderr)
- `[CON] ...` — зеркало движковой консоли (ConOut/ConOutL) в stderr
  (`MSystem_Core.cpp`), иначе `World doesn't exist` и пр. не видны в run.log.
- `[GLES3-TEX-OK/FAIL]` — аплоад текстур; `[GL-TEXREQ]` — разовый ценз
  каждого texture ID из атрибутов draw-вызовов (RIDDICK_DBG_GL=1);
  `[GL-DBG]` — счётчики draw/verts/texB каждые 60 кадров (RIDDICK_DBG_GL=1);
  `[GLES3-RT]` — SetRenderTarget/CopyToTexture (RIDDICK_DBG_GL=1);
  `[GLES3-RTT]` — создание FBO для RTT-текстур; `[SURF]` — первые 120
  вызовов CXR_Util::Render_Surface с TextureID слоёв (RIDDICK_DBG_SURF=1);
  `[REG-ERR]` — backtrace при Index-out-of-range в CRegistry_Dynamic::GetChild;
  `(Command_ChangeMap)` — резолв путей мира; `(Con_StartNewCampaign)` — старт кампании.

### Игровые обходы (bring-up, потом пересмотреть)
- **Профиль форсируется** в `Con_StartNewCampaign` (WGameContextMain.cpp):
  savegame-контекста на Linux нет, `m_bValidProfileLoaded` ставится в true c
  дефолтными настройками — иначе Con_ChangeMap молча отказывает. Сейвов НЕТ.
- **Стартовый мир кампании** ищется по кандидатам (campaign -> Pa1_Intro)
  через FileExists — в PC-наборе EFBB нет bootstrap-мира campaign.xw.
- `setdifficultycampaign` дополнительно пишет числовую опцию GAME_DIFFICULTY.
- `startnewcampaign`/`setdifficultycampaign` реализованы в
  CGameContextMod (были DummyInt-стабами в XRApp.cpp); `checkinvite`/
  `issignedin` и пр. Live-функции — стабы/отсутствуют (Parse error в логе —
  безвреден).
- GUI-окна, отсутствующие в EFBB-ресурсах (`remove_efbb_wait` и пр.) —
  DoWindowSwitch логирует и живёт дальше; это НЕ OS-окна, а страницы меню.
- Guard'ы против PC-данных: RAGDOLLS (компактная запись вместо дыр,
  WObj_GameCore.cpp), пустой анимграф (GetMatchingGraphBlock -> NULL),
  INVALID MOVETOKEN (MoveGraphBlock -> ConOut+return), target-state
  TERMINATE/STARTAG (WAG2I_Resources.cpp).

### Стабы подсистем
- **Звук**: контекста НЕТ (SND_CLASS=DSound2 не создаётся, ловится
  исключением — «Failed to initialize sound»). План реализации —
  `Docs/Sound_SDL2.md`.
- **Видео**: WMV9-ролики не декодируются; Theora-плеер в дереве есть, нет
  libtheora. Варианты — `Docs/Video_Playback.md`.
- **Сеть**: BSD-сокеты точечно в MRTC_Task/WGameMultiplayerHandler,
  остальное заглушки.
- **VPU/SPU**: CPU-путь, VPUManager застаблен; `VPU/VPUWorkers.cpp`
  исключён из сборки (32-битный SPU ABI).
- **Occlusion queries**: заглушки CRC_Core (поэтому caps-флаг снят — см. выше).

### Файловый слой (MRTC_System_Linux.cpp)
- Case-insensitive разрешение путей + `\`->`/` (Linux_ResolvePath);
  find-хэндлы — таблица слотов (MFile_Misc хранит хэндл в int);
  DEFAULTGAMEPATH нормализуется (завершающие `\` у компонентов) в
  MSystem_Core.cpp; async-IO синхронный; OS_Alloc зануляет память
  (семантика VirtualAlloc — на этом уже ловили «работало случайно»).

### Рендер (GLES3, Shared/MOS/RenderContexts/GLES3/)
- `M_STATIC_RENDERER` ВЫКЛЮЧЕН (виртуальный CRenderContext).
- Весь «backbuffer» рендерится в экранный FBO логического разрешения;
  `PresentToWindow` (из PageFlip) композитит в окно с поворотом
  0/90/180/270 (`-rotate/-winsize/-fbosize`, общий стейт g_RiddickPresent,
  поворот дельт мыши в MInput_SDL2). Раздельные UI/3D FBO — не сделаны.
- Один GLSL-шейдер (pos+uv0+uv1+color): текстура канала 0 + модуляция
  каналом 1 (лайтмапы), альфа-тест, туман, uTexMat; separate stencil есть.
  Шейдер-генератора по attrib-комбинациям НЕТ (M4 не сделан).
- UI Dark Athena рисуется через RenderTarget_CopyToTexture (~30 копий/кадр,
  glCopyTexSubImage2D в GL-ориентации — НЕ флипать, проверено).
- `Render_VertexBuffer(VBID)`: без GPU-кэша — VB_Get(BUILD) на каждый draw,
  поддержаны форматы F32/I16/U16/NS/NU (VRegFetch).
- Texture ID, привязанный к RTT-FBO, резолвится в его color-текстуру
  (EnsureUploaded -> GetFBOSlot).
- Caps честные (см. выше); RC регистрируется в CTextureContext/CXR_VBContext
  (AddRenderContext, как PS3).

## Журнал
- `CLAUDE.md` — ведётся журнал портирования (§4) и TODO; обновлять при смене этапа.


## Правила кода
### Структура модулей (снизу вверх по зависимостям)

| Модуль | Путь (от `Source/P5/`) | Строк | Назначение |
|---|---|---|---|
| **SDK** | `SDK/` | — | Third-party: zlib, libpng, ogg/vorbis, NeuQuant |
| **MCC** (+MRTC) | `Shared/MCC/` | ~103k | Runtime core: память, потоки (`MRTC_Thread`, `MThreadManager`), время, файлы (`MFile_*`, стримы, MegaFile/XDF-архивы), контейнеры, математика (`MMath_*`, SIMD: SSE/VMX/эмуляция `MMath_Vec128_Emu.h`), строки, hash. Системный слой: `MRTC_System_Win32.cpp` (8k строк, WinAPI), `MRTC_System_PS3.cpp` |
| **MSystem** | `Shared/MOS/MSystem/` | ~149k | Системный слой движка: дисплей (`Raster/MDisplay.*`), абстракция рендера (`Raster/MRender.h` — `CRenderContext`, `Raster/MRCCore.h` — `CRC_Core`), текстуры (`MTexture*`, контейнеры XTC2 и видео), картинки (`MImage*`, S3TC/3DC-компрессия), ввод (`Input/MInput.*`), звук (`Sound/MSound_*`: свой микшер + DSP, кодеки), скрипты, реестр (`Misc/MRegistry*`), консоль |
| **XR** | `Shared/MOS/XR/` | ~117k | 3D-движок: сцена (`XREngine`), шейдерная система (`XRShader*` — материалы поверх render-attrib модели), vertex-buffer менеджмент (`XRVB*`), анимация, скелеты, ткань, навигация, физика (`Phys/`), солиды |
| **XRModels** | `Shared/MOS/XRModels/` | ~109k | Типы моделей: BSP1-4 (уровни), TriMesh, Multi, Sky, Flare и пр. |
| **MOS/Classes** | `Shared/MOS/Classes/` | ~152k | GameWorld-фреймворк (клиент/сервер, WData-ресурсы), GUI (`Win/MWinCtrl*`), видео, рендер-утилиты (`Render/MRenderUtil` и др.) |
| **RenderContexts** | `Shared/MOS/RenderContexts/` | ~19k+ | `GLES3/` — активный рендерер порта (`MDisplaySDL2.cpp` — дисплей-контекст SDL2/GLES3, `GLES3_Texture.cpp` — аплоад/DXT-декод, `GLES3_VBOStreamer`, `GLES3_Shader`); `PS3GCM/` — эталонный рендерер PS3 (`CRCPS3GCM : CRC_Core`, `MDisplayPS3`) |
| **GameClasses** | `Projects/Main/GameClasses/` | ~333k | Игровой код Riddick: персонажи, оружие, AI, RPG-система |
| **GameWorld** | `Projects/Main/GameWorld/` | ~36k | Клиентские моды, фронтенд (меню) |
| **Exe** | `Projects/Main/Exe/` | ~11k | Точка входа: `XRApp.cpp`, `WGameContextMain.*` |

Сборка исторически: vcproj-файлы (`Lib_MCC`, `Lib_MSystem`, `Lib_XR`, `Lib_XRClasses`,
`Lib_GameClasses*`, `Lib_GameWorld`, `Exe_Main_PS3`). Они задают состав библиотек — их
используем как справочник списков файлов для CMake.