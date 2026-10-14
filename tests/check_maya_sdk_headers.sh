#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "$0")/.." && pwd)"
maya_location="${MAYA_LOCATION:-${1:-}}"

if [[ -z "$maya_location" ]]; then
  echo "Usage: MAYA_LOCATION=/path/to/maya ./tests/check_maya_sdk_headers.sh" >&2
  exit 2
fi

compiler="${CXX:-clang++}"
python3 "$repo_root/tests/check_utility_scalar_safety.py" --sdk "$maya_location"
python3 "$repo_root/tests/check_diagnostic_text_safety.py" --sdk "$maya_location"
python3 "$repo_root/tests/check_command_transactions.py" --sdk "$maya_location"
python3 "$repo_root/tests/check_point_export_safety.py" --sdk "$maya_location"
python3 "$repo_root/tests/check_mesh_duplication_vertices.py" --sdk "$maya_location"
python3 "$repo_root/tests/check_viewport_staging_safety.py" --sdk "$maya_location"
python3 "$repo_root/tests/check_vdb_viewport_safety.py" --maya-sdk "$maya_location"
python3 "$repo_root/tests/check_draw_callback_safety.py" --sdk "$maya_location"
python3 "$repo_root/tests/check_light_draw_preparation_safety.py" --sdk "$maya_location"
python3 "$repo_root/tests/check_display_metadata_safety.py" --sdk "$maya_location"
python3 "$repo_root/tests/check_arnold_export_safety.py" --sdk "$maya_location"
python3 "$repo_root/tests/check_export_input_safety.py" --sdk "$maya_location"
python3 "$repo_root/tests/check_export_input_safety.py" --alembic --sdk "$maya_location"
python3 "$repo_root/tests/check_arnold_translation_safety.py" --sdk "$maya_location"
python3 "$repo_root/tests/check_arnold_mesh_metadata_safety.py" --sdk "$maya_location"
python3 "$repo_root/tests/check_alembic_sample_safety.py" --sdk "$maya_location"
python3 "$repo_root/tests/check_alembic_transform_safety.py" --sdk "$maya_location"
python3 "$repo_root/tests/check_alembic_root_safety.py" --sdk "$maya_location"
python3 "$repo_root/tests/check_alembic_callback_safety.py" --sdk "$maya_location"
python3 "$repo_root/tests/check_alembic_geometry_safety.py" --sdk "$maya_location"
python3 "$repo_root/tests/check_alembic_destination_safety.py" --sdk "$maya_location"
python3 "$repo_root/tests/check_alembic_job_bounds_safety.py" --sdk "$maya_location"
python3 "$repo_root/tests/check_alembic_attribute_state_safety.py" --sdk "$maya_location"
python3 "$repo_root/tests/check_bullet_input_budget.py" --sdk "$maya_location"
python3 "$repo_root/tests/check_bullet_connection_safety.py" --sdk "$maya_location"
python3 "$repo_root/tests/check_archival_connection_safety.py" --sdk "$maya_location"
python3 "$repo_root/tests/check_bullet_dm_safety.py" --sdk "$maya_location"
python3 "$repo_root/tests/check_nrigid_safety.py" --sdk "$maya_location"
python3 "$repo_root/tests/check_nucleus_particle_safety.py" --sdk "$maya_location"
python3 "$repo_root/tests/check_bullet_mesh_input_safety.py" --sdk "$maya_location"

python3 "$repo_root/tests/check_default_ramp_safety.py" --sdk "$maya_location"
python3 "$repo_root/tests/check_packing_emitter_safety.py" --sdk "$maya_location"
python3 "$repo_root/tests/check_packing_input_safety.py" --sdk "$maya_location"
python3 "$repo_root/tests/check_geometry_read_safety.py" --sdk "$maya_location"
python3 "$repo_root/tests/check_curve_boundary_safety.py" --sdk-connections "$maya_location"
python3 "$repo_root/tests/check_timeshift_safety.py" --sdk "$maya_location"
python3 "$repo_root/tests/check_scalar_array_safety.py" --sdk "$maya_location"
python3 "$repo_root/tests/check_array_utility_safety.py" --sdk "$maya_location"
python3 "$repo_root/tests/check_numeric_to_float_safety.py" --sdk "$maya_location"
python3 "$repo_root/tests/check_procedural_api_safety.py" --sdk-boundary "$maya_location"
python3 "$repo_root/tests/check_motion_history_safety.py" --sdk "$maya_location"
python3 "$repo_root/tests/check_motion_duplicator_safety.py" --sdk "$maya_location"
python3 "$repo_root/tests/check_motion_trail_safety.py" --sdk "$maya_location"
python3 "$repo_root/tests/check_generator_boundary_safety.py" --sdk "$maya_location"
python3 "$repo_root/tests/check_surface_uv_wrapping.py" --sdk "$maya_location"
python3 "$repo_root/tests/check_surface_scratch_safety.py" --sdk "$maya_location"
python3 "$repo_root/tests/check_vdb_field_safety.py" --sdk "$maya_location"
python3 "$repo_root/tests/check_mesh_generation_safety.py" --sdk "$maya_location"
python3 "$repo_root/tests/check_curve_transactions.py" --sdk "$maya_location"
python3 "$repo_root/tests/check_wire_array_safety.py" --sdk "$maya_location"
python3 "$repo_root/tests/check_mesh_utility_safety.py" --sdk "$maya_location"
python3 "$repo_root/tests/check_mesh_refinement_safety.py" --sdk "$maya_location"
python3 "$repo_root/tests/check_mesh_controls_safety.py" --sdk "$maya_location"
python3 "$repo_root/tests/check_mesh_remesh_safety.py" --sdk "$maya_location"
python3 "$repo_root/tests/check_mesh_noise_initialization.py" --sdk "$maya_location"
python3 "$repo_root/tests/check_mesh_group_safety.py" --sdk "$maya_location"
python3 "$repo_root/tests/check_primitive_metadata_safety.py" --sdk "$maya_location"
python3 "$repo_root/tests/check_group_transfer_safety.py" --sdk "$maya_location"
python3 "$repo_root/tests/check_point_utility_safety.py" --sdk "$maya_location"
python3 "$repo_root/tests/check_point_batch_transactions.py" --sdk "$maya_location"
python3 "$repo_root/tests/check_motion_rotation_safety.py" --sdk "$maya_location"
python3 "$repo_root/tests/check_animation_boundary_safety.py" --sdk "$maya_location"
python3 "$repo_root/tests/check_copy_utility_safety.py" --sdk "$maya_location"
python3 "$repo_root/tests/check_point_edit_safety.py" --sdk "$maya_location"
python3 "$repo_root/tests/check_direction_search_safety.py" --sdk "$maya_location"
python3 "$repo_root/tests/check_point_force_safety.py" --sdk "$maya_location"
python3 "$repo_root/tests/check_point_coordinate_output.py" --sdk "$maya_location"
python3 "$repo_root/tests/check_point_noise_output.py" --sdk "$maya_location"
python3 "$repo_root/tests/check_point_deformation_output.py" --sdk "$maya_location"
python3 "$repo_root/tests/check_collision_distance_safety.py" --sdk "$maya_location"
python3 "$repo_root/tests/check_affect_spiral_safety.py" --sdk "$maya_location"
python3 "$repo_root/tests/check_ray_safety.py" --sdk "$maya_location"
python3 "$repo_root/tests/check_color_noise_safety.py" --sdk "$maya_location"
python3 "$repo_root/tests/check_point_cache_safety.py" --sdk "$maya_location"
python3 "$repo_root/tests/check_vdb_point_bridge_safety.py" --sdk "$maya_location"
python3 "$repo_root/tests/check_vdb_smoothing_safety.py" --sdk "$maya_location"
python3 "$repo_root/tests/check_vdb_vector_merge_safety.py" --sdk "$maya_location"
python3 "$repo_root/tests/check_vdb_particle_safety.py" --sdk "$maya_location"
python3 "$repo_root/tests/check_mesh_normal_safety.py" --sdk "$maya_location"
python3 "$repo_root/tests/check_point_deform_safety.py" --sdk "$maya_location"
python3 "$repo_root/tests/check_convert_weights_safety.py" --sdk "$maya_location"
python3 "$repo_root/tests/check_adjust_space_safety.py" --sdk "$maya_location"
python3 "$repo_root/tests/check_point_uv_mesh_safety.py" --sdk "$maya_location"
python3 "$repo_root/tests/check_voronoi_safety.py" --sdk "$maya_location"
python3 "$repo_root/tests/check_polygon_generation_safety.py" --sdk "$maya_location"

"$compiler" \
  -std=c++11 \
  -Wall -Wextra -Wpedantic \
  -fsyntax-only \
  -isystem "$maya_location/include" \
  -I"$repo_root/src/plugin" \
  -I"$repo_root/src/core" \
  -I"$repo_root/src/modules/point" \
  -I"$repo_root/src/modules/curve" \
  -I"$repo_root/src/modules/mesh" \
  -I"$repo_root/src/modules/volume" \
  -I"$repo_root/src/modules/particles" \
  "$repo_root/tests/cpp/maya_sdk_registration_compile_test.cpp" \
  "$repo_root/tests/cpp/maya_node_contract_compile_test.cpp"

# Maya 2025's MHWGeometry header itself uses a C++14 return type.  The helper
# remains C++11-tested against the local stub suite and is also checked here
# against the installed SDK's real Viewport 2.0 declarations.
"$compiler" \
  -std=c++14 \
  -Wall -Wextra -Wpedantic \
  -fsyntax-only \
  -isystem "$maya_location/include" \
  -I"$repo_root/src/core" \
  "$repo_root/tests/cpp/maya_safety_helpers_compile_test.cpp"

"$compiler" \
  -std=c++11 \
  -Wall -Wextra -Wpedantic \
  -fsyntax-only \
  -isystem "$maya_location/include" \
  -I"$repo_root/src/plugin" \
  -I"$repo_root/src/core" \
  -I"$repo_root/src/core/Data" \
  -I"$repo_root/src/core/Utility" \
  -I"$repo_root/src/core/Utility/3d/Math" \
  -I"$repo_root/src/modules/point" \
  -I"$repo_root/src/modules/point/DPoint" \
  -I"$repo_root/src/modules/point/DPoint/Common" \
  -I"$repo_root/src/core/DDataUtility/Data" \
  -I"$repo_root/src/modules/mesh" \
  -I"$repo_root/src/modules/volume" \
  -idirafter "$repo_root/tests/stubs" \
  "$repo_root/src/core/DDataUtility/Data/DPGroupData.cpp" \
  "$repo_root/src/core/DDataUtility/Utility/DPMerge.cpp"

"$compiler" \
  -std=c++11 \
  -Wall -Wextra -Wpedantic \
  -fsyntax-only \
  -isystem "$maya_location/include" \
  -I"$repo_root/src/core" \
  -I"$repo_root/src/modules/rendering/global/render" \
  "$repo_root/src/core/common/file/DFileOperation.cpp" \
  "$repo_root/src/commands/setParticleRGBCmd.cpp"

"$compiler" \
  -std=c++11 \
  -Wall -Wextra -Wpedantic \
  -fsyntax-only \
  -isystem "$maya_location/include" \
  -I"$repo_root/src/core" \
  "$repo_root/tests/cpp/maya_particle_field_compile_test.cpp"

"$compiler" \
  -std=c++11 \
  -Wall -Wextra -Wpedantic \
  -fsyntax-only \
  -isystem "$maya_location/include" \
  -I"$repo_root/src/core" \
  -I"$repo_root/src/modules/volume" \
  "$repo_root/tests/cpp/maya_fluid_grid_compile_test.cpp"

