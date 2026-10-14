#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "$0")/.." && pwd)"
dependency_root="${1:-${DOMMY_LEGACY_DEPENDENCY_ROOT:-${TMPDIR:-/tmp}/dommytool-legacy-dependencies}}"

bullet_root="$dependency_root/bullet3-126b6762200b1a9ec8df39b8fd53cb2ad62fb146"
openvdb_root="$dependency_root/openvdb-a79df2f32b8ebb5d15fd73325c46cee845a81e49"
openexr_root="$dependency_root/openexr-dcaf5fdb4d1244d8e60a58832cfe9c54734a2257"
opensubdiv_root="$dependency_root/OpenSubdiv-3b5c3d99d29ba078385013d15f8b2ebeac0daf45"
seexpr_root="$dependency_root/SeExpr-a5f02bb03199630759b0b94a64f37ce56c08675a"
nanoflann_root="$dependency_root/nanoflann-f2efec0110eb68edf8cae90fce394a5044bf3db8"

for required_directory in \
  "$bullet_root" \
  "$openvdb_root" \
  "$openexr_root" \
  "$opensubdiv_root" \
  "$seexpr_root" \
  "$nanoflann_root"; do
  if [[ ! -d "$required_directory" ]]; then
    echo "Dependency source is missing: $required_directory" >&2
    echo "Run tools/fetch_legacy_dependency_sources.sh first." >&2
    exit 1
  fi
done

missing=0
bullet_header_count=0
openvdb_header_count=0
opensubdiv_header_count=0

