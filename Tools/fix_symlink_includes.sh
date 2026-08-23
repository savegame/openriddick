#!/usr/bin/env bash
# Убирает симлинки-совместимости из дерева исходников и правит #include,
# которые на них ссылались.
#
# Зачем: порт собирается на case-sensitive ФС, а исходники писались под
# Windows, где регистр в путях не важен. Вместо правки сотен include при
# bring-up были заведены симлинки. Теперь это долг: они ломают архивацию,
# git на других ФС и просто путают.
#
# ВАЖНО: симлинки двух видов, и обрабатывать их надо по-разному --
#   * КАТАЛОГИ (17 шт., напр. shared -> Shared): в include это КОМПОНЕНТ
#     пути, за ним всегда слэш -> "shared/MCC/Mrtc.h";
#   * ФАЙЛЫ (56 шт., напр. MRTC.h -> Mrtc.h, PCH.H -> PCH.h): в include это
#     ПОСЛЕДНИЙ компонент, за ним закрывающая кавычка или '>', а перед ним
#     кавычка, '<' или слэш -> #include "MRTC.h", #include "../MRTC.h".
# Первая версия скрипта знала только про каталоги и поэтому пропускала
# строки вида #include "MRTC.h" -- ровно тот случай, на который указал
# пользователь.
#
# Алгоритм:
#   1. собрать две таблицы: каталоги-симлинки и файлы-симлинки
#      («имя симлинка -> реальное имя»);
#   2. пройти по всем .h/.cpp/.inl/.c, ТОЛЬКО по строкам #include;
#   3. заменить компонент пути (каталог) или имя файла (последний компонент);
#   4. удалить симлинки;
#   5. напечатать сводку; сборку и коммит делает человек.
#
# Запуск из корня репозитория:
#   bash Tools/fix_symlink_includes.sh          # сухой прогон
#   bash Tools/fix_symlink_includes.sh --apply  # выполнить правки
set -u

ROOT="${ROOT:-Source}"
APPLY=0
[ "${1:-}" = "--apply" ] && APPLY=1

SRC=$(mktemp); MAPD=$(mktemp); MAPF=$(mktemp)
find "$ROOT" -type f \( -name '*.h' -o -name '*.cpp' -o -name '*.inl' -o -name '*.c' \) -not -type l > "$SRC"

# --- 1. таблицы -----------------------------------------------------------
find "$ROOT" -type l -xtype d | while read -r link; do
	l=$(basename "$link"); t=$(basename "$(readlink "$link")")
	[ "$l" = "$t" ] || printf '%s\t%s\n' "$l" "$t"
done | sort -u > "$MAPD"

find "$ROOT" -type l -xtype f | while read -r link; do
	l=$(basename "$link"); t=$(basename "$(readlink "$link")")
	[ "$l" = "$t" ] || printf '%s\t%s\n' "$l" "$t"
done | sort -u > "$MAPF"

echo "== 1. Таблицы симлинков =="
echo "-- каталоги ($(wc -l < "$MAPD")):"; cat "$MAPD"
echo "-- файлы ($(wc -l < "$MAPF")):";    cat "$MAPF"

# --- 2-3. правка include --------------------------------------------------
echo
echo "== 2-3. Правка #include =="
INC='^[[:space:]]*#[[:space:]]*include'
HITS=0

# каталоги: "<name>/" где перед name кавычка, '<' или слэш
while IFS=$'\t' read -r L T; do
	[ -z "${L:-}" ] && continue
	while read -r f; do
		grep -qE "${INC}.*[\"</]${L}/" "$f" || continue
		echo "  dir  ${L}/ -> ${T}/    $f"
		HITS=$((HITS+1))
		[ "$APPLY" = "1" ] && sed -i -E "\|${INC}|{ s#([\"</])${L}/#\1${T}/#g }" "$f"
	done < "$SRC"
done < "$MAPD"

# файлы: "<name>" в самом конце пути (перед закрывающей кавычкой или '>')
while IFS=$'\t' read -r L T; do
	[ -z "${L:-}" ] && continue
	LQ=$(printf '%s' "$L" | sed 's/[.[\*^$]/\\&/g')   # экранируем точку в имени
	while read -r f; do
		grep -qE "${INC}.*[\"</]${LQ}[\">]" "$f" || continue
		echo "  file ${L} -> ${T}          $f"
		HITS=$((HITS+1))
		[ "$APPLY" = "1" ] && sed -i -E "\|${INC}|{ s#([\"</])${LQ}([\">])#\1${T}\2#g }" "$f"
	done < "$SRC"
done < "$MAPF"
echo "  файлов затронуто (с повторами по парам): $HITS"

# --- 4. удаление симлинков ------------------------------------------------
echo
echo "== 4. Удаление симлинков =="
if [ "$APPLY" = "1" ]; then find "$ROOT" -type l -print -delete
else find "$ROOT" -type l -print | sed 's/^/  /'; fi

rm -f "$SRC" "$MAPD" "$MAPF"
echo
[ "$APPLY" = "1" ] && echo "Готово. ПЕРЕСОБРАТЬ, затем коммит." \
                   || echo "Это был сухой прогон. Повторить с --apply."
