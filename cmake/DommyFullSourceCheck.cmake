# Full archival source compilation is intentionally opt-in. It requires the
# Maya 2019-era SDK and the complete matching dependency stack. The optional
# link check verifies symbol closure but does not claim runtime compatibility.

set(DOMMY_DEPENDENCY_PREFIX "$ENV{DOMMY_DEPENDENCY_PREFIX}" CACHE PATH
    "Prefix containing Bullet, OpenVDB, OpenSubdiv, Boost, TBB, and related headers")
set(DOMMY_LEGACY_PREFIX "$ENV{DOMMY_LEGACY_PREFIX}" CACHE PATH
    "Prefix containing legacy SeExpr and related headers")
set(DOMMY_MAYA_DEVKIT_LOCATION "$ENV{MAYA_DEVKIT_LOCATION}" CACHE PATH
    "Maya DevKit root")
set(DOMMY_ARNOLD_LOCATION "$ENV{ARNOLD_SDK_LOCATION}" CACHE PATH
    "Arnold SDK root")
set(DOMMY_MTOA_LOCATION "$ENV{MTOA_LOCATION}" CACHE PATH
    "MtoA SDK root")
set(DOMMY_JET_LOCATION "$ENV{JET_LOCATION}" CACHE PATH
    "Jet framework root")
set(DOMMY_FASTNOISE_LOCATION "$ENV{FASTNOISE_LOCATION}" CACHE PATH
    "Directory containing FastNoise headers")
set(DOMMY_FREETYPE_LOCATION "$ENV{FREETYPE_LOCATION}" CACHE PATH
    "FreeType installation prefix")
set(DOMMY_LEGACY_LINK_LIBRARIES "" CACHE STRING
    "Semicolon-separated libraries needed by the full link check or plug-in build")

set(_dommy_required_paths
    "${DOMMY_MAYA_LOCATION}/include/maya/MFnPlugin.h"
    "${DOMMY_DEPENDENCY_PREFIX}/include/opensubdiv/far/topologyRefiner.h"
    "${DOMMY_DEPENDENCY_PREFIX}/include/boost/version.hpp"
)
if(DOMMY_ENABLE_ALEMBIC_EXPORT)
    list(APPEND _dommy_required_paths
        "${DOMMY_MAYA_DEVKIT_LOCATION}/include/Alembic/AbcGeom/All.h")
endif()
if(DOMMY_ENABLE_FASTNOISE_NODES)
    list(APPEND _dommy_required_paths
        "${DOMMY_FASTNOISE_LOCATION}/FastNoise.h"
        "${DOMMY_FASTNOISE_LOCATION}/FastNoise.cpp")
endif()
if(DOMMY_ENABLE_FREETYPE_TEXT_NODE)
    if(EXISTS "${DOMMY_FREETYPE_LOCATION}/include/freetype2/ft2build.h")
        set(DOMMY_FREETYPE_INCLUDE_DIR
            "${DOMMY_FREETYPE_LOCATION}/include/freetype2")
    elseif(EXISTS "${DOMMY_FREETYPE_LOCATION}/include/ft2build.h")
        set(DOMMY_FREETYPE_INCLUDE_DIR
            "${DOMMY_FREETYPE_LOCATION}/include")
    elseif(EXISTS "${DOMMY_DEPENDENCY_PREFIX}/include/freetype2/ft2build.h")
        set(DOMMY_FREETYPE_INCLUDE_DIR
            "${DOMMY_DEPENDENCY_PREFIX}/include/freetype2")
    else()
        message(FATAL_ERROR
            "DOMMY_ENABLE_FREETYPE_TEXT_NODE requires ft2build.h. "
            "Set FREETYPE_LOCATION or DOMMY_FREETYPE_LOCATION.")
    endif()
endif()
if(DOMMY_ENABLE_OPENVDB_NODES)
    list(APPEND _dommy_required_paths
        "${DOMMY_DEPENDENCY_PREFIX}/include/openvdb/openvdb.h")
endif()
if(DOMMY_ENABLE_BULLET_NODES)
    list(APPEND _dommy_required_paths
        "${DOMMY_DEPENDENCY_PREFIX}/include/bullet/btBulletDynamicsCommon.h")
endif()
if(DOMMY_ENABLE_ARNOLD_NODES)
    list(APPEND _dommy_required_paths
        "${DOMMY_ARNOLD_LOCATION}/include/ai.h"
        "${DOMMY_MTOA_LOCATION}/include/extension/Extension.h")
