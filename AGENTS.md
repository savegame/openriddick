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

## Состояние порта (на 2026-07-19)
- Движок стартует и работает **без падений**: доходит до рут-меню (`cg_rootmenu('legal'/'kiosk'/'esrb')`), мир Pa1_Intro грузится до конца (прекэш, Simulate_Resume).
- Загрузка AG2 v6 (PC-графы анимаций) реализована в `Source/P5/Shared/MOS/XR/XRAnimGraph2/` — версии 3/4/5/6; guard'ы от спецзначений target-state (TERMINATE/STARTAG) в `WAG2I_Resources.cpp`.
- **Игровой графики на экране нет (чёрный экран)** — текущая активная задача. Шрифты и 12 текстур аплоадятся (`[GLES3-TEX-OK]`), ~500 текстур уровня — нет (`[GLES3-TEX-FAIL]`): форматы 0x40/0x800/0x20000 и S3TC sub=0/4 (DXT0/DXT4) не поддержаны в `GLES3_Texture.cpp` — placeholder.
- Латентная порча кучи (`munmap_chunk` в разных местах: radeonsi, OS_FileAsyncClose) — в последних прогонах не воспроизводится; при рецидиве — valgrind-прогон (команда зафиксирована в переписке 2026-07-19).

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