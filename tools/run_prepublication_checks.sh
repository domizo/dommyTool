#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "$0")/.." && pwd)"

bash "$repo_root/tests/check_publication_readiness.sh"
bash "$repo_root/tests/run_cpp_tests.sh"
bash "$repo_root/tests/run_clang_static_analysis.sh"
bash "$repo_root/tests/check_maya_node_contracts.sh"

maya_location="${MAYA_LOCATION:-}"
if [[ -n "$maya_location" ]]; then
  bash "$repo_root/tests/check_maya_sdk_headers.sh" "$maya_location"
else
  echo "Maya SDK source check skipped: MAYA_LOCATION is not set."
fi

echo "Pre-publication checks: OK"
