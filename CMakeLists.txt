cmake_minimum_required(VERSION 3.15)

project(dommyTool LANGUAGES CXX)

option(DOMMY_BUILD_TESTS "Build Maya-independent safety tests" ON)
option(DOMMY_ENABLE_SANITIZERS "Enable AddressSanitizer and UndefinedBehaviorSanitizer" OFF)
option(DOMMY_ENABLE_DEBUG_LOGS "Enable diagnostic-only C++ logging branches" OFF)
option(DOMMY_ENABLE_ARNOLD_NODES "Include Arnold-dependent nodes and translators" OFF)
option(DOMMY_ENABLE_BULLET_NODES "Include legacy Bullet-dependent nodes" OFF)
option(DOMMY_ENABLE_EXPERIMENTAL_BULLET_TEST_NODE
       "Register the incomplete legacy DMBulletTest locator node" OFF)
option(DOMMY_ENABLE_EXPERIMENTAL_FRACTURE_SOLVER
       "Register the incomplete legacy DBulletFractureSolver node" OFF)
option(DOMMY_ENABLE_EXPERIMENTAL_STRETCH_NODE
       "Register the incomplete legacy DStretchData utility node" OFF)
option(DOMMY_ENABLE_EXPERIMENTAL_ARRAY_TO_COMPOUND_NODE
       "Register the incomplete legacy DPArrayToCompound node" OFF)
option(DOMMY_ENABLE_OPENVDB_NODES
       "Include OpenVDB data, volume, cache, scatter, and spatial-search nodes" OFF)
option(DOMMY_ENABLE_DG_FILE_WRITERS
       "Register dependency-graph nodes that can write external files during evaluation" OFF)
option(DOMMY_ENABLE_JET_NODES "Include the unfinished legacy Jet smoke solver" OFF)
option(DOMMY_ENABLE_ALEMBIC_EXPORT
       "Include the legacy Alembic export command and writer sources" OFF)
option(DOMMY_ENABLE_FASTNOISE_NODES
       "Include nodes backed by an external legacy FastNoise checkout" OFF)
option(DOMMY_ENABLE_FREETYPE_TEXT_NODE
       "Include the FreeType-backed DCText node" OFF)
option(DOMMY_BUILD_MAYA_SDK_CHECK "Compile the registration layer against an installed Maya SDK" OFF)
option(DOMMY_BUILD_FULL_SOURCE_CHECK
       "Compile all active C++ sources by module using the complete legacy SDK stack" OFF)
option(DOMMY_BUILD_FULL_LINK_CHECK
       "Link all active C++ sources and reject unresolved symbols (requires all legacy SDK libraries)" OFF)
option(DOMMY_BUILD_PLUGIN
       "Build and install the archival Maya plug-in (requires the complete legacy SDK stack)" OFF)
set(DOMMY_MAYA_LOCATION "$ENV{MAYA_LOCATION}" CACHE PATH "Maya installation root")

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(DOMMY_ENABLE_DEBUG_LOGS)
    add_compile_definitions(DOMMY_ENABLE_DEBUG_LOGS=1)
endif()

foreach(_dommy_feature IN ITEMS ARNOLD BULLET OPENVDB JET)
    if(DOMMY_ENABLE_${_dommy_feature}_NODES)
        add_compile_definitions(DOMMY_WITH_${_dommy_feature}=1)
    else()
        add_compile_definitions(DOMMY_WITH_${_dommy_feature}=0)
    endif()
endforeach()

if(DOMMY_ENABLE_FASTNOISE_NODES)
    add_compile_definitions(DOMMY_WITH_FASTNOISE=1)
else()
    add_compile_definitions(DOMMY_WITH_FASTNOISE=0)
endif()

if(DOMMY_ENABLE_ALEMBIC_EXPORT)
    add_compile_definitions(DOMMY_WITH_ALEMBIC=1)
else()
    add_compile_definitions(DOMMY_WITH_ALEMBIC=0)
endif()

if(DOMMY_ENABLE_FREETYPE_TEXT_NODE)
    add_compile_definitions(DOMMY_WITH_FREETYPE_TEXT=1)
