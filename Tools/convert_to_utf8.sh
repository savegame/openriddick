#!/usr/bin/env bash
# Переводит исходники из CP1252 в UTF-8 (задача 2 из port_status.md).
#
# Осторожно: правит файлы на месте. Делать ОТДЕЛЬНЫМ коммитом, ничего
# больше в него не класть -- иначе diff станет нечитаемым.
#
#   bash Tools/convert_to_utf8.sh          # только показать, что не UTF-8
#   bash Tools/convert_to_utf8.sh --apply  # конвертировать
set -u
ROOT="${ROOT:-Source}"
APPLY=0
[ "${1:-}" = "--apply" ] && APPLY=1
N=0
while IFS= read -r f; do
	# уже валидный UTF-8 -- пропускаем (iconv -f UTF-8 -t UTF-8 проверяет)
	if iconv -f UTF-8 -t UTF-8 "$f" >/dev/null 2>&1; then continue; fi
	N=$((N+1))
	echo "  cp1252 -> utf8: $f"
	if [ "$APPLY" = "1" ]; then
		iconv -f CP1252 -t UTF-8 "$f" > "$f.utf8tmp" && mv "$f.utf8tmp" "$f"
	fi
done < <(find "$ROOT" -type f \( -name '*.h' -o -name '*.cpp' -o -name '*.inl' -o -name '*.c' \) -not -type l)
echo "файлов не в UTF-8: $N"
[ "$APPLY" = "1" ] && echo "Готово. ПЕРЕСОБРАТЬ и только потом коммитить." \
                   || echo "Сухой прогон. Повторить с --apply."
