#!/usr/bin/env python3
"""
Граф вызовов декомпила + распространение опознания по графу (этапы 1-2
следующего прохода поверх Docs/Decomp_Coverage.md / Decomp_Map.md).

Границы функций декомпила НЕ пересчитываются заново — берутся из
Tools/out/decomp_funcs.tsv, который строит decomp_inventory.py той же
state-машиной (сигнатура FUN_xxxxxxxx в начале строки -> '{' -> баланс
скобок до '}' в столбце 0). Здесь это единственный источник границ,
переиспользуется как есть (см. AGENTS.md — "не изобретай заново").

Тоже переиспользуется: DEF_RE/BAD_PREFIXES/FUN_ANY_RE из
decomp_inventory.py — тот же regex для "Class::Method(" что используется
и для разбора исходника (там как определения), и здесь для разбора
декомпила (там это вызовы/резолвленные импорты) и тела метода исходника
(вызовы).

Режимы:
  python3 Tools/decomp_callgraph.py graph      -> строит граф, пишет
      Tools/out/callgraph_out_fun.tsv    (dllfile, fun, callee_fun)
      Tools/out/callgraph_out_named.tsv  (dllfile, fun, callee_name)
      Tools/out/callgraph_in_fun.tsv     (dllfile, fun, n_callers, callers[:20])
      Tools/out/callgraph_stats.txt      (агрегаты)
  python3 Tools/decomp_callgraph.py propagate  -> строит граф в памяти +
      применяет этап 2, пишет
      Tools/out/callgraph_inferred_pairs.tsv
  python3 Tools/decomp_callgraph.py all        -> оба режима одним проходом
      (граф строится один раз, используется для обоих этапов).
"""
import os
import sys
import collections
import bisect

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC_ROOT = os.path.join(ROOT, "Source", "P5")
OUT_DIR = os.path.join(ROOT, "Tools", "out")

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from decomp_inventory import (  # noqa: E402  (переиспользуем как просили)
    DECOMP_FILES, DEF_RE, BAD_PREFIXES, FUN_ANY_RE,
)


# ---------------------------------------------------------------------------
# Этап 1: граф вызовов
# ---------------------------------------------------------------------------

def load_func_boundaries():
    """dllfile -> (starts[], ends[], funs[]) отсортировано по starts,
    как их пишет decomp_inventory.py (в порядке появления в файле)."""
    per_starts = collections.defaultdict(list)
    per_ends = collections.defaultdict(list)
    per_funs = collections.defaultdict(list)
    path = os.path.join(OUT_DIR, "decomp_funcs.tsv")
    if not os.path.exists(path):
        print("ОШИБКА: Tools/out/decomp_funcs.tsv не найден — сначала "
              "python3 Tools/decomp_inventory.py decomp", file=sys.stderr)
        sys.exit(1)
    with open(path, encoding="utf-8") as f:
        next(f)
        for line in f:
            dllfile, fun, s, e, nc = line.rstrip("\n").split("\t")
            per_starts[dllfile].append(int(s))
            per_ends[dllfile].append(int(e))
            per_funs[dllfile].append(fun)
    return per_starts, per_ends, per_funs


def build_graph():
    """Возвращает per-dllfile: outgoing_fun[fun]=set(callee_fun),
    outgoing_named[fun]=set("Class::Method"), incoming_fun[fun]=set(caller_fun).
    Один линейный проход на файл (та же схема, что anchors-проход в
    decomp_inventory.py: bisect-курсор по границам функций)."""
    per_starts, per_ends, per_funs = load_func_boundaries()
    graph = {}  # dllfile -> dict(outgoing_fun, outgoing_named, incoming_fun)

    for dllfile in DECOMP_FILES:
        path = os.path.join(ROOT, dllfile)
        if not os.path.exists(path):
            continue
        starts = per_starts.get(dllfile, [])
        ends = per_ends.get(dllfile, [])
        funs = per_funs.get(dllfile, [])
        n_funcs = len(starts)
        outgoing_fun = collections.defaultdict(set)
        outgoing_named = collections.defaultdict(set)

        idx = 0
        with open(path, "r", encoding="utf-8", errors="replace") as f:
            lineno = 0
            for line in f:
                lineno += 1
                if "FUN_" not in line and "::" not in line:
                    continue
                while idx < n_funcs and lineno > ends[idx]:
                    idx += 1
                if idx >= n_funcs or lineno < starts[idx]:
                    continue
                cur = funs[idx]
                if "FUN_" in line:
                    for cm in FUN_ANY_RE.finditer(line):
                        fid = cm.group(1)
                        if fid != cur:
                            outgoing_fun[cur].add(fid)
                if "::" in line:
                    for m in DEF_RE.finditer(line):
                        cls, meth = m.group(1), m.group(2)
                        if cls in BAD_PREFIXES or meth in BAD_PREFIXES:
                            continue
                        outgoing_named[cur].add(f"{cls}::{meth}")

        incoming_fun = collections.defaultdict(set)
        for caller, callees in outgoing_fun.items():
            for callee in callees:
                incoming_fun[callee].add(caller)

        graph[dllfile] = dict(
            outgoing_fun=outgoing_fun,
            outgoing_named=outgoing_named,
            incoming_fun=incoming_fun,
            all_funs=funs,
        )
        print(f"[graph] {dllfile}: funcs={n_funcs} "
              f"with_fun_callees={len(outgoing_fun)} "
              f"with_named_callees={len(outgoing_named)}")
    return graph