else()
    add_compile_definitions(DOMMY_WITH_FREETYPE_TEXT=0)
endif()

if(DOMMY_ENABLE_DG_FILE_WRITERS)
    add_compile_definitions(DOMMY_WITH_DG_FILE_WRITERS=1)
else()
    add_compile_definitions(DOMMY_WITH_DG_FILE_WRITERS=0)
endif()

if(DOMMY_ENABLE_EXPERIMENTAL_FRACTURE_SOLVER AND NOT DOMMY_ENABLE_BULLET_NODES)
    message(FATAL_ERROR
        "DOMMY_ENABLE_EXPERIMENTAL_FRACTURE_SOLVER requires DOMMY_ENABLE_BULLET_NODES=ON")
endif()
if(DOMMY_ENABLE_EXPERIMENTAL_BULLET_TEST_NODE AND
   NOT DOMMY_ENABLE_BULLET_NODES)
    message(FATAL_ERROR
        "DOMMY_ENABLE_EXPERIMENTAL_BULLET_TEST_NODE requires DOMMY_ENABLE_BULLET_NODES=ON")
endif()
if(DOMMY_ENABLE_EXPERIMENTAL_BULLET_TEST_NODE)
    add_compile_definitions(
        DOMMY_WITH_EXPERIMENTAL_BULLET_TEST_NODE=1)
else()
    add_compile_definitions(
        DOMMY_WITH_EXPERIMENTAL_BULLET_TEST_NODE=0)
endif()
if(DOMMY_ENABLE_EXPERIMENTAL_FRACTURE_SOLVER)
    add_compile_definitions(DOMMY_WITH_EXPERIMENTAL_FRACTURE_SOLVER=1)
else()
    add_compile_definitions(DOMMY_WITH_EXPERIMENTAL_FRACTURE_SOLVER=0)
endif()
if(DOMMY_ENABLE_EXPERIMENTAL_STRETCH_NODE)
    add_compile_definitions(DOMMY_WITH_EXPERIMENTAL_STRETCH_NODE=1)
else()
    add_compile_definitions(DOMMY_WITH_EXPERIMENTAL_STRETCH_NODE=0)
endif()
if(DOMMY_ENABLE_EXPERIMENTAL_ARRAY_TO_COMPOUND_NODE)
    add_compile_definitions(
        DOMMY_WITH_EXPERIMENTAL_ARRAY_TO_COMPOUND_NODE=1)
else()
    add_compile_definitions(
        DOMMY_WITH_EXPERIMENTAL_ARRAY_TO_COMPOUND_NODE=0)
endif()