project_include_args=()
while IFS= read -r directory; do
  project_include_args+=("-I$directory")
done < <(find "$repo_root/src" -type d)

# Keep the lightweight feedback solvers, curve resampling, bounded geometry
# generators, and the polyBlast history path checked as complete translation
# units. These nodes only require the Maya SDK and the checked-in libnoise
# source, so they remain available even when optional legacy dependencies are
# absent.
"$compiler" \
  -std=c++14 \
  -Wall -Wextra -Wpedantic -Werror \
  -Wno-deprecated-declarations \
  -Wno-unknown-pragmas \
  -fsyntax-only \
  -isystem "$maya_location/include" \
  "${project_include_args[@]}" \
  -isystem "$repo_root/third_party/libnoise/source" \
  -idirafter "$repo_root/tests/stubs" \
  "$repo_root/src/commands/getCurveLengthPosCmd.cpp" \
  "$repo_root/src/commands/shapeFillCmd.cpp" \
  "$repo_root/src/commands/surfaceDuplicaterCmd.cpp" \
  "$repo_root/src/commands/spaceDuplicaterCmd.cpp" \
  "$repo_root/src/modules/mesh/polyModifier/basePolyModifierFty.cpp" \
  "$repo_root/src/modules/mesh/polyModifier/basePolyModifierNode.cpp" \
  "$repo_root/src/modules/mesh/polyModifier/polyBlast/polyBlastAction.cpp" \
  "$repo_root/src/modules/mesh/polyModifier/polyBlast/polyBlastCmd.cpp" \
  "$repo_root/src/modules/mesh/polyModifier/polyBlast/polyBlastFty.cpp" \
  "$repo_root/src/modules/mesh/polyModifier/polyBlast/polyBlastNode.cpp" \
  "$repo_root/src/core/DataModifier/ShapeSequencer/DShapeSequencer.cpp" \
  "$repo_root/src/modules/deformers/transform/DPointToSurfaceTransformNode.cpp" \
  "$repo_root/src/modules/curve/Curve/Node/DBSCurve.cpp" \
  "$repo_root/src/modules/curve/Curve/Node/DResampleCurve.cpp" \
  "$repo_root/src/modules/curve/Curve/Node/remakeCurveNode.cpp" \
  "$repo_root/src/modules/generators/Generate/DBubbleCrystal.cpp" \
  "$repo_root/src/modules/generators/Generate/DCurveDuplicator.cpp" \
  "$repo_root/src/modules/generators/icosahedron/icosahedronNode.cpp" \
  "$repo_root/src/modules/point/DPoint/Solver/DPSolver.cpp" \
  "$repo_root/src/modules/point/DPoint/animation/DPMotionCreate.cpp" \
  "$repo_root/src/modules/point/DPoint/animation/DPRotation.cpp" \
  "$repo_root/src/modules/point/DPoint/animation/DPAnimMatrix.cpp" \
  "$repo_root/src/modules/point/DPoint/animation/DPSignal.cpp" \
  "$repo_root/src/modules/point/DPoint/animation/DPSignal3D.cpp" \
  "$repo_root/src/modules/curve/DCurve/Generate/DCurveFromCurve.cpp" \
  "$repo_root/src/modules/curve/DCurve/Generate/DCurveFromSurface.cpp" \
  "$repo_root/src/modules/curve/DCurve/Generate/DCurveClothFromSurface.cpp" \
  "$repo_root/src/modules/curve/DCurve/Utility/DCFitCopy.cpp" \
  "$repo_root/src/modules/curve/DCurve/Utility/DCCopy.cpp" \
  "$repo_root/src/modules/curve/DCurve/Utility/DCMeshCopy.cpp" \
  "$repo_root/src/modules/curve/DCurve/Utility/DCReplicator.cpp" \
  "$repo_root/src/modules/curve/DCurve/Utility/DCNormal.cpp" \
  "$repo_root/src/modules/curve/DCurve/Utility/DCReform.cpp" \
  "$repo_root/src/modules/curve/DCurve/Utility/DCResample.cpp" \
  "$repo_root/src/modules/curve/DCurve/Utility/DCCharactor.cpp" \
  "$repo_root/src/modules/curve/DCurve/Utility/DCMotionPath.cpp" \
  "$repo_root/src/modules/curve/DCurve/Utility/DCDivide.cpp" \
  "$repo_root/src/modules/curve/DCurve/Utility/DCStitch.cpp" \
  "$repo_root/src/modules/curve/DCurve/Utility/DCExtend.cpp" \
  "$repo_root/src/modules/curve/DCurve/Utility/DCFillBetween.cpp" \
  "$repo_root/src/modules/curve/DCurve/Utility/DCConnect.cpp" \
  "$repo_root/src/modules/curve/DCurve/Utility/DCReplace.cpp" \
  "$repo_root/src/modules/curve/DCurve/Utility/DCOffset.cpp" \
  "$repo_root/src/modules/curve/DCurve/Utility/DCCutObject.cpp" \
  "$repo_root/src/modules/curve/DCurve/Utility/DCReflect.cpp" \
  "$repo_root/src/modules/curve/DCurve/Utility/DCConvex2D.cpp" \
  "$repo_root/src/modules/curve/DCurve/Utility/DCPartition.cpp" \
  "$repo_root/src/modules/curve/DCurve/Utility/DCDelete.cpp" \
  "$repo_root/src/modules/curve/DCurve/Utility/DCExplodeView.cpp" \
  "$repo_root/src/modules/curve/DCurve/Utility/DCWeightMap.cpp" \
  "$repo_root/src/modules/curve/DCurve/Utility/DCUVMap.cpp" \
  "$repo_root/src/modules/curve/DCurve/Utility/DCClumping.cpp" \
  "$repo_root/src/core/DDataUtility/Generate/DPointCurveEmit.cpp" \
  "$repo_root/src/core/DDataUtility/Generate/DPointMeshEmit.cpp" \
  "$repo_root/src/core/DDataUtility/Generate/DPointCircleEmit.cpp" \
  "$repo_root/src/core/DDataUtility/Generate/DPointSurfaceEmit.cpp" \
  "$repo_root/src/core/DDataUtility/Generate/DPointPlaneEmit.cpp" \
  "$repo_root/src/core/DDataUtility/Generate/DPAnimationEmit.cpp" \
  "$repo_root/src/core/DDataUtility/Generate/DPAddPoint.cpp" \
  "$repo_root/src/modules/curve/DCurve/DCurveUtil.cpp" \
  "$repo_root/src/core/common/object/DTransformOp.cpp" \
  "$repo_root/src/core/common/object/DNodeOperation.cpp" \
  "$repo_root/src/modules/curve/DCurve/Solver/DCSolver.cpp" \
  "$repo_root/src/modules/mesh/DMesh/Solver/DMSolver.cpp"

for analyzed_source in \
  "$repo_root/src/modules/mesh/polyModifier/polyBlast/polyBlastNode.cpp" \
  "$repo_root/src/core/DataModifier/ShapeSequencer/DShapeSequencer.cpp" \
  "$repo_root/src/modules/curve/Curve/Node/DBSCurve.cpp" \
  "$repo_root/src/modules/curve/Curve/Node/DResampleCurve.cpp" \
  "$repo_root/src/modules/generators/Generate/DBubbleCrystal.cpp" \
  "$repo_root/src/modules/generators/Generate/DCurveDuplicator.cpp" \
  "$repo_root/src/modules/point/DPoint/Solver/DPSolver.cpp" \
  "$repo_root/src/modules/point/DPoint/animation/DPMotionCreate.cpp" \
  "$repo_root/src/modules/point/DPoint/animation/DPRotation.cpp" \
  "$repo_root/src/modules/point/DPoint/animation/DPAnimMatrix.cpp" \
  "$repo_root/src/modules/point/DPoint/animation/DPSignal.cpp" \
  "$repo_root/src/modules/point/DPoint/animation/DPSignal3D.cpp" \
  "$repo_root/src/modules/curve/DCurve/Generate/DCurveFromCurve.cpp" \
  "$repo_root/src/modules/curve/DCurve/Generate/DCurveFromSurface.cpp" \
  "$repo_root/src/modules/curve/DCurve/Generate/DCurveClothFromSurface.cpp" \
  "$repo_root/src/modules/curve/DCurve/Utility/DCFitCopy.cpp" \
  "$repo_root/src/modules/curve/DCurve/Utility/DCCopy.cpp" \
  "$repo_root/src/modules/curve/DCurve/Utility/DCMeshCopy.cpp" \
  "$repo_root/src/modules/curve/DCurve/Utility/DCReplicator.cpp" \
  "$repo_root/src/modules/curve/DCurve/Utility/DCNormal.cpp" \
  "$repo_root/src/modules/curve/DCurve/Utility/DCReform.cpp" \
  "$repo_root/src/modules/curve/DCurve/Utility/DCResample.cpp" \
  "$repo_root/src/modules/curve/DCurve/Utility/DCCharactor.cpp" \
  "$repo_root/src/modules/curve/DCurve/Utility/DCMotionPath.cpp" \
  "$repo_root/src/modules/curve/DCurve/Utility/DCDivide.cpp" \
  "$repo_root/src/modules/curve/DCurve/Utility/DCStitch.cpp" \
  "$repo_root/src/modules/curve/DCurve/Utility/DCExtend.cpp" \
  "$repo_root/src/modules/curve/DCurve/Utility/DCFillBetween.cpp" \
  "$repo_root/src/modules/curve/DCurve/Utility/DCConnect.cpp" \
  "$repo_root/src/modules/curve/DCurve/Utility/DCReplace.cpp" \
  "$repo_root/src/modules/curve/DCurve/Utility/DCOffset.cpp" \
  "$repo_root/src/modules/curve/DCurve/Utility/DCCutObject.cpp" \
  "$repo_root/src/modules/curve/DCurve/Utility/DCReflect.cpp" \
  "$repo_root/src/modules/curve/DCurve/Utility/DCConvex2D.cpp" \
  "$repo_root/src/modules/curve/DCurve/Utility/DCPartition.cpp" \
  "$repo_root/src/modules/curve/DCurve/Utility/DCDelete.cpp" \
  "$repo_root/src/modules/curve/DCurve/Utility/DCExplodeView.cpp" \
  "$repo_root/src/modules/curve/DCurve/Utility/DCWeightMap.cpp" \
  "$repo_root/src/modules/curve/DCurve/Utility/DCUVMap.cpp" \
  "$repo_root/src/modules/curve/DCurve/Utility/DCClumping.cpp" \
  "$repo_root/src/core/DDataUtility/Generate/DPointCurveEmit.cpp" \
  "$repo_root/src/core/DDataUtility/Generate/DPointMeshEmit.cpp" \
  "$repo_root/src/core/DDataUtility/Generate/DPointCircleEmit.cpp" \
  "$repo_root/src/core/DDataUtility/Generate/DPointSurfaceEmit.cpp" \
  "$repo_root/src/core/DDataUtility/Generate/DPointPlaneEmit.cpp" \
  "$repo_root/src/core/DDataUtility/Generate/DPAnimationEmit.cpp" \
  "$repo_root/src/core/DDataUtility/Generate/DPAddPoint.cpp" \
  "$repo_root/src/modules/curve/DCurve/DCurveUtil.cpp" \
  "$repo_root/src/core/common/object/DTransformOp.cpp" \
  "$repo_root/src/core/common/object/DNodeOperation.cpp" \
  "$repo_root/src/modules/curve/DCurve/Solver/DCSolver.cpp" \
  "$repo_root/src/modules/mesh/DMesh/Solver/DMSolver.cpp"; do
  "$compiler" \
    --analyze \
    -std=c++14 \
    -w \
    -Xanalyzer -analyzer-output=text \
    -Xanalyzer -analyzer-werror \
    -o /dev/null \
    -isystem "$maya_location/include" \
    "${project_include_args[@]}" \
    -isystem "$repo_root/third_party/libnoise/source" \
    -idirafter "$repo_root/tests/stubs" \
    "$analyzed_source"
