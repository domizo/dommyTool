#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "$0")/.." && pwd)"

# MPxData returned by MDataHandle/MFnPluginData is owned by Maya. A default
# smart-pointer deleter can therefore cause double-free/use-after-free on an
# early return, even when a later success path calls release().
matches="$({
  find "$repo_root" \
    \( -path "$repo_root/.git" -o -path "$repo_root/build" -o -path "$repo_root/build-*" \) -prune -o \
    -type f \( -name '*.cpp' -o -name '*.cc' -o -name '*.h' -o -name '*.hpp' \) \
    -exec grep -nHE \
      '^[[:space:]]*(const[[:space:]]+)?std::(auto_ptr|shared_ptr|unique_ptr)<[^;]+(asPluginData\(\)|\.data\(&)' \
      {} +
} || true)"

if [[ -n "$matches" ]]; then
  echo "Maya-owned MPxData must not be wrapped in a default smart pointer:" >&2
  echo "$matches" >&2
  exit 1
fi

# The baseline source block in the real-SDK check is the publication boundary
# for default DPoint/DCurve/DMesh nodes. Those nodes may mutate a non-owning
# MPxData pointer while constructing output, but must commit the MObject owned
# by MFnPluginData rather than pass that pointer to MDataHandle::set().
baseline_sources="$(
  sed -n \
    '/Compile the remaining baseline custom-data nodes/,/baseline_data_nodes_result=/p' \
    "$repo_root/tests/check_maya_sdk_headers.sh" |
    sed -n 's/.*"\$repo_root\/\([^"]*\.cpp\)".*/\1/p'
)"
raw_output_matches=""
while IFS= read -r source; do
  [[ -z "$source" ]] && continue
  source_matches="$({
    grep -nHE \
      '\.(set)\((outputDPData|outDPointData|outDCrvs|outDCurves|outDMeshes|dpData|output)\)' \
      "$repo_root/$source"
  } || true)"
  if [[ -n "$source_matches" ]]; then
    raw_output_matches+="${source_matches}"$'\n'
  fi
done <<< "$baseline_sources"

if [[ -n "$raw_output_matches" ]]; then
  echo "Baseline plug-in data outputs must publish an MObject, not an MPxData pointer:" >&2
  echo "$raw_output_matches" >&2
  exit 1
fi

python3 "$repo_root/tests/check_default_maya_data_ownership.py"
python3 "$repo_root/tests/check_plug_handle_ownership.py"

echo "Maya plug-in data ownership check: OK"
