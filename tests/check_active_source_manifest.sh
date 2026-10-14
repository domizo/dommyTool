#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "$0")/.." && pwd)"
export PYTHONDONTWRITEBYTECODE=1
python3 "$repo_root/tests/check_active_cpp_source_manifest.py"
check_dir="$(mktemp -d "${TMPDIR:-/tmp}/dommytool-source-manifest.XXXXXX")"
trap 'rm -rf "$check_dir"' EXIT
manifest_file="$check_dir/active_sources.txt"
focused_manifest_file="$check_dir/focused_sources.txt"
cmake_check_dir="$check_dir/cmake"

python3 "$repo_root/tools/list_active_cpp_sources.py" \
  --manifest "$repo_root/config/active_cpp_sources.txt" \
  --repository-root "$repo_root" \
  > "$manifest_file"

cmake -S "$repo_root/tests/fixtures/active_source_manifest" \
  -B "$cmake_check_dir" -DDOMMY_REPOSITORY_ROOT="$repo_root"
if ! cmp -s "$manifest_file" "$cmake_check_dir/active_sources.txt"; then
  echo "CMake and the source-manifest reader disagree on active sources." >&2
  exit 1
fi

source_count="$(wc -l < "$manifest_file" | tr -d '[:space:]')"
unique_count="$(sort -u "$manifest_file" | wc -l | tr -d '[:space:]')"
if [[ "$source_count" -ne 457 || "$unique_count" -ne "$source_count" ]]; then
  echo "Unexpected active source inventory: $source_count ($unique_count unique)" >&2
  exit 1
fi

for required_source in \
  src/plugin/pluginMain.cpp \
  src/core/DDataUtility/Utility/DPMerge.cpp \
  src/modules/point/DPoint/Duplicator/DPInstancer.cpp \
  src/modules/volume/VDB/Data/DOpenVDBData.cpp; do
  if ! grep -qxF "$required_source" "$manifest_file"; then
    echo "Required active source is absent from the manifest: $required_source" >&2
    exit 1
  fi
done

for inactive_source in \
  src/core/DDataUtility/Attribute/Edit/DPAttributePromote.cpp \
  src/modules/mesh/DMesh/Dynamics/Bullet/Constraint/BTHingeConstraint.cpp \
  src/modules/mesh/DMesh/Dynamics/Bullet/Constraint/BTSliderConstraint.cpp \
  src/modules/volume/VDB/Data/DPVDBData.cpp; do
  if grep -qxF "$inactive_source" "$manifest_file"; then
    echo "Unregistered source entered the active manifest: $inactive_source" >&2
    exit 1
  fi
done

while IFS= read -r source; do
  if [[ "$source" != src/* && "$source" != third_party/* ]] ||
     [[ ! -f "$repo_root/$source" ]]; then
    echo "Invalid project source in the active manifest: $source" >&2
    exit 1
  fi
done < "$manifest_file"

while IFS= read -r source; do
  relative_source="${source#$repo_root/}"
  if ! grep -qxF "$relative_source" "$manifest_file"; then
    echo "Checked-in C++ implementation is absent from the active manifest: $relative_source" >&2
    exit 1
  fi
done < <(find "$repo_root/src" -type f -name '*.cpp' | sort)

sed -n \
  's#.*"\$repo_root/\([^"]*\.cpp\)".*#\1#p' \
  "$repo_root/tests/check_maya_sdk_headers.sh" \
  | grep '^src/' \
  | sort -u \
  > "$focused_manifest_file"

focused_source_count="$(
  wc -l < "$focused_manifest_file" | tr -d '[:space:]'
)"
project_source_count="$(
  grep -c '^src/' "$manifest_file" | tr -d '[:space:]'
)"
if [[ "$focused_source_count" -lt 132 ]]; then
  echo "Focused Maya SDK coverage unexpectedly decreased: $focused_source_count" >&2
  exit 1
fi

while IFS= read -r source; do
  if ! grep -qxF "$source" "$manifest_file"; then
    echo "Focused Maya SDK source is absent from the active manifest: $source" >&2
    exit 1
  fi
done < "$focused_manifest_file"

if ! grep -qxF \
  'src/core/DDataUtility/Utility/DPTransform.cpp' \
  "$focused_manifest_file"; then
  echo "DPTransform is absent from the focused Maya SDK coverage" >&2
  exit 1
fi

echo "Active source manifest check: OK ($source_count unique checked-in sources)"
echo "Focused Maya SDK source coverage: $focused_source_count/$project_source_count project sources"
python3 "$repo_root/tests/check_optional_source_coverage.py"
