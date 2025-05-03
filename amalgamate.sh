#!/usr/bin/env bash
set -euo pipefail

if [[ $# -lt 2 ]]; then
  echo "Usage: $0 <output-file> <src-dir1> [<src-dir2> ...]"
  exit 1
fi

out="$1"; shift
src_dirs=("$@")

# start fresh
> "$out"
echo "// Amalgamated on $(date)" >> "$out"
echo                                >> "$out"

# gather & merge
find "${src_dirs[@]}" -type f \( -name '*.hpp' -o -name '*.cpp' \) \
  | sort -t. -k2,2r -k1,1 \
  | while IFS= read -r f; do
      rel="$f"
      echo "// --- $rel ---" >> "$out"
      cat "$f"               >> "$out"
      echo                   >> "$out"
    done

echo "✔  Wrote $out"