done

# Registered commands are user-facing mutation and inspection boundaries.
# Keep their modifier transactions, selection handling, and bounded reporting
# warning-clean against the real Maya API.
"$compiler" \
  -std=c++14 \
  -Wall -Wextra -Wpedantic -Werror \
  -Wno-deprecated-declarations \
  -Wno-unknown-pragmas \
  -fsyntax-only \
  -isystem "$maya_location/include" \
  "${project_include_args[@]}" \
  -idirafter "$repo_root/tests/stubs" \
  "$repo_root/src/modules/mesh/mesh/Cmd/DClusterWeightCmd.cpp" \
  "$repo_root/src/modules/generators/icosahedron/icosahedronCmd.cpp" \
  "$repo_root/src/commands/Cmd/BlindData/DBlindDataSelectCmd.cpp" \
  "$repo_root/src/commands/Cmd/Metadata/DMetadataSelectCmd.cpp" \
  "$repo_root/src/commands/woolCurveCmd.cpp"

for analyzed_source in \
  "$repo_root/src/commands/Cmd/BlindData/DBlindDataSelectCmd.cpp" \
  "$repo_root/src/commands/Cmd/Metadata/DMetadataSelectCmd.cpp" \
  "$repo_root/src/commands/woolCurveCmd.cpp"; do
  "$compiler" \
    --analyze \
    -std=c++14 \
    -w \
    -Xanalyzer -analyzer-output=text \
    -Xanalyzer -analyzer-werror \
    -o /dev/null \
    -isystem "$maya_location/include" \
    "${project_include_args[@]}" \
    -idirafter "$repo_root/tests/stubs" \
    "$analyzed_source"
done

# Dependency-node manipulators and direction generation are sensitive to Maya
# function-set signatures and evaluation-thread rules. Keep the paths that do
# not require SeExpr warning-clean even when only the SDK is available.
"$compiler" \
  -std=c++14 \
  -Wall -Wextra -Wpedantic -Werror \
  -Wno-deprecated-declarations \
  -Wno-unknown-pragmas \
  -fsyntax-only \
  -isystem "$maya_location/include" \
  "${project_include_args[@]}" \
  -idirafter "$repo_root/tests/stubs" \
  "$repo_root/src/modules/curve/DCurve/Group/DCGroup.cpp" \
  "$repo_root/src/modules/mesh/DMesh/Data/DMGroup.cpp" \
  "$repo_root/src/modules/point/DPoint/Utility/DPDirection.cpp"

"$compiler" \
  --analyze \
  -std=c++14 \
  -w \
  -Xanalyzer -analyzer-output=text \
  -Xanalyzer -analyzer-werror \
  -o /dev/null \
  -isystem "$maya_location/include" \
  "${project_include_args[@]}" \
  -idirafter "$repo_root/tests/stubs" \
  "$repo_root/src/modules/point/DPoint/Utility/DPDirection.cpp"

# Compile the mesh-analysis, conversion, component-pattern, and camera paths
# hardened for publication. These nodes are independent of the optional
# OpenVDB/Bullet/Arnold stacks and must remain warning-clean against a real SDK.
"$compiler" \
  -std=c++14 \
  -Wall -Wextra -Wpedantic -Werror \
  -Wno-deprecated-declarations \
  -Wno-unknown-pragmas \
  -fsyntax-only \
  -isystem "$maya_location/include" \
  "${project_include_args[@]}" \
  -idirafter "$repo_root/tests/stubs" \
  "$repo_root/src/core/UtilityNode/DNumericToFloatArray/DNumericToFloatArray.cpp" \
  "$repo_root/src/core/UtilityNode/DComponentListGen/DComponentListGen.cpp" \
  "$repo_root/src/modules/mesh/mesh/DSkinInfo.cpp" \
  "$repo_root/src/modules/mesh/mesh/DBSUpDownMesh.cpp" \
  "$repo_root/src/modules/mesh/mesh/DBSMirrorMesh.cpp" \
  "$repo_root/src/modules/mesh/mesh/DBSFlipMesh.cpp" \
  "$repo_root/src/modules/mesh/mesh/DPolySymmetry.cpp" \
  "$repo_root/src/modules/mesh/mesh/DPolyThickness.cpp" \
  "$repo_root/src/modules/rendering/Render/Camera/DCameraFrustum.cpp"

for analyzed_source in \
  "$repo_root/src/core/UtilityNode/DNumericToFloatArray/DNumericToFloatArray.cpp" \
  "$repo_root/src/core/UtilityNode/DComponentListGen/DComponentListGen.cpp" \
  "$repo_root/src/modules/mesh/mesh/DBSFlipMesh.cpp" \
  "$repo_root/src/modules/mesh/mesh/DPolySymmetry.cpp" \
  "$repo_root/src/modules/mesh/mesh/DPolyThickness.cpp" \
  "$repo_root/src/modules/rendering/Render/Camera/DCameraFrustum.cpp"; do
  "$compiler" \
    --analyze \
    -std=c++14 \
    -w \
    -Xanalyzer -analyzer-output=text \
    -Xanalyzer -analyzer-werror \
    -o /dev/null \
    -isystem "$maya_location/include" \
    "${project_include_args[@]}" \
    -idirafter "$repo_root/tests/stubs" \
    "$analyzed_source"
done

# Registered deformers are high-frequency Maya entry points. Compile every
# implementation against the real SDK so exception boundaries, data-handle
# access, and geometry-space calls cannot silently drift from the API.
"$compiler" \
  -std=c++14 \
  -Wall -Wextra -Wpedantic -Werror \
  -Wno-deprecated-declarations \
  -fsyntax-only \
  -isystem "$maya_location/include" \
  "${project_include_args[@]}" \
  -isystem "$repo_root/third_party/libnoise/source" \
  -idirafter "$repo_root/tests/stubs" \
  "$repo_root/src/modules/deformers/Deformer/surfaceDeformer.cpp" \
  "$repo_root/src/modules/deformers/Deformer/noiseDeformer.cpp" \
  "$repo_root/src/modules/deformers/Deformer/pushDeformer.cpp" \
  "$repo_root/src/modules/deformers/Deformer/DMotionDeformer.cpp" \
  "$repo_root/src/modules/deformers/Deformer/DRayDeformer.cpp" \
  "$repo_root/src/modules/deformers/Deformer/DCollisionDeformer.cpp" \
  "$repo_root/src/modules/mesh/DMesh/Deform/DMMotionDeform.cpp"

"$compiler" \
  -std=c++14 \
  -Wall -Wextra -Wpedantic -Werror \
  -Wno-deprecated-declarations \
  -fsyntax-only \
  -isystem "$maya_location/include" \
  "${project_include_args[@]}" \
  -idirafter "$repo_root/tests/stubs" \
  "$repo_root/src/core/Data/DDataUtility.cpp"

"$compiler" \
  -std=c++14 \
  -Wall -Wextra -Wpedantic -Werror \
  -Wno-deprecated-declarations \
  -fsyntax-only \
  -isystem "$maya_location/include" \
  "${project_include_args[@]}" \
  -isystem "$repo_root/third_party/libnoise/source" \
  -idirafter "$repo_root/tests/stubs" \
  "$repo_root/src/modules/generators/Generate/LeafGen/LeafGen.cpp"

# Keep the registered core mesh-analysis and matrix utility nodes, plus the
# default-off archival stretch node, warning-clean against a real Maya SDK.
"$compiler" \
  -std=c++14 \
  -Wall -Wextra -Wpedantic -Werror \
  -Wno-deprecated-declarations \
  -Wno-unknown-pragmas \
  -fsyntax-only \
  -isystem "$maya_location/include" \
  "${project_include_args[@]}" \
  -isystem "$repo_root/third_party/libnoise/source" \
  -idirafter "$repo_root/tests/stubs" \
  "$repo_root/src/core/UtilityNode/DCurvatureData/DCurvatureData.cpp" \
  "$repo_root/src/core/UtilityNode/DPolyFaceInfo/DPolyFaceInfo.cpp" \
  "$repo_root/src/core/UtilityNode/DStretchData/DStretchData.cpp" \
  "$repo_root/src/core/UtilityNode/DMatToVec/DMatToVec.cpp"

for analyzed_source in \
  "$repo_root/src/core/UtilityNode/DCurvatureData/DCurvatureData.cpp" \
  "$repo_root/src/core/UtilityNode/DPolyFaceInfo/DPolyFaceInfo.cpp" \
  "$repo_root/src/core/UtilityNode/DStretchData/DStretchData.cpp" \
  "$repo_root/src/core/UtilityNode/DMatToVec/DMatToVec.cpp"; do
  "$compiler" \
    --analyze \
    -std=c++14 \
    -w \
    -Xanalyzer -analyzer-output=text \
    -Xanalyzer -analyzer-werror \
    -o /dev/null \
    -isystem "$maya_location/include" \
    "${project_include_args[@]}" \
    -isystem "$repo_root/third_party/libnoise/source" \
    -idirafter "$repo_root/tests/stubs" \
    "$analyzed_source"
done

legacy_dependency_root="${DOMMY_LEGACY_DEPENDENCY_ROOT:-${TMPDIR:-/tmp}/dommytool-legacy-dependencies}"
bullet_root="$legacy_dependency_root/bullet3-126b6762200b1a9ec8df39b8fd53cb2ad62fb146"
seexpr_root="$legacy_dependency_root/SeExpr-a5f02bb03199630759b0b94a64f37ce56c08675a"
openexr_root="$legacy_dependency_root/openexr-dcaf5fdb4d1244d8e60a58832cfe9c54734a2257"
opensubdiv_root="$legacy_dependency_root/OpenSubdiv-3b5c3d99d29ba078385013d15f8b2ebeac0daf45"
nanoflann_root="$legacy_dependency_root/nanoflann-f2efec0110eb68edf8cae90fce394a5044bf3db8"
legacy_bullet_result="legacy Bullet solver source checks skipped (sources not fetched)"

