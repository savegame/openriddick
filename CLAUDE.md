# OpenRiddick — портирование на GLES3 + SDL2 (Linux x86_64 / ARM Linux)

Этот документ — анализ исходников и рабочий план портирования. Он ведётся по мере
выполнения работ: каждый завершённый шаг помечается, каждый логический шаг — отдельный
коммит в ветке `claude/gles3-sdl2-fbo-port-bd56ra`.

---

## 1. Анализ исходников

### 1.1 Что это за код

Движок **Starbreeze P5** — движок игр *The Chronicles of Riddick: Escape from Butcher Bay /
Assault on Dark Athena*. Данный снапшот — конфигурация **PS3** (единственный target-заголовок
`Source/P5/Shared/Platform/Targets/Target_PS3.h`, entry point `MMain.cpp` включает
`MMain_PS3.cpp`, единственный рендер-бэкенд — PS3 GCM). Код исторически
мультиплатформенный (Win32/Xbox/Xenon/PS2/PS3/Dreamcast/GC), в дереве остались
Win32-реализации многих подсистем, но Win32 target-заголовки удалены.

Общий объём: ~3000 файлов, ~1,03 млн строк C++ (только .cpp/.h).

### 1.2 Структура модулей (снизу вверх по зависимостям)

| Модуль | Путь (от `Source/P5/`) | Строк | Назначение |
|---|---|---|---|
| **SDK** | `SDK/` | — | Third-party: zlib, libpng, ogg/vorbis, NeuQuant |
| **MCC** (+MRTC) | `Shared/MCC/` | ~103k | Runtime core: память, потоки (`MRTC_Thread`, `MThreadManager`), время, файлы (`MFile_*`, стримы, MegaFile/XDF-архивы), контейнеры, математика (`MMath_*`, SIMD: SSE/VMX/эмуляция `MMath_Vec128_Emu.h`), строки, hash. Системный слой: `MRTC_System_Win32.cpp` (8k строк, WinAPI), `MRTC_System_PS3.cpp` |
| **MSystem** | `Shared/MOS/MSystem/` | ~149k | Системный слой движка: дисплей (`Raster/MDisplay.*`), абстракция рендера (`Raster/MRender.h` — `CRenderContext`, `Raster/MRCCore.h` — `CRC_Core`), текстуры (`MTexture*`, контейнеры XTC2 и видео), картинки (`MImage*`, S3TC/3DC-компрессия), ввод (`Input/MInput.*`), звук (`Sound/MSound_*`: свой микшер + DSP, кодеки), скрипты, реестр (`Misc/MRegistry*`), консоль |
| **XR** | `Shared/MOS/XR/` | ~117k | 3D-движок: сцена (`XREngine`), шейдерная система (`XRShader*` — материалы поверх render-attrib модели), vertex-buffer менеджмент (`XRVB*`), анимация, скелеты, ткань, навигация, физика (`Phys/`), солиды |
| **XRModels** | `Shared/MOS/XRModels/` | ~109k | Типы моделей: BSP1-4 (уровни), TriMesh, Multi, Sky, Flare и пр. |
| **MOS/Classes** | `Shared/MOS/Classes/` | ~152k | GameWorld-фреймворк (клиент/сервер, WData-ресурсы), GUI (`Win/MWinCtrl*`), видео, рендер-утилиты (`Render/MRenderUtil` и др.) |
| **RenderContexts** | `Shared/MOS/RenderContexts/PS3GCM/` | ~19k | Единственный конкретный рендерер: `CRCPS3GCM : CRC_Core` + `CDisplayContext` PS3 (`MDisplayPS3`) |
| **GameClasses** | `Projects/Main/GameClasses/` | ~333k | Игровой код Riddick: персонажи, оружие, AI, RPG-система |
| **GameWorld** | `Projects/Main/GameWorld/` | ~36k | Клиентские моды, фронтенд (меню) |
| **Exe** | `Projects/Main/Exe/` | ~11k | Точка входа: `XRApp.cpp`, `WGameContextMain.*` |

