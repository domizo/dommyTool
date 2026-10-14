#pragma once

#include <exception>

#include <maya/MGlobal.h>
#include <maya/MStatus.h>
#include <maya/MString.h>

#include <openvdb/Exceptions.h>

namespace dommy {
namespace vdb {

inline MStatus reportComputeException(
    const char* nodeName, const std::exception_ptr& exception) noexcept {
    try {
        if (exception) {
            std::rethrow_exception(exception);
        }
        MGlobal::displayError(MString(nodeName) + ": unknown compute error.");
    } catch (const openvdb::Exception& error) {
        try {
            MGlobal::displayError(MString(nodeName) + ": OpenVDB error: " + error.what());
        } catch (...) {
        }
    } catch (const std::exception& error) {
        try {
            MGlobal::displayError(MString(nodeName) + ": compute error: " + error.what());
        } catch (...) {
        }
    } catch (...) {
        try {
            MGlobal::displayError(MString(nodeName) + ": unknown compute error.");
        } catch (...) {
        }
    }
    return MS::kFailure;
}

} // namespace vdb
} // namespace dommy

#define DOMMY_VDB_COMPUTE_CATCH(nodeName) \
    catch (...) { \
        return ::dommy::vdb::reportComputeException(nodeName, std::current_exception()); \
    }
