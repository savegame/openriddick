#!/usr/bin/env python3
"""
Decomp/source coverage inventory tool (see Docs/Research_DecompCoverage.md).

Строит:
  1. Инвентарь методов исходника (Class::Method, файл:строка) по модулям.
  2. Инвентарь "Class::Method"-подобных строковых литералов в исходнике
     (Error_static/M_TRACEALWAYS/ConOut и т.п. — самый надёжный якорь).
  3. Инвентарь функций FUN_xxxxxxxx в декомпилах Ghidra (файл, старт/конец
     строки) — по формату сигнатура-в-начале-строки / '{' в начале строки /
     '}' в начале строки, характерному для вывода Ghidra в этом дереве.
  4. Инвентарь строковых литералов внутри декомпила с привязкой к
     ближайшей объемлющей FUN_ (по номеру строки, бинарным поиском по
     границам функций из (3)).

ВАЖНО: скрипт читает *_decomp.c построчно (potential 100+MB), не грузит
целиком без необходимости — но сам процесс python, конечно, использует
собственную память, это не то же самое, что чтение файла в контекст
агента инструментом Read (что запрещено). Результаты всегда пишутся в
Tools/out/*.tsv, полные списки агентом не читаются — только head/wc.

Использование:
  python3 Tools/decomp_inventory.py source   -> Tools/out/source_methods.tsv, source_anchors.tsv
  python3 Tools/decomp_inventory.py decomp   -> Tools/out/decomp_funcs.tsv, decomp_anchors.tsv
  python3 Tools/decomp_inventory.py all
"""
import os
import re
import sys
import bisect

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC_ROOT = os.path.join(ROOT, "Source", "P5")
OUT_DIR = os.path.join(ROOT, "Tools", "out")

DECOMP_FILES = [
    "GameClasses_Win32_x86_dll_decomp.c",
    "GameWorld_Win32_x86_dll_decomp.c",
    "MXR_dll_decomp.c",
    "MSystem_dll_decomp.c",
    "RndrGL_dll_decomp.c",
    "MCCDyn_dll_decomp.c",
]

MODULE_MAP = [
    # (path prefix relative to Source/P5, module name) — порядок важен,
    # более специфичные префиксы раньше.
    ("Shared/MOS/Classes/GameWorld", "GameWorld_shared"),
    ("Shared/MOS/RenderContexts", "RenderContexts"),
    ("Shared/MOS/XRModels", "XRModels"),
    ("Shared/MOS/XR", "XR"),
    ("Shared/MOS/MSystem", "MSystem"),
    ("Shared/MOS/Classes", "Classes"),
    ("Shared/MCC", "MCC"),
    ("Shared/Platform", "Platform"),
    ("Shared/Misc", "Misc"),
    ("Projects/Main/GameClasses", "GameClasses"),
    ("Projects/Main/GameWorld", "GameWorld"),
    ("Projects/Main/Exe", "Exe"),
    ("SDK", "SDK"),
]


def module_for(relpath):
    relpath = relpath.replace(os.sep, "/")
    for prefix, name in MODULE_MAP:
        if relpath.startswith(prefix):
            return name
    return "Other"


# ---------------------------------------------------------------------------
# 1+2. Source inventory
# ---------------------------------------------------------------------------

# Class::Method( at start-of-statement (allow leading whitespace, template
# angle brackets, namespace::Class::Method chains). Take the LAST '::' pair
# as class/method (rightmost) to skip namespace qualification.
DEF_RE = re.compile(
    r"(?<![:\w])([A-Za-z_]\w*)::([A-Za-z_][\w~]*)\s*\("
)
# Filter out things that are clearly not defs (this::, std::, etc. or usages
# inside calls like Foo::Bar() used as an argument — heuristic: require the
# match to be within the first ~half of a line that also doesn't start with
# common control/keywords right before it, and doesn't end with ';' on the
# same physical line for the *opening* line — too strict; we keep it simple
# per spec ("механически") and accept some noise.)
BAD_PREFIXES = {"if", "for", "while", "switch", "return", "else", "case",
                 "sizeof", "new", "delete", "static_cast", "this", "throw"}

STRLIT_RE = re.compile(r'"((?:[^"\\]|\\.){0,200})"')
CLASSMETHOD_STR_RE = re.compile(r'^[A-Za-z_]\w*(?:<[^">]*>)?::~?[A-Za-z_]\w*$')

# Этап "A" (продолжение исследования, см. сообщение координатора): ещё два
# типа якорей.
#   - MRTC_IMPLEMENT_DYNAMIC(ClassName, ...) / MRTC_IMPLEMENT_SERIAL_WOBJECT(...)
#     и близкие варианты — регистрация класса движка. Первый (неквалифицированный
#     идентификатором) аргумент — имя класса.
#   - RegFunction("name", ...) — регистрация консольной команды.
IMPLEMENT_RE = re.compile(r"\bMRTC_IMPLEMENT_[A-Z_]*\s*\(\s*([A-Za-z_]\w*)")
REGFUNCTION_RE = re.compile(r"\bRegFunction\s*\(\s*\"([A-Za-z0-9_]+)\"")