# Compile the complete curve-data invariant and its two highest-risk producers
# and consumers whenever the pinned SeExpr headers used by their umbrella
# headers are available. This keeps the archive check useful with a modern Maya
# SDK without pretending to provide Maya 2019 runtime validation.
curve_invariant_result="curve invariant source checks skipped (SeExpr source not fetched)"
historical_geometry_result="additional historical geometry consumer checks skipped (SeExpr source not fetched; DPTimeShift and DNodeOperation checked above)"
viewport_result="shared Viewport 2.0 and DPFoamMesh checks skipped (SeExpr source not fetched)"
procedural_generator_result="bounded procedural generator checks skipped (SeExpr source not fetched)"
opensubdiv_result="OpenSubdiv source checks skipped (sources not fetched)"
spatial_search_result="attribute-transfer and spatial-search checks skipped (nanoflann or SeExpr not configured)"
baseline_data_nodes_result="baseline DPoint/DCurve/DMesh warning checks skipped (nanoflann or SeExpr not configured)"
text_generation_result="FreeType text-generation checks skipped (FreeType or SeExpr not configured)"
point_influence_result="registered point influence and SeExpr checks skipped (IlmBase or SeExpr source not fetched)"
bounded_legacy_nodes_result="bounded legacy DPoint/DCurve/DMesh node checks skipped (IlmBase or SeExpr source not fetched)"
if [[ -d "$seexpr_root/src/SeExpr" ]]; then
  if [[ -d "$openexr_root/IlmBase/Imath" ]]; then
    # These nodes are registered in the baseline plug-in and operate on the
    # historical Imath-backed point containers. Compile the implementations,
    # not only their registration declarations, against the pinned API.
    "$compiler" \
      -std=c++14 \
      -Wall -Wextra -Wpedantic -Werror \
      -Wno-deprecated-declarations \
      -Wno-unknown-pragmas \
      -fsyntax-only \
      -isystem "$repo_root/tests/compat/ilmbase-2.2" \
      -isystem "$openexr_root/IlmBase/Imath" \
      -isystem "$openexr_root/IlmBase/Iex" \
      -isystem "$openexr_root/IlmBase/Half" \
      -isystem "$maya_location/include" \
      -isystem "$seexpr_root/src/SeExpr" \
      "${project_include_args[@]}" \
      -isystem "$repo_root/third_party/libnoise/source" \
      -idirafter "$repo_root/tests/stubs" \
      "$repo_root/src/plugin/pluginMain.cpp" \
      "$repo_root/src/core/common/string/DStringOp.cpp" \
      "$repo_root/src/core/DDataUtility/Attribute/DPAttributeSeExpr.cpp" \
      "$repo_root/src/core/common/object/DNoiseOp.cpp" \
      "$repo_root/src/core/common/utility/DPRayUtility.cpp" \
      "$repo_root/src/modules/point/DPoint/Common/DPointSeExpr.cpp" \
      "$repo_root/src/modules/point/DPoint/math/DPMathUtility.cpp" \
      "$repo_root/src/modules/point/DPoint/math/DPMath.cpp" \
      "$repo_root/src/modules/point/DPoint/math/DPAdd.cpp" \
      "$repo_root/src/modules/point/DPoint/math/DPSubtract.cpp" \
      "$repo_root/src/modules/point/DPoint/math/DPMultiply.cpp" \
      "$repo_root/src/modules/point/DPoint/math/DPDivide.cpp" \
      "$repo_root/src/modules/point/DPoint/math/DPModulo.cpp" \
      "$repo_root/src/modules/point/DPoint/math/DPPow.cpp" \
      "$repo_root/src/modules/point/DPoint/math/DPMin.cpp" \
      "$repo_root/src/modules/point/DPoint/math/DPMax.cpp" \
      "$repo_root/src/modules/point/DPoint/math/DPMathFunc.cpp" \
      "$repo_root/src/modules/point/DPoint/math/DPNegative.cpp" \
      "$repo_root/src/modules/point/DPoint/math/DPSqrt.cpp" \
      "$repo_root/src/modules/point/DPoint/math/DPFloor.cpp" \
      "$repo_root/src/modules/point/DPoint/math/DPRound.cpp" \
      "$repo_root/src/modules/point/DPoint/math/DPCeil.cpp" \
      "$repo_root/src/modules/point/DPoint/math/DPExponential.cpp" \
      "$repo_root/src/modules/point/DPoint/math/DPAbsolute.cpp" \
      "$repo_root/src/modules/point/DPoint/Modify/DPEditDouble.cpp" \
      "$repo_root/src/core/DDataUtility/Generate/DPAddPoint.cpp" \
      "$repo_root/src/core/DDataUtility/Utility/DPFillSpace.cpp" \
      "$repo_root/src/modules/point/DPoint/Utility/DPAffect.cpp" \
      "$repo_root/src/modules/point/DPoint/Utility/DPAvoid.cpp" \
      "$repo_root/src/modules/point/DPoint/Utility/DPCopyTransform.cpp" \
      "$repo_root/src/modules/point/DPoint/Utility/DPMagnet.cpp" \
      "$repo_root/src/modules/point/DPoint/Utility/DPDistance.cpp" \
      "$repo_root/src/modules/point/DPoint/Utility/DPSpiralField.cpp" \
      "$repo_root/src/modules/point/DPoint/Utility/DPPyroNoise.cpp" \
      "$repo_root/src/modules/point/DPoint/Utility/DPDFNoise.cpp" \
      "$repo_root/src/modules/point/DPoint/Utility/DPColor.cpp" \
      "$repo_root/src/modules/point/DPoint/Utility/DPNoise.cpp" \
      "$repo_root/src/modules/point/DPoint/Utility/DPRay.cpp" \
      "$repo_root/src/modules/point/DPoint/Utility/DPBlend.cpp" \
      "$repo_root/src/modules/point/DPoint/Utility/DPPointToFloat.cpp" \
      "$repo_root/src/modules/point/DPoint/Utility/DPSort.cpp" \
      "$repo_root/src/modules/point/DPoint/Utility/DPSmooth.cpp" \
      "$repo_root/src/modules/point/DPoint/nurbs/DPUVGen.cpp" \
      "$repo_root/src/modules/curve/DCurve/Generate/DCurveFromMesh.cpp" \
      "$repo_root/src/modules/curve/DCurve/Utility/DCNormal.cpp" \
      "$repo_root/src/modules/curve/DCurve/Utility/DCClumping.cpp" \
      "$repo_root/src/modules/curve/DCurve/Utility/DCWeightMap.cpp" \
      "$repo_root/src/modules/curve/DCurve/Utility/DCCutObject.cpp" \
      "$repo_root/src/modules/curve/DCurve/Utility/DCReflect.cpp" \
      "$repo_root/src/modules/mesh/DMesh/Utility/DMCopyTransform.cpp" \
      "$repo_root/src/modules/mesh/mesh/applyVtxColorFromFileNode.cpp" \
      "$repo_root/src/core/DDataUtility/Particle/nobject/DPnParticle.cpp" \
      "$repo_root/src/core/DDataUtility/Particle/nobject/DPnPopSolver.cpp"

    # These baseline nodes were historically accepted through umbrella and
    # transitive includes. Compile each implementation as its own translation
    # unit while also enforcing the runtime generation budgets, staged
    # plug-in-data output, and serial historical-sampling contracts.
    "$compiler" \
      -std=c++14 \
      -Wall -Wextra -Wpedantic -Werror \
      -Wno-deprecated-declarations \
      -Wno-unknown-pragmas \
      -fsyntax-only \
      -isystem "$repo_root/tests/compat/ilmbase-2.2" \
      -isystem "$openexr_root/IlmBase/Imath" \
      -isystem "$openexr_root/IlmBase/Iex" \
      -isystem "$openexr_root/IlmBase/Half" \
      -isystem "$maya_location/include" \
      -isystem "$seexpr_root/src/SeExpr" \
      "${project_include_args[@]}" \
      -isystem "$repo_root/third_party/libnoise/source" \
      -idirafter "$repo_root/tests/stubs" \
      "$repo_root/src/core/DDataUtility/Attribute/Convert/DPAttributeToDataArray.cpp" \
      "$repo_root/src/modules/point/DPoint/animation/DPAnimMatrix.cpp" \
      "$repo_root/src/modules/curve/DCurve/DCConvertFromCurves.cpp" \
      "$repo_root/src/modules/curve/DCurve/Generate/DCurveLinkGen.cpp" \
      "$repo_root/src/modules/curve/DCurve/Utility/DCCharactor.cpp" \
      "$repo_root/src/modules/point/DPoint/animation/DPMatrix.cpp" \
      "$repo_root/src/modules/curve/DCurve/Utility/DCFillBetween.cpp" \
      "$repo_root/src/modules/curve/DCurve/Utility/DCReplicator.cpp" \
      "$repo_root/src/modules/curve/DCurve/Utility/DCResample.cpp" \
      "$repo_root/src/modules/curve/DCurve/Utility/DCReform.cpp" \
      "$repo_root/src/modules/point/DPoint/Utility/DPDuplicator.cpp" \
      "$repo_root/src/modules/mesh/DMesh/Generate/DMTimeShapeGen.cpp" \
      "$repo_root/src/modules/point/DPoint/animation/DPMotionCreate.cpp" \
      "$repo_root/src/modules/point/DPoint/animation/DPRotation.cpp"
    bounded_legacy_nodes_result="bounded legacy DPoint/DCurve/DMesh node warning checks"

    for analyzed_source in \
      "$repo_root/src/core/common/string/DStringOp.cpp" \
      "$repo_root/src/core/DDataUtility/Attribute/DPAttributeSeExpr.cpp" \
      "$repo_root/src/core/common/object/DNoiseOp.cpp" \
      "$repo_root/src/core/common/utility/DPRayUtility.cpp" \
      "$repo_root/src/modules/point/DPoint/Common/DPointSeExpr.cpp" \
      "$repo_root/src/modules/point/DPoint/math/DPMathUtility.cpp" \
      "$repo_root/src/modules/point/DPoint/Modify/DPEditDouble.cpp" \
      "$repo_root/src/core/DDataUtility/Generate/DPAddPoint.cpp" \
      "$repo_root/src/core/DDataUtility/Utility/DPFillSpace.cpp" \
      "$repo_root/src/modules/point/DPoint/Utility/DPAffect.cpp" \
      "$repo_root/src/modules/point/DPoint/Utility/DPAvoid.cpp" \
      "$repo_root/src/modules/point/DPoint/Utility/DPCopyTransform.cpp" \
      "$repo_root/src/modules/point/DPoint/Utility/DPMagnet.cpp" \
      "$repo_root/src/modules/point/DPoint/Utility/DPDistance.cpp" \
      "$repo_root/src/modules/point/DPoint/Utility/DPSpiralField.cpp" \
      "$repo_root/src/modules/point/DPoint/Utility/DPPyroNoise.cpp" \
      "$repo_root/src/modules/point/DPoint/Utility/DPDFNoise.cpp" \
      "$repo_root/src/modules/point/DPoint/Utility/DPColor.cpp" \
      "$repo_root/src/modules/point/DPoint/Utility/DPNoise.cpp" \
      "$repo_root/src/modules/point/DPoint/Utility/DPRay.cpp" \
      "$repo_root/src/modules/point/DPoint/Utility/DPBlend.cpp" \
      "$repo_root/src/modules/point/DPoint/Utility/DPPointToFloat.cpp" \
      "$repo_root/src/modules/point/DPoint/Utility/DPSort.cpp" \
      "$repo_root/src/modules/point/DPoint/Utility/DPSmooth.cpp" \
      "$repo_root/src/modules/point/DPoint/nurbs/DPUVGen.cpp" \
      "$repo_root/src/modules/curve/DCurve/Generate/DCurveFromMesh.cpp" \
      "$repo_root/src/modules/curve/DCurve/Utility/DCNormal.cpp" \
      "$repo_root/src/modules/curve/DCurve/Utility/DCClumping.cpp" \
      "$repo_root/src/modules/curve/DCurve/Utility/DCWeightMap.cpp" \
      "$repo_root/src/modules/curve/DCurve/Utility/DCCutObject.cpp" \
      "$repo_root/src/modules/curve/DCurve/Utility/DCReflect.cpp" \
      "$repo_root/src/modules/mesh/DMesh/Utility/DMCopyTransform.cpp" \
      "$repo_root/src/modules/mesh/mesh/applyVtxColorFromFileNode.cpp" \
      "$repo_root/src/core/DDataUtility/Particle/nobject/DPnParticle.cpp" \
      "$repo_root/src/core/DDataUtility/Particle/nobject/DPnPopSolver.cpp"; do
      "$compiler" \
        --analyze \
        -std=c++14 \
        -w \
        -Xanalyzer -analyzer-output=text \
        -Xanalyzer -analyzer-werror \
        -o /dev/null \
        -isystem "$repo_root/tests/compat/ilmbase-2.2" \
        -isystem "$openexr_root/IlmBase/Imath" \
        -isystem "$openexr_root/IlmBase/Iex" \
        -isystem "$openexr_root/IlmBase/Half" \
        -isystem "$maya_location/include" \
        -isystem "$seexpr_root/src/SeExpr" \
        "${project_include_args[@]}" \
        -isystem "$repo_root/third_party/libnoise/source" \
        -idirafter "$repo_root/tests/stubs" \
        "$analyzed_source"
    done
    point_influence_result="registered point influence, SeExpr, point utility, Nucleus, and curve projection warning/analyzer checks"
  fi

  "$compiler" \
    -std=c++14 \
    -Wall -Wextra -Wpedantic -Werror \
    -Wno-deprecated-declarations \
    -Wno-unknown-pragmas \
    -fsyntax-only \
    -isystem "$maya_location/include" \
    -isystem "$seexpr_root/src/SeExpr" \
    "${project_include_args[@]}" \
    -idirafter "$repo_root/tests/stubs" \
    "$repo_root/src/core/DDataUtility/Group/DPGroup.cpp" \
    "$repo_root/src/core/Data/DGroupCommon.cpp"

  "$compiler" \
    -std=c++14 \
    -Wall -Wextra -Wpedantic -Werror \
    -Wno-deprecated-declarations \
    -Wno-unknown-pragmas \
    -fsyntax-only \
    -isystem "$maya_location/include" \
    -isystem "$seexpr_root/src/SeExpr" \
    "${project_include_args[@]}" \
    -idirafter "$repo_root/tests/stubs" \
    "$repo_root/src/core/DDataUtility/Utility/DPTransform.cpp" \
    "$repo_root/src/modules/mesh/mesh/DPolyWire.cpp" \
    "$repo_root/src/modules/mesh/DMesh/Generate/DMWireGen.cpp" \
    "$repo_root/src/modules/curve/DCurve/Generate/DCurveClothFromSurface.cpp" \
    "$repo_root/src/modules/curve/legacy/DSpiralCurve.cpp" \
    "$repo_root/src/modules/point/DPoint/generate/data/DPStringArrayGen.cpp" \
    "$repo_root/src/modules/curve/Curve/Node/DArcCurveNode.cpp" \
    "$repo_root/src/modules/curve/Curve/Node/DRigController.cpp" \
    "$repo_root/src/modules/mesh/mesh/foamGenerateNode.cpp"
  procedural_generator_result="bounded string, cloth, spiral, polygon-wire, DMesh-wire, arc, rig-controller, and foam generator warning checks"

  for analyzed_source in \
    "$repo_root/src/core/DDataUtility/Utility/DPTransform.cpp" \
    "$repo_root/src/modules/curve/Curve/Node/DArcCurveNode.cpp" \
    "$repo_root/src/modules/curve/Curve/Node/DRigController.cpp" \
    "$repo_root/src/modules/mesh/mesh/foamGenerateNode.cpp"; do
    "$compiler" \
      --analyze \
      -std=c++14 \
      -w \
      -Xanalyzer -analyzer-output=text \
      -Xanalyzer -analyzer-werror \
      -o /dev/null \
      -isystem "$maya_location/include" \
      -isystem "$seexpr_root/src/SeExpr" \
      "${project_include_args[@]}" \
      -isystem "$repo_root/third_party/libnoise/source" \
      -idirafter "$repo_root/tests/stubs" \
      "$analyzed_source"
  done

  # These Viewport 2.0 implementations share custom DPoint/DMesh/DCurve data
  # and therefore transitively include the pinned SeExpr headers. Compile them
  # together so checked plug-in data access and GPU buffer contracts stay in
  # sync across every default display node.
  "$compiler" \
    -std=c++14 \
    -Wall -Wextra -Wpedantic -Werror \
    -Wno-deprecated-declarations \
    -Wno-unused-parameter \
    -Wno-unknown-pragmas \
    -fsyntax-only \
    -isystem "$maya_location/include" \
    -isystem "$seexpr_root/src/SeExpr" \
    "${project_include_args[@]}" \
    -isystem "$repo_root/third_party/libnoise/source" \
    -idirafter "$repo_root/tests/stubs" \
    "$repo_root/src/modules/point/DPoint/Display/DPointDisplay.cpp" \
    "$repo_root/src/modules/point/DPoint/Display/DPMarker.cpp" \
    "$repo_root/src/modules/point/DPoint/mesh/DPFoamMesh.cpp" \
    "$repo_root/src/modules/curve/DCurve/Display/DCurveDisplay.cpp" \
    "$repo_root/src/modules/curve/DCurve/Display/DCMarker.cpp" \
    "$repo_root/src/modules/mesh/DMesh/Display/DMeshDisplay.cpp" \
    "$repo_root/src/modules/mesh/DMesh/Display/DMMarker.cpp"
  viewport_result="shared Viewport 2.0, checked custom-data access, and DPFoamMesh warning/analyzer checks"

  "$compiler" \
    --analyze \
    -std=c++14 \
    -w \
    -Xanalyzer -analyzer-output=text \
    -Xanalyzer -analyzer-werror \
    -o /dev/null \
    -isystem "$maya_location/include" \
    -isystem "$seexpr_root/src/SeExpr" \
    "${project_include_args[@]}" \
    -isystem "$repo_root/third_party/libnoise/source" \
    -idirafter "$repo_root/tests/stubs" \
    "$repo_root/src/modules/point/DPoint/mesh/DPFoamMesh.cpp"

  "$compiler" \
    -std=c++14 \
    -Wall -Wextra -Wpedantic -Werror \
    -Wno-deprecated-declarations \
    -Wno-unused-parameter \
    -fsyntax-only \
    -isystem "$maya_location/include" \
    -isystem "$seexpr_root/src/SeExpr" \
    "${project_include_args[@]}" \
    -isystem "$repo_root/third_party/libnoise/source" \
    -idirafter "$repo_root/tests/stubs" \
    "$repo_root/src/core/Data/DCurveData.cpp" \
    "$repo_root/src/modules/mesh/DMesh/Data/DMeshData.cpp" \
    "$repo_root/src/modules/mesh/DMesh/common/DMeshUtil.cpp" \
    "$repo_root/src/modules/mesh/DMesh/Convert/DMToMesh.cpp" \
    "$repo_root/src/core/common/object/DDeformMath.cpp" \
    "$repo_root/src/modules/curve/DCurve/DCurveUtil.cpp" \
    "$repo_root/src/modules/curve/DCurve/Generate/DCurveFromDP.cpp" \
    "$repo_root/src/modules/curve/DCurve/Generate/DCAhoGen.cpp" \
    "$repo_root/src/modules/curve/DCurve/Generate/DCEyelashGen.cpp" \
    "$repo_root/src/modules/curve/DCurve/Shape/DCurveShape.cpp" \
    "$repo_root/src/modules/curve/DCurve/Utility/DCConnect.cpp" \
    "$repo_root/src/modules/curve/DCurve/Utility/DCDelete.cpp" \
    "$repo_root/src/modules/curve/DCurve/Convert/DCToCurves.cpp" \
    "$repo_root/src/modules/curve/Curve/Node/DMotionTrailCurveNode.cpp" \
    "$repo_root/src/modules/deformers/transform/DTDelayTransformMatrix.cpp" \
    "$repo_root/src/modules/generators/Generate/motionDuplicator.cpp" \
    "$repo_root/src/modules/generators/legacy/surfaceDuplicater.cpp" \
    "$repo_root/src/modules/generators/legacy/spaceDuplicater.cpp" \
    "$repo_root/src/modules/generators/Generate/Human/DHEyeGen.cpp" \
    "$repo_root/src/modules/generators/Generate/Human/DHEyelashGen.cpp" \
    "$repo_root/src/core/DDataUtility/Attribute/DPAttributeCreate.cpp" \
    "$repo_root/src/modules/point/DPoint/math/other/DPFitRange.cpp" \
    "$repo_root/src/modules/point/DPoint/math/other/DPRemap.cpp" \
    "$repo_root/src/modules/point/DPoint/animation/DPSignal.cpp" \
    "$repo_root/src/modules/point/DPoint/animation/DPSignal3D.cpp" \
    "$repo_root/src/core/DDataUtility/IO/DPointDataExportCmd.cpp"

  # Compile the shared historical-sampling helpers together with their main
  # point-node consumers and the legacy mesh snapshot boundary. These sources
  # previously relied on unchecked Maya API results and implicit array sizes.
  "$compiler" \
    -std=c++14 \
    -Wall -Wextra -Wpedantic -Werror \
    -Wno-deprecated-declarations \
    -Wno-unused-parameter \
    -Wno-unknown-pragmas \
    -fsyntax-only \
    -isystem "$maya_location/include" \
    -isystem "$seexpr_root/src/SeExpr" \
    "${project_include_args[@]}" \
    -isystem "$repo_root/third_party/libnoise/source" \
    -idirafter "$repo_root/tests/stubs" \
    "$repo_root/src/core/common/object/DNodeOperation.cpp" \
    "$repo_root/src/core/common/object/DPolygonUtil.cpp" \
    "$repo_root/src/modules/point/DPoint/Common/DPAttributeCommon.cpp" \
    "$repo_root/src/modules/point/DPoint/Utility/DPTimeShift.cpp" \
    "$repo_root/src/modules/point/DPoint/animation/DPMotionCreate.cpp" \
    "$repo_root/src/modules/point/DPoint/animation/DPRotation.cpp" \
    "$repo_root/src/modules/mesh/legacy/dupMesh.cpp" \
    "$repo_root/tests/cpp/dup_mesh_contract_compile_test.cpp" \
    "$repo_root/src/modules/point/DPoint/mesh/DPMeshDuplicator.cpp"

  for analyzed_source in \
    "$repo_root/src/core/common/object/DNodeOperation.cpp" \
    "$repo_root/src/core/common/object/DPolygonUtil.cpp" \
    "$repo_root/src/modules/point/DPoint/Common/DPAttributeCommon.cpp" \
    "$repo_root/src/modules/point/DPoint/Utility/DPTimeShift.cpp" \
    "$repo_root/src/modules/mesh/legacy/dupMesh.cpp" \
    "$repo_root/src/modules/point/DPoint/mesh/DPMeshDuplicator.cpp"; do
    "$compiler" \
      --analyze \
      -std=c++14 \
      -w \
      -Xanalyzer -analyzer-output=text \
      -Xanalyzer -analyzer-werror \
      -o /dev/null \
      -isystem "$maya_location/include" \
      -isystem "$seexpr_root/src/SeExpr" \
      "${project_include_args[@]}" \
      -isystem "$repo_root/third_party/libnoise/source" \
      -idirafter "$repo_root/tests/stubs" \
      "$analyzed_source"
  done
  historical_geometry_result="historical geometry snapshot, bounded mesh duplication, array-contract, time-sampling, warning, and analyzer checks"

  for analyzed_source in \
    "$repo_root/src/core/Data/DCurveData.cpp" \
    "$repo_root/src/modules/mesh/DMesh/Data/DMeshData.cpp" \
    "$repo_root/src/modules/mesh/DMesh/common/DMeshUtil.cpp" \
    "$repo_root/src/modules/mesh/DMesh/Utility/DMCopyUV.cpp" \
    "$repo_root/src/modules/mesh/DMesh/Convert/DMToMesh.cpp" \
    "$repo_root/src/core/common/object/DDeformMath.cpp" \
    "$repo_root/src/modules/curve/DCurve/Generate/DCurveFromDP.cpp" \
    "$repo_root/src/modules/curve/DCurve/Generate/DCAhoGen.cpp" \
    "$repo_root/src/modules/curve/DCurve/Generate/DCEyelashGen.cpp" \
    "$repo_root/src/modules/curve/DCurve/Shape/DCurveShape.cpp" \
    "$repo_root/src/modules/curve/DCurve/Utility/DCConnect.cpp" \
    "$repo_root/src/modules/curve/DCurve/Utility/DCDelete.cpp" \
    "$repo_root/src/modules/curve/DCurve/Convert/DCToCurves.cpp" \
    "$repo_root/src/modules/curve/Curve/Node/DMotionTrailCurveNode.cpp" \
    "$repo_root/src/modules/deformers/transform/DTDelayTransformMatrix.cpp" \
    "$repo_root/src/modules/generators/Generate/motionDuplicator.cpp" \
    "$repo_root/src/modules/generators/legacy/surfaceDuplicater.cpp" \
    "$repo_root/src/modules/generators/legacy/spaceDuplicater.cpp" \
    "$repo_root/src/modules/generators/Generate/Human/DHEyeGen.cpp" \
    "$repo_root/src/modules/generators/Generate/Human/DHEyelashGen.cpp" \
    "$repo_root/src/core/DDataUtility/Attribute/DPAttributeCreate.cpp" \
    "$repo_root/src/modules/point/DPoint/math/other/DPFitRange.cpp" \
    "$repo_root/src/modules/point/DPoint/math/other/DPRemap.cpp" \
    "$repo_root/src/modules/point/DPoint/animation/DPSignal.cpp" \
    "$repo_root/src/modules/point/DPoint/animation/DPSignal3D.cpp" \
    "$repo_root/src/core/DDataUtility/IO/DPointDataExportCmd.cpp"; do
    "$compiler" \
      --analyze \
      -std=c++14 \
      -w \
      -Xanalyzer -analyzer-output=text \
      -Xanalyzer -analyzer-werror \
      -o /dev/null \
      -isystem "$maya_location/include" \
      -isystem "$seexpr_root/src/SeExpr" \
      "${project_include_args[@]}" \
      -isystem "$repo_root/third_party/libnoise/source" \
      -idirafter "$repo_root/tests/stubs" \
      "$analyzed_source"
  done
  curve_invariant_result="curve and mesh invariant, bounded procedural generation, stateless remapping, finite signal math, historical sampling, Maya mesh conversion, and atomic DPoint export source checks"
