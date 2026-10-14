#!/usr/bin/env bash
set -euo pipefail

export PYTHONDONTWRITEBYTECODE=1

repo_root="$(cd "$(dirname "$0")/.." && pwd)"
cd "$repo_root"

failed=0

report_paths() {
  local message="$1"
  shift

  local paths
  paths="$("$@")"
  if [[ -n "$paths" ]]; then
    echo "$message" >&2
    echo "$paths" >&2
    failed=1
  fi
}

for excluded_path in \
  legacy/xcode \
  src/modules/point/DPoint/generate/data/cyUtility \
  docs/SAFETY_AUDIT.md \
  docs/PUBLICATION_CHECKLIST.md \
  third_party/exprtk \
  third_party/licenses/CPL-1.0.txt \
  maze \
  OpenCV \
  python/DPyTools/vray \
  python/DPyTools/gui \
  python/DPyTools/debug \
  python/DPyTools/ftrack \
  python/DPyTools/integrations/ftrack \
  python/DPyTools/rigging/DSkinClusterOp.py \
  python/DPyTools/system/DReloadModule.py \
  python/DPyTools/utility/DShaderManager.py \
  python/DPyTools/utility/DNodeRenamer.py \
  python/DPyTools/ui/DComponentUI.py \
  python/DPyTools/ui/DPyUI.py \
  python/DPyTools/rigging/JointOrient/cometJointOrient.mel; do
  if [[ -e "$excluded_path" ]]; then
    echo "Publication-excluded path is present: $excluded_path" >&2
    failed=1
  fi
done

removed_sampling_dependency="$(rg -l \
  '\bcy::|cy(Core|Heap|Point|PointCloud|SampleElim)\.h|cyUtility' \
  src CMakeLists.txt cmake config tools tests/cpp tests/fixtures tests/run_cpp_tests.sh || true)"
if [[ -n "$removed_sampling_dependency" ]]; then
  echo "Removed sampling-library dependency is still referenced:" >&2
  echo "$removed_sampling_dependency" >&2
  failed=1
fi

PYTHONDONTWRITEBYTECODE=1 python3 tests/check_third_party_notices.py
PYTHONDONTWRITEBYTECODE=1 python3 tests/check_python_archive.py
PYTHONDONTWRITEBYTECODE=1 python3 tests/check_python_architecture.py
PYTHONDONTWRITEBYTECODE=1 python3 tests/check_python_semantics.py
PYTHONDONTWRITEBYTECODE=1 python3 tests/run_python_tests.py

copied_vendor_python="$(rg -l -i --hidden \
  --glob 'python/**/*.py' \
  'taken from .*tractor-spool|copyright[^\n]*(pixar|ftrack).*all rights reserved' \
  . || true)"
if [[ -n "$copied_vendor_python" ]]; then
  echo "Python source with unapproved third-party provenance found:" >&2
  echo "$copied_vendor_python" >&2
  failed=1
fi

report_paths \
  "IDE state, caches, or operating-system metadata found:" \
  find . -not -path './.git/*' \
    \( -name .DS_Store -o -name .idea -o -name .vscode -o -name xcuserdata \
       -o -name __pycache__ -o -name '*.pyc' -o -name '*.pyo' \
       -o -name '*.pbxuser' -o -name '*.mode1v3' -o -name '*.xcuserstate' \) \
    -print

report_paths \
  "Credential, private-key, or local-database file found:" \
  find . -type f -not -path './.git/*' \
    \( -name .env -o \( -name '.env.*' -not -name .env.example \) \
       -o -name '*.pem' -o -name '*.key' -o -name '*.p12' \
       -o -name '*.pfx' -o -name '*credential*' -o -name '*secret*' \
       -o -name '*.sqlite' -o -name '*.sqlite3' -o -name '*.db' \) \
    -print

report_paths \
  "Compiled binary found in the source tree:" \
  find . -type f -not -path './.git/*' \
    \( -name '*.mll' -o -name '*.bundle' -o -name '*.dylib' -o -name '*.dll' \
       -o -name '*.so' -o -name '*.a' -o -name '*.lib' -o -name '*.exe' \) \
    -print

report_paths \
  "Root-level analysis report found in the publication tree:" \
  find . -maxdepth 1 -type f -name '*.plist' -print

local_path_files="$(rg -l --hidden \
  --glob '!.git/**' \
  --glob '!tests/check_build_configuration.sh' \
  --glob '!tests/check_publication_readiness.sh' \
  '/Users/|/Volumes/|[A-Za-z]:\\Users\\' . || true)"
if [[ -n "$local_path_files" ]]; then
  echo "Workstation-specific absolute path found:" >&2
  echo "$local_path_files" >&2
  failed=1
fi

private_key_files="$(rg -l --hidden \
  --glob '!.git/**' \
  --glob '!tests/check_publication_readiness.sh' \
  'BEGIN (RSA |EC |OPENSSH |DSA )?PRIVATE KEY' . || true)"