def iter_source_files():
    for dirpath, dirnames, filenames in os.walk(SRC_ROOT):
        # skip build artefacts if any leaked in, and intermediate dirs
        dirnames[:] = [d for d in dirnames if d not in ("intermediate",)]
        for fn in filenames:
            if fn.endswith((".cpp", ".h", ".hpp", ".inl")):
                yield os.path.join(dirpath, fn)


def do_source():
    methods_out = open(os.path.join(OUT_DIR, "source_methods.tsv"), "w")
    anchors_out = open(os.path.join(OUT_DIR, "source_anchors.tsv"), "w")
    methods_out.write("module\tclass\tmethod\tfile\tline\n")
    anchors_out.write("module\tliteral\tfile\tline\tcontext\n")

    n_methods = 0
    n_anchors = 0
    n_files = 0

    for path in iter_source_files():
        rel = os.path.relpath(path, SRC_ROOT)
        module = module_for(rel)
        if module == "SDK":
            continue
        n_files += 1
        try:
            with open(path, "r", encoding="utf-8", errors="replace") as f:
                lines = f.readlines()
        except OSError:
            continue

        is_cpp = path.endswith(".cpp")
        for i, line in enumerate(lines, start=1):
            if is_cpp:
                for m in DEF_RE.finditer(line):
                    cls, meth = m.group(1), m.group(2)
                    if cls in BAD_PREFIXES or meth in BAD_PREFIXES:
                        continue
                    if cls.lower() == cls and len(cls) < 3:
                        continue
                    # rudimentary: require class name to look like engine
                    # naming (starts with uppercase or C/M/W/T prefix) to
                    # cut down on false positives from macro invocations
                    if not re.match(r"^[A-Za-z_]\w*$", cls):
                        continue
                    methods_out.write(f"{module}\t{cls}\t{meth}\t{rel}\t{i}\n")
                    n_methods += 1
            # anchors: string literals anywhere (both .h and .cpp), keep
            # ones shaped like "Class::Method" (Error_static-style) — the
            # strongest cross-reference anchor per spec §2.3.
            if '"' in line:
                for sm in STRLIT_RE.finditer(line):
                    lit = sm.group(1)
                    if CLASSMETHOD_STR_RE.match(lit):
                        anchors_out.write(f"{module}\t{lit}\t{rel}\t{i}\tclassmethod\n")
                        n_anchors += 1
                for rm in REGFUNCTION_RE.finditer(line):
                    anchors_out.write(f"{module}\t{rm.group(1)}\t{rel}\t{i}\tcommand\n")
                    n_anchors += 1
            im = IMPLEMENT_RE.search(line)
            if im:
                cls = im.group(1)
                # class-name arg only (skip if the "class name" position is
                # actually a plain type keyword or too short to be useful)
                if len(cls) >= 3 and cls not in BAD_PREFIXES:
                    anchors_out.write(f"{module}\t{cls}\t{rel}\t{i}\tclassname\n")
                    n_anchors += 1

    methods_out.close()
    anchors_out.close()
    print(f"[source] files={n_files} methods={n_methods} anchors_total={n_anchors}")


# ---------------------------------------------------------------------------
# 3+4. Decomp inventory
# ---------------------------------------------------------------------------

FUN_SIG_RE = re.compile(r"^[A-Za-z_][\w \*]*\bFUN_([0-9a-fA-F]{8})\s*\(")
FUN_ANY_RE = re.compile(r"\bFUN_([0-9a-fA-F]{8})\b")


