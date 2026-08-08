#!/usr/bin/env python3
"""
Этап 2 (Docs/Research_DecompCoverage.md): сопоставление исходник<->декомпил
по якорям-литералам "Класс::Метод" (Error_static-стиль, самый надёжный
признак по §2.3) + метрики покрытия по модулям.

Вход:  Tools/out/source_methods.tsv, source_anchors.tsv,
       Tools/out/decomp_funcs.tsv,   decomp_anchors.tsv
       (генерируются decomp_inventory.py)
       Docs/Decomp_Map.md — уже подтверждённые вручную пары (FUN_xxxxxxxx)
Выход: Tools/out/matched_pairs.tsv       — literal, source class::method, file:line, dllfile, FUN, line
       Tools/out/coverage_by_module.tsv  — module, src_methods, src_matched, decomp_funcs, decomp_matched
"""
import os
import re
import collections

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT_DIR = os.path.join(ROOT, "Tools", "out")

# decomp dllfile -> наш(и) исходный(е) модуль(и), для сведения метрик.
DLL_TO_MODULES = {
    "GameClasses_Win32_x86_dll_decomp.c": {"GameClasses"},
    "GameWorld_Win32_x86_dll_decomp.c": {"GameWorld", "GameWorld_shared", "Classes"},
    "MXR_dll_decomp.c": {"XR", "XRModels"},
    "MSystem_dll_decomp.c": {"MSystem"},
    "RndrGL_dll_decomp.c": set(),  # нет аналога в нашем PS3-снапшоте
    "MCCDyn_dll_decomp.c": {"MCC"},
}


def load_tsv(name):
    path = os.path.join(OUT_DIR, name)
    with open(path, encoding="utf-8") as f:
        header = f.readline().rstrip("\n").split("\t")
        for line in f:
            vals = line.rstrip("\n").split("\t")
            yield dict(zip(header, vals))


def main():
    # --- source side ---
    src_methods = list(load_tsv("source_methods.tsv"))
    # literal -> list of source method rows sharing this "Class::Method" text
    src_lit_index = collections.defaultdict(list)
    for row in load_tsv("source_anchors.tsv"):
        src_lit_index[row["literal"]].append(row)

    # set of "Class::Method" literal strings actually present as a source
    # method definition too (defensive cross-check == same literal derived
    # from Error_static("Class::Method", ...) usually names the *enclosing*
    # method, so it should match a real Class::Method in source_methods).
    src_method_set = {f'{r["class"]}::{r["method"]}' for r in src_methods}

    # --- decomp side ---
    decomp_funcs = list(load_tsv("decomp_funcs.tsv"))
    fun_count_by_dll = collections.Counter(r["dllfile"] for r in decomp_funcs)

    decomp_classmethod = [r for r in load_tsv("decomp_anchors.tsv") if r["kind"] == "classmethod"]

    # already manually confirmed pairs (Docs/Decomp_Map.md) — fold into the
    # "identified" set for decomp coverage accounting.
    confirmed_funs = set()
    map_path = os.path.join(ROOT, "Docs", "Decomp_Map.md")
    if os.path.exists(map_path):
        with open(map_path, encoding="utf-8") as f:
            text = f.read()
        confirmed_funs = set(re.findall(r"FUN_[0-9a-fA-F]{8}", text))

    # --- matching ---
    matched_out = open(os.path.join(OUT_DIR, "matched_pairs.tsv"), "w")
    matched_out.write("literal\tsrc_class_method\tsrc_file\tsrc_line\tdllfile\tfun\tdecomp_line\n")

    matched_decomp_funs = collections.defaultdict(set)  # dllfile -> set(fun)
    matched_src_methods = set()  # "Class::Method" strings confirmed via anchor

    n_pairs = 0
    for row in decomp_classmethod:
        lit = row["literal"]
        if lit not in src_method_set:
            continue  # literal doesn't correspond to any known Class::Method in source
        srcs = [r for r in src_methods if f'{r["class"]}::{r["method"]}' == lit]
        for s in srcs:
            matched_out.write(
                f"{lit}\t{lit}\t{s['file']}\t{s['line']}\t{row['dllfile']}\t{row['fun']}\t{row['line']}\n"
            )
            n_pairs += 1
        matched_decomp_funs[row["dllfile"]].add(row["fun"])
        matched_src_methods.add(lit)
    matched_out.close()

    # fold in confirmed pairs from Decomp_Map.md as decomp-side identified,
    # regardless of which dllfile (we don't reliably know without deeper
    # parsing here; count them once globally for the "manual" line).
    manual_confirmed_count = len(confirmed_funs)
    manual_confirmed_auto_overlap = sum(
        1 for s in matched_decomp_funs.values() for fu in s if fu in confirmed_funs
    )

    # --- per-module metrics ---
    cov_out = open(os.path.join(OUT_DIR, "coverage_by_module.tsv"), "w")
    cov_out.write("module\tsrc_methods\tsrc_matched_by_anchor\tsrc_match_pct\n")
    by_module_total = collections.Counter(r["module"] for r in src_methods)
    by_module_matched = collections.Counter()
    for r in src_methods:
        key = f'{r["class"]}::{r["method"]}'
        if key in matched_src_methods:
            by_module_matched[r["module"]] += 1
    for module in sorted(by_module_total):
        tot = by_module_total[module]
        mat = by_module_matched[module]
        pct = 100.0 * mat / tot if tot else 0.0
        cov_out.write(f"{module}\t{tot}\t{mat}\t{pct:.2f}\n")
    cov_out.close()

    dll_cov_out = open(os.path.join(OUT_DIR, "coverage_by_dll.tsv"), "w")
    dll_cov_out.write("dllfile\tfun_total\tfun_matched_by_anchor\tfun_matched_pct\tfun_confirmed_manual_in_map\n")
    for dll in DLL_TO_MODULES:
        tot = fun_count_by_dll.get(dll, 0)
        mat = len(matched_decomp_funs.get(dll, set()))
        pct = 100.0 * mat / tot if tot else 0.0
        dll_cov_out.write(f"{dll}\t{tot}\t{mat}\t{pct:.2f}\t?\n")
    dll_cov_out.close()

    print(f"pairs_written={n_pairs}")
    print(f"distinct_matched_src_methods={len(matched_src_methods)}")
    print(f"decomp_funs_identified_by_anchor_total={sum(len(v) for v in matched_decomp_funs.values())}")
    print(f"manual_confirmed_in_Decomp_Map={manual_confirmed_count} (overlap_with_auto={manual_confirmed_auto_overlap})")
    for dll in DLL_TO_MODULES:
        print(f"  {dll}: FUN_total={fun_count_by_dll.get(dll,0)} matched_by_anchor={len(matched_decomp_funs.get(dll, set()))}")


if __name__ == "__main__":
    main()
