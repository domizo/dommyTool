#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "$0")/.." && pwd)"

python3 "$repo_root/tests/check_maya_type_ids.py"

require_fixed_text() {
  local file="$1"
  local text="$2"
  if ! grep -qF "$text" "$repo_root/$file"; then
    echo "Maya compatibility contract changed in $file: $text" >&2
    exit 1
  fi
}

require_regex() {
  local file="$1"
  local pattern="$2"
  if ! grep -qE "$pattern" "$repo_root/$file"; then
    echo "Maya node contract is missing in $file: $pattern" >&2
    exit 1
  fi
}

forbid_regex() {
  local file="$1"
  local pattern="$2"
  if grep -nE "$pattern" "$repo_root/$file"; then
    echo "Forbidden Maya node pattern found in $file: $pattern" >&2
    exit 1
  fi
}

# Serialized identities for the three reference nodes.
require_fixed_text "src/modules/point/DPoint/dpoint_ids.h" "#define ID_DP_MERGE (DOMMY_POINT_ID_BLOCKSTART + 1028)"
require_fixed_text "src/modules/volume/VDB/dvdb_ids.h" "#define ID_VDB_DOPENVDB_TO_MESH (DOMMY_VDB_ID_BLOCKSTART + 150)"
require_fixed_text "src/modules/point/DPoint/dpoint_ids.h" "#define ID_DB_SOLVER (DOMMY_POINT_ID_BLOCKSTART + 3500)"
require_regex "src/plugin/registration/modules/PointRegistrations.inl" 'registerNode\([[:space:]]*"DPMerge"'
require_regex "src/plugin/registration/modules/OpenVDBRegistrations.inl" 'registerNode\([[:space:]]*"DVDBToMesh"'
require_fixed_text "src/modules/dynamics/DBullet/Solver/DBSolverNode.cpp" 'const MString DBSolverNode::typeName("DBSolverNode");'

for attribute in \
  '"inDPs", "idps"' \
  '"outDP", "odp"'; do
  require_fixed_text "src/core/DDataUtility/Utility/DPMerge.cpp" "$attribute"
done

for attribute in \
  '"vdbInput", "input"' \
  '"outMesh", "om"' \
  '"outDM", "odm"' \
  '"isovalue", "iso"' \
  '"adaptivity", "adapt"' \
  '"vdbAllGridNames", "allgrids"' \
  '"vdbSelectedGridNames", "selectedgrids"'; do
  require_fixed_text "src/modules/volume/VDB/Convert/DOpenVDBToMesh.cpp" "$attribute"
done

# Reference implementations must retain the agreed compute and scheduling rules.
require_regex "src/core/DDataUtility/Utility/DPMerge.h" 'compute\(const MPlug& plug, MDataBlock& data\) override'
require_regex "src/modules/volume/VDB/Convert/DOpenVDBToMesh.h" 'compute\(const MPlug& plug, MDataBlock& data\) override'
require_regex "src/modules/dynamics/DBullet/Solver/DBSolverNode.h" 'schedulingType\(\) const override'
require_regex "src/modules/dynamics/DBullet/DM/DCBullet2DSolver.h" 'schedulingType\(\) const override'
require_regex "src/modules/dynamics/DBullet/DM/DMBulletTest.h" 'schedulingType\(\) const override'
require_regex "src/modules/mesh/DMesh/Dynamics/Bullet/DBulletFractureSolver.h" 'schedulingType\(\) const override'
for writer in \
  "src/core/DDataUtility/IO/DPWriteToAi.h" \
  "src/modules/curve/DCurve/IO/DWriteDCurveToAi.h" \
  "src/modules/point/DPoint/render/light/DPLightGen.h" \
  "src/modules/volume/VDB/IO/DWriteOpenVDB.h"; do
  require_regex "$writer" 'schedulingType\(\) const override.*kGloballySerialize'
done
require_fixed_text "src/modules/volume/VDB/IO/DWriteOpenVDB.cpp" \
  '"WriteFile", "wrt"'
require_regex "src/modules/volume/VDB/IO/DWriteOpenVDB.cpp" \
  'false,[[:space:]]*&stat'
require_regex "src/modules/volume/VDB/IO/DWriteOpenVDB.cpp" \
  'nAttr\.setStorable\(false\)'
require_regex "src/modules/volume/VDB/IO/DWriteOpenVDB.cpp" \
  'nAttr\.setConnectable\(false\)'
require_regex "src/modules/volume/VDB/IO/DWriteOpenVDB.cpp" \
  'isSafeExportPath\(filename,[[:space:]]*"\.vdb"\)'
require_fixed_text "src/modules/point/DPoint/Utility/DPCache.cpp" \
  '"CreateMissingCache", "cmc"'
