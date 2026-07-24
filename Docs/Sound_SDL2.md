# Звуковая подсистема через SDL2 Audio — план реализации

Как реализовать платформенный звуковой бэкенд для Linux-порта поверх
существующего CPU-микшера движка. Писалось по анализу `MSound_PS3.*`
(референс-реализация той же схемы) и `MSound_SCMixer.*`.

## 1. Архитектура движка

```
CSoundContext                      (интерфейс, MSound.h)
  └─ CSoundContext_Mixer           (MSound_SCMixer.*: весь микшинг/DSP на CPU,
     │                              CSC_Mixer — граф DSP: voices → volume matrix
     │                              → router → reverb/biquad → master)
     └─ CSoundContext_<Platform>   (платформа: только ВЫВОД готового PCM
                                    + прокачка данных голосов в микшер)
```

Ключевое: **микшер полностью CPU-шный** (SPU-путь PS3 нам не нужен — CPU-вариант
DSP уже собран: `MSound_Mixer_Worker_CPUVPUAccess.cpp`, `*.imp.h`). Платформа
реализует только чистые виртуалы `CSoundContext_Mixer`:

| Метод | Что делает |
|---|---|
| `Platform_GetInfo(CPlatformInfo&)` | частота (48000), число микшерных голосов, потоков обработки |
| `Platform_Init(MaxMixerVoices)` | старт потоков/устройства |
| `Platform_StartStreamingToMixer(MixerVoice, pVoice, WaveID, SampleRate)` | начать прокачку волны в голос микшера |
| `Platform_StopStreamingToMixer(MixerVoice)` | остановить голос |
| `Wave_Precache*` | загрузка волн в память |
| `Create/Init/Refresh/KillThreads` | жизненный цикл |

## 2. Как это сделано на PS3 (наш образец)

`CSoundContext_PS3` (MSound_PS3.cpp, ~2.4k строк — большая часть переносима):

1. **Вывод**: `Thread_AudioRingBuffer` — поток по событию аудиопорта берёт у
   микшера готовый блок: `CSC_Mixer_OuputFrame* pFrame = m_Mixer.StartNewFrame()`
   и копирует его в кольцевой буфер железа (блоки по **256 сэмплов**, 48 кГц,
   float). Это единственное место, привязанное к аудио-API платформы.
2. **Прокачка голосов**: `Thread_Read` (чтение+декодирование волн в
   `CSC_Mixer_Packet`) и `Thread_Submit` (подача пакетов в голос микшера),
   `Thread_StartSounds` — отложенный старт. Кодеки (Vorbis/ADPCM/PCM) — общие,
   в `Sound/Codecs` + `Sound/Vorbis` (уже собираются).

## 3. Реализация SDL2 (`Shared/MOS/MSystem/Sound/SDL2/MSound_SDL2.{h,cpp}`)

### Точка входа
- `XRApp.cpp` создаёт контекст: `MCreateSoundContext("CSoundContext_" + SND_CLASS)`;
  дефолт SND_CLASS = "DSound2" — добавить ветку `#elif defined(PLATFORM_LINUX)`
  → `"SDL2"`.
- Класс регистрируется как остальные: `MRTC_IMPLEMENT_DYNAMIC(CSoundContext_SDL2,
  CSoundContext_Mixer)` + `MRTC_REFERENCE` в exe (иначе линкер выкинет из
  статической библиотеки — та же грабля, что была с input/display).

### Вывод через SDL
Два варианта; рекомендуется **(а) callback**:

```cpp
SDL_AudioSpec Want{};
Want.freq = 48000;              // микшер фиксирован на 48k (как CellAudio)
Want.format = AUDIO_F32SYS;     // микшер выдаёт float; иначе конвертить в S16
Want.channels = 2;
Want.samples = 512;             // 2 блока микшера по 256
Want.callback = &AudioCallback; // static, userdata = this
m_Dev = SDL_OpenAudioDevice(NULL, 0, &Want, &Have, 0);
SDL_PauseAudioDevice(m_Dev, 0);
```

`AudioCallback(len)`: пока не набрали `len` байт — `m_Mixer.StartNewFrame()` →
копировать interleaved-стерео из output-фрейма; если микшер не готов — нули
(тишина), НЕ блокироваться в callback. Остаток фрейма хранить между вызовами.

(б) альтернатива — свой поток + `SDL_QueueAudio` (без callback, проще
дебажить, чуть больше латентность).

`SDL_InitSubSystem(SDL_INIT_AUDIO)` — в `Create()`.

### Прокачка голосов
Первая итерация — без отдельных потоков чтения (у нас файловый слой
синхронный): в `Platform_StartStreamingToMixer` декодировать волну в память
(как PS3 `Thread_Read`, но инлайн или в один общий worker-поток
`MRTC_Thread`), резать на `CSC_Mixer_Packet` и подавать; unpause голоса слотом
`EPauseSlot_Delayed` после первого пакета (см. комментарий к пюре-методу в
MSound_SCMixer.h:730). Стримящиеся треки (музыка/VO) — через
`CSoundContext_Mixer`-овский dual-stream/`MSound_CoreDualStream` — смотреть,
что уже даёт базовый класс, прежде чем писать своё.

### Этапы (каждый — коммит, работоспособный билд)
1. **M0 — контекст без звука**: класс создаётся, `Platform_*` заглушки,
   `Platform_GetInfo` отдаёт 48k/64 голоса. Ценность: `SYSTEM.SOUND`
   зарегистрирован, игровой код перестаёт ходить по NULL-веткам.
2. **M1 — вывод мастера**: SDL-устройство + callback, `StartNewFrame`-цикл.
   Слышна тишина; проверка, что микшер тикает без крэшей.
3. **M2 — статические волны**: `Wave_Precache` + StartStreaming → первые
   звуки GUI/шаги.
4. **M3 — стриминг** (музыка, диалоги) + `Platform_IsPlaying`.
5. **M4 — тюнинг**: латентность (samples=256), приоритеты потоков, пере-
   открытие устройства при hot-plug (SDL_AUDIODEVICEREMOVED).

### Грабли, о которых уже знаем
- Волновые контейнеры PC (XSA/XWC) могут иметь версии новее снапшота — как с
  BSP; при первом же `Wave_Precache` смотреть версии заголовков (лог + bt).
- `MRTC_THREAD_PRIO_TIMECRITICAL` на Linux — обычный поток (у нас нет RT-прав);
  callback SDL и так живёт в потоке SDL.
- Микшер использует VPU-абстракцию задач — на Linux уже работает через
  CPU-путь (VPUManager застаблен), отдельной работы не требует.
- Все ассерты движка у нас лог-и-продолжить (`RIDDICK_ASSERT_FATAL=1` вернёт стоп).

Оценка: M0-M1 ~300-500 строк; M2-M3 — основная работа, ориентир 1-2k строк
(большинство — адаптация логики Thread_Read/Submit из MSound_PS3.cpp).