endif()
if(DOMMY_ENABLE_JET_NODES)
    list(APPEND _dommy_required_paths
        "${DOMMY_JET_LOCATION}/include/jet/jet.h")
endif()
foreach(_dommy_path IN LISTS _dommy_required_paths)
    if(NOT EXISTS "${_dommy_path}")
        message(FATAL_ERROR
            "The requested full-source target requires ${_dommy_path}. "
            "Configure the paths documented in config/legacy-build.env.example.")
    endif()
endforeach()

file(GLOB_RECURSE DOMMY_PROJECT_HEADERS CONFIGURE_DEPENDS
    "${DOMMY_SOURCE_ROOT}/*.h"
    "${DOMMY_SOURCE_ROOT}/*.hpp"
    "${DOMMY_SOURCE_ROOT}/*.inl"
)
set(DOMMY_PROJECT_INCLUDE_DIRS ${DOMMY_LEGACY_INCLUDE_ROOTS})
foreach(_dommy_header IN LISTS DOMMY_PROJECT_HEADERS)
    get_filename_component(_dommy_header_dir "${_dommy_header}" DIRECTORY)
    list(APPEND DOMMY_PROJECT_INCLUDE_DIRS "${_dommy_header_dir}")
endforeach()
list(REMOVE_DUPLICATES DOMMY_PROJECT_INCLUDE_DIRS)

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/DommyActiveSources.cmake")

set(DOMMY_EXTERNAL_INCLUDE_DIRS
    "${DOMMY_MAYA_LOCATION}/include"
    "${DOMMY_MAYA_DEVKIT_LOCATION}/include"
    "${DOMMY_DEPENDENCY_PREFIX}/include"
    "${DOMMY_DEPENDENCY_PREFIX}/include/bullet"
    "${DOMMY_DEPENDENCY_PREFIX}/include/OpenEXR"
    "${DOMMY_LEGACY_PREFIX}/include"
)
if(DOMMY_ENABLE_FASTNOISE_NODES)
    list(APPEND DOMMY_EXTERNAL_INCLUDE_DIRS "${DOMMY_FASTNOISE_LOCATION}")
endif()
if(DOMMY_ENABLE_FREETYPE_TEXT_NODE)
    list(APPEND DOMMY_EXTERNAL_INCLUDE_DIRS "${DOMMY_FREETYPE_INCLUDE_DIR}")
endif()
if(DOMMY_ENABLE_ARNOLD_NODES)
    list(APPEND DOMMY_EXTERNAL_INCLUDE_DIRS
        "${DOMMY_ARNOLD_LOCATION}/include"
        "${DOMMY_MTOA_LOCATION}/include")
endif()
if(DOMMY_ENABLE_JET_NODES)
    list(APPEND DOMMY_EXTERNAL_INCLUDE_DIRS
        "${DOMMY_JET_LOCATION}/include")
endif()

function(dommy_collect_active_cpp output root)
    set(_dommy_sources)
    foreach(_dommy_source IN LISTS DOMMY_ACTIVE_CPP_SOURCES)
        string(FIND "${_dommy_source}" "${root}/" _dommy_root_prefix)
        if(_dommy_root_prefix EQUAL 0)
            list(APPEND _dommy_sources "${_dommy_source}")
        endif()
    endforeach()
    if(NOT DOMMY_ENABLE_ARNOLD_NODES)
        list(FILTER _dommy_sources EXCLUDE REGEX
            "/(DArnoldExtentionPlugins|DWriteDCurveToAi|DPWriteToAi|DPInstancer|DPLightGen|[^/]*TranslatorAi)\\.cpp$")
    endif()
    if(NOT DOMMY_ENABLE_BULLET_NODES)
        list(FILTER _dommy_sources EXCLUDE REGEX "/DBullet/")
        list(FILTER _dommy_sources EXCLUDE REGEX "/DMesh/Dynamics/Bullet/")
        # DCRest is a curve-space utility retained in the historical Bullet
        # folder, but it has no Bullet API dependency and remains registered.
        if(EXISTS "${root}/DMesh/Dynamics/Bullet/DCRest.cpp")
            list(APPEND _dommy_sources
                "${root}/DMesh/Dynamics/Bullet/DCRest.cpp")
        endif()
    elseif(NOT DOMMY_ENABLE_EXPERIMENTAL_FRACTURE_SOLVER)
        # These three implementations form one unfinished prototype. Keeping
        # them in a normal Bullet build created dead symbols and implied a
        # support level that the default registration gate does not provide.
        list(FILTER _dommy_sources EXCLUDE REGEX
            "/(DBulletFractureSolver|DBFractureDynamicsWorld|dbtFractureBody)\\.cpp$")
    endif()
    if(NOT DOMMY_ENABLE_OPENVDB_NODES)
        list(FILTER _dommy_sources EXCLUDE REGEX
            "/(DVdbOp|DPAttributeVDB|DPAttrSmooth|DPScatter|DPointPlaneEmit|DPointVolumeEmit|DPCache|DPClosestPointVDB|DPSelfCollision)\\.cpp$")
    endif()
    if(NOT DOMMY_ENABLE_ALEMBIC_EXPORT)
        list(FILTER _dommy_sources EXCLUDE REGEX
            "/DCurve/Cmd/(DExportAbcCmd|DDataExportUtil)\\.cpp$")
        list(FILTER _dommy_sources EXCLUDE REGEX
            "/DCurve/Cmd/Abc/")
    endif()
    if(NOT DOMMY_ENABLE_FASTNOISE_NODES)
        list(FILTER _dommy_sources EXCLUDE REGEX
            "/(DPFastNoise|DMFastNoise)\\.cpp$")
    endif()
    if(NOT DOMMY_ENABLE_FREETYPE_TEXT_NODE)
        list(FILTER _dommy_sources EXCLUDE REGEX "/DCText\\.cpp$")
    endif()
    set(${output} ${_dommy_sources} PARENT_SCOPE)