Сборка исторически: vcproj-файлы (`Lib_MCC`, `Lib_MSystem`, `Lib_XR`, `Lib_XRClasses`,
`Lib_GameClasses*`, `Lib_GameWorld`, `Exe_Main_PS3`). Они задают состав библиотек — их
используем как справочник списков файлов для CMake.

### 1.3 Ключевые архитектурные точки для порта

**Рендер.** Вся отрисовка идёт через виртуальный интерфейс `CRenderContext`
(`MSystem/Raster/MRender.h`) и его расширение `CRC_Core` (`MRCCore.h`): immediate-mode
модель со стейт-атрибутами (`CRC_Attributes`), «vertex buffer» — это CPU-сторонние
пакеты геометрии от `CXR_VBManager`, которые бэкенд транслирует в GPU-команды.
Шейдеров в современном смысле в интерфейсе нет — материалы описываются
attrib-комбинациями (texenv/register-combiner-наследие: `XRShader_FP20/NV20/
TexEnvCombine`), а бэкенд (как `CRCPS3GCM`) сам генерирует/выбирает GPU-программы.
**Порт = написать новый бэкенд `CRC_GLES3 : CRC_Core` (~15–19k строк по аналогии с PS3GCM)
+ `CDisplayContext` на SDL2.** `M_STATIC_RENDERER` (в Target_PS3.h) статически линкует
рендерер — сохраняем этот механизм.

**Платформенный слой.** Выбор платформы — `Shared/Platform/Platform.h` по define
`TARGET_*` → включает `Targets/Target_<X>.h`, который задаёт `PLATFORM_*`, `CPU_*`,
эндианность, SIMD и пр. Для порта нужен новый `Target_Linux_SDL2.h` (x86_64/ARM64 —
little-endian, `CPU_PTR64`, `MMath_Vec128_Emu` или SSE/NEON). Системные реализации:
за образец брать пары `MRTC_System_Win32.cpp` / `MRTC_System_PS3.cpp` (потоки, время,
файлы, память) и писать `MRTC_System_Linux.cpp` на POSIX, `MSystem_Linux.cpp` по образцу
`MSystem_Win32.cpp` / `MSystem_PS3.cpp`.

**Ввод.** `MSystem/Input/MInput.*` — абстракция с платформенными реализациями
(scankey-модель + мышь/геймпад). Пишем SDL2-реализацию.

**Звук.** `MSound_Core` — свой микшер, платформенный слой лишь выводит PCM
(на Win32 — WaveOut/DSound/ASIO). SDL2 audio callback — простой бэкенд-таргет.
На первом этапе можно собрать с null-звуком.

**Эндианность.** PS3 — big-endian (`CPU_BIGENDIAN` в Target_PS3.h). Целевые платформы —
little-endian. Загрузчики файлов движка исторически поддерживают своп (движок
шипился и на LE, и на BE), но **ресурсы от PS3-версии игры могут быть в BE-форматах** —
для запуска нужны ресурсы PC-версии (LE) либо своп при загрузке. Это надо проверить
на этапе запуска с реальными ресурсами.

**VPU/SPU.** `MCC/VPU/MRTC_VPU_*` — задачи для SPU (PS3) с Win32-хелпером
(`MRTC_VPU_Win32_Helper.cpp`) — т.е. существует CPU-путь эмуляции; используем его.

**Известные ресурсные форматы Dark Athena, не покрытые исходником PS3-снапшота (по реверсу PC MSystem.dll):**
- `CImage_FileHeader` v0x0400 — поддержано (см. журнал).
- `CTextureContainer_VirtualXTC2` + секция `IMAGEDIRECTORY5` — **отдельный container-класс**, не наследник существующего `VirtualXTC`. Наши текущие .xtc всё ещё старого формата (парсятся через `IMAGEDIRECTORY4`), но часть архивов DA использует XTC2 и потребует нового класса-парсера (`ReadImageDirectory` считывает список через `ReadImageDirectoryData` в `TThinArray<CTextureDesc>`; текстуры регистрируются в `m_pTC` по описателям). Ожидаемый следующий блокер после подъёма рендера.