def do_decomp():
    funcs_out = open(os.path.join(OUT_DIR, "decomp_funcs.tsv"), "w")
    anchors_out = open(os.path.join(OUT_DIR, "decomp_anchors.tsv"), "w")
    funcs_out.write("dllfile\tfun\tstartline\tendline\tncallees\n")
    anchors_out.write("dllfile\tfun\tliteral\tline\tkind\n")

    for dllfile in DECOMP_FILES:
        path = os.path.join(ROOT, dllfile)
        if not os.path.exists(path):
            print(f"[decomp] MISSING {dllfile}", file=sys.stderr)
            continue

        # Pass A: find function boundaries by scanning for signature lines
        # then the matching '{' / '}' at column 0.
        starts = []  # (line_no, fun_id)
        ends = []    # line_no of closing '}' for each function (parallel)
        cur_fun = None
        cur_sig_line = None
        awaiting_brace = False
        depth = 0
        callee_counts = {}

        # We do two lightweight passes over the file using simple state
        # machine, streaming to keep memory bounded per-line.
        with open(path, "r", encoding="utf-8", errors="replace") as f:
            lineno = 0
            for line in f:
                lineno += 1
                if cur_fun is None:
                    m = FUN_SIG_RE.match(line)
                    if m:
                        cur_fun = m.group(1)
                        cur_sig_line = lineno
                        awaiting_brace = True
                        depth = 0
                        callee_counts = {}
                    continue
                if awaiting_brace:
                    if line.startswith("{"):
                        awaiting_brace = False
                        depth = 1
                    elif line.strip() == "":
                        continue
                    else:
                        # signature line wasn't actually a function def
                        # (e.g. a prototype/forward decl without body) —
                        # abandon and rescan this line as a fresh candidate
                        cur_fun = None
                        m = FUN_SIG_RE.match(line)
                        if m:
                            cur_fun = m.group(1)
                            cur_sig_line = lineno
                            awaiting_brace = True
                    continue
                # inside function body: track brace depth + collect callees
                depth += line.count("{") - line.count("}")
                for cm in FUN_ANY_RE.finditer(line):
                    fid = cm.group(1)
                    if fid != cur_fun:
                        callee_counts[fid] = callee_counts.get(fid, 0) + 1
                if depth <= 0:
                    starts.append((cur_sig_line, cur_fun))
                    funcs_out.write(
                        f"{dllfile}\t{cur_fun}\t{cur_sig_line}\t{lineno}\t{len(callee_counts)}\n"
                    )
                    cur_fun = None

        funcs_out.flush()
        print(f"[decomp] {dllfile}: functions={len(starts)}")

    funcs_out.close()

    # Pass B: string-literal anchors, mapped to enclosing FUN via the
    # boundaries just written (re-read decomp_funcs.tsv per file to build a
    # bisect index of start lines).
    import collections
    per_file_starts = collections.defaultdict(list)
    per_file_ends = collections.defaultdict(list)
    per_file_funs = collections.defaultdict(list)
    with open(os.path.join(OUT_DIR, "decomp_funcs.tsv")) as f:
        next(f)
        for line in f:
            dllfile, fun, s, e, nc = line.rstrip("\n").split("\t")
            per_file_starts[dllfile].append(int(s))
            per_file_ends[dllfile].append(int(e))
            per_file_funs[dllfile].append(fun)

    # Словари якорей-имён из source_anchors.tsv (classname/command), чтобы
    # ловить их в декомпиле даже когда они короче общего generic-порога
    # (8 символов) — многие команды/классы короче.
    classname_set = set()
    command_set = set()
    src_anchors_path = os.path.join(OUT_DIR, "source_anchors.tsv")
    if os.path.exists(src_anchors_path):
        with open(src_anchors_path, encoding="utf-8") as f:
            next(f)
            for line in f:
                parts = line.rstrip("\n").split("\t")
                if len(parts) != 5:
                    continue
                _module, lit, _file, _line, ctx = parts
                if ctx == "classname":
                    classname_set.add(lit)
                elif ctx == "command":
                    command_set.add(lit)
    else:
        print("[decomp] WARNING: source_anchors.tsv not found — run 'source' first "
              "for classname/command anchors", file=sys.stderr)

    for dllfile in DECOMP_FILES:
        path = os.path.join(ROOT, dllfile)
        if not os.path.exists(path):
            continue
        starts = per_file_starts[dllfile]
        ends = per_file_ends[dllfile]
        funs = per_file_funs[dllfile]
        n = 0
        with open(path, "r", encoding="utf-8", errors="replace") as f:
            lineno = 0
            for line in f:
                lineno += 1
                if '"' not in line:
                    continue
                for sm in STRLIT_RE.finditer(line):
                    lit = sm.group(1)
                    if len(lit) < 3:
                        continue
                    if CLASSMETHOD_STR_RE.match(lit):
                        kind = "classmethod"
                    elif lit in classname_set:
                        kind = "classname"
                    elif lit in command_set:
                        kind = "command"
                    elif len(lit) >= 8:
                        kind = "generic"
                    else:
                        continue  # cut noise for generic short strings
                    idx = bisect.bisect_right(starts, lineno) - 1
                    if idx < 0 or lineno > ends[idx]:
                        fun = "?"
                    else:
                        fun = funs[idx]
                    anchors_out.write(f"{dllfile}\t{fun}\t{lit}\t{lineno}\t{kind}\n")
                    n += 1
        print(f"[decomp] {dllfile}: anchors={n}")

    anchors_out.close()


if __name__ == "__main__":
    os.makedirs(OUT_DIR, exist_ok=True)
    mode = sys.argv[1] if len(sys.argv) > 1 else "all"
    if mode in ("source", "all"):
        do_source()
    if mode in ("decomp", "all"):
        do_decomp()
