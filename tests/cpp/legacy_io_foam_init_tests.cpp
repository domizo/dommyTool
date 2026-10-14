#include "maya_attribute_doubles.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DMayaEntryPointGuard.h"
#include <fstream>
#include <iostream>
#include <iterator>

std::ostream& operator<<(std::ostream& out, const MObject& value) { return out << value.name; }
struct MFnNurbsCurveData { static const MFnData::Type kMesh = MFnData::kMesh; };
struct MFnStringData {
    MObject create(const char* text, MStatus* status = nullptr) {
        const auto s = operation(std::string("string.default ") + text);
        if (status) *status = s;
        return {"emptyString"};
    }
};
struct MPointArray { MStatus clear() { return operation("points.clear"); } };
struct MFnPointArrayData {
    static const MFnData::Type kPointArray = MFnData::kPointArray;
    MObject create(const MPointArray&, MStatus* status = nullptr) {
        const auto s = operation("points.default");
        if (status) *status = s;
        return {"emptyPoints"};
    }
};
#define CHECK_MSTATUS(value) { (void)(value); }
#define CHECK_MSTATUS_AND_RETURN_IT(value) DOMMY_RETURN_IF_FAILURE(value)
#include "legacy_io_foam_init.inc"

int main(int argc, char** argv) {
    assert(argc == 2);
    const char* names[] = {"applyVtxColorFromFileNode", "ReadDPointDataNode", "foamGenerateNode", "DSkinInfo"};
    MStatus (*initializers[])() = {applyVtxColorFromFileNode::initialize,
        ReadDPointDataNode::initialize, foamGenerateNode::initialize, DSkinInfo::initialize};
    std::string schema;
    int counts[4] = {}, total = 0;
    for (int i = 0; i < 4; ++i) {
        schema += schemaTrace(names[i], initializers[i]);
        counts[i] = callCount; total += callCount;
    }
    if (std::string(argv[1]) == "--emit") { std::cout << schema; return 0; }
    std::ifstream file(argv[1]); assert(file.good());
    assert(schema == std::string((std::istreambuf_iterator<char>(file)), {}));
    for (int i = 0; i < 4; ++i) {
        testFailures(names[i], initializers[i], counts[i]);
        for (int at = 0; at < counts[i]; ++at) {
            resetCalls(); attributeThrowAt = at;
            assert(initializers[i]() == MS::kFailure && callCount == at + 1);
        }
    }
    std::cout << "Legacy IO/Foam initialization: " << total << " frozen operations, "
              << total*2 << " failures, " << total << " exceptions passed\n";
}
