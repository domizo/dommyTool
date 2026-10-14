#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "$0")/.." && pwd)"
cd "$repo_root"

for required_path in \
  src/plugin/pluginMain.cpp \
  src/plugin/registration/PluginRegistration.h \
  src/core/DDataUtility \
  src/modules/point/DPoint \
  src/modules/curve/DCurve \
  src/modules/mesh/DMesh \
  src/modules/volume/VDB \
  src/modules/dynamics/DBullet \
  src/commands \
  src/integrations \
  python/DPyTools/__init__.py \
  python/DPyTools/bootstrap.py \
  python/DPyTools/core/paths.py \
  python/DPyTools/core/json_store.py \
  python/DPyTools/core/strings.py \
  python/DPyTools/pipeline/assets/operations.py \
  python/DPyTools/pipeline/assets/naming.py \
  python/DPyTools/features/animation/format.py \
  python/DPyTools/features/animation/serializer.py \
  python/DPyTools/scene \
  python/DPyTools/features \
  python/DPyTools/pipeline \
  python/DPyTools/integrations \
  python/DPyTools/plugins/dpt_locator.py \
  docs/MAYA_2019_VALIDATION.md \
  third_party/licenses \
  config/active_cpp_sources.txt \
  config/third_party_sources.json \
  tests/check_third_party_notices.py \
  src/modules/volume/VDB/Common/DVdbGridDispatch.h \
  tools/list_active_cpp_sources.py; do
  if [[ ! -e "$required_path" ]]; then
    echo "Required repository-layout path is missing: $required_path" >&2
    exit 1
  fi
done

for removed_prototype in \
  legacy/xcode \
  src/modules/point/DPoint/generate/data/cyUtility \
  tools/list_legacy_xcode_sources.py \
  tests/check_legacy_source_manifest.sh \
  src/core/DDataUtility/Particle/DParticle.cpp \
  src/core/DDataUtility/Particle/DSPHHasher.cpp \
  src/core/DDataUtility/Particle/DSPHSolver.cpp \
  src/core/DDataUtility/Common/DPSampler.cpp \
  src/core/noise/CurlNoise.cpp \
  src/core/noise/DBaseNoise.cpp; do
  if [[ -e "$removed_prototype" ]]; then
    echo "Removed unregistered prototype was reintroduced: $removed_prototype" >&2
    exit 1
  fi
done

for obsolete_root in \
  Cmd Curve DBullet DCurve DDataUtility DEmitter DJet DMesh DPoint DPyTools \
  Data DataModifier Debug Deformer Generate Render Utility UtilityNode VDB \
  arnold common curveEmitter global icosahedron math mesh noise otherlib \
  particle polyModifier registration transform dommyTool.xcodeproj; do
  if [[ -e "$obsolete_root" ]]; then
    echo "Pre-publication source root was reintroduced: $obsolete_root" >&2
    exit 1
  fi
done

loose_sources="$(find . -maxdepth 1 -type f \
  \( -name '*.c' -o -name '*.cc' -o -name '*.cpp' -o -name '*.cxx' \
     -o -name '*.h' -o -name '*.hpp' -o -name '*.py' -o -name '*.mel' \) \
  -print)"
if [[ -n "$loose_sources" ]]; then
  echo "Source file found at repository root:" >&2
  echo "$loose_sources" >&2
  exit 1
fi

analysis_reports="$(find . -maxdepth 1 -type f -name '*.plist' -print)"
if [[ -n "$analysis_reports" ]]; then
  echo "Analysis output found at repository root; write reports outside the publication tree:" >&2
  echo "$analysis_reports" >&2
  exit 1
fi

echo "Repository layout check: OK"