fi

nanoflann_location="${NANOFLANN_LOCATION:-$nanoflann_root}"
nanoflann_include=""
if [[ -f "$nanoflann_location/include/nanoflann.hpp" ]]; then
  nanoflann_include="$nanoflann_location/include"
elif [[ -f "$nanoflann_location/nanoflann.hpp" ]]; then
  nanoflann_include="$nanoflann_location"
fi

if [[ -d "$seexpr_root/src/SeExpr" &&
      -d "$openexr_root/IlmBase/Imath" &&
      -n "$nanoflann_include" ]]; then
  "$compiler" \
    -std=c++14 \
    -Wall -Wextra -Wpedantic -Werror \
    -Wno-deprecated-declarations \
    -Wno-unused-parameter \
    -Wno-unknown-pragmas \
    -fsyntax-only \
    -isystem "$repo_root/tests/compat/ilmbase-2.2" \
    -isystem "$openexr_root/IlmBase/Imath" \
    -isystem "$openexr_root/IlmBase/Iex" \
    -isystem "$openexr_root/IlmBase/Half" \
    -isystem "$maya_location/include" \
    -isystem "$seexpr_root/src/SeExpr" \
    -isystem "$nanoflann_include" \
    "${project_include_args[@]}" \
    -isystem "$repo_root/third_party/libnoise/source" \
    -idirafter "$repo_root/tests/stubs" \
    "$repo_root/src/core/DDataUtility/Attribute/DPAttributeTransfer.cpp" \
    "$repo_root/src/modules/curve/DCurve/Attribute/DCAttributeTransfer.cpp" \
    "$repo_root/src/modules/mesh/DMesh/Attribute/DMAttributeTransfer.cpp" \
    "$repo_root/src/modules/mesh/DMesh/Utility/DMConnetedPieces.cpp" \
    "$repo_root/src/modules/mesh/DMesh/Utility/DMConnectTargets.cpp" \
    "$repo_root/src/modules/mesh/DMesh/Utility/DMEdgeDisplaceDir.cpp" \
    "$repo_root/src/modules/mesh/DMesh/Group/DMGroupFromDP.cpp" \
    "$repo_root/src/modules/curve/DCurve/Group/DCGroupFromDP.cpp" \
    "$repo_root/src/modules/point/DPoint/Utility/DPSearchPoint.cpp" \
    "$repo_root/src/modules/point/DPoint/Deform/DPConvertWeights.cpp" \
    "$repo_root/src/modules/point/DPoint/Deform/DPPointDeform.cpp" \
    "$repo_root/src/modules/point/DPoint/Deform/DPVectorDeform.cpp" \
    "$repo_root/src/modules/curve/DCurve/deform/DCVectorDeform.cpp" \
    "$repo_root/src/modules/mesh/DMesh/Deform/DMVectorDeform.cpp"
  for analyzed_source in \
    "$repo_root/src/core/DDataUtility/Attribute/DPAttributeTransfer.cpp" \
    "$repo_root/src/modules/curve/DCurve/Attribute/DCAttributeTransfer.cpp" \
    "$repo_root/src/modules/mesh/DMesh/Attribute/DMAttributeTransfer.cpp" \
    "$repo_root/src/modules/mesh/DMesh/Utility/DMConnetedPieces.cpp" \
    "$repo_root/src/modules/mesh/DMesh/Utility/DMConnectTargets.cpp" \
    "$repo_root/src/modules/mesh/DMesh/Utility/DMEdgeDisplaceDir.cpp" \
    "$repo_root/src/modules/mesh/DMesh/Group/DMGroupFromDP.cpp" \
    "$repo_root/src/modules/curve/DCurve/Group/DCGroupFromDP.cpp" \
    "$repo_root/src/modules/point/DPoint/Utility/DPSearchPoint.cpp" \
    "$repo_root/src/modules/point/DPoint/Deform/DPConvertWeights.cpp" \
    "$repo_root/src/modules/point/DPoint/Deform/DPPointDeform.cpp" \
    "$repo_root/src/modules/point/DPoint/Deform/DPVectorDeform.cpp" \
    "$repo_root/src/modules/curve/DCurve/deform/DCVectorDeform.cpp" \
    "$repo_root/src/modules/mesh/DMesh/Deform/DMVectorDeform.cpp"; do
    "$compiler" \
      --analyze \
      -std=c++14 \
      -w \
      -Xanalyzer -analyzer-output=text \
      -Xanalyzer -analyzer-werror \
      -o /dev/null \
      -isystem "$repo_root/tests/compat/ilmbase-2.2" \
      -isystem "$openexr_root/IlmBase/Imath" \
      -isystem "$openexr_root/IlmBase/Iex" \
      -isystem "$openexr_root/IlmBase/Half" \
      -isystem "$maya_location/include" \
      -isystem "$seexpr_root/src/SeExpr" \
      -isystem "$nanoflann_include" \
      "${project_include_args[@]}" \
      -isystem "$repo_root/third_party/libnoise/source" \
      -idirafter "$repo_root/tests/stubs" \
      "$analyzed_source"
  done

  # Compile the remaining baseline custom-data nodes that do not depend on
  # OpenVDB, Arnold, or FastNoise. This closes the warning-check gap
  # around DPoint/DCurve/DMesh conversion and utility code used by the default
  # plug-in registration set.
  "$compiler" \
    -std=c++14 \
    -Wall -Wextra -Wpedantic -Werror \
    -Wno-deprecated-declarations \
    -Wno-unknown-pragmas \
    -fsyntax-only \
    -isystem "$repo_root/tests/compat/ilmbase-2.2" \
    -isystem "$openexr_root/IlmBase/Imath" \
    -isystem "$openexr_root/IlmBase/Iex" \
    -isystem "$openexr_root/IlmBase/Half" \
    -isystem "$maya_location/include" \
    -isystem "$seexpr_root/src/SeExpr" \
    -isystem "$nanoflann_include" \
    "${project_include_args[@]}" \
    -isystem "$repo_root/third_party/libnoise/source" \
    -isystem "$repo_root/third_party/voro++" \
    -idirafter "$repo_root/tests/stubs" \
    "$repo_root/src/core/DDataUtility/Attribute/DPAttributeBlend.cpp" \
    "$repo_root/src/core/DDataUtility/Attribute/DPAttributeDelete.cpp" \
    "$repo_root/src/core/DDataUtility/Attribute/Edit/DPAttributeCast.cpp" \
    "$repo_root/src/plugin/DReservedTypeIds.cpp" \
    "$repo_root/src/core/DDataUtility/Data/DPAttributeData.cpp" \
    "$repo_root/src/core/DDataUtility/Data/DPData.cpp" \
    "$repo_root/src/core/DDataUtility/DataConvert/DPArrayToCompound.cpp" \
    "$repo_root/src/core/DDataUtility/Generate/DPointPlaneEmit.cpp" \
    "$repo_root/src/core/DDataUtility/Group/DPGroupDelete.cpp" \
    "$repo_root/src/core/DDataUtility/IO/ReadDPointDataNode.cpp" \
    "$repo_root/src/core/DDataUtility/Particle/utility/DPParticleAttribute.cpp" \
    "$repo_root/src/core/DDataUtility/Particle/nobject/DPnPopCommon.cpp" \
    "$repo_root/src/core/DDataUtility/Particle/nobject/DPnPopBase.cpp" \
    "$repo_root/src/core/Data/DPrimBaseData.cpp" \
    "$repo_root/src/core/common/DDrawHelper/DDrawHelper.cpp" \
    "$repo_root/src/core/common/color/DColorOperation.cpp" \
    "$repo_root/src/core/common/data/DDataOp.cpp" \
    "$repo_root/src/core/common/object/DBBoxOp.cpp" \
    "$repo_root/src/core/common/object/DLine3DOp.cpp" \
    "$repo_root/src/core/common/object/DOsdCommon.cpp" \
    "$repo_root/src/core/common/object/DPlaneOp.cpp" \
    "$repo_root/src/core/common/object/DPointOp.cpp" \
    "$repo_root/src/core/common/object/DTransformOp.cpp" \
    "$repo_root/src/core/common/object/DVoroUtil.cpp" \
    "$repo_root/src/core/common/utility/DMetaDataUtility.cpp" \
    "$repo_root/src/core/math/dommyMath.cpp" \
    "$repo_root/src/core/noise/PerlinNoise.cpp" \
    "$repo_root/src/modules/curve/DCurve/Attribute/DCAttributeCopy.cpp" \
    "$repo_root/src/modules/curve/DCurve/Generate/DCFurGen.cpp" \
    "$repo_root/src/modules/curve/DCurve/Generate/DCurveCircleGen.cpp" \
    "$repo_root/src/modules/curve/DCurve/Generate/DCurveFromCurve.cpp" \
    "$repo_root/src/modules/curve/DCurve/Generate/DCurveFromPfx.cpp" \
    "$repo_root/src/modules/curve/DCurve/Generate/DCurveFromSurface.cpp" \
    "$repo_root/src/modules/curve/DCurve/Group/DCGroupDelete.cpp" \
    "$repo_root/src/modules/curve/DCurve/Utility/DCBend.cpp" \
    "$repo_root/src/modules/curve/DCurve/Utility/DCColor.cpp" \
    "$repo_root/src/modules/curve/DCurve/Utility/DCConvex2D.cpp" \
    "$repo_root/src/modules/curve/DCurve/Utility/DCCopy.cpp" \
    "$repo_root/src/modules/curve/DCurve/Utility/DCCurl.cpp" \
    "$repo_root/src/modules/curve/DCurve/Utility/DCDivide.cpp" \
    "$repo_root/src/modules/curve/DCurve/Utility/DCExplodeView.cpp" \
    "$repo_root/src/modules/curve/DCurve/Utility/DCExtend.cpp" \
    "$repo_root/src/modules/curve/DCurve/Utility/DCFitCopy.cpp" \
    "$repo_root/src/modules/curve/DCurve/Utility/DCMeshCopy.cpp" \
    "$repo_root/src/modules/curve/DCurve/Utility/DCMotionPath.cpp" \
    "$repo_root/src/modules/curve/DCurve/Utility/DCNoise.cpp" \
    "$repo_root/src/modules/curve/DCurve/Utility/DCOffset.cpp" \
    "$repo_root/src/modules/curve/DCurve/Utility/DCPartition.cpp" \
    "$repo_root/src/modules/curve/DCurve/Utility/DCReplace.cpp" \
    "$repo_root/src/modules/curve/DCurve/Utility/DCStitch.cpp" \
    "$repo_root/src/modules/curve/DCurve/Utility/DCUVMap.cpp" \
    "$repo_root/src/modules/curve/legacy/woolCurve.cpp" \
    "$repo_root/src/modules/mesh/mesh/DPolyEdgeToMat.cpp" \
    "$repo_root/src/modules/mesh/mesh/DPolyFaceToMat.cpp" \
    "$repo_root/src/modules/mesh/mesh/DPolyResetVtx.cpp" \
    "$repo_root/src/modules/mesh/DMesh/Attribute/DMAttributeCopy.cpp" \
    "$repo_root/src/modules/mesh/DMesh/Convert/DMConvertFromMeshes.cpp" \
    "$repo_root/src/modules/mesh/DMesh/Data/DMGroupDelete.cpp" \
    "$repo_root/src/modules/mesh/DMesh/Dynamics/Bullet/DCRest.cpp" \
    "$repo_root/src/modules/mesh/DMesh/Dynamics/Utility/DMRest.cpp" \
    "$repo_root/src/modules/mesh/DMesh/Group/DMGroupToIndex.cpp" \
    "$repo_root/src/modules/mesh/DMesh/Utility/DMCharactor.cpp" \
    "$repo_root/src/modules/mesh/DMesh/Utility/DMBoolean.cpp" \
    "$repo_root/src/modules/mesh/DMesh/Utility/DMColor.cpp" \
    "$repo_root/src/modules/mesh/DMesh/Utility/DMCopyUV.cpp" \
    "$repo_root/src/modules/mesh/DMesh/Utility/DMDelete.cpp" \
    "$repo_root/src/modules/mesh/DMesh/Utility/DMDebrisGen.cpp" \
    "$repo_root/src/modules/mesh/DMesh/Utility/DMEdgeDisplace.cpp" \
    "$repo_root/src/modules/mesh/DMesh/Utility/DMExplodeView.cpp" \
    "$repo_root/src/modules/mesh/DMesh/Utility/DMExtrude.cpp" \
    "$repo_root/src/modules/mesh/DMesh/Utility/DMFaceInOut.cpp" \
    "$repo_root/src/modules/mesh/DMesh/Utility/DMInfo.cpp" \
    "$repo_root/src/modules/mesh/DMesh/Utility/DMMakePiece.cpp" \
    "$repo_root/src/modules/mesh/DMesh/Utility/DMMerge.cpp" \
    "$repo_root/src/modules/mesh/DMesh/Utility/DMNoise.cpp" \
    "$repo_root/src/modules/mesh/DMesh/Utility/DMNormal.cpp" \
    "$repo_root/src/modules/mesh/DMesh/Utility/DMRemesh.cpp" \
    "$repo_root/src/modules/mesh/DMesh/Utility/DMSlicer.cpp" \
    "$repo_root/src/modules/mesh/DMesh/Utility/DMTransform.cpp" \
    "$repo_root/src/modules/point/DPoint/Utility/DPArraySize.cpp" \
    "$repo_root/src/modules/point/DPoint/Utility/DPDelete.cpp" \
    "$repo_root/src/modules/point/DPoint/Utility/DPGoal.cpp" \
    "$repo_root/src/modules/point/DPoint/Utility/DPAdjustSpace.cpp" \
    "$repo_root/src/modules/point/DPoint/Utility/DPNull.cpp" \
    "$repo_root/src/modules/point/DPoint/Utility/DPVecCompare.cpp" \
    "$repo_root/src/modules/point/DPoint/math/vector/DPCrossProduct.cpp" \
    "$repo_root/src/modules/point/DPoint/math/vector/DPDotProduct.cpp" \
    "$repo_root/src/modules/point/DPoint/math/vector/DPNormalize.cpp" \
    "$repo_root/src/modules/point/DPoint/math/vector/DPVecAngle.cpp" \
    "$repo_root/src/modules/point/DPoint/math/vector/DPVecAverageComponent.cpp" \
    "$repo_root/src/modules/point/DPoint/math/vector/DPVecLength.cpp" \
    "$repo_root/src/modules/point/DPoint/math/vector/DPVecMaxComponent.cpp" \
    "$repo_root/src/modules/point/DPoint/math/vector/DPVecMinComponent.cpp" \
    "$repo_root/src/modules/point/DPoint/mesh/DPToMesh.cpp" \
    "$repo_root/src/modules/point/DPoint/mesh/DPVoronoi.cpp" \
    "$repo_root/src/modules/rendering/global/render/DGlobalTex.cpp"
  baseline_data_nodes_result="baseline DPoint/DCurve/DMesh warning checks"

  spatial_search_result="bounded attribute-transfer, connection, grouping, deformation, and KD-tree search warning/analyzer checks"
