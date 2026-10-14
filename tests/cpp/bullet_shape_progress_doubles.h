#pragma once
#include <cstddef>

// No Maya UI calls are made by portable or native-Bullet shape tests.
namespace MS { enum Code { kSuccess, kFailure }; }
class MString { public: explicit MString(const char*) {} };
class MGlobal { public: static void displayInfo(const char*) {} };
class MProgressWindow {
public:
    static int& activeCount() { static int count = 0; return count; }
    static bool reserve() { ++activeCount(); return true; }
    static void setProgressRange(int, int) {}
    static void setTitle(const char*) {}
    static void setInterruptable(bool) {}
    static int setProgress(int) { return 0; }
    static MS::Code startProgress() { return MS::kSuccess; }
    static void endProgress() { --activeCount(); }
    static int setProgressStatus(const MString&) { return 0; }
    static bool isCancelled() { return false; }
};
#define CHECK_MSTATUS(value) ((void)(value))
