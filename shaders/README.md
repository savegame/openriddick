# shaders/ — оригинальные шейдерные ресурсы PC-версии

Файлы в этой папке извлечены из `System/GL/` установленной PC-версии игры
(The Chronicles of Riddick: Assault on Dark Athena / Escape from Butcher Bay).
Это **справочный материал для порта**, не часть сборки CMake — ничего отсюда
не компилируется и не подключается движком напрямую.

- `VP.xrg` — шаблон вершинного конвейера (DSL `CRC_VPGenerator`), из которого
  оригинальный движок на PC/PS3 генерирует конкретные ARB/Cg вершинные программы
  под каждую комбинацию `CRC_VPFormat`. Разбор см. `Docs/VP_Reference.md`.
- `EXT_Shading_Language_100/GLSL_TexEnv{0..4,Alpha}.fp` — фрагментные шейдеры
  фиксed-function эмуляции texture-environment комбайнеров (modulate/decal по
  числу активных текстурных стадий).

Итоговые GLSL ES 3.00 шейдеры нашего порта живут в
`Source/P5/Shared/MOS/RenderContexts/GLES3/` и пишутся по мотивам этих файлов,
а не берутся из них напрямую (другой язык, другая конвенция констант).