fi

freetype_location="${FREETYPE_LOCATION:-${DOMMY_DEPENDENCY_PREFIX:-}}"
freetype_include=""
if [[ -f "$freetype_location/include/freetype2/ft2build.h" ]]; then
  freetype_include="$freetype_location/include/freetype2"
elif [[ -f "$freetype_location/include/ft2build.h" ]]; then
  freetype_include="$freetype_location/include"
fi

if [[ -d "$seexpr_root/src/SeExpr" && -n "$freetype_include" ]]; then
  "$compiler" \
    -std=c++14 \
    -Wall -Wextra -Wpedantic -Werror \
    -Wno-deprecated-declarations \
    -Wno-unused-parameter \
    -Wno-unknown-pragmas \
    -fsyntax-only \
    -isystem "$maya_location/include" \
    -isystem "$seexpr_root/src/SeExpr" \
    -isystem "$freetype_include" \
    "${project_include_args[@]}" \
    -isystem "$repo_root/third_party/libnoise/source" \
    -idirafter "$repo_root/tests/stubs" \
    "$repo_root/src/modules/curve/DCurve/Generate/DCText.cpp"
  "$compiler" \
    --analyze \
    -std=c++14 \
    -w \
    -Xanalyzer -analyzer-output=text \
    -Xanalyzer -analyzer-werror \
    -o /dev/null \
    -isystem "$maya_location/include" \
    -isystem "$seexpr_root/src/SeExpr" \
    -isystem "$freetype_include" \
    "${project_include_args[@]}" \
    -isystem "$repo_root/third_party/libnoise/source" \
    -idirafter "$repo_root/tests/stubs" \
    "$repo_root/src/modules/curve/DCurve/Generate/DCText.cpp"
  text_generation_result="stateless, bounded FreeType text-generation warning checks"