**Прочие препятствия к компиляции современным GCC/Clang:**
- код 2003–2008 гг. под MSVC/GCC-4 (PS3 SNC/GCC): нестандартные конструкции, `__forceinline`, `#pragma`, кодировка CP1252 в комментариях;
- inline-ассемблер x86/AMD64 (`MAsm.asm`, `MRTC_System_AMD64.asm`, `MScriptAMD64.asm`) — заменить интринсиками/С++;
- `fp32/fp64`, свои типы — ок, но проверка `CPU_PTR64`-путей (Win64 был, значит 64-бит поддержан);
- PCH-структура (`PCH.h` в каждом модуле) — в CMake через target_precompile_headers или просто обычным include.

### 1.4 Оценка объёма нового кода

| Компонент | Оценка |
|---|---|
| CMake-сборка всех модулей | средняя (списки файлов из vcproj) |
| `Target_Linux_SDL2.h` | малая (по образцу Target_PS3/упоминаний Win64) |
| `MRTC_System_Linux.cpp` (POSIX: потоки/время/файлы/память) | ~3–5k строк |
| `MSystem_Linux.cpp` + `MMain_Linux.cpp` (SDL2 окно/цикл) | ~1–2k строк |
| `CDisplayContext` SDL2 (`MDisplaySDL2.*`) | ~1k строк |
| Рендерер `CRC_GLES3` (attrib→GL state, VB→VBO/DrawArrays, текстуры, шейдер-генератор) | **~10–20k строк — основной объём работ** |
| Ввод SDL2 | ~0.5–1k строк |
| Звук SDL2 | ~0.5k строк |
| FBO-композитор + поворот + трансляция ввода | ~1k строк |

---

## 2. План работ (фазы)

### Фаза 0 — Анализ и план ✅
- [x] Анализ исходников, этот документ.

### Фаза 1 — Каркас сборки CMake
- [x] Топ-левел `CMakeLists.txt`.
- [x] Сборка SDK-библиотек: zlib, libpng, ogg, vorbis (статически, из дерева).
- [ ] Цели-заглушки для модулей движка: MCC → MSystem → XR → XRClasses/XRModels → GameWorld → GameClasses → exe (компилируются по мере портирования; управляется опцией `ENGINE_MODULES`).
- Коммит на каждый работающий уровень.