require_regex "src/modules/point/DPoint/Utility/DPCache.cpp" \
  'MFnNumericData::kBoolean,[[:space:]]*false,[[:space:]]*&stat'
require_regex "src/modules/point/DPoint/Utility/DPCache.cpp" \
  'nAttr\.setStorable\(false\)'
require_regex "src/modules/point/DPoint/Utility/DPCache.cpp" \
  'nAttr\.setConnectable\(false\)'
require_regex "src/modules/curve/DCurve/IO/DWriteDCurveToAi.cpp" \
  'aExportMode[[:space:]]*=[[:space:]]*eAttr\.create\("ExportMode",[[:space:]]*"expm",[[:space:]]*2'
require_regex "src/modules/dynamics/DBullet/RigidBody/DBRigidBodyArrayNode.cpp" \
  'ia_fileIO[[:space:]]*=[^;]*MFnNumericData::kBoolean,[[:space:]]*0'
require_regex "src/modules/dynamics/DBullet/RigidBody/DBRigidBodyArrayNode.cpp" \
  'fnNumericAttr\.setStorable\(false\)'
require_regex "src/modules/dynamics/DBullet/RigidBody/DBRigidBodyArrayNode.cpp" \
  'fnNumericAttr\.setConnectable\(false\)'
for generator in \
  "src/modules/generators/Generate/Human/DHEyeGen.h" \
  "src/modules/generators/Generate/Human/DHEyelashGen.h" \
  "src/modules/generators/Generate/LeafGen/LeafGen.h" \
  "src/modules/generators/Generate/DBubbleCrystal.h" \
  "src/modules/generators/Generate/DCurveDuplicator.h"; do
  require_regex "$generator" 'schedulingType\(\) const override.*kSerial'
done
for stateful_node in \
  "src/modules/generators/Generate/motionDuplicator.h" \
  "src/modules/generators/legacy/surfaceDuplicater.h" \
  "src/core/DDataUtility/Particle/nobject/DPnParticle.h" \
  "src/core/UtilityNode/DStretchData/DStretchData.h" \
  "src/core/DDataUtility/Particle/nobject/DPnPopSolver.h" \
  "src/modules/point/DPoint/Utility/DPTimeShift.h" \
  "src/modules/point/DPoint/Duplicator/DPInstancer.h" \
  "src/modules/dynamics/DBullet/RigidBody/DBRigidBodyNode.h" \
  "src/modules/dynamics/DBullet/RigidBody/DBRigidBodyArrayNode.h" \
  "src/modules/dynamics/DBullet/SoftBody/DBSoftBodyNode.h"; do
  require_regex "$stateful_node" 'schedulingType\(\) const override.*kUntrusted'
done
for serial_node in \
  "src/modules/curve/DCurve/Solver/DCSolver.h" \
  "src/modules/mesh/DMesh/Solver/DMSolver.h" \
  "src/modules/point/DPoint/Solver/DPSolver.h" \
  "src/core/DataModifier/ShapeSequencer/DShapeSequencer.h" \
  "src/core/DDataUtility/IO/ReadDPointDataNode.h"; do
  require_regex "$serial_node" 'schedulingType\(\) const override.*kSerial'
done
require_regex "src/modules/point/DPoint/Utility/DPCache.h" \
  'schedulingType\(\) const override.*kGloballySerialize'
forbid_regex "src/core/DDataUtility/Utility/DPMerge.cpp" 'EchoWarning|std::cout|(^|[^[:alnum:]_])cout[[:space:]]*<<'
forbid_regex "src/modules/volume/VDB/Convert/DOpenVDBToMesh.cpp" '\([[:space:]]*DMeshData[[:space:]]*\*[[:space:]]*\)'
forbid_regex "src/modules/dynamics/DBullet/Solver/DBSolverNode.cpp" 'pause[[:space:]]+-sec|Calling dSolverNode::compute'

namespace_leaks="$({
  find "$repo_root" \
    \( -path "$repo_root/.git" \
       -o -path "$repo_root/build" \
       -o -path "$repo_root/build-*" \
       -o -path "$repo_root/third_party" \
       -o -path "$repo_root/backup" \
       -o -path '*/backup' \
       -o -path '*/old' \) -prune -o \
    -type f \( -name '*.h' -o -name '*.hpp' \) \
    -exec grep -nHE '^using[[:space:]]+namespace[[:space:]]+(std|cv|Eigen|MHWRender|mvl)[[:space:]]*;' {} +
} || true)"

if [[ -n "$namespace_leaks" ]]; then
  echo "Namespace directive found in a project-owned header:" >&2
  echo "$namespace_leaks" >&2
  exit 1
fi

echo "Maya node compatibility and reference-contract check: OK"