if [[ -n "$private_key_files" ]]; then
  echo "Private-key material found:" >&2
  echo "$private_key_files" >&2
  failed=1
fi

token_files="$(rg -l --hidden \
  --glob '!.git/**' \
  --glob '!tests/check_publication_readiness.sh' \
  'AKIA[0-9A-Z]{16}|gh[pousr]_[A-Za-z0-9_]{30,}|github_pat_[A-Za-z0-9_]{20,}|xox[baprs]-[A-Za-z0-9-]{10,}|AIza[0-9A-Za-z_-]{30,}' \
  . || true)"
if [[ -n "$token_files" ]]; then
  echo "Known credential-token format found:" >&2
  echo "$token_files" >&2
  failed=1
fi

credential_literal_files="$({
  rg -l -i --hidden \
    --glob '!.git/**' \
    --glob '!.env.example' \
    --glob '!tests/check_publication_readiness.sh' \
    --glob '!third_party/**' \
    '(password|passwd|api[_-]?key|access[_-]?token|auth[_-]?token|client[_-]?secret)[[:space:]]*[:=][[:space:]]*"[^"]+"' \
    . || true
  rg -l -i --hidden \
    --glob '!.git/**' \
    --glob '!.env.example' \
    --glob '!tests/check_publication_readiness.sh' \
    --glob '!third_party/**' \
    "(password|passwd|api[_-]?key|access[_-]?token|auth[_-]?token|client[_-]?secret)[[:space:]]*[:=][[:space:]]*'[^']+'" \
    . || true
} | sort -u)"
if [[ -n "$credential_literal_files" ]]; then
  echo "Literal credential assignment found:" >&2
  echo "$credential_literal_files" >&2
  failed=1
fi

private_address_files="$(rg -l --hidden \
  --glob '!.git/**' \
  --glob '!tests/check_publication_readiness.sh' \
  '\b(10\.[0-9]{1,3}\.[0-9]{1,3}\.[0-9]{1,3}|192\.168\.[0-9]{1,3}\.[0-9]{1,3}|172\.(1[6-9]|2[0-9]|3[01])\.[0-9]{1,3}\.[0-9]{1,3})\b' \
  . || true)"
if [[ -n "$private_address_files" ]]; then
  echo "Private network address found:" >&2
  echo "$private_address_files" >&2
  failed=1
fi

unpinned_actions="$(
  rg -n 'uses:[[:space:]]+[^[:space:]#]+@' .github/workflows \
    | rg -v '@[0-9a-f]{40}([[:space:]]*#.*)?$' || true
)"
if [[ -n "$unpinned_actions" ]]; then
  echo "GitHub Action is not pinned to an immutable commit SHA:" >&2
  echo "$unpinned_actions" >&2
  failed=1
fi

if [[ ! -f LICENSE ]] || ! rg -q '^MIT License$' LICENSE; then
  echo "The project-level MIT LICENSE file is missing or invalid." >&2
  failed=1
fi

for license_file in \
  third_party/licenses/LGPL-2.1.txt \
  third_party/licenses/MPL-2.0.txt \
  third_party/licenses/Voro++-modified-BSD.txt; do
  if [[ ! -s "$license_file" ]]; then
    echo "Required third-party license text is missing or empty: $license_file" >&2
    failed=1
  fi
done

for modified_vendor_source in \
  src/modules/dynamics/DBullet/File/pdbIO.cpp \
  src/modules/dynamics/DBullet/File/pdbIO.h \
  src/modules/dynamics/DBullet/mvl/mathUtils.h \
  src/modules/dynamics/DBullet/mvl/vec.h \
  src/modules/dynamics/DBullet/mvl/mat.h \
  src/modules/dynamics/DBullet/mvl/quat.h; do
  for required_notice in \
    'Copyright (c) 2008 Walt Disney Studios' \
    'Written by: Nicola Candussi' \
    'Modified for dommyTool; not the original upstream version.'; do
    if ! rg -qF "$required_notice" "$modified_vendor_source"; then
      echo "Required upstream or local-modification notice is missing: $modified_vendor_source" >&2
      failed=1
    fi
  done
done

if rg -qi 'exprtk' CMakeLists.txt cmake config src python docs README.md THIRD_PARTY_NOTICES.md; then
  echo "An unused, removed dependency is still referenced by the publication tree." >&2
  failed=1
fi

if rg -q 'mazeMesh|DSfmCmd|DSfmGen|DOpenCVHeaders' \
  src/plugin/pluginMain.cpp src/modules/generators/Generate \
  src/plugin/registration config/active_cpp_sources.txt; then
  echo "A removed feature is still referenced by the active plug-in or source manifest." >&2
  failed=1
fi

if [[ "$failed" -ne 0 ]]; then
  exit 1
fi

echo "Publication readiness source-tree check: OK"
