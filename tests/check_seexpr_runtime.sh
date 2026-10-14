#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "$0")/.." && pwd)"
dependency_root="${1:-${DOMMY_LEGACY_DEPENDENCY_ROOT:-${TMPDIR:-/tmp}/dommytool-legacy-dependencies}}"
source_root="$dependency_root/SeExpr-a5f02bb03199630759b0b94a64f37ce56c08675a"
if [[ ! -f "$source_root/src/SeExpr/SeExpression.cpp" ]]; then
  echo "Pinned SeExpr sources are missing. Run tools/fetch_legacy_dependency_sources.sh first." >&2
  exit 1
fi
build_dir="$(mktemp -d "${TMPDIR:-/tmp}/dommy-seexpr-runtime.XXXXXX")"
trap 'rm -rf "$build_dir"' EXIT

"${CXX:-clang++}" -std=c++11 -Wno-deprecated-register -Wno-deprecated-declarations \
  -fsanitize=address,undefined,float-cast-overflow -fno-sanitize-recover=all \
  -fno-omit-frame-pointer -I"$repo_root/src/core" \
  -I"$source_root/src/SeExpr" -I"$source_root/windows7/SeExpr/generated" \
  "$repo_root/tests/cpp/seexpr_runtime_tests.cpp" \
  "$source_root/src/SeExpr/SeExpression.cpp" \
  "$source_root/src/SeExpr/SeExprNode.cpp" \
  "$source_root/src/SeExpr/SeContext.cpp" \
  "$source_root/windows7/SeExpr/generated/SeExprParser.cpp" \
  "$source_root/windows7/SeExpr/generated/SeExprParserLex.cpp" \
  -o "$build_dir/seexpr_runtime_tests"

if [[ "$(uname -s)" == "Darwin" ]]; then
  ASAN_OPTIONS=detect_leaks=0 "$build_dir/seexpr_runtime_tests"
else
  ASAN_OPTIONS=detect_leaks=1 "$build_dir/seexpr_runtime_tests"
fi
