# OpenRiddick — портирование на GLES3 + SDL2 (Linux x86_64 / ARM Linux / Android)

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
- [x] Топ-левел `CMakeLists.txt` (опции: `TARGET_LINUX_X86_64`, `TARGET_LINUX_ARM`, Android toolchain).
- [x] Сборка SDK-библиотек: zlib, libpng, ogg, vorbis (статически, из дерева).
- [ ] Цели-заглушки для модулей движка: MCC → MSystem → XR → XRClasses/XRModels → GameWorld → GameClasses → exe (компилируются по мере портирования; управляется опцией `ENGINE_MODULES`).
- Коммит на каждый работающий уровень.

### Фаза 2 — Платформенный слой Linux/SDL2
- [x] `Shared/Platform/Targets/Target_Linux_SDL2.h` + ветка в `Platform.h` (`TARGET_LINUX_SDL2`).
- [x] `MRTC_System_Linux.cpp` (POSIX-порт): память, потоки/синхронизация (pthread/sem), время (CLOCK_MONOTONIC), файлы (fd + синхронный async-слой), поиск файлов (`PS3File_Find*` через opendir/fnmatch), сеть — заглушки. Case-insensitive включения решены симлинками.
- [x] Компиляция **MCC** → `libp5_mcc.a` (без `-fpermissive`; точечные фиксы two-phase lookup, MSVC-измов, vec128-кастов + `-flax-vector-conversions`). Отложено: `VPU/VPUWorkers.cpp` — SPU-job ABI предполагает 32-битные указатели, нужен 64-битный порт.
- [x] Компиляция **MSystem** → `libp5_msystem.a` (106 TU; исключены Win32/Xenon/PS3-TU, ASIO, FaceFX/Bink/XMV-обёртки). Платформенная реализация `MSystem_Linux.cpp` — Фаза 3.
- [ ] Компиляция XR, XRModels, XRClasses, GameWorld, GameClasses, Exe → **линкуемый бинарь** (рендер — null-контекст).

### Фаза 3 — Окно, цикл, ввод (SDL2)
- [ ] `MMain_Linux.cpp`: `main()` → SDL_Init → создание окна `SDL_WINDOW_OPENGL` (EGL/GLES3-контекст) → главный цикл движка (по образцу `MMain_PS3.cpp` / Win32-message-loop).
- [ ] `MDisplaySDL2.*`: `CDisplayContext` (режимы, размер окна, vsync/flip).
- [ ] Ввод: SDL2 → `MInput` (клавиатура→scankey, мышь, `SDL_GameController`).
- [ ] Инструкция запуска с указанием папки ресурсов (см. §3).

### Фаза 4 — Рендерер GLES3 (`Shared/MOS/RenderContexts/GLES3/`)
- [ ] Каркас `CRC_GLES3 : CRC_Core` (по образцу `CRCPS3GCM`), статическая регистрация через `M_STATIC_RENDERER`.
- [ ] Трансляция `CRC_Attributes` → GL-состояние (blend/depth/stencil/cull/scissor).
- [ ] Текстуры: `CTextureContext`-интеграция, форматы (S3TC → распаковка в RGBA8 на GLES, где нет `EXT_texture_compression_s3tc`; на десктопном GLES-эмуляторе расширение обычно есть).
- [ ] Геометрия: пакеты `CXR_VBManager` → стриминг в VBO (кольцевой буфер) + `glDrawArrays/Elements`.
- [ ] Генератор GLSL ES 3.00-шейдеров из attrib/texenv-комбинаций (кэш по ключу состояния) — эквивалент того, что PS3-бэкенд делает для RSX.
- [ ] Поэтапная проверка: сначала UI/2D (фронтенд-меню), затем 3D-мир, потом спецэффекты (стенсильные тени движка — критичная фича Riddick).

### Фаза 5 — FBO-композиция и поворот экрана
Требование: движок «видит» разрешение не окна, а FBO; UI и 3D — раздельные FBO;
финальная композиция в окно с поворотом 0/90/180/270.
- [ ] В `CRC_GLES3`: два offscreen-таргета —
      `FBO_UI` (нативный DPI, размер = логический размер экрана до поворота) и
      `FBO_3D` (масштабируемый, может быть меньше для слабых GPU).
- [ ] `CDisplayContext` (`MDisplaySDL2`) отдаёт движку размеры/aspect **из FBO**
      (с учётом поворота: при 90/270 ширина и высота меняются местами) — все
      viewport'ы и матрицы проекций движок строит от этих значений без его модификации.
- [ ] Композитор: fullscreen-quad, шейдер с матрицей поворота; 3D-слой →
      билинейный апскейл в окно, поверх — UI-слой (alpha blend), затем `SDL_GL_SwapWindow`.
- [ ] Конфиг: cvar/параметры командной строки `-rotate 0|90|180|270`, `-res3d WxH`, `-resui WxH`.
- [ ] Трансляция ввода: координаты мыши/тача из оконных → в координаты FBO
      (обратный поворот + масштаб; отдельная функция `WindowToFBO(x,y)` используется
      слоем SDL2-ввода до передачи в `MInput`).

### Фаза 6 — imGui (отладочный оверлей)
- [ ] Добавить исходники Dear ImGui (`Source/ThirdParty/imgui/`), бэкенды
      `imgui_impl_sdl2` + `imgui_impl_opengl3` (GLES3-совместимы из коробки).
- [ ] Оверлей поверх композиции (рисуется в координатах окна, не FBO):
      панель управления поворотом/масштабом FBO, статистика рендера.

### Фаза 7 — Платформы
- [ ] x86_64 Linux — основная платформа разработки (GLES3 через Mesa/ANGLE или
      десктопный драйвер с `libGLESv2`).
- [ ] ARM Linux (aarch64/armhf) — кросс-компиляция или нативная сборка; NEON-путь
      математики (сначала `MMath_Vec128_Emu`, оптимизация потом).
- [ ] Android: CMake уже готов к NDK toolchain; `SDL2` android-проект
      (`SDLActivity`), ресурсы — во внешнем хранилище, путь через intent/конфиг.

### Фаза 8 — Звук
- [ ] SDL2-аудиобэкенд для `MSound_Core` (callback → микшер движка).

---

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
./build/bin/openriddick -datapath /path/to/riddick/data [-rotate 90] [-res3d 960x540]
```
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