endfunction()

function(dommy_add_source_check target root)
    dommy_collect_active_cpp(_dommy_sources "${root}")
    if(NOT _dommy_sources)
        return()
    endif()

    add_library(${target} OBJECT EXCLUDE_FROM_ALL ${_dommy_sources})
    target_include_directories(${target} PRIVATE ${DOMMY_PROJECT_INCLUDE_DIRS})
    target_include_directories(${target} SYSTEM PRIVATE ${DOMMY_EXTERNAL_INCLUDE_DIRS})
    dommy_enable_warnings(${target})
    if(NOT MSVC)
        target_compile_options(${target} PRIVATE -Wno-deprecated-declarations)
    endif()
    set_property(GLOBAL APPEND PROPERTY DOMMY_FULL_SOURCE_CHECK_TARGETS ${target})
    set_property(GLOBAL APPEND PROPERTY DOMMY_FULL_SOURCE_CHECK_SOURCES ${_dommy_sources})
endfunction()

dommy_add_source_check(dommy_core_source_check "${DOMMY_CORE_ROOT}")
dommy_add_source_check(dommy_point_source_check "${DOMMY_MODULE_ROOT}/point")
dommy_add_source_check(dommy_curve_source_check "${DOMMY_MODULE_ROOT}/curve")
dommy_add_source_check(dommy_mesh_source_check "${DOMMY_MODULE_ROOT}/mesh")
if(DOMMY_ENABLE_OPENVDB_NODES)
    dommy_add_source_check(dommy_volume_source_check "${DOMMY_MODULE_ROOT}/volume")
endif()
if(DOMMY_ENABLE_BULLET_NODES)
    dommy_add_source_check(dommy_dynamics_source_check "${DOMMY_MODULE_ROOT}/dynamics")
endif()
dommy_add_source_check(dommy_particles_source_check "${DOMMY_MODULE_ROOT}/particles")
dommy_add_source_check(dommy_deformers_source_check "${DOMMY_MODULE_ROOT}/deformers")
dommy_add_source_check(dommy_generators_source_check "${DOMMY_MODULE_ROOT}/generators")
dommy_add_source_check(dommy_rendering_source_check "${DOMMY_MODULE_ROOT}/rendering")
dommy_add_source_check(dommy_commands_source_check "${DOMMY_SOURCE_ROOT}/commands")
dommy_add_source_check(dommy_integrations_source_check "${DOMMY_SOURCE_ROOT}/integrations")
dommy_add_source_check(dommy_plugin_source_check "${DOMMY_PLUGIN_ROOT}")
dommy_add_source_check(dommy_libnoise_source_check "${DOMMY_THIRD_PARTY_ROOT}/libnoise/source")
dommy_add_source_check(dommy_voro_source_check "${DOMMY_THIRD_PARTY_ROOT}/voro++")

if(DOMMY_ENABLE_FASTNOISE_NODES)
    add_library(dommy_fastnoise_source_check OBJECT EXCLUDE_FROM_ALL
        "${DOMMY_FASTNOISE_LOCATION}/FastNoise.cpp")
    target_include_directories(dommy_fastnoise_source_check SYSTEM PRIVATE
        "${DOMMY_FASTNOISE_LOCATION}")
    dommy_enable_warnings(dommy_fastnoise_source_check)
    set_property(GLOBAL APPEND PROPERTY DOMMY_FULL_SOURCE_CHECK_TARGETS
        dommy_fastnoise_source_check)