while IFS= read -r header; do
  [[ -z "$header" ]] && continue
  bullet_header_count=$((bullet_header_count + 1))

  local_header="$(find \
    "$repo_root/src/modules/dynamics/DBullet" "$repo_root/src/modules/mesh/DMesh/Dynamics/Bullet" \
    -type f -name "${header##*/}" -print -quit)"
  if [[ -n "$local_header" ]]; then
    continue
  fi

  if [[ -f "$bullet_root/src/$header" ]]; then
    continue
  fi

  external_header="$(find "$bullet_root/src" -type f -name "${header##*/}" -print -quit)"
  if [[ -z "$external_header" ]]; then
    echo "Bullet 2.88 header not found: $header" >&2
    missing=$((missing + 1))
  fi
done < <(
  find "$repo_root/src/modules/dynamics/DBullet" "$repo_root/src/modules/mesh/DMesh" \
    \( -path '*/old' -o -path '*/backup' \) -prune -o \
    -type f \( -name '*.cpp' -o -name '*.h' -o -name '*.hpp' \) \
    -exec sed -nE \
      's@^[[:space:]]*#[[:space:]]*include[[:space:]]*[<"]((Bullet|bt)[^>"]+)[>"].*$@\1@p' \
      {} + | sort -u
)

if (( bullet_header_count == 0 )); then
  echo "No active Bullet includes were discovered; dependency audit is invalid." >&2
  exit 1
fi

while IFS= read -r header; do
  [[ -z "$header" ]] && continue
  openvdb_header_count=$((openvdb_header_count + 1))
  if [[ ! -f "$openvdb_root/$header" ]]; then
    echo "OpenVDB 5.2.0 header not found: $header" >&2
    missing=$((missing + 1))
  fi
done < <(
  find "$repo_root/src/modules/volume/VDB" \
    -type f \( -name '*.cpp' -o -name '*.h' -o -name '*.hpp' \) \
    -exec sed -nE \
      's|^[[:space:]]*#[[:space:]]*include[[:space:]]*[<"](openvdb[^>"]+)[>"].*$|\1|p' \
      {} + | sort -u
)

if (( openvdb_header_count == 0 )); then
  echo "No active OpenVDB includes were discovered; dependency audit is invalid." >&2
  exit 1
fi

while IFS= read -r header; do
  [[ -z "$header" ]] && continue
  opensubdiv_header_count=$((opensubdiv_header_count + 1))
  if [[ ! -f "$opensubdiv_root/$header" ]]; then
    echo "OpenSubdiv 3.3.3 header not found: $header" >&2
    missing=$((missing + 1))
  fi
done < <(
  find "$repo_root" \
    \( -path "$repo_root/.git" -o -path '*/old' -o -path '*/backup' \) -prune -o \
    -type f \( -name '*.cpp' -o -name '*.h' -o -name '*.hpp' \) \
    -exec sed -nE \
      's|^[[:space:]]*#[[:space:]]*include[[:space:]]*[<"](opensubdiv[^>"]+)[>"].*$|\1|p' \
      {} + | sort -u
)

if (( opensubdiv_header_count == 0 )); then
  echo "No active OpenSubdiv includes were discovered; dependency audit is invalid." >&2
  exit 1
fi

if (( missing != 0 )); then
  exit 1
fi

"${CXX:-clang++}" \
  -std=c++11 -Wall -Wextra -Wpedantic -fsyntax-only \
  -isystem "$bullet_root/src" \
  "$repo_root/tests/cpp/bullet_legacy_api_compile_test.cpp"

project_include_args=()
while IFS= read -r directory; do
  project_include_args+=("-I$directory")
done < <(find "$repo_root/src" -type d -print)

"${CXX:-clang++}" \
  -std=c++11 -w -fsyntax-only \
  -isystem "$bullet_root/src" \
  -I"$repo_root/tests/stubs" \
  "${project_include_args[@]}" \
  "$repo_root/tests/cpp/bullet_project_headers_compile_test.cpp"

"${CXX:-clang++}" \
  -std=c++11 -w -fsyntax-only \
  -isystem "$bullet_root/src" \
  -I"$repo_root/tests/stubs" \
  "${project_include_args[@]}" \
  "$repo_root/src/modules/dynamics/DBullet/dbSolver_t.cpp"

"${CXX:-clang++}" \
  -std=c++11 -Wall -Wextra -Wpedantic -Werror -fsyntax-only \
  -isystem "$bullet_root/src" \
  -I"$repo_root/src/modules/mesh/DMesh/Dynamics/Bullet" \
  -I"$repo_root/src" -I"$repo_root/src/core" \
  "$repo_root/src/modules/mesh/DMesh/Dynamics/Bullet/DBFractureDynamicsWorld.cpp"

"${CXX:-clang++}" \
  -std=c++11 -Wall -Wextra -Wpedantic -fsyntax-only \
  -isystem "$repo_root/tests/compat/ilmbase-2.2" \
  -isystem "$openexr_root/IlmBase/Imath" \
  -isystem "$openexr_root/IlmBase/Iex" \
  -isystem "$openexr_root/IlmBase/Half" \
  "$repo_root/tests/cpp/openexr_legacy_api_compile_test.cpp"

"${CXX:-clang++}" \
  -std=c++11 -Wall -Wextra -Wpedantic -fsyntax-only \
  -isystem "$opensubdiv_root" \
  "$repo_root/tests/cpp/opensubdiv_legacy_api_compile_test.cpp"

"${CXX:-clang++}" \
  -std=c++11 -Wall -Wextra -Wpedantic -fsyntax-only \
  -isystem "$seexpr_root/src/SeExpr" \
  "$repo_root/tests/cpp/seexpr_legacy_api_compile_test.cpp"

bash "$repo_root/tests/check_seexpr_runtime.sh" "$dependency_root"

"${CXX:-clang++}" \
  -std=c++11 -Wall -Wextra -Wpedantic -Werror \
  -fsanitize=address,undefined -fno-omit-frame-pointer \
  -I"$repo_root/src/core" \
  -isystem "$nanoflann_root/include" \
  "$repo_root/tests/cpp/nanoflann_legacy_api_compile_test.cpp" \
  -o "${TMPDIR:-/tmp}/dommytool_nanoflann_tests"

if [[ "$(uname -s)" == "Darwin" ]]; then
  ASAN_OPTIONS=detect_leaks=0 "${TMPDIR:-/tmp}/dommytool_nanoflann_tests"
else
  ASAN_OPTIONS=detect_leaks=1 "${TMPDIR:-/tmp}/dommytool_nanoflann_tests"
fi

echo "Legacy dependency header/API compatibility check: OK"
