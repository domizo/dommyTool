# Use an explicit compile boundary so unfinished and unregistered experiments
# cannot enter optional native targets through recursive source discovery.
# This module intentionally has no Maya SDK or IDE dependency.
get_filename_component(_dommy_manifest_root "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(DOMMY_ACTIVE_CPP_MANIFEST "${_dommy_manifest_root}/config/active_cpp_sources.txt")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${DOMMY_ACTIVE_CPP_MANIFEST}"
    "${_dommy_manifest_root}/tools/list_active_cpp_sources.py")
execute_process(
    COMMAND "${Python3_EXECUTABLE}"
            "${_dommy_manifest_root}/tools/list_active_cpp_sources.py"
            --manifest "${DOMMY_ACTIVE_CPP_MANIFEST}"
            --repository-root "${_dommy_manifest_root}"
    RESULT_VARIABLE _dommy_manifest_result
    OUTPUT_VARIABLE _dommy_manifest_output
    ERROR_VARIABLE _dommy_manifest_error
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
if(NOT _dommy_manifest_result EQUAL 0)
    message(FATAL_ERROR "Unable to read the active source manifest: ${_dommy_manifest_error}")
endif()
string(REPLACE "\n" ";" _dommy_manifest_paths "${_dommy_manifest_output}")
set(DOMMY_ACTIVE_CPP_SOURCES)
foreach(_dommy_path IN LISTS _dommy_manifest_paths)
    list(APPEND DOMMY_ACTIVE_CPP_SOURCES "${_dommy_manifest_root}/${_dommy_path}")
endforeach()
