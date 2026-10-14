#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "$0")/.." && pwd)"

if [[ "$#" -gt 1 ]]; then
  echo "Usage: $0 [--source-only]" >&2
  exit 2
fi
case "${1:-}" in
  '') run_runtime_tests=true ;;
  --source-only) run_runtime_tests=false ;;
  *) echo "Usage: $0 [--source-only]" >&2; exit 2 ;;
esac

# Standalone use preserves the complete historical suite. CTest schedules these
# commands separately and runs only the source guards here to avoid duplication.
if [[ "$run_runtime_tests" == true ]]; then
  python3 "$repo_root/tests/check_clang_static_analysis_gate.py"
  python3 "$repo_root/tests/check_path_reservation_exception_safety.py"
  python3 "$repo_root/tests/check_sequence_failure_safety.py"
  python3 "$repo_root/tests/check_text_formatting_safety.py"
  python3 "$repo_root/tests/check_parser_codec_safety.py"
  python3 "$repo_root/tests/check_utility_scalar_safety.py"
  python3 "$repo_root/tests/check_diagnostic_text_safety.py"
  python3 "$repo_root/tests/check_plugin_registration_safety.py"
  python3 "$repo_root/tests/check_plugin_initialization_safety.py"
  python3 "$repo_root/tests/check_host_factory_safety.py"
  python3 "$repo_root/tests/check_command_syntax_safety.py"
  python3 "$repo_root/tests/check_command_transactions.py"
  python3 "$repo_root/tests/check_point_export_safety.py"
  python3 "$repo_root/tests/check_mesh_duplication_vertices.py"
  python3 "$repo_root/tests/check_poly_modifier_transactions.py"
  python3 "$repo_root/tests/check_vdb_grid_dispatch.py"
  python3 "$repo_root/tests/check_third_party_notices.py"
  python3 "$repo_root/tests/check_owned_utility_api.py"
  python3 "$repo_root/tests/check_owned_attribute_api.py"
  python3 "$repo_root/tests/check_owned_noise_fields.py"
  python3 "$repo_root/tests/check_viewport_staging_safety.py"
  python3 "$repo_root/tests/check_numeric_to_float_safety.py"
  python3 "$repo_root/tests/check_packing_input_safety.py"
  python3 "$repo_root/tests/check_draw_callback_safety.py"
  python3 "$repo_root/tests/check_light_draw_preparation_safety.py"
  python3 "$repo_root/tests/check_display_metadata_safety.py"
  python3 "$repo_root/tests/check_arnold_export_safety.py"
  python3 "$repo_root/tests/check_export_input_safety.py"
  python3 "$repo_root/tests/check_export_input_safety.py" --alembic
  python3 "$repo_root/tests/check_arnold_translation_safety.py"
  python3 "$repo_root/tests/check_arnold_mesh_metadata_safety.py"
  python3 "$repo_root/tests/check_bullet_connection_safety.py"
  python3 "$repo_root/tests/check_archival_connection_safety.py"
  python3 "$repo_root/tests/check_viewport_attribute_conversion.py"
  python3 "$repo_root/tests/check_vdb_viewport_clip_safety.py"
  python3 "$repo_root/tests/check_vdb_viewport_safety.py"
  python3 "$repo_root/tests/check_alembic_attribute_safety.py"
  python3 "$repo_root/tests/check_alembic_sample_safety.py"
  python3 "$repo_root/tests/check_alembic_transform_safety.py"
  python3 "$repo_root/tests/check_alembic_root_safety.py"
  python3 "$repo_root/tests/check_alembic_callback_safety.py"
  python3 "$repo_root/tests/check_alembic_geometry_safety.py"
  python3 "$repo_root/tests/check_alembic_destination_safety.py"
  python3 "$repo_root/tests/check_alembic_job_bounds_safety.py"
  python3 "$repo_root/tests/check_alembic_attribute_state_safety.py"
  python3 "$repo_root/tests/check_bullet_input_budget.py"
  python3 "$repo_root/tests/check_attribute_publication_paths.py"
  python3 "$repo_root/tests/check_attribute_curve_constraint_safety.py"
  python3 "$repo_root/tests/check_vdb_writer_status_safety.py"
  python3 "$repo_root/tests/check_attribute_vdb_read_status.py"
  python3 "$repo_root/tests/check_geometry_state_transactions.py"
  python3 "$repo_root/tests/check_curve_deformation_paths.py"
  python3 "$repo_root/tests/check_curve_connection_queries.py"
  python3 "$repo_root/tests/check_curve_world_replacement_paths.py"
  python3 "$repo_root/tests/check_curve_fill_vector_paths.py"
  python3 "$repo_root/tests/check_particle_boundary_safety.py"
  python3 "$repo_root/tests/check_dpoint_field_safety.py"
  python3 "$repo_root/tests/check_particle_output_safety.py"
  python3 "$repo_root/tests/check_swept_emitter_safety.py"
  python3 "$repo_root/tests/check_mesh_particle_emitter_safety.py"
  python3 "$repo_root/tests/check_bullet_mesh_input_safety.py"
  python3 "$repo_root/tests/check_bullet_shape_safety.py"
  python3 "$repo_root/tests/check_bullet_soft_body_safety.py"
  python3 "$repo_root/tests/check_bullet_constraint_safety.py"
  python3 "$repo_root/tests/check_bullet_step_safety.py"
  python3 "$repo_root/tests/check_bullet_dm_safety.py"
  python3 "$repo_root/tests/check_bullet_fracture_evaluation.py"
  python3 "$repo_root/tests/check_nrigid_safety.py"
  python3 "$repo_root/tests/check_nucleus_particle_safety.py"
  python3 "$repo_root/tests/check_delay_transform_safety.py"
  python3 "$repo_root/tests/check_feedback_trail_safety.py"
  python3 "$repo_root/tests/check_curve_resample_safety.py"
  python3 "$repo_root/tests/check_arc_remake_safety.py"
  python3 "$repo_root/tests/check_component_sequencer_safety.py"
  python3 "$repo_root/tests/check_rig_utility_safety.py"
  python3 "$repo_root/tests/check_curvature_face_safety.py"
  python3 "$repo_root/tests/check_array_node_safety.py"
  python3 "$repo_root/tests/check_scalar_array_safety.py"
  python3 "$repo_root/tests/check_array_utility_safety.py"
  python3 "$repo_root/tests/check_motion_rotation_safety.py"
  python3 "$repo_root/tests/check_frame_transform_safety.py"
  python3 "$repo_root/tests/check_animation_boundary_safety.py"
  python3 "$repo_root/tests/check_signal_compute_safety.py"
  python3 "$repo_root/tests/check_signal_blend_numeric_safety.py"
  python3 "$repo_root/tests/check_curve_boundary_safety.py"
  python3 "$repo_root/tests/check_curve_copy_safety.py"
  python3 "$repo_root/tests/check_mesh_curve_copy_safety.py"
  python3 "$repo_root/tests/check_curve_utility_safety.py"
  python3 "$repo_root/tests/check_curve_path_safety.py"
  python3 "$repo_root/tests/check_curve_periodic_safety.py"
  python3 "$repo_root/tests/check_point_foam_safety.py"
  python3 "$repo_root/tests/check_curve_extension_safety.py"
  python3 "$repo_root/tests/check_curve_collision_safety.py"
  python3 "$repo_root/tests/check_curve_partition_safety.py"
  python3 "$repo_root/tests/check_polygon_algorithm_safety.py"
  python3 "$repo_root/tests/check_slicer_algorithm_safety.py"
  python3 "$repo_root/tests/check_remesh_algorithm_safety.py"
  python3 "$repo_root/tests/check_curve_weight_map_safety.py"
  python3 "$repo_root/tests/check_curve_emitter_safety.py"
  python3 "$repo_root/tests/check_default_ramp_safety.py"
  python3 "$repo_root/tests/check_curve_uv_safety.py"
  python3 "$repo_root/tests/check_mesh_emitter_safety.py"
  python3 "$repo_root/tests/check_circle_surface_emitter_safety.py"
  python3 "$repo_root/tests/check_packing_emitter_safety.py"
  python3 "$repo_root/tests/check_geometry_read_safety.py"
  python3 "$repo_root/tests/check_timeshift_safety.py"
  python3 "$repo_root/tests/check_motion_history_safety.py"
  python3 "$repo_root/tests/check_motion_duplicator_safety.py"
  python3 "$repo_root/tests/check_motion_trail_safety.py"
  python3 "$repo_root/tests/check_generator_boundary_safety.py"
  python3 "$repo_root/tests/check_fur_aho_safety.py"
  python3 "$repo_root/tests/check_curve_generation_safety.py"
  python3 "$repo_root/tests/check_curve_source_generation_safety.py"
  python3 "$repo_root/tests/check_curve_conversion_safety.py"
  python3 "$repo_root/tests/check_legacy_mesh_safety.py"
  python3 "$repo_root/tests/check_poly_component_frame_safety.py"
  python3 "$repo_root/tests/check_noise_deformer_initialization.py"
  python3 "$repo_root/tests/check_legacy_io_foam_safety.py"
  python3 "$repo_root/tests/check_mesh_generation_safety.py"
  python3 "$repo_root/tests/check_mesh_merge_safety.py"
  python3 "$repo_root/tests/check_curve_transactions.py"
  python3 "$repo_root/tests/check_mesh_copy_safety.py"
  python3 "$repo_root/tests/check_attribute_validation_safety.py"
  python3 "$repo_root/tests/check_wire_array_safety.py"
  python3 "$repo_root/tests/check_mesh_utility_safety.py"
  python3 "$repo_root/tests/check_mesh_refinement_safety.py"
  python3 "$repo_root/tests/check_mesh_controls_safety.py"
  python3 "$repo_root/tests/check_mesh_remesh_safety.py"
  python3 "$repo_root/tests/check_mesh_noise_initialization.py"
  python3 "$repo_root/tests/check_mesh_group_safety.py"
  python3 "$repo_root/tests/check_primitive_metadata_safety.py"
  python3 "$repo_root/tests/check_group_transfer_safety.py"
  python3 "$repo_root/tests/check_point_utility_safety.py"
  python3 "$repo_root/tests/check_point_batch_transactions.py"
  python3 "$repo_root/tests/check_copy_utility_safety.py"
  python3 "$repo_root/tests/check_point_edit_safety.py"
  python3 "$repo_root/tests/check_direction_search_safety.py"
  python3 "$repo_root/tests/check_point_force_safety.py"
  python3 "$repo_root/tests/check_point_coordinate_output.py"
  python3 "$repo_root/tests/check_point_noise_output.py"
  python3 "$repo_root/tests/check_point_deformation_output.py"
  python3 "$repo_root/tests/check_collision_distance_safety.py"
  python3 "$repo_root/tests/check_affect_spiral_safety.py"
  python3 "$repo_root/tests/check_ray_safety.py"
  python3 "$repo_root/tests/check_color_noise_safety.py"
  python3 "$repo_root/tests/check_point_cache_safety.py"
  python3 "$repo_root/tests/check_vdb_point_bridge_safety.py"
  python3 "$repo_root/tests/check_vdb_smoothing_safety.py"
  python3 "$repo_root/tests/check_vdb_vector_merge_safety.py"
  python3 "$repo_root/tests/check_vdb_generation_safety.py"
  python3 "$repo_root/tests/check_vdb_distribution_safety.py"
  python3 "$repo_root/tests/check_vdb_mesh_safety.py"
  python3 "$repo_root/tests/check_vdb_meshing_safety.py"
  python3 "$repo_root/tests/check_vdb_particle_safety.py"
  python3 "$repo_root/tests/check_mesh_normal_safety.py"
  python3 "$repo_root/tests/check_mesh_convert_safety.py"
  python3 "$repo_root/tests/check_point_deform_safety.py"
  python3 "$repo_root/tests/check_convert_weights_safety.py"
  python3 "$repo_root/tests/check_adjust_space_safety.py"
  python3 "$repo_root/tests/check_point_uv_mesh_safety.py"
  python3 "$repo_root/tests/check_voronoi_safety.py"
  python3 "$repo_root/tests/check_polygon_generation_safety.py"
  python3 "$repo_root/tests/check_normal_numeric_safety.py"
  python3 "$repo_root/tests/check_force_range_numeric_safety.py"
fi

forbid_regex() {
  local file="$1"
  local pattern="$2"
  if [[ ! -f "$repo_root/$file" ]]; then
    return
  fi
  if grep -nE -- "$pattern" "$repo_root/$file"; then
    echo "Known C++ regression pattern found in $file: $pattern" >&2
    exit 1
  fi
}

require_regex() {
  local file="$1"
  local pattern="$2"
  if ! grep -qE -- "$pattern" "$repo_root/$file"; then
    echo "Required C++ safety pattern is missing in $file: $pattern" >&2
    exit 1
  fi
}

for retired_source in \
  "src/modules/point/DPoint/generate/fromAnimation/DPTrailCurve.cpp" \
  "src/modules/point/DPoint/generate/fromAnimation/DPTrailCurve.h" \
  "src/core/math/random.cpp" \
  "src/core/math/random.h"; do
  if [[ -e "$repo_root/$retired_source" ]]; then
    echo "Retired incomplete source returned: $retired_source" >&2
    exit 1
  fi
done

if [[ -e "$repo_root/src/core/math/DInterp.h" ]]; then
  echo "Retired unchecked interpolation templates returned" >&2
  exit 1
fi

commented_unsafe_code="$({
  grep -R -nE \
    --include='*.cpp' --include='*.h' --include='*.hpp' --include='*.inl' \
    '^[[:space:]]*//+[[:space:]]*(if[[:space:]]*\([^)]*\)[[:space:]]*)?delete[[:space:]]+[^;]+;[[:space:]]*$|^[[:space:]]*//+.*=[[:space:]]*new[[:space:]]+[^;]+;[[:space:]]*$|^[[:space:]]*//+.*\((const[[:space:]]+)?[A-Za-z_][A-Za-z0-9_:<>]*[[:space:]]*\*\)|^[[:space:]]*//+.*(reinterpret_cast|const_cast|static_cast<[^>]*\*>)|^[[:space:]]*//+.*&[A-Za-z_][A-Za-z0-9_]*\[[[:space:]]*0[[:space:]]*\]' \
    "$repo_root/src"
} || true)"
if [[ -n "$commented_unsafe_code" ]]; then
  echo "Unsafe disabled C++ ownership or cast example found:" >&2
  echo "$commented_unsafe_code" >&2
  exit 1
fi

commented_maya_data_recipe="$({
  grep -R -nE \
    --include='*.cpp' --include='*.h' --include='*.hpp' --include='*.inl' \
    '^[[:space:]]*//.*(std::auto_ptr|pluginDataPointer|MAnimControl::currentTime|MDataHandle.*\.data\(\))' \
    "$repo_root/src"
} || true)"
if [[ -n "$commented_maya_data_recipe" ]]; then
  echo "Disabled Maya plug-in data ownership recipe found:" >&2
  echo "$commented_maya_data_recipe" >&2
  exit 1
fi

obsolete_commented_random_code="$({
  grep -R -nE \
    --include='*.cpp' --include='*.h' --include='*.hpp' --include='*.inl' \
    '^[[:space:]]*//.*boost::(mt[0-9A-Za-z_]*|uniform_real|variate_generator)' \
    "$repo_root/src"
} || true)"
if [[ -n "$obsolete_commented_random_code" ]]; then
  echo "Obsolete disabled Boost random implementation found:" >&2
  echo "$obsolete_commented_random_code" >&2
  exit 1
fi

obsolete_commented_vdb_code="$({
  grep -R -nE \
    --include='*.cpp' --include='*.h' --include='*.hpp' --include='*.inl' \
    '^[[:space:]]*//.*(pluginDataPointer|tbb::task::self|std::string\(getenv\("MTOA_PATH"\))' \
    "$repo_root/src/modules/volume/VDB"
} || true)"
if [[ -n "$obsolete_commented_vdb_code" ]]; then
  echo "Obsolete disabled VDB API or ownership example found:" >&2
  echo "$obsolete_commented_vdb_code" >&2
  exit 1
fi

commented_include="$({
  grep -R -nE \
    --include='*.cpp' --include='*.h' --include='*.hpp' --include='*.inl' \
    '^[[:space:]]*//+[[:space:]]*#include' \
    "$repo_root/src"
} || true)"
if [[ -n "$commented_include" ]]; then
  echo "Commented-out include directive found:" >&2
  echo "$commented_include" >&2
  exit 1
fi

disabled_preprocessor_block="$({
  grep -R -nE \
    --include='*.cpp' --include='*.h' --include='*.hpp' --include='*.inl' \
    '^[[:space:]]*#if[[:space:]]+0([[:space:]]|$)' \
    "$repo_root/src"
} || true)"
if [[ -n "$disabled_preprocessor_block" ]]; then
  echo "Disabled #if 0 C++ block found:" >&2
  echo "$disabled_preprocessor_block" >&2
  exit 1
fi

commented_registration="$({
  grep -R -nE \
    --include='*.cpp' --include='*.h' --include='*.hpp' --include='*.inl' \
    '^[[:space:]]*//.*(registerNode|registerCommand|registerShape|RegisterTranslator|register[A-Za-z]+Override)' \
    "$repo_root/src/plugin" "$repo_root/src/integrations"
} || true)"
if [[ -n "$commented_registration" ]]; then
  echo "Commented-out plug-in registration found:" >&2
  echo "$commented_registration" >&2
  exit 1
fi

forbid_regex "src/modules/point/DPoint/Display/DPointDisplay.cpp" \
  'DPointLocatorData|DPointDisplayNodeOverride::prepareForDraw|color point flat shader'
require_regex "src/modules/point/DPoint/Display/DPointDisplay.cpp" \
  'mSolidUIShader\(nullptr\)'
require_regex "src/modules/volume/VDB/Edit/DOpenVDBVectorMerge.cpp" \
  'openvdb::util::wasInterrupted\(mInterrupt\)'
forbid_regex "src/modules/volume/VDB/Edit/DOpenVDBVectorMerge.cpp" \
  'count[[:space:]]*%[[:space:]]*100[[:space:]]*==[[:space:]]*0[[:space:]]*/\*'
require_regex "src/core/common/object/DPointVdbSearch.h" \
  'class[[:space:]]+DPointVdbParticleList'
forbid_regex "src/core/common/object/DPointVdbSearch.h" \
  'class[[:space:]]+DVdbParticleList'
require_regex "src/modules/volume/VDB/Convert/DOpenVDBConvert.cpp" \
  'getConnectedFaces\(fIds,[[:space:]]*&stat\)'
require_regex "src/modules/volume/VDB/Convert/DOpenVDBConvert.cpp" \
  '![^;]*seamGroup\[j\][[:space:]]*\|\|'
require_regex "src/modules/volume/VDB/Convert/DOpenVDBConvert.cpp" \
  'interiorGroup\[j\]'
forbid_regex "src/modules/volume/VDB/Convert/DOpenVDBConvert.cpp" \
  'seamGroup\[j\][[:space:]]*/\*'
for version_agnostic_vdb_source in \
  "src/modules/volume/VDB/Edit/DOpenVDBAdvect.cpp" \
  "src/modules/volume/VDB/Edit/DOpenVDBMorphLevelSet.cpp"; do
  forbid_regex "$version_agnostic_vdb_source" \
    'openvdb::v[0-9]+_[0-9]+::'
  require_regex "$version_agnostic_vdb_source" \
    'openvdb::math::TemporalIntegrationScheme'
done
for vector_advection_source in \
  "src/modules/volume/VDB/Edit/DOpenVDBAdvect.cpp" \
  "src/modules/volume/VDB/Edit/DOpenVDBAdvectPoints.cpp"; do
  forbid_regex "$vector_advection_source" \
    'DVdbGridType::kVec3f[[:space:]]*/\*'
  require_regex "$vector_advection_source" \
    'DVdbGridType::kVec3d'
done
require_regex "src/modules/volume/VDB/Edit/DOpenVDBAdvect.cpp" \
  'processGrids<openvdb::Vec3DGrid'
require_regex "src/modules/volume/VDB/Edit/DOpenVDBAdvectPoints.cpp" \
  'parms\.mSteps[[:space:]]*<=[[:space:]]*0'
require_regex "src/modules/volume/VDB/Edit/DOpenVDBAdvectPoints.cpp" \
  'validateAdvectionBudget'
require_regex "src/modules/volume/VDB/Edit/DOpenVDBAdvectPoints.cpp" \
  'outputDPData->updateBBox\(\)'
require_regex "src/modules/volume/VDB/Edit/DOpenVDBAdvectPoints.cpp" \
  'outputCurves->hasValidCurveTopology\(\)'
forbid_regex "src/modules/volume/VDB/Edit/DOpenVDBAdvectPoints.cpp" \
  'pluginDataPointer|^[[:space:]]*//.*(cout|parms\.)'
forbid_regex "src/modules/volume/VDB/Edit/DOpenVDBAdvectPoints.h" \
  'boost::scoped_ptr|stringTo(Propagation|Integration)Type'
require_regex "src/modules/volume/VDB/Edit/DOpenVDBAdvectPoints.cpp" \
  'std::unique_ptr<ProjectorType>'
forbid_regex "src/modules/volume/VDB/Edit/DOpenVDBAdvectPoints.h" \
  'static_cast<const GridType&>|^[[:space:]]*//.*cout'
require_regex "src/modules/volume/VDB/Common/DVdbGridDispatch.h" \
  '!vdbGridCast<First>\(base\)'
require_regex "src/modules/volume/VDB/Common/DVdbGridDispatch.h" \
  'struct Family<First, Rest\.\.\.>'
require_regex "src/modules/volume/VDB/Common/DVdbGridDispatch.h" \
  'enum class DVdbGridType'
forbid_regex "src/modules/volume/VDB/Common/DOpenVDBUtil.h" \
  'DOMMY_DECLARE_VDB_GRID_DISPATCH|vdb_dispatch_detail|UTvdb|UT_VDB'
require_regex "src/modules/volume/VDB/Edit/DOpenVDBAnalysis.cpp" \
  '!result[[:space:]]*\|\|[[:space:]]*result\.get\(\)[[:space:]]*=='
require_regex "src/modules/volume/VDB/Edit/DOpenVDBAnalysis.cpp" \
  'outputVdb->insert\(gridRef\)'
forbid_regex "src/modules/volume/VDB/Edit/DOpenVDBAnalysis.cpp" \
  'pluginDataPointer'
require_regex "src/modules/volume/VDB/Edit/DOpenVDBResample.cpp" \
  'dommy::maya::isFiniteMatrix'
require_regex "src/modules/volume/VDB/Edit/DOpenVDBResample.cpp" \
  '!processed[[:space:]]*\|\|[[:space:]]*!outGrid'
forbid_regex "src/modules/volume/VDB/Edit/DOpenVDBResample.cpp" \
  'pluginDataPointer|auto_ptr|^[[:space:]]*//.*cout'
forbid_regex "src/modules/volume/VDB/Edit/DOpenVDBFilterLevelSet.h" \
  'stringTo(FilterType|Accuracy)|buildFilterMenu'
require_regex "src/modules/mesh/DMesh/Dynamics/Bullet/db_collision_shape_base.h" \
  'std::vector<std::unique_ptr<db_collision_shape_base>'
require_regex "src/modules/mesh/DMesh/Dynamics/Bullet/db_constraint_base.h" \
  'enableSpring\(0,[[:space:]]*enabled\)'
forbid_regex \
  "src/modules/mesh/DMesh/Dynamics/Bullet/btConvexConvexMprAlgorithm.cpp" \
  'btAssert\(0\)|#if[[:space:]]+0'
require_regex \
  "src/modules/mesh/DMesh/Dynamics/Bullet/btConvexConvexMprAlgorithm.cpp" \
  'return[[:space:]]+btScalar\(1\.0\)'
if [[ "$(grep -cE 'enableSpring\([0-5],[[:space:]]*enabled\)' \
  "$repo_root/src/modules/mesh/DMesh/Dynamics/Bullet/db_constraint_base.h")" -ne 6 ]]; then
  echo "Six-axis spring enablement must honor the caller-provided state." >&2
  exit 1
fi
forbid_regex "src/modules/mesh/DMesh/Display/DMeshDisplay.cpp" \
  'mBBox\.width\(\).+mBBox\.width\(\).+mBBox\.width\(\)'
require_regex "src/modules/mesh/DMesh/Display/DMeshDisplay.cpp" \
  'mBBox\.height\(\)[[:space:]]*>'
require_regex "src/modules/mesh/DMesh/Display/DMeshDisplay.cpp" \
  'mBBox\.depth\(\)[[:space:]]*>'
require_regex "src/core/common/utility/DMayaValueValidation.h" \
  'isFiniteBoundingBox\(const MBoundingBox&'
require_regex "src/core/common/utility/DMayaValueValidation.h" \
  'isFiniteColor\(const MColor&'
require_regex "src/modules/particles/particle/field/DKillField.cpp" \
  'dommy::maya::isFiniteBoundingBox\(bounds\)'
forbid_regex "src/modules/particles/particle/field/DKillField.cpp" \
  'bool[[:space:]]+isFiniteBoundingBox\(const MBoundingBox&'

forbid_regex "src/core/common/object/DNodeOperation.cpp" 'bbox\.width\(\)[[:space:]]*==[[:space:]]*bbox\.height\(\)'
forbid_regex "src/core/common/object/DNodeOperation.cpp" 'MPoint corner1\([[:space:]]*minX,[[:space:]]*minY,[[:space:]]*minY'
forbid_regex "src/core/common/object/DNodeOperation.cpp" 'if[[:space:]]*\([[:space:]]*min\[[012]\][[:space:]]*>[[:space:]]*max\[[012]\]'
forbid_regex "src/core/common/object/DNodeOperation.h" 'void[[:space:]]+DGetPlugTime'
require_regex "src/core/common/object/DNodeOperation.h" 'MStatus[[:space:]]+DGetPlugTimeDPPP'
require_regex "src/core/common/object/DNodeOperation.cpp" 'currentTime\.unit\(\)'
require_regex "src/core/common/object/DNodeOperation.h" \
  'MStatus[[:space:]]+readPlugValue\(const MPlug& plug,[[:space:]]*MVector& value\)'
require_regex "src/core/common/object/DNodeOperation.cpp" \
  'value[[:space:]]*=[[:space:]]*MVector\(components\[0\],[[:space:]]*components\[1\],[[:space:]]*components\[2\]\)'
forbid_regex "src/core/common/object/DNodeOperation.h" \
  'getPlug(Vector|Boolean|Int|Double|String)Value|vectorPlugValue|getMColorFromPlug'
forbid_regex "src/core/common/object/DNodeOperation.cpp" \
  'getPlug(Vector|Boolean|Int|Double|String)Value|vectorPlugValue|getMColorFromPlug'
require_regex "src/core/common/object/DNodeOperation.h" \
  'MStatus[[:space:]]+getMeshInformation'
forbid_regex "src/core/common/object/DNodeOperation.h" 'getMeshInfomation'
forbid_regex "src/core/common/object/DNodeOperation.cpp" 'getMeshInfomation'
forbid_regex "src/core/common/object/DNodeOperation.cpp" \
  'void[[:space:]]+getBoundingBox\(MFloatPointArray'
require_regex "src/core/common/object/DNodeOperation.cpp" 'bounds\.empty\(\)'
forbid_regex "src/modules/mesh/DMesh/Utility/DMSlicer.cpp" \
  'planeBaseNorm[[:space:]]*-[[:space:]]*planeBasePos|^[[:space:]]*/\*'
require_regex "src/modules/mesh/DMesh/Utility/DMSlicer.cpp" \
  'planeDirection[[:space:]]*\*[[:space:]]*step'
require_regex "src/modules/mesh/DMesh/Utility/DMSlicer.cpp" \
  'checkedProduct\('
require_regex "src/modules/mesh/DMesh/Utility/DMSlicer.cpp" \
  'hasValidCurveTopology\(\)'
for dependency_reduced_header in \
  "src/core/common/object/DNodeOperation.h" \
  "src/core/common/object/DBBoxOp.h" \
  "src/core/Data/DGroupCommon.h"; do
  forbid_regex "$dependency_reduced_header" 'DMayaHeaders\.h'
done
require_regex "src/core/common/utility/DCheckedSize.h" 'boundedCopyCount'
require_regex "src/core/common/utility/DCheckedSize.h" 'copyPrefix'
require_regex "src/core/common/utility/DNumericValidation.h" \
  'bool[[:space:]]+checkedNumericCast'
require_regex "src/core/common/utility/DVectorLookup.h" 'bool[[:space:]]+findIndex'
forbid_regex "src/core/common/utility/DVectorLookup.h" 'return[[:space:]]+-1'
if [[ "$(grep -c 'inputData->hasValidCurveTopology' \
  "$repo_root/src/modules/curve/DCurve/DCurveUtil.cpp")" -lt 4 ]]; then
  echo "Every DCurve input helper must reject invalid topology." >&2
  exit 1
fi
if [[ "$(grep -c 'inputData->hasValidMeshTopology' \
  "$repo_root/src/modules/mesh/DMesh/common/DMeshUtil.cpp")" -lt 4 ]]; then
  echo "Every DMesh input helper must reject invalid topology." >&2
  exit 1
fi
forbid_regex "src/modules/mesh/DMesh/Dynamics/Bullet/DMbtFracture.cpp" 'rbList\[findStdVecIndex'
require_regex "src/modules/point/DPoint/Common/DPAttributeCommon.cpp" 'dommy::attribute::copyMayaArrayPrefix'
require_regex "src/modules/mesh/legacy/dupMesh.h" 'bool[[:space:]]+isValid\(\)[[:space:]]+const'
require_regex "src/modules/mesh/legacy/dupMesh.cpp" 'totalEdgeNum[[:space:]]*>[[:space:]]*0U'
require_regex "src/modules/mesh/legacy/dupMesh.cpp" 'normIdsArray\.length\(\)[[:space:]]*!=[[:space:]]*storage_->polyConnectList\.length\(\)'
forbid_regex "src/modules/mesh/legacy/dupMesh.cpp" 'isEdgeSmooth\([[:space:]]*0[[:space:]]*\)'
require_regex "src/modules/point/DPoint/mesh/DPMeshDuplicator.cpp" 'buildMeshDuplicationBudget'
require_regex "src/modules/point/DPoint/mesh/DPMeshDuplicator.cpp" 'normalizeDuplicationSourceIndices'
require_regex "src/modules/point/DPoint/mesh/DPMeshDuplicator.cpp" 'createPluginData'
require_regex "src/modules/point/DPoint/mesh/DPMeshDuplicator.cpp" 'outputMeshes\.reserve'
require_regex "src/modules/point/DPoint/mesh/DPMeshDuplicator.cpp" 'isFloatRepresentablePoint\(pt\)'
forbid_regex "src/core/common/data/DDataOp.h" 'DVecVectorListToMayaVecArray|DMayaVecToMayaVec'
forbid_regex "src/modules/point/DPoint/mesh/DPMeshDuplicator.cpp" 'boost::|getMIntArrayFromAttribute|static_cast<DMeshData[[:space:]]*\*>'
forbid_regex "src/modules/point/DPoint/mesh/DPMeshDuplicator.cpp" '(^|[^[:alnum:]_])cout[[:space:]]*<<'
for safe_output_source in \
  "src/modules/curve/DCurve/Utility/DCCopy.cpp" \
  "src/modules/curve/DCurve/Utility/DCFitCopy.cpp" \
  "src/modules/point/DPoint/mesh/DPMeshDuplicator.cpp" \
  "src/modules/mesh/DMesh/Dynamics/Bullet/DMbtFracture.cpp"; do
  require_regex "$safe_output_source" 'dommy::maya::createPluginData'
  forbid_regex "$safe_output_source" 'static_cast<(DPData|DMeshData|DCurveData)[[:space:]]*\*>'
done
for point_output_source in \
  "src/modules/point/DPoint/Deform/DPVectorDeform.cpp" \
  "src/modules/point/DPoint/Deform/DPPointDeform.cpp" \
  "src/modules/point/DPoint/Utility/DPBlend.cpp" \
  "src/modules/point/DPoint/Utility/DPSmooth.cpp"; do
  require_regex "$point_output_source" 'dommy::maya::setPluginDataOutput<DPData>'
  require_regex "$point_output_source" 'copyRepeatedFrom\('
  forbid_regex "$point_output_source" 'setDPData\(|replacePositions\('
  forbid_regex "$point_output_source" 'static_cast<DPData[[:space:]]*\*>'
done
for point_view_source in \
  "src/modules/point/DPoint/Deform/DPVectorDeform.cpp" \
  "src/modules/point/DPoint/Utility/DPBlend.cpp" \
  "src/modules/point/DPoint/Utility/DPSmooth.cpp"; do
  forbid_regex "$point_view_source" 'getPositionConst\('
done
for point_workspace_source in \
  "src/modules/point/DPoint/Deform/DPVectorDeform.cpp" \
  "src/modules/point/DPoint/Utility/DPBlend.cpp" \
  "src/modules/point/DPoint/Utility/DPSmooth.cpp" \
  "src/core/DDataUtility/Utility/DPTransform.cpp" \
  "src/modules/point/DPoint/animation/DPAnimMatrix.cpp" \
  "src/modules/point/DPoint/animation/DPMotionCreate.cpp" \
  "src/modules/point/DPoint/animation/DPRotation.cpp" \
  "src/modules/point/DPoint/Utility/DPFastNoise.cpp" \
  "src/modules/point/DPoint/Utility/DPDFNoise.cpp" \
  "src/modules/point/DPoint/Utility/DPPyroNoise.cpp"; do
  require_regex "$point_workspace_source" 'dommy::point::preflightPointPayload'
done
for point_noise_source in \
  "src/modules/point/DPoint/Utility/DPFastNoise.cpp" \
  "src/modules/point/DPoint/Utility/DPDFNoise.cpp"; do
  require_regex "$point_noise_source" 'dommy::point::preflightPrimitivePointPayload'
  require_regex "$point_noise_source" 'dommy::maya::setPluginDataOutput<DPData>'
  require_regex "$point_noise_source" 'copyRepeatedFrom\('
  forbid_regex "$point_noise_source" 'setDPData\(|getPositionConst\('
done
for animation_connection_source in \
  "src/modules/point/DPoint/animation/DPAnimMatrix.cpp" \
  "src/modules/point/DPoint/animation/DPMotionCreate.cpp" \
  "src/modules/point/DPoint/animation/DPRotation.cpp"; do
  forbid_regex "$animation_connection_source" 'isConnected\([[:space:]]*\)'
done
require_regex "src/core/DDataUtility/Data/DPData.cpp" 'buildPointCopyPlan\(data,[[:space:]]*1U,[[:space:]]*copyPlan\)'
require_regex "src/core/common/object/DTransformOp.cpp" 'dommy::point::PointPayloadBudget workspace'
require_regex "src/core/common/object/DTransformOp.cpp" 'workspace\.add\(matrixArray\.capacity\(\), sizeof\(MMatrix\)\)'
require_regex "src/modules/point/DPoint/mesh/DPMeshDuplicator.cpp" 'new std::vector<MMatrix>\(\)'
forbid_regex "src/modules/point/DPoint/mesh/DPMeshDuplicator.cpp" 'new std::vector<MMatrix>\(totalCount'
require_regex "src/modules/point/DPoint/mesh/DPMeshDuplicator.cpp" 'candidate\.mAnimationInputIsData = true'
require_regex "src/core/common/object/DTransformOp.cpp" 'getMatrixArrayFromData\(param\.mAnim_Mat_InputData_Object, values\)'
require_regex "src/modules/point/DPoint/Common/DPointCommon.cpp" 'return read\(input\.data\(\), output\)'
for checked_array_connection in \
  "src/modules/point/DPoint/Utility/DPArraySize.cpp" \
  "src/modules/point/DPoint/Utility/DPVecCompare.cpp"; do
  forbid_regex "$checked_array_connection" 'isConnected\([[:space:]]*\)'
done
for block_array_consumer in \
  "src/modules/point/DPoint/Utility/DPVecCompare.cpp" \
  "src/modules/point/DPoint/Utility/DPPointToFloat.cpp" \
  "src/modules/point/DPoint/Modify/DPEditDouble.cpp" \
  "src/modules/point/DPoint/Deform/DPConvertWeights.cpp" \
  "src/modules/volume/VDB/Generate/DOpenVDBFromParticle.cpp"; do
  require_regex "$block_array_consumer" 'get(Vector|Point|Double|Float)ArrayFromData\(block,'
  forbid_regex "$block_array_consumer" 'get(Vector|Point|Double|Float)ArrayFromData\(thisMObject\('
done
require_regex "src/modules/point/DPoint/Utility/DPArraySize.cpp" 'getPointArrayFromData\(data,'
forbid_regex "src/modules/point/DPoint/Utility/DPArraySize.cpp" 'objectPlug\.getValue'
require_regex "src/core/common/object/DNodeOperation.cpp" 'getDoubleArrayFromData\(data, arrayAttribute, source\)'
require_regex "src/modules/point/DPoint/animation/DPMotionCreate.cpp" 'copyRepeatedFrom\(\*inputDPData, 1U,'
forbid_regex "src/modules/point/DPoint/animation/DPMotionCreate.cpp" 'setDPData\(|replacePositions\(|updateBBox\('
require_regex "src/modules/point/DPoint/Deform/DPPointDeform.cpp" \
  'DOMMY_RETURN_IF_FAILURE\(getValidInputDP\(inDP, data, input\)\)'
forbid_regex "src/core/common/object/DTransformOp.h" 'c minus:|mat44|MQuaternion yQuat'
require_regex "src/modules/point/DPoint/Utility/DPTimeShift.cpp" 'dommy::random::generateFloatValues'
require_regex "src/modules/point/DPoint/Utility/DPTimeShift.cpp" 'requireTimeSamplingInputs'
require_regex "src/modules/point/DPoint/Utility/DPTimeShift.cpp" 'dommy::numeric::checkedScalarArrayCopy'
forbid_regex "src/modules/point/DPoint/Utility/DPTimeShift.cpp" 'DScalarMayaArrayToVectorList'
forbid_regex "tests/cpp/timeshift_doubles.h" 'DScalarMayaArrayToVectorList|checkedScalarArrayCopy[[:space:]]*\('
forbid_regex "src/modules/point/DPoint/Utility/DPTimeShift.cpp" '\(uint\)(timeOffset|randVal)|CHECK_MSTATUS\('
forbid_regex "src/modules/point/DPoint/Utility/DPTimeShift.cpp" 'boost::(mt19937|uniform_real)'
require_regex "src/modules/point/DPoint/Utility/DPTimeShift.cpp" 'if[[:space:]]*\(!body\.status\)[[:space:]]+return[[:space:]]+body\.status'
require_regex "src/modules/volume/VDB/Convert/DOpenVDBConvert.h" 'MStatus[[:space:]]+referenceMeshing'
require_regex "src/modules/volume/VDB/Convert/DOpenVDBConvert.cpp" 'const[[:space:]]+openvdb::Index32[[:space:]]+primitiveIndex[[:space:]]*=[[:space:]]*primitives\[p\]'
require_regex "src/modules/volume/VDB/Convert/DOpenVDBConvert.cpp" 'j[[:space:]]*!=[[:space:]]*connectedFace'
forbid_regex "src/modules/volume/VDB/Convert/DOpenVDBConvert.cpp" 'primitives\[n\]|grids\.clear\(\)'
forbid_regex "src/modules/volume/VDB/Convert/DOpenVDBConvert.cpp" \
  'aColorType|setMetadata|fragments\.push_back'
require_regex "src/modules/volume/VDB/Convert/DOpenVDBToSpheres.cpp" \
  'getValidInputVDB'
require_regex "src/modules/volume/VDB/Convert/DOpenVDBToSpheres.cpp" \
  'dommy::generation::checkedProduct'
require_regex "src/modules/volume/VDB/Convert/DOpenVDBToSpheres.cpp" \
  'kMaxGenerated(Point|Face|Transform)Count'
require_regex "src/modules/volume/VDB/Convert/DOpenVDBToSpheres.cpp" \
  'output mesh exceeds the topology safety budget'
forbid_regex "src/modules/volume/VDB/Convert/DOpenVDBToSpheres.cpp" \
  'idsArray|idNumber|if[[:space:]]*\([[:space:]]*preserve[[:space:]]*\)|createSphere'
require_regex "src/core/common/object/DPolygonUtil.cpp" \
  'MStatus[[:space:]]+findCollapsibleEdge'
require_regex "src/core/common/object/DPolygonUtil.cpp" \
  'maximumCollapseCount'
forbid_regex "src/core/common/object/DPolygonUtil.cpp" \
  'checkZeroLengthEdge|while[[:space:]]*\([[:space:]]*true[[:space:]]*\)|void[[:space:]]+createSphere'
forbid_regex "src/modules/mesh/DMesh/Dynamics/Bullet/DBulletFractureSolver.cpp" \
  'gNumObjects|createRigidBody|m_ownedCollisionShapes|m_reInitialize'
forbid_regex "src/modules/mesh/DMesh/Dynamics/Bullet/DBulletFractureSolver.h" \
  'DBtSolverOptionParams|m_ownedCollisionShapes|m_reInitialize'
forbid_regex "src/modules/dynamics/DBullet/DM/DMBulletTest.cpp" \
  '\(customDBData[[:space:]]*\*\)'
forbid_regex "src/modules/point/DPoint/math/DPMathUtility.cpp" 'if[[:space:]]*\([[:alnum:]_]+\.size\(\),'
require_regex "src/modules/point/DPoint/math/DPMathUtility.cpp" \
  'MStatus[[:space:]]+DPMGetConstantValues'
require_regex "src/modules/point/DPoint/math/DPMathUtility.cpp" \
  'output\.setDouble'
require_regex "src/modules/point/DPoint/math/DPMathUtility.cpp" \
  'isValidMathOperation'
require_regex "src/modules/point/DPoint/math/DPMathUtility.cpp" \
  'isValidSingleMathOperation'
require_regex "src/modules/point/DPoint/math/DPMathUtility.cpp" \
  'operand\.size\(\)[[:space:]]*!=[[:space:]]*result\.size\(\)'
forbid_regex "src/modules/point/DPoint/math/DPMathUtility.cpp" \
  'outplug\.setValue|outputData\.set\([[:space:]]*outplug'
require_regex "src/modules/point/DPoint/math/DPMathUtility.cpp" \
  'evaluateMayaMathInputs'
require_regex "src/modules/point/DPoint/math/DPMathUtility.cpp" \
  'convertMayaMathScalars<MIntArray, int>'
require_regex "src/modules/point/DPoint/math/DPMathUtility.cpp" \
  'validateMathOperandBudget'
require_regex "src/modules/point/DPoint/math/DPMathUtility.cpp" \
  'DMathVec3DataList\(1U, value\)'
forbid_regex "src/modules/point/DPoint/math/DPMathUtility.cpp" \
  'CHECK_MSTATUS_AND_RETURN_IT\(|newDataArray\.append\(int\('
require_regex "src/modules/point/DPoint/math/DMathEvaluation.h" \
  'checkedMathBinary'
require_regex "src/core/common/utility/DMayaNumericInput.h" \
  'checkedScalarArrayCopy'
require_regex "src/core/common/utility/DMayaNumericInput.h" \
  'handle\.asGenericDouble\(\)'
require_regex "src/modules/point/DPoint/math/DPMathUtility.cpp" \
  'output\.setGenericDouble'
require_regex "src/core/common/object/DNodeOperation.h" \
  'readPlugValue\(const MPlug& plug, MDataHandle& value\)[[:space:]]*=[[:space:]]*delete'
require_regex "src/modules/volume/VDB/Edit/DOpenVDBRemap.cpp" \
  'dommy::vdb::remapValue'
require_regex "src/modules/volume/VDB/Edit/DOpenVDBValuePolicy.h" \
  'numeric::checkedNumericCast'
require_regex "src/modules/volume/VDB/Edit/DOpenVDBRemap.cpp" \
  '!processVdbGrid'
forbid_regex "src/modules/volume/VDB/Edit/DOpenVDBRemap.cpp" \
  'evalMinMax|NodeMinMax|mInfo'
for generic_math_node in \
  "src/modules/point/DPoint/math/DPMath.cpp" \
  "src/modules/point/DPoint/math/DPMathFunc.cpp"; do
  require_regex "$generic_math_node" 'processDDataMathOutput'
  forbid_regex "$generic_math_node" \
    'output\.set\([[:space:]]*(outputDPData|outDCrvs|outDMeshes)[[:space:]]*\)'
done
for math_node in \
  "src/modules/point/DPoint/math/DPMath.cpp" \
  "src/modules/point/DPoint/math/DPAdd.cpp" \
  "src/modules/point/DPoint/math/DPSubtract.cpp" \
  "src/modules/point/DPoint/math/DPMultiply.cpp" \
  "src/modules/point/DPoint/math/DPDivide.cpp" \
  "src/modules/point/DPoint/math/DPModulo.cpp" \
  "src/modules/point/DPoint/math/DPPow.cpp" \
  "src/modules/point/DPoint/math/DPMin.cpp" \
  "src/modules/point/DPoint/math/DPMax.cpp" \
  "src/modules/point/DPoint/math/DPMathFunc.cpp" \
  "src/modules/point/DPoint/math/DPNegative.cpp" \
  "src/modules/point/DPoint/math/DPSqrt.cpp" \
  "src/modules/point/DPoint/math/DPFloor.cpp" \
  "src/modules/point/DPoint/math/DPRound.cpp" \
  "src/modules/point/DPoint/math/DPCeil.cpp" \
  "src/modules/point/DPoint/math/DPExponential.cpp" \
  "src/modules/point/DPoint/math/DPAbsolute.cpp"; do
  require_regex "$math_node" \
    'return[[:space:]]+processDDataMathOutput'
  require_regex "$math_node" \
    'return[[:space:]]+processMayaNumericMathFunc'
  forbid_regex "$math_node" \
    'output\.set\([[:space:]]*(outputDPData|outDCrvs|outDMeshes)[[:space:]]*\)'
done
forbid_regex "src/modules/mesh/DMesh/Dynamics/Bullet/DBFractureDynamicsWorld.cpp" 'f0[[:space:]]*==[[:space:]]*f1[[:space:]]*=='
forbid_regex "src/modules/point/DPoint/Utility/DPTimeShift.cpp" 'new[[:space:]]+(DScalarDataList|DAUIntList)'
forbid_regex "src/modules/curve/DCurve/IO/DWriteDCurveToAi.cpp" 'paramAdjustCount|unsigned[[:space:]]+[^;]*=[[:space:]]*-2'
forbid_regex "src/modules/volume/VDB/IO/DReadOpenVDB.cpp" 'MAnimControl'
forbid_regex "src/modules/volume/VDB/IO/DWriteOpenVDB.cpp" 'outputVdb[[:space:]]*=[[:space:]]*vdb'
forbid_regex "src/modules/particles/particle/field/DCurveField.cpp" 'velocityCachedArray|std::cout|(^|[^[:alnum:]_])cout[[:space:]]*<<'
forbid_regex "src/core/common/utility/DPRayUtility.h" 'MItMeshVertex|tbb::parallel_for'
forbid_regex "src/modules/point/DPoint/Utility/DPPyroNoise.cpp" 'tbb::parallel_for'
forbid_regex "src/modules/point/DPoint/nurbs/DPUVGen.cpp" 'tbb::parallel_for'
forbid_regex "src/modules/point/DPoint/Common/DPointCommon.h" 'tbb::parallel_for'
forbid_regex "src/modules/volume/VDB/Data/DOpenVDBData.cpp" 'mGrids\.reserve\(mGrids\.size\(\)[[:space:]]*\+[[:space:]]*rhs\.size\(\)\)'
forbid_regex "src/core/DDataUtility/Particle/nobject/DPnParticle.cpp" 'delete[[:space:]]+nObj'
forbid_regex "src/core/Data/DPrimBaseData.cpp" 'return[[:space:]]+&pointArray\[0\]\[0\]'
forbid_regex "src/core/DDataUtility/Data/DPData.cpp" 'return[[:space:]]+&pointArray\[0\]\[0\]'
forbid_regex "src/core/DDataUtility/Data/DPData.cpp" 'out[[:space:]]*<<[[:space:]]*vertex\[[012]\]'
require_regex "src/core/DDataUtility/Data/DPData.cpp" 'dommy::generation::checkedCount'
require_regex "src/core/DDataUtility/Data/DPData.cpp" 'dommy::numeric::checkedFloatCast'
require_regex "src/core/DDataUtility/Data/DPData.cpp" 'pointArray\.swap\(decodedPoints\)'
require_regex "src/core/DDataUtility/Data/DPData.cpp" 'std::ostringstream[[:space:]]+encoded'
require_regex "src/core/DDataUtility/Utility/DPMerge.cpp" 'setPluginDataOutput<DPData>'
forbid_regex "src/core/DDataUtility/Utility/DPMerge.cpp" 'static_cast<DPData[[:space:]]*\*>'
require_regex "src/core/DDataUtility/Utility/DPTransform.h" 'class[[:space:]]+DPTransform[[:space:]]+final'
require_regex "src/core/DDataUtility/Utility/DPTransform.cpp" 'connectToRotationCenterPlug\(rotatePivotPlug\)'
require_regex "src/core/DDataUtility/Utility/DPTransform.cpp" 'isFloatRepresentablePoint'
forbid_regex "src/core/DDataUtility/Utility/DPTransform.cpp" 'MFnDagNode|rotationChangedCallback|static[[:space:]]+MEulerRotation'
forbid_regex "src/modules/point/DPoint/math/other/DPRemap.cpp" 'fabsf\(m_min[123]D\)[[:space:]]*/'
forbid_regex "src/modules/volume/VDB/Edit/DOpenVDBRemap.cpp" 'pos[[:space:]]*\*[[:space:]]*xScale'
forbid_regex "src/modules/mesh/DMesh/Dynamics/Bullet/DBFractureDynamicsWorld.cpp" 'removeRigidBody\(fracObj\).*should it'
forbid_regex "src/modules/curve/DCurve/Cmd/Abc/MayaAttributeWriter.cpp" 'getData2Double\(v2dVal\[0\],[[:space:]]*v2dVal\[0\]\)'
forbid_regex "src/modules/curve/DCurve/Cmd/Abc/MayaAttributeWriter.cpp" '&\(val\.front\(\)\)|dimSize[[:space:]]*\*|length[[:space:]]*\*[[:space:]]*extent'
forbid_regex "src/modules/curve/DCurve/Cmd/Abc/DPointWriter.cpp" '&(position|velocity)\.front\(\)'
forbid_regex "src/modules/curve/DCurve/Cmd/Abc/DPointWriter.cpp" '&val\[0\]'
forbid_regex "src/modules/curve/DCurve/Cmd/Abc/DCurveWriter.cpp" '&val\[0\]'
forbid_regex "src/modules/curve/DCurve/Cmd/Abc/DMeshWriter.cpp" '&val\[0\]'
forbid_regex "src/modules/volume/VDB/Edit/DOpenVDBClip.cpp" 'fmax\(fmax\(minp\.x\(\),minp\.y\(\)\),minp\.y\(\)\)'
forbid_regex "src/modules/mesh/polyModifier/basePolyModifierCmd.cpp" 'dgeval[[:space:]]+-src'
forbid_regex "src/modules/mesh/polyModifier/basePolyModifierCmd.cpp" '#[[:space:]]*ifdef[[:space:]]+_DEBUG'
require_regex "src/modules/mesh/polyModifier/basePolyModifierCmd.cpp" 'throw StatusFailure\{status\}'
require_regex "src/modules/mesh/polyModifier/basePolyModifierCmd.cpp" 'graph\.newPlugValue'
forbid_regex "src/modules/volume/VDB/Edit/DOpenVDBFracture.cpp" 'badTypeList\.push_back\(residual->getName\(\)\)'
forbid_regex "src/modules/volume/VDB/Edit/DOpenVDBFracture.cpp" 'instancePoints\[i\][[:space:]]*=[[:space:]]*transform->worldToIndex'
forbid_regex "src/modules/volume/VDB/Edit/DOpenVDBFracture.cpp" '(min|max)[XYZ][[:space:]]*=[[:space:]]*-?9999'
forbid_regex "src/modules/curve/DCurve/Display/DCMarker.cpp" 'p[[:space:]]*<[[:space:]]*attrValues\.size\(\)'
forbid_regex "src/modules/mesh/DMesh/Display/DMMarker.cpp" 'p[[:space:]]*<[[:space:]]*attrValues\.size\(\)'
for arnold_translator in \
  "src/modules/volume/VDB/Translator/DVDBTranslatorAi.cpp" \
  "src/modules/curve/DCurve/Translator/DCurveTranslatorAi.cpp" \
  "src/modules/mesh/DMesh/Translator/DMeshTranslatorAi.cpp" \
  "src/modules/point/DPoint/Translator/DPointTranslatorAi.cpp" \
  "src/modules/point/DPoint/Translator/DPInstancerTranslatorAi.cpp" \
  "src/modules/point/DPoint/Translator/DPLightTranslatorAi.cpp"; do
  forbid_regex "$arnold_translator" '\[test extension\]'
  require_regex "$arnold_translator" 'AiMsgDebug\("\[dommyTool\] Exporting'
done

require_regex "src/modules/curve/DCurve/IO/DWriteDCurveToAi.cpp" 'CurveLayout::create'
require_regex "src/core/common/utility/DPRayUtility.cpp" 'validateCollections'
require_regex "src/core/common/utility/DPRayUtility.cpp" 'index[[:space:]]*<[[:space:]]*points_\.size\(\)'
require_regex "src/modules/volume/VDB/Data/DOpenVDBData.cpp" 'mGrids\.swap\(decoded\)'
require_regex "src/modules/volume/VDB/Data/DOpenVDBData.cpp" 'mGrids\.swap\(updated\)'
require_regex "src/modules/volume/VDB/Data/DOpenVDBData.cpp" 'dommy::memory::checkedAdd'
require_regex "src/core/common/utility/DGenerationLimits.h" 'kMaxGeneratedPointCount'
require_regex "src/core/common/utility/DGenerationLimits.h" 'kMaxGeneratedStringCount'
require_regex "src/core/common/utility/DGenerationLimits.h" 'kMaxGeneratedStringByteCount'
require_regex "src/core/common/utility/DGenerationLimits.h" 'checkedGridBudget'
require_regex "src/core/common/utility/DRandomValueGeneration.h" 'generateIntValues'
require_regex "src/modules/generators/icosahedron/DIcosahedronTopology.h" 'buildHistoricalIcosahedron'
require_regex "src/modules/generators/icosahedron/icosahedronNode.cpp" 'dommy::geometry::buildHistoricalIcosahedron'
require_regex "src/core/DDataUtility/Generate/DPointCircleEmit.cpp" 'dommy::generation::checkedCount'
require_regex "src/core/DDataUtility/Generate/DPointSurfaceEmit.cpp" 'dommy::generation::checkedProduct'
require_regex "src/core/DDataUtility/Generate/DPointSurfaceEmit.cpp" 'startU[[:space:]]*\+[[:space:]]*stepLengthU'
require_regex "src/core/DDataUtility/Generate/DPointSurfaceEmit.cpp" 'totalSamplingU[[:space:]]*-[[:space:]]*1U'
require_regex "src/core/DDataUtility/Generate/DPointPlaneEmit.cpp" 'dommy::generation::checkedProduct'
require_regex "src/core/DDataUtility/Generate/DPointPlaneEmit.cpp" 'm_distriby\(m_eng\)'
require_regex "src/core/DDataUtility/Generate/DPScatter.cpp" 'dommy::generation::checkedProduct'
require_regex "src/core/DDataUtility/Generate/DPScatter.cpp" 'dommy::random::generateIntValues'
require_regex "src/core/DDataUtility/Generate/DPScatter.cpp" 'totalArea[[:space:]]*<=[[:space:]]*0\.0'
require_regex "src/core/DDataUtility/Generate/DPScatter.cpp" 'm_grid_sizeX[[:space:]]*=[[:space:]]*static_cast<float>'
require_regex "src/core/DDataUtility/Generate/DPScatter.cpp" \
  'publishPluginDataObject'
forbid_regex "src/core/DDataUtility/Generate/DPScatter.cpp" \
  'outputData\.set\(dpData\)'
forbid_regex "src/core/DDataUtility/Generate/DPScatter.h" 'DPointPlaneEmit\.h'
require_regex "src/core/DDataUtility/Generate/DPointVolumeEmit.cpp" 'dommy::generation::checkedCount'
require_regex "src/core/DDataUtility/Generate/DPointVolumeEmit.cpp" 'dommy::generation::checkedProduct'
require_regex "src/core/DDataUtility/Generate/DPointVolumeEmit.cpp" 'std::mt19937'
forbid_regex "src/core/DDataUtility/Generate/DPointVolumeEmit.cpp" 'MDoubleArrayPtr|boost::mt19937|boost::uniform_real'
for generator in DPointMeshEmit DPointCurveEmit DPAnimationEmit; do
  forbid_regex "src/core/DDataUtility/Generate/$generator.cpp" 'DPointDataHeaders\.h|DMayaHeaders\.h'
done
for generator in \
  "src/core/DDataUtility/Data/DPIntArrayGen" \
  "src/modules/point/DPoint/generate/data/DPDoubleArrayGen" \
  "src/modules/point/DPoint/generate/data/DPVectorArrayGen"; do
  forbid_regex "$generator.cpp" 'DPointDataHeaders\.h|DMayaHeaders\.h|DNoiseOp\.h|boost/'
  require_regex "$generator.h" 'class[[:space:]]+[[:alnum:]_]+[[:space:]]+final'
  require_regex "$generator.h" 'return[[:space:]]+kSerial'
  require_regex "$generator.cpp" 'dommy::generation::checkedCount'
  require_regex "$generator.cpp" 'DOMMY_COMPUTE_CATCH'
done
forbid_regex "src/core/DDataUtility/Generate/DPointCurveEmit.cpp" 'boost::(mt19937|uniform_real)'
forbid_regex "src/core/Data/DCurveData.h" 'boost/shared_ptr\.hpp'
require_regex "src/core/DDataUtility/Generate/DPointMeshEmit.cpp" 'requireAdditionalCapacity'
require_regex "src/core/DDataUtility/Generate/DPointMeshEmit.cpp" 'DOMMY_COMPUTE_CATCH'
require_regex "src/core/DDataUtility/Generate/DPointCurveEmit.cpp" 'requireWorkBudget'
require_regex "src/core/DDataUtility/Generate/DPointCurveEmit.cpp" 'sampleValueRamp\('
require_regex "src/core/DDataUtility/Generate/DPointCurveEmit.cpp" 'DOMMY_COMPUTE_CATCH'
require_regex "src/core/DDataUtility/Generate/DPAnimationEmit.cpp" 'dommy::generation::checkedProduct'
require_regex "src/core/DDataUtility/Generate/DPAnimationEmit.cpp" 'finishOutput'
require_regex "src/core/DDataUtility/Generate/DPAnimationEmit.cpp" 'DOMMY_COMPUTE_CATCH'
forbid_regex "src/core/DDataUtility/Generate/DPAnimationEmit.cpp" 'vector<[[:space:]]*std::vector<MMatrix>'
require_regex "src/core/common/utility/DTimeSampling.h" 'std::min\(values\.length\(\),[[:space:]]*source\.length\(\)\)'
require_regex "src/core/common/utility/DTimeSampling.h" 'computeSampleTime'
require_regex "src/core/common/utility/DMayaArrayOutput.h" 'DOutputDataMayaIntArray'
require_regex "src/modules/volume/VDB/Common/VDBPointUtil.h" 'kMaxGeneratedPointCount'
require_regex "src/modules/volume/VDB/Common/VDBPointUtil.h" 'std::length_error'
require_regex "src/core/common/utility/DFluidGridPlan.h" 'buildGridPlan'
require_regex "src/core/common/utility/DFluidGridPlan.h" 'candidate\.scalar\.origin\[axis\][[:space:]]*='
require_regex "src/core/common/utility/DFluidGridPlan.h" 'candidate\.velocity\[component\]\.dimensions\[component\][[:space:]]*\+='
require_regex "src/modules/volume/VDB/Generate/DOpenVDBFromFluid.cpp" 'velocityGridSizes'
require_regex "src/modules/volume/VDB/Generate/DOpenVDBFromFluid.cpp" 'inclusiveMatrix'
require_regex "src/modules/volume/VDB/Generate/DOpenVDBFromFluid.cpp" 'isFiniteMatrix'
require_regex "src/modules/volume/VDB/Generate/DOpenVDBFromFluid.cpp" 'gridPlan\.velocity\[component\]'
forbid_regex "src/modules/volume/VDB/Generate/DOpenVDBFromFluid.cpp" 'openvdb::Coord\(xRes[[:space:]]*-[[:space:]]*1'
require_regex "src/core/DDataUtility/Generate/DPointFromFluid.cpp" 'dommy::fluid::buildGridPlan'
require_regex "src/core/DDataUtility/Generate/DPointFromFluid.cpp" 'kMaxGeneratedPointCount'
require_regex "src/core/DDataUtility/Generate/DPointFromFluid.cpp" 'fluid\.gridSize'
require_regex "src/core/DDataUtility/Generate/DPointFromFluid.cpp" 'dommy::generation::checkedCount'
require_regex "src/core/DDataUtility/Generate/DPointFromFluid.cpp" 'inclusiveMatrix'
require_regex "src/core/DDataUtility/Generate/DPointFromFluid.cpp" 'dommy::maya::isFloatRepresentablePoint'
require_regex "src/core/DDataUtility/Generate/DPointFromFluid.cpp" 'adoptPositions\(std::move\(pointArray\)\)'
require_regex "src/core/DDataUtility/Generate/DPointFromFluid.cpp" 'DOMMY_COMPUTE_CATCH'
forbid_regex "src/core/DDataUtility/Generate/DPointFromFluid.cpp" 'xRes[[:space:]]*\*[[:space:]]*yRes[[:space:]]*\*[[:space:]]*zRes'
forbid_regex "src/core/DDataUtility/Generate/DPointFromFluid.cpp" 'getFluidTransform'
forbid_regex "src/core/common/object/DNodeOperation.cpp" 'getFluidTransform'
forbid_regex "src/core/common/object/DNodeOperation.h" 'getFluidTransform'
require_regex "src/core/common/utility/DNumericValidation.h" 'checkedFloatCast'
require_regex "src/core/DDataUtility/Particle/emitter/DPFluidEmitter.cpp" 'fluid\.object\(\)[[:space:]]*!=[[:space:]]*MObject::kNullObj'
require_regex "src/core/DDataUtility/Particle/emitter/DPFluidEmitter.cpp" 'dommy::fluid::buildGridPlan'
require_regex "src/core/DDataUtility/Particle/emitter/DPFluidEmitter.cpp" 'kMaxGeneratedPointCount'
require_regex "src/core/DDataUtility/Particle/emitter/DPFluidEmitter.cpp" 'kMaxWorkItemCount'
require_regex "src/core/DDataUtility/Particle/emitter/DPFluidEmitter.cpp" 'dommy::generation::checkedProduct'
require_regex "src/core/DDataUtility/Particle/emitter/DPFluidEmitter.cpp" 'dommy::generation::checkedCount'
require_regex "src/core/DDataUtility/Particle/emitter/DPFluidEmitter.cpp" 'attribute->attrSize\(\)[[:space:]]*!=[[:space:]]*expectedCount'
require_regex "src/core/DDataUtility/Particle/emitter/DPFluidEmitter.cpp" 'getPositionPtrConst'
require_regex "src/core/DDataUtility/Particle/emitter/DPFluidEmitter.cpp" 'voxelCenterPosition'
require_regex "src/core/DDataUtility/Particle/emitter/DPFluidEmitter.cpp" 'fluid\.updateGrid\(\)'
require_regex "src/core/DDataUtility/Particle/emitter/DPFluidEmitter.cpp" 'DOMMY_COMPUTE_CATCH'
require_regex "src/core/DDataUtility/Particle/emitter/DPFluidEmitter.h" 'class[[:space:]]+DPFluidEmitter[[:space:]]+final'
require_regex "src/core/DDataUtility/Particle/emitter/DPFluidEmitter.h" 'return[[:space:]]+kSerial'
forbid_regex "src/core/DDataUtility/Particle/emitter/DPFluidEmitter.cpp" 'res\[0\][[:space:]]*\*[[:space:]]*res\[1\][[:space:]]*\*[[:space:]]*res\[2\]'
forbid_regex "src/core/DDataUtility/Particle/emitter/DPFluidEmitter.cpp" 'getPositionConst\(\)'
forbid_regex "src/core/DDataUtility/Particle/emitter/DPFluidEmitter.cpp" 'fArray\[fluid\.index'
for field in DPFluidVectorField DPFluidNoiseField; do
  require_regex "src/core/DDataUtility/Particle/field/$field.h" 'class[[:space:]]+'"$field"'[[:space:]]+final'
  require_regex "src/core/DDataUtility/Particle/field/$field.h" 'return[[:space:]]+kSerial'
  require_regex "src/core/DDataUtility/Particle/field/$field.cpp" 'outputArrayValue\(mInputData'
  require_regex "src/core/DDataUtility/Particle/field/$field.cpp" 'attributeAffects\('
  require_regex "src/core/DDataUtility/Particle/field/$field.cpp" 'dommy::maya::buildCheckedArray'
  require_regex "src/core/DDataUtility/Particle/field/$field.cpp" 'kMaxGeneratedPointCount'
  forbid_regex "src/core/DDataUtility/Particle/field/$field.cpp" 'MVectorArray[[:space:]]+(forceArray|attributeValues)\(receptorCount'
  require_regex "src/core/DDataUtility/Particle/field/$field.cpp" 'DOMMY_COMPUTE_CATCH'
  forbid_regex "src/core/DDataUtility/Particle/field/$field.cpp" 'DPAttributeCommon\.h|DMayaHeaders\.h|MFnNumericAttribute'
done
require_regex "src/core/DDataUtility/Particle/field/DPFluidVectorField.cpp" 'dommy::attribute::convertVector3Attribute'
require_regex "src/core/DDataUtility/Particle/field/DPFluidVectorField.cpp" 'computeVectorFieldForceComponent'
require_regex "src/core/DDataUtility/Particle/field/DPFluidNoiseField.cpp" 'getCheckedDScalarArrayFromAttribute'
require_regex "src/core/DDataUtility/Particle/field/DPFluidNoiseField.cpp" 'computeNoiseFieldForceComponent'
require_regex "src/modules/point/DPoint/Common/DPAttributeAccess.h" 'attribute->attrSize\(\)[[:space:]]*!=[[:space:]]*expectedCount'
require_regex "src/core/DDataUtility/Particle/field/DPGoalField.h" 'class[[:space:]]+DPGoalField[[:space:]]+final'
require_regex "src/core/DDataUtility/Particle/field/DPGoalField.h" 'return[[:space:]]+kSerial'
require_regex "src/core/DDataUtility/Particle/field/DPGoalField.cpp" 'outputArrayValue\(mInputData'
require_regex "src/core/DDataUtility/Particle/field/DPGoalField.cpp" 'attributeAffects\('
require_regex "src/core/DDataUtility/Particle/field/DPGoalField.cpp" 'dommy::maya::buildCheckedArray'
require_regex "src/core/DDataUtility/Particle/field/DPGoalField.cpp" 'kMaxGeneratedPointCount'
forbid_regex "src/core/DDataUtility/Particle/field/DPGoalField.cpp" 'MVectorArray[[:space:]]+forceArray\(receptorCount'
require_regex "src/core/DDataUtility/Particle/field/DPGoalField.cpp" 'computeGoalDistanceScale'
require_regex "src/core/DDataUtility/Particle/field/DPGoalField.cpp" 'computeGoalFieldForceComponent'
require_regex "src/core/DDataUtility/Particle/field/DPGoalField.cpp" 'DOMMY_COMPUTE_CATCH'
forbid_regex "src/core/DDataUtility/Particle/field/DPGoalField.cpp" 'apply(No)?MaxDist|ownerPosition|getWorldPosition|DMayaHeaders\.h|DPointDataHeaders\.h|glVertex|beginGL'
require_regex "src/core/DDataUtility/Particle/DPEmitter.h" 'class[[:space:]]+DPEmitter[[:space:]]+final'
require_regex "src/core/DDataUtility/Particle/DPEmitter.h" 'return[[:space:]]+kSerial'
require_regex "src/core/DDataUtility/Particle/DPEmitter.cpp" 'finishOutput'
require_regex "src/core/DDataUtility/Particle/DPEmitter.cpp" 'isWithinInclusiveBounds'
require_regex "src/core/DDataUtility/Particle/DPEmitter.cpp" 'kMaxGeneratedPointCount'
require_regex "src/core/DDataUtility/Particle/DPEmitter.cpp" 'getCheckedMVectorArrayFromAttribute'
require_regex "src/core/DDataUtility/Particle/DPEmitter.cpp" 'getCheckedMIntArrayFromAttribute'
require_regex "src/core/DDataUtility/Particle/DPEmitter.cpp" 'getPositionPtrConst'
require_regex "src/core/DDataUtility/Particle/DPEmitter.cpp" 'DOMMY_COMPUTE_CATCH'
require_regex "src/core/DDataUtility/Particle/DPEmitter.cpp" 'dommy::maya::buildCheckedArray'
forbid_regex "src/core/DDataUtility/Particle/DPEmitter.cpp" 'MVectorArray[[:space:]]+velocities[[:space:]]*\('
forbid_regex "src/core/DDataUtility/Particle/DPEmitter.cpp" 'VERBOSE|std::cout|(^|[^[:alnum:]_])cout[[:space:]]*<<|DMayaHeaders\.h|DPointDataHeaders\.h|getPositionConst\(\)'
require_regex "src/modules/particles/particle/emitter/DMeshVolumeEmitter.h" 'class[[:space:]]+DMeshVolumeEmitter[[:space:]]+final'
require_regex "src/modules/particles/particle/emitter/DMeshVolumeEmitter.h" 'return[[:space:]]+kSerial'
require_regex "src/modules/particles/particle/emitter/DMeshVolumeEmitter.cpp" 'finishOutput'
for mesh_emitter in DMeshVolumeEmitter DPolygonEmitter DRadialGeometryEmitter; do
  require_regex "src/modules/particles/particle/emitter/${mesh_emitter}.cpp" 'numVertices\(&status\)'
  require_regex "src/modules/particles/particle/emitter/${mesh_emitter}.cpp" 'length\(\) != (checkedVertexCount|vertexCount)'
done
for mesh_emitter in DPolygonEmitter DRadialGeometryEmitter; do
  require_regex "src/modules/particles/particle/emitter/${mesh_emitter}.cpp" 'blendMeshSample'
  require_regex "src/modules/particles/particle/emitter/${mesh_emitter}.cpp" 'quadMeshSample'
done
require_regex "src/modules/particles/particle/emitter/DMeshVolumeEmitter.cpp" 'checkedSum'
require_regex "src/modules/particles/particle/emitter/DMeshVolumeEmitter.cpp" 'checkedProduct'
require_regex "src/modules/particles/particle/emitter/DMeshVolumeEmitter.cpp" 'MSpace::kObject'
require_regex "src/modules/particles/particle/emitter/DMeshVolumeEmitter.cpp" 'isFiniteMatrix'
require_regex "src/modules/particles/particle/emitter/DMeshVolumeEmitter.cpp" 'DOMMY_COMPUTE_CATCH'
require_regex "src/modules/particles/particle/emitter/DMeshVolumeEmitter.cpp" 'dommy::maya::buildCheckedArray'
require_regex "src/modules/particles/particle/emitter/DMeshVolumeEmitter.cpp" 'emittedPositions.length\(\) != gridSampleCount'
require_regex "src/modules/particles/particle/emitter/DMeshVolumeEmitter.cpp" 'emittedPositions.length\(\) != emittedCount'
forbid_regex "src/modules/particles/particle/emitter/DMeshVolumeEmitter.cpp" 'MVectorArray[[:space:]]+emittedVelocities[[:space:]]*\('
require_regex "src/modules/particles/particle/emitter/DMeshVolumeEmitter.cpp" 'attributeAffects\('
forbid_regex "src/modules/particles/particle/emitter/DMeshVolumeEmitter.cpp" 'std::cout|(^|[^[:alnum:]_])cout[[:space:]]*<<|MFnDependencyNode|connectedTo\(|getClosestPointAndNormal\([^;]*MSpace::kWorld|for[[:space:]]*\([^;]*float'
require_regex "src/modules/particles/particle/emitter/DPolygonEmitter.h" 'class[[:space:]]+DPolygonEmitter[[:space:]]+final'
require_regex "src/modules/particles/particle/emitter/DPolygonEmitter.h" 'return[[:space:]]+kSerial'
require_regex "src/modules/particles/particle/emitter/DPolygonEmitter.cpp" 'finishOutput'
require_regex "src/modules/particles/particle/emitter/DPolygonEmitter.cpp" 'checkedProduct'
require_regex "src/modules/particles/particle/emitter/DPolygonEmitter.cpp" 'checkedSum'
require_regex "src/modules/particles/particle/emitter/DPolygonEmitter.cpp" 'isValidVertexIndex'
require_regex "src/modules/particles/particle/emitter/DPolygonEmitter.cpp" 'outputIndex[[:space:]]*>=[[:space:]]*positions\.length\(\)'
require_regex "src/modules/particles/particle/emitter/DPolygonEmitter.cpp" 'MSpace::kObject'
require_regex "src/modules/particles/particle/emitter/DPolygonEmitter.cpp" 'DOMMY_COMPUTE_CATCH'
require_regex "src/modules/particles/particle/emitter/DPolygonEmitter.cpp" 'attributeAffects\('
forbid_regex "src/modules/particles/particle/emitter/DPolygonEmitter.cpp" 'std::cout|(^|[^[:alnum:]_])cout[[:space:]]*<<|MFnDependencyNode|MFnMatrixData|faceSamplingVal|edgeSamplingVal|emitCountPerPoint|useRotation'
require_regex "src/modules/particles/particle/emitter/DRadialGeometryEmitter.h" 'class[[:space:]]+DRadialGeometryEmitter[[:space:]]+final'
require_regex "src/modules/particles/particle/emitter/DRadialGeometryEmitter.h" 'return[[:space:]]+kSerial'
require_regex "src/modules/particles/particle/emitter/DRadialGeometryEmitter.cpp" 'finishOutput'
require_regex "src/modules/particles/particle/emitter/DRadialGeometryEmitter.cpp" 'collectCurveSamples'
require_regex "src/modules/particles/particle/emitter/DRadialGeometryEmitter.cpp" 'collectSurfaceSamples'
require_regex "src/modules/particles/particle/emitter/DRadialGeometryEmitter.cpp" 'collectMeshSamples'
require_regex "src/modules/particles/particle/emitter/DRadialGeometryEmitter.cpp" 'checkedProduct'
require_regex "src/modules/particles/particle/emitter/DRadialGeometryEmitter.cpp" 'kMaxWorkItemCount'
require_regex "src/modules/particles/particle/emitter/DRadialGeometryEmitter.cpp" 'isSafePerlinCoordinate'
require_regex "src/modules/particles/particle/emitter/DRadialGeometryEmitter.cpp" 'MPoint[[:space:]]+worldPoint'
require_regex "src/modules/particles/particle/emitter/DRadialGeometryEmitter.cpp" 'MSpace::kObject'
require_regex "src/modules/particles/particle/emitter/DRadialGeometryEmitter.cpp" 'isFiniteMatrix'
require_regex "src/modules/particles/particle/emitter/DRadialGeometryEmitter.cpp" 'DOMMY_COMPUTE_CATCH'
require_regex "src/modules/particles/particle/emitter/DRadialGeometryEmitter.cpp" 'attributeAffects\('
forbid_regex "src/modules/particles/particle/emitter/DRadialGeometryEmitter.cpp" 'std::cout|(^|[^[:alnum:]_])cout[[:space:]]*<<|emitCountPerPoint|void[[:space:]]+DRadialGeometryEmitter::emit|pts\[vlist|posVec[[:space:]]*\*[[:space:]]*worldMatrix|MFnDependencyNode|MFnDagNode|MFnMatrixData'
require_regex "src/modules/particles/particle/field/DKillField.h" 'class[[:space:]]+DKillField[[:space:]]+final'
require_regex "src/modules/particles/particle/field/DKillField.h" 'return[[:space:]]+kGloballySerialize'
require_regex "src/modules/particles/particle/field/DKillField.cpp" 'outputArrayValue\(mInputData'
require_regex "src/modules/particles/particle/field/DKillField.cpp" 'dommy::maya::buildCheckedArray'
forbid_regex "src/modules/particles/particle/field/DKillField.cpp" 'M(Vector|Double)Array[[:space:]]+(forceArray|lifespans)[[:space:]]*\('
require_regex "src/core/common/object/DParticleAttributeUtils.h" 'dommy::maya::copyCheckedArray'
forbid_regex "src/core/common/object/DParticleAttributeUtils.h" 'output\[index\][[:space:]]*=[[:space:]]*source\[index\]'
require_regex "src/modules/particles/particle/field/DKillField.cpp" 'attributeAffects\('
require_regex "src/modules/particles/particle/field/DKillField.cpp" 'findNeighborPresence'
require_regex "src/modules/particles/particle/field/DKillField.cpp" 'parseIdExpression'
require_regex "src/modules/particles/particle/field/DKillField.cpp" 'kMaxWorkItemCount'
require_regex "src/modules/particles/particle/field/DKillField.cpp" 'MSpace::kObject'
require_regex "src/modules/particles/particle/field/DKillField.cpp" 'isFiniteMatrix'
require_regex "src/modules/particles/particle/field/DKillField.cpp" 'setPerParticleAttribute'
require_regex "src/modules/particles/particle/field/DKillField.cpp" 'DOMMY_COMPUTE_CATCH'
forbid_regex "src/modules/particles/particle/field/DKillField.cpp" 'applyKill|ownerPosition|getWorldPosition|MFnMatrixData|std::cout|(^|[^[:alnum:]_])cout[[:space:]]*<<|glVertex|beginGL|getClosestPointAndNormal\([^;]*MSpace::kWorld'
forbid_regex "src/modules/particles/particle/field/DKillField.h" 'void[[:space:]]+draw|applyKill|ownerPosition|getWorldPosition|DMayaDebug'
# Signed count conversion and per-mode native numeric limits are exercised by
# check_vdb_distribution_safety.py (including float-cast UBSan boundaries).
# The actual shared adapter and FromMesh/emitter paths run in vdb_mesh_safety.
require_regex "src/core/common/vdb/DVdbOp.cpp" 'preflightMeshTriangles'
require_regex "src/core/common/vdb/DVdbOp.cpp" 'convertMeshVolume'
require_regex "src/modules/dynamics/DBullet/Collision/convex_decomposition_hacd.cpp" '~DOwningCompoundShape\(\)[[:space:]]+override'
require_regex "src/modules/mesh/DMesh/Dynamics/Bullet/dbtFractureBody.cpp" 'addSingleResult\('
require_regex "src/modules/mesh/DMesh/Dynamics/Bullet/dbtFractureBody.cpp" '\)[[:space:]]+override'
require_regex "src/modules/particles/particle/field/DCurveField.cpp" 'setZeroForces'
require_regex "src/modules/particles/particle/field/DCurveField.cpp" 'buildCheckedArray<MVectorArray>'
require_regex "src/modules/particles/particle/field/DCurveField.cpp" 'readFiniteDoubleInput'
require_regex "src/modules/particles/particle/field/DCurveField.cpp" 'falloffCurve\(ratio,[[:space:]]*&status\)'
forbid_regex "src/modules/particles/particle/field/DCurveField.cpp" 'applyNoMaxDist|applyMaxDist|double[[:space:]]+param[[:space:]]*;'
require_regex "src/modules/particles/particle/field/DCurveField.cpp" 'DOMMY_COMPUTE_CATCH'
require_regex "src/modules/particles/particle/field/DCurveField.cpp" 'attributeAffects\('
require_regex "src/modules/particles/particle/field/DCurveField.cpp" 'getValueAtPosition\(position,[[:space:]]*value,[[:space:]]*&status\)'
require_regex "src/modules/particles/particle/field/DCurveField.h" 'class[[:space:]]+DCurveField[[:space:]]+final'
require_regex "src/modules/particles/particle/field/DCurveField.h" 'return[[:space:]]+kSerial'
forbid_regex "src/modules/particles/particle/field/DCurveField.cpp" 'beginGL|glVertex|getWorldPosition|ownerPosition'
require_regex "src/plugin/pluginMain.cpp" 'DOMMY_WITH_ARNOLD'
require_regex "src/plugin/pluginMain.cpp" 'DOMMY_WITH_BULLET'
require_regex "src/plugin/pluginMain.cpp" 'DOMMY_WITH_OPENVDB'
require_regex "src/plugin/pluginMain.cpp" '^[[:space:]]*openvdb::initialize\(\);'
require_regex "src/plugin/pluginMain.cpp" '^[[:space:]]*openvdb::uninitialize\(\);'
require_regex "src/plugin/registration/modules/ArnoldRegistrations.inl" 'DWriteDCurveToAi'
require_regex "src/integrations/arnold/DArnoldSession.h" 'class Session'
require_regex "src/core/common/utility/DEvaluationContext.h" 'context.getTime\(time\)'
require_regex "src/core/common/utility/DCheckedSize.h" 'checkedMultiply'
require_regex "src/modules/volume/VDB/Common/DOpenVDBUtil.cpp" 'createOpenVDBOutputData'
require_regex "src/modules/volume/VDB/Common/DOpenVDBUtil.cpp" 'getActiveVoxelWorldBounds'
require_regex "src/modules/volume/VDB/Common/DOpenVDBUtil.cpp" 'return[[:space:]]+!hasSelector'
require_regex "src/modules/volume/VDB/Common/DOpenVDBUtil.cpp" 'matchesGridName\(word,[[:space:]]*gridName\)'
forbid_regex "src/modules/volume/VDB/Common/DOpenVDBUtil.cpp" 'return[[:space:]]+boost::lexical_cast<size_t>\(word\)[[:space:]]*=='
require_regex "src/modules/volume/VDB/Edit/DVDBPotentialFlow.cpp" 'mask[[:space:]]*=[[:space:]]*resampled'
require_regex "src/modules/volume/VDB/Edit/DVDBPotentialFlow.cpp" 'aDilation[[:space:]]*=[^;]*MFnNumericData::kFloat'
require_regex "src/modules/volume/VDB/Edit/DVDBPotentialFlow.cpp" \
  'createPotentialFlowMask<openvdb::FloatGrid,[[:space:]]*openvdb::BoolGrid>'
require_regex "src/modules/volume/VDB/Convert/DOpenVDBToMesh.cpp" \
  'DMeshInternalData::createFromMayaObject'
require_regex "src/modules/volume/VDB/Convert/DOpenVDBToMesh.cpp" \
  'inputs\.adaptivity[[:space:]]*>[[:space:]]*1\.0f'
require_regex "src/modules/volume/VDB/Convert/DOpenVDBToMesh.cpp" \
  'nAttr\.setMax\(1\.0\)'
forbid_regex "src/modules/volume/VDB/Convert/DOpenVDBToMesh.cpp" \
  'new[[:space:]]+DMeshInternalData\(\*it\)'
require_regex "src/modules/volume/VDB/Edit/DOpenVDBVectorSplit.cpp" \
  'std::set<const openvdb::GridBase\*>[[:space:]]+splitSources'
require_regex "src/modules/volume/VDB/Edit/DOpenVDBVectorSplit.cpp" \
  '\+\+it\)'
require_regex "src/modules/volume/VDB/Edit/DOpenVDBVectorSplit.cpp" \
  'splitSources\.count\(source\.get\(\)\)'
forbid_regex "src/modules/volume/VDB/Edit/DOpenVDBVectorSplit.cpp" \
  '/\*[[:space:]]*\+\+it[[:space:]]*\*/'
require_regex "src/modules/volume/VDB/Edit/DOpenVDBClip.cpp" 'activeBounds\.expand\(gridBounds\)'
require_regex "src/modules/volume/VDB/Edit/DOpenVDBFracture.cpp" 'getSelectedGrids\(selectedGrids,[[:space:]]*selection'
require_regex "src/modules/volume/VDB/Edit/DOpenVDBRemoveDivergence.cpp" 'getSelectedGrids\(selected,[[:space:]]*selectGroupName,[[:space:]]*\*inputVdb,[[:space:]]*\*outputVdb\)'
require_regex "src/modules/volume/VDB/Edit/DOpenVDBRemoveDivergence.cpp" 'outputVdb->insert\(parms\.velocityGrid\)'
require_regex "src/modules/volume/VDB/Edit/DOpenVDBRemoveDivergence.cpp" 'settings\.colliderType[[:space:]]*<[[:space:]]*0[[:space:]]*\|\|[[:space:]]*settings\.colliderType[[:space:]]*>[[:space:]]*2'
require_regex "src/modules/volume/VDB/Edit/DOpenVDBRemoveDivergence.cpp" 'pressureLeafPairs\(velocityGrid\.tree\(\),[[:space:]]*gradientOfPressure->tree\(\)\)'
forbid_regex "src/modules/volume/VDB/Edit/DOpenVDBRemoveDivergence.cpp" 'mGradientOfPressureNodes\[n\]|CHECK_MSTATUS\('
require_regex "src/modules/curve/DCurve/Cmd/Abc/DPointWriter.cpp" 'dommy::memory::dataOrNull\(val\)'
require_regex "src/modules/point/DPoint/Duplicator/DPInstancer.cpp" 'dommy::memory::checkedMultiply'
require_regex "src/modules/dynamics/DBullet/File/pdbIO.cpp" 'dommy::memory::checkedMultiply'
forbid_regex "src/modules/dynamics/DBullet/Draw/drawUtils.cpp" \
  'quadric[[:space:]]*==[[:space:]]*NULL'
forbid_regex "src/modules/dynamics/DBullet/RigidBody/DBRigidBodyNode.cpp" \
  'setDependentsDirty'
for modernized_bullet_source in \
  "src/modules/dynamics/DBullet/Constraint/DBHingeConstraintNode.cpp" \
  "src/modules/dynamics/DBullet/Constraint/DBNailConstraintNode.cpp" \
  "src/modules/dynamics/DBullet/Constraint/DBSixdofConstraintNode.cpp" \
  "src/modules/dynamics/DBullet/Constraint/DBSliderConstraintNode.cpp" \
  "src/modules/dynamics/DBullet/DM/DCBullet2DSolver.cpp" \
  "src/modules/dynamics/DBullet/Draw/drawUtils.cpp" \
  "src/modules/dynamics/DBullet/RigidBody/DBRigidBodyNode.cpp"; do
  forbid_regex "$modernized_bullet_source" '(^|[^[:alnum:]_])NULL([^[:alnum:]_]|$)'
done
for solver in \
  "src/modules/dynamics/DBullet/DM/DCBullet2DSolver" \
  "src/modules/dynamics/DBullet/DM/DMBulletTest"; do
  forbid_regex "$solver.cpp" 'm_[[:alnum:]_]+[[:space:]]*=[[:space:]]*new[[:space:]]+bt'
  require_regex "$solver.cpp" 'std::unique_ptr<bt'
  require_regex "$solver.cpp" 'resources must be cleared before initialization'
  require_regex "$solver.cpp" 'normalizedVector\('
  require_regex "$solver.cpp" 'data\.inputValue\(attribute,[[:space:]]*&status\)'
  forbid_regex "$solver.cpp" 'MPlug\(thisMObject\(\),[[:space:]]*aIsPlane\)'
  require_regex "$solver.h" 'clearBullet\(\)[[:space:]]+noexcept'
  require_regex "$solver.h" 'deleteRigidBodies\(\)[[:space:]]+noexcept'
done
forbid_regex "src/modules/dynamics/DBullet/SoftBody/DBSoftBodyNode.cpp" 'DBSoftBodyNode::computeWorldMatrix'
require_regex "src/modules/mesh/mesh/applyVtxColorFromFileNode.cpp" 'readVertexColors'
require_regex "src/modules/mesh/mesh/applyVtxColorFromFileNode.cpp" 'replaceFrameToken'
require_regex "src/modules/mesh/mesh/applyVtxColorFromFileNode.cpp" \
  'checkedFloatCast'
require_regex "src/modules/mesh/mesh/applyVtxColorFromFileNode.cpp" 'setMeshDataVertexColors'
require_regex "src/core/common/utility/DMayaMeshColorOutput.h" 'createColorSetDataMesh\(name\)'
require_regex "src/core/common/utility/DMayaMeshColorOutput.h" 'clearColors\(&name\)'
require_regex "src/core/common/utility/DMayaMeshColorOutput.h" 'setColors\(colors,[[:space:]]*&name\)'
require_regex "src/core/common/utility/DMayaMeshColorOutput.h" 'assignColors\(colorIds,[[:space:]]*&name\)'
forbid_regex "src/modules/mesh/mesh/applyVtxColorFromFileNode.cpp" \
  'newMeshFn\.(setVertexColors|setCurrentColorSetName|createColorSetWithName)\('
forbid_regex "src/modules/mesh/mesh/applyVtxColorFromFileNode.cpp" \
  'outputMeshHnd\.set\(newOutMeshData\).*readVertexColors'
require_regex "src/core/common/file/DFileOperation.cpp" 'replaceLegacyFrameToken'
require_regex "src/modules/volume/VDB/Common/DOpenVDBUtil.cpp" 'replaceHashFrameToken'
require_regex "src/modules/volume/VDB/IO/DWriteOpenVDB.cpp" '!insertFrameNumber'
forbid_regex "src/modules/volume/VDB/Common/DOpenVDBUtil.cpp" 'tmpStr\.substr|int\(frame\)|find_first_of\("#"\)'
require_regex "src/core/DDataUtility/IO/ReadDPointDataNode.cpp" 'readPointData'
require_regex "src/modules/point/DPoint/Utility/DPCache.cpp" 'writeCacheAtomically'
require_regex "src/modules/point/DPoint/Utility/DPCache.cpp" 'kMaxDPointCacheFileBytes'
require_regex "src/modules/point/DPoint/Utility/DPCache.cpp" 'fileSizeWithinLimit'
require_regex "src/modules/point/DPoint/Utility/DPCache.cpp" 'kMaxGeneratedPointCount'
require_regex "src/modules/point/DPoint/Utility/DPCache.cpp" 'registerDPointCacheAttributeTypes\(\);'
forbid_regex "src/modules/point/DPoint/Utility/DPCache.cpp" 'maxPointCount[[:space:]]*=[[:space:]]*100000000'
require_regex "src/modules/point/DPoint/Utility/DPCache.cpp" 'isCacheGridWithinBudget'
require_regex "src/modules/point/DPoint/Utility/DPCache.cpp" 'std::vector<bool>[[:space:]]+pointAssigned'
require_regex "src/modules/point/DPoint/Utility/DPCache.cpp" 'DPCache is missing one or more point indices'
forbid_regex "src/modules/point/DPoint/Utility/DPCache.cpp" 'const bool VERBOSE'
require_regex "src/modules/volume/VDB/Data/DOpenVDBData.cpp" 'kMaxVdbPayloadBytes'
require_regex "src/modules/volume/VDB/Data/DOpenVDBData.cpp" 'hasValidGridBudget'
require_regex "src/modules/volume/VDB/Data/DOpenVDBData.cpp" 'DOpenVDBData::write payload exceeds the safety limit'
require_regex "src/core/common/file/DAtomicFile.h" 'O_CREAT[[:space:]]*\|[[:space:]]*O_EXCL'
require_regex "src/core/common/file/DAtomicFile.h" 'CREATE_NEW'
require_regex "src/core/common/file/DAtomicFile.h" 'isRegularFile\(temporary_\)'
require_regex "src/core/common/file/DAtomicFile.h" 'TrustedPath is an explicit caller precondition'
require_regex "src/core/common/file/DAtomicFile.h" 'ScopedReservedPathCleanup cleanup\(path\)'
require_regex "tests/run_clang_static_analysis.sh" '-Xanalyzer -analyzer-werror'
require_regex "src/core/common/file/DHandleOutput.h" '::fdopen\(descriptor'
require_regex "src/core/common/file/DHandleOutput.h" '::renameat\(stage_'
require_regex "src/core/common/file/DHandleOutput.h" 'FileRenameInfo'
require_regex "src/core/common/file/DHandleOutput.h" '::acl_set_fd_np\(stage_'
forbid_regex "src/core/common/file/DHandleOutput.h" '::fopen\(|std::ofstream|DeleteFileA'
require_regex "src/modules/volume/VDB/IO/DWriteOpenVDB.cpp" 'PendingOutputFile[[:space:]]+pending'
require_regex "src/modules/volume/VDB/IO/DWriteOpenVDB.cpp" 'fileSizeWithinLimit'
require_regex "src/modules/volume/VDB/IO/DWriteOpenVDB.cpp" 'pending\.commit\(\)'
require_regex "src/modules/volume/VDB/IO/DReadOpenVDB.cpp" 'fileSizeWithinLimit'
require_regex "src/modules/mesh/DMesh/common/DMeshUtil.cpp" 'mesh\.hasValidMeshData\(\)'
require_regex "src/modules/mesh/DMesh/common/DMeshUtil.cpp" 'status[[:space:]]*=[[:space:]]*meshFn\.assignUVs'
require_regex "src/modules/mesh/DMesh/Convert/DMToMesh.cpp" 'getValidInputDM\(inDM, data, input\)'
forbid_regex "src/modules/mesh/DMesh/Convert/DMToMesh.cpp" 'getInputDM\(inDM, data\)'
require_regex "src/modules/mesh/DMesh/Convert/DMToMesh.cpp" 'getCheckedTotalConnect'
require_regex "src/modules/mesh/DMesh/Convert/DMToMesh.cpp" 'generateMMeshFromDmeshWithUV'
forbid_regex "src/modules/mesh/DMesh/Convert/DMToMesh.cpp" 'MFnMeshData\(\)\.create\(\)'
require_regex "src/modules/generators/Generate/LeafGen/LeafGen.cpp" 'stat[[:space:]]*=[[:space:]]*newMeshFn\.assignUVs'
require_regex "src/integrations/arnold/DArnoldArrayBudget.h" 'checkedArrayLayout'
require_regex "src/integrations/arnold/DArnoldArrayBudget.h" 'checkedAdjustedAccumulate'
for arnold_source in \
  "src/modules/mesh/DMesh/Translator/DMeshTranslatorAi.cpp" \
  "src/modules/curve/DCurve/Translator/DCurveTranslatorAi.cpp" \
  "src/modules/point/DPoint/Translator/DPointTranslatorAi.cpp" \
  "src/modules/point/DPoint/Translator/DPInstancerTranslatorAi.cpp" \
  "src/modules/curve/DCurve/IO/DWriteDCurveToAi.cpp" \
  "src/core/DDataUtility/IO/DPWriteToAi.cpp"; do
  require_regex "$arnold_source" 'checkedArrayLayout'
done
require_regex "src/modules/mesh/DMesh/Translator/DMeshTranslatorAi.cpp" 'getCheckedTotalVertex'
require_regex "src/modules/mesh/DMesh/Translator/DMeshTranslatorAi.cpp" 'getCheckedTotalConnect'
require_regex "src/modules/curve/DCurve/Translator/DCurveTranslatorAi.cpp" 'makeRenderableCurvePlan'
require_regex "src/modules/curve/DCurve/Translator/DCurveTranslatorAi.cpp" 'CurveLayout::create'
require_regex "src/modules/curve/DCurve/Translator/DCurveTranslatorAi.cpp" 'varyingSourceIndex'
require_regex "src/modules/curve/DCurve/Translator/DCurveTranslatorAi.cpp" 'remapInPlace'
for motion_source in \
  "src/modules/point/DPoint/Translator/DPointTranslatorAi.cpp" \
  "src/modules/curve/DCurve/Translator/DCurveTranslatorAi.cpp" \
  "src/modules/mesh/DMesh/Translator/DMeshTranslatorAi.cpp"; do
  require_regex "$motion_source" 'setCheckedVectorMotionKey'
  forbid_regex "$motion_source" 'AiArraySetKey'
done
require_regex "src/modules/volume/VDB/Translator/DVDBTranslatorAi.cpp" 'internalVar -userTmpDir'
require_regex "src/modules/volume/VDB/Translator/DVDBTranslatorAi.cpp" 'createTemporarySiblingFileWithSuffix'
require_regex "src/modules/volume/VDB/Translator/DVDBTranslatorAi.cpp" 'fileSizeWithinLimit'
forbid_regex "src/modules/volume/VDB/Translator/DVDBTranslatorAi.cpp" '"/tmp/"|"cached_"'
require_regex "src/modules/point/DPoint/Duplicator/DPInstancer.cpp" 'kMaxAssTocFileBytes'
require_regex "src/modules/point/DPoint/Duplicator/DPInstancer.cpp" 'kMaxAssTocLineCount'
require_regex "src/modules/point/DPoint/Duplicator/DPInstancer.cpp" 'fileSizeWithinLimit'
require_regex "src/modules/point/DPoint/Duplicator/DPInstancer.cpp" 'readBoundedLine'
require_regex "src/core/common/file/DLegacyTextData.h" 'readBoundedLine'
forbid_regex "src/modules/point/DPoint/Duplicator/DPInstancer.cpp" 'std::getline'
forbid_regex "src/core/math/DBounds.h" 'm_sum|double center\('
require_regex "src/core/common/object/DBBoxOp.cpp" 'PointMeanAccumulator'
require_regex "src/core/common/utility/DPointTransform.h" 'checkedProductSum'
forbid_regex "src/core/common/utility/DPointTransform.h" 'long double value = 0\.0L'
require_regex "src/core/DDataUtility/Data/DPData.cpp" 'CalBBoxFromPointArray\(pointArray, bounds\);'
require_regex "CMakeLists.txt" 'option\(DOMMY_ENABLE_FASTNOISE_NODES'
require_regex "CMakeLists.txt" 'option\(DOMMY_ENABLE_FREETYPE_TEXT_NODE'
require_regex "CMakeLists.txt" 'option\(DOMMY_ENABLE_EXPERIMENTAL_STRETCH_NODE'
require_regex "CMakeLists.txt" \
  'option\(DOMMY_ENABLE_EXPERIMENTAL_ARRAY_TO_COMPOUND_NODE'
require_regex "src/plugin/registration/modules/CoreRegistrations.inl" 'DOMMY_WITH_EXPERIMENTAL_STRETCH_NODE'
require_regex "src/plugin/PluginFeatures.h" \
  '^#define DOMMY_WITH_EXPERIMENTAL_ARRAY_TO_COMPOUND_NODE 0$'
require_regex "src/plugin/registration/modules/PointRegistrations.inl" \
  '#if DOMMY_WITH_EXPERIMENTAL_ARRAY_TO_COMPOUND_NODE'
for independent_geometry_source in \
  "src/modules/point/DPoint/Utility/DPAdjustSpace.cpp" \
  "src/modules/point/DPoint/mesh/DPToMesh.cpp" \
  "src/modules/point/DPoint/mesh/DPVoronoi.cpp" \
  "src/modules/curve/DCurve/Utility/DCPartition.cpp" \
  "src/modules/mesh/DMesh/Utility/DMBoolean.cpp" \
  "src/modules/mesh/DMesh/Utility/DMDebrisGen.cpp" \
  "src/modules/mesh/DMesh/Utility/DMRemesh.cpp" \
  "src/modules/mesh/DMesh/Utility/DMSlicer.cpp"; do
  forbid_regex "$independent_geometry_source" 'CGAL|DOMMY_WITH_CGAL|DPolyCGALCommon'
done
forbid_regex "CMakeLists.txt" 'DOMMY_ENABLE_CGAL_NODES|DOMMY_WITH_CGAL'
forbid_regex "cmake/DommyFullSourceCheck.cmake" 'CGAL_LOCATION|DOMMY_ENABLE_CGAL_NODES'
forbid_regex "config/active_cpp_sources.txt" \
  'CGAL|GMP|MPFR'
forbid_regex "THIRD_PARTY_NOTICES.md" 'CGAL|GMP|MPFR'
for cleaned_source in \
  "src/core/common/object/DPointOp.h" \
  "src/modules/point/DPoint/Utility/DPCache.cpp" \
  "src/modules/point/DPoint/math/DPMathUtility.h" \
  "src/modules/curve/DCurve/Generate/DCurveFromSurface.cpp" \
  "src/modules/mesh/DMesh/Utility/DMEdgeDisplace.cpp" \
  "src/modules/curve/DCurve/Utility/DCWeightMap.cpp" \
  "src/modules/generators/Generate/Human/DHEyeGen.cpp"; do
  forbid_regex "$cleaned_source" '^[[:space:]]*/\*[[:space:]]*$'
done
forbid_regex "src/modules/curve/DCurve/Utility/DCWeightMap.cpp" \
  'tbb::parallel_for|weightLsit'
require_regex "src/plugin/registration/modules/PointRegistrations.inl" \
  'registerNode\( "DPAdjustSpace"'
require_regex "src/plugin/registration/modules/PointRegistrations.inl" \
  'registerNode\( "DPVoronoi"'
require_regex "src/plugin/registration/modules/MeshRegistrations.inl" \
  'registerNode\("DMRemesh"'
require_regex "src/plugin/registration/modules/CurveRegistrations.inl" \
  'registerNode\("DCPartition"'
require_regex "src/plugin/PluginFeatures.h" '^#define DOMMY_WITH_FASTNOISE 0$'
require_regex "src/plugin/PluginFeatures.h" '^#define DOMMY_WITH_FREETYPE_TEXT 0$'
require_regex "src/core/common/object/DNoiseOp.h" '#if DOMMY_WITH_FASTNOISE'
for fastnoise_surface in \
  "src/modules/point/DPoint/DPointHeaders.h" \
  "src/modules/mesh/DMesh/DMeshHeaders.h" \
  "src/plugin/registration/modules/PointRegistrations.inl" \
  "src/plugin/registration/modules/MeshRegistrations.inl"; do
  require_regex "$fastnoise_surface" '#if DOMMY_WITH_FASTNOISE'
done
require_regex "src/modules/curve/DCurve/DCurveHeaders.h" '#if DOMMY_WITH_FREETYPE_TEXT'
require_regex "src/plugin/registration/modules/CurveRegistrations.inl" '#if DOMMY_WITH_FREETYPE_TEXT'
require_regex "cmake/DommyFullSourceCheck.cmake" 'DPFastNoise\|DMFastNoise'
require_regex "cmake/DommyFullSourceCheck.cmake" '/DCText\\\\\.cpp\$'
forbid_regex "src/core/Utility/DUtility.h" 'D_OUTPUT|operator<<\(std::string'
require_regex "CMakeLists.txt" 'tests/cpp/utility_vector_tests\.cpp'
require_regex "tests/run_cpp_tests.sh" 'tests/cpp/utility_vector_tests\.cpp'
require_regex "tests/cpp/math_memory_tests.cpp" '^[[:space:]]+runUtilityVectorTests\(\);'
require_regex "CMakeLists.txt" 'tests/cpp/atomic_output_tests\.cpp'
require_regex "tests/run_cpp_tests.sh" 'tests/cpp/atomic_output_tests\.cpp'
require_regex "tests/cpp/math_memory_tests.cpp" '^[[:space:]]+runAtomicOutputTests\(\);'
require_regex "src/core/Utility/3d/Math/DVec.h" 'removeParallelComponent'
forbid_regex "src/core/Utility/3d/Math/DVec.h" 'DReal x = norm2\(B\)'
forbid_regex "src/modules/point/DPoint/mesh/DPVoronoi.cpp" 'TETLIBRARY|#include <fstream>'
require_regex "src/core/DataModifier/ShapeSequencer/DShapeSequencer.cpp" 'std::isfinite\(sequenceVal\)'
forbid_regex "src/core/DataModifier/ShapeSequencer/DShapeSequencer.cpp" 'inputObjectArray\[outIndex\]'
require_regex "src/core/math/DRandomGenerator.h" 'class[[:space:]]+RandomGenerator'
require_regex "src/core/math/DRandomGenerator.h" 'std::mt19937'
require_regex "src/modules/curve/DCurve/Shape/DCurveShape.h" 'std::unique_ptr<DCurveData>'
require_regex "src/modules/curve/DCurve/Utility/DCWeightMap.cpp" 'std::isfinite\(affectDist\)'
require_regex "src/modules/curve/DCurve/Utility/DCClumping.cpp" 'idxList.size\(\)[[:space:]]*<[[:space:]]*curveCount'
require_regex "src/modules/curve/DCurve/Generate/DCurveFromSurface.cpp" 'checkedSurfaceSamplingPlan'
require_regex "src/core/common/utility/DCurveSampling.h" 'kMaxGeneratedPointCount'
require_regex "src/modules/curve/DCurve/Utility/DCWeightMap.cpp" 'const bool repeatWeight = handle.asBool\(\)'
require_regex "src/modules/mesh/DMesh/Dynamics/Bullet/DBFractureDynamicsWorld.cpp" 'destroyRigidBody'
require_regex "src/modules/point/DPoint/math/other/DPRemap.cpp" 'dommy::math::remapValues'
require_regex "src/modules/point/DPoint/math/DUnaryMath.h" 'normalizeRange'
require_regex "src/modules/point/DPoint/math/other/DPRemap.cpp" 'readRemapSettings'
forbid_regex "src/modules/point/DPoint/math/other/DPRemap.h" 'm_(isKeepRange|min[123]D|max[123]D)'
require_regex "src/modules/point/DPoint/math/other/DPFitRange.cpp" 'dommy::math::FitOperation'
require_regex "src/modules/point/DPoint/math/DUnaryMath.h" 'fitFiniteValue'
forbid_regex "src/modules/point/DPoint/math/other/DPFitRange.cpp" 'tbb::parallel_for|DGetGlobalProcessNum'
require_regex "src/core/DDataUtility/Attribute/DPAttributeCreate.cpp" 'createPluginData'
require_regex "src/core/DDataUtility/Attribute/DPAttributeCreate.cpp" 'stat[[:space:]]*=[[:space:]]*arrayHandle\.jumpToArrayElement'
require_regex "src/core/common/utility/DSignalMath.h" 'sampleTime'
for signal_source in \
  "src/modules/point/DPoint/animation/DPSignal.cpp" \
  "src/modules/point/DPoint/animation/DPSignal3D.cpp"; do
  require_regex "$signal_source" 'dommy::signal::sampleTime'
  forbid_regex "$signal_source" 'timeRampSize'
done
require_regex "src/core/common/utility/DAhoGenerationPlan.h" 'buildAhoGenerationPlan'
require_regex "src/modules/curve/DCurve/Generate/DCAhoGen.cpp" 'buildAhoGenerationPlan'
forbid_regex "src/modules/curve/DCurve/Generate/DCAhoGen.cpp" 'DNoiseOp\.h|boost::'
require_regex "src/modules/generators/legacy/spaceDuplicater.cpp" 'checkedProduct'
forbid_regex "src/modules/generators/legacy/spaceDuplicater.h" 'colPoints|dupShapeArray'
require_regex "src/modules/curve/DCurve/Cmd/DExportAbcCmd.cpp" 'escapeMelStringLiteral'
require_regex "src/core/common/utility/DMayaStateGuard.h" 'class[[:space:]]+ScopedCurrentTimeRestore[[:space:]]+final'
require_regex "src/core/common/utility/DMayaStateGuard.h" 'class[[:space:]]+ScopedComputation[[:space:]]+final'
require_regex "src/modules/curve/DCurve/Cmd/DExportAbcCmd.cpp" 'ScopedCurrentTimeRestore[[:space:]]+timeRestore'
require_regex "src/modules/curve/DCurve/Cmd/DExportAbcCmd.cpp" 'ScopedComputation[[:space:]]+computation'
forbid_regex "src/modules/curve/DCurve/Cmd/DExportAbcCmd.cpp" 'MAnimControl::currentTime|MComputation[[:space:]]+computation'
require_regex "src/core/common/utility/DGeometryLayout.h" 'checkedAccumulatePolylineIndices'
require_regex "src/core/common/utility/DViewportBuffer.h" 'descriptor\.dimension\(\)'
require_regex "src/core/common/utility/DViewportBuffer.h" 'acquireIndexBuffer'
require_regex "src/core/common/utility/DViewportBuffer.h" 'createIndexBuffer'
require_regex "src/core/common/utility/DViewportPointBudget.h" 'kMaxVdbViewportPointCount'
require_regex "src/core/common/utility/DViewportPointBudget.h" 'buildPointSamplePlan'
require_regex "src/modules/volume/VDB/Common/DOpenVDBUtil.h" 'buildLeafPointSamplePlan'
forbid_regex "src/modules/volume/VDB/Common/DOpenVDBUtil.h" 'maxVoxelPoints[[:space:]]*=[[:space:]]*(26000000|30000000)'
require_regex "src/core/common/utility/DViewportGeometryBudget.h" 'class[[:space:]]+GeometryBudget'
require_regex "src/core/common/utility/DViewportGeometryBudget.h" 'checkedSurfaceIndexCount'
require_regex "src/core/common/utility/DViewportGeometryBudget.h" 'kMaxViewportVertexCount'
require_regex "src/core/common/utility/DViewportGeometryBudget.h" 'kMaxViewportIndexCount'
require_regex "src/core/common/utility/DViewportGeometryBudget.h" 'kMaxViewportScalarCount'
require_regex "src/modules/volume/VDB/Common/DOpenVDBUtil.h" 'numTriangles\(\)'
require_regex "src/modules/volume/VDB/Display/DOpenVDBDisplayVP2.cpp" 'GeometryBudget[[:space:]]+vertexBudget'
require_regex "src/modules/volume/VDB/Display/DOpenVDBDisplayVP2.cpp" 'mSurfaceBuffersVP2\[gridIndex\]'
require_regex "src/modules/volume/VDB/Display/DOpenVDBDisplayVP2.cpp" \
  'clipWidth[[:space:]]*=[[:space:]]*0\.2f'
# The complete clip-box construction, settings and width admission are tested
# by the clip-helper and complete VDB viewport suites, not a local variable name.
require_regex "src/modules/volume/VDB/Display/DOpenVDBDisplayVP2.cpp" \
  'buildVdbViewportClipBox\(grids,[[:space:]]*axis,[[:space:]]*position,[[:space:]]*width'
forbid_regex "src/modules/volume/VDB/Display/DOpenVDBDisplayVP2.cpp" \
  'float[[:space:]]+width[[:space:]]*;[[:space:]]*$'
require_regex "src/modules/point/DPoint/Translator/DPInstancerTranslatorAi.cpp" \
  'readHandle\("AxisVector",[[:space:]]*axisVecHndOwner\)'
forbid_regex "src/modules/point/DPoint/Translator/DPInstancerTranslatorAi.cpp" \
  'readHandle\("AimVector",[[:space:]]*axisVecHndOwner\)'
require_regex "src/integrations/arnold/DArnoldResources.h" \
  'class[[:space:]]+NodeOwner'
require_regex "src/integrations/arnold/DArnoldResources.h" \
  'class[[:space:]]+ArrayOwner'
require_regex "src/integrations/arnold/DArnoldResources.h" \
  'class[[:space:]]+ArrayMapGuard'
require_regex "src/integrations/arnold/DArnoldMayaPlug.h" \
  'class[[:space:]]+PlugReader'
require_regex "src/integrations/arnold/DArnoldEntryPointGuard.h" \
  'DOMMY_ARNOLD_TRANSLATOR_CATCH'
for arnold_translator in \
  "src/modules/point/DPoint/Translator/DPointTranslatorAi.cpp" \
  "src/modules/point/DPoint/Translator/DPLightTranslatorAi.cpp" \
  "src/modules/point/DPoint/Translator/DPInstancerTranslatorAi.cpp" \
  "src/modules/mesh/DMesh/Translator/DMeshTranslatorAi.cpp" \
  "src/modules/curve/DCurve/Translator/DCurveTranslatorAi.cpp" \
  "src/modules/volume/VDB/Translator/DVDBTranslatorAi.cpp"; do
  require_regex "$arnold_translator" 'DOMMY_ARNOLD_TRANSLATOR_CATCH'
  forbid_regex "$arnold_translator" \
    'FindMayaPlug\([^)]*\)\.as(Bool|String|Short|Int|Float|Double)\('
  forbid_regex "$arnold_translator" \
    'AtArray[[:space:]]*\*[^;=]*=[[:space:]]*AiArrayAllocate'
done
for arnold_node_owner_source in \
  "src/core/DDataUtility/IO/DPWriteToAi.cpp" \
  "src/modules/curve/DCurve/IO/DWriteDCurveToAi.cpp" \
  "src/modules/point/DPoint/Translator/DPLightTranslatorAi.cpp" \
  "src/modules/point/DPoint/Translator/DPInstancerTranslatorAi.cpp" \
  "src/modules/point/DPoint/render/light/DPLightGen.cpp"; do
  if [[ "$arnold_node_owner_source" == "src/modules/point/DPoint/Translator/DPLightTranslatorAi.cpp" ]]; then
    require_regex "$arnold_node_owner_source" 'dommy::arnold::NodeBatchScope'
    require_regex "$arnold_node_owner_source" 'pendingNodes_\.create'
    require_regex "$arnold_node_owner_source" 'generationScope\.commit'
  else
    require_regex "$arnold_node_owner_source" 'dommy::arnold::NodeOwner'
  fi
  forbid_regex "$arnold_node_owner_source" \
    'AtArray[[:space:]]*\*[^;=]*=[[:space:]]*AiArrayAllocate'
done
require_regex "src/integrations/arnold/DArnoldResources.h" 'class[[:space:]]+NodeBatch'
require_regex "src/integrations/arnold/DArnoldResources.h" 'class[[:space:]]+NodeBatchScope'
require_regex "src/modules/point/DPoint/Translator/DPLightTranslatorAi.cpp" \
  'collectGeneratedNodes'
require_regex "src/modules/point/DPoint/Translator/DPLightTranslatorAi.cpp" \
  'destroyGeneratedNodes\(previousNodes\)'
require_regex "src/modules/point/DPoint/Translator/DPInstancerTranslatorAi.cpp" \
  'dommy_instancer_'
require_regex "src/modules/point/DPoint/Translator/DPInstancerTranslatorAi.cpp" \
  '_ass_instance_'
require_regex "src/modules/mesh/DMesh/Translator/DMeshTranslatorAi.cpp" \
  'meshoffset[[:space:]]*>=[[:space:]]*idsList\.size\(\)'
require_regex "src/modules/volume/VDB/Common/DOpenVDBGeoUtil.h" 'kMaxGeneratedFaceCount'
require_regex "src/core/common/utility/DComputeGuard.h" 'reportComputeException'
require_regex "src/core/common/utility/DMayaPluginData.h" 'MStatus[[:space:]]+getPluginData'
require_regex "src/core/common/utility/DMayaPluginData.h" 'getPluginDataFromHandle'
require_regex "src/core/common/utility/DMayaPluginData.h" 'getPluginDataFromPlug'
require_regex "src/core/common/utility/DMayaPluginData.h" 'dynamic_cast<DataType\*>'
require_regex "src/core/common/utility/DCommandGuard.h" 'reportCommandException'
require_regex "src/core/common/utility/DMayaEntryPointGuard.h" 'reportEntryPointException'
require_regex "src/core/common/utility/DLegacyDrawGuard.h" 'class[[:space:]]+ScopedLegacyGL[[:space:]]+final'
require_regex "src/core/common/utility/DLegacyDrawGuard.h" 'DOMMY_DRAW_CATCH'
require_regex "src/integrations/DJet/DJetSmokeSolver.h" \
  'MStatus[[:space:]]+initSolver\(MDataBlock& data\)'
require_regex "src/integrations/DJet/DJetSmokeSolver.cpp" \
  'dommy::fluid::buildGridPlan'
require_regex "src/integrations/DJet/DJetSmokeSolver.cpp" \
  'kMaxVdbViewportPointCount'
require_regex "src/integrations/DJet/DJetSmokeSolver.cpp" \
  'attributeAffects\(aGridSize,[[:space:]]*aOutFluid\)'
forbid_regex "src/integrations/DJet/DJetSmokeSolver.cpp" \
  'm_solver[[:space:]]*=[[:space:]]*(NULL|0)'
# The complete callback/default-ramp contract test above verifies that radius
# defaults are written to radiusRamp, not accidentally to the lens ramp.
require_regex "src/modules/generators/Generate/Human/DHEyeGen.cpp" 'thisMObject\(\), radiusRamp'
require_regex "src/modules/generators/Generate/Human/DHEyeGen.cpp" 'checkedEyeResolution'
require_regex "src/modules/generators/Generate/Human/DHEyeGen.cpp" 'publishGeneratedMesh'
require_regex "src/modules/generators/Generate/Human/DHEyeGen.cpp" 'status[[:space:]]*=[[:space:]]*mesh\.assignUVs'
require_regex "src/core/DDataUtility/Particle/nobject/DPnParticle.cpp" 'checkedMayaPointCount'
# The actual-body Nucleus setup suite checks scalar staging, default names and
# failed-setup recovery; do not couple those checks to local variable names.
require_regex "src/core/UtilityNode/DStretchData/DStretchData.cpp" 'sourceCount != relaxedCount'
require_regex "src/core/UtilityNode/DStretchData/DStretchData.cpp" 'hasMatchingEdgeTopology'
require_regex "src/core/UtilityNode/DCurvatureData/DCurvatureData.cpp" 'validFaceCount[[:space:]]*>[[:space:]]*0U'
require_regex "src/core/UtilityNode/DCurvatureData/DCurvatureData.cpp" 'smoothing work exceeds the safety budget'
require_regex "src/core/UtilityNode/DPolyFaceInfo/DPolyFaceInfo.cpp" 'foundTangent'
require_regex "src/core/UtilityNode/DMatToVec/DMatToVec.cpp" 'isFiniteMatrix'
require_regex "src/modules/point/DPoint/mesh/DPFoamMesh.cpp" 'sphere subdivision exceeds the topology budget'
require_regex "src/modules/point/DPoint/mesh/DPFoamMesh.cpp" 'self-deformation work exceeds the safety budget'
require_regex "src/modules/point/DPoint/mesh/DPFoamMesh.cpp" 'dist[[:space:]]*>[[:space:]]*std::numeric_limits<float>::epsilon'
forbid_regex "src/core/Utility/DUtility.h" '(DotProduct|Thermograph|Mod)[[:space:]]*\('
require_regex "src/modules/curve/DCurve/Generate/DCurveLinkGen.cpp" 'wrapPeriodicProductSum'
require_regex "src/modules/curve/DCurve/Utility/DCMotionPath.cpp" 'wrapPeriodicProductSum'
require_regex "src/modules/generators/Generate/DBubbleCrystal.cpp" 'mLengthMax[[:space:]]*>[[:space:]]*1\.0e-12f'
forbid_regex "src/modules/generators/Generate/DBubbleCrystal.cpp" 'mDist/mLengthMax\)\),[[:space:]]*heightNoisePowerVal'
for generator in \
  "src/modules/generators/Generate/DCurveDuplicator.cpp" \
  "src/modules/generators/Generate/LeafGen/LeafGen.cpp"; do
  forbid_regex "$generator" 'DMayaHeaders\.h'
done
require_regex "src/core/Data/DCurveData.cpp" 'expectedKnotCount'
require_regex "src/core/Data/DCurveData.h" \
  'static[[:space:]]+MStatus[[:space:]]+createFromMayaObject'
require_regex "src/core/Data/DCurveData.cpp" 'result\.swap\(candidate\)'
forbid_regex "src/core/Data/DCurveData.h" \
  'DCurveInternalData\(const MObject'
require_regex "src/core/Data/DCurveData.cpp" 'MStatus DCurveData::readASCII'
require_regex "src/core/Data/DCurveData.cpp" 'MStatus DCurveData::writeBinary'
require_regex "src/modules/mesh/DMesh/Data/DMeshData.cpp" 'hasValidPolygonTopology'
require_regex "src/modules/mesh/DMesh/Data/DMeshData.h" \
  'static[[:space:]]+MStatus[[:space:]]+createFromMayaObject'
require_regex "src/modules/mesh/DMesh/Data/DMeshData.cpp" 'result\.swap\(candidate\)'
forbid_regex "src/modules/mesh/DMesh/Data/DMeshData.h" \
  'DMeshInternalData\(const MObject'
require_regex "src/modules/mesh/DMesh/Data/DMeshData.cpp" 'hasValidMeshData'
require_regex "src/modules/mesh/DMesh/Data/DMeshData.cpp" 'hasValidUVTopology'
require_regex "src/modules/mesh/DMesh/Data/DMeshData.cpp" 'appendMeshes'
require_regex "src/modules/mesh/DMesh/Data/DMeshData.cpp" 'meshItemArray\.at\(index\)'
forbid_regex "src/modules/mesh/DMesh/Data/DMeshData.cpp" 'std::swap\(\*this,[[:space:]]*rhs\)'
forbid_regex "src/modules/mesh/DMesh/Data/DMeshData.cpp" 'beginGrp\(\).*endEdgeGrp\(\)'
require_regex "src/modules/mesh/DMesh/Data/DMeshData.cpp" 'getCheckedTotalVertexForDisplay'
require_regex "src/modules/mesh/DMesh/Data/DMeshData.cpp" 'MStatus DMeshData::readASCII'
require_regex "src/modules/mesh/DMesh/Data/DMeshData.cpp" 'MStatus DMeshData::writeBinary'
# Complete display conversion adapters are tested by check_viewport_attribute_conversion.py.
for curve_viewport_source in \
  "src/modules/curve/DCurve/Display/DCurveDisplay.cpp" \
  "src/modules/curve/DCurve/Display/DCMarker.cpp"; do
  require_regex "$curve_viewport_source" 'kMaxViewportVertexCount'
  require_regex "$curve_viewport_source" 'kMaxViewportIndexCount'
  require_regex "$curve_viewport_source" 'kMaxViewportScalarCount'
  require_regex "$curve_viewport_source" 'getPositionPtrConst'
  require_regex "$curve_viewport_source" 'generated viewport layout is inconsistent'
  require_regex "$curve_viewport_source" 'DOMMY_VIEWPORT_VOID_CATCH|reportViewportCallbackException'
  forbid_regex "$curve_viewport_source" \
    'DCurveBufferObject|cachedDCurve|displayUpDate|postEvaluation|&v\[0\]|getPositionPtr\(\)|^[[:space:]]*/\*'
done
require_regex "src/modules/curve/DCurve/Display/DCMarker.cpp" \
  'clearMarkerGeometry'
require_regex "src/modules/curve/DCurve/Display/DCMarker.cpp" \
  'curveBounds\.center\(\)'
require_regex "src/modules/curve/DCurve/Display/DCMarker.cpp" \
  'sourceOffset[[:space:]]*=[[:space:]]*i[[:space:]]*\*[[:space:]]*3U'
forbid_regex "src/modules/curve/DCurve/Display/DCMarker.cpp" \
  'centerPtBuffer\[i(\]|\+)'
require_regex "src/modules/curve/DCurve/Display/DCMarker.cpp" \
  'if[[:space:]]*\([[:space:]]*centerPositionBuffer[[:space:]]*&&[[:space:]]*centerPositions[[:space:]]*\)'
require_regex "src/modules/curve/DCurve/Display/DCMarker.cpp" \
  'if[[:space:]]*\([[:space:]]*indexPositionBuffer[[:space:]]*&&[[:space:]]*indexPositions[[:space:]]*\)'
require_regex "src/modules/curve/DCurve/Display/DCMarker.cpp" \
  'if[[:space:]]*\([[:space:]]*indexNumericBuffer[[:space:]]*&&[[:space:]]*indexNumerics[[:space:]]*\)'
for viewport_source in \
  "src/modules/point/DPoint/Display/DPMarker.cpp" \
  "src/modules/mesh/DMesh/Display/DMMarker.cpp" \
  "src/modules/mesh/DMesh/Display/DMeshDisplay.cpp"; do
  require_regex "$viewport_source" 'kMaxViewportVertexCount'
  require_regex "$viewport_source" 'kMaxViewportScalarCount'
  require_regex "$viewport_source" 'getPositionPtrConst'
  require_regex "$viewport_source" 'DOMMY_VIEWPORT_VOID_CATCH|reportViewportCallbackException'
  require_regex "$viewport_source" 'verticesBuffer[[:space:]]*&&[[:space:]]*!vertices'
  forbid_regex "$viewport_source" \
    'getPositionPtr\(\)|^[[:space:]]*/\*|meshFirstIndex\.push_back\(vIndex\)|norm[[:space:]]*\*='
done
require_regex "src/modules/point/DPoint/Display/DPMarker.cpp" \
  'visibleAttribute->attrSize\(\)[[:space:]]*!=[[:space:]]*count'
require_regex "src/modules/point/DPoint/Display/DPMarker.cpp" \
  'color attribute layout is inconsistent'
require_regex "src/modules/point/DPoint/Display/DPMarker.cpp" \
  'vector attribute layout is inconsistent'
require_regex "src/modules/point/DPoint/Display/DPMarker.cpp" \
  'value attribute layout is inconsistent'
require_regex "src/modules/point/DPoint/Display/DPMarker.cpp" \
  'group viewport layout is inconsistent'
require_regex "src/modules/mesh/DMesh/Display/DMMarker.cpp" \
  'meshFirstIndex\.push_back\(vOffset\)'
require_regex "src/modules/mesh/DMesh/Display/DMMarker.cpp" \
  'generated viewport layout is inconsistent'
require_regex "src/modules/mesh/DMesh/Display/DMMarker.cpp" \
  'meshFirstIndex\[i\][[:space:]]*\*[[:space:]]*3U'
require_regex "src/modules/mesh/DMesh/Display/DMMarker.cpp" \
  'kMaxViewportIndexCount'
require_regex "src/modules/mesh/DMesh/Display/DMeshDisplay.cpp" \
  'kMaxViewportIndexCount'
require_regex "src/modules/mesh/DMesh/Display/DMeshDisplay.cpp" \
  'generated viewport layout is inconsistent'
require_regex "src/modules/mesh/DMesh/Display/DMeshDisplay.cpp" \
  'transformDisplayNormal'
require_regex "src/modules/mesh/DMesh/Display/DMeshDisplay.cpp" \
  'normalTransform = \(\*it\)->getTransformMatrix\(\)'
require_regex "src/modules/mesh/DMesh/Display/DMeshDisplay.cpp" \
  'expandedColorScalarCount'
for mesh_display_source in \
  "src/modules/mesh/DMesh/Display/DMeshDisplay.cpp" \
  "src/modules/mesh/DMesh/Display/DMeshDisplay.h"; do
  forbid_regex "$mesh_display_source" \
    'mDataIndexList|allTriPointCount|displayPtCount|meshFirstIndex'
done
require_regex "src/modules/curve/DCurve/Cmd/Abc/DCurveWriter.cpp" 'hasExportableCurveTopology'
require_regex "src/modules/curve/DCurve/Cmd/Abc/DMeshWriter.cpp" 'hasExportableMeshTopology'
require_regex "src/modules/curve/DCurve/Cmd/Abc/DPointWriter.cpp" 'std::vector<Alembic::AbcGeom::V3f>[[:space:]]+position'
require_regex "src/modules/curve/DCurve/Translator/DCurveTranslatorAi.cpp" 'hasRenderablePointAttribute'
require_regex "src/modules/point/DPoint/Utility/DPAffect.cpp" 'dommy::math::normalizeRange'
require_regex "src/modules/point/DPoint/Utility/DPAffect.cpp" 'MStatus[[:space:]]+sampleAffectRamp'
require_regex "src/modules/point/DPoint/Utility/DPAffect.cpp" 'DOMMY_RETURN_IF_FAILURE\(affectgen\(\)\)'
require_regex "src/modules/point/DPoint/Utility/DPAvoid.cpp" 'kMaximumAvoidIterations'
require_regex "src/modules/point/DPoint/Utility/DPAvoid.cpp" 'std::atomic<bool>'
require_regex "src/modules/point/DPoint/Utility/DPAvoid.cpp" 'DOMMY_RETURN_IF_FAILURE\(avGen\(\)\)'
require_regex "src/modules/mesh/DMesh/common/DMeshUtil.cpp" \
  'MStatus[[:space:]]+getValidInputDM'
require_regex "src/modules/mesh/DMesh/common/DMeshUtil.cpp" \
  'hasValidMeshTopology'
require_regex "src/modules/curve/DCurve/DCurveUtil.cpp" \
  'MStatus[[:space:]]+getValidInputDC'
require_regex "src/modules/curve/DCurve/DCurveUtil.cpp" \
  'hasValidCurveTopology'
# Affect now uses the shared typed input/publication path. Its executable suite
# injects failures through all three geometry branches and checks input immutability.
require_regex "src/modules/point/DPoint/Utility/DPAffect.cpp" 'dommy::point::PointForceInput input'
require_regex "src/modules/point/DPoint/Utility/DPAffect.cpp" 'DOMMY_RETURN_IF_FAILURE\(input\.bind\(inputHandle\)\)'
for topology in hasValidPointTopology hasValidCurveTopology hasValidMeshTopology; do
  require_regex "src/modules/point/DPoint/Common/DPointForceOutput.h" "$topology"
done
for output_type in DPData DCurveData DMeshData; do
  require_regex "src/modules/point/DPoint/Utility/DPAffect.cpp" "setPluginDataOutput<$output_type>"
done
forbid_regex "src/modules/point/DPoint/Utility/DPAffect.cpp" \
  'output\.set\((outputDPData|outDCrvs|outDMeshes)\)'
for force_source in \
  "src/modules/point/DPoint/Utility/DPAvoid.cpp" \
  "src/modules/point/DPoint/Utility/DPMagnet.cpp"; do
  require_regex "$force_source" 'input\.bind\(inputHandle\)'
  require_regex "$force_source" 'validPointForceTargetBudget'
  require_regex "$force_source" 'evaluatePointForceOutput'
  forbid_regex "$force_source" 'data\.inputValue\([^,&]+\)\.|data\.outputValue\(outDD\)'
done
for invariant in hasValidPointTopology hasValidCurveTopology hasValidMeshTopology \
  getPluginDataFromHandle setPluginDataOutput replacePositions; do
  require_regex "src/modules/point/DPoint/Common/DPointForceOutput.h" "$invariant"
done
require_regex "src/modules/point/DPoint/Utility/DPSpiralField.cpp" 'distancePosition'
require_regex "src/modules/point/DPoint/Utility/DPSpiralField.cpp" 'static_cast<double>\(cache\.m_distance\)'
require_regex "src/modules/point/DPoint/Utility/DPSpiralField.cpp" 'static_cast<double>\(radius\)'
require_regex "src/core/common/utility/DMayaMeshClosestPoint.h" 'class MeshClosestPointQuery final'
for closest_point_source in \
  "src/modules/point/DPoint/Utility/DPAffect.cpp" \
  "src/modules/point/DPoint/Utility/DPAvoid.cpp" \
  "src/modules/point/DPoint/Utility/DPMagnet.cpp" \
  "src/modules/point/DPoint/Utility/DPDistance.cpp" \
  "src/modules/deformers/Deformer/DRayDeformer.cpp" \
  "src/modules/deformers/Deformer/DCollisionDeformer.cpp" \
  "src/modules/mesh/DMesh/Utility/DMFaceInOut.cpp"; do
  require_regex "$closest_point_source" 'MeshClosestPointQuery'
  forbid_regex "$closest_point_source" 'MFnMesh[^;]*\.getClosestPoint|getClosestPoint\([^;]*MSpace::kWorld'
done
forbid_regex "src/core/common/object/DNoiseOp.h" 'MDataHandle[[:space:]]+m_objHnd'
require_regex "src/modules/curve/DCurve/DCurveUtil.cpp" 'dommy::maya::buildCurveKnots'
require_regex "src/core/common/utility/DMayaCurveKnots.h" 'memory::checkedAdd'
require_regex "src/core/common/utility/DMayaCurveKnots.h" 'finalKnotValue'
require_regex "src/modules/curve/DCurve/Generate/DCEyelashGen.cpp" 'nAttr\.setMin\([[:space:]]*2[[:space:]]*\)'
require_regex "src/modules/generators/Generate/Human/DHEyelashGen.cpp" 'nAttr\.setMin\([[:space:]]*2[[:space:]]*\)'
require_regex "src/modules/curve/DCurve/Cmd/Abc/DCAbcWriteJob.cpp" 'prepareExportCallback'
require_regex "src/modules/curve/DCurve/Cmd/Abc/DCAbcWriteJob.cpp" 'maximumBytes[[:space:]]*=[[:space:]]*4U[[:space:]]*[*][[:space:]]*1024U[[:space:]]*[*][[:space:]]*1024U'
require_regex "src/modules/curve/DCurve/Cmd/Abc/DCAbcWriteJob.cpp" 'replaceAll[(]candidate,[^;]*maximumBytes[)]'
require_regex "src/modules/mesh/DMesh/Utility/DMSubDivide.cpp" 'switch[[:space:]]*\(interpType\)'
require_regex "src/modules/mesh/DMesh/Utility/DMSubDivide.cpp" 'case[[:space:]]+6:'
for bullet_mesh_reader in \
  "src/modules/dynamics/DBullet/Collision/DBCollisionShapeNode.cpp" \
  "src/modules/dynamics/DBullet/SoftBody/DBSoftBodyNode.cpp"; do
  require_regex "$bullet_mesh_reader" 'numVertices\(&status\)'
  require_regex "$bullet_mesh_reader" 'preflightMeshTriangles'
  require_regex "$bullet_mesh_reader" 'validMeshTriangleArrays'
done
require_regex "src/modules/dynamics/DBullet/Collision/DBCollisionShapeNode.cpp" 'getVertexNormals\(false, mayaNormals, MSpace::kWorld\)'
require_regex "src/modules/dynamics/DBullet/Collision/DBCollisionShapeNode.cpp" 'status && mayaNormals.length\(\) == vertexCount'
forbid_regex "src/modules/dynamics/DBullet/Collision/DBCollisionShapeNode.cpp" '\.getNormals\('
require_regex "src/modules/dynamics/DBullet/dbSolver_t.cpp" 'hasValidCollisionMeshInput'
require_regex "src/modules/dynamics/DBullet/DB_BulletSolver_t.h" 'hasValidCollisionMeshInput'
require_regex "src/modules/dynamics/DBullet/dbSolver_t.h" 'class[[:space:]]+LockedSolverAccess[[:space:]]+final'
require_regex "src/modules/dynamics/DBullet/dbSolver_t.h" 'std::recursive_mutex[[:space:]]+m_mutex'
require_regex "src/modules/dynamics/DBullet/dbSolver_t.cpp" 'std::lock_guard<std::recursive_mutex>[[:space:]]+stateLock'
forbid_regex "src/modules/dynamics/DBullet/dbSolver_t.h" 'get_solver\('
require_regex "src/modules/dynamics/DBullet/Collision/DCollisionMeshInput.h" 'generation::kMaxGeneratedPointCount'
require_regex "src/modules/dynamics/DBullet/Collision/DCollisionMeshInput.h" 'generation::kMaxGeneratedFaceCount'
require_regex "src/modules/dynamics/DBullet/Collision/DCollisionMeshInput.h" 'a[[:space:]]*==[[:space:]]*b'
require_regex "src/modules/dynamics/DBullet/Collision/DCollisionMeshInput.h" 'hasValidSoftBodyMeshInput'
require_regex "src/modules/dynamics/DBullet/Collision/DBCollisionShapeNode.h" 'schedulingType\(\)[[:space:]]+const[[:space:]]+override[[:space:]]*\{[[:space:]]*return[[:space:]]+kUntrusted'
require_regex "src/modules/dynamics/DBullet/Collision/DBCollisionShapeNode.cpp" 'dbCollisionShape_t::pointer[[:space:]]+replacement'
for legacy_connection_source in \
  "src/modules/dynamics/DBullet/Collision/DBCollisionShapeNode.cpp" \
  "src/modules/dynamics/DBullet/DM/DCBullet2DSolver.cpp" \
  "src/modules/dynamics/DBullet/DM/DMBulletTest.cpp" \
  "src/modules/mesh/DMesh/Dynamics/Bullet/DBulletFractureSolver.cpp" \
  "src/modules/mesh/DMesh/Dynamics/Bullet/commonDBulletConstraint.h"; do
  forbid_regex "$legacy_connection_source" \
    'connectedTo\([^;]*\)[[:space:]]*(!=|==)[[:space:]]*MS::kSuccess'
done
require_regex "src/modules/dynamics/DBullet/Constraint/dbConstraint_t.h" \
  'shared_ptr<dbRigidBody_t>[[:space:]]+m_rigid_bodyA'
require_regex "src/modules/dynamics/DBullet/Constraint/dbConstraint_t.h" \
  'shared_ptr<dbRigidBody_t>[[:space:]]+m_rigid_bodyB'
for constraint_impl in \
  "src/modules/dynamics/DBullet/Constraint/DB_Nail_Constraint_t.h" \
  "src/modules/dynamics/DBullet/Constraint/DB_Hinge_Constraint_t.h" \
  "src/modules/dynamics/DBullet/Constraint/DB_Slider_Constraint_t.h" \
  "src/modules/dynamics/DBullet/Constraint/DB_Sixdof_Constraint_t.h"; do
  forbid_regex "$constraint_impl" 'add_constraint\([[:space:]]*this[[:space:]]*\)'
done
require_regex "src/modules/dynamics/DBullet/dbSolver_t.cpp" \
  'while[[:space:]]*\(!m_constraints\.empty\(\)\)'
require_regex "src/modules/dynamics/DBullet/dbSolver_t.cpp" \
  'remove_constraint\(constraint\)'
require_regex "src/modules/dynamics/DBullet/dbSolver_t.cpp" \
  'reconfigure_rigid_body'
require_regex "src/modules/dynamics/DBullet/solver_impl.h" \
  'virtual[[:space:]]+bool[[:space:]]+add_constraint'
for constraint_node_header in \
  "src/modules/dynamics/DBullet/Constraint/DBNailConstraintNode.h" \
  "src/modules/dynamics/DBullet/Constraint/DBHingeConstraintNode.h" \
  "src/modules/dynamics/DBullet/Constraint/DBSliderConstraintNode.h" \
  "src/modules/dynamics/DBullet/Constraint/DBSixdofConstraintNode.h"; do
  require_regex "$constraint_node_header" \
    'schedulingType\(\)[[:space:]]+const[[:space:]]+override[[:space:]]*\{[[:space:]]*return[[:space:]]+kUntrusted'
  require_regex "$constraint_node_header" 'MStatus[[:space:]]+evaluate_constraint'
done
forbid_regex "src/modules/dynamics/DBullet/Solver/DBSolverNode.cpp" \
  '->(rigid_body|rigid_bodies|soft_body|constraint)\(\)'
require_regex "src/modules/dynamics/DBullet/Solver/DBSolverNode.cpp" \
  'cached_rigid_body\(\)'
forbid_regex "src/modules/dynamics/DBullet/Solver/DBSolverNode.cpp" \
  'rbNode->computeRigidBody|sbNode->computeSoftBody|deleteRigidBodies'
require_regex "src/modules/dynamics/DBullet/Solver/DBSolverNode.cpp" \
  'const auto failStartInitialization'
forbid_regex "src/modules/dynamics/DBullet/RigidBody/dbRigidBody_t.h" \
  'remove_all_constraints'
require_regex "src/modules/dynamics/DBullet/Collision/DMassProperties.h" 'computeMassProperties'
require_regex "src/modules/dynamics/DBullet/Collision/DMassProperties.h" 'std::abs\(signedSixVolume\)'
require_regex "src/modules/dynamics/DBullet/Mesh/DB_MeshShape_t.h" 'm_bulletIndices'
require_regex "src/modules/mesh/DMesh/Dynamics/Bullet/db_collision_mesh_shapes.h" 'computeMassProperties'
require_regex "src/modules/mesh/DMesh/Dynamics/Bullet/commonDBulletCollision.h" 'hasValidCollisionMeshInput'
forbid_regex "src/modules/dynamics/DBullet/Mesh/DB_MeshShape_t.h" '\([[:space:]]*int[[:space:]]*\*[[:space:]]*\)'
forbid_regex "src/modules/dynamics/DBullet/Mesh/DB_HacdShape_t.h" '\([[:space:]]*int[[:space:]]*\*[[:space:]]*\)'
forbid_regex "src/modules/mesh/DMesh/Dynamics/Bullet/db_collision_mesh_shapes.h" '\([[:space:]]*int[[:space:]]*\*[[:space:]]*\)'
forbid_regex "src/modules/dynamics/DBullet/mvl/mathUtils.h" 'center_of_mass|^[[:space:]]*T[[:space:]]+volume\(|inertia\('
require_regex "src/modules/dynamics/DBullet/mvl/quat.h" 'rhs\[0\],[[:space:]]*-rhs\[1\],[[:space:]]*-rhs\[2\],[[:space:]]*-rhs\[3\]'
require_regex "src/modules/dynamics/DBullet/mvl/mat.h" 'mat<T,[[:space:]]*C,[[:space:]]*R>'
require_regex "src/modules/dynamics/DBullet/DB_BulletSolver_t.cpp" 'm_debugMode\(btIDebugDraw::DBG_NoDebug\)'
require_regex "src/modules/mesh/DMesh/Dynamics/Bullet/commonDBullet.h" 'm_debugMode\(btIDebugDraw::DBG_NoDebug\)'
require_regex "src/modules/dynamics/DBullet/shared_ptr.h" 'using[[:space:]]+shared_ptr[[:space:]]*=[[:space:]]*std::shared_ptr'
forbid_regex "src/modules/dynamics/DBullet/shared_ptr.h" 'shared_count|pthread_mutex|Interlocked'
forbid_regex "src/modules/dynamics/DBullet/Collision/DBCollisionShapeNode.cpp" 'UPDATE_SHAPE|btGeometryUtil|std::cout|(^|[^[:alnum:]_])cout[[:space:]]*<<'
forbid_regex "src/modules/dynamics/DBullet/Collision/convex_decomposition_hacd.cpp" 'gCancelRequest'
require_regex "src/modules/dynamics/DBullet/Collision/convex_decomposition_hacd.cpp" 'class ProgressWindowGuard'
require_regex "src/core/Utility/3d/DataStruct/DHashGrid.h" 'DHashGridPositionIsFinite'
require_regex "src/core/Utility/3d/DataStruct/DHashGrid.h" 'std::vector<uint>[[:space:]]+hSortedIndex'
forbid_regex "src/core/Utility/3d/DataStruct/DHashGrid.h" 'delete[[:space:]]*\[[[:space:]]*\][[:space:]]+m_hCellData'
forbid_regex "src/core/Utility/3d/DataStruct/DHashGrid.h" 'm_hCellData\.h(SortedIndex|GridParticleHash|CellStart|CellEnd)[[:space:]]*=[[:space:]]*new[[:space:]]+uint'
forbid_regex "src/core/Utility/3d/DataStruct/DHashGrid.h" 'new[[:space:]]+uint\['
forbid_regex "src/modules/mesh/DMesh/Dynamics/Bullet/DBFractureDynamicsWorld.cpp" 'static[[:space:]]+float[[:space:]]+maxImpact'
require_regex "src/core/DDataUtility/Generate/DPointPlaneEmit.cpp" 'sampling::generateBlueNoise2D'
require_regex "src/core/geometry/DBlueNoiseSampler.h" 'generation::WorkBudget'
require_regex "src/core/geometry/DBlueNoiseSampler.h" 'std::vector<std::size_t>[[:space:]]+ids_, positions_'
forbid_regex "src/core/geometry/DBlueNoiseSampler.h" 'new[[:space:]]*\[|delete[[:space:]]*\['
forbid_regex "src/core/DDataUtility/Generate/DPointPlaneEmit.cpp" 'cy::|cyPoint|cySampleElim'
require_regex "src/modules/curve/DCurve/Solver/DCSolver.h" 'class[[:space:]]+DCSolver[[:space:]]+final'
forbid_regex "src/modules/curve/DCurve/Solver/DCSolver.h" 'DCurveDataHeaders\.h|DMayaHeaders\.h'
forbid_regex "src/modules/curve/DCurve/Solver/DCSolver.cpp" 'DCurveDataHeaders\.h|DMayaHeaders\.h'
forbid_regex "src/core/DataModifier/ShapeSequencer/DShapeSequencer.cpp" 'VERBOSE|std::cout|(^|[^[:alnum:]_])cout[[:space:]]*<<'
require_regex "src/modules/volume/VDB/Edit/DOpenVDBCopy.cpp" 'DOMMY_VDB_COMPUTE_CATCH'
require_regex "src/modules/volume/VDB/Common/DOpenVDBGeoUtil.cpp" \
  'status[[:space:]]*=[[:space:]]*mesh\.getPoints\(vertices,[[:space:]]*MSpace::kObject\)'
require_regex "src/modules/volume/VDB/Common/DOpenVDBGeoUtil.cpp" \
  'pointList\.swap\(candidate\)'
require_regex "src/modules/volume/VDB/Common/DOpenVDBGeoUtil.cpp" \
  'triangleVertices\.length\(\)[[:space:]]*%[[:space:]]*3U[[:space:]]*!='
require_regex "src/modules/volume/VDB/Common/DOpenVDBGeoUtil.cpp" \
  'primList\.swap\(candidate\)'
require_regex "src/modules/volume/VDB/Common/DOpenVDBGeoUtil.cpp" \
  'validVertexIndex'
require_regex "src/modules/volume/VDB/Generate/DOpenVDBFromMesh.cpp" \
  'appendOpenVDBMesh'
forbid_regex "src/modules/volume/VDB/Generate/DOpenVDBFromMesh.cpp" \
  'for[[:space:]]*\([[:space:]]*MItMeshPolygon'
require_regex "src/modules/volume/VDB/Dynamics/DOpenVDBField.cpp" 'BoxSampler::sample'
require_regex "src/modules/volume/VDB/Edit/DOpenVDBPlatonic.cpp" 'createLevelSetSphere'
require_regex "src/modules/volume/VDB/Edit/DOpenVDBPlatonic.cpp" 'sdfToFogVolume'
require_regex "src/modules/particles/particle/field/DCurlNoiseField.cpp" 'DOMMY_COMPUTE_CATCH'
require_regex "src/modules/particles/particle/field/DCurlNoiseField.cpp" 'outputForce.copy\(candidate\)'
require_regex "src/modules/particles/particle/field/DCurlNoiseField.cpp" \
  'checkedProduct'
require_regex "src/modules/particles/particle/field/DCurlNoiseField.cpp" \
  'kMaximumPositionNoiseFrequency'
require_regex "src/modules/particles/particle/field/DCurlNoiseField.cpp" \
  'parameters.attenuation[[:space:]]*<[[:space:]]*0\.0'
require_regex "src/modules/particles/particle/field/DCurlNoiseField.cpp" \
  'getWorldPosition\(block,[[:space:]]*worldPosition\)'
forbid_regex "src/modules/particles/particle/field/DCurlNoiseField.cpp" \
  'getWorldPosition\(MVector&|status[[:space:]]*=[[:space:]]*getWorldPosition\(\)'
require_regex "src/modules/curve/DCurve/Cmd/Abc/MayaAttributeWriter.cpp" 'arrayDataOrNull'
require_regex "src/modules/curve/DCurve/Cmd/Abc/MayaAttributeWriter.cpp" 'resizeArrayValues'
require_regex "src/modules/curve/DCurve/Cmd/Abc/MayaAttributeWriter.cpp" 'readNumericData4Double'
forbid_regex "src/modules/curve/DCurve/Cmd/Abc/MayaAttributeWriter.cpp" 'numdFn\.getData(2|3|4)(Short|Int|Float|Double)'
require_regex "src/core/Data/DDataUtility.h" 'checkDDataPointAttribute\(const std::string,[[:space:]]*T&,[[:space:]]*bool\)[[:space:]]*=[[:space:]]*delete'
require_regex "src/core/Data/DDataUtility.h" 'checkDDataAttribute\(const std::string,[[:space:]]*T&,[[:space:]]*bool\)[[:space:]]*=[[:space:]]*delete'
forbid_regex "src/core/Data/DDataUtility.cpp" 'template[[:space:]]*<typename[[:space:]]+T>'
forbid_regex "src/plugin/pluginMain.cpp" 'executeCommand\([[:space:]]*"DommyTool"'
# Nonempty fog conversion and the native-empty-input bypass are executed by
# check_vdb_generation_safety.py. A blanket GRID_FOG_VOLUME ban would reject
# the correctly classified, zero-background empty result.
forbid_regex "src/modules/volume/VDB/Edit/DOpenVDBPlatonic.cpp" 'makeSphere'
forbid_regex "src/modules/volume/VDB/Dynamics/DOpenVDBField.cpp" '^[[:space:]]*(std::)?cout[[:space:]]*<<'
forbid_regex "src/modules/particles/particle/field/DCurlNoiseField.cpp" 'ages\[i\]|^[[:space:]]*(std::)?cout[[:space:]]*<<'
require_regex "src/commands/setParticleRGBCmd.cpp" 'shadingSets\.length\(\)[[:space:]]*==[[:space:]]*0'
require_regex "src/commands/setParticleRGBCmd.cpp" 'sampledColors\.length\(\)[[:space:]]*!='
require_regex "src/commands/setParticleRGBCmd.cpp" 'kCameraLongFlag'
require_regex "src/commands/setParticleRGBCmd.cpp" 'queueMissingParticleRgbAttributes'
require_regex "src/commands/setParticleRGBCmd.cpp" 'setParticleRGBCmd::undoIt'
require_regex "src/commands/setParticleRGBCmd.cpp" 'attributeModifier\.undoIt\(\)'
require_regex "src/commands/setParticleRGBCmd.h" 'isUndoable\(\)[[:space:]]+const[[:space:]]+override'
require_regex "src/modules/mesh/mesh/Cmd/DClusterWeightCmd.cpp" 'cluster\.getWeights'
require_regex "src/modules/mesh/mesh/Cmd/DClusterWeightCmd.cpp" 'cluster\.setWeight\(srcMeshPath,[[:space:]]*componentObject,[[:space:]]*weights\)'
require_regex "src/modules/mesh/mesh/Cmd/DClusterWeightCmd.cpp" 'applyWeights\(originalWeights\)'
forbid_regex "src/modules/mesh/mesh/Cmd/DClusterWeightCmd.cpp" 'MStatus[[:space:]]+DClusterWeightCmd::(redoIt|undoIt)\([^}]*return[[:space:]]+MStatus\(MS::kSuccess\)'
require_regex "src/modules/generators/icosahedron/icosahedronCmd.cpp" 'shadingGroupFn\.addMember'
require_regex "src/modules/generators/icosahedron/icosahedronCmd.cpp" 'shadingGroupFn\.removeMember'
require_regex "src/modules/generators/icosahedron/icosahedronCmd.cpp" 'dagMod\.undoIt\(\)'
# Scatter dispatch is covered for all eight topology/four scalar types by the
# executable distribution suite, including the historical IsLocal polarity.
require_regex "src/core/common/utility/DShapeFillPlan.h" 'buildShapeFillPlan'
require_regex "src/commands/shapeFillCmd.cpp" 'double[[:space:]]+randomOffset\[3\][[:space:]]*=[[:space:]]*\{0\.0,[[:space:]]*0\.0,[[:space:]]*0\.0\}'
require_regex "src/commands/shapeFillCmd.h" 'isUndoable\(\)[[:space:]]+const[[:space:]]+override[[:space:]]*\{[[:space:]]*return[[:space:]]+true'
require_regex "src/modules/dynamics/DBullet/Solver/DBSolverNode.cpp" 'mass\.append\(bodyMass\)'
require_regex "src/modules/dynamics/DBullet/Solver/DBSolverNode.cpp" 'force\.length\(\)[[:space:]]*!=[[:space:]]*rigid_bodies\.size\(\)'
require_regex "src/modules/dynamics/DBullet/Solver/DBSolverNode.h" 'MStatus[[:space:]]+initRigidBodies'
require_regex "src/modules/dynamics/DBullet/Solver/DBSolverNode.h" 'MStatus[[:space:]]+gatherPassiveTransforms'
require_regex "src/modules/dynamics/DBullet/Solver/DBSolverNode.h" 'MStatus[[:space:]]+updatePassiveRigidBodies'
require_regex "src/modules/dynamics/DBullet/Solver/DBSolverNode.cpp" 'passiveBodyIndex[[:space:]]*>=[[:space:]]*xforms\.size\(\)'
require_regex "src/modules/dynamics/DBullet/Solver/DBSolverNode.cpp" 'kMaxGeneratedTransformCount'
require_regex "src/modules/dynamics/DBullet/Solver/DPassiveTransformMath.h" \
  'normalizedSubstepFraction'
require_regex "src/modules/dynamics/DBullet/Solver/DPassiveTransformMath.h" \
  'interpolateQuaternionShortest'
require_regex "src/modules/dynamics/DBullet/Solver/DBSolverNode.cpp" \
  'BulletWorldMutationGuard[[:space:]]+mutationGuard'
require_regex "src/modules/dynamics/DBullet/Solver/DBSolverNode.cpp" \
  'rigidForceTotals'
require_regex "src/modules/dynamics/DBullet/Solver/DBSolverNode.cpp" \
  'Commit only after every Maya read'
forbid_regex "src/modules/dynamics/DBullet/Solver/DBSolverNode.cpp" \
  'i[[:space:]]*\*[[:space:]]*dt[[:space:]]*/[[:space:]]*subSteps'
require_regex "src/modules/dynamics/DBullet/Solver/DBSolverNode.h" 'std::atomic<bool>[[:space:]]+s_isStartTime'
require_regex "src/modules/dynamics/DBullet/Solver/DBSolverNode.cpp" 's_isStartTime\.store'
require_regex "src/modules/dynamics/DBullet/Solver/DBSolverNode.h" 'std::atomic<DBSolverNode\*>[[:space:]]+s_activeSolver'
require_regex "src/modules/dynamics/DBullet/Solver/DBSolverNode.cpp" 'Only one DBSolverNode can own'
require_regex "src/modules/dynamics/DBullet/Solver/DBSolverNode.cpp" 'm_debugDrawMode\.load'
forbid_regex "src/modules/dynamics/DBullet/Solver/DBSolverNode.cpp" 'getDbgDrawVal|MPlug[[:space:]]+plug\(thisObj,[[:space:]]*attr\)'
require_regex "src/modules/dynamics/DBullet/DB_BulletSolver_t.h" 'DB_BulletSolver_t\(const DB_BulletSolver_t&\)[[:space:]]*=[[:space:]]*delete'
require_regex "src/modules/dynamics/DBullet/DB_BulletSolver_t.h" 'DB_BulletSolver_t\(DB_BulletSolver_t&&\)[[:space:]]*=[[:space:]]*delete'
require_regex "src/modules/dynamics/DBullet/DB_BulletSolver_t.cpp" 'std::unique_ptr<dommy::bullet::DBoundedSoftDynamicsWorld>'
forbid_regex "src/modules/dynamics/DBullet/DB_BulletSolver_t.cpp" 'm_(collisionConfiguration|dispatcher|broadphase|solver|dynamicsWorld)[[:space:]]*=[[:space:]]*new'
require_regex "src/modules/dynamics/DBullet/DBulletResourceOwnership.h" \
  'DBulletSoftBodyWorldInfoDeleter'
for bullet_owner_header in \
  "src/modules/dynamics/DBullet/DB_BulletSolver_t.h" \
  "src/modules/dynamics/DBullet/DM/DCBullet2DSolver.h" \
  "src/modules/dynamics/DBullet/DM/DMBulletTest.h" \
  "src/modules/mesh/DMesh/Dynamics/Bullet/DBulletFractureSolver.h"; do
  require_regex "$bullet_owner_header" \
    'std::unique_ptr<[^;]+DynamicsWorld>[[:space:]]+m_dynamicsWorld'
  require_regex "$bullet_owner_header" \
    'std::unique_ptr<btIDebugDraw>[[:space:]]+m_debugDrawer'
  forbid_regex "$bullet_owner_header" \
    '(bt[A-Za-z0-9_:]+|DBFractureDynamicsWorld)[[:space:]]*\*[[:space:]]*m_(worldInfo|broadphase|solver|collisionConfiguration|dispatcher|dynamicsWorld|plane_rb|plane_col_shape)'
done
for bullet_owner_source in \
  "src/modules/dynamics/DBullet/DB_BulletSolver_t.cpp" \
  "src/modules/dynamics/DBullet/DM/DCBullet2DSolver.cpp" \
  "src/modules/dynamics/DBullet/DM/DMBulletTest.cpp" \
  "src/modules/mesh/DMesh/Dynamics/Bullet/DBulletFractureSolver.cpp"; do
  forbid_regex "$bullet_owner_source" 'delete[[:space:]]+m_'
  forbid_regex "$bullet_owner_source" \
    'm_[A-Za-z0-9_]+[[:space:]]*=[[:space:]]*[A-Za-z0-9_]+\.release\(\)'
done
require_regex "src/modules/dynamics/DBullet/DM/DMBulletTest.cpp" \
  'const DCItem curve'
for modernized_source in \
  "src/core/Data/DPrimBaseData.cpp" \
  "src/core/DDataUtility/Data/DPData.cpp" \
  "src/core/Data/DCurveData.cpp" \
  "src/modules/mesh/DMesh/Data/DMeshData.cpp" \
  "src/modules/dynamics/DBullet/DM/DCBullet2DSolver.cpp" \
  "src/modules/dynamics/DBullet/DM/DMBulletTest.cpp" \
  "src/modules/mesh/DMesh/Dynamics/Bullet/DBulletFractureSolver.cpp"; do
  forbid_regex "$modernized_source" '#pragma[[:space:]]+mark'
  forbid_regex "$modernized_source" \
    '^// -----------------------------------------------------------------------------//'
  forbid_regex "$modernized_source" \
    '^// (simulation|outContactDP Data|bullet solver|field attribute|problem|out put|bbox func|point method|group list method|add attribute).*(::|template|[;{=])'
done
for nullptr_boundary in \
  "src/core/Data/DCurveData.cpp" \
  "src/modules/mesh/DMesh/Data/DMeshData.cpp" \
  "src/modules/dynamics/DBullet/DB_BulletSolver_t.h"; do
  forbid_regex "$nullptr_boundary" '(^|[^A-Za-z0-9_])NULL([^A-Za-z0-9_]|$)'
done
forbid_regex "src/modules/dynamics/DBullet/Draw/drawUtils.cpp" 'gl(GenLists|NewList|CallList|EndList)'
for bullet_draw_source in \
  "src/modules/dynamics/DBullet/RigidBody/DBRigidBodyNode.cpp" \
  "src/modules/dynamics/DBullet/RigidBody/DBRigidBodyArrayNode.cpp" \
  "src/modules/dynamics/DBullet/SoftBody/DBSoftBodyNode.cpp" \
  "src/modules/dynamics/DBullet/Constraint/DBNailConstraintNode.cpp" \
  "src/modules/dynamics/DBullet/Constraint/DBHingeConstraintNode.cpp" \
  "src/modules/dynamics/DBullet/Constraint/DBSliderConstraintNode.cpp" \
  "src/modules/dynamics/DBullet/Constraint/DBSixdofConstraintNode.cpp"; do
  forbid_regex "$bullet_draw_source" '^[[:space:]]*update[[:space:]]*\(\)[[:space:]]*;'
  require_regex "$bullet_draw_source" 'dbSolver_t::lock_solver\(\)'
  require_regex "$bullet_draw_source" 'ScopedLegacyGL'
  require_regex "$bullet_draw_source" 'DOMMY_DRAW_CATCH'
done
for draw_only_solver in \
  "src/modules/dynamics/DBullet/DM/DMBulletTest.cpp" \
  "src/modules/dynamics/DBullet/DM/DCBullet2DSolver.cpp" \
  "src/modules/mesh/DMesh/Dynamics/Bullet/DBulletFractureSolver.cpp"; do
  forbid_regex "$draw_only_solver" 'fnParentTransform\.set(Translation|Rotation|Scale)'
  require_regex "$draw_only_solver" 'ScopedLegacyGL'
  require_regex "$draw_only_solver" 'DOMMY_DRAW_CATCH'
done
for legacy_draw_source in \
  "src/modules/dynamics/DBullet/Solver/DBSolverNode.cpp" \
  "src/modules/rendering/Render/Camera/DCameraFrustum.cpp" \
  "src/modules/point/DPoint/Utility/DPDirection.cpp" \
  "src/core/DDataUtility/Group/DPGroup.cpp" \
  "src/modules/curve/DCurve/Group/DCGroup.cpp" \
  "src/modules/mesh/DMesh/Data/DMGroup.cpp" \
  "src/integrations/DJet/DJetSmokeSolver.cpp"; do
  require_regex "$legacy_draw_source" 'ScopedLegacyGL'
  require_regex "$legacy_draw_source" 'DOMMY_DRAW_CATCH'
done
for owned_field_source in \
  "src/modules/particles/particle/field/DCurlNoiseField.cpp" \
  "src/modules/volume/VDB/Dynamics/DOpenVDBField.cpp"; do
  require_regex "$owned_field_source" 'dommy::maya::drawFieldIcon\(view\)'
  require_regex "$owned_field_source" 'dommy::maya::readFieldInputs\('
  require_regex "$owned_field_source" 'dommy::maya::publishFieldForces\('
  require_regex "$owned_field_source" 'DOMMY_DRAW_CATCH'
done
require_regex "src/core/common/utility/DMayaFieldDraw.h" 'ScopedLegacyGL drawing'
require_regex "src/core/common/utility/DMayaFieldDraw.h" 'ScopedLegacyGLPrimitive lines'
require_regex "src/core/common/utility/DMayaFieldData.h" 'DOMMY_RETURN_IF_FAILURE\(array\.set\(builder\)\)'
require_regex "src/core/common/utility/DMayaFieldData.h" 'return data\.setClean\(requested\)'
for dependency_manip_source in \
  "src/modules/point/DPoint/Utility/DPDirection.cpp" \
  "src/core/DDataUtility/Group/DPGroup.cpp" \
  "src/modules/curve/DCurve/Group/DCGroup.cpp" \
  "src/modules/mesh/DMesh/Data/DMGroup.cpp"; do
  forbid_regex "$dependency_manip_source" 'MFnDagNode|displayWithNode|fNodePath'
  require_regex "$dependency_manip_source" 'addPlugToManipConversion'
done
for group_manip_source in \
  "src/core/DDataUtility/Group/DPGroup.cpp" \
  "src/modules/curve/DCurve/Group/DCGroup.cpp" \
  "src/modules/mesh/DMesh/Data/DMGroup.cpp"; do
  require_regex "$group_manip_source" 'DDependencyNodeValues\.h'
done
require_regex "src/modules/point/DPoint/Utility/DPDirection.cpp" 'DDirectionMath\.h'
require_regex "src/modules/point/DPoint/Utility/DPDirection.cpp" 'std::atomic<bool>'
require_regex "src/modules/point/DPoint/Utility/DPDirection.cpp" 'MStatus[[:space:]]+adjustDirections'
forbid_regex "src/modules/point/DPoint/Utility/DPDirection.cpp" '\.normalize[[:space:]]*\('
forbid_regex "src/modules/dynamics/DBullet/Solver/DBSolverNode.cpp" 'fnParentTransform\.set(Translation|Rotation|Scale).*lock'
require_regex "src/core/common/utility/DMayaUserNode.h" 'checkedUserNode'
require_regex "src/modules/mesh/DMesh/Dynamics/Bullet/commonDBulletConstraint.h" 'dommy::maya::checkedUserNode'
for bullet_user_node_source in \
  "src/modules/dynamics/DBullet/Solver/DBSolverNode.cpp" \
  "src/modules/dynamics/DBullet/RigidBody/DBRigidBodyNode.cpp" \
  "src/modules/dynamics/DBullet/RigidBody/DBRigidBodyArrayNode.cpp" \
  "src/modules/dynamics/DBullet/SoftBody/DBSoftBodyNode.cpp" \
  "src/modules/dynamics/DBullet/Constraint/DBNailConstraintNode.cpp" \
  "src/modules/dynamics/DBullet/Constraint/DBHingeConstraintNode.cpp" \
  "src/modules/dynamics/DBullet/Constraint/DBSliderConstraintNode.cpp" \
  "src/modules/dynamics/DBullet/Constraint/DBSixdofConstraintNode.cpp" \
  "src/modules/dynamics/DBullet/DM/DCBullet2DSolver.cpp" \
  "src/modules/dynamics/DBullet/DM/DMBulletTest.cpp"; do
  forbid_regex "$bullet_user_node_source" 'userNode[[:space:]]*\('
  require_regex "$bullet_user_node_source" 'dommy::maya::checkedUserNode'
done
require_regex "src/modules/mesh/DMesh/Dynamics/Bullet/commonDBulletConstraint.h" 'MStatus[[:space:]]+getDPointPositionToBTVec'
require_regex "src/modules/mesh/DMesh/Dynamics/Bullet/commonDBulletConstraint.h" 'if[[:space:]]*\(!body[[:space:]]*\|\|'
require_regex "src/modules/mesh/DMesh/Dynamics/Bullet/commonDBulletConstraint.h" 'findLinearSearch\(body\)'
for bullet_constraint_source in \
  "src/modules/mesh/DMesh/Dynamics/Bullet/Constraint/DBPointConstraint.cpp" \
  "src/modules/mesh/DMesh/Dynamics/Bullet/Constraint/DBLinkConstraint.cpp"; do
  forbid_regex "$bullet_constraint_source" 'userNode[[:space:]]*\('
  require_regex "$bullet_constraint_source" 'DOMMY_RETURN_IF_FAILURE\(getRBPositionToBTVecList'
  require_regex "$bullet_constraint_source" 'dommy::bullet::prepareConstraintFrames'
  require_regex "$bullet_constraint_source" 'constraint\(\)->setEnabled\(param\.m_is_enable\)'
  require_regex "$bullet_constraint_source" 'attributeAffects\([[:space:]]*aIsEnable,[[:space:]]*outConst[[:space:]]*\)'
  require_regex "$bullet_constraint_source" 'attributeAffects\([[:space:]]*aIsUpdate,[[:space:]]*outConst[[:space:]]*\)'
done
require_regex "src/modules/mesh/DMesh/Dynamics/Bullet/Constraint/DBPointConstraint.cpp" 'DOMMY_RETURN_IF_FAILURE\(getDPointPositionToBTVec'
require_regex "src/modules/mesh/DMesh/Dynamics/Bullet/Constraint/DBLinkConstraint.cpp" 'return[[:space:]]+createConstraint\(world,[[:space:]]*m_worldReference\.lock\(\)\)'
require_regex "src/modules/mesh/DMesh/Dynamics/Bullet/commonDBullet.h" 'class[[:space:]]+DBulletWorldReference[[:space:]]+final'
require_regex "src/modules/mesh/DMesh/Dynamics/Bullet/commonDBullet.h" 'DBulletRigidBodyDeleter'
require_regex "src/modules/mesh/DMesh/Dynamics/Bullet/commonDBullet.cpp" 'DetachAndReleaseBulletRigidBodies'
require_regex "src/modules/mesh/DMesh/Dynamics/Bullet/commonDBullet.cpp" 'AddBulletConstraint'
require_regex "src/modules/mesh/DMesh/Dynamics/Bullet/DCbtRope.h" 'ropeShapes'
require_regex "src/modules/mesh/DMesh/Dynamics/Bullet/commonBulletSoftBody.h" 'if[[:space:]]*\(!dommy::bullet::softBodyMassWeights\('
require_regex "src/modules/mesh/DMesh/Dynamics/Bullet/db_collision_shape_base.h" 'std::vector<std::unique_ptr<db_collision_shape_base>'
for bullet_companion_header in \
  "src/modules/mesh/DMesh/Dynamics/Bullet/DMbtRigid.h" \
  "src/modules/mesh/DMesh/Dynamics/Bullet/DCbtRigid2D.h" \
  "src/modules/mesh/DMesh/Dynamics/Bullet/DMbtSoft.h" \
  "src/modules/mesh/DMesh/Dynamics/Bullet/DCbtRope.h" \
  "src/modules/mesh/DMesh/Dynamics/Bullet/DMbtFracture.h" \
  "src/modules/mesh/DMesh/Dynamics/Bullet/Constraint/DBPointConstraint.h" \
  "src/modules/mesh/DMesh/Dynamics/Bullet/Constraint/DBLinkConstraint.h" \
  "src/modules/mesh/DMesh/Dynamics/Bullet/Constraint/BTP2PConstraint.h" \
  "src/modules/mesh/DMesh/Dynamics/Bullet/Constraint/BTFixConstraint.h" \
  "src/modules/mesh/DMesh/Dynamics/Bullet/Constraint/BTConeTwistConstraint.h" \
  "src/modules/mesh/DMesh/Dynamics/Bullet/Constraint/BT6DofConstraint.h"; do
  require_regex "$bullet_companion_header" 'schedulingType\(\)[[:space:]]+const[[:space:]]+override[[:space:]]*\{[[:space:]]*return[[:space:]]+kUntrusted'
done
for bullet_body_source in \
  "src/modules/mesh/DMesh/Dynamics/Bullet/DMbtRigid.cpp" \
  "src/modules/mesh/DMesh/Dynamics/Bullet/DCbtRigid2D.cpp" \
  "src/modules/mesh/DMesh/Dynamics/Bullet/DCbtRope.cpp" \
  "src/modules/mesh/DMesh/Dynamics/Bullet/DMbtFracture.cpp"; do
  forbid_regex "$bullet_body_source" 'new[[:space:]]+btRigidBody'
done
require_regex "src/plugin/registration/modules/BulletRegistrations.inl" 'DOMMY_WITH_EXPERIMENTAL_FRACTURE_SOLVER'
require_regex "src/plugin/registration/modules/BulletRegistrations.inl" 'DOMMY_WITH_EXPERIMENTAL_BULLET_TEST_NODE'
require_regex "cmake/DommyFullSourceCheck.cmake" 'DBulletFractureSolver\|DBFractureDynamicsWorld\|dbtFractureBody'
forbid_regex "src/modules/dynamics/DBullet/Solver/DBSolverNode.h" 'static[[:space:]]+bool[[:space:]]+isStartTime'
forbid_regex "src/modules/dynamics/DBullet/Solver/DBSolverNode.h" 'aSolverType|ia_DBG_DrawFeaturesText|ia_DBG_NoHelpText|ia_DBG_DrawText|ia_DBG_EnableSatComparison|ia_DBG_DisableBulletLCP|ia_DBG_EnableCCD|ia_DBG_FastWireframe'
for unused_start_flag in \
  "src/modules/dynamics/DBullet/DM/DCBullet2DSolver" \
  "src/modules/dynamics/DBullet/DM/DMBulletTest" \
  "src/modules/mesh/DMesh/Dynamics/Bullet/DBulletFractureSolver" \
  "src/integrations/DJet/DJetSmokeSolver"; do
  forbid_regex "${unused_start_flag}.h" 'static[[:space:]]+bool[[:space:]]+isStartTime'
  forbid_regex "${unused_start_flag}.cpp" '::isStartTime'
done
forbid_regex "src/modules/dynamics/DBullet/Solver/DBSolverNode.cpp" 'mass\.append\(1\.0\)'
require_regex "src/core/common/utility/DEvaluationContext.h" 'getEvaluationStepSeconds'
require_regex "src/modules/mesh/DMesh/Dynamics/nObject/DMnRigidGen.cpp" 'getEvaluationStepSeconds'
require_regex "src/core/DDataUtility/Particle/nobject/DPnParticle.cpp" 'getEvaluationStepSeconds'
for deformer in surfaceDeformer noiseDeformer pushDeformer DMotionDeformer DRayDeformer DCollisionDeformer; do
  require_regex "src/modules/deformers/Deformer/${deformer}.h" 'return[[:space:]]+kSerial'
  require_regex "src/modules/deformers/Deformer/${deformer}.cpp" 'DOMMY_DEFORMER_CATCH'
done
forbid_regex "src/modules/deformers/Deformer/pushDeformer.cpp" 'active3dView|M3dView'
require_regex "src/modules/deformers/Deformer/pushDeformer.cpp" 'cameraMatrix'
require_regex "src/modules/deformers/Deformer/noiseDeformer.cpp" 'radialFalloff'
require_regex "src/modules/deformers/Deformer/noiseDeformer.cpp" 'jumpToElement\(geomIndex\)'
require_regex "src/modules/deformers/Deformer/noiseDeformer.cpp" 'kMaxNoiseOctaves'
forbid_regex "src/modules/deformers/Deformer/noiseDeformer.cpp" '\(void\)(dropoffTypeVal|attenuationVal)'
require_regex "src/modules/deformers/Deformer/pushDeformer.cpp" 'jumpToElement\(geomIndex\)'
require_regex "src/modules/deformers/Deformer/DMotionDeformer.cpp" 'sampled topology changed'
require_regex "src/modules/deformers/Deformer/DMotionDeformer.cpp" 'MDGContextGuard'
require_regex "src/modules/deformers/Deformer/DMotionDeformer.cpp" 'forceCache\(\)'
require_regex "src/modules/deformers/Deformer/DMotionDeformer.cpp" 'validSampleStorageRequest'
forbid_regex "src/modules/deformers/Deformer/DMotionDeformer.cpp" 'dataBlock\.setContext'
require_regex "src/modules/mesh/DMesh/Deform/DMMotionDeform.h" 'return[[:space:]]+kSerial'
require_regex "src/modules/mesh/DMesh/Deform/DMMotionDeform.cpp" 'validMotionSampleRequest'
require_regex "src/modules/mesh/DMesh/Deform/DMMotionDeform.cpp" 'hasMatchingSampleLayout'
require_regex "src/modules/mesh/DMesh/Deform/DMMotionDeform.cpp" 'sampled topology changed'
require_regex "src/modules/mesh/DMesh/Deform/DMMotionDeform.cpp" 'normalizedVector'
forbid_regex "src/modules/mesh/DMesh/Deform/DMMotionDeform.cpp" 'std::vector<std::vector<DPointList>>[[:space:]]+ptsArray\(sampling'
forbid_regex "src/modules/mesh/DMesh/Deform/DMMotionDeform.cpp" 'movNormVec\.normalize\(\)'
require_regex "src/modules/curve/DCurve/Utility/DCDelete.cpp" 'buildWorldLineDeletionPlan'
require_regex "src/modules/curve/DCurve/Utility/DCurveDuplicatePlan.h" 'kMaxWorkItemCount'
forbid_regex "src/modules/curve/DCurve/Utility/DCDelete.cpp" 'getCurveIndex\('
forbid_regex "src/modules/curve/DCurve/Utility/DCDelete.cpp" 'std::find\(deleteIds'
forbid_regex "src/modules/curve/DCurve/Utility/DCDelete.cpp" 'fmin\('
for eyelash_source in \
  "src/modules/curve/DCurve/Generate/DCEyelashGen.cpp" \
  "src/modules/generators/Generate/Human/DHEyelashGen.cpp"; do
  require_regex "$eyelash_source" 'buildEyelashGenerationPlan'
  require_regex "$eyelash_source" 'generateRandomDoubleArray'
  require_regex "$eyelash_source" 'sampleFiniteRamp'
  require_regex "$eyelash_source" 'normalizedVector'
  forbid_regex "$eyelash_source" 'DNoiseOp\.h'
done
require_regex "src/core/common/object/DDeformMath.h" 'bool[[:space:]]+deformBendWithCvs'
require_regex "src/core/common/object/DDeformMath.h" 'bool[[:space:]]+deformBend\('
require_regex "src/core/common/object/DDeformMath.cpp" 'pointCount[[:space:]]*<[[:space:]]*2U'
require_regex "src/core/common/object/DDeformMath.cpp" 'weights\.length\(\)'
require_regex "src/core/common/object/DDeformMath.cpp" 'copyCheckedArray\(cvs, result\)'
require_regex "src/core/common/object/DDeformMath.cpp" 'copyCheckedArray\(result, cvs\)'
forbid_regex "src/core/common/object/DDeformMath.cpp" 'cvs\.length\(\)[[:space:]]*-[[:space:]]*1'
forbid_regex "src/modules/dynamics/DBullet/solver_impl.h" 'import_bullet_file'
forbid_regex "src/modules/dynamics/DBullet/DB_BulletSolver_t.cpp" 'fprintf\(stderr'
require_regex "src/modules/dynamics/DBullet/solver_impl.h" 'virtual[[:space:]]+bool[[:space:]]+export_bullet_file'
require_regex "src/modules/dynamics/DBullet/DB_BulletSolver_t.cpp" 'MGlobal::displayError'
forbid_regex "src/modules/curve/DCurve/Shape/DCurveShape.cpp" 'const_cast'
require_regex "src/modules/curve/DCurve/Shape/DCurveShape.h" 'const DCurveData\*[[:space:]]+geometry\(\)[[:space:]]+const'
forbid_regex "src/modules/mesh/DMesh/Data/DMeshData.cpp" 'const_cast'
forbid_regex "src/modules/dynamics/DBullet/dbSolver_t.cpp" 'const_cast'
forbid_regex "src/modules/dynamics/DBullet/Constraint/DB_Sixdof_Constraint_t.h" 'const_cast'
forbid_regex "src/modules/dynamics/DBullet/Constraint/DB_Slider_Constraint_t.h" 'const_cast'
forbid_regex "src/core/Data/DCurveData.h" 'getCurveIndex'
forbid_regex "src/modules/mesh/DMesh/Data/DMeshData.h" 'getMeshIndex'
require_regex "src/modules/deformers/Deformer/DRayDeformer.cpp" 'closestIntersection'
require_regex "src/modules/deformers/Deformer/DRayDeformer.cpp" 'rayTypeValue'
for collider_deformer in DRayDeformer DCollisionDeformer; do
  require_regex "src/modules/deformers/Deformer/${collider_deformer}.h" 'inObjectMatrix'
  require_regex "src/modules/deformers/Deformer/${collider_deformer}.cpp" 'inObjectMatrix'
  require_regex "src/modules/deformers/Deformer/${collider_deformer}.cpp" 'worldTo(Target|Collider)'
done
forbid_regex "src/modules/deformers/Deformer/DCollisionDeformer.cpp" 'getTransformNodeFromPlug'
require_regex "src/modules/deformers/Deformer/DCollisionDeformer.cpp" 'transformAsNormal'
require_regex "src/modules/deformers/Deformer/DCollisionDeformer.cpp" 'jumpToElement\(geomIndex\)'
require_regex "src/modules/deformers/Deformer/DCollisionDeformer.cpp" 'kMaxWorkItemCount'
require_regex "src/modules/deformers/Deformer/DCollisionDeformer.cpp" 'gridCellCount'
require_regex "src/modules/volume/VDB/Common/DVdbParticleRasterPlan.h" 'checkedParticleRasterWork'
require_regex "src/modules/volume/VDB/Common/DVdbCombinePlan.h" 'buildCombinePlan'
require_regex "src/modules/volume/VDB/Edit/DOpenVDBCombine.cpp" 'buildCombinePlan'
require_regex "src/modules/volume/VDB/Edit/DOpenVDBCombine.cpp" '"Divide",[[:space:]]*kDivide'
forbid_regex "src/modules/volume/VDB/Edit/DOpenVDBCombine.cpp" 'grids[AB]\[0\]'
require_regex "src/modules/volume/VDB/Generate/DOpenVDBFromParticle.cpp" 'findPlug\(velocityArray,[[:space:]]*true,[[:space:]]*&stat\)'
require_regex "src/modules/volume/VDB/Generate/DOpenVDBFromParticle.cpp" 'maxGrid->setName\(maskVolumeNameVal\.asChar\(\)\)'
require_regex "src/modules/volume/VDB/Generate/DOpenVDBFromDP.cpp" 'radiusArray\[i\] = 1\.0'
require_regex "src/modules/volume/VDB/Common/DOpenVDBCommon.h" \
  'mHasRadius\(rArray\.length\(\)[[:space:]]*!=[[:space:]]*0U\)'
require_regex "src/modules/volume/VDB/Common/DOpenVDBCommon.h" \
  'scaleRadiusMultiplier'
forbid_regex "src/modules/volume/VDB/Common/DOpenVDBCommon.h" \
  'assert\(mHas(Radius|Velocity)\)|bool[[:space:]]+use(Radius|Velocity)|DVdbParticleList'
require_regex "src/modules/volume/VDB/Generate/DVDBFoamGen.cpp" 'acc\.setValue\(ijk,[[:space:]]*distance\)'
forbid_regex "src/modules/volume/VDB/Generate/DVDBFoamGen.cpp" 'worldToIndexCellCentered\(ijk'
forbid_regex "src/modules/volume/VDB/Common/DOpenVDBCommon.cpp" 'setRmax\(1e15f\)'
require_regex "src/modules/dynamics/DBullet/SoftBody/DBSoftBodyNode.cpp" 'm_soft_body->set_parameters\(parameters\)'
require_regex "src/modules/dynamics/DBullet/SoftBody/DBSoftBodyNode.cpp" 'readSoftBodyParameters\(parameters,[[:space:]]*&data\)'
require_regex "src/modules/dynamics/DBullet/Solver/DBSolverNode.cpp" 'sbNode->readSoftBodyParameters\(parameters\)'
forbid_regex "src/modules/dynamics/DBullet/SoftBody/DBSoftBodyNode.h" 'm_numClusters'
require_regex "src/modules/dynamics/DBullet/RigidBody/DBRigidBodyArrayNode.cpp" \
  'kMaxGeneratedTransformCount'
require_regex "src/modules/dynamics/DBullet/RigidBody/DBRigidBodyArrayNode.cpp" \
  'dommy::generation::checkedCount'
for bullet_update_source in \
  "src/modules/dynamics/DBullet/RigidBody/DBRigidBodyNode.cpp" \
  "src/modules/dynamics/DBullet/RigidBody/DBRigidBodyArrayNode.cpp" \
  "src/modules/dynamics/DBullet/SoftBody/DBSoftBodyNode.cpp" \
  "src/modules/dynamics/DBullet/Constraint/DBNailConstraintNode.cpp" \
  "src/modules/dynamics/DBullet/Constraint/DBHingeConstraintNode.cpp" \
  "src/modules/dynamics/DBullet/Constraint/DBSliderConstraintNode.cpp" \
  "src/modules/dynamics/DBullet/Constraint/DBSixdofConstraintNode.cpp"; do
  require_regex "$bullet_update_source" 'MStatus[[:space:]]+[[:alnum:]_:]+::update\(\)'
done
require_regex "src/modules/dynamics/DBullet/Solver/DBSolverNode.cpp" \
  'MStatus DBSolverNode::updateAllRigidBodies\(\)'
forbid_regex "src/modules/dynamics/DBullet/SoftBody/soft_body_impl_t.h" 'set_inertia|set_restitution'
forbid_regex "src/modules/dynamics/DBullet/SoftBody/DB_SoftBody_t.cpp" 'set_inertia|set_restitution'
forbid_regex "src/modules/mesh/DMesh/Dynamics/Bullet/commonBulletSoftBody.h" 'void[[:space:]]+set_(inertia|restitution)'
require_regex "src/modules/dynamics/DBullet/Constraint/DB_Slider_Constraint_t.h" 'setDampingOrthoAng'
require_regex "src/modules/dynamics/DBullet/Constraint/DB_Sixdof_Constraint_t.h" 'getRotationalLimitMotor\(axis\)->m_damping'
require_regex "src/core/common/file/DSequencePath.h" 'replaceAll'
require_regex "src/core/common/file/DSequencePath.h" 'digits\.imbue\(std::locale::classic\(\)\)'
require_regex "src/core/common/file/DSequencePath.h" 'numeric::checkedNumericCast'
require_regex "src/core/common/file/DSequencePath.h" 'if \(!digits\) return false;'
require_regex "src/core/common/file/DSequencePath.h" 'if \(!value\) return false;'
forbid_regex "src/core/common/file/DSequencePath.h" 'static_cast<long long>|result\.clear\(\)'
require_regex "src/core/common/file/DAtomicFile.h" 'path\.imbue\(std::locale::classic\(\)\)'
require_regex "src/core/common/file/DAtomicFile.h" 'if \(!path\) throw std::runtime_error'
require_regex "src/modules/curve/DCurve/Cmd/Abc/DCAbcWriteJob.cpp" 'if \(!value\) return false;'
require_regex "src/modules/curve/DCurve/Cmd/Abc/DCAbcWriteJob.cpp" 'if \(!scalar \|\| !array\) return false;'
for grid_name_source in \
  "src/modules/volume/VDB/Generate/DOpenVDBFromDP.cpp" \
  "src/modules/volume/VDB/Edit/DOpenVDBFracture.cpp"; do
  require_regex "$grid_name_source" 'name\.imbue\(std::locale::classic\(\)\)'
  require_regex "$grid_name_source" 'if \(!name\) throw std::runtime_error'
done
require_regex "src/core/common/file/DLegacyTextData.h" 'values\.imbue\(std::locale::classic\(\)\)'
require_regex "src/core/common/file/DLegacyTextData.h" 'values\.exceptions\(std::ios::badbit\)'
require_regex "src/core/common/file/DLegacyTextData.h" 'readFiniteDoubleToken\(values, value\)'
require_regex "src/core/common/file/DLegacyTextData.h" 'readNonnegativeSizeToken\(values, count\)'
require_regex "src/core/common/utility/DIndexPattern.h" 'numeric::parseUnsignedDecimal\(token, value\)'
require_regex "src/core/common/utility/DUnsignedDecimal.h" 'character < .0. \|\| character > .9.'
require_regex "src/core/Utility/DUtility.h" 'value == std::numeric_limits<T>::min\(\)'
require_regex "src/core/Utility/DUtility.h" 'numeric::checkedNumericCast\(value \* factor, result\)'
require_regex "src/core/Utility/DUtility.h" 'legacyInterpolation\(a, b, t, Checked\{\}\)'
require_regex "src/core/Utility/DUtility.h" 'return dommy::text::checkedStreamText\(ss,'
require_regex "src/core/common/string/DStreamFormat.h" 'if \(!stream\) throw std::runtime_error'
require_regex "src/core/Utility/DUtility.h" 'math::normalizeRange\(x, xmin, xmax, dx\)'
require_regex "src/core/Utility/DUtility.h" 'std::copy\(candidate.begin\(\), candidate.end\(\), col\)'
require_regex "src/core/common/utility/DIntegerBlend.h" 'finishIntegerBlend'
require_regex "src/core/common/file/DNumericTextRead.h" 'if \(!delimiter\) return NumericTokenRead::Invalid;'
require_regex "src/modules/point/DPoint/Duplicator/DPInstancer.cpp" 'readFiniteDoubleToken\(values, \*coordinate, true\)'
require_regex "src/core/DDataUtility/IO/DPointDataExportCmd.cpp" 'serialized\.imbue\(std::locale::classic\(\)\)'
require_regex "src/core/DDataUtility/Data/DPData.cpp" 'encoded\.imbue\(std::locale::classic\(\)\)'
require_regex "src/modules/point/DPoint/Duplicator/DPInstancer.cpp" 'values\.imbue\(std::locale::classic\(\)\)'
for locale_source in \
  "src/core/common/file/DLegacyTextData.h" \
  "src/core/common/file/DSequencePath.h" \
  "src/core/common/file/DAtomicFile.h" \
  "src/modules/curve/DCurve/Cmd/Abc/DCAbcWriteJob.cpp" \
  "src/modules/volume/VDB/Generate/DOpenVDBFromDP.cpp" \
  "src/modules/volume/VDB/Edit/DOpenVDBFracture.cpp" \
  "src/core/DDataUtility/Data/DPData.cpp" \
  "src/modules/point/DPoint/Duplicator/DPInstancer.cpp" \
  "src/core/DDataUtility/IO/DPointDataExportCmd.cpp"; do
  forbid_regex "$locale_source" 'std::locale::global'
done
require_regex "tests/cpp/math_memory_tests.cpp" 'runIoLocaleTests\(\);'
require_regex "CMakeLists.txt" 'tests/cpp/io_locale_tests.cpp'
require_regex "tests/run_cpp_tests.sh" 'tests/cpp/io_locale_tests.cpp'
require_regex "src/core/DDataUtility/IO/DPointDataExportCmd.cpp" 'output\.flush\(\)'
require_regex "src/core/DDataUtility/IO/DPointDataExportCmd.cpp" 'PendingOutputFile'
require_regex "src/core/DDataUtility/IO/DPointDataExportCmd.cpp" \
  'isSafePathSyntax\(outputPath\)'
require_regex "src/core/DDataUtility/IO/DPointDataExportCmd.h" \
  'class[[:space:]]+DPointDataExportCmd[[:space:]]+final'
forbid_regex "src/core/DDataUtility/IO/DPointDataExportCmd.cpp" 'ofstream[[:space:]]+output\(filePath'
require_regex "src/modules/dynamics/DBullet/Solver/DBSolverNode.cpp" 'PendingOutputFile'
forbid_regex "src/modules/dynamics/DBullet/Solver/DBSolverNode.cpp" 'ofstream[[:space:]]+out\(file_name'
require_regex "src/modules/dynamics/DBullet/DB_BulletSolver_t.cpp" 'PendingOutputFile'
forbid_regex "src/modules/dynamics/DBullet/DB_BulletSolver_t.cpp" 'fopen\(fileName'
for stream_writer in \
  "src/core/DDataUtility/IO/DPointDataExportCmd.cpp" \
  "src/modules/dynamics/DBullet/Solver/DBSolverNode.cpp" \
  "src/modules/dynamics/DBullet/DB_BulletSolver_t.cpp"; do
  require_regex "$stream_writer" 'pending\.stream\(\)'
  require_regex "$stream_writer" 'pending\.close\(\)'
  forbid_regex "$stream_writer" '[Tt]emporaryPath\(\)'
done
for sdk_writer in \
  "src/modules/volume/VDB/IO/DWriteOpenVDB.cpp" \
  "src/modules/point/DPoint/Utility/DPCache.cpp" \
  "src/modules/curve/DCurve/Cmd/DExportAbcCmd.cpp"; do
  require_regex "$sdk_writer" 'Access::TrustedPath'
  require_regex "$sdk_writer" 'trustedTemporaryPath\(\)'
done
require_regex "src/modules/point/DPoint/Utility/DPCache.cpp" 'PendingOutputFile'
forbid_regex "src/modules/point/DPoint/Utility/DPCache.cpp" 'std::rename\('
require_regex "src/modules/curve/DCurve/Cmd/DExportAbcCmd.cpp" 'PendingAbcWriteJob'
require_regex "src/modules/curve/DCurve/Cmd/DExportAbcCmd.cpp" 'buildAbcSamplingPlan'
require_regex "src/modules/curve/DCurve/Cmd/DExportAbcCmd.cpp" 'kMaxExportSampleEvents'
require_regex "src/modules/curve/DCurve/Cmd/DExportAbcCmd.cpp" 'isSafeExportPath\(fileName,[[:space:]]*"\.abc"\)'
forbid_regex "src/modules/curve/DCurve/Cmd/DExportAbcCmd.cpp" 'workspace[[:space:]]+-s|std::ofstream|frame[[:space:]]*\+=[[:space:]]*range->strideTime'
require_regex "src/core/common/utility/DAbcSamplingPlan.h" \
  'setup\(\) writes both schemas'
require_regex "src/core/common/utility/DAbcSamplingPlan.h" \
  'kAbcAcyclicTimeSampling'
require_regex "src/modules/curve/DCurve/Cmd/Abc/DCAbcWriteJob.cpp" \
  'processBestEffortCallback'
require_regex "src/modules/curve/DCurve/Cmd/DExportAbcCmd.cpp" \
  'Unable to read the active Maya selection'
require_regex "src/modules/curve/DCurve/Cmd/Abc/DCAbcWriteJob.cpp" \
  'validFrameSet'
require_regex "src/modules/curve/DCurve/Cmd/Abc/DCAbcWriteJob.cpp" \
  'mFirstFrame[[:space:]]*=[[:space:]]*\*mTransFrames\.begin'
require_regex "src/modules/curve/DCurve/Cmd/Abc/DCAbcWriteJob.cpp" \
  'must share the setup frame'
forbid_regex "src/modules/curve/DCurve/Cmd/Abc/DCAbcWriteJob.cpp" \
  'assert[[:space:]]*\('
if [[ -e "$repo_root/src/core/math/DVector.h" ]]; then
  echo "Retired unused vector templates returned" >&2
  exit 1
fi
forbid_regex "src/core/math/dommyMath.h" 'smooth_step|randhash[fd]|inline[[:space:]]+long[[:space:]]+lround'
require_regex "src/core/math/dommyMath.h" \
  'namespace[[:space:]]+dommy[[:space:]]*\{'
require_regex "src/core/math/dommyMath.h" \
  'namespace[[:space:]]+math[[:space:]]*\{'
require_regex "src/core/math/dommyMath.h" \
  'bool[[:space:]]+intersectSegmentAabb'
forbid_regex "src/core/math/dommyMath.h" \
  'using[[:space:]]+std::(min|max|swap)'
forbid_regex "src/modules/dynamics/DBullet/mvl/mathUtils.h" \
  'std::sqrt\([[:space:]]*sqr[[:space:]]*\('
if grep -R -nE \
  --include='*.cpp' --include='*.h' --include='*.inl' \
  '(^|[^[:alnum:]_])(toRad|toDo|distanceBetween|pointDistanceBetween|isHitLineInBBox|GetIntersection|InBox)[[:space:]]*\(' \
  "$repo_root/src"; then
  echo "Removed global math API is still referenced under src/." >&2
  exit 1
fi
if grep -R -nE \
  --include='*.cpp' --include='*.h' --include='*.inl' \
  --exclude='dommyMath.h' \
  '(^|[^[:alnum:]_:])(orderBounds|isInsideUnitCircle)[[:space:]]*\(' \
  "$repo_root/src"; then
  echo "Shared math helper is used outside dommy::math." >&2
  exit 1
fi
require_regex "src/core/Data/DCurveData.cpp" 'hasValidCurveTopology'
require_regex "src/core/Data/DCurveData.cpp" 'std::isfinite'
require_regex "src/modules/curve/DCurve/Generate/DCurveFromDP.cpp" 'ptArray\.size\(\)[[:space:]]*>[[:space:]]*1U'
forbid_regex "src/modules/curve/DCurve/Generate/DCurveFromDP.cpp" 'inknotStep'
forbid_regex "src/core/common/string/DStringOp.cpp" 'res\[res\.size\(\)[[:space:]]*-[[:space:]]*1\]'
forbid_regex "src/core/common/string/DStringOp.cpp" 'long long[[:space:]]+userInput[[:space:]]*=[[:space:]]*number'
require_regex "src/modules/dynamics/DBullet/File/pdbIO.cpp" 'std::isfinite\(m_time\)'
require_regex "src/modules/dynamics/DBullet/File/pdbIO.cpp" 'kMaxParticleCount'
require_regex "src/core/common/file/DLegacyTextData.h" 'pointDataSectionCountMatches'
require_regex "src/core/DDataUtility/IO/DPointDataExportCmd.cpp" 'kPointRecordsPerLine'
require_regex "src/core/DDataUtility/IO/DPointDataExportCmd.cpp" 'kMaxLegacyTextRecords[[:space:]]*-[[:space:]]*totalPointCount'
require_regex "src/modules/volume/VDB/DReservedVdbTypeIds.cpp" 'DOpenVDBCreate::typeId\(ID_VDB_DOPENVDB_CREATE\)'
require_regex "src/plugin/DReservedTypeIds.cpp" 'DPAttributePromote::typeId\(ID_DP_ATTR_PROMOTE\)'
require_regex "src/plugin/DReservedTypeIds.cpp" 'BTHingeConstraint::typeId\(ID_BT_HINGE_CONSTRAINT\)'
require_regex "src/plugin/DReservedTypeIds.cpp" 'BTSliderConstraint::typeId\(ID_BT_SLIDER_CONSTRAINT\)'
forbid_regex "src/modules/mesh/DMesh/Dynamics/Bullet/commonDBulletConstraint.h" \
  'BTHingeConstraint|BTSliderConstraint'
for viewport_contract in \
  'src/modules/point/DPoint/Display/DPointDisplay.h:DPointDisplayOverride' \
  'src/modules/point/DPoint/Display/DPMarker.h:DPMarkerOverride' \
  'src/modules/rendering/Render/Camera/DCameraFrustum.h:DCameraFrustumOverride' \
  'src/modules/mesh/DMesh/Display/DMeshDisplay.h:DMeshDisplayOverride' \
  'src/modules/mesh/DMesh/Display/DMMarker.h:DMMarkerOverride' \
  'src/modules/curve/DCurve/Display/DCurveDisplay.h:DCurveDisplayOverride' \
  'src/modules/curve/DCurve/Display/DCMarker.h:DCMarkerOverride' \
  'src/modules/point/DPoint/Duplicator/DPInstancer.h:DPInstancerOverride'; do
  viewport_file="${viewport_contract%%:*}"
  viewport_class="${viewport_contract##*:}"
  require_regex "$viewport_file" \
    "class[[:space:]]+$viewport_class[[:space:]]+final[[:space:]]*:"
done
forbid_regex "src/modules/point/DPoint/Duplicator/DPInstancer.h" \
  'virtual[[:space:]]+(~DPInstancerOverride|MHWRender::DrawAPI|bool|void)'
require_regex "src/core/DDataUtility/Particle/nobject/DPnPopSolver.cpp" 'fieldMassFromInverseMass'
require_regex "src/core/DDataUtility/Particle/nobject/DPnPopSolver.cpp" 'fieldForce\.length\(\)[[:space:]]*!=[[:space:]]*numPoint'
require_regex "src/core/DDataUtility/Particle/nobject/DPnPopSolver.cpp" 'integrateFieldVelocity'
forbid_regex "src/core/DDataUtility/Particle/nobject/DPnPopSolver.cpp" 'masses\[i\][[:space:]]*=[[:space:]]*inverseMass'
forbid_regex "src/core/DDataUtility/Particle/nobject/DPnPopSolver.cpp" 'DP_NPOP_SOLVER_DEBUG[[:space:]]*=[[:space:]]*1'
require_regex "src/modules/point/DPoint/mesh/DPVoronoi.h" 'class[[:space:]]+DPVoronoi[[:space:]]+final'
require_regex "src/modules/point/DPoint/mesh/DPVoronoi.h" 'schedulingType\(\)[[:space:]]+const[[:space:]]+override[[:space:]]*\{[[:space:]]*return[[:space:]]+kSerial'
require_regex "src/modules/point/DPoint/mesh/DPVoronoi.cpp" 'kMaxVoronoiSeedCount'
require_regex "src/modules/point/DPoint/mesh/DPVoronoi.cpp" 'DPointLexicographicLess'
require_regex "src/modules/point/DPoint/mesh/DPVoronoi.cpp" 'DOMMY_COMPUTE_CATCH\("DPVoronoi"\)'
require_regex "src/modules/point/DPoint/mesh/DPVoronoi.cpp" 'kMaxGeneratedPointCount'
require_regex "src/modules/point/DPoint/mesh/DPVoronoi.cpp" 'checkedSum'
require_regex "src/modules/point/DPoint/mesh/DPVoronoi.cpp" 'canRepresentAsFloat\(1\.0[[:space:]]*\+[[:space:]]*cellSpace\)'
forbid_regex "src/modules/point/DPoint/mesh/DPVoronoi.cpp" 'MFnMesh[[:space:]]+tempFn\(meshObj\)'
forbid_regex "src/modules/point/DPoint/mesh/DPVoronoi.cpp" 'std::find\(tmpPos'
forbid_regex "src/modules/point/DPoint/mesh/DPVoronoi.cpp" 'DPointDataHeaders|DCurveDataHeaders|DMeshDataHeaders'
forbid_regex "src/core/common/object/DVoroUtil.cpp" 'insideContainer\(_con,[[:space:]]*\*_pnt\)[[:space:]]*;'
for legacy_emitter in \
  "src/modules/particles/DEmitter/DEmitter" \
  "src/modules/curve/curveEmitter/curveEmitter"; do
  require_regex "${legacy_emitter}.h" 'final[[:space:]]*:[[:space:]]*public[[:space:]]+MPxEmitterNode'
  require_regex "${legacy_emitter}.h" 'schedulingType\(\)[[:space:]]+const[[:space:]]+override[[:space:]]*\{[[:space:]]*return[[:space:]]+kSerial'
  require_regex "${legacy_emitter}.cpp" 'kMaxGeneratedPointCount'
  require_regex "${legacy_emitter}.cpp" 'checkedProduct'
  require_regex "${legacy_emitter}.cpp" 'RandomGenerator'
  require_regex "${legacy_emitter}.cpp" 'isConnected\(&status\)'
  require_regex "${legacy_emitter}.cpp" 'checkedLinearBlend'
  forbid_regex "${legacy_emitter}.cpp" 'MFnDependencyNode|MFnMatrixData|useRotation|emitCountPerPoint'
done
require_regex "src/modules/particles/DEmitter/DEmitter.cpp" 'DOMMY_COMPUTE_CATCH\("DEmitter"\)'
require_regex "src/modules/curve/curveEmitter/curveEmitter.cpp" 'DOMMY_COMPUTE_CATCH\("curveEmitter"\)'
require_regex "src/modules/particles/DEmitter/DEmitter.cpp" 'checkedPointMean'
require_regex "src/modules/point/DPoint/Utility/DPPointToFloat.cpp" \
  'MFnFloatArrayData'
forbid_regex "src/modules/point/DPoint/Utility/DPPointToFloat.cpp" \
  'MFnDoubleArrayData'
require_regex "src/modules/point/DPoint/Utility/DPBlend.cpp" \
  'targetCount[[:space:]]*==[[:space:]]*0'
require_regex "src/modules/point/DPoint/Utility/DPBlend.cpp" \
  '%[[:space:]]*targetCount'
forbid_regex "src/modules/curve/DCurve/Utility/DCNormal.cpp" \
  'MVector[[:space:]]+baseVec[[:space:]]*\([[:space:]]*baseVec'
require_regex "src/modules/point/DPoint/Deform/DPConvertWeights.cpp" \
  'outputArrayValue\(aWeightsList'
forbid_regex "src/modules/point/DPoint/Deform/DPConvertWeights.cpp" \
  'constructHandle|wPlug\.setValue'
require_regex "src/modules/point/DPoint/nurbs/DPUVGen.cpp" \
  'dommy::uv::normalizeParameter'
forbid_regex "src/modules/point/DPoint/nurbs/DPUVGen.cpp" \
  'tbb::parallel_for'
require_regex "src/modules/point/DPoint/Modify/DPEditDouble.cpp" \
  'child\(DPEditDouble::dataValue\)'
forbid_regex "src/modules/curve/curveEmitter/curveEmitter.cpp" 'for[[:space:]]*\([[:space:]]*int[[:space:]]+j[[:space:]]*=[[:space:]]*0;[[:space:]]*j[[:space:]]*<[[:space:]]*rate'
for point_emitter in \
  DPointPlaneEmit \
  DPointVolumeEmit \
  DPointSurfaceEmit \
  DPointCircleEmit; do
  require_regex "src/core/DDataUtility/Generate/${point_emitter}.h" "class[[:space:]]+${point_emitter}[[:space:]]+final"
  require_regex "src/core/DDataUtility/Generate/${point_emitter}.h" \
    'schedulingType\(\)[[:space:]]+const[[:space:]]+override'
  require_regex "src/core/DDataUtility/Generate/${point_emitter}.h" \
    'return[[:space:]]+kSerial'
done

if [[ -e "$repo_root/src/modules/volume/VDB/Generate/DOpenVDBCreate.cpp" ||
      -e "$repo_root/src/modules/volume/VDB/Generate/DOpenVDBCreate.h" ]]; then
  echo "Incomplete DOpenVDBCreate source returned to the publication tree." >&2
  exit 1
fi

for arnold_writer in \
  "src/core/DDataUtility/IO/DPWriteToAi.cpp" \
  "src/modules/curve/DCurve/IO/DWriteDCurveToAi.cpp" \
  "src/modules/point/DPoint/render/light/DPLightGen.cpp"; do
  require_regex "$arnold_writer" 'dommy::arnold::Session'
  require_regex "$arnold_writer" 'dommy::maya::getEvaluationTime'
  forbid_regex "$arnold_writer" 'AiBegin\(|AiEnd\(|MAnimControl::currentTime'
done

forbid_regex "src/plugin/registration/modules/PointRegistrations.inl" 'DPWriteToAi|DPInstancer|DPLightGen'
forbid_regex "src/plugin/registration/modules/CurveRegistrations.inl" 'DWriteDCurveToAi'

require_regex "src/modules/point/DPoint/render/light/DPLightGen.h" 'MObjectHandle[[:space:]]+mNodeHandle'
require_regex "src/modules/point/DPoint/render/light/DPLightGen.cpp" 'mNodeHandle\.isValid\(\)[[:space:]]*&&[[:space:]]*ovr->mNodeHandle\.isAlive\(\)'
require_regex "src/modules/point/DPoint/render/light/DPLightGen.cpp" 'OnModelEditorChanged,[[:space:]]+this,[[:space:]]+&status\)'
forbid_regex "src/modules/point/DPoint/render/light/DPLightGen.cpp" 'mLightGen|node\.userNode\('
forbid_regex "src/modules/point/DPoint/Common/DPointSeExpr.h" 'static[[:space:]]+DPData[[:space:]]*\*[[:space:]]*m_dpoint'
require_regex "src/modules/point/DPoint/Common/DPointSeExpr.h" 'evaluationSucceeded\(\)[[:space:]]+const'
require_regex "src/modules/point/DPoint/Common/DPointSeExpr.h" 'static_cast<long double>\(value\)'
require_regex "src/core/DDataUtility/Attribute/DPAttributeSeExpr.cpp" 'MStatus[[:space:]]+evaluateSeExpr'
require_regex "src/core/DDataUtility/Attribute/DPAttributeSeExpr.cpp" 'SeExprParameters[[:space:]]+parameters'
require_regex "src/core/DDataUtility/Attribute/DPAttributeSeExpr.cpp" 'dommy::expression::evaluateResults'
require_regex "src/core/common/utility/DExpressionResult.h" 'evaluateChecked\(expression, value\)'
require_regex "src/core/common/utility/DExpressionEvaluation.h" 'expression\.evaluationSucceeded\(\)'
forbid_regex "src/core/DDataUtility/Attribute/DPAttributeSeExpr.h" 'm_user_(float|vector|color)|m_expr_str|m_current_time'
forbid_regex "src/core/DDataUtility/Group/DPGroup.cpp" 'outputDPData\.get\(\)'

require_regex "src/core/DDataUtility/Particle/emitter/DPFluidEmitter.h" '^#pragma once|^#ifndef '
require_regex "src/core/DDataUtility/Particle/nobject/DPnPopCommon.h" '^#pragma once|^#ifndef '
require_regex "src/core/noise/Poisson/PoissonGenerator.h" '^#pragma once|^#ifndef '
require_regex "src/modules/mesh/DMesh/Dynamics/Bullet/DBulletFractureSolver.h" '^#pragma once|^#ifndef '

# Publication hardening for legacy Maya mesh/utility nodes.
require_regex "src/core/common/utility/DMayaMeshInput.h" 'checkedMatchingPointCount'
require_regex "src/core/common/utility/DSymmetryPointMatcher.h" 'nearestMirrored'
require_regex "src/core/common/utility/DIndexPattern.h" 'maximumOutputCount'
require_regex "src/core/common/utility/DIndexPattern.h" 'if \(!stream\.eof\(\) \|\| stream\.bad\(\)\) return false;'
require_regex "src/core/UtilityNode/DNumericToFloatArray/DNumericToFloatArray.cpp" 'kInt64'
require_regex "src/core/UtilityNode/DNumericToFloatArray/DNumericToFloatArray.cpp" 'appendMatrix\(inputHandle\.asMatrix\(\),[[:space:]]*output\)'
require_regex "src/core/UtilityNode/DNumericToFloatArray/DNumericToFloatArray.cpp" 'outputFitsBudget'
require_regex "src/core/UtilityNode/DComponentListGen/DComponentListGen.cpp" 'parseIndexPattern'
require_regex "src/modules/point/DPoint/Modify/DPEditDouble.cpp" \
  'parseIndexPattern'
require_regex "src/modules/point/DPoint/Modify/DPEditDouble.cpp" \
  'dataIndexValue[[:space:]]*<[[:space:]]*0'
require_regex "src/core/DDataUtility/Utility/DPFillSpace.cpp" \
  'buildFillSpacePlan'
require_regex "src/core/DDataUtility/Utility/DPFillSpace.cpp" \
  'reserveFillSpacePairWork'
require_regex "src/core/common/utility/DFillSpacePlan.h" \
  'kMaxWorkItemCount'
require_regex "src/modules/mesh/mesh/DPolyThickness.cpp" 'autoUniformGridParams'
require_regex "src/modules/mesh/mesh/DPolyThickness.cpp" 'closestIntersection'
for symmetry_source in \
  "src/modules/mesh/mesh/DBSFlipMesh.cpp" \
  "src/modules/mesh/mesh/DPolySymmetry.cpp"; do
  require_regex "$symmetry_source" 'getMirroredPointMatches'
  forbid_regex "$symmetry_source" 'DPointOp|openvdb'
done
require_regex "src/core/common/utility/DViewportCallbackGuard.h" 'class[[:space:]]+ScopedUIDrawable[[:space:]]+final'
require_regex "src/modules/rendering/Render/Camera/DCameraFrustum.cpp" 'dynamic_cast<const DCameraFrustumData\*>'
require_regex "src/modules/rendering/Render/Camera/DCameraFrustum.cpp" 'frustumBounds'
require_regex "src/modules/point/DPoint/render/light/DPLightGen.cpp" 'dynamic_cast<const DPLightGenData\*>'
forbid_regex "src/modules/point/DPoint/render/light/DPLightGen.cpp" '\(DPLightGenData\*\)data'
forbid_regex "src/core/common/object/DPointOp.h" 'openvdb|DOpenVDB|ParticleAtlas'
require_regex "src/core/common/object/DPointVdbSearch.h" 'ParticleAtlas'
require_regex "CMakeLists.txt" 'Include OpenVDB data, volume, cache, scatter, and spatial-search nodes'
require_regex "cmake/DommyFullSourceCheck.cmake" 'DVdbOp\|DPAttributeVDB\|DPAttrSmooth\|DPScatter'

# Bounded procedural generators and deterministic noise utilities.
require_regex "src/modules/point/DPoint/generate/data/DPStringArrayGen.cpp" 'kMaxGeneratedStringByteCount'
require_regex "src/modules/point/DPoint/generate/data/DPStringArrayGen.cpp" 'MStringArray[[:space:]]+newStringArray'
forbid_regex "src/modules/point/DPoint/generate/data/DPStringArrayGen.cpp" '&defaultString|&defaultStr'
require_regex "src/modules/curve/DCurve/Generate/DCurveClothFromSurface.cpp" 'checkedClothSamplingPlan'
forbid_regex "src/modules/curve/DCurve/Generate/DCurveClothFromSurface.cpp" 'tbb::(parallel_for|task_group)'
for wire_source in \
  "src/modules/mesh/mesh/DPolyWire.cpp" \
  "src/modules/mesh/DMesh/Generate/DMWireGen.cpp"; do
  require_regex "$wire_source" 'checkedWireTopologyPlan'
  require_regex "$wire_source" 'copyWireTopologyToMaya'
done
require_regex "src/modules/curve/legacy/DSpiralCurve.cpp" 'buildSpiralCurvePlan'
require_regex "src/modules/curve/legacy/DSpiralCurvePlan.h" 'checkedCount'
require_regex "src/modules/curve/legacy/DSpiralCurve.cpp" 'knotCount'
require_regex "src/modules/mesh/DMesh/Utility/DMSubDivide.cpp" 'checkedSubdivisionBudget'
require_regex "src/modules/mesh/DMesh/Utility/DMSubDivide.cpp" 'setMax\(dommy::mesh::kMaxSubdivisionLevel\)'
require_regex "src/modules/mesh/DMesh/Utility/DMSubDivide.cpp" 'refineEdges'
require_regex "src/modules/mesh/DMesh/Utility/DMSubDivide.cpp" 'appendSubdivisionBudget'
require_regex "src/plugin/PluginFeatures.h" '^#define DOMMY_WITH_OPENVDB 0$'
require_regex "src/core/noise/PerlinNoise.h" 'kMaxPerlinOctaves'
require_regex "src/core/noise/PerlinNoise.cpp" 'splitCoordinate'
require_regex "src/core/noise/PerlinNoise.cpp" 'valid_'
forbid_regex "src/core/noise/PerlinNoise.cpp" 'time\(|srand|rand\('

# Historical motion sampling and transactional Maya array outputs.
require_regex "src/core/common/utility/DDeformerMath.h" 'MotionSampleBlend'
require_regex "src/core/common/utility/DDeformerMath.h" 'motionSampleBlend'
require_regex "src/modules/deformers/Deformer/DMotionDeformer.cpp" 'motionSampleBlend'
require_regex "src/modules/mesh/DMesh/Deform/DMMotionDeform.cpp" 'motionSampleBlend'
require_regex "src/core/common/utility/DMotionGenerationPlan.h" 'buildMotionTrailBudget'
require_regex "src/core/common/utility/DMotionGenerationPlan.h" 'motionTrailStep'
require_regex "src/core/common/utility/DMotionGenerationPlan.h" 'buildMotionDuplicationBudget'
require_regex "src/core/common/utility/DMayaHistoricalSampling.h" 'origin\.unit\(\)'
require_regex "src/core/common/utility/DMayaHistoricalSampling.h" 'sampleScalarPlugSeries'
require_regex "src/core/common/utility/DMayaHistoricalSampling.h" 'sampleMatrixPlugSeries'
require_regex "src/core/common/utility/DMayaObjectArrayOutput.h" 'builder\.addElement'
require_regex "src/core/common/utility/DMayaObjectArrayOutput.h" 'output\.setAllClean'
require_regex "src/modules/curve/Curve/Node/DMotionTrailCurveNode.cpp" 'setObjectArrayOutput'
require_regex "src/modules/generators/Generate/motionDuplicator.cpp" 'setObjectArrayOutput'
require_regex "src/modules/generators/Generate/motionDuplicator.cpp" 'evaluationTime,[[:space:]]*-delayValue,[[:space:]]*-delayValue'
require_regex "src/modules/generators/Generate/motionDuplicator.cpp" '-globalDelay,[[:space:]]*-globalDelay'
require_regex "src/modules/generators/Generate/motionDuplicator.cpp" 'setRotatePivot'
require_regex "src/modules/generators/Generate/motionDuplicator.cpp" 'setScalePivot'
require_regex "src/modules/generators/Generate/motionDuplicator.cpp" 'baseTranslation'
require_regex "src/modules/generators/Generate/motionDuplicator.cpp" 'baseRotation'
require_regex "src/modules/generators/Generate/motionDuplicator.cpp" 'baseScale'
require_regex "src/modules/generators/Generate/motionDuplicator.cpp" '\?[[:space:]]*baseMatrix'
require_regex "src/modules/deformers/transform/DTDelayTransformMatrix.cpp" '-delayValue,[[:space:]]*0\.0'
require_regex "src/modules/generators/Generate/Human/DHEyelashGen.cpp" 'setObjectArrayOutput'
require_regex "src/modules/curve/DCurve/Convert/DCToCurves.cpp" 'setObjectArrayOutput'
require_regex "src/modules/generators/legacy/surfaceDuplicater.cpp" 'validTextureSamples'
require_regex "src/modules/generators/legacy/surfaceDuplicater.cpp" 'connectedSourceNode'
require_regex "src/modules/generators/legacy/surfaceDuplicater.cpp" 'isFloatRepresentablePoint'
require_regex "src/modules/generators/legacy/surfaceDuplicater.cpp" 'DOMMY_RETURN_IF_FAILURE\(animMatPlug\.getValue'

# Transactional index transforms, signed VDB display normalization, and
# release-quiet Maya diagnostics.
require_regex "src/core/common/utility/DIndexTransform.h" 'isValidPermutation'
require_regex "src/core/common/utility/DIndexTransform.h" 'normalizeCyclicOffset'
require_regex "src/modules/point/DPoint/Utility/DPSort.cpp" 'normalizeCyclicOffset'
forbid_regex "src/modules/point/DPoint/Utility/DPSort.cpp" 'labs[[:space:]]*\('
require_regex "src/modules/volume/VDB/Common/DVdbViewportMath.h" 'normalizedSignedWeight'
require_regex "src/modules/volume/VDB/Common/DOpenVDBUtil.h" 'normalizedSignedWeight'
require_regex "src/core/Debug/DMayaDebug.h" 'DOMMY_MAYA_DEBUG'
require_regex "src/modules/curve/DCurve/Generate/DCText.cpp" 'class FreeTypeLibrary final'
require_regex "src/modules/curve/DCurve/Generate/DCText.cpp" 'class FreeTypeFace final'
require_regex "src/modules/curve/DCurve/Generate/DCText.cpp" 'buildTextGenerationPlan'
require_regex "src/modules/curve/DCurve/Generate/DCText.h" 'class[[:space:]]+DCText[[:space:]]+final'
forbid_regex "src/modules/curve/DCurve/Generate/DCText.h" \
  'm_(Text|Font|Size|Sampling|Tracking|Group|Face|Library)'
require_regex "src/core/common/object/DPointOp.h" \
  'std::vector<size_t>[[:space:]]+ret_indices'
require_regex "src/core/common/object/DPointOp.h" \
  'ret_indices\.data\(\),[[:space:]]*out_distances_sqr\.data\(\)'
for spatial_source in \
  "src/modules/mesh/DMesh/Utility/DMConnetedPieces.cpp" \
  "src/modules/mesh/DMesh/Utility/DMConnectTargets.cpp" \
  "src/modules/mesh/DMesh/Group/DMGroupFromDP.cpp" \
  "src/modules/curve/DCurve/Group/DCGroupFromDP.cpp"; do
  require_regex "$spatial_source" 'buildSpatialSearchPlan'
done
require_regex "src/modules/point/DPoint/Utility/DPSearchPoint.cpp" \
  'PointSearchKDTreeNearestWithDistanceOp'
for transfer_source in \
  "src/core/DDataUtility/Attribute/DPAttributeTransfer.cpp" \
  "src/modules/curve/DCurve/Attribute/DCAttributeTransfer.cpp" \
  "src/modules/mesh/DMesh/Attribute/DMAttributeTransfer.cpp"; do
  require_regex "$transfer_source" 'buildAttributeTransferPlan'
  require_regex "$transfer_source" 'DPointAttributeTransfer\.h'
done
for procedural_source in \
  "src/modules/curve/Curve/Node/DArcCurveNode.cpp" \
  "src/modules/curve/Curve/Node/DRigController.cpp"; do
  require_regex "$procedural_source" 'DProceduralMeshPlans\.h'
done
for foam_source in \
  "src/modules/point/DPoint/mesh/DPFoamMesh.cpp" \
  "src/modules/mesh/mesh/foamGenerateNode.cpp"; do
  require_regex "$foam_source" 'DIcosahedronTopology\.h'
done
require_regex "src/modules/volume/VDB/IO/DWriteOpenVDB.cpp" 'aWriteFile'
require_regex "src/modules/volume/VDB/IO/DWriteOpenVDB.cpp" 'isSafeExportPath'
require_regex "src/modules/volume/VDB/IO/DWriteOpenVDB.cpp" \
  'getPluginDataFromHandle'
require_regex "src/modules/point/DPoint/Utility/DPCache.h" \
  'class[[:space:]]+DPCache[[:space:]]+final'
require_regex "src/modules/point/DPoint/Utility/DPCache.cpp" \
  'aCreateMissingCache'
require_regex "src/modules/point/DPoint/Utility/DPCache.cpp" \
  'isSafeExportPath\(filename,[[:space:]]*"\.vdb"\)'
for arnold_writer in \
  "src/core/DDataUtility/IO/DPWriteToAi.cpp" \
  "src/modules/curve/DCurve/IO/DWriteDCurveToAi.cpp" \
  "src/modules/point/DPoint/render/light/DPLightGen.cpp"; do
  require_regex "$arnold_writer" 'isSafeExportPath'
  require_regex "$arnold_writer" '"\.ass"'
done
require_regex "src/modules/point/DPoint/render/light/DPLightGen.cpp" \
  '#if DOMMY_WITH_DG_FILE_WRITERS'
require_regex "src/modules/dynamics/DBullet/Solver/DBSolverNode.cpp" \
  '#if DOMMY_WITH_DG_FILE_WRITERS'
require_regex "src/modules/dynamics/DBullet/Solver/DBSolverNode.cpp" \
  'isSafeExportPath\(file_name,[[:space:]]*"\.pdb"\)'
require_regex "src/modules/dynamics/DBullet/DB_BulletSolver_t.cpp" \
  'isSafePathSyntax\(fileName\)'
require_regex "tools/fetch_legacy_dependency_sources.sh" \
  'nanoflann-1\.2\.3-f2efec01'
require_regex "tests/check_legacy_dependency_headers.sh" \
  'nanoflann_legacy_api_compile_test\.cpp'
require_regex "src/plugin/PluginFeatures.h" \
  '^#define DOMMY_WITH_DG_FILE_WRITERS 0$'
forbid_regex "src/modules/point/DPoint/Common/DPAttributeCommon.h" 'std::(cout|cerr)'
require_regex "src/core/common/object/DNucleusSolverControls.h" 'substeps[[:space:]]*<=[[:space:]]*0'
require_regex "src/core/DDataUtility/Particle/nobject/DPnParticle.cpp" 'solveNucleus<DPnParticle>'
require_regex "src/core/DDataUtility/Particle/nobject/DPnPopSolver.cpp" 'solveNucleus<DPnPopSolver>'
require_regex "src/core/DDataUtility/Particle/nobject/DPnPopSolver.cpp" 'nAttr\.setMin\(1\)'
require_regex "src/core/common/object/DNucleusCollisionUtils.h" \
  'inputArrayValue\(attribute,[[:space:]]*&status\)'
require_regex "src/core/common/object/DNucleusCollisionUtils.h" \
  'rigidObjects\.swap\(collected\)'
require_regex "src/core/common/object/DNucleusCollisionUtils.h" \
  'removeAllCollisions\(\)'
require_regex "src/core/common/object/DNucleusCollisionUtils.h" \
  'registeredColliders\.pop_back\(\)'
require_regex "src/core/common/object/DNucleusCollisionUtils.h" \
  'registeredColliders\.push_back\(collider\)'
for nucleus_node in \
  "src/core/DDataUtility/Particle/nobject/DPnParticle.cpp" \
  "src/core/DDataUtility/Particle/nobject/DPnPopSolver.cpp"; do
  require_regex "$nucleus_node" 'syncNucleusRigidCollisions'
  require_regex "$nucleus_node" 'MStatus[[:space:]]+[^:]+::ensureNParticle'
  require_regex "$nucleus_node" 'setTopology\(static_cast<int>'
  forbid_regex "$nucleus_node" '\.getValue\('
  forbid_regex "$nucleus_node" 'calcPopSolver|RayIntersectionPoint'
done
forbid_regex "src/core/DDataUtility/Particle/nobject/DPnPopSolver.cpp" \
  'generateDPointDataFromNParticle'
require_regex "src/core/DDataUtility/Particle/nobject/DPnParticle.cpp" \
  'attributeAffects\(input,[[:space:]]*prevDP\)'
require_regex "src/core/DDataUtility/Particle/nobject/DPnParticle.cpp" \
  'attributeAffects\(input,[[:space:]]*outDP\)'
require_regex "src/core/DDataUtility/Particle/nobject/DPnPopSolver.cpp" \
  'attributeAffects\(input,[[:space:]]*outDP\)'
# WoolCurve and the other command rollback contracts are exercised by complete
# production bodies in check_command_transactions.py, not variable-name regexes.
require_regex "src/commands/Cmd/Metadata/DMetadataSelectCmd.cpp" \
  'class BoundedMetadataReport'
require_regex "src/commands/Cmd/Metadata/DMetadataSelectCmd.cpp" \
  'metadata report truncated'
forbid_regex "src/commands/Cmd/BlindData/DBlindDataSelectCmd.cpp" \
  'pointlight'

require_regex "src/core/common/utility/DGenerationLimits.h" \
  'kMaxCurveSampleCount'
require_regex "src/core/common/utility/DGenerationLimits.h" \
  'kMaxConnectedGeometryCount'
require_regex "src/modules/curve/legacy/woolCurve.cpp" \
  'buildWoolCurvePlan'
require_regex "src/modules/curve/legacy/woolCurve.cpp" \
  'for[[:space:]]*\(std::size_t sample'
forbid_regex "src/modules/curve/legacy/woolCurve.cpp" \
  'while[[:space:]]*\([[:space:]]*i[[:space:]]*<[[:space:]]*eRadiusVal'
require_regex "src/modules/point/DPoint/math/DVectorNodeMath.h" \
  'target\[broadcast \? 0U : offset \+ i\]'
forbid_regex "src/modules/point/DPoint/math/vector/DPCrossProduct.cpp" \
  '\.normalized\(\)'
forbid_regex "src/modules/point/DPoint/math/vector/DPDotProduct.cpp" \
  '\.normalized\(\)'
forbid_regex "src/modules/point/DPoint/math/vector/DPVecAngle.cpp" \
  '\.normalized\(\)'
require_regex "src/modules/point/DPoint/math/DPMathUtility.h" \
  'normalized3OrZero'
require_regex "src/core/DDataUtility/Particle/nobject/DPnPopBase.cpp" \
  'return MS::kUnknownParameter'
require_regex "src/core/common/utility/DMetaDataUtility.h" \
  'findMetadataStructure'
require_regex "src/modules/curve/Curve/Node/DCurveResampleUtils.h" \
  'sampleCurveByLength'
require_regex "src/modules/curve/Curve/Node/DCurveResampleUtils.h" \
  'MSpace::kObject'
require_regex "src/modules/curve/Curve/Node/DCurveResampleUtils.h" \
  'outputCurve[[:space:]]*=[[:space:]]*rebuiltCurve'
require_regex "src/modules/curve/Curve/Node/DBSCurve.cpp" \
  'buildLengthBlendedCurve'
require_regex "src/modules/curve/Curve/Node/DResampleCurve.cpp" \
  'buildLengthResampledCurve'
forbid_regex "src/modules/curve/Curve/Node/DBSCurve.cpp" \
  'findParamFromLength|getPointAtParam|MFnNurbsCurveData'
forbid_regex "src/modules/curve/Curve/Node/DResampleCurve.cpp" \
  'findParamFromLength|getPointAtParam|MFnNurbsCurveData'

require_regex "src/core/common/utility/DMayaPluginDataOutput.h" \
  'createPluginDataObject'
require_regex "src/core/common/utility/DFeedbackSolver.h" \
  'computeFeedbackSolver'
require_regex "src/core/common/utility/DFeedbackSolver.h" \
  'const MObject originalPrimary'
for feedback_solver in \
  "src/modules/point/DPoint/Solver/DPSolver.cpp" \
  "src/modules/curve/DCurve/Solver/DCSolver.cpp" \
  "src/modules/mesh/DMesh/Solver/DMSolver.cpp"; do
  require_regex "$feedback_solver" 'computeFeedbackSolver'
  forbid_regex "$feedback_solver" 'MFnPluginData'
  require_regex "$feedback_solver" 'attributeAffects\(aCurrentTime'
done

require_regex "src/core/DataModifier/ShapeSequencer/DShapeSequencer.cpp" \
  'kMaxConnectedGeometryCount'
require_regex "src/modules/generators/Generate/DCurveDuplicator.cpp" \
  'kMaxConnectedGeometryCount'
require_regex "src/modules/generators/Generate/DBubbleCrystal.cpp" \
  'arcNoise\.SetSeed\(seedVal\)'
require_regex "src/modules/generators/Generate/DBubbleCrystal.cpp" \
  'arcNoise\.SetFrequency\(arcNoiseFrequencyVal\)'
require_regex "src/modules/generators/Generate/DBubbleCrystal.cpp" \
  'heightNoise\.SetSeed\(seedVal[[:space:]]*\+[[:space:]]*1\)'
require_regex "src/modules/generators/Generate/DBubbleCrystal.cpp" \
  'heightNoise\.SetFrequency\(heightNoiseFrequencyVal\)'

require_regex "src/modules/mesh/polyModifier/polyBlast/polyBlastNode.cpp" \
  'readSelectedFaces'
require_regex "src/modules/mesh/polyModifier/polyBlast/polyBlastNode.cpp" \
  '"inputComponents"'
require_regex "src/modules/mesh/polyModifier/polyBlast/polyBlastNode.cpp" \
  'pBlastFactory\.setFaces\(faceIds\)'
require_regex "src/modules/mesh/polyModifier/polyBlast/polyBlastNode.cpp" \
  'meshFunction\.copy\(inputMesh,[[:space:]]*outputData'
require_regex "src/modules/mesh/polyModifier/polyBlast/polyBlastCmd.cpp" \
  'component\.hasFn\(MFn::kMeshPolygonComponent\)'
require_regex "src/modules/mesh/polyModifier/polyBlast/polyBlastCmd.cpp" \
  'readPolygonFaceComponent'
require_regex "src/modules/mesh/polyModifier/polyBlast/polyBlastCmd.cpp" \
  'buildPercentageFaceSelection'
require_regex "src/modules/mesh/polyModifier/polyBlast/polyBlastSelection.h" \
  'isComplete\(&status\)'
require_regex "src/modules/mesh/polyModifier/polyBlast/polyBlastSelection.h" \
  'getCompleteData\(completeCount\)'
forbid_regex "src/modules/mesh/polyModifier/polyBlast/polyBlastCmd.cpp" \
  'kMeshMapComponent|validateFaces|pruneUVs|splitUV'
require_regex "src/modules/mesh/polyModifier/polyBlast/polyBlastAction.cpp" \
  'status[[:space:]]*=[[:space:]]*meshFunction\.deleteFace'
require_regex "src/modules/mesh/polyModifier/polyBlast/polyBlastAction.cpp" \
  'normalizeFaceSelection'
forbid_regex "src/modules/mesh/polyModifier/polyBlast/polyBlastNode.cpp" \
  'output\.set\(input\.asMesh\(\)\)'

require_regex "src/modules/curve/DCurve/Utility/DCColor.cpp" \
  'colorAddTypeVal[[:space:]]*==[[:space:]]*dommy::color::kExpression'
require_regex "src/modules/curve/DCurve/Utility/DCColor.cpp" \
  'Expression mode is retained for scene'
forbid_regex "src/modules/curve/DCurve/Utility/DCColor.cpp" \
  'DPointSeExpr|outputDPData'
for color_node in \
  "src/modules/point/DPoint/Utility/DPColor.cpp" \
  "src/modules/curve/DCurve/Utility/DCColor.cpp" \
  "src/modules/mesh/DMesh/Utility/DMColor.cpp"; do
  require_regex "$color_node" 'DColorMode\.h'
  require_regex "$color_node" 'isValidMode\(colorAddTypeVal\)'
  require_regex "$color_node" 'isFiniteTriple'
  require_regex "$color_node" 'isOrderedFiniteRange'
  require_regex "$color_node" 'DColorPalette\.h'
  require_regex "$color_node" 'dommy::color::readPalette\('
  forbid_regex "$color_node" 'notExistCurve|countMissing'
done
require_regex "src/core/common/utility/DColorPalette.h" 'count[[:space:]]*==[[:space:]]*0U'
require_regex "src/core/common/utility/DColorPalette.h" 'validArrayConversionBudget'
require_regex "src/core/common/utility/DColorPalette.h" 'buildCheckedArray'
for assigned_color_node in \
  "src/modules/curve/DCurve/Utility/DCColor.cpp" \
  "src/modules/mesh/DMesh/Utility/DMColor.cpp"; do
  require_regex "$assigned_color_node" \
    'requiresPointAssignment\(colorAddTypeVal\)'
done
require_regex "src/modules/rendering/global/render/DGlobalTex.cpp" \
  'status[[:space:]]*=[[:space:]]*outputU\.set\('
require_regex "src/modules/rendering/global/render/DGlobalTex.cpp" \
  'static_cast<float>\(uCoordinates\[index\]\)'
require_regex "src/modules/rendering/global/render/DGlobalTex.cpp" \
  'points\.length\(\)[[:space:]]*!=[[:space:]]*uCoordinates\.length\(\)'
forbid_regex "src/modules/rendering/global/render/DGlobalTex.cpp" \
  'MFloatArray[[:space:]]+uarray\([^;]*,[[:space:]]*0\.0\)|MFnDependencyNode|M3dView'

require_regex "src/modules/point/DPoint/math/DPMathUtility.cpp" \
  'From Attribute is not available for Maya array output'
require_regex "src/modules/point/DPoint/math/DPMathUtility.h" \
  'MStatus[[:space:]]+processMayaNumericMathFuncForVector'
for vector_getter in \
  DPMGetDPointVectorArray \
  DPMGetDCurvePrimVectorArray \
  DPMGetDCurvePointVectorArray \
  DPMGetDMeshPrimVectorArray \
  DPMGetDMeshPointVectorArray; do
  forbid_regex "src/modules/point/DPoint/math/DPMathUtility.h" "$vector_getter"
  forbid_regex "src/modules/point/DPoint/math/DPMathUtility.cpp" "$vector_getter"
done
require_regex "src/modules/point/DPoint/math/DPMathUtility.cpp" \
  'readConstantVectorTarget'
forbid_regex "src/modules/point/DPoint/math/DPMathUtility.cpp" \
  'readVectorArrayTarget|readTargetAttributeName|validateVectorAttributeResult|isValidVectorTargetType|DMayaVecArrayToVecVectorList'
forbid_regex "src/core/common/data/DDataOp.h" 'DScalarVectorListToVectorList'
require_regex "src/modules/point/DPoint/generate/data/DPDoubleArrayGen.cpp" \
  'dommy::math::checkedLinearInterpolation'
forbid_regex "src/modules/point/DPoint/generate/data/DPDoubleArrayGen.cpp" \
  'static_cast<float>\(generated\)'
for scalar_ramp_source in \
  "src/core/DDataUtility/Data/DPIntArrayGen.cpp" \
  "src/modules/point/DPoint/generate/data/DPDoubleArrayGen.cpp"; do
  require_regex "$scalar_ramp_source" 'rampValue == 0.0f \? minimum :'
  require_regex "$scalar_ramp_source" 'rampValue == 1.0f \? maximum :'
done
for vector_node in DPVecAngle DPDotProduct DPCrossProduct; do
  require_regex "src/modules/point/DPoint/math/vector/${vector_node}.cpp" \
    'computeVectorNode'
done

require_regex "src/modules/mesh/DMesh/Dynamics/Bullet/commonDBulletConstraint.h" \
  'type[[:space:]]*!=[[:space:]]*DMbtRigid::typeId.*return[[:space:]]+MS::kInvalidParameter'
require_regex "src/modules/deformers/Deformer/noiseDeformer.cpp" \
  'directionSeed[[:space:]]*=[[:space:]]*dommy::math::randhash'
forbid_regex "src/modules/deformers/Deformer/noiseDeformer.cpp" \
  'normVec\.[xyz][[:space:]]*=[[:space:]]*random\('
forbid_regex "src/modules/curve/DCurve/Cmd/Abc/DMeshWriter.cpp" \
  'uv_samp|N_samp|if[[:space:]]*\([^)]*hasUV\('
forbid_regex "src/modules/volume/VDB/Display/DOpenVDBDisplayVP2.cpp" \
  '(leafColorBuffer|tileColorBuffer)->acquire'

require_regex "src/core/DDataUtility/Attribute/Convert/DPAttributeToDataArray.cpp" \
  'DMayaArrayOutput\.h'
require_regex "src/modules/point/DPoint/animation/DPAnimMatrix.cpp" \
  'DNodeOperation\.h'
require_regex "src/modules/curve/DCurve/DCConvertFromCurves.cpp" \
  'MFnDependencyNode\.h'
for standard_random_source in \
  "src/modules/curve/DCurve/Generate/DCurveLinkGen.cpp" \
  "src/modules/curve/DCurve/Utility/DCCharactor.cpp"; do
  require_regex "$standard_random_source" \
    'std::uniform_real_distribution'
  forbid_regex "$standard_random_source" \
    'boost::(mt19937|uniform_real)'
done

require_regex "src/modules/point/DPoint/animation/DPMatrix.cpp" \
  'kMaxGeneratedTransformCount'
require_regex "src/modules/point/DPoint/animation/DPMatrix.cpp" \
  'rotateAmplitude\[2\]'
require_regex "src/modules/point/DPoint/animation/DPMatrix.cpp" \
  'scaleAmplitude\[2\]'
for bounded_generation_source in \
  "src/modules/curve/DCurve/Generate/DCurveLinkGen.cpp" \
  "src/modules/curve/DCurve/Utility/DCFillBetween.cpp" \
  "src/modules/curve/DCurve/Utility/DCReplicator.cpp" \
  "src/modules/curve/DCurve/Utility/DCResample.cpp" \
  "src/modules/curve/DCurve/Utility/DCReform.cpp" \
  "src/modules/point/DPoint/Utility/DPDuplicator.cpp" \
  "src/modules/mesh/DMesh/Generate/DMTimeShapeGen.cpp"; do
  require_regex "$bounded_generation_source" \
    'dommy::generation::checked(Product|Count|Sum)'
  require_regex "$bounded_generation_source" \
    '(createPluginData(Object)?|setPluginDataOutput)'
done

for serial_sampling_header in \
  "src/modules/point/DPoint/animation/DPAnimMatrix.h" \
  "src/modules/point/DPoint/animation/DPMotionCreate.h" \
  "src/modules/point/DPoint/animation/DPRotation.h" \
  "src/modules/mesh/DMesh/Generate/DMTimeShapeGen.h"; do
  require_regex "$serial_sampling_header" \
    'schedulingType\(\)[[:space:]]+const[[:space:]]+override[[:space:]]*\{[[:space:]]*return[[:space:]]+kSerial'
done
require_regex "src/core/common/utility/DMayaHistoricalSampling.h" \
  'sampleMatrixPlugAtTime'
require_regex "src/core/common/utility/DMayaHistoricalSampling.h" \
  'sampleVectorPlugAtTime'
for historical_sampling_source in \
  "src/modules/point/DPoint/animation/DPAnimMatrix.cpp" \
  "src/modules/point/DPoint/animation/DPMotionCreate.cpp" \
  "src/modules/point/DPoint/animation/DPRotation.cpp"; do
  require_regex "$historical_sampling_source" \
    'computeSampleTime'
  require_regex "$historical_sampling_source" \
    'createPluginDataObject'
  forbid_regex "$historical_sampling_source" \
    'getTimeDGContext'
done
require_regex "src/modules/mesh/DMesh/Generate/DMTimeShapeGen.cpp" \
  'historicalSampleTime'
require_regex "src/modules/mesh/DMesh/Generate/DMTimeShapeGen.cpp" \
  'sampling[[:space:]]*-[[:space:]]*1U'

require_regex "src/modules/mesh/DMesh/Dynamics/Bullet/DCRest.h" \
  'class[[:space:]]+DCRest[[:space:]]+final'
require_regex "src/modules/mesh/DMesh/Dynamics/Bullet/DCRest.cpp" \
  'createPluginDataObject<DCurveData>'
require_regex "src/modules/mesh/DMesh/Dynamics/Bullet/DCRest.cpp" \
  'hasValidCurveTopology'
require_regex "src/modules/mesh/DMesh/Dynamics/Bullet/DCRest.cpp" \
  'addAttr\([[:space:]]*restMatrixName'
require_regex "src/modules/mesh/DMesh/Dynamics/Bullet/DCRest.cpp" \
  'output\.set\(outputDataObject\)'
forbid_regex "src/modules/mesh/DMesh/Dynamics/Bullet/DCRest.cpp" \
  'output\.set\(outDCurves\)|addAttr\("restmat"'

require_regex "src/modules/mesh/DMesh/Dynamics/Utility/DMRest.h" \
  'class[[:space:]]+DMRest[[:space:]]+final'
require_regex "src/modules/mesh/DMesh/Dynamics/Utility/DMRest.cpp" \
  'createPluginDataObject<DMeshData>'
require_regex "src/modules/mesh/DMesh/Dynamics/Utility/DMRest.cpp" \
  'hasValidMeshTopology'
require_regex "src/modules/mesh/DMesh/Dynamics/Utility/DMRest.cpp" \
  'type[[:space:]]*<[[:space:]]*0[[:space:]]*\|\|[[:space:]]*type[[:space:]]*>[[:space:]]*1'
require_regex "src/modules/mesh/DMesh/Dynamics/Utility/DMRest.cpp" \
  'isFiniteMatrix'
require_regex "src/modules/mesh/DMesh/Dynamics/Utility/DMRest.cpp" \
  'DOMMY_RETURN_IF_FAILURE\(\(\*mesh\)->bakeTransform\(matrix\)\)'
require_regex "src/modules/mesh/DMesh/Utility/DMCopyTransform.cpp" \
  'DOMMY_RETURN_IF_FAILURE\(copied\.bakeTransform\(finMat\)\)'
require_regex "src/modules/mesh/DMesh/Utility/DMCopyTransform.cpp" \
  'DOMMY_RETURN_IF_FAILURE\(outDMeshes->copyRepeatedFrom\(\*inputDMData, copies, operation\)\)'
require_regex "src/modules/mesh/DMesh/Dynamics/Utility/DMRest.cpp" \
  'output\.set\(outputDataObject\)'
forbid_regex "src/modules/mesh/DMesh/Dynamics/Utility/DMRest.cpp" \
  'output\.set\(outDMeshes\)|output\.set\(pluginData\.object\(\)\)'

require_regex "src/modules/mesh/DMesh/Utility/DMDelete.h" \
  'class[[:space:]]+DMDelete[[:space:]]+final'
require_regex "src/modules/mesh/DMesh/Utility/DMDelete.cpp" \
  'type[[:space:]]*!=[[:space:]]*D_COMP_PRIM[[:space:]]*&&[[:space:]]*type[[:space:]]*!=[[:space:]]*D_COMP_FACE'
require_regex "src/modules/mesh/DMesh/Utility/DMDelete.cpp" \
  'else[[:space:]]+if[[:space:]]*\(type[[:space:]]*==[[:space:]]*D_COMP_FACE\)'
require_regex "src/modules/mesh/DMesh/Utility/DMDelete.cpp" \
  '\+\+mesh,[[:space:]]*\+\+meshIndex'
require_regex "src/modules/mesh/DMesh/Utility/DMDelete.cpp" \
  'hasValidMeshTopology'
require_regex "src/modules/mesh/DMesh/Utility/DMDelete.cpp" \
  'output\.set\(outputDataObject\)'
forbid_regex "src/modules/mesh/DMesh/Utility/DMDelete.cpp" \
  'else[[:space:]]+if[[:space:]]*\(D_COMP_FACE\)'

require_regex "src/modules/point/DPoint/Utility/DPGoal.cpp" \
  'data\.inputValue\(aTargetDP,[[:space:]]*&status\)'
require_regex "src/modules/point/DPoint/Utility/DPGoal.cpp" \
  'targetDPData->numberOfPoints\(\)[[:space:]]*!='
require_regex "src/modules/point/DPoint/Utility/DPGoal.cpp" \
  'computeGoalVelocityComponent'
require_regex "src/modules/point/DPoint/Utility/DPGoal.cpp" \
  'DPVec3fAttr'
require_regex "src/modules/point/DPoint/Utility/DPGoal.cpp" \
  'hasValidPointTopology'

require_regex "src/core/common/utility/DIndexPattern.h" \
  'mergeIndexPatternSelection'
for pattern_source in \
  "src/modules/point/DPoint/Utility/DPDelete.cpp" \
  "src/modules/mesh/DMesh/Utility/DMDelete.cpp" \
  "src/core/common/utility/DGroupEvaluation.h"; do
  require_regex "$pattern_source" \
    'mergeIndexPatternSelection'
  forbid_regex "$pattern_source" \
    'isContainDData\(ptnarray'
done

# OpenVDB attribute transfer, viewport sampling and fracture hardening.
require_regex "src/core/Data/DGroupCommon.cpp" 'dommy::group::selectPattern'

require_regex "src/modules/volume/VDB/Common/DOpenVDBAttributeTransferUtil.h" \
  'class[[:space:]]+TransferAttributeData'
require_regex "src/modules/volume/VDB/Common/DOpenVDBAttributeTransferUtil.h" \
  'const[[:space:]]+std::string&[[:space:]]+attributeName\(\)[[:space:]]+const'
forbid_regex "src/modules/volume/VDB/Common/DOpenVDBAttributeTransferUtil.h" \
  'AttributeDetail|PointAttrTransfer|evalAttr'
require_regex "src/modules/volume/VDB/Generate/DOpenVDBFromDP.cpp" \
  'isValidClosestPointIndex'
require_regex "src/modules/volume/VDB/Generate/DOpenVDBFromDP.cpp" \
  'addAttributeSingle<openvdb::Int64,[[:space:]]*uint32_t>'
forbid_regex "src/modules/volume/VDB/Generate/DOpenVDBFromDP.cpp" \
  'attrListValue<AttrType>\(customName\)|constructGenericAtttributeList'
require_regex "src/modules/volume/VDB/Common/DOpenVDBUtil.h" \
  'const[[:space:]]+openvdb::Coord&[[:space:]]+ijk[[:space:]]*=[[:space:]]*\(\*mCoordinates\)\[n\]'
forbid_regex "src/modules/volume/VDB/Common/DOpenVDBUtil.h" \
  'Coord::round|mGrid->worldToIndex'
forbid_regex "src/modules/volume/VDB/Common/DOpenVDBUtil.h" \
  'voxelNum|ijk\[[012]\][[:space:]]*=[[:space:]]*int\(pos\[[012]\]\)'
require_regex "src/modules/volume/VDB/Common/DOpenVDBGeoUtil.h" \
  'static_cast<std::size_t>\(tmpIdx\)[[:space:]]*>=[[:space:]]*normalCount'
require_regex "src/modules/volume/VDB/Edit/DOpenVDBFracture.cpp" \
  'kMaxFractureInstanceCount'
require_regex "src/modules/volume/VDB/Edit/DOpenVDBFracture.cpp" \
  'getPoints\(vertexArray,[[:space:]]*MSpace::kObject\)'
require_regex "src/modules/volume/VDB/Edit/DOpenVDBFracture.cpp" \
  'vertexArray\[i\]\.x[[:space:]]*-[[:space:]]*cutterCenter\.x'
forbid_regex "src/modules/volume/VDB/Edit/DOpenVDBFracture.cpp" \
  'postTranslate\(mpos\)'

# DAG traversal, curve reconstruction and point-to-surface output contracts.
require_regex "src/core/common/object/DNodeOperation.h" \
  'MStatus[[:space:]]+getChildNodeFromNode'
require_regex "src/core/common/object/DNodeOperation.cpp" \
  'std::vector<MObject>[[:space:]]+pending'
require_regex "src/core/common/object/DNodeOperation.cpp" \
  'matrixPlug\.connectedTo\(connectedPlugs,[[:space:]]*true,[[:space:]]*false,[[:space:]]*&status\)'
forbid_regex "src/core/common/object/DNodeOperation.cpp" \
  '/\*if[[:space:]]*\(|MDGContext[[:space:]]+context\(currentTime-timeValueArray'
require_regex "src/modules/curve/DCurve/DCConvertFromCurves.cpp" \
  'getNurbsCurveNodeFromHierarchy'
forbid_regex "src/modules/curve/DCurve/DCConvertFromCurves.cpp" \
  'tcNode2|^[[:space:]]*//.*extractDagNodeFromTypeName'

require_regex "src/modules/curve/Curve/Node/remakeCurveNode.cpp" \
  'dommy::generation::checkedCount'
require_regex "src/modules/curve/Curve/Node/remakeCurveNode.cpp" \
  'gAttr\.setReadable\(false\)'
require_regex "src/modules/curve/Curve/Node/remakeCurveNode.cpp" \
  'getKnotDomain'
require_regex "src/modules/curve/Curve/Node/remakeCurveNode.cpp" \
  'isFinitePoint'
forbid_regex "src/modules/curve/Curve/Node/remakeCurveNode.cpp" \
  '^[[:space:]]*/\*|tAttr\.setReadable\(false\)|cPos[[:space:]]*\+[[:space:]]*offsetPosVal'

require_regex "src/modules/deformers/transform/DPointToSurfaceTransformNode.cpp" \
  'normalizedVector'
require_regex "src/modules/deformers/transform/DPointToSurfaceTransformNode.cpp" \
  'getClosestNormal'
require_regex "src/modules/deformers/transform/DPointToSurfaceTransformNode.cpp" \
  'positionOutput\.setClean\(\)'
require_regex "src/modules/deformers/transform/DPointToSurfaceTransformNode.cpp" \
  'normalOutput\.setClean\(\)'
require_regex "src/modules/deformers/transform/DPointToSurfaceTransformNode.cpp" \
  'requestedAttribute[[:space:]]*==[[:space:]]*outputPositionX'
require_regex "src/modules/deformers/transform/DPointToSurfaceTransformNode.cpp" \
  'closestDistanceSquared'
forbid_regex "src/modules/deformers/transform/DPointToSurfaceTransformNode.cpp" \
  'MSpace::kWorld'
forbid_regex "src/modules/deformers/transform/DPointToSurfaceTransformNode.cpp" \
  '^[[:space:]]*/\*|^[[:space:]]*//.*(cout|meshFn\.getClosestPoint|rayDirection)'

# Removed code must not remain as an unsafe implementation recipe.
forbid_regex "src/core/DDataUtility/Particle/nobject/DPnParticle.cpp" \
  '^[[:space:]]*/\*|std::auto_ptr|MFloatPointArrayPtr[[:space:]]+(pts|velArray|forceArray)'
require_regex "src/core/DDataUtility/Particle/nobject/DPnParticle.cpp" \
  'MFloatPointArray[[:space:]]+forceArray'
require_regex "src/core/DDataUtility/Particle/nobject/DPnParticle.cpp" \
  'ParticleScalarInput radius, mass'
require_regex "src/core/DDataUtility/Particle/nobject/DPnParticle.cpp" \
  'dommy::maya::createPluginData'
require_regex "src/core/DDataUtility/Particle/nobject/DPnParticle.cpp" \
  'emitted point count'
forbid_regex "src/core/DDataUtility/Particle/nobject/DPnParticle.cpp" \
  'plug[[:space:]]*==[[:space:]]*(nextDP|inDP)'
forbid_regex "src/core/DDataUtility/Generate/DPointPlaneEmit.cpp" \
  '^[[:space:]]*/\*|^[[:space:]]*//.*(boost::|openvdb::|pluginDataPointer|cout)|shrink_to_fit'
require_regex "src/core/DDataUtility/Generate/DPointPlaneEmit.cpp" \
  'placedPoints\.reserve\(countVal\)'
forbid_regex "src/modules/generators/Generate/DBubbleCrystal.cpp" \
  '^[[:space:]]*/\*|^[[:space:]]*//.*(cout|PerlinNoise3D|newMeshFn\.)'

# Curve noise and emitter cleanup must remain evaluation-safe and buildable.
require_regex "src/modules/curve/DCurve/Utility/DCNoise.cpp" \
  'getValidInputDC\(inDC,[[:space:]]*data,[[:space:]]*inputCurves\)'
require_regex "src/modules/curve/DCurve/Utility/DCNoise.cpp" \
  'dommy::maya::createPluginData'
require_regex "src/modules/curve/DCurve/Utility/DCNoise.cpp" \
  'params\.mSeed[[:space:]]*=[[:space:]]*baseSeed[[:space:]]*\+'
require_regex "src/modules/curve/DCurve/Utility/DCNoise.cpp" \
  'if[[:space:]]*\(isNoiseAttribute\)[[:space:]]*\{'
require_regex "src/modules/curve/DCurve/Utility/DCNoise.cpp" \
  'tangent\([[:space:]]*parameter,[[:space:]]*MSpace::kObject'
forbid_regex "src/modules/curve/DCurve/Utility/DCNoise.cpp" \
  '^[[:space:]]*/\*|cachedValueIsValid|params\.mSeed[[:space:]]*\+=|MSpace::kWorld|cout[[:space:]]*<<'
forbid_regex "src/modules/curve/DCurve/Utility/DCNoise.h" \
  'cachedValueIsValid|setDependentsDirty|preEvaluation'

forbid_regex "src/modules/curve/DCurve/Shape/DCurveShape.cpp" \
  '^[[:space:]]*/\*|DCurveShapeUI|glu(New|Delete)Quadric|beginGL\(\)'
forbid_regex "src/modules/curve/DCurve/Shape/DCurveShape.h" \
  'DCurveShapeUI|MPxSurfaceShapeUI'
forbid_regex "src/plugin/registration/modules/CurveRegistrations.inl" \
  'registerShape'

for cleaned_emitter_source in \
  "src/core/DDataUtility/Generate/DPointMeshEmit.cpp" \
  "src/core/DDataUtility/Generate/DPointVolumeEmit.cpp"; do
  forbid_regex "$cleaned_emitter_source" \
    '^[[:space:]]*/\*|^[[:space:]]*//.*(cout|boost::|MFnMatrixAttribute|MFloatArray|finPointList)'
done

# Custom data mutations must prepare a complete candidate before replacing
# live Maya plug-in data.
require_regex "src/modules/point/DPoint/Common/DPAttributeCommon.cpp" \
  'DPAttrList[[:space:]]+copiedAttributes'
require_regex "src/modules/point/DPoint/Common/DPAttributeCommon.cpp" \
  'baseAttrlist\.swap\(copiedAttributes\)'
require_regex "src/core/Data/DGroupCommon.cpp" \
  'DPGrpList[[:space:]]+copiedGroups'
require_regex "src/core/Data/DGroupCommon.cpp" \
  'baseGroup\.swap\(copiedGroups\)'
require_regex "src/core/DDataUtility/Data/DPData.cpp" \
  'bool DPData::deletePointFromIds'
require_regex "src/core/DDataUtility/Data/DPData.cpp" \
  'bool DPData::sortPointFromIds'
require_regex "src/core/DDataUtility/Data/DPData.cpp" \
  'DPData[[:space:]]+candidate'
forbid_regex "src/core/DDataUtility/Data/DPData.cpp" \
  'void DPData::(deletePointFromIds|sortPointFromIds)'
require_regex "src/core/Data/DCurveData.cpp" \
  'appendCurvesTransactional'
require_regex "src/core/Data/DCurveData.cpp" \
  'kMaxGeneratedCurveCount'
require_regex "src/core/Data/DCurveData.cpp" \
  'bool DCurveData::deleteFromIds'
require_regex "src/modules/mesh/DMesh/Data/DMeshData.cpp" \
  'bool DMeshInternalData::deleteFaceFromIds'
require_regex "src/modules/mesh/DMesh/Data/DMeshData.cpp" \
  'retainedFaceVertexIds'
require_regex "src/modules/mesh/DMesh/Data/DMeshData.cpp" \
  'MeshEdgeIndexMap'
require_regex "src/modules/mesh/DMesh/Data/DMeshData.cpp" \
  'bool DMeshData::deleteFromIds'
require_regex "src/modules/mesh/DMesh/Data/DMeshData.cpp" \
  'DMeshData::~DMeshData\(\)[[:space:]]*=[[:space:]]*default'
forbid_regex "src/modules/mesh/DMesh/Data/DMeshData.cpp" \
  'deleteFaceFromIdsLegacy|edgeCenterList'

# Attribute selection edits must dispatch through the base interface so every
# supported type, including DIntArray, participates automatically.
for attribute_edit in \
  deleteArrayFromIds \
  cropArrayFromIds \
  sortArrayFromIds \
  remapFromIds; do
  require_regex "src/core/DDataUtility/Data/DPAttributeData.h" \
    "virtual bool ${attribute_edit}"
  require_regex "src/modules/point/DPoint/Common/DPAttributeCommon.cpp" \
    "second->${attribute_edit}"
done
forbid_regex "src/modules/point/DPoint/Common/DPAttributeCommon.cpp" \
  'static_pointer_cast<[^>]+>[^;]*(deleteArrayFromIds|cropArrayFromIds|sortArrayFromIds|remapFromIds)'
require_regex "src/core/Data/DCurveData.cpp" \
  'DCurveData::addAttrWithValues does not support this type'
require_regex "src/modules/mesh/DMesh/Data/DMeshData.cpp" \
  'DMeshData::addDPAttributeValues does not support this type'

# Custom-data geometry replacement must stay atomic, value-validated, and
# unable to publish stale derived bounds.
require_regex "src/core/common/utility/DAttributeValidation.h" \
  'validateAttributeList'
require_regex "src/core/common/utility/DAttributeValidation.h" \
  'isFiniteMatrix'
require_regex "src/core/common/utility/DAttributeValidation.h" \
  'dynamic_pointer_cast'
require_regex "src/core/DDataUtility/Data/DPData.cpp" \
  'bool DPData::mergeDPData'
require_regex "src/core/DDataUtility/Data/DPData.cpp" \
  'bool DPData::addPoints'
require_regex "src/core/DDataUtility/Data/DPData.cpp" \
  'bool DPData::replacePositions'
for bounds_header in \
  "src/core/DDataUtility/Data/DPData.h" \
  "src/core/Data/DCurveData.h" \
  "src/modules/mesh/DMesh/Data/DMeshData.h"; do
  forbid_regex "$bounds_header" 'mutable (MBoundingBox|bool mBBoxDirty)'
  require_regex "$bounds_header" 'MBoundingBox calculateBoundingBox\(\) const'
  forbid_regex "$bounds_header" \
    'const MBoundingBox getBoundingBox\(\) const'
done
forbid_regex "src/core/Data/DPrimBaseData.h" \
  'mCenter(Dirty)?'
for point_data_header in \
  "src/core/Data/DPrimBaseData.h" \
  "src/core/DDataUtility/Data/DPData.h"; do
  forbid_regex "$point_data_header" 'getFirstPoint\('
  forbid_regex "$point_data_header" \
    'const DPointList (getPositionConst|getPointsInWorld)\('
done
require_regex "src/core/Data/DPrimBaseData.cpp" \
  'DPoint DPrimBaseData::getCenter\(\) const'
require_regex "src/core/Data/DCurveData.cpp" \
  'bool DCurveInternalData::setReplaceCurveData'
require_regex "src/core/Data/DCurveData.cpp" \
  'bool DCurveInternalData::setRemappedCurveData'
require_regex "src/core/Data/DCurveData.cpp" \
  'bool DCurveInternalData::replaceKnots'
forbid_regex "src/core/Data/DCurveData.h" \
  '(setKnots|setDegree|setCurveOpen)[[:space:]]*\('
forbid_regex "src/modules/curve/DCurve/Utility/DCResample.cpp" \
  'remapAtttributeFromIds|remapGroupFromIds|setKnots[[:space:]]*\('
require_regex "src/modules/mesh/DMesh/Data/DMeshData.h" \
  'enum class DMeshReplacementPolicy'
forbid_regex "src/modules/mesh/DMesh/Data/DMeshData.h" \
  'setReplaceMeshData[^;]*bool[[:space:]]+keepData'
require_regex "src/modules/mesh/DMesh/Data/DMeshData.cpp" \
  'bool DMeshInternalData::replacePositions'
for component_data_source in \
  "src/core/Data/DCurveData.cpp" \
  "src/modules/mesh/DMesh/Data/DMeshData.cpp"; do
  forbid_regex "$component_data_source" \
    '\*(AttributeListConst|AttributeList|GroupListConst|GroupList)\(cType\)'
  require_regex "$component_data_source" \
    '(attributes|groups) \?'
done
for component_data_header in \
  "src/core/Data/DCurveData.h" \
  "src/modules/mesh/DMesh/Data/DMeshData.h"; do
  require_regex "$component_data_header" \
    'bool addCompGrp\('
  require_regex "$component_data_header" \
    'bool removeCompGrp\('
done
require_regex "src/modules/mesh/DMesh/Data/DMeshData.h" \
  'bool addCompAttr\('
require_regex "src/modules/mesh/DMesh/Data/DMeshData.h" \
  'bool removeCompAttr\('
for const_safe_data_header in \
  "src/core/Data/DPrimBaseData.h" \
  "src/core/DDataUtility/Data/DPData.h" \
  "src/core/Data/DCurveData.h" \
  "src/modules/mesh/DMesh/Data/DMeshData.h"; do
  forbid_regex "$const_safe_data_header" \
    'DPAttributeData::Ptr[[:space:]]+getAttrPtr\([^;]*\)[[:space:]]+const'
done
forbid_regex "src/core/Data/DPrimBaseData.h" \
  'getTransformMatrixPtr'

# Staged Maya-owned custom data has one invariant-checking publication gate.
require_regex "src/core/common/utility/DMayaPluginData.h" \
  'bool validatePluginData\(const DataType& data\)'
require_regex "src/core/common/utility/DMayaPluginDataOutput.h" \
  'MStatus publishPluginDataObject'
require_regex "src/core/common/utility/DMayaPluginDataOutput.h" \
  'MStatus publishTypedPluginDataObject'
require_regex "src/core/common/utility/DMayaPluginDataOutput.h" \
  'validatePluginData\(\*outputData\)'
require_regex "src/modules/volume/VDB/Data/DOpenVDBData.cpp" \
  'bool DOpenVDBData::hasValidData\(\) const'

# The archived Bullet contact-clear operation had no implementation and must
# not reappear as a misleading solver command.
forbid_regex "src/modules/dynamics/DBullet/DM/DMBulletTest.h" \
  'DB_SLV_CLEAR_CONTACT_RB'
forbid_regex "src/modules/mesh/DMesh/Dynamics/Bullet/DMbtRigid.h" \
  'clearContactData'

# Poly component matrices share one finite, object-space frame contract.
for poly_frame_source in \
  "src/modules/mesh/mesh/DPolyFaceToMat.cpp" \
  "src/modules/mesh/mesh/DPolyEdgeToMat.cpp"; do
  require_regex "$poly_frame_source" 'buildPolyComponentFrame'
  require_regex "$poly_frame_source" 'publishPolyComponentFrame'
  require_regex "$poly_frame_source" 'MSpace::kObject'
  forbid_regex "$poly_frame_source" 'getPoints[[:space:]]*\('
  require_regex "$poly_frame_source" 'validComponentFrameStorage'
  require_regex "$poly_frame_source" 'getPolyComponentPoint'
done
require_regex "src/modules/mesh/mesh/DPolyFaceToMat.cpp" \
  'faceIndex[[:space:]]*<[[:space:]]*0'
require_regex "src/modules/mesh/mesh/DPolyEdgeToMat.cpp" \
  'edgeIndex[[:space:]]*<[[:space:]]*0'
require_regex "src/core/common/utility/DPolyComponentFrame.h" \
  'isFiniteQuaternion'

# Removed viewport/Bullet recipes must not return as misleading disabled code.
forbid_regex "src/modules/point/DPoint/Duplicator/DPInstancer.cpp" \
  '^[[:space:]]*/\*|^[[:space:]]*//.*(cout|MGeometryExtractor|getVertexNormal\()'
forbid_regex "src/modules/dynamics/DBullet/RigidBody/DBRigidBodyNode.cpp" \
  'crashes Maya|outputValue\(ca_rigidBody\)|^[[:space:]]*//.*(cout|glMaterialfv)'

# Attribute writes must validate lookup, concrete type, and cardinality before
# dereferencing. The uint32 enum keeps its old numeric value only as an alias.
for checked_attribute_source in \
  "src/core/Data/DPrimBaseData.cpp" \
  "src/core/DDataUtility/Data/DPData.cpp" \
  "src/core/Data/DCurveData.cpp" \
  "src/modules/mesh/DMesh/Data/DMeshData.cpp"; do
  forbid_regex "$checked_attribute_source" \
    'getAttr\([^;]*\)[[:space:]]*->'
done
require_regex "src/core/DDataUtility/Data/DPAttributeData.h" \
  'DP_ATTR_UINT32'
require_regex "src/core/DDataUtility/Data/DPAttributeData.h" \
  'DP_ATTR_INT32[[:space:]]*=[[:space:]]*DP_ATTR_UINT32'
require_regex "src/core/DDataUtility/Data/DPAttributeData.h" \
  'getValue\(const size_t[[:space:]]*&[[:space:]]*n\) const'
require_regex "src/core/DDataUtility/Data/DPAttributeData.h" \
  'm_valueArray\.at\(n\)'
require_regex "src/core/DDataUtility/Data/DPAttributeData.h" \
  "typeName\\(\\)[[:space:]]*<<[[:space:]]*'\\['"
legacy_uint32_uses="$({
  grep -R -nE \
    --include='*.cpp' --include='*.h' --include='*.hpp' --include='*.inl' \
    'DP_ATTR_INT32' "$repo_root/src" |
    grep -v 'src/core/DDataUtility/Data/DPAttributeData.h'
} || true)"
if [[ -n "$legacy_uint32_uses" ]]; then
  echo "Legacy uint32 attribute name used outside its compatibility alias:" >&2
  echo "$legacy_uint32_uses" >&2
  exit 1
fi

# Optional exporters and the instancer retain the MObject/MDataHandle that owns
# custom plug-in data and reject failed plug reads.
for alembic_reader in \
  "src/modules/curve/DCurve/Cmd/Abc/DCAbcWriteJob.cpp" \
  "src/modules/curve/DCurve/Cmd/Abc/DPointWriter.cpp" \
  "src/modules/curve/DCurve/Cmd/Abc/DCurveWriter.cpp" \
  "src/modules/curve/DCurve/Cmd/Abc/DMeshWriter.cpp"; do
  require_regex "$alembic_reader" 'getPluginDataFromPlug'
done
require_regex "src/modules/point/DPoint/Duplicator/DPInstancer.cpp" \
  'MStatus readDataHandle'
require_regex "src/modules/point/DPoint/Duplicator/DPInstancer.cpp" \
  'getPluginDataFromPlug'

# Bullet downcasts must be preceded by a checked runtime invariant, and raw
# observer members must advertise their non-owning role.
require_regex "src/modules/dynamics/DBullet/Collision/bt_convex_hull_shape.h" \
  'getShapeType\(\)[[:space:]]*!=[[:space:]]*COMPOUND_SHAPE_PROXYTYPE'
require_regex "src/modules/dynamics/DBullet/Collision/bt_convex_hull_shape.h" \
  'm_sourceVertices'
require_regex "src/modules/dynamics/DBullet/RigidBody/DB_RigidBody_t.h" \
  'm_collisionShapeObserver'
require_regex "src/modules/dynamics/DBullet/Mesh/DB_MeshShape_t.h" \
  'm_concaveShapeObserver'
require_regex "src/modules/mesh/DMesh/Dynamics/Bullet/db_collision_mesh_shapes.h" \
  'm_concaveShapeObserver'
require_regex "src/modules/mesh/DMesh/Dynamics/Bullet/DBFractureDynamicsWorld.cpp" \
  'fractureBodyFrom'
bullet_old_pointer_syntax="$({
  grep -R -nE \
    --include='*.cpp' --include='*.h' --include='*.hpp' --include='*.inl' \
    '\bNULL\b|\([A-Za-z_][A-Za-z0-9_:<>]*[[:space:]]*\*\)[[:space:]]*[A-Za-z_(]' \
    "$repo_root/src/modules/dynamics/DBullet" \
    "$repo_root/src/modules/mesh/DMesh/Dynamics/Bullet"
} || true)"
if [[ -n "$bullet_old_pointer_syntax" ]]; then
  echo "Old-style Bullet pointer syntax found:" >&2
  echo "$bullet_old_pointer_syntax" >&2
  exit 1
fi

debug_defaults="$({
  find "$repo_root/src" \
    \( -path '*/old' -o -path '*/backup' \) -prune -o \
    -type f \( -name '*.cpp' -o -name '*.h' -o -name '*.hpp' \) \
    -exec grep -nHE \
      '^[[:space:]]*(#define[[:space:]]+[A-Z0-9_]*(DEBUG|VERBOSE)[A-Z0-9_]*[[:space:]]+1([[:space:]]|$)|const bool VERBOSE[[:space:]]*=[[:space:]]*true)' \
      {} +
} || true)"

if [[ -n "$debug_defaults" ]]; then
  echo "Always-on debug output found in active C++ source:" >&2
  echo "$debug_defaults" >&2
  exit 1
fi

unqualified_mvl_symbols="$({
  find \
    "$repo_root/src/modules/dynamics/DBullet" \
    "$repo_root/src/modules/mesh/DMesh/Dynamics/Bullet" \
    \( -path '*/old' -o -path '*/backup' \) -prune -o \
    -type f \( -name '*.cpp' -o -name '*.h' -o -name '*.hpp' \) \
    -exec grep -nHE '(^|[^:[:alnum:]_])(qidentity|cmat)<' {} +
} || true)"

if [[ -n "$unqualified_mvl_symbols" ]]; then
  echo "Unqualified MVL math symbol found in active Bullet source:" >&2
  echo "$unqualified_mvl_symbols" >&2
  exit 1
fi

# Maya renderer sampling must remain bounded and transactional. These guards
# also preserve the direct API declarations needed by the real SDK check.
require_regex "src/core/common/utility/DBatchRange.h" \
  'kMaximumShadingSampleCount[[:space:]]*=[[:space:]]*4096U'
require_regex "src/modules/rendering/global/render/DGlobalTex.cpp" \
  '#include[[:space:]]*<maya/MString.h>'
require_regex "src/modules/rendering/global/render/DGlobalTex.cpp" \
  'sampleShadingNetworkInBatches'
require_regex "src/modules/rendering/global/render/DGlobalTex.cpp" \
  'copyVectorBatchToResult'
require_regex "src/modules/rendering/global/render/DGlobalTex.h" \
  'evaluate2dTextureInBatches'
require_regex "src/modules/rendering/global/render/DGlobalTex.h" \
  'evaluateDynamics2dTextureInBatches'
require_regex "src/modules/rendering/global/render/DGlobalTex.cpp" \
  'copyTextureBatchToResult'
for batched_shading_consumer in \
  "src/commands/shapeFillCmd.cpp" \
  "src/commands/setParticleRGBCmd.cpp" \
  "src/modules/deformers/Deformer/pushDeformer.cpp"; do
  require_regex "$batched_shading_consumer" \
    'sampleShadingNetworkInBatches'
  forbid_regex "$batched_shading_consumer" \
    'MRenderUtil::sampleShadingNetwork'
done
forbid_regex "src/modules/deformers/Deformer/pushDeformer.cpp" \
  'colors\.clear\(\)'
require_regex "src/modules/deformers/Deformer/pushDeformer.cpp" \
  'DOMMY_RETURN_IF_FAILURE\(uCoordinates\.setLength'
for map_sampler in \
  "src/core/DDataUtility/Generate/DPointPlaneEmit.cpp" \
  "src/core/DDataUtility/Generate/DPScatter.cpp"; do
  require_regex "$map_sampler" 'getMapValues\(positions,[[:space:]]*mapValues\)'
  require_regex "$map_sampler" 'evaluateDynamics2dTextureInBatches'
  forbid_regex "$map_sampler" 'getMapValue\('
  forbid_regex "$map_sampler" 'MDynamicsUtil::evalDynamics2dTexture'
done
require_regex "src/core/DDataUtility/Generate/DPointVolumeEmit.cpp" \
  'evaluateDynamics2dTextureInBatches'
forbid_regex "src/core/DDataUtility/Generate/DPointVolumeEmit.cpp" \
  'MDynamicsUtil::evalDynamics2dTexture'
forbid_regex "src/modules/generators/legacy/surfaceDuplicater.cpp" \
  'getColorMapData|MRenderUtil::(eval2dTexture|sampleShadingNetwork)|^[[:space:]]*/\*'
forbid_regex "src/modules/generators/legacy/surfaceDuplicater.h" \
  'getColorMapData'

direct_renderer_calls="$({
  find "$repo_root/src" -type f \( -name '*.cpp' -o -name '*.h' \) \
    ! -path '*/DGlobalTex.cpp' \
    -exec grep -nHE 'MRenderUtil::(sampleShadingNetwork|eval2dTexture)|MDynamicsUtil::evalDynamics2dTexture' {} +
} || true)"
if [[ -n "$direct_renderer_calls" ]]; then
  echo "Renderer texture sampling bypasses DGlobalTex batching:" >&2
  echo "$direct_renderer_calls" >&2
  exit 1
fi
require_regex "src/core/DDataUtility/Generate/DPointVolumeEmit.cpp" \
  'getMapValues\(positions,[[:space:]]*mapValues\)'
require_regex "src/core/DDataUtility/Generate/DPointVolumeEmit.cpp" \
  'kMaximumShadingSampleCount'
require_regex "src/modules/curve/DCurve/Utility/DCColor.cpp" \
  '#include[[:space:]]*"DNoiseOp.h"'
require_regex "src/modules/mesh/polyModifier/polyBlast/polyBlastSelection.h" \
  'MObject[[:space:]]+component[[:space:]]*=[[:space:]]*componentFunction\.create'
forbid_regex "src/modules/deformers/Deformer/noiseDeformer.cpp" \
  '(^|[^:[:alnum:]_])randhash\('

# Procedural curve/mesh generators must keep exact allocation plans, checked
# ramp evaluation, and checked Maya-array growth at the legacy boundaries.
require_regex "src/modules/curve/DCurve/Utility/DCUVMap.cpp" \
  'buildUVMappingPlan'
require_regex "src/modules/curve/DCurve/Utility/DCUVMap.cpp" \
  'checkDCurvePointAttribute'
require_regex "src/modules/curve/DCurve/Utility/DCUVMap.cpp" \
  'sampleFiniteRamp'
require_regex "src/modules/curve/DCurve/Utility/DCUVMap.cpp" \
  'checkedFloatCast'
forbid_regex "src/modules/curve/DCurve/Utility/DCUVMap.cpp" \
  'static_cast<DScalarData>\(mappedValue\)'
forbid_regex "src/modules/curve/DCurve/Utility/DCUVMap.cpp" \
  'getValueAtPosition'
require_regex "src/modules/generators/Generate/LeafGen/LeafGen.cpp" \
  'buildLeafMeshPlan'
require_regex "src/modules/generators/Generate/LeafGen/LeafGen.cpp" \
  'sampleFiniteRamp'
forbid_regex "src/modules/generators/Generate/LeafGen/LeafGen.cpp" \
  'getValueAtPosition'
require_regex "src/modules/generators/Generate/Human/DHEyeGen.cpp" \
  'buildEyeMeshPlan'
require_regex "src/modules/generators/Generate/Human/DHEyeGen.cpp" \
  'CheckedArray'
require_regex "src/modules/generators/Generate/Human/DHEyeGen.cpp" \
  'sampleFiniteRamp'
forbid_regex "src/modules/generators/Generate/Human/DHEyeGen.cpp" \
  'getValueAtPosition|Old Type Eye'
for checked_ramp_source in \
  "src/modules/curve/DCurve/Utility/DCNormal.cpp" \
  "src/modules/deformers/Deformer/DCollisionDeformer.cpp" \
  "src/core/DDataUtility/Generate/DPointPlaneEmit.cpp" \
  "src/core/DDataUtility/Generate/DPointVolumeEmit.cpp" \
  "src/core/DDataUtility/Generate/DPScatter.cpp"; do
  require_regex "$checked_ramp_source" 'sampleFiniteRamp'
  forbid_regex "$checked_ramp_source" 'getValueAtPosition'
done
require_regex "src/modules/curve/DCurve/Utility/DCCharactor.cpp" \
  'values\.length\(\)[[:space:]]*!=[[:space:]]*pointCount'
require_regex "src/modules/generators/legacy/surfaceDuplicater.cpp" \
  'validGeneratedMeshArrays'
require_regex "src/modules/generators/legacy/surfaceDuplicater.cpp" \
  'CheckedArray<MFloatPointArray>'
require_regex "src/modules/curve/Curve/Node/DRigController.cpp" \
  'CheckedArray<MPointArray>'
require_regex "src/modules/curve/Curve/Node/DRigController.cpp" \
  'DRigController point allocation failed'

require_regex "src/modules/generators/Generate/Human/DHEyeGen.cpp" 'matchesEyePartPlan'
require_regex "src/modules/generators/Generate/Human/DHEyeGen.cpp" 'meshPlan\.lensTipPointCount'
require_regex "src/modules/generators/Generate/LeafGen/LeafGen.cpp" 'prependLeafBendStep'
require_regex "src/modules/generators/Generate/LeafGen/LeafGen.cpp" 'nextNonnegativeSeed'
require_regex "src/modules/generators/Generate/LeafGen/LeafGen.cpp" 'pointUVPosition'
forbid_regex "src/modules/generators/Generate/LeafGen/LeafGen.cpp" \
  'seed[[:space:]]*\+[[:space:]]*1|length-lengthStep|i<pointWriteIndex'
for tile_consumer in \
  "src/modules/generators/legacy/surfaceDuplicater.cpp" \
  "src/modules/point/DPoint/mesh/DPMeshDuplicator.cpp"; do
  require_regex "$tile_consumer" 'mapTileUV'
done
require_regex "src/modules/mesh/legacy/dupMesh.h" 'std::shared_ptr<Storage>'
forbid_regex "src/modules/mesh/legacy/dupMesh.h" 'void get.*Array\(|void get[UV]Coords\('
require_regex "tests/check_maya_sdk_headers.sh" 'dup_mesh_contract_compile_test\.cpp'

forbid_regex "src/modules/mesh/DMesh/Utility/DMEdgeDisplaceDir.cpp" \
  'uvDirs|getUvShellsIds|getConnectedVertices'
require_regex "src/modules/mesh/DMesh/Data/DMeshData.cpp" 'copyMeshUVArrays'
require_regex "src/modules/mesh/DMesh/Data/DMeshData.cpp" 'validFullMeshUVArrays'
require_regex "src/modules/mesh/DMesh/Data/DMeshData.cpp" 'appendMeshMergePlan'
require_regex "src/modules/mesh/DMesh/common/DMeshUtil.cpp" \
  'MStatus status = mesh\.getUvData'
require_regex "src/modules/mesh/DMesh/Translator/DMeshTranslatorAi.cpp" \
  'Unable to copy the DMesh UV arrays'
require_regex "src/modules/mesh/DMesh/Utility/DMCopyUV.cpp" 'planMeshUVTransfer'
require_regex "src/modules/mesh/DMesh/Utility/DMCopyUV.cpp" \
  'if \(!\(\*mesh\)->setUvData'
forbid_regex "src/modules/mesh/DMesh/Utility/DMCopyUV.cpp" \
  'break;'
require_regex "src/modules/mesh/DMesh/common/DMeshUtil.cpp" 'getCenterDouble'
forbid_regex "src/modules/mesh/DMesh/common/DMeshUtil.cpp" \
  'float d[xyz][[:space:]]*='
require_regex "src/modules/mesh/DMesh/Utility/DMCharactor.cpp" \
  'status = DMGeneratePrimitiveCenterList'

require_regex "src/modules/mesh/DMesh/Data/DMeshData.cpp" 'MStatus DMeshData::copyFrom'
require_regex "src/modules/mesh/DMesh/Data/DMeshData.cpp" 'copyCheckedArray\(src.arrays_->mFaceList, arrays_->mFaceList\)'
require_regex "src/modules/mesh/DMesh/Convert/DMToMesh.cpp" 'DMeshPointSpace::Transformed'
for merge_consumer in \
  "src/modules/mesh/DMesh/Convert/DMToMesh.cpp" \
  "src/modules/mesh/DMesh/Utility/DMMakePiece.cpp"; do
  require_regex "$merge_consumer" 'DMeshInternalData::createMerged'
  forbid_regex "$merge_consumer" 'mergeMesh\('
done
forbid_regex "src/core/common/object/DPointOp.h" 'radiusSearch\('
require_regex "src/core/common/object/DPointOp.h" 'BoundedRadiusResultSet'
require_regex "src/core/common/utility/DMayaPluginDataOutput.h" 'status = invokeStatusOperation'
require_regex "src/modules/mesh/DMesh/Solver/DMSolver.cpp" 'return destination.copyFrom'
unchecked_mesh_copy="$(grep -R -nE --include='*.cpp' \
  '(outDMeshes|outputMeshes)->copy\(|cache->copy\(\*inputDMData\)' \
  "$repo_root/src" || true)"
if [[ -n "$unchecked_mesh_copy" ]]; then
  echo "Unchecked internal mesh copy returned: $unchecked_mesh_copy" >&2
  exit 1
fi

# Geometry-to-Maya consumers must explicitly bake primitive transforms.
for transformed_consumer in \
  "src/modules/mesh/DMesh/Utility/DMBoolean.cpp" \
  "src/modules/volume/VDB/Generate/DOpenVDBFromMesh.cpp"; do
  require_regex "$transformed_consumer" 'DMeshPointSpace::Transformed'
  conversion_count="$(grep -c 'generateMMeshFromDmeshWithUV' "$repo_root/$transformed_consumer")"
  transformed_count="$(grep -c 'DMeshPointSpace::Transformed' "$repo_root/$transformed_consumer")"
  if [[ "$conversion_count" != "$transformed_count" ]]; then
    echo "Missing explicit primitive transform in $transformed_consumer" >&2
    exit 1
  fi
done

require_regex "src/core/Data/DCurveData.cpp" 'MStatus DCurveData::copyFrom'
require_regex "src/core/Data/DCurveData.cpp" 'copyCheckedArray\(src.arrays_->knots, arrays_->knots\)'
require_regex "src/modules/curve/DCurve/Solver/DCSolver.cpp" 'return destination.copyFrom'
require_regex "src/modules/curve/DCurve/Shape/DCurveShape.cpp" 'if \(!dCurveGeom->copyFrom'
unchecked_geometry_copy="$(grep -R -nE --include='*.cpp' \
  '(outDCrvs|tmpDCrvs|outDCurves|outputCurves|dCurveGeom|outDPointData|outputData|output)->copy\(|cache->copy\(\*inputDCData\)' \
  "$repo_root/src" || true)"
if [[ -n "$unchecked_geometry_copy" ]]; then
  echo "Unchecked internal curve/point copy returned: $unchecked_geometry_copy" >&2
  exit 1
fi
for point_copy_consumer in \
  "src/core/DDataUtility/Attribute/DPAttributeTransfer.cpp" \
  "src/core/DDataUtility/Attribute/Edit/DPAttributeCast.cpp" \
  "src/core/DDataUtility/Attribute/Edit/DPAttributeVDB.cpp" \
  "src/modules/point/DPoint/Utility/DPClosestPointVDB.cpp"; do
  require_regex "$point_copy_consumer" '(->|\.)setDPData\('
  require_regex "$point_copy_consumer" 'DOMMY_(VDB_)?COMPUTE_CATCH'
done

for batch_generator in \
  "src/modules/mesh/DMesh/Convert/DMConvertFromMeshes.cpp" \
  "src/modules/mesh/DMesh/Generate/DMTimeShapeGen.cpp"; do
  require_regex "$batch_generator" 'generatedMeshes.reserve\('
  require_regex "$batch_generator" '!.*->insert\(generatedMeshes\)'
  forbid_regex "$batch_generator" '->insert\(item\)'
  if [[ "$(grep -c -- '->insert(' "$repo_root/$batch_generator")" != 1 ]]; then
    echo "Expected one checked batch insertion in $batch_generator" >&2
    exit 1
  fi
done
require_regex "src/modules/mesh/DMesh/Convert/DMConvertFromMeshes.cpp" 'kMaxGeneratedMeshCount'
require_regex "src/modules/mesh/DMesh/Convert/DMConvertFromMeshes.cpp" 'polygonIterator.getPoints\(points, MSpace::kObject, &status\)'
require_regex "src/modules/mesh/DMesh/Convert/DMConvertFromMeshes.cpp" 'generatedMeshes.size\(\) != outputMeshCount'
require_regex "src/modules/mesh/DMesh/Generate/DMTimeShapeGen.cpp" 'data.outputValue\(outDM, &stat\)'

require_regex "src/modules/mesh/mesh/DPolyResetVtx.cpp" 'getMatchingMeshPair'
require_regex "src/modules/mesh/mesh/DPolyResetVtx.cpp" 'getPerVertexColors'
require_regex "src/modules/mesh/mesh/DPolyResetVtx.cpp" 'checkedMaskedLinearBlend'
require_regex "src/modules/mesh/mesh/DPolyResetVtx.cpp" 'maskColors\[index\].r\) : 1.0'
require_regex "src/modules/mesh/mesh/DPolyResetVtx.cpp" 'outputMesh.setPoints'
forbid_regex "src/modules/mesh/mesh/DPolyResetVtx.cpp" 'getMayaMesh|MItMeshVertex|weightArray'
require_regex "src/core/common/utility/DMayaMeshInput.h" 'return colors.copy\(candidate\)'
require_regex "src/core/common/utility/DMayaMeshInput.h" 'status = result.firstPoints.copy'
require_regex "src/core/common/utility/DMayaMeshInput.h" 'status = result.secondPoints.copy'

require_regex "src/core/DDataUtility/Generate/DPAnimationEmit.cpp" 'const std::size_t trailCount'
require_regex "src/core/DDataUtility/Generate/DPAnimationEmit.cpp" 'CheckedArray<MPointArray> trailPositions'
require_regex "src/core/DDataUtility/Generate/DPAnimationEmit.cpp" 'CheckedArray<MPointArray> outputPoints'
require_regex "src/core/DDataUtility/Generate/DPAnimationEmit.cpp" 'outputIndex != expectedPointCount'
forbid_regex "src/core/DDataUtility/Generate/DPAnimationEmit.cpp" '\.(append|copy)\('

require_regex "src/modules/volume/VDB/Data/DOpenVDBData.cpp" 'MStatus DOpenVDBData::copyFrom'
require_regex "src/modules/volume/VDB/Data/DOpenVDBData.cpp" 'GridCPtrVec copied\(source.mGrids\)'
require_regex "src/modules/volume/VDB/IO/DWriteOpenVDB.cpp" 'DOMMY_RETURN_IF_FAILURE\(outputVdb->copyFrom'
forbid_regex "src/modules/volume/VDB/IO/DWriteOpenVDB.cpp" 'outputVdb->copy\(|Int32Metadata\(int\('
require_regex "src/modules/volume/VDB/IO/DWriteOpenVDB.cpp" 'checkedTimeMetadata'
require_regex "src/modules/volume/VDB/Common/DVDBTimeMetadata.h" 'numeric::checkedNumericCast'
require_regex "src/modules/volume/VDB/Common/DVDBTimeMetadata.h" 'std::floor\(tick \+ 0.5\)'
if ! awk '
  /checkedTimeMetadata/ { validated = 1 }
  /outputVdb->copyFrom\(\*vdb\)/ {
    if (!validated) exit 1
    found = 1
  }
  END { if (!found) exit 1 }
' "$repo_root/src/modules/volume/VDB/IO/DWriteOpenVDB.cpp"; then
  echo "VDB export metadata must be checked before preparing its output copy." >&2
  exit 1
fi

# Shared Maya typed-array output construction must propagate API failures.
require_regex "src/core/common/object/DNodeOperation.cpp" 'MStatus outputMayaArray'
for array_type in Int Float Double Vector Point; do
  require_regex "src/core/common/object/DNodeOperation.cpp" \
    "return outputMayaArray<MFn${array_type}ArrayData>"
done
require_regex "src/core/common/object/DNodeOperation.cpp" 'if \(candidate.isNull\(\)\) return MS::kFailure'
forbid_regex "src/modules/mesh/mesh/DSkinInfo.cpp" 'CHECK_MSTATUS\(newMeshFn.setVertexColors|CHECK_MSTATUS\(outHandle.set'
require_regex "src/modules/mesh/mesh/DSkinInfo.cpp" 'data.outputValue\(outMesh, &stat\)'
forbid_regex "src/modules/mesh/mesh/DPolyWire.cpp" 'CHECK_MSTATUS\(outHandle.set'

require_regex "src/core/Utility/3d/DataStruct/DHashGrid.h" 'bool Setup\('
require_regex "src/core/Utility/3d/DataStruct/DHashGrid.h" 'bool SetObjectToCell\('
require_regex "src/core/Utility/3d/DataStruct/DHashGrid.h" 'bool GetNN\('
require_regex "src/core/Utility/3d/DataStruct/DHashGrid.h" 'RadiusSearchBudget m_workBudget'
require_regex "src/core/Utility/3d/DataStruct/DHashGrid.h" 'neighbors.swap\(result\)'
require_regex "src/core/Utility/3d/DataStruct/DHashGrid.h" 'count != m_indexedCount'
require_regex "src/core/Utility/3d/DataStruct/DHashGrid.h" 'm_workBudget.consume\(1U\); // Includes empty-cell traversal'
for grid_consumer in \
  "src/modules/point/DPoint/Utility/DPSmooth.cpp" \
  "src/core/DDataUtility/Generate/DPScatter.cpp"; do
  for grid_operation in Setup SetObjectToCell GetNN; do
    require_regex "$grid_consumer" "if \\(!nnGrid\\.${grid_operation}\\("
  done
  forbid_regex "$grid_consumer" '^[[:space:]]*nnGrid\.(Setup|SetObjectToCell|GetNN)\('
done
for grid_operation in Setup SetObjectToCell GetNN; do
  require_regex "src/core/UtilityNode/DStretchData/DStretchData.cpp" "!grid\\.${grid_operation}\\("
done
require_regex "src/modules/point/DPoint/Utility/DPSmooth.cpp" 'checkedProduct\(ptCount, iterationCount'
require_regex "src/modules/point/DPoint/Utility/DPSmooth.cpp" 'if \(iterationCount > 0U\)'
require_regex "src/modules/point/DPoint/Utility/DPSmooth.cpp" 'workspace.add\(cells, 2U \* sizeof\(uint\)\)'
require_regex "src/modules/point/DPoint/Utility/DPSmooth.cpp" 'point = newPtArray\[index\]'
require_regex "src/core/Utility/3d/DataStruct/DHashGrid.h" 'if \(!admitStorage\(checkedCount, cells\)\) return false'
require_regex "src/core/DDataUtility/Generate/DPScatter.cpp" 'projectPointPositions<openvdb::Vec3R>'
require_regex "src/core/DDataUtility/Generate/DPScatter.cpp" 'return clpGrid->searchAndReplace'
forbid_regex "src/core/DDataUtility/Generate/DPScatter.cpp" 'Vec3R\(0,0,0\)|^[[:space:]]*clpGrid->searchAndReplace'
require_regex "src/core/common/utility/DPointProjection.h" '!project\(projected\) \|\| projected.size\(\) != positions.size\(\)'
require_regex "src/core/common/utility/DPointProjection.h" 'positions.swap\(candidate\)'

python3 "$repo_root/tests/check_status_macro_calls.py"
require_regex "src/modules/curve/DCurve/Generate/DCurveFromMesh.cpp" 'getMeshObject\(inMesh, data, meshObject\)'
require_regex "src/modules/curve/DCurve/Generate/DCurveFromMesh.cpp" 'DOMMY_RETURN_IF_FAILURE\(mesh.getPoints\(points, MSpace::kObject\)\)'
require_regex "src/modules/curve/DCurve/Generate/DCurveFromMesh.cpp" 'DOMMY_RETURN_IF_FAILURE\(mesh.getEdgeVertices'
require_regex "src/modules/curve/DCurve/Generate/DCurveFromMesh.cpp" 'validEdgeVertexIndices'
require_regex "src/modules/curve/DCurve/Generate/DCurveFromMesh.cpp" 'output.insert\(curves\)'
forbid_regex "src/modules/curve/DCurve/Generate/DCurveFromMesh.cpp" 'MSpace::kWorld|outDCrvs->insert'
require_regex "src/core/common/utility/DPointTransform.h" 'exactNormalDirection'
require_regex "src/core/common/utility/DPointTransform.h" 'largestError > directionScale'
require_regex "src/core/common/utility/DProductSum.h" 'std::fma'
require_regex "src/core/common/utility/DProductSum.h" 'errorBound >= 0.5'
require_regex "src/core/common/utility/DForceMath.h" 'addProduct4'
require_regex "src/core/common/utility/DGoalMath.h" 'checkedSpringDampingResponse'
require_regex "src/core/common/utility/DParticleFieldMath.h" 'checkedIntegratedVelocity'
forbid_regex "src/core/common/utility/DParticleFieldMath.h" 'force \* inverseMass \* deltaTime'
require_regex "src/core/math/DRemapMath.h" 'scaledRangeDifference'
require_regex "src/core/math/DRemapMath.h" 'checkedLinearInterpolation'
require_regex "src/modules/point/DPoint/Utility/DPAffect.cpp" 'checkedLinearInterpolation'
require_regex "src/modules/point/DPoint/Utility/DPAffect.cpp" 'checkedMaskedLinearInterpolation'
forbid_regex "src/modules/point/DPoint/Utility/DPAffect.h" 'm_valueSize'
require_regex "src/core/common/utility/DNumericValidation.h" 'periodicValue'
require_regex "src/core/common/utility/DAttributeBlendMath.h" 'checkedMaskedLinearInterpolation'
for signal_source in \
  "src/modules/point/DPoint/animation/DPSignal.cpp" \
  "src/modules/point/DPoint/animation/DPSignal3D.cpp"; do
  require_regex "$signal_source" 'dommy::signal::outputValue'
  require_regex "$signal_source" 'dommy::signal::applyModulation'
done
forbid_regex "src/core/math/DRemapMath.h" 'maximum - minimum'
require_regex "src/core/common/utility/DPointTransform.h" 'math::normalized3OrZero'
forbid_regex "src/modules/mesh/DMesh/Display/DMeshDisplay.cpp" '\.inverse\(\)\.transpose\(\)'
require_regex "src/modules/mesh/DMesh/Display/DMeshDisplay.cpp" 'checkedTransformNormal'
require_regex "src/modules/point/DPoint/math/DVectorNodeMath.h" 'PointMeanAccumulator'
forbid_regex "src/modules/point/DPoint/math/DVectorNodeMath.h" 'source\[axis\] / scale'
require_regex "src/core/common/utility/DScaledProduct.h" 'productExponent > std::numeric_limits<double>::max_exponent'
require_regex "src/modules/point/DPoint/math/DVectorMath.h" '#include "math/DVectorNormalization.h"'
require_regex "src/core/common/utility/DNumericValidation.h" 'value >= std::ldexp\(1.0L, std::numeric_limits<T>::digits\)'
require_regex "src/modules/point/DPoint/math/DPMathUtility.cpp" 'DPublishAttributeValues'
require_regex "src/core/common/utility/DAttributeMathPublication.h" 'publishAttributeReplacement'
require_regex "src/modules/point/DPoint/Common/DPAttributeCommon.cpp" 'dommy::attribute::findAttribute'
require_regex "src/core/DDataUtility/Data/DPAttributeData.h" 'std::make_shared<TypedDPAttributeData<T> >\(\*this\)'
forbid_regex "src/core/DDataUtility/Data/DPAttributeData.h" 'new TypedDPAttributeData<T>\(T\(\)\)|std::vector<T>\(0, T\(\)\)'
require_regex "src/core/common/utility/DAttributeValueConversion.h" 'checkedValueCast\(value, candidate\)'
forbid_regex "src/modules/point/DPoint/Common/DPAttributeCommon.h" 'Destination\(\*value\)'
for attribute_lookup_source in \
  "src/modules/point/DPoint/Common/DPAttributeCommon.cpp" \
  "src/modules/curve/DCurve/DCurveUtil.h" \
  "src/modules/mesh/DMesh/common/DMeshUtil.h"; do
  forbid_regex "$attribute_lookup_source" 'name\.erase\('
done
require_regex "src/modules/point/DPoint/math/DPMathUtility.cpp" 'readMathAttribute'
require_regex "src/modules/point/DPoint/math/DPMathUtility.cpp" 'replaceMathAttribute'
forbid_regex "src/modules/point/DPoint/math/DPMathUtility.cpp" 'getAttr\([^)]*\)->|setDPAttributeValues\('
require_regex "src/modules/point/DPoint/math/DProceduralMath.h" 'using DMathScalarData = double'
require_regex "src/modules/point/DPoint/math/DPMathUtility.cpp" 'DOMMY_RETURN_IF_FAILURE\(processDPointMathFncSingle'
require_regex "src/modules/point/DPoint/math/DPMathUtility.cpp" 'DOMMY_RETURN_IF_FAILURE\(processDCurveMathFncSingle'
require_regex "src/modules/point/DPoint/math/DPMathUtility.cpp" 'DOMMY_RETURN_IF_FAILURE\(processDMeshMathFncSingle'
forbid_regex "src/modules/point/DPoint/math/DPMathUtility.cpp" 'removeAttr\(sourceAttrName\)|addAttrWithValues\(outAttrName|addDPAttributeValues\(outAttrName'
require_regex "src/core/common/utility/DAttributePublication.h" 'sourceName != outputName'
require_regex "tests/run_cpp_tests.sh" 'fno-sanitize-recover=all'
require_regex "CMakeLists.txt" 'fno-sanitize-recover=all'
require_regex "src/core/common/object/DPointOp.cpp" 'geometry::generateRelaxedPoints2D'
require_regex "src/core/common/object/DPointOp.cpp" 'maya::buildCheckedArray'
require_regex "src/core/common/object/DPointOp.cpp" 'geometry::buildConvexHull2D'
forbid_regex "src/core/common/object/DPointOp.cpp" 'newPoints.append|seed[[:space:]]*\+[[:space:]]*50|DPCross2D'
require_regex "src/core/common/object/DPointGeneration.h" 'MStatus DGenerateRelaxPoint2D'
require_regex "src/core/DDataUtility/Generate/DPointPlaneEmit.cpp" 'DOMMY_RETURN_IF_FAILURE\(DGenerateRelaxPoint2D'
require_regex "src/core/DDataUtility/Generate/DPointMeshEmit.cpp" 'status = DGenerateRelaxPoint2D'
require_regex "src/core/common/utility/DRelaxPoint2D.h" 'offsetNonnegativeSeed\(seed, 50U'
require_regex "src/core/common/utility/DRelaxPoint2D.h" '~RelaxDiagram\(\).*jcv_diagram_free'
require_regex "src/core/common/utility/DRelaxPoint2D.h" 'scale = std::max\(width, height\)'
require_regex "src/core/common/utility/DRelaxPoint2D.h" 'output.swap\(candidate\)'
require_regex "src/core/common/utility/DConvexHull2D.h" 'cross2\(Point2\('
require_regex "src/core/common/utility/DConvexHull2D.h" 'std::isfinite\(orientation\)'
require_regex "src/modules/curve/DCurve/Utility/DCConvex2D.cpp" 'validArrayConversionBudget'
require_regex "src/modules/curve/DCurve/Utility/DCConvex2D.cpp" 'getValidInputDC'
forbid_regex "src/modules/point/DPoint/mesh/voro/jc_voronoi.h" '#pragma pack'
require_regex "src/modules/point/DPoint/mesh/voro/jc_voronoi.h" 'alignof\(std::max_align_t\)'
require_regex "src/modules/point/DPoint/mesh/voro/jc_voronoi.h" 'jcv_diagram_free\(&candidate\)'
require_regex "src/modules/point/DPoint/mesh/voro/jc_voronoi.h" 'if \(!originalmem\) throw std::bad_alloc\(\)'
require_regex "tests/cpp/point_generation_tests.cpp" 'runPointGenerationTests'
require_regex "CMakeLists.txt" 'tests/cpp/point_generation_tests.cpp'
# Shared random generation, signed stream offsets, and finite UV sampling.
require_regex "src/core/common/object/DNoiseOp.cpp" 'generateRealTriples<float>'
require_regex "src/core/common/object/DNoiseOp.h" 'random::generateValues'
forbid_regex "src/core/common/object/DNoiseOp.cpp" 'seed[[:space:]]*\+[[:space:]]*[12]|values.setLength|DScalarData minX'
for source in DCNormal DCFitCopy DCMeshCopy DCCopy DCReplicator; do
  forbid_regex "src/modules/curve/DCurve/Utility/$source.cpp" 'seed[[:space:]]*\+'
  require_regex "src/modules/curve/DCurve/Utility/$source.cpp" 'requireOffsetNonnegativeSeed'
done
forbid_regex "src/modules/mesh/DMesh/Utility/DMColor.cpp" 'seedVal[[:space:]]*\+[[:space:]]*offsetRand'
forbid_regex "src/modules/curve/DCurve/Utility/DCColor.cpp" 'seedVal[[:space:]]*\+[[:space:]]*offsetRand'
require_regex "src/core/common/utility/DRandomValueGeneration.h" 'prepareRealBounds'
require_regex "src/core/common/utility/DRandomValueGeneration.h" 'checkedNumericCast'
require_regex "src/core/common/utility/DMayaRandomValueGeneration.h" 'buildCheckedArray'
require_regex "src/core/DDataUtility/Generate/DPointMeshEmit.cpp" 'generateUniformFloatSamples'
require_regex "src/core/DDataUtility/Generate/DPointMeshEmit.cpp" 'if \(!queryStatus'
require_regex "src/core/DDataUtility/Generate/DPointMeshEmit.cpp" 'inputValue\(attribute, &readStatus\)'
require_regex "src/core/DDataUtility/Generate/DPointMeshEmit.cpp" 'type < 0 \|\| type > 1'
require_regex "tests/cpp/random_sampling_tests.cpp" 'runRandomSamplingTests'
require_regex "CMakeLists.txt" 'tests/cpp/random_sampling_tests.cpp'
require_regex "tests/run_cpp_tests.sh" 'tests/cpp/random_sampling_tests.cpp'
# Legacy attribute readers and fur generation must retain checked boundaries.
require_regex "src/modules/point/DPoint/Common/DPointCommon.h" 'requireTypedArray'
require_regex "src/modules/point/DPoint/Common/DPointCommon.h" 'copyTypedMayaArray'
forbid_regex "src/modules/point/DPoint/Common/DPointCommon.h" '\.setLength\(|static_cast<(float|int|DScalarData)>'
forbid_regex "src/modules/point/DPoint/Common/DPAttributeAccess.h" 'output = candidate|\.setLength\('
require_regex "src/modules/point/DPoint/Common/DPAttributeAccess.h" 'copyTypedMayaArray'
require_regex "src/core/common/utility/DMayaAttributeArray.h" 'maya::buildCheckedArray'
require_regex "src/core/common/utility/DMayaCheckedArray.h" 'return copyCheckedArray'
require_regex "src/core/common/utility/DMayaCheckedArray.h" 'output.length\(\) == count'
require_regex "src/modules/point/DPoint/generate/data/DPDoubleArrayGen.cpp" 'checkedFloatCast'
require_regex "src/modules/curve/DCurve/Generate/DCFurGen.cpp" 'normalized3OrZero'
require_regex "src/modules/curve/DCurve/Generate/DCFurGen.cpp" 'getCheckedMVectorArrayFromAttribute'
require_regex "src/modules/curve/DCurve/Generate/DCFurGen.cpp" 'getCheckedDScalarArrayFromAttribute'
require_regex "src/modules/curve/DCurve/Generate/DCFurGen.cpp" 'isFloatRepresentablePoint\(curvePoint\)'
require_regex "src/modules/curve/DCurve/Generate/DCFurGen.cpp" 'DOMMY_RETURN_IF_FAILURE\(curvePoints.append'
require_regex "src/modules/curve/DCurve/Generate/DCFurGen.cpp" 'curvePoints.length\(\) != cvIndex \+ 1U'
require_regex "src/modules/curve/DCurve/Generate/DCFurGen.cpp" '!output->insert\(generatedCurves\)'
require_regex "src/modules/curve/DCurve/Generate/DCFurGen.cpp" 'output->numberOfCurves\(\) != selectedIndices.size\(\)'
require_regex "src/core/common/utility/DMayaCheckedArray.h" 'status = output.set\(source\[i\], i\)'
require_regex "src/modules/point/DPoint/Common/DPAttributeAccess.h" 'copyStringMayaArray'
forbid_regex "src/modules/curve/DCurve/Generate/DCFurGen.cpp" 'direction.length\(\)'
require_regex "tests/cpp/attribute_access_tests.cpp" 'runAttributeAccessTests'
require_regex "CMakeLists.txt" 'tests/cpp/attribute_access_tests.cpp'
require_regex "tests/run_cpp_tests.sh" 'tests/cpp/attribute_access_tests.cpp'
for point_owner in "src/core/Data/DPrimBaseData.cpp" "src/core/DDataUtility/Data/DPData.cpp"; do
  require_regex "$point_owner" 'copyPointsToMayaArray\(pointArray, points\)'
  require_regex "$point_owner" 'replacePointArray\(pts, pts.length\(\), pointArray\)'
  require_regex "$point_owner" 'replacePointArray\(pts, pts.size\(\), pointArray\)'
done
forbid_regex "src/core/Data/DPrimBaseData.cpp" 'pointArray.shrink_to_fit|const double \*pptr = &pts'
require_regex "src/core/common/utility/DPointArrayConversion.h" 'numeric::checkedVectorArrayCopy'
require_regex "src/core/common/utility/DPointArrayConversion.h" 'maya::buildCheckedArray'
require_regex "src/core/common/utility/DPointArrayConversion.h" 'kMaxGeneratedPointCount'
require_regex "src/core/Data/DCurveData.cpp" 'copyCheckedArray\(k, arrays_->knots\)'
forbid_regex "src/core/Data/DCurveData.cpp" 'knots = k;'
require_regex "src/modules/mesh/DMesh/Data/DMeshData.cpp" 'copyCheckedArray\(faceCountList, arrays_->mFaceList\)'
require_regex "src/modules/mesh/DMesh/Data/DMeshData.cpp" 'copyCheckedArray\(connectList, arrays_->mConnectList\)'
forbid_regex "src/modules/mesh/DMesh/Data/DMeshData.cpp" 'mFaceList = faceCountList|mConnectList = connectList|m_u_vals = ucoords'
require_regex "src/core/common/utility/DMeshUVTransfer.h" 'maya::copyCheckedArray'
fur_source="src/modules/curve/DCurve/Generate/DCFurGen.cpp"
require_regex "$fur_source" 'generatedCurves.push_back'
require_regex "$fur_source" '^    if \(!generatedCurves.empty\(\) && !output->insert\(generatedCurves\)\)'
require_regex "$fur_source" 'generatedCurves.clear\(\)'
if [[ "$(grep -c 'output->insert' "$repo_root/$fur_source")" -ne 1 ]]; then
  echo "DCFurGen must publish its curve batch in a single insertion." >&2
  exit 1
fi
require_regex "CMakeLists.txt" 'tests/cpp/point_storage_tests.cpp'
require_regex "tests/run_cpp_tests.sh" 'tests/cpp/point_storage_tests.cpp'
for surface_source in DCurveFromSurface DCurveClothFromSurface; do
  forbid_regex "src/modules/curve/DCurve/Generate/$surface_source.cpp" 'MSpace::kWorld'
  require_regex "src/modules/curve/DCurve/Generate/$surface_source.cpp" 'MSpace::kObject'
done
require_regex "src/modules/curve/DCurve/Generate/DCurveFromSurface.cpp" 'maya::buildCheckedArray'
require_regex "src/modules/curve/DCurve/Generate/DCurveFromSurface.cpp" 'checkedSurfaceParameter'
require_regex "src/core/common/utility/DCurveSampling.h" 'kMaxGeneratedCurveCount'
require_regex "src/core/common/utility/DCurveSampling.h" 'kMaxCurveSampleCount'
require_regex "src/core/common/utility/DCurveSampling.h" 'direction::difference'
require_regex "src/core/common/utility/DCurveSampling.h" 'attribute::checkedLinearBlend'
require_regex "src/modules/curve/DCurve/Utility/DCResample.cpp" 'resampleCurvePolyline'
require_regex "src/modules/curve/DCurve/Generate/DCurveLinkGen.cpp" 'DCurveInternalData\(cvs, curveDegree, true\)'
forbid_regex "src/modules/curve/DCurve/Generate/DCurveLinkGen.cpp" 'GenerateDCurveKnots\(cvs.length\(\), degree'
forbid_regex "src/modules/curve/DCurve/Utility/DCResample.cpp" 'continue;|firstPt-secondPt|newPts.append'
forbid_regex "src/core/Data/DCurveData.cpp" 'convertedPoints.reserve|MPToDP\(points\[index\]\)'
require_regex "src/core/Data/DCurveData.cpp" 'replacePointArray\(points, points.length\(\), convertedPoints\)'
require_regex "CMakeLists.txt" 'tests/cpp/curve_sampling_tests.cpp'
require_regex "tests/run_cpp_tests.sh" 'tests/cpp/curve_sampling_tests.cpp'
python3 - "$repo_root" <<'PY'
from pathlib import Path
import re
import sys

root = Path(sys.argv[1])
for name in ('DCurveFromSurface', 'DCurveFromPfx', 'DCAhoGen',
             'DCurveCircleGen', 'DCurveLinkGen', 'DCurveFromDP'):
    source = (root / 'src/modules/curve/DCurve/Generate' / (name + '.cpp')).read_text()
    insertions = re.findall(r'\w+->insert\(', source)
    checked = re.findall(r'!\w+->insert\(', source)
    if len(insertions) != 1 or len(checked) != 1:
        raise SystemExit(name + ': publish exactly one checked curve insertion')
data = (root / 'src/core/Data/DCurveData.cpp').read_text()
if data.count('replacePointArray(points, points.length(), convertedPoints)') != 3:
    raise SystemExit('All three Maya-array curve replacements require checked conversion')
surface_input = (root / 'src/core/common/object/DNodeOperation.cpp').read_text()
surface_input = surface_input.split('bool DGetMayaSurfaceFromPlug(', 1)[1].split('bool DGetMayaMeshFromPlug(', 1)[0]
if 'asNurbsSurfaceTransformed()' not in surface_input or 'dataHnd.data()' in surface_input:
    raise SystemExit('Surface readers require explicitly transformed read-only geometry')
resample = (root / 'src/modules/curve/DCurve/Utility/DCResample.cpp').read_text()
failure_path = resample.split('crvFn.rebuild(', 1)[1].split('MFnNurbsCurve newCrvFn', 1)[0]
if not re.search(r'if \(!stat\)\s*return stat;', failure_path) or 'continue' in failure_path:
    raise SystemExit('Curve rebuild failure must abort staged output')

aho = (root / 'src/modules/curve/DCurve/Generate/DCAhoGen.cpp').read_text()
if not re.search(r'setToRotationAxis\(\s*tangent,\s*dommy::math::degreesToRadians\(\s*rollAngles\[', aho):
    raise SystemExit('Aho degree-valued rolls must be converted to radians at the rotation API')
if 'radiansToDegrees' in aho or 'rollRandom.uniform(0.0, 360.0)' not in aho:
    raise SystemExit('Aho must preserve the legacy degree-valued random stream')
for axis in 'XYZ':
    if not re.search(r'std::vector<float> noise' + axis + r'\(plan.pointsPerCurve, 0.0f\)', aho):
        raise SystemExit('Aho noise arrays must have all-or-throw sized storage')
if 'DOMMY_COMPUTE_CATCH("DCAhoGen")' not in aho:
    raise SystemExit('Aho allocation exceptions must terminate the compute safely')

point_curve = (root / 'src/modules/curve/DCurve/Generate/DCurveFromDP.cpp').read_text()
inputs = point_curve.split('MStatus DCurveFromDP::compute(', 1)[1].split('#pragma mark inDP', 1)[0]
for attribute, accessor in (
        ('aGroup', 'asString'), ('aCreateType', 'asShort'),
        ('aDegreeType', 'asShort'), ('aDistanceMethod', 'asShort'),
        ('aRandomMethod', 'asShort'), ('aIsClose', 'asBool'),
        ('aSeed', 'asInt'), ('aStepCount', 'asInt'),
        ('aSearchDistance', 'asDouble'), ('aSearchMaxCount', 'asInt')):
    pattern = (r'handle = data.inputValue\(' + attribute + r', &stat\);\s*'
               r'CHECK_MSTATUS_AND_RETURN_IT\(stat\);\s*const [^;]+ = handle\.' + accessor + r'\(\);')
    if not re.search(pattern, inputs):
        raise SystemExit('DCurveFromDP must check the input status before reading ' + attribute)
for raw in ('rawSeed', 'rawStepCount', 'rawSearchMaxCount'):
    if 'const int ' + raw + ' = handle.asInt();' not in inputs:
        raise SystemExit('DCurveFromDP must retain the signed raw input: ' + raw)
required = (
    'createTypeVal < DC_DP_GEN_ALL', 'createTypeVal >= DC_DP_GEN_TYPE_COUNT',
    'degreeTypeVal != DC_kLinear && degreeTypeVal != DC_kCubic',
    'distanceMethodVal < DC_DISTANCE_ALL', 'distanceMethodVal >= DC_DISTANCE_METHOD_TYPE_COUNT',
    'randomMethodVal < DC_RANDOM_ALL', 'randomMethodVal >= DC_RANDOM_METHOD_TYPE_COUNT',
    'rawSeed < 0', 'stepCountVal < 2U',
    '!std::isfinite(searchDistanceVal)', 'searchDistanceVal < 0.0')
for check in required:
    if check not in inputs:
        raise SystemExit('DCurveFromDP input validation missing: ' + check)
for raw, count in (('rawStepCount', 'stepCountVal'), ('rawSearchMaxCount', 'searchMaxCountVal')):
    if not re.search(r'!dommy::generation::checkedCount\(\s*' + raw + r', maximumInputCount, ' + count + r'\)', inputs):
        raise SystemExit('DCurveFromDP must validate counts before unsigned conversion')
if inputs.index('return MS::kInvalidParameter;') > inputs.index('static_cast<unsigned int>(rawSeed)'):
    raise SystemExit('DCurveFromDP must reject negative seeds before conversion')
if not re.search(r'default:\s*return MS::kInvalidParameter;', point_curve):
    raise SystemExit('DCurveFromDP must not publish empty success for an unknown generation mode')

knots = data.split('bool DCurveInternalData::replaceKnots(', 1)[1].split('bool DCurveInternalData::setReplaceCurveData(', 1)[0]
if not re.search(r'if \(dommy::maya::copyCheckedArray\(\s*replacementKnots, candidate.arrays_->knots\) != MS::kSuccess\)\s*\{\s*return false;', knots):
    raise SystemExit('Knot replacement must check copy status and length on a detached candidate')
commit = 'arrays_->knots[index] = candidate.arrays_->knots[index];'
if ('candidate.arrays_->knots =' in knots or 'copy(candidate)' in knots or
        'candidate.arrays_->knots.length() != arrays_->knots.length()' not in knots or commit not in knots or
        not (knots.index('copyCheckedArray(') < knots.index('candidate.hasValidCurveTopology()') < knots.index(commit))):
    raise SystemExit('Knot replacement must validate the copied candidate before publication')

for path, name, commits in (
        ('src/core/Data/DCurveData', 'DCurveInternalData', 6),
        ('src/modules/mesh/DMesh/Data/DMeshData', 'DMeshInternalData', 4)):
    source = (root / (path + '.cpp')).read_text()
    header = (root / (path + '.h')).read_text()
    if 'std::unique_ptr<ArrayStorage> arrays_' not in header:
        raise SystemExit(name + ': Maya arrays require uniquely owned swappable storage')
    if re.search(r'arrays_->\w+\s*=(?!=)', source):
        raise SystemExit(name + ': unchecked whole Maya-array assignment returned')
    if source.count('swapState(candidate);') != commits:
        raise SystemExit(name + ': every geometry replacement must use its no-allocation commit')
    if 'arrays_.swap(other.arrays_);' not in source or 'swapState(' + name + '& other) noexcept' not in source:
        raise SystemExit(name + ': geometry commit must swap owned array storage')

bend = (root / 'src/modules/curve/DCurve/Utility/DCBend.cpp').read_text()
compute = bend.split('MStatus DCBend::compute(', 1)[1].split('DOMMY_COMPUTE_CATCH', 1)[0]
if re.search(r'closestPoint\([^;]*,\s*true\s*,', compute) or 'length()-1' in compute:
    raise SystemExit('DCBend must not use an uninitialized guess or unsigned length subtraction')
for operation in ('closestPoint', 'setCVs', 'updateCurve', 'setToRotationAxis'):
    if not re.search(r'\.' + operation + r'\([^;]+;\s*CHECK_MSTATUS_AND_RETURN_IT\(status\)', compute):
        raise SystemExit('DCBend must propagate failure from ' + operation)
for check in ('pointCount < 2U', 'points.length() != pointCount',
              'axes.length() != pointCount', 'weights.length() != pointCount',
              'angleType < 0 || angleType > 2', 'double parameter = 0.0',
              'isFloatRepresentablePoint(transformed)', 'appendBendWork('):
    if check not in compute:
        raise SystemExit('DCBend missing boundary: ' + check)
if compute.index('appendBendWork(') > compute.index('createPluginData('):
    raise SystemExit('DCBend must check aggregate work before copying the input')
PY
echo "C++ regression guards: OK"