def write_graph_outputs(graph):
    out_fun = open(os.path.join(OUT_DIR, "callgraph_out_fun.tsv"), "w")
    out_named = open(os.path.join(OUT_DIR, "callgraph_out_named.tsv"), "w")
    in_fun = open(os.path.join(OUT_DIR, "callgraph_in_fun.tsv"), "w")
    out_fun.write("dllfile\tfun\tcallee_fun\n")
    out_named.write("dllfile\tfun\tcallee_name\n")
    in_fun.write("dllfile\tfun\tn_callers\tcallers_sample\n")

    total_funcs = 0
    total_leaf = 0          # 0 исходящих FUN_-вызовов
    total_indeg0 = 0        # ни разу не вызвана другой FUN_ в том же файле
    total_indeg1 = 0        # ровно один вызывающий
    outdeg_hist = collections.Counter()
    indeg_hist = collections.Counter()

    for dllfile, g in graph.items():
        for fun, callees in g["outgoing_fun"].items():
            for c in callees:
                out_fun.write(f"{dllfile}\t{fun}\t{c}\n")
        for fun, names in g["outgoing_named"].items():
            for c in names:
                out_named.write(f"{dllfile}\t{fun}\t{c}\n")

        all_funs = g["all_funs"]
        total_funcs += len(all_funs)
        for fun in all_funs:
            outdeg = len(g["outgoing_fun"].get(fun, ()))
            indeg = len(g["incoming_fun"].get(fun, ()))
            outdeg_hist[outdeg if outdeg < 10 else 10] += 1
            indeg_hist[indeg if indeg < 10 else 10] += 1
            if outdeg == 0:
                total_leaf += 1
            if indeg == 0:
                total_indeg0 += 1
            if indeg == 1:
                total_indeg1 += 1
            callers = sorted(g["incoming_fun"].get(fun, ()))
            in_fun.write(f"{dllfile}\t{fun}\t{indeg}\t{','.join(callers[:20])}\n")

    out_fun.close()
    out_named.close()
    in_fun.close()

    stats_lines = []
    stats_lines.append(f"total_funcs={total_funcs}")
    stats_lines.append(f"leaf(out-degree FUN_ == 0)={total_leaf} "
                        f"({100.0*total_leaf/total_funcs:.1f}%)")
    stats_lines.append(f"in-degree==0 (никем не вызвана как FUN_ в своём файле)="
                        f"{total_indeg0} ({100.0*total_indeg0/total_funcs:.1f}%)")
    stats_lines.append(f"in-degree==1 (ровно один вызывающий)={total_indeg1} "
                        f"({100.0*total_indeg1/total_funcs:.1f}%)")
    stats_lines.append("out-degree histogram (0..9,10+): " +
                        ",".join(str(outdeg_hist.get(k, 0)) for k in range(11)))
    stats_lines.append("in-degree  histogram (0..9,10+): " +
                        ",".join(str(indeg_hist.get(k, 0)) for k in range(11)))

    with open(os.path.join(OUT_DIR, "callgraph_stats.txt"), "w") as f:
        f.write("\n".join(stats_lines) + "\n")
    for l in stats_lines:
        print("[stats] " + l)


# ---------------------------------------------------------------------------
# Этап 2: распространение опознания
# ---------------------------------------------------------------------------

def load_known_pairs():
    """(dllfile,fun) -> class_method ; class_method -> (dllfile,fun,src_file,src_line)
    Источник: Tools/out/matched_pairs.tsv (222 пары, подтверждённые строковыми
    якорями — см. Docs/Decomp_Coverage.md). Это ровно множество "известных
    пар", от которого просили расти по графу."""
    path = os.path.join(OUT_DIR, "matched_pairs.tsv")
    fun_to_cm = {}
    cm_to_fun = {}
    with open(path, encoding="utf-8") as f:
        next(f)
        for line in f:
            parts = line.rstrip("\n").split("\t")
            if len(parts) != 7:
                continue
            literal, cm, src_file, src_line, dllfile, fun, decomp_line = parts
            fun_to_cm[(dllfile, fun)] = cm
            # может быть несколько src-определений (оверлоады/дубликаты
            # литерала) — берём первое стабильно
            cm_to_fun.setdefault(cm, (dllfile, fun, src_file, src_line))
    return fun_to_cm, cm_to_fun


