#!/usr/bin/env python3
"""
Этап 3: грубая карта "крупных неопознанных областей" декомпила.

Бьёт функции каждого декомпила на N бакетов (по порядку появления в
файле == порядку адресов у Ghidra-вывода в этом дереве), для каждого
бакета считает долю функций, опознанных якорем "Класс::Метод"
(matched_pairs.tsv/decomp_anchors classmethod), и для НЕопознанных
бакетов с наибольшим числом функций выводит top-N самых частых generic-
строковых литералов внутри — как подсказку для гипотезы "что здесь".

Вход:  Tools/out/decomp_funcs.tsv, decomp_anchors.tsv
Выход: Tools/out/regions_<dllfile>.tsv (bucket, fun_count, classmethod_hits,
       top_strings) + печать сводки по крупнейшим "тёмным" бакетам.
"""
import os
import collections

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT_DIR = os.path.join(ROOT, "Tools", "out")
NBUCKETS = 20


def load_tsv(name):
    path = os.path.join(OUT_DIR, name)
    with open(path, encoding="utf-8") as f:
        header = f.readline().rstrip("\n").split("\t")
        for line in f:
            vals = line.rstrip("\n").split("\t")
            yield dict(zip(header, vals))


def main():
    funcs_by_dll = collections.defaultdict(list)
    for r in load_tsv("decomp_funcs.tsv"):
        funcs_by_dll[r["dllfile"]].append(r["fun"])

    anchors_by_fun = collections.defaultdict(list)  # (dllfile,fun) -> [ (literal,kind) ]
    for r in load_tsv("decomp_anchors.tsv"):
        anchors_by_fun[(r["dllfile"], r["fun"])].append((r["literal"], r["kind"]))

    summary_lines = []
    for dllfile, funlist in funcs_by_dll.items():
        n = len(funlist)
        bucket_size = max(1, n // NBUCKETS)
        out_path = os.path.join(OUT_DIR, f"regions_{dllfile}.tsv")
        with open(out_path, "w") as out:
            out.write("bucket\tfun_range\tfun_count\tclassmethod_hits\ttop_strings\n")
            for b in range(0, n, bucket_size):
                chunk = funlist[b:b + bucket_size]
                if not chunk:
                    continue
                cm_hits = 0
                str_counter = collections.Counter()
                for fu in chunk:
                    for lit, kind in anchors_by_fun.get((dllfile, fu), []):
                        if kind == "classmethod":
                            cm_hits += 1
                        else:
                            str_counter[lit] += 1
                top = ", ".join(f"{s!r}x{c}" for s, c in str_counter.most_common(6))
                out.write(
                    f"{b//bucket_size}\t{chunk[0]}..{chunk[-1]}\t{len(chunk)}\t{cm_hits}\t{top}\n"
                )
                if cm_hits == 0 and str_counter:
                    total_hits = sum(str_counter.values())
                    summary_lines.append((dllfile, b // bucket_size, len(chunk), total_hits, top))

    # print the "darkest but richest" buckets (0 classmethod hits, most generic anchors)
    summary_lines.sort(key=lambda t: -t[3])
    print("dllfile\tbucket\tfun_count\tgeneric_anchor_count\ttop_strings")
    for row in summary_lines[:25]:
        print("\t".join(str(x) for x in row))


if __name__ == "__main__":
    main()