fi

if [[ -d "$seexpr_root/src/SeExpr" &&
      -d "$opensubdiv_root/opensubdiv/far" ]]; then
  "$compiler" \
    -std=c++14 \
    -Wall -Wextra -Wpedantic -Werror \
    -Wno-deprecated-declarations \
    -Wno-unknown-pragmas \
    -fsyntax-only \
    -isystem "$maya_location/include" \
    -isystem "$seexpr_root/src/SeExpr" \
    -isystem "$opensubdiv_root" \
    "${project_include_args[@]}" \
    -idirafter "$repo_root/tests/stubs" \
    "$repo_root/src/modules/mesh/DMesh/Utility/DMSubDivide.cpp"
  opensubdiv_result="OpenSubdiv topology-budget and output-invariant warning checks"
fi

if [[ -d "$bullet_root/src" && -d "$seexpr_root/src/SeExpr" ]]; then
  # Maya 2025 headers require C++14 syntax. This is a syntax-only check; the
  # production Maya 2019 target remains configured as C++11 in CMake.
  "$compiler" \
    -std=c++14 \
    -w \
    -fsyntax-only \
    -isystem "$maya_location/include" \
    -isystem "$bullet_root/src" \
    -isystem "$bullet_root/Extras" \
    -isystem "$bullet_root/src/BulletCollision/NarrowPhaseCollision" \
    -isystem "$seexpr_root/src/SeExpr" \
    "${project_include_args[@]}" \
    -isystem "$repo_root/third_party/libnoise/source" \
    -idirafter "$repo_root/tests/stubs" \
    "$repo_root/src/modules/dynamics/DBullet/dbSolver_t.cpp" \
    "$repo_root/src/modules/dynamics/DBullet/Solver/DBSolverNode.cpp" \
    "$repo_root/src/modules/dynamics/DBullet/RigidBody/DBRigidBodyNode.cpp" \
    "$repo_root/src/modules/dynamics/DBullet/RigidBody/DBRigidBodyArrayNode.cpp" \
    "$repo_root/src/modules/dynamics/DBullet/Collision/DBCollisionShapeNode.cpp" \
    "$repo_root/src/modules/dynamics/DBullet/Constraint/DBNailConstraintNode.cpp" \
    "$repo_root/src/modules/dynamics/DBullet/Constraint/DBHingeConstraintNode.cpp" \
    "$repo_root/src/modules/dynamics/DBullet/Constraint/DBSliderConstraintNode.cpp" \
    "$repo_root/src/modules/dynamics/DBullet/Constraint/DBSixdofConstraintNode.cpp" \
    "$repo_root/src/modules/dynamics/DBullet/DM/DCBullet2DSolver.cpp" \
    "$repo_root/src/modules/dynamics/DBullet/DM/DMBulletTest.cpp" \
    "$repo_root/src/modules/dynamics/DBullet/SoftBody/DBSoftBodyNode.cpp" \
    "$repo_root/src/modules/dynamics/DBullet/Collision/convex_decomposition_hacd.cpp" \
    "$repo_root/src/modules/dynamics/DBullet/Draw/drawUtils.cpp" \
    "$repo_root/src/modules/dynamics/DBullet/File/pdbIO.cpp" \
    "$repo_root/src/modules/dynamics/DBullet/RigidBody/DB_RigidBody_t.cpp" \
    "$repo_root/src/modules/dynamics/DBullet/SoftBody/dbSoftBody_t.cpp" \
    "$repo_root/src/modules/dynamics/DBullet/SoftBody/soft_body_impl_t.cpp" \
    "$repo_root/src/modules/mesh/DMesh/Dynamics/Bullet/Constraint/BT6DofConstraint.cpp" \
    "$repo_root/src/modules/mesh/DMesh/Dynamics/Bullet/Constraint/BTConeTwistConstraint.cpp" \
    "$repo_root/src/modules/mesh/DMesh/Dynamics/Bullet/Constraint/BTFixConstraint.cpp" \
    "$repo_root/src/modules/mesh/DMesh/Dynamics/Bullet/Constraint/BTP2PConstraint.cpp" \
    "$repo_root/src/modules/mesh/DMesh/Dynamics/Bullet/DBFractureDynamicsWorld.cpp" \
    "$repo_root/src/modules/mesh/DMesh/Dynamics/Bullet/btConvexConvexMprAlgorithm.cpp" \
    "$repo_root/src/modules/mesh/DMesh/Dynamics/Bullet/commonBulletRigidBody.cpp" \
    "$repo_root/src/modules/mesh/DMesh/Dynamics/Bullet/dbtFractureBody.cpp" \
    "$repo_root/src/modules/mesh/DMesh/Dynamics/Bullet/DBulletFractureSolver.cpp" \
    "$repo_root/src/modules/mesh/DMesh/Dynamics/nObject/DMnRigidGen.cpp"

  # Compile the dependency-graph writer branches separately. They are
  # intentionally disabled in the baseline profile, so the normal pass above
  # would otherwise preprocess the PDB output implementation away.
  "$compiler" \
    -std=c++14 \
    -w \
    -fsyntax-only \
    -DDOMMY_WITH_DG_FILE_WRITERS=1 \
    -isystem "$maya_location/include" \
    -isystem "$bullet_root/src" \
    -isystem "$seexpr_root/src/SeExpr" \
    "${project_include_args[@]}" \
    -isystem "$repo_root/third_party/libnoise/source" \
    -idirafter "$repo_root/tests/stubs" \
    "$repo_root/src/modules/dynamics/DBullet/Solver/DBSolverNode.cpp" \
    "$repo_root/src/modules/dynamics/DBullet/RigidBody/DBRigidBodyArrayNode.cpp"

  # Keep the ownership/API boundary and every registered Bullet companion
  # implementation warning-clean. The broad historical source check above
  # still tolerates warnings from unregistered experimental nodes.
  "$compiler" \
    -std=c++14 \
    -Wall -Wextra -Wpedantic -Werror \
    -Wno-deprecated-declarations \
    -Wno-unused-parameter \
    -fsyntax-only \
    -isystem "$maya_location/include" \
    -isystem "$bullet_root/src" \
    -isystem "$seexpr_root/src/SeExpr" \
    "${project_include_args[@]}" \
    -isystem "$repo_root/third_party/libnoise/source" \
    -idirafter "$repo_root/tests/stubs" \
    "$repo_root/src/modules/dynamics/DBullet/DB_BulletSolver_t.cpp" \
    "$repo_root/src/modules/dynamics/DBullet/dbSolver_t.cpp" \
    "$repo_root/src/modules/dynamics/DBullet/Solver/DBSolverNode.cpp" \
    "$repo_root/src/modules/dynamics/DBullet/RigidBody/DBRigidBodyNode.cpp" \
    "$repo_root/src/modules/dynamics/DBullet/RigidBody/DBRigidBodyArrayNode.cpp" \
    "$repo_root/src/modules/dynamics/DBullet/Collision/DBCollisionShapeNode.cpp" \
    "$repo_root/src/modules/dynamics/DBullet/SoftBody/DBSoftBodyNode.cpp" \
    "$repo_root/src/modules/dynamics/DBullet/Constraint/DBNailConstraintNode.cpp" \
    "$repo_root/src/modules/dynamics/DBullet/Constraint/DBHingeConstraintNode.cpp" \
    "$repo_root/src/modules/dynamics/DBullet/Constraint/DBSliderConstraintNode.cpp" \
    "$repo_root/src/modules/dynamics/DBullet/Constraint/DBSixdofConstraintNode.cpp" \
    "$repo_root/src/modules/dynamics/DBullet/SoftBody/DB_SoftBody_t.cpp" \
    "$repo_root/src/modules/mesh/DMesh/Dynamics/Bullet/commonDBullet.cpp" \
    "$repo_root/src/modules/mesh/DMesh/Dynamics/Bullet/DMbtRigid.cpp" \
    "$repo_root/src/modules/mesh/DMesh/Dynamics/Bullet/DCbtRigid2D.cpp" \
    "$repo_root/src/modules/mesh/DMesh/Dynamics/Bullet/DCbtRope.cpp" \
    "$repo_root/src/modules/mesh/DMesh/Dynamics/Bullet/DMbtFracture.cpp" \
    "$repo_root/src/modules/mesh/DMesh/Dynamics/Bullet/DMbtSoft.cpp" \
    "$repo_root/src/modules/mesh/DMesh/Dynamics/Bullet/Constraint/DBPointConstraint.cpp" \
    "$repo_root/src/modules/mesh/DMesh/Dynamics/Bullet/Constraint/DBLinkConstraint.cpp" \
    "$repo_root/src/modules/dynamics/DBullet/DM/DMBulletTest.cpp" \
    "$repo_root/src/modules/dynamics/DBullet/DM/DCBullet2DSolver.cpp"

  for analyzed_source in \
    "$repo_root/src/modules/dynamics/DBullet/DB_BulletSolver_t.cpp" \
    "$repo_root/src/modules/dynamics/DBullet/dbSolver_t.cpp" \
    "$repo_root/src/modules/dynamics/DBullet/Solver/DBSolverNode.cpp" \
    "$repo_root/src/modules/dynamics/DBullet/RigidBody/DBRigidBodyNode.cpp" \
    "$repo_root/src/modules/dynamics/DBullet/RigidBody/DBRigidBodyArrayNode.cpp" \
    "$repo_root/src/modules/dynamics/DBullet/Collision/DBCollisionShapeNode.cpp" \
    "$repo_root/src/modules/dynamics/DBullet/SoftBody/DBSoftBodyNode.cpp" \
    "$repo_root/src/modules/dynamics/DBullet/Constraint/DBNailConstraintNode.cpp" \
    "$repo_root/src/modules/dynamics/DBullet/Constraint/DBHingeConstraintNode.cpp" \
    "$repo_root/src/modules/dynamics/DBullet/Constraint/DBSliderConstraintNode.cpp" \
    "$repo_root/src/modules/dynamics/DBullet/Constraint/DBSixdofConstraintNode.cpp" \
    "$repo_root/src/modules/mesh/DMesh/Dynamics/Bullet/DMbtFracture.cpp" \
    "$repo_root/src/modules/mesh/DMesh/Dynamics/Bullet/Constraint/DBPointConstraint.cpp" \
    "$repo_root/src/modules/mesh/DMesh/Dynamics/Bullet/Constraint/DBLinkConstraint.cpp"; do
    "$compiler" \
      --analyze \
      -std=c++14 \
      -w \
      -Xanalyzer -analyzer-output=text \
      -Xanalyzer -analyzer-werror \
      -o /dev/null \
      -isystem "$maya_location/include" \
      -isystem "$bullet_root/src" \
      -isystem "$seexpr_root/src/SeExpr" \
      "${project_include_args[@]}" \
      -isystem "$repo_root/third_party/libnoise/source" \
      -idirafter "$repo_root/tests/stubs" \
      "$analyzed_source"
  done

  "$compiler" \
    -std=c++14 \
    -w \
    -fsyntax-only \
    -isystem "$maya_location/include" \
    -isystem "$bullet_root/src" \
    -isystem "$seexpr_root/src/SeExpr" \
    "${project_include_args[@]}" \
    -isystem "$repo_root/third_party/libnoise/source" \
    -idirafter "$repo_root/tests/stubs" \
    "$repo_root/src/modules/rendering/Render/Camera/DCameraFrustum.cpp" \
    "$repo_root/src/modules/particles/particle/field/DCurlNoiseField.cpp"
  legacy_bullet_result="legacy Bullet solver source, focused warning, and analyzer checks"