### Фаза 2 — Платформенный слой Linux/SDL2
- [x] `Shared/Platform/Targets/Target_Linux_SDL2.h` + ветка в `Platform.h` (`TARGET_LINUX_SDL2`).
- [x] `MRTC_System_Linux.cpp` (POSIX-порт): память, потоки/синхронизация (pthread/sem), время (CLOCK_MONOTONIC), файлы (fd + синхронный async-слой), поиск файлов (`PS3File_Find*` через opendir/fnmatch), сеть — заглушки. Case-insensitive включения решены симлинками.
- [x] Компиляция **MCC** → `libp5_mcc.a` (без `-fpermissive`; точечные фиксы two-phase lookup, MSVC-измов, vec128-кастов + `-flax-vector-conversions`). Отложено: `VPU/VPUWorkers.cpp` — SPU-job ABI предполагает 32-битные указатели, нужен 64-битный порт.
- [x] Компиляция **MSystem** → `libp5_msystem.a` (106 TU; исключены Win32/Xenon/PS3-TU, ASIO, FaceFX/Bink/XMV-обёртки). Платформенная реализация `MSystem_Linux.cpp` — Фаза 3.
- [x] Компиляция **XR** (+ часть XRModels из Lib_XR.vcproj) → `libp5_xr.a`
- [x] Компиляция **XRClasses** → `libp5_xrclasses.a`
- [x] Компиляция **GameWorld** (вкл. Shared/MOS/Classes/GameWorld) → `libp5_gameworld.a` (98 TU) и **GameClasses** → `libp5_gameclasses.a` (177 TU); игровой код собирается с `-fpermissive`.
- [x] **Линкуемый и запускаемый бинарь** `build/bin/openriddick`: `MMain_Linux.cpp` (Linux_Main), `MSystem_Linux.cpp` (CSystemLinux + NULL-дисплей/рендер), загрузчик доходит до поиска игровых ресурсов (`Content\`) и корректно сообщает об их отсутствии. Все float-самотесты движка проходят.
- Замечания bring-up: `M_STATIC_RENDERER` отключён (виртуальный CRenderContext, включим обратно при желании после GLES3); `CRC_Attributes` расширен до 11 vec128 на 64-битных указателях; кастомный аллокатор — MDA_ALIGNMENT 16; `-datapath` реализован через chdir + case-insensitive/backslash-разрешение путей в файловом слое.

### Фаза 3 — Окно, цикл, ввод (SDL2)
- [x] `MMain_Linux.cpp`: `main()` → Linux_Main → CSystemLinux → DoModal (главный цикл движка работает; окно пока NULL-дисплей).
- [x] SDL_Init + окно `SDL_WINDOW_OPENGL` с GLES 3.0-контекстом в `MDisplaySDL2.cpp` (clear+swap в PageFlip, SDL_QUIT, деградация в headless при недоступном GL); каркас `CRC_GLES3 : CRC_Core` (методы-заглушки) — `p5_rc_gles3`.
- [ ] `MDisplaySDL2.*`: `CDisplayContext` (режимы, размер окна, vsync/flip).
- [~] Ввод: `CInputContext_SDL2::Update` дренит `SDL_PollEvent` и мапит клавиатуру (`SDL_Scancode` → таблица `SKEY_*`), мышь (motion → `SKEY_MOUSEMOVEREL`, кнопки 1..5 → `SKEY_MOUSE1..5`, wheel → `SKEY_MOUSEWHEELUP/DOWN`), text input (`SDL_TEXTINPUT` → `DownKey(0, ch, ...)`), `SDL_QUIT` → exit. `SDL_PollEvent` в `PageFlip` убран (иначе события расщепляются между двумя дренажами). Геймпад — TODO.
- [x] Инструкция запуска с указанием папки ресурсов (см. §3): `-datapath` работает (chdir + case-insensitive пути).

### Фаза 4 — Рендерер GLES3 (`Shared/MOS/RenderContexts/GLES3/`)
- [x] Каркас `CRC_GLES3 : CRC_Core` (виртуальный, в `RenderContexts/GLES3/MDisplaySDL2.cpp`); наполнение методов — далее.
- [~] Render target: `RenderTarget_SetRenderTarget` биндит default fb0 + viewport, `RenderTarget_Clear` — реальный `glClear` (color/depth/stencil) со scissor'ом при частичном rect. FBO-композиция (offscreen UI/3D + поворот) — Фаза 5.
- [~] M1: Трансляция `CRC_Attributes` → GL state (blend/depth/stencil/cull/scissor/colormask/polygon-offset) в `ApplyAttribs`; `Matrix_SetRender` захватывает Model/Projection/Texture0..3 в поля `CRC_GLES3`; `BeginScene` синкает `glViewport` с `CRC_Viewport::GetViewArea()` (Y-flip). Separate-stencil и per-attribute diff — в M4.
- [~] M2: Текстуры — на `Texture_Precache(TextureID)` тянем `CImage` через `m_pTC->GetTexture` и аплоадим `CGLES3TextureUploader::Upload2D` → GLuint; кэш `TArray<GLuint>` индексируется движковым TextureID; `Texture_Flush`/`Texture_MakeAllDirty` освобождают GL-текстуры. DXT/cube — M5.
- [~] M3: Геометрия — один шейдер `uUseTexture` (pos3+uv2+col4b), `Render_IndexedTriangles/Strip/Wires/Polygon/Primitives` интерлив в `SUIVert{xyz uv col}` + стриминг через `CGLES3VBOStreamer` + `glDrawElements`; текстура берётся из `m_pCurAttrib->m_TextureID[0]`, MVP = Model*Proj; поддерживается конвенция «list of lists» (nTriangles==0xffff) как у PS3. `Render_VertexBuffer(VBID)` — no-op (VBID кэш из Geometry_Precache в M4).
- [ ] Генератор GLSL ES 3.00-шейдеров из attrib/texenv-комбинаций (кэш по ключу состояния) — эквивалент того, что PS3-бэкенд делает для RSX.
- [ ] Поэтапная проверка: сначала UI/2D (фронтенд-меню), затем 3D-мир, потом спецэффекты (стенсильные тени движка — критичная фича Riddick).

### Фаза 5 — FBO-композиция и поворот экрана
Требование: движок «видит» разрешение не окна, а FBO; UI и 3D — раздельные FBO;
финальная композиция в окно с поворотом 0/90/180/270.
- [x] Экранный FBO (первый инкремент): `CRC_GLES3` рендерит все backbuffer-пассы
      в offscreen FBO логического разрешения (color RGBA8 + depth24stencil8);
      `PresentToWindow` (из `PageFlip`, до `SDL_GL_SwapWindow`) — fullscreen-quad
      композит в окно с поворотом 0/90/180/270 (uRot в шейдере).
- [x] `CDisplayContextSDL2` отдаёт движку размеры **из FBO** (`m_Width/m_Height`
      логические; окно — `m_WinWidth/m_WinHeight`); при 90/270 логический размер =
      окно со свапом сторон. Все Y-флипы (scissor/clear/viewport) — от высоты FBO.
- [x] Конфиг: `-rotate 0|90|180|270`, `-winsize WxH` (окно), `-fbosize WxH`
      (логическое разрешение движка; даёт масштабируемый рендер). Общий стейт —
      `g_RiddickPresent` (`MSystem/Raster/MDisplayPresent.*`).
- [x] Трансляция ввода: `RotateDelta` для относительных дельт мыши в
      `CInputContext_SDL2` (обратный поворот); `WindowToFBO(x,y)` готова для
      абсолютных координат/тача. Геймпад: SDL_GameController в
      `CInputContext_SDL2` (PS3-раскладка кнопок/осей, deadzone/power как в
      MInput_PS3, hot-plug).
- [ ] Раздельные `FBO_UI` (нативный DPI) и `FBO_3D` (масштабируемый) + композиция
      3D→апскейл, UI→alpha blend поверх (`-res3d`, `-resui`) — поверх текущего каркаса.

### Фаза 6 — Платформы
- [ ] x86_64 Linux — основная платформа разработки (GLES3 через Mesa/ANGLE или
      десктопный драйвер с `libGLESv2`).
- [ ] ARM Linux (aarch64/armhf) — кросс-компиляция или нативная сборка; NEON-путь
      математики (сначала `MMath_Vec128_Emu`, оптимизация потом).

### Фаза 7 — Звук
- [ ] SDL2-аудиобэкенд поверх `CSoundContext_Mixer` — детальный план в
      `Docs/Sound_SDL2.md` (этапы M0–M4, образец — MSound_PS3).
      M0 (контекст-заглушка CSoundContext_SDL2) сделан 2026-07-20.

### Видео
- Ролики PC — WMV9; варианты декодирования разобраны в
  `Docs/Video_Playback.md` (рекомендация: libtheora + офлайн-конвертация,
  штатный плеер CTextureContainer_Video_Theora уже в дереве).

### Фаза 8 — imGui (отладочный оверлей, в самом конце)
- [ ] Добавить исходники Dear ImGui (`Source/ThirdParty/imgui/`), бэкенды
      `imgui_impl_sdl2` + `imgui_impl_opengl3` (GLES3-совместимы из коробки).
- [ ] ImGui рисуется в **UI FBO** (в логических координатах UI-слоя, поворот
      применяется композитором как и ко всему UI): панель управления
      поворотом/масштабом FBO, статистика рендера. Ввод ImGui получает уже
      транслированные координаты (WindowToFBO).

---

Формат .XW-миров PC-версии (реверс BSPNODES/FACES 0x0204 и пр.) задокументирован
в `Docs/BSP_PC_Format.md` — читать перед любыми работами с BSP-загрузчиками.

## 3. Сборка и запуск

### Сборка (Linux x86_64)
```bash
sudo apt install cmake ninja-build libsdl2-dev libgles-dev
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build
```

Опции CMake:
- `-DENGINE_MODULES=ON|OFF` — собирать модули движка (OFF = только SDK-библиотеки, пока порт не завершён);
- `-DRIDDICK_ROTATE=...` — см. Фазу 5 (появится позже).

### Запуск
```bash
./build/bin/openriddick -datapath /path/to/riddick/data \
    [-rotate 0|90|180|270] [-winsize 1280x720] [-fbosize 720x1280]
```
- `-rotate` — поворот картинки в окне по часовой; при 90/270 движок видит
  разрешение окна со свапом сторон (портретный контент в ландшафтном окне);
- `-winsize` — физический размер окна (по умолчанию 1280x720);
- `-fbosize` — переопределение логического разрешения движка (рендер в FBO
  этого размера + масштабирование при композиции в окно).
`-datapath` — папка с игровыми ресурсами (файлы `.XTC/.XW/.XSA` и MegaFile-архивы
PC-версии игры; уточнение структуры — на Фазе 3, когда заработает загрузка).

---

## 4. Журнал выполнения

| Дата | Шаг | Коммит |
|---|---|---|
| 2026-07-13 | Анализ исходников, план (этот файл) | Фаза 0 |
| 2026-07-13 | CMake-каркас: SDK-библиотеки (zlib, libpng, ogg, vorbis) собираются на x86_64 | Фаза 1 |
| 2026-07-13 | Target_Linux_SDL2.h (x86_64 SSE2 / ARM NEON-emu), ветка в Platform.h, smoke-тест platform_check | Фаза 2 (начало) |
| 2026-07-13 | MRTC_System_Linux (POSIX), MFloat_Linux, симлинки для case-insensitive включений, фиксы MSVC-измов — libp5_mcc.a собирается | Фаза 2 |
| 2026-07-13 | MSystem: фиксы two-phase lookup (StrToIntParse/M_Pow/M_Sqrt), BSD-сокеты в MRTC_Task, ветки PLATFORM_LINUX — libp5_msystem.a собирается | Фаза 2 |
| 2026-07-13 | XR: глобальные фиксы (`template<> static`, операторы TFStr/CStr, bool→NULL, this->) — libp5_xr.a собирается (88 TU) | Фаза 2 |
| 2026-07-13 | XRClasses: dDOT-декларации, M_Sqrt(int), restrict-касты, copysign — libp5_xrclasses.a (30 TU) | Фаза 2 |
| 2026-07-14 | GameWorld + GameClasses собираются (275 TU, -fpermissive), ветка MACRO_MAIN для Linux, BSD-сокеты в WGameMultiplayerHandler | Фаза 2 |
| 2026-07-14 | Из плана исключён Android; imGui перенесён в конец (рисует в UI FBO) | план |
| 2026-07-14 | Бинарь openriddick линкуется и запускается: MMain_Linux, MSystem_Linux (NULL-дисплей), VPU-стабы, bootstrap init_priority, фиксы 64-бит (CRC_Attributes 11×vec128, MDA_ALIGNMENT 16, RoundToInt), -datapath | Фазы 2-3 |
| 2026-07-14 | Фикс поиска ресурсов: нормализация DEFAULTGAMEPATH (компоненты без разделителя на конце, `Content`+`FONTS\...`) — шрифт находится с реальным Environment.cfg | Фаза 3 |
| 2026-07-14 | Фикс усечения find-хэндла (aint→int в MFile_Misc): таблица слотов вместо указателей | Фаза 3 |
| 2026-07-14 | Каркас рендера: MDisplaySDL2 (окно SDL2 + GLES 3.0, clear+swap, SDL_QUIT) + CRC_GLES3-заглушка, цель p5_rc_gles3, дисплей SDL2 в списке CSystemLinux с fallback на NULL | Фазы 3-4 |
| 2026-07-14 | Диагностика загрузки XTC: hex-дамп «плохого» хедера в CImage_FileHeader::Read, опциональная трасса CTexture::ReadIndexData + VirtualXTC::ScanImageList (`RIDDICK_DBG_XTC=1`) | bring-up |
| 2026-07-14 | Поддержка формата PC/Dark Athena CImage_FileHeader v0x0400: структура расширена m_ChunkSize/m_ChunkCount, добавлен пост-инвариантный fixup под флагом 0x4000 (сверено с MSystem.dll Ghidra) | bring-up |
| 2026-07-14 | Фикс бесконечной рекурсии в трёх шаблонных `operator+` для TFStr<N> (MRTC_String.h): каст правого операнда к CStrBase& для выбора мембер-`operator+` — переполнение стека при конкатенациях от 4 уровней и глубже | bring-up |
| 2026-07-14 | No-op заглушка ввода: `CInputContext_SDL2` (Input/MInput_SDL2.cpp) наследует CInputContextCore, регистрируется через MRTC_IMPLEMENT_DYNAMIC + MRTC_REFERENCE в MCreateInputContext; разблокирует CSystemCore::CreateInput. Реального pump'а событий пока нет. | Фаза 3 |
| 2026-07-14 | Первые методы CRC_GLES3: RenderTarget_SetRenderTarget (bind fb0 + viewport) и RenderTarget_Clear (glClearColor/Depth/Stencil + scissor при частичном rect, с учётом top-left→bottom-left оси Y). Движок дошёл до цикла отрисовки фронтэнда (cg_rootmenu 'legal'→'esrb'→'logo_atari'). | Фаза 4 |
| 2026-07-14 | Фаза 4 M0: скелетные модули GLES3-бэкенда — `GLES3_Shader` (compile/link + uniform cache), `GLES3_Texture` (CImage→GLuint аплоадер, RGBA8/BGRA8-swizzle/RGB8/I8/A8/I8A8; DXT в M5), `GLES3_VBOStreamer` (кольцевые dynamic VBO+IBO, orphan-refill). Добавлены в цель p5_rc_gles3, к CRC_GLES3 пока не подключены — база для M1..M3. | Фаза 4 |
| 2026-07-14 | Фаза 4 M1: `Attrib_Set/Attrib_SetAbsolute` → GL state (depth-test/write, blend + src/dst mapping, color/alpha mask, cull + winding, scissor + Y-flip, polygon-offset, stencil w/ front-only + op-таблица keep/zero/replace/incr/decr/invert/wrap); `Matrix_SetRender` → Model/Projection/Texture0..3 в CRC_GLES3; `BeginScene` → `glViewport` от `CRC_Viewport::GetViewArea()`. Помощники `GLES3_MapBlend`/`GLES3_MapCompare`. | Фаза 4 |
| 2026-07-14 | Фаза 4 M2: on-demand текстурный аплоад — `Texture_Precache(TextureID)` тянет CImage через `m_pTC->GetTexture` и загружает GL-текстуру через `CGLES3TextureUploader::Upload2D`; кэш `TArray<GLuint>` индексируется движковым TextureID и растёт лениво; `Texture_Flush`/`Texture_MakeAllDirty` освобождают. `TextureID_EnsureUploaded` — точка входа для draw path M3. | Фаза 4 |
| 2026-07-14 | Фаза 4 M3: реальный draw path — единый GLSL ES 3.00 UI-шейдер (pos+uv+col+uUseTexture+uTex), интерлив вершин `SUIVert{xyz uv col=RGBA-swapped-BGRA}` из `m_Geom`, стриминг в кольцевой VBO/IBO, `glDrawElements`; `Render_IndexedTriangles/Strip/Wires/Polygon/Primitives` реализованы; текущая текстура берётся из `m_pCurAttrib->m_TextureID[0]` и лениво аплоадится; MVP = Model*Proj + column-major upload. `Render_VertexBuffer(VBID)` пока no-op. Первые пиксели: белый квадрат в верхнем правом углу (все DXT-текстуры пока пропускаются). | Фаза 4 |
| 2026-07-17 | Старт кампании: форс дефолтного профиля в Con_StartNewCampaign (m_bValidProfileLoaded без savegame-контекста); VBID-путь Render_VertexBuffer (VB_Get→CRC_BuildVertexBuffer, V3_F32/V2_F32/N4_COL, CRCPrimStreamIterator, DrawUserVerts) — куб/анимации фронтенда | Фазы 3-4 |
| 2026-07-17 | Фаза 5 (инкремент 1): экранный FBO логического разрешения + композит в окно с поворотом 0/90/180/270 (`PresentToWindow`), `-rotate/-winsize/-fbosize`, g_RiddickPresent (MDisplayPresent.*), поворот дельт мыши в CInputContext_SDL2 | Фаза 5 |
| 2026-07-17 | Ввод: геймпад SDL_GameController (оси POS/NEG-сканы с PS3-кривыми, кнопки в PS3-нумерации, dpad→POV, hot-plug) | Фаза 3 |
| 2026-07-17 | GLES3: мультитекстура канал 1 (uv1 в вершине, uTex1 modulate — лайтмапы BSP), общий сетап юниформ вынесен в SetupCommonUniforms; separate stencil (CRC_FLAGS_SEPARATESTENCIL → glStencilFunc/OpSeparate) | Фаза 4 |
| 2026-07-17 | GLES3: packed-форматы вершин VBID-пути (VRegFetch: I16/U16 raw, NS/NU нормализованные) для позиций и UV; диагностика lastFmt пропусков | Фаза 4 |
| 2026-07-14 | Фаза 4 M5-partial: CPU DXT1 + DXT5 декодер (`GLES3_DXT.*`) — на аплоаде S3TC-сжатой CImage дёргаем `LockCompressed()`, читаем 16-байт `CImage_CompressHeader_S3TC`, распаковываем блоки 4x4 → RGBA8, `glTexImage2D(GL_RGBA8)` + mipmaps. Wrap=REPEAT для нормалей/env. DXT3 добавим по мере встречаемости. | Фаза 4 |
| 2026-07-20 | Фаза 7 M0: звуковой контекст-заглушка `CSoundContext_SDL2` (`Sound/SDL2/MSound_SDL2.*`) — наследник `CSoundContext_Mixer`, Platform_* no-op (вывода звука нет, голоса молчат), `Platform_GetInfo` отдаёт 48 кГц/стерео/фрейм 256. SND_CLASS=SDL2 под PLATFORM_LINUX (XRApp.cpp), класс вытаскивается из libp5_msystem.a через MRTC_REFERENCE в MCreateSoundContext (MSound.cpp). Диагностика `[SND-SDL2]` в Create/Platform_Init. | Фаза 7 |
| 2026-07-20 | Фаза 7 M1: вывод мастера микшера в SDL — `CSoundContext_SDL2` открывает SDL_OpenAudioDevice (48 кГц, AUDIO_F32SYS, стерео, 512 сэмплов) в Platform_Init; статический AudioCallback тянет готовые фреймы через `m_Mixer.StartNewFrame()` (lock-free, «safe to call from interrupts»), копирует stereo fp32 из vec128-упаковки (stride=4 float/сэмпл, L=[0], R=[1]), остаток фрейма хранится между callback'ами, при недоборе — тишина + счётчик underrun. Диагностика `[SND-SDL2] cb #N` первые 3 + каждый 3000-й. Голоса ещё не подаются (M2) — вывод тишины микшера. | Фаза 7 |
| 2026-07-20 | Фаза 7 M2: голоса в микшер. `CSoundContext_SDL2` теперь наследует `CSoundContext_Vorbis` — готовую CPU-реализацию прокачки волн (MSound_Vorbis.*, была под PLATFORM_WIN_PC, включена и для PLATFORM_LINUX): Wave_Precache декодирует статические волны потоком LoadStatic, короткие SFX играют одним circular-пакетом, длинные/стрим — decode-потоки с 3×100 мс пакетами, луп через CSCC_Codec::GetData(_bLooping); пауза EPauseSlot_Delayed снимается после первого пакета, Platform_IsPlaying честный. В SDL2 остались только Platform_GetInfo + SDL-вывод + логи start/stop voice (кап 200). Кодек `CMSound_Codec_VORB` собран (Sound/Codecs/MSound_Codec_Vorbis.cpp, снят IMAGE_IO_NOVORBIS в Target_Linux_SDL2.h, MRTC_REFERENCE в MSound.cpp); RAW-кодеку добавлен GetData(fp32) для PCM-волн. | Фаза 7 |