endif()

get_property(DOMMY_FULL_SOURCE_CHECK_TARGETS GLOBAL PROPERTY DOMMY_FULL_SOURCE_CHECK_TARGETS)
add_custom_target(dommy_full_source_check)
add_dependencies(dommy_full_source_check ${DOMMY_FULL_SOURCE_CHECK_TARGETS})

function(dommy_collect_link_sources output)
    get_property(_dommy_sources GLOBAL PROPERTY DOMMY_FULL_SOURCE_CHECK_SOURCES)
    list(REMOVE_DUPLICATES _dommy_sources)
    if(DOMMY_ENABLE_FASTNOISE_NODES)
        list(APPEND _dommy_sources "${DOMMY_FASTNOISE_LOCATION}/FastNoise.cpp")
    endif()
    set(${output} ${_dommy_sources} PARENT_SCOPE)
endfunction()

function(dommy_configure_maya_module target output_name)
    set_target_properties(${target} PROPERTIES
        PREFIX ""
        OUTPUT_NAME "${output_name}")
    if(APPLE)
        set_target_properties(${target} PROPERTIES SUFFIX ".bundle")
    elseif(WIN32)
        set_target_properties(${target} PROPERTIES SUFFIX ".mll")
    endif()

    target_include_directories(${target} PRIVATE ${DOMMY_PROJECT_INCLUDE_DIRS})
    target_include_directories(${target} SYSTEM PRIVATE
        ${DOMMY_EXTERNAL_INCLUDE_DIRS})
    target_link_libraries(${target} PRIVATE ${DOMMY_LEGACY_LINK_LIBRARIES})
    dommy_enable_warnings(${target})
    if(NOT MSVC)
        target_compile_options(${target} PRIVATE -Wno-deprecated-declarations)
        if(APPLE)
            target_link_options(${target} PRIVATE "-Wl,-undefined,error")
        elseif(UNIX)
            target_link_options(${target} PRIVATE "-Wl,--no-undefined")
        endif()
    endif()
endfunction()

if(DOMMY_BUILD_FULL_LINK_CHECK)
    if(NOT DOMMY_LEGACY_LINK_LIBRARIES)
        message(FATAL_ERROR
            "DOMMY_BUILD_FULL_LINK_CHECK requires DOMMY_LEGACY_LINK_LIBRARIES. "
            "Provide the Maya, OpenVDB, OpenSubdiv, Bullet (when enabled), "
            "Alembic, Arnold (when enabled), TBB, Boost, and platform libraries.")
    endif()

    dommy_collect_link_sources(_dommy_link_sources)

    add_library(dommy_full_link_check MODULE EXCLUDE_FROM_ALL ${_dommy_link_sources})
    dommy_configure_maya_module(dommy_full_link_check "dommyTool_link_check")

    add_custom_target(dommy_full_link_check_target)
    add_dependencies(dommy_full_link_check_target dommy_full_link_check)
endif()

if(DOMMY_BUILD_PLUGIN)
    if(NOT DOMMY_LEGACY_LINK_LIBRARIES)
        message(FATAL_ERROR
            "DOMMY_BUILD_PLUGIN requires DOMMY_LEGACY_LINK_LIBRARIES. "
            "Provide the Maya, OpenVDB, OpenSubdiv, Alembic, TBB, Boost, "
            "and enabled optional-integration libraries.")
    endif()

    dommy_collect_link_sources(_dommy_plugin_sources)
    add_library(dommyTool MODULE ${_dommy_plugin_sources})
    dommy_configure_maya_module(dommyTool "dommyTool")

    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/config/dommyTool.mod.in"
        "${CMAKE_CURRENT_BINARY_DIR}/dommyTool.mod"
        @ONLY)

    install(TARGETS dommyTool
        LIBRARY DESTINATION "dommyTool/plug-ins"
        RUNTIME DESTINATION "dommyTool/plug-ins")
    install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/python/"
        DESTINATION "dommyTool/python"
        FILES_MATCHING PATTERN "*.py")
    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/dommyTool.mod"
        DESTINATION ".")
    install(FILES
        "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE"
        "${CMAKE_CURRENT_SOURCE_DIR}/README.md"
        DESTINATION "dommyTool")
endif()
