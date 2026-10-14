#pragma once
#include <maya/MStatus.h>
#include "common/file/DAtomicFile.h"
#include "common/file/DSequencePath.h"
#include <cassert>
#include <cmath>
#include <fstream>
#include <iterator>
#include <limits>
#include <locale>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
#ifdef DOMMY_SDK_CHECK
#include <maya/MGlobal.h>
#include <maya/MBoundingBox.h>
#include <maya/MPoint.h>
#else
struct MPoint {
    double x, y, z;
    MPoint(double a = 0.0, double b = 0.0, double c = 0.0) : x(a), y(b), z(c) {}
};
struct MBoundingBox {
    MPoint lower, upper;
    MBoundingBox(const MPoint& a, const MPoint& b) : lower(a), upper(b) {}
    MPoint min() const { return lower; }
    MPoint max() const { return upper; }
};
std::vector<std::string> executed;
unsigned int warnings = 0U;
bool nativeFailure = false, nativeException = false;
bool lastMel = false;
struct MGlobal {
    static void displayWarning(const char*) { ++warnings; }
    static MStatus executeCommand(const char* command, bool) {
        lastMel = true;
        if (nativeException) throw std::runtime_error("callback transport failed");
        executed.push_back(command);
        return nativeFailure ? MS::kFailure : MS::kSuccess;
    }
    static MStatus executePythonCommand(const char* command, bool) {
        const auto status = executeCommand(command, true); lastMel = false; return status;
    }
};
#endif