def load_source_method_defs():
    """class::method -> list[(file,line)] из source_methods.tsv."""
    path = os.path.join(OUT_DIR, "source_methods.tsv")
    d = collections.defaultdict(list)
    with open(path, encoding="utf-8") as f:
        next(f)
        for line in f:
            parts = line.rstrip("\n").split("\t")
            if len(parts) != 5:
                continue
            module, cls, meth, file, line_no = parts
            d[f"{cls}::{meth}"].append((file, line_no))
    return d


_body_cache = {}


def extract_body_calls(file_rel, start_line, self_cm):
    """Множество "Class::Method" строк, встреченных как ВЫЗОВЫ (текстово —
    паттерн Class::Method() внутри тела функции по грепу, как просили)
    в теле метода self_cm, определённого в file_rel начиная со start_line.
    """
    key = (file_rel, start_line)
    if key in _body_cache:
        return _body_cache[key]
    path = os.path.join(SRC_ROOT, file_rel)
    calls = set()
    try:
        with open(path, "r", encoding="utf-8", errors="replace") as f:
            lines = f.readlines()
    except OSError:
        _body_cache[key] = calls
        return calls

    n = len(lines)
    i = start_line - 1
    cap = min(n, start_line + 4000)
    started = False
    depth = 0
    j = i
    while j < cap:
        line = lines[j]
        if not started:
            if "{" not in line:
                if ";" in line:
                    break  # прототип/декларация без тела
                j += 1
                continue
            started = True
        depth += line.count("{") - line.count("}")
        for m in DEF_RE.finditer(line):
            cls, meth = m.group(1), m.group(2)
            if cls in BAD_PREFIXES or meth in BAD_PREFIXES:
                continue
            calls.add(f"{cls}::{meth}")
        if started and depth <= 0:
            break
        j += 1

    calls.discard(self_cm)
    _body_cache[key] = calls
    return calls


def propagate(graph):
    fun_to_cm, cm_to_fun = load_known_pairs()
    src_defs = load_source_method_defs()
    known_cm_set = set(cm_to_fun.keys())

    out_path = os.path.join(OUT_DIR, "callgraph_inferred_pairs.tsv")
    out = open(out_path, "w")
    out.write("dllfile\tfun_x\tinferred_class_method\tsrc_file\tsrc_line\t"
              "evidence_caller_fun\tevidence_caller_cm\tnote\n")

    n_inferred = 0
    n_candidates_single_caller = 0
    for dllfile, g in graph.items():
        incoming = g["incoming_fun"]
        outgoing_named = g["outgoing_named"]
        for fun_x in g["all_funs"]:
            if (dllfile, fun_x) in fun_to_cm:
                continue  # уже известна
            callers = incoming.get(fun_x, ())
            if len(callers) != 1:
                continue
            fun_y = next(iter(callers))
            cm_y = fun_to_cm.get((dllfile, fun_y))
            if cm_y is None:
                continue  # вызывающий сам не опознан
            n_candidates_single_caller += 1

            # опознанные декомпилом именованные вызовы FUN_Y тоже могут
            # исключать кандидатов (если Y вызывает и по имени тот же
            # класс/метод, что и по FUN_ — противоречие, пропускаем)
            _dll, _fy, src_file, src_line = cm_to_fun[cm_y]
            src_calls = extract_body_calls(src_file, int(src_line), cm_y)
            candidates = src_calls - known_cm_set
            if len(candidates) != 1:
                continue
            target = next(iter(candidates))
            defs = src_defs.get(target)
            if not defs:
                continue  # нет определения в source_methods.tsv (только объявление?)
            note = "ok" if len(defs) == 1 else f"AMBIGUOUS({len(defs)} defs, взято первое)"
            tfile, tline = defs[0]
            out.write(f"{dllfile}\t{fun_x}\t{target}\t{tfile}\t{tline}\t"
                      f"{fun_y}\t{cm_y}\t{note}\n")
            n_inferred += 1

    out.close()
    print(f"[propagate] кандидатов с единственным известным вызывающим="
          f"{n_candidates_single_caller}")
    print(f"[propagate] выведено однозначных пар (FUN_x <-> Class::Method)="
          f"{n_inferred} -> {out_path}")


if __name__ == "__main__":
    os.makedirs(OUT_DIR, exist_ok=True)
    mode = sys.argv[1] if len(sys.argv) > 1 else "all"
    graph = build_graph()
    if mode in ("graph", "all"):
        write_graph_outputs(graph)
    if mode in ("propagate", "all"):
        propagate(graph)
