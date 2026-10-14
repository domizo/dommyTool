#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "$0")/.." && pwd)"

# Project-owned source must not reintroduce shell execution or unbounded C
# string writers. Vendored and archival trees are intentionally excluded.
matches="$({
  find "$repo_root" \
    \( -path "$repo_root/.git" \
       -o -path "$repo_root/build" \
       -o -path "$repo_root/build-*" \
       -o -path "$repo_root/third_party" \
       -o -path "$repo_root/backup" \
       -o -path '*/backup' \
       -o -path '*/old' \) -prune -o \
    -type f \( -name '*.c' -o -name '*.cpp' -o -name '*.cc' -o -name '*.h' -o -name '*.hpp' \) \
    -exec grep -nHE \
      '^[[:space:]]*[^/].*\b(system|popen|sprintf|strcpy|strcat|gets)[[:space:]]*\(' \
      {} +
} || true)"

if [[ -n "$matches" ]]; then
  echo "Unsafe process or C-string API found in project-owned source:" >&2
  echo "$matches" >&2
  exit 1
fi

maya_command_hazards="$({
  find "$repo_root" \
    \( -path "$repo_root/.git" \
       -o -path "$repo_root/build" \
       -o -path "$repo_root/build-*" \
       -o -path "$repo_root/third_party" \
       -o -path "$repo_root/backup" \
       -o -path '*/backup' \
       -o -path '*/old' \) -prune -o \
    -type f \( -name '*.c' -o -name '*.cpp' -o -name '*.cc' -o -name '*.h' -o -name '*.hpp' \) \
    -exec grep -nHE 'pause[[:space:]]+-sec|addAttr[^;]*(rgbPP|rgbPP0)' {} +
} || true)"

if [[ -n "$maya_command_hazards" ]]; then
  echo "Blocking or string-built Maya command found in project-owned source:" >&2
  echo "$maya_command_hazards" >&2
  exit 1
fi

echo "Project source safety check: OK"