set(DOMMY_SOURCE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/src")
set(DOMMY_PLUGIN_ROOT "${DOMMY_SOURCE_ROOT}/plugin")
set(DOMMY_CORE_ROOT "${DOMMY_SOURCE_ROOT}/core")
set(DOMMY_MODULE_ROOT "${DOMMY_SOURCE_ROOT}/modules")
set(DOMMY_THIRD_PARTY_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/third_party")

# The historical source uses source-root-qualified includes such as
# modules/generators/..., module-qualified includes such as DPoint/..., and
# basename-only includes within a module. Keep those compatibility roots in one
# place so archival checks do not depend on the old flat repository layout.
set(DOMMY_LEGACY_INCLUDE_ROOTS
    "${DOMMY_SOURCE_ROOT}"
    "${DOMMY_PLUGIN_ROOT}"
    "${DOMMY_CORE_ROOT}"
    "${DOMMY_CORE_ROOT}/math"
    "${DOMMY_CORE_ROOT}/Utility"
    "${DOMMY_CORE_ROOT}/Utility/3d/Math"
    "${DOMMY_CORE_ROOT}/Utility/3d/DataStruct"
    "${DOMMY_MODULE_ROOT}/point"
    "${DOMMY_MODULE_ROOT}/point/DPoint"
    "${DOMMY_MODULE_ROOT}/curve"
    "${DOMMY_MODULE_ROOT}/mesh"
    "${DOMMY_MODULE_ROOT}/volume"
    "${DOMMY_MODULE_ROOT}/dynamics"
    "${DOMMY_MODULE_ROOT}/particles"
    "${DOMMY_MODULE_ROOT}/deformers"
    "${DOMMY_MODULE_ROOT}/generators"
    "${DOMMY_MODULE_ROOT}/rendering"
    "${DOMMY_SOURCE_ROOT}/commands"
    "${DOMMY_SOURCE_ROOT}/integrations"
    "${DOMMY_THIRD_PARTY_ROOT}/libnoise/source"
    "${DOMMY_THIRD_PARTY_ROOT}/fastnoise_simd/source"
    "${DOMMY_THIRD_PARTY_ROOT}/voro++"
)

function(dommy_enable_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endfunction()

function(dommy_enable_sanitizers target)
    if(NOT DOMMY_ENABLE_SANITIZERS)
        return()
    endif()

    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(${target} PRIVATE
            -fsanitize=address,undefined,float-cast-overflow
            -fno-sanitize-recover=all -fno-omit-frame-pointer)
        target_link_options(${target} PRIVATE
            -fsanitize=address,undefined,float-cast-overflow
            -fno-sanitize-recover=all)
    else()
        message(FATAL_ERROR "DOMMY_ENABLE_SANITIZERS requires Clang or GCC")
    endif()
endfunction()

if(DOMMY_BUILD_TESTS)
    include(CTest)

    add_executable(dommy_core_safety_tests
        tests/cpp/math_memory_tests.cpp
        tests/cpp/attribute_cast_group_tests.cpp
        tests/cpp/expression_sort_tests.cpp
        tests/cpp/point_generation_tests.cpp
        tests/cpp/random_sampling_tests.cpp
        tests/cpp/blue_noise_tests.cpp
        tests/cpp/attribute_access_tests.cpp
        tests/cpp/point_storage_tests.cpp
        tests/cpp/curve_sampling_tests.cpp
        tests/cpp/kill_field_math_tests.cpp
        tests/cpp/utility_vector_tests.cpp
        tests/cpp/atomic_output_tests.cpp
        tests/cpp/io_locale_tests.cpp
        src/core/DDataUtility/Data/DPGroupData.cpp
        tests/cpp/geometry_tests.cpp
        tests/cpp/mesh_boundary_tests.cpp
        tests/cpp/compute_guard_compile_test.cpp
        tests/cpp/poisson_header_compile_test.cpp
        tests/cpp/vdb_compute_guard_compile_test.cpp
        src/core/math/dommyMath.cpp
        src/core/common/object/DLine3DOp.cpp
        src/core/common/object/DPlaneOp.cpp
        src/core/noise/PerlinNoise.cpp
        src/modules/dynamics/DBullet/File/pdbIO.cpp
    )
    target_include_directories(dommy_core_safety_tests PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/stubs"
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
        "${DOMMY_CORE_ROOT}/noise/Poisson"
        "${DOMMY_MODULE_ROOT}/dynamics/DBullet/File"
        ${DOMMY_LEGACY_INCLUDE_ROOTS}
    )
    dommy_enable_warnings(dommy_core_safety_tests)
    # The standalone suite intentionally uses assert for executable contracts.
    # Keep those checks active even when CI configures a Release build.
    if(MSVC)
        target_compile_options(dommy_core_safety_tests PRIVATE /UNDEBUG)
    else()
        target_compile_options(dommy_core_safety_tests PRIVATE -UNDEBUG)
    endif()
    dommy_enable_sanitizers(dommy_core_safety_tests)

    add_test(NAME core_safety COMMAND dommy_core_safety_tests)
    add_test(NAME polygon_algorithm_safety
        COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_polygon_algorithm_safety.py")
    add_test(NAME slicer_algorithm_safety
        COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_slicer_algorithm_safety.py")
    add_test(NAME remesh_algorithm_safety
        COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_remesh_algorithm_safety.py")
    add_test(NAME affine_numeric_safety
        COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_affine_numeric_safety.py")
    add_test(NAME normal_numeric_safety
        COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_normal_numeric_safety.py")
    add_test(NAME force_range_numeric_safety
        COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_force_range_numeric_safety.py")
    add_test(NAME signal_compute_safety
        COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_signal_compute_safety.py")
    add_test(NAME signal_blend_numeric_safety
        COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_signal_blend_numeric_safety.py")
    add_test(NAME surface_uv_wrapping
        COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_surface_uv_wrapping.py")
    add_test(NAME surface_scratch_safety
        COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_surface_scratch_safety.py")
    add_test(NAME wire_array_safety
        COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_wire_array_safety.py")
    add_test(NAME plugin_registration_safety
        COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_plugin_registration_safety.py")
    add_test(NAME plugin_initialization_safety
        COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_plugin_initialization_safety.py")
    add_test(NAME host_factory_safety
        COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_host_factory_safety.py")
    add_test(NAME command_syntax_safety
        COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_command_syntax_safety.py")
    add_test(NAME command_transactions
        COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_command_transactions.py")
    add_test(NAME point_export_safety
        COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_point_export_safety.py")
    add_test(NAME poly_modifier_transactions
        COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_poly_modifier_transactions.py")
    add_test(NAME vdb_grid_dispatch
        COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_vdb_grid_dispatch.py")
    add_test(NAME third_party_notice_coverage
        COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_third_party_notices.py")
    add_test(NAME owned_utility_api
        COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_owned_utility_api.py")
    add_test(NAME owned_attribute_api
        COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_owned_attribute_api.py")
    add_test(NAME viewport_staging_safety
        COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_viewport_staging_safety.py")
    add_test(NAME numeric_to_float_safety
        COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_numeric_to_float_safety.py")
    add_test(NAME packing_input_safety
        COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_packing_input_safety.py")
    add_test(NAME draw_callback_safety
        COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_draw_callback_safety.py")
    add_test(NAME light_draw_preparation_safety
        COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_light_draw_preparation_safety.py")
    add_test(NAME display_metadata_safety
        COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_display_metadata_safety.py")
    add_test(NAME arnold_export_safety
        COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_arnold_export_safety.py")
    add_test(NAME arnold_light_admission_safety
        COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_export_input_safety.py")
    add_test(NAME arnold_translation_safety
        COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_arnold_translation_safety.py")
    add_test(NAME arnold_mesh_metadata_safety
        COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_arnold_mesh_metadata_safety.py")
    add_test(NAME bullet_connection_safety
        COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_bullet_connection_safety.py")
    add_test(NAME archival_connection_safety
        COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_archival_connection_safety.py")
    add_test(NAME alembic_input_admission_safety
        COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_export_input_safety.py" --alembic)
    add_test(NAME viewport_attribute_conversion
        COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_viewport_attribute_conversion.py")
    add_test(NAME vdb_viewport_clip_safety
        COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_vdb_viewport_clip_safety.py")
    add_test(NAME vdb_viewport_safety
        COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_vdb_viewport_safety.py")
    add_test(NAME alembic_attribute_safety
        COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_alembic_attribute_safety.py")
    add_test(NAME alembic_sample_safety
        COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_alembic_sample_safety.py")
    add_test(NAME alembic_transform_safety
        COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_alembic_transform_safety.py")
    add_test(NAME alembic_root_safety
        COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_alembic_root_safety.py")
    add_test(NAME alembic_callback_safety
        COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_alembic_callback_safety.py")
    add_test(NAME alembic_geometry_safety
        COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_alembic_geometry_safety.py")
    add_test(NAME alembic_destination_safety
        COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_alembic_destination_safety.py")
    add_test(NAME alembic_job_bounds_safety
        COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_alembic_job_bounds_safety.py")
    add_test(NAME alembic_attribute_state_safety
        COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_alembic_attribute_state_safety.py")
    add_test(NAME bullet_input_budget
        COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_bullet_input_budget.py")
    add_test(NAME nrigid_safety
        COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_nrigid_safety.py")
    add_test(NAME nucleus_particle_safety
        COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_nucleus_particle_safety.py")
    add_test(
        NAME noise_safety
        COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_noise_safety_paths.py"
    )
    add_test(NAME owned_noise_fields
        COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_owned_noise_fields.py")
    add_test(
        NAME deformer_safety
        COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_deformer_safety_paths.py"
    )
    add_test(
        NAME noise_deformer_initialization
        COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_noise_deformer_initialization.py"
    )
    add_test(
        NAME curl_field_safety
        COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_curl_field_safety.py"
    )
    add_test(
        NAME curve_field_safety
        COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_curve_field_safety.py"
    )
    add_test(NAME vdb_field_safety
        COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_vdb_field_safety.py")
    add_test(
        NAME particle_boundary_safety
        COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_particle_boundary_safety.py"
    )
    add_test(
        NAME fluid_emitter_safety
        COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_fluid_emitter_safety.py"
    )
    add_test(
        NAME dpoint_field_safety
        COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_dpoint_field_safety.py"
    )
    add_test(
        NAME particle_output_safety
        COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_particle_output_safety.py"
    )
    add_test(
        NAME swept_emitter_safety
        COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_swept_emitter_safety.py"
    )
    add_test(
        NAME mesh_particle_emitter_safety
        COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_mesh_particle_emitter_safety.py"
    )
    add_test(
        NAME bullet_mesh_input_safety
        COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_bullet_mesh_input_safety.py"
    )
    add_test(
        NAME bullet_shape_safety
        COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_bullet_shape_safety.py"
    )
    add_test(
        NAME bullet_soft_body_safety
        COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_bullet_soft_body_safety.py"
    )
    add_test(
        NAME bullet_constraint_safety
        COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_bullet_constraint_safety.py"
    )
    add_test(
        NAME bullet_step_safety
        COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_bullet_step_safety.py"
    )
    add_test(
        NAME bullet_dm_safety
        COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_bullet_dm_safety.py"
    )
    add_test(
        NAME bullet_fracture_evaluation
        COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_bullet_fracture_evaluation.py"
    )
    add_test(
        NAME delay_transform_safety
        COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_delay_transform_safety.py"
    )
    add_test(
        NAME feedback_trail_safety
        COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_feedback_trail_safety.py"
    )
    add_test(
        NAME curve_resample_safety
        COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_curve_resample_safety.py"
    )
    add_test(
        NAME arc_remake_safety
        COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_arc_remake_safety.py"
    )
    add_test(
        NAME component_sequencer_safety
        COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_component_sequencer_safety.py"
    )
    add_test(
        NAME rig_utility_safety
        COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_rig_utility_safety.py"
    )
    add_test(
        NAME curvature_face_safety
        COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_curvature_face_safety.py"
    )
    add_test(
        NAME array_node_safety
        COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_array_node_safety.py"
    )
    add_test(
        NAME scalar_array_safety
        COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_scalar_array_safety.py"
    )
    add_test(
        NAME array_utility_safety
        COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_array_utility_safety.py"
    )
    add_test(
        NAME motion_rotation_safety
        COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_motion_rotation_safety.py"
    )
    add_test(
        NAME frame_transform_safety
        COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_frame_transform_safety.py"
    )
    add_test(
        NAME animation_boundary_safety
        COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_animation_boundary_safety.py"
    )
    add_test(
        NAME curve_boundary_safety
        COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_curve_boundary_safety.py"
    )
    add_test(
        NAME curve_copy_safety
        COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_curve_copy_safety.py"
    )
    add_test(
        NAME mesh_curve_copy_safety
        COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_mesh_curve_copy_safety.py"
    )
    add_test(
        NAME curve_utility_safety
        COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_curve_utility_safety.py"
    )
    add_test(
        NAME curve_path_safety
        COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_curve_path_safety.py"
    )
    add_test(
        NAME curve_periodic_safety
        COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_curve_periodic_safety.py"
    )
    add_test(
        NAME point_foam_safety
        COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_point_foam_safety.py"
    )
    add_test(
        NAME procedural_api_safety
        COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_procedural_api_safety.py"
    )
    add_test(
        NAME mesh_duplication_vertices
        COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_mesh_duplication_vertices.py"
    )
    add_test(
        NAME attribute_curve_constraint_safety
        COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_attribute_curve_constraint_safety.py"
    )
    add_test(
        NAME vdb_writer_status_safety
        COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_vdb_writer_status_safety.py"
    )
    add_test(
        NAME attribute_vdb_read_status
        COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_attribute_vdb_read_status.py"
    )
    add_test(
        NAME curve_extension_safety
        COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_curve_extension_safety.py"
    )
    add_test(
        NAME curve_collision_safety
        COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_curve_collision_safety.py"
    )
    add_test(
        NAME curve_partition_safety
        COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_curve_partition_safety.py"
    )
    add_test(
        NAME curve_weight_map_safety
        COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_curve_weight_map_safety.py"
    )
    add_test(
        NAME curve_emitter_safety
        COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_curve_emitter_safety.py"
    )
    add_test(
        NAME default_ramp_safety
        COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_default_ramp_safety.py"
    )
    add_test(
        NAME curve_uv_safety
        COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_curve_uv_safety.py"
    )
    add_test(
        NAME mesh_emitter_safety
        COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_mesh_emitter_safety.py"
    )
    add_test(
        NAME circle_surface_emitter_safety
        COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_circle_surface_emitter_safety.py"
    )
    add_test(
        NAME packing_emitter_safety
        COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_packing_emitter_safety.py"
    )
    add_test(
        NAME geometry_read_safety
        COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_geometry_read_safety.py"
    )
    add_test(
        NAME timeshift_safety
        COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_timeshift_safety.py"
    )
    add_test(
        NAME mesh_generation_safety
        COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_mesh_generation_safety.py"
    )
    add_test(
        NAME attribute_validation_safety
        COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_attribute_validation_safety.py"
    )
    add_test(
        NAME mesh_merge_safety
        COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_mesh_merge_safety.py"
    )
    add_test(
        NAME curve_transactions
        COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_curve_transactions.py"
    )
    add_test(
        NAME mesh_copy_safety
        COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_mesh_copy_safety.py"
    )
    add_test(
        NAME mesh_utility_safety
        COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_mesh_utility_safety.py"
    )
    add_test(
        NAME mesh_refinement_safety
        COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_mesh_refinement_safety.py"
    )
    add_test(
        NAME mesh_controls_safety
        COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_mesh_controls_safety.py"
    )
    add_test(
        NAME mesh_remesh_safety
        COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_mesh_remesh_safety.py"
    )
    add_test(
        NAME mesh_noise_initialization
        COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_mesh_noise_initialization.py"
    )
    add_test(
        NAME mesh_group_safety
        COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_mesh_group_safety.py"
    )
    add_test(
        NAME primitive_metadata_safety
        COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_primitive_metadata_safety.py"
    )
    add_test(
        NAME group_transfer_safety
        COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_group_transfer_safety.py"
    )
    add_test(
        NAME point_utility_safety
        COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_point_utility_safety.py"
    )
    add_test(
        NAME point_batch_transactions
        COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_point_batch_transactions.py"
    )
    add_test(
        NAME copy_utility_safety
        COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_copy_utility_safety.py"
    )
    add_test(
        NAME point_edit_safety
        COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_point_edit_safety.py"
    )
    add_test(
        NAME direction_search_safety
        COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_direction_search_safety.py"
    )
    add_test(
        NAME point_force_safety
        COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_point_force_safety.py"
    )
    add_test(
        NAME point_coordinate_output
        COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_point_coordinate_output.py"
    )
    add_test(
        NAME point_noise_output
        COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_point_noise_output.py"
    )
    add_test(
        NAME point_deformation_output
        COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_point_deformation_output.py"
    )
    add_test(
        NAME collision_distance_safety
        COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_collision_distance_safety.py"
    )
    add_test(
        NAME affect_spiral_safety
        COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_affect_spiral_safety.py"
    )
    add_test(
        NAME ray_safety
        COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_ray_safety.py"
    )
    add_test(
        NAME color_noise_safety
        COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_color_noise_safety.py"
    )
    add_test(
        NAME point_cache_safety
        COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_point_cache_safety.py"
    )
    add_test(
        NAME vdb_point_bridge_safety
        COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_vdb_point_bridge_safety.py"
    )
    add_test(
        NAME vdb_smoothing_safety
        COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_vdb_smoothing_safety.py"
    )
    add_test(
        NAME vdb_vector_merge_safety
        COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_vdb_vector_merge_safety.py"
    )
    add_test(
        NAME vdb_edit_safety
        COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_vdb_edit_safety.py"
    )
    add_test(
        NAME vdb_noise_safety
        COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_vdb_noise_safety.py"
    )
    add_test(
        NAME vdb_filter_safety
        COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_vdb_filter_safety.py"
    )
    add_test(
        NAME vdb_value_safety
        COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_vdb_value_safety.py"
    )
    add_test(
        NAME vdb_transport_safety
        COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_vdb_transport_safety.py"
    )
    add_test(
        NAME vdb_point_advection_safety
        COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_vdb_point_advection_safety.py"
    )
    add_test(
        NAME vdb_fill_safety
        COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_vdb_fill_safety.py"
    )
    add_test(
        NAME vdb_clip_safety
        COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_vdb_clip_safety.py"
    )
    add_test(
        NAME vdb_analysis_safety
        COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_vdb_analysis_safety.py"
    )
    add_test(
        NAME vdb_divergence_safety
        COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_vdb_divergence_safety.py"
    )
    add_test(
        NAME vdb_potential_safety
        COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_vdb_potential_safety.py"
    )
    add_test(
        NAME vdb_resample_safety
        COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_vdb_resample_safety.py"
    )
    add_test(
        NAME vdb_generation_safety
        COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_vdb_generation_safety.py"
    )
    add_test(
        NAME vdb_distribution_safety
        COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_vdb_distribution_safety.py"
    )
    add_test(
        NAME vdb_mesh_safety
        COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_vdb_mesh_safety.py"
    )
    add_test(
        NAME vdb_meshing_safety
        COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_vdb_meshing_safety.py"
    )
    add_test(
        NAME vdb_particle_safety
        COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_vdb_particle_safety.py"
    )
    add_test(
        NAME mesh_normal_safety
        COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_mesh_normal_safety.py"
    )
    add_test(
        NAME mesh_convert_safety
        COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_mesh_convert_safety.py"
    )
    add_test(
        NAME motion_history_safety
        COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_motion_history_safety.py"
    )
    add_test(
        NAME motion_duplicator_safety
        COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_motion_duplicator_safety.py"
    )
    add_test(
        NAME motion_trail_safety
        COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_motion_trail_safety.py"
    )
    add_test(
        NAME generator_boundary_safety
        COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_generator_boundary_safety.py"
    )
    add_test(
        NAME fur_aho_safety
        COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_fur_aho_safety.py"
    )
    add_test(
        NAME curve_generation_safety
        COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_curve_generation_safety.py"
    )
    add_test(
        NAME curve_source_generation_safety
        COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_curve_source_generation_safety.py"
    )
    add_test(
        NAME curve_conversion_safety
        COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_curve_conversion_safety.py"
    )
    add_test(
        NAME legacy_mesh_safety
        COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_legacy_mesh_safety.py"
    )
    add_test(
        NAME poly_component_frame_safety
        COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_poly_component_frame_safety.py"
    )
    add_test(
        NAME legacy_io_foam_safety
        COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_legacy_io_foam_safety.py"
    )
    add_test(
        NAME mesh_color_safety
        COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_mesh_color_safety.py"
    )
    add_test(
        NAME face_charactor_safety
        COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_face_charactor_safety.py"
    )
    add_test(
        NAME point_deform_safety
        COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_point_deform_safety.py"
    )
    add_test(
        NAME convert_weights_safety
        COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_convert_weights_safety.py"
    )
    add_test(
        NAME adjust_space_safety
        COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_adjust_space_safety.py"
    )
    add_test(
        NAME point_uv_mesh_safety
        COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_point_uv_mesh_safety.py"
    )
    add_test(
        NAME voronoi_safety
        COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_voronoi_safety.py"
    )
    add_test(
        NAME polygon_generation_safety
        COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_polygon_generation_safety.py"
    )
    add_test(
        NAME maya_data_ownership
        COMMAND bash "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_maya_data_ownership.sh"
    )
    add_test(
        NAME source_safety
        COMMAND bash "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_source_safety.sh"
    )
    add_test(
        NAME build_configuration
        COMMAND bash "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_build_configuration.sh"
    )
    add_test(
        NAME active_source_manifest
        COMMAND bash "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_active_source_manifest.sh"
    )
    add_test(
        NAME active_cpp_safety
        COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_active_cpp_safety.py"
    )
    add_test(
        NAME cpp_regressions
        COMMAND bash "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_cpp_regressions.sh" --source-only
    )
    add_test(NAME clang_static_analysis_gate
        COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_clang_static_analysis_gate.py")
    add_test(NAME path_reservation_exception_safety
        COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_path_reservation_exception_safety.py")
    add_test(NAME sequence_failure_safety
        COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_sequence_failure_safety.py")
    add_test(NAME text_formatting_safety
        COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_text_formatting_safety.py")
    add_test(NAME parser_codec_safety
        COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_parser_codec_safety.py")
    add_test(NAME utility_scalar_safety
        COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_utility_scalar_safety.py")
    add_test(NAME diagnostic_text_safety
        COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_diagnostic_text_safety.py")
    # Previously these guards ran only inside the aggregate shell suite.
    # Register them individually before disabling its duplicated runtime calls.
    add_test(NAME attribute_publication_paths
        COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_attribute_publication_paths.py")
    add_test(NAME geometry_state_transactions
        COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_geometry_state_transactions.py")
    add_test(NAME curve_deformation_paths
        COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_curve_deformation_paths.py")
    add_test(NAME curve_connection_queries
        COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_curve_connection_queries.py")
    add_test(NAME curve_world_replacement_paths
        COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_curve_world_replacement_paths.py")
    add_test(NAME curve_fill_vector_paths
        COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_curve_fill_vector_paths.py")
    add_test(NAME cpp_test_inventory
        COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_cpp_test_inventory.py"
            --build-dir "${CMAKE_CURRENT_BINARY_DIR}")
    add_test(
        NAME maya_node_contracts
        COMMAND bash "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_maya_node_contracts.sh"
    )
    add_test(
        NAME publication_readiness
        COMMAND bash "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_publication_readiness.sh"
    )
    add_test(
        NAME repository_layout
        COMMAND bash "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_repository_layout.sh"
    )
    if(DOMMY_ENABLE_SANITIZERS)
        if(APPLE)
            set_tests_properties(core_safety PROPERTIES ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0")
        else()
            set_tests_properties(core_safety PROPERTIES ENVIRONMENT "ASAN_OPTIONS=detect_leaks=1")
        endif()
    endif()
endif()

if(DOMMY_BUILD_FULL_SOURCE_CHECK OR DOMMY_BUILD_FULL_LINK_CHECK OR DOMMY_BUILD_PLUGIN)
    include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/DommyFullSourceCheck.cmake")
endif()

if(DOMMY_BUILD_MAYA_SDK_CHECK)
    if(NOT EXISTS "${DOMMY_MAYA_LOCATION}/include/maya/MFnPlugin.h")
        message(FATAL_ERROR
            "Maya SDK headers were not found. Set DOMMY_MAYA_LOCATION or MAYA_LOCATION.")
    endif()

    add_library(dommy_maya_sdk_registration_check OBJECT
        tests/cpp/maya_sdk_registration_compile_test.cpp
        tests/cpp/maya_node_contract_compile_test.cpp
        tests/cpp/maya_particle_field_compile_test.cpp
        tests/cpp/maya_fluid_grid_compile_test.cpp
    )
    target_include_directories(dommy_maya_sdk_registration_check PRIVATE
        ${DOMMY_LEGACY_INCLUDE_ROOTS}
    )
    target_include_directories(dommy_maya_sdk_registration_check SYSTEM PRIVATE
        "${DOMMY_MAYA_LOCATION}/include"
    )
    dommy_enable_warnings(dommy_maya_sdk_registration_check)
endif()
