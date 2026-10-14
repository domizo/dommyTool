#pragma once

// Test-only state for modeled payload-binding/connection boundaries.
// Real input readers call a fallible DataBlock double; optional-input
// classification runs production code, not native Maya evaluation.
struct PointInputFixture {
    bool connected = false;
    bool wrongMayaType = false;
    bool wrongCppType = false;
    bool nullRawData = false;
    int dataReads = 0;
};
static PointInputFixture pointInputFixture;
inline void resetPointInputFixture() { pointInputFixture = PointInputFixture(); }
inline bool pointInputConnection(MStatus* status) {
    const MStatus query = operation("point.connection");
    if (status) *status = query;
    // A failed native query can still return a valid-looking boolean.
    return pointInputFixture.connected;
}
