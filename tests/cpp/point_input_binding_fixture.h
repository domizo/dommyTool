// Included once after each fixture declares DPData, MDataHandle and its payload.
// The payload/validator is an explicit boundary, not a Maya storage emulator.
#ifndef DOMMY_POINT_INPUT_DATA
#error Define the fixture payload before including this header
#endif
#ifndef DOMMY_POINT_INPUT_PRESENT
#error Define the fixture presence flag before including this header
#endif
namespace dommy { namespace maya {
template <class T>
MStatus getPluginDataFromHandle(const MDataHandle& handle, int expectedType,
                               const T*& output)
{
    assert(handle.valid);
    output = nullptr;
    const MStatus status = operation("point.bind");
    if (!status) return status;
    if (expectedType != DPData::id || !DOMMY_POINT_INPUT_PRESENT ||
        pointInputFixture.wrongMayaType || pointInputFixture.wrongCppType ||
        pointInputFixture.nullRawData) return MS::kFailure;
    output = &DOMMY_POINT_INPUT_DATA;
    return MS::kSuccess;
}
}}
#undef DOMMY_POINT_INPUT_DATA
#undef DOMMY_POINT_INPUT_PRESENT
