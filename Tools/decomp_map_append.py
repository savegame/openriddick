#!/usr/bin/env python3
"""
Генерирует markdown-фрагмент с автосопоставленными парами (все три типа
якорей: classmethod/classname/command) для ручного append в
Docs/Decomp_Map.md. Печатает готовый Markdown в stdout — вызывающий
append'ит через `>> Docs/Decomp_Map.md`.

Разделяет однозначные (литерал встретился ровно в одном месте исходника)
и неоднозначные (несколько мест — обычно платформенные дубли
MRTC_System_Win32/PS3/Linux.cpp) пары.
"""
import os
import collections

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT_DIR = os.path.join(ROOT, "Tools", "out")


def load_tsv(name):
    path = os.path.join(OUT_DIR, name)
    with open(path, encoding="utf-8") as f:
        header = f.readline().rstrip("\n").split("\t")
        for line in f:
            vals = line.rstrip("\n").split("\t")
            if len(vals) != len(header):
                continue
            yield dict(zip(header, vals))


def dllshort(dll):
    return dll.replace("_Win32_x86_dll_decomp.c", "").replace("_dll_decomp.c", "")


def section_classmethod():
    rows = list(load_tsv("matched_pairs.tsv"))
    by_lit = collections.defaultdict(list)
    for r in rows:
        by_lit[r["literal"]].append(r)

    unambiguous = []
    ambiguous = []
    for lit, rs in sorted(by_lit.items()):
        srcs = {(r["src_file"], r["src_line"]) for r in rs}
        funs = {(r["dllfile"], r["fun"]) for r in rs if r["fun"] and r["fun"] != "?"}
        if not funs:
            continue
        if len(srcs) == 1:
            (sf, sl), = srcs
            for dll, fun in sorted(funs):
                unambiguous.append((fun, dllshort(dll), lit, sf, sl))
        else:
            files = "; ".join(sorted(f"{sf}:{sl}" for sf, sl in srcs))
            for dll, fun in sorted(funs):
                ambiguous.append((fun, dllshort(dll), lit, files))

    out = []
    out.append("\n### Автосопоставление по якорю \"Класс::Метод\" — однозначные (этап A/C)\n")
    out.append("\nЛитерал встретился ровно в одном месте исходника — привязка к")
    out.append(" конкретному `.cpp:line` надёжна. Сгенерировано `Tools/decomp_match.py`")
    out.append(" + `Tools/decomp_map_append.py`, семантика тела функции НЕ сверялась")
    out.append(" построчно (см. `Docs/Decomp_Coverage.md`).\n")
    out.append("\n| FUN | Декомпил | Класс::Метод | Файл:строка исходника |")
    out.append("\n|---|---|---|---|")
    for fun, dll, lit, sf, sl in unambiguous:
        out.append(f"\n| `FUN_{fun}` | {dll} | `{lit}` | `{sf}:{sl}` |")
    out.append("\n")

    out.append("\n### Автосопоставление по якорю \"Класс::Метод\" — неоднозначные (этап A/C)\n")
    out.append("\nЛитерал совпадает с несколькими местами исходника (обычно один и тот же")
    out.append(" `Error_static(\"Класс::Метод\", ...)`, продублированный под разные платформы")
    out.append(" — `MRTC_System_Win32.cpp`/`_PS3.cpp`/`_Linux.cpp`). Имя метода подтверждено,")
    out.append(" конкретный `.cpp` — нет.\n")
    out.append("\n| FUN | Декомпил | Класс::Метод | Кандидаты в исходнике |")
    out.append("\n|---|---|---|---|")
    for fun, dll, lit, files in ambiguous:
        out.append(f"\n| `FUN_{fun}` | {dll} | `{lit}` | {files} |")
    out.append("\n")
    return "".join(out), len(unambiguous), len(ambiguous)


def section_simple(fname, title, note):
    rows = list(load_tsv(fname))
    by_lit_fun = collections.defaultdict(set)
    src_files = collections.defaultdict(set)
    for r in rows:
        if not r["fun"] or r["fun"] == "?":
            continue
        by_lit_fun[r["literal"]].add((dllshort(r["dllfile"]), r["fun"]))
        src_files[r["literal"]].add(f"{r['src_file']}:{r['src_line']}")

    out = [f"\n### {title}\n", f"\n{note}\n"]
    out.append("\n| Литерал | Декомпил:FUN | Файл:строка исходника |")
    out.append("\n|---|---|---|")
    n = 0
    for lit in sorted(by_lit_fun):
        funs = ", ".join(f"{dll}:`FUN_{fu}`" for dll, fu in sorted(by_lit_fun[lit]))
        files = "; ".join(sorted(src_files[lit]))
        out.append(f"\n| `{lit}` | {funs} | {files} |")
        n += 1
    out.append("\n")
    return "".join(out), n


def main():
    print("\n---\n")
    print("## Автосопоставление, этапы A/C (продолжение исследования покрытия)\n")
    print("Полная методика и метрики — `Docs/Decomp_Coverage.md`. Ниже —")
    print("**все** пары, которые скрипты нашли автоматически по трём типам")
    print("якорей (classmethod/classname/command), без ручной построчной")
    print("сверки тела функции — то есть это подтверждение **факта присутствия**")
    print("(имя метода/класса/команды опознано в конкретной `FUN_xxxxxxxx`),")
    print("не подтверждение идентичности логики.\n")

    cm_md, n_unamb, n_amb = section_classmethod()
    print(cm_md)
    print(f"\n*(классметод: {n_unamb} однозначных пар, {n_amb} неоднозначных)*\n")

    cn_md, n_cn = section_simple(
        "matched_classnames.tsv",
        "Автосопоставление по якорю \"имя класса\" (MRTC_IMPLEMENT_*)",
        "Литерал — первый аргумент `MRTC_IMPLEMENT_DYNAMIC`/`MRTC_IMPLEMENT_SERIAL_WOBJECT`/"
        "аналогов. Совпадение имени класса в декомпиле — как правило, это"
        " конструктор/RTTI-thunk этого класса, не факт что FUN == сам конструктор"
        " один в один (не сверено вручную).",
    )
    print(cn_md)
    print(f"\n*(classname: {n_cn} пар)*\n")

    co_md, n_co = section_simple(
        "matched_commands.tsv",
        "Автосопоставление по якорю \"консольная команда\" (RegFunction)",
        "Литерал — имя консольной команды из `RegFunction(\"...\")`. Многие"
        " совпадения — `fun=?` (строка вне обнаруженной функции — похоже,"
        " статическая таблица команд в `.rdata`, а не код), такие строки в"
        " таблицу ниже не попали (нет привязки к FUN).",
    )
    print(co_md)
    print(f"\n*(command: {n_co} пар)*\n")


if __name__ == "__main__":
    main()
