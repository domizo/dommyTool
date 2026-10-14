#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "$0")/.." && pwd)"
clangxx="${CLANGXX:-clang++}"

analyze() {
  local source_file="$1"
  shift

  "$clangxx" \
    --analyze \
    -std=c++11 \
    -Xanalyzer -analyzer-output=text \
    -Xanalyzer -analyzer-werror \
    -o /dev/null \
    "$@" \
    "$repo_root/$source_file"
}

core_includes=(
  -I"$repo_root/tests/stubs"
  -I"$repo_root/src/plugin"
  -I"$repo_root/src/core"
  -I"$repo_root/src/core/math"
  -I"$repo_root/src/core/Utility"
  -I"$repo_root/src/core/Utility/3d/Math"
  -I"$repo_root/src/core/Utility/3d/DataStruct"
  -I"$repo_root/src/modules/point"
  -I"$repo_root/src/modules/point/DPoint"
)

pdb_includes=(
  -I"$repo_root/src/core"
  -I"$repo_root/src/modules/dynamics/DBullet/File"
)

generation_includes=(
  -I"$repo_root/src"
  -I"$repo_root/src/core"
)

analyze "src/core/math/dommyMath.cpp" "${core_includes[@]}"
analyze "src/core/noise/PerlinNoise.cpp"
analyze "src/modules/dynamics/DBullet/File/pdbIO.cpp" "${pdb_includes[@]}"
analyze "tests/cpp/atomic_output_tests.cpp" "${pdb_includes[@]}"
analyze "tests/cpp/utility_vector_tests.cpp" "${core_includes[@]}"
analyze "tests/cpp/generation_limits_analysis.cpp" "${generation_includes[@]}"

echo "Clang static analysis: OK"
