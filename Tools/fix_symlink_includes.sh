#!/usr/bin/env bash
# Убирает симлинки-совместимости из дерева исходников и правит #include,
# которые на них ссылались.
#
# Зачем: порт собирается на case-sensitive ФС, а исходники писались под
# Windows, где регистр в путях не важен. Вместо правки сотен include при
# bring-up были заведены симлинки (mcc -> MCC и т.п.). Теперь это долг:
# симлинки ломают архивацию, git на других ФС и просто путают.
#
# Алгоритм:
#   1. собрать таблицу «имя симлинка -> реальное имя каталога»;
#   2. пройти по всем .h/.cpp/.inl, ТОЛЬКО по строкам #include;
#   3. если в пути встречается компонент-симлинк -- заменить на реальный;
#   4. напечатать сводку; коммит делает человек.
#
# Запуск из корня репозитория:
#   bash Tools/fix_symlink_includes.sh          # показать, что будет сделано
#   bash Tools/fix_symlink_includes.sh --apply  # выполнить правки
set -u

ROOT="${ROOT:-Source}"
APPLY=0
[ "${1:-}" = "--apply" ] && APPLY=1

echo "== 1. Таблица симлинков =="
MAP=$(mktemp)
find "$ROOT" -type l | while read -r link; do
	target=$(readlink "$link")
	lname=$(basename "$link")
	tname=$(basename "$target")
	# интересуют только те, что отличаются регистром/именем
	[ "$lname" = "$tname" ] && continue
	printf '%s\t%s\n' "$lname" "$tname"
done | sort -u > "$MAP"
column -t "$MAP" || cat "$MAP"
echo "  всего пар: $(wc -l < "$MAP")"

if [ ! -s "$MAP" ]; then
	echo "Симлинков, требующих правки include, не найдено."
	rm -f "$MAP"
	exit 0
fi

echo
echo "== 2-4. Правка #include =="
CHANGED=0
while IFS=$'\t' read -r LNAME TNAME; do
	# Компонент пути ровно между / или кавычкой/угловой скобкой.
	# Не трогаем include, где имя совпадает с реальным (регистр уже верный).
	FILES=$(grep -rlE "^[[:space:]]*#[[:space:]]*include[[:space:]]*[\"<][^\">]*(^|/)?${LNAME}/" \
		--include='*.h' --include='*.cpp' --include='*.inl' "$ROOT" 2>/dev/null)
	for f in $FILES; do
		[ -L "$f" ] && continue          # сам симлинк на файл не правим
		if [ "$APPLY" = "1" ]; then
			# sed только по строкам include, только по компоненту пути
			sed -i -E "\|^[[:space:]]*#[[:space:]]*include|{ s#(^|[\"</])${LNAME}/#\1${TNAME}/#g }" "$f"
		fi
		echo "  ${LNAME}/ -> ${TNAME}/   $f"
		CHANGED=$((CHANGED+1))
	done
done < "$MAP"
echo "  строк-файлов затронуто: $CHANGED"

echo
echo "== 5. Удаление симлинков =="
if [ "$APPLY" = "1" ]; then
	find "$ROOT" -type l -print -delete
else
	find "$ROOT" -type l -print
fi

rm -f "$MAP"
echo
[ "$APPLY" = "1" ] && echo "Готово. Пересобрать и закоммитить." \
                   || echo "Это был сухой прогон. Повторить с --apply."
