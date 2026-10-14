#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "$0")/.." && pwd)"
test_binary="${TMPDIR:-/tmp}/dommytool_math_memory_tests"
arnold_resource_test_binary="${TMPDIR:-/tmp}/dommytool_arnold_resource_tests"

"$repo_root/tests/check_maya_data_ownership.sh"
"$repo_root/tests/check_source_safety.sh"
"$repo_root/tests/check_build_configuration.sh"
bash "$repo_root/tests/check_active_source_manifest.sh"
python3 "$repo_root/tests/check_active_cpp_safety.py"
bash "$repo_root/tests/check_cpp_regressions.sh"
"$repo_root/tests/check_repository_layout.sh"

"${CXX:-clang++}" \
  -std=c++11 \
  -Wall -Wextra -Wpedantic \
  -fsanitize=address,undefined,float-cast-overflow \
  -fno-sanitize-recover=all \
  -fno-omit-frame-pointer \
  -I"$repo_root/tests/stubs" \
  -I"$repo_root/src" \
  -I"$repo_root/src/plugin" \
  -I"$repo_root/src/core" \
  -I"$repo_root/src/core/math" \
  -I"$repo_root/src/core/noise/Poisson" \
  -I"$repo_root/src/core/Utility" \
  -I"$repo_root/src/core/Utility/3d/Math" \
  -I"$repo_root/src/core/Utility/3d/DataStruct" \
  -I"$repo_root/src/modules/point" \
  -I"$repo_root/src/modules/point/DPoint" \
  -I"$repo_root/src/modules/curve" \
  -I"$repo_root/src/modules/particles" \
  -I"$repo_root/src/modules/dynamics/DBullet/File" \
  -I"$repo_root/src/modules/dynamics/DBullet/mvl" \
  "$repo_root/tests/cpp/math_memory_tests.cpp" \
  "$repo_root/tests/cpp/attribute_cast_group_tests.cpp" \
  "$repo_root/tests/cpp/expression_sort_tests.cpp" \
  "$repo_root/tests/cpp/point_generation_tests.cpp" \
  "$repo_root/tests/cpp/random_sampling_tests.cpp" \
  "$repo_root/tests/cpp/blue_noise_tests.cpp" \
  "$repo_root/tests/cpp/attribute_access_tests.cpp" \
  "$repo_root/tests/cpp/point_storage_tests.cpp" \
  "$repo_root/tests/cpp/curve_sampling_tests.cpp" \
  "$repo_root/tests/cpp/kill_field_math_tests.cpp" \
  "$repo_root/tests/cpp/utility_vector_tests.cpp" \
  "$repo_root/tests/cpp/atomic_output_tests.cpp" \
  "$repo_root/tests/cpp/io_locale_tests.cpp" \
  "$repo_root/src/core/DDataUtility/Data/DPGroupData.cpp" \
  "$repo_root/tests/cpp/geometry_tests.cpp" \
  "$repo_root/tests/cpp/mesh_boundary_tests.cpp" \
  "$repo_root/tests/cpp/compute_guard_compile_test.cpp" \
  "$repo_root/tests/cpp/poisson_header_compile_test.cpp" \
  "$repo_root/tests/cpp/vdb_compute_guard_compile_test.cpp" \
  "$repo_root/src/core/math/dommyMath.cpp" \
  "$repo_root/src/core/common/object/DLine3DOp.cpp" \
  "$repo_root/src/core/common/object/DPlaneOp.cpp" \
  "$repo_root/src/core/noise/PerlinNoise.cpp" \
  "$repo_root/src/modules/dynamics/DBullet/File/pdbIO.cpp" \
  -o "$test_binary"

if [[ "$(uname -s)" == "Darwin" ]]; then
  ASAN_OPTIONS=detect_leaks=0 "$test_binary"
else
  ASAN_OPTIONS=detect_leaks=1 "$test_binary"
fi

"${CXX:-clang++}" \
  -std=c++11 \
  -Wall -Wextra -Wpedantic \
  -fsanitize=address,undefined \
  -fno-sanitize-recover=all \
  -fno-omit-frame-pointer \
  -I"$repo_root/tests/stubs/arnold" \
  -I"$repo_root/src" \
  "$repo_root/tests/cpp/arnold_resources_test.cpp" \
  -o "$arnold_resource_test_binary"

if [[ "$(uname -s)" == "Darwin" ]]; then
  ASAN_OPTIONS=detect_leaks=0 "$arnold_resource_test_binary"
else
  ASAN_OPTIONS=detect_leaks=1 "$arnold_resource_test_binary"
fi

echo "C++ math and memory-safety tests: OK"
