#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "$0")/.." && pwd)"
build_files=(
  "$repo_root/CMakeLists.txt"
  "$repo_root/cmake/DommyActiveSources.cmake"
  "$repo_root/cmake/DommyFullSourceCheck.cmake"
  "$repo_root/config/active_cpp_sources.txt"
)

for required_cmake_text in \
  'option(DOMMY_BUILD_FULL_SOURCE_CHECK' \
  'option(DOMMY_BUILD_FULL_LINK_CHECK' \
  'option(DOMMY_BUILD_PLUGIN' \
  'dommy_full_source_check' \
  'dommy_full_link_check_target' \
  'add_library(dommyTool MODULE' \
  'config/dommyTool.mod.in' \
  'DOMMY_LEGACY_LINK_LIBRARIES' \
  'dommy_core_source_check' \
  'dommy_point_source_check' \
  'dommy_curve_source_check' \
  'dommy_mesh_source_check' \
  'dommy_volume_source_check' \
  'dommy_dynamics_source_check' \
  'list_active_cpp_sources.py' \
  'config/active_cpp_sources.txt' \
  'include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/DommyActiveSources.cmake")' \
  'DOMMY_ACTIVE_CPP_MANIFEST' \
  'CMAKE_CONFIGURE_DEPENDS' \
  'DOMMY_ACTIVE_CPP_SOURCES' \
  'dommy_fastnoise_source_check' \
  'dommy_libnoise_source_check' \
  'dommy_voro_source_check' \
  'option(DOMMY_ENABLE_ARNOLD_NODES' \
	  'option(DOMMY_ENABLE_BULLET_NODES' \
	  'option(DOMMY_ENABLE_EXPERIMENTAL_BULLET_TEST_NODE' \
	  'option(DOMMY_ENABLE_EXPERIMENTAL_FRACTURE_SOLVER' \
  'option(DOMMY_ENABLE_EXPERIMENTAL_STRETCH_NODE' \
  'option(DOMMY_ENABLE_EXPERIMENTAL_ARRAY_TO_COMPOUND_NODE' \
  'option(DOMMY_ENABLE_OPENVDB_NODES' \
  'option(DOMMY_ENABLE_DG_FILE_WRITERS' \
  'option(DOMMY_ENABLE_JET_NODES' \
  'option(DOMMY_ENABLE_FASTNOISE_NODES' \
  'option(DOMMY_ENABLE_FREETYPE_TEXT_NODE'; do
  if ! grep -R -qF "$required_cmake_text" "$repo_root/CMakeLists.txt" "$repo_root/cmake"; then
    echo "Required full-source CMake boundary is missing: $required_cmake_text" >&2
    exit 1
  fi
done

if ! sed -n '/set(DOMMY_LEGACY_INCLUDE_ROOTS/,/^)/p' \
  "$repo_root/CMakeLists.txt" | grep -qF '"${DOMMY_SOURCE_ROOT}"'; then
  echo "Source-root-qualified legacy includes are not covered by CMake." >&2
  exit 1
fi

if ! grep -qF '#define DOMMY_WITH_DG_FILE_WRITERS 0' \
  "$repo_root/src/plugin/PluginFeatures.h" ||
   ! grep -qF '#if DOMMY_WITH_DG_FILE_WRITERS' \
  "$repo_root/src/plugin/registration/modules/OpenVDBRegistrations.inl" ||
   ! grep -qF '#if DOMMY_WITH_DG_FILE_WRITERS' \
  "$repo_root/src/modules/point/DPoint/render/light/DPLightGen.cpp" ||
   ! grep -qF '#if DOMMY_WITH_DG_FILE_WRITERS' \
  "$repo_root/src/modules/dynamics/DBullet/Solver/DBSolverNode.cpp" ||
   [[ "$(grep -cF '#if DOMMY_WITH_DG_FILE_WRITERS' \
     "$repo_root/src/plugin/registration/modules/ArnoldRegistrations.inl")" -ne 2 ]]; then
  echo "Dependency-graph file writers must remain an explicit opt-in." >&2
  exit 1
fi

if ! grep -qF '#define DOMMY_WITH_FASTNOISE 0' \
  "$repo_root/src/plugin/PluginFeatures.h" ||
   ! grep -qF '#define DOMMY_WITH_FREETYPE_TEXT 0' \
  "$repo_root/src/plugin/PluginFeatures.h" ||
   ! grep -qF '#if DOMMY_WITH_FASTNOISE' \
  "$repo_root/src/plugin/registration/modules/PointRegistrations.inl" ||
   ! grep -qF '#if DOMMY_WITH_FASTNOISE' \
  "$repo_root/src/plugin/registration/modules/MeshRegistrations.inl" ||
   ! grep -qF '#if DOMMY_WITH_FREETYPE_TEXT' \
  "$repo_root/src/plugin/registration/modules/CurveRegistrations.inl" ||
   ! grep -qF 'DPFastNoise|DMFastNoise' \
  "$repo_root/cmake/DommyFullSourceCheck.cmake" ||
   ! grep -qF '/DCText\\.cpp$' \
  "$repo_root/cmake/DommyFullSourceCheck.cmake"; then
  echo "External FastNoise and FreeType nodes must remain explicit opt-ins." >&2
  exit 1