fi

"$compiler" \
  -std=c++11 \
  -Wall -Wextra -Wpedantic \
  -fsyntax-only \
  -isystem "$maya_location/include" \
  "${project_include_args[@]}" \
  -isystem "$repo_root/third_party/libnoise/source" \
  -idirafter "$repo_root/tests/stubs" \
  "$repo_root/src/modules/point/DPoint/Common/DPointCommon.cpp" \
  "$repo_root/src/modules/curve/DCurve/DCurveUtil.cpp" \
  "$repo_root/src/modules/mesh/DMesh/common/DMeshUtil.cpp" \
  "$repo_root/src/core/DDataUtility/Generate/DPointMeshEmit.cpp" \
  "$repo_root/src/core/DDataUtility/Generate/DPointCurveEmit.cpp" \
  "$repo_root/src/core/DDataUtility/Generate/DPAnimationEmit.cpp" \
  "$repo_root/src/core/DDataUtility/Generate/DPointCircleEmit.cpp" \
  "$repo_root/src/core/DDataUtility/Generate/DPointSurfaceEmit.cpp" \
  "$repo_root/src/core/DDataUtility/Data/DPIntArrayGen.cpp" \
  "$repo_root/src/modules/point/DPoint/generate/data/DPDoubleArrayGen.cpp" \
  "$repo_root/src/modules/point/DPoint/generate/data/DPVectorArrayGen.cpp" \
  "$repo_root/src/core/DDataUtility/Generate/DPointFromFluid.cpp" \
  "$repo_root/src/core/DDataUtility/Particle/DPEmitter.cpp" \
  "$repo_root/src/core/DDataUtility/Particle/emitter/DPFluidEmitter.cpp" \
  "$repo_root/src/core/DDataUtility/Particle/field/DPFluidVectorField.cpp" \
  "$repo_root/src/core/DDataUtility/Particle/field/DPFluidNoiseField.cpp" \
  "$repo_root/src/core/DDataUtility/Particle/field/DPGoalField.cpp" \
  "$repo_root/src/modules/particles/particle/emitter/DMeshVolumeEmitter.cpp" \
  "$repo_root/src/modules/particles/particle/emitter/DPolygonEmitter.cpp" \
  "$repo_root/src/modules/particles/particle/emitter/DRadialGeometryEmitter.cpp" \
  "$repo_root/src/modules/particles/DEmitter/DEmitter.cpp" \
  "$repo_root/src/modules/curve/curveEmitter/curveEmitter.cpp" \
  "$repo_root/src/modules/particles/particle/field/DCurveField.cpp" \
  "$repo_root/src/modules/particles/particle/field/DKillField.cpp"

"$compiler" \
  -std=c++11 \
  -Wall -Wextra -Wpedantic \
  -fsyntax-only \
  -isystem "$maya_location/include" \
  -I"$repo_root/src/modules/mesh/polyModifier" \
  -I"$repo_root/src/core" \
  "$repo_root/src/modules/mesh/polyModifier/basePolyModifierCmd.cpp"

echo "Maya SDK registration, safety-helper, custom-data accessors, node-contract, feedback solvers, curve resampling, bounded geometry generators, polyBlast history, registered command, dependency-node group manipulators and direction generation, registered deformers, core mesh-analysis utilities, array generators, particle/fluid field, DPointFromFluid, DP and legacy/mesh particle/fluid emitters and fields (including DCurveField and DKillField), nucleus mesh bridge, particle command, DPGroupData, DPMerge, poly-modifier, $point_influence_result, $bounded_legacy_nodes_result, $procedural_generator_result, $baseline_data_nodes_result, $spatial_search_result, $text_generation_result, $opensubdiv_result, $viewport_result, $curve_invariant_result, $historical_geometry_result, and $legacy_bullet_result: OK"