fi

if grep -R -nE \
  'DOMMY_ENABLE_CGAL_NODES|DOMMY_WITH_CGAL|CGAL_LOCATION|CGAL_DIR|-lCGAL|-lgmp|-lmpfr' \
  "$repo_root/CMakeLists.txt" \
  "$repo_root/cmake" \
  "$repo_root/config" \
  "$repo_root/src/plugin" >/dev/null; then
  echo "Removed CGAL/GMP/MPFR dependencies must not remain in the build surface." >&2
  exit 1
fi

if ! grep -qF '#if DOMMY_WITH_EXPERIMENTAL_FRACTURE_SOLVER' \
  "$repo_root/src/plugin/registration/modules/BulletRegistrations.inl"; then
  echo "Experimental fracture solver must remain disabled by default." >&2
  exit 1
fi

if ! grep -qF '#define DOMMY_WITH_EXPERIMENTAL_BULLET_TEST_NODE 0' \
  "$repo_root/src/plugin/PluginFeatures.h" ||
   ! grep -qF '#if DOMMY_WITH_EXPERIMENTAL_BULLET_TEST_NODE' \
  "$repo_root/src/plugin/registration/modules/BulletRegistrations.inl"; then
  echo "Experimental DMBulletTest node must remain disabled by default." >&2
  exit 1
fi

if ! grep -qF 'DBulletFractureSolver|DBFractureDynamicsWorld|dbtFractureBody' \
  "$repo_root/cmake/DommyFullSourceCheck.cmake"; then
  echo "Experimental fracture sources must be excluded from normal Bullet builds." >&2
  exit 1
fi

for forbidden in \
  '/Users/' \
  '/usr/local' \
  '/opt/homebrew' \
  '/Applications/' \
  'c++0x' \
  'BulletDynamics/btBulletDynamicsCommon.h' \
  'BulletCollision/btBulletCollisionCommon.h'; do
  if grep -nF "$forbidden" "${build_files[@]}" >/dev/null; then
    echo "Non-portable native build setting found: $forbidden" >&2
    grep -nF "$forbidden" "${build_files[@]}" >&2
    exit 1
  fi
done

source_include_errors="$({
  find "$repo_root/src/modules/dynamics/DBullet" "$repo_root/src/modules/mesh/DMesh" \
    \( -path '*/old' -o -path '*/backup' \) -prune -o \
    -type f \( -name '*.cpp' -o -name '*.h' -o -name '*.hpp' \) \
    -exec grep -nHE \
      'Bullet(Dynamics|Collision)/btBullet(Dynamics|Collision)Common\.h|DB_(Nail|Hinge|Slider|Sixdof)_constraint_t\.h' \
      {} +
} || true)"

if [[ -n "$source_include_errors" ]]; then
  echo "Non-portable Bullet include found in active source:" >&2
  echo "$source_include_errors" >&2
  exit 1
fi

for required in \
  '$ENV{MAYA_LOCATION}' \
  '$ENV{MAYA_DEVKIT_LOCATION}' \
  '$ENV{DOMMY_DEPENDENCY_PREFIX}' \
  '$ENV{DOMMY_LEGACY_PREFIX}' \
  '$ENV{FASTNOISE_LOCATION}'; do
  if ! grep -qF "$required" "${build_files[@]}"; then
    echo "Required environment-backed build setting is missing: $required" >&2
    exit 1
  fi
done

obsolete_bullet_links="$({
  grep -nE -- \
    '-l(Bullet2FileLoader|Bullet3[^" ]*|BulletFileLoader|BulletInverseDynamics[^" ]*|BulletRobotics|BulletWorldImporter|BulletXmlWorldImporter|GIMPACTUtils)' \
    "${build_files[@]}"
} || true)"

if [[ -n "$obsolete_bullet_links" ]]; then
  echo "Unused Bullet link dependencies remain in the native build configuration:" >&2
  echo "$obsolete_bullet_links" >&2
  exit 1
fi

if grep -qE 'USE_RENDERMAN|particleToReal[Ff]low' "${build_files[@]}"; then
  echo "Removed RenderMan/RealFlow integration remains in the native build configuration." >&2
  exit 1
fi

cpp_file_count="$(find "$repo_root/src" -type f \
  \( -name '*.c' -o -name '*.cc' -o -name '*.cpp' -o -name '*.cxx' \
     -o -name '*.h' -o -name '*.hpp' -o -name '*.inl' \) | wc -l | tr -d '[:space:]')"
documented_cpp_file_count="$(sed -nE \
  's/^The repository currently contains ([0-9]+) C\/C\+\+ source, header, and inline files under .*/\1/p' \
  "$repo_root/docs/ARCHITECTURE.md")"
if [[ ! "$documented_cpp_file_count" =~ ^[0-9]+$ ]] ||
   [[ "$cpp_file_count" -ne "$documented_cpp_file_count" ]]; then
  echo "The documented C++ source inventory is stale: actual=$cpp_file_count, documented=$documented_cpp_file_count." >&2
  exit 1
fi

echo "Native build configuration portability check: OK"
