#include "maya_attribute_doubles.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DStatusOperation.h"
#include <fstream>
#include <iostream>

#define CHECK_MSTATUS(x)                                                                           \
    do {                                                                                           \
        const MStatus s = (x);                                                                     \
        if (!s)                                                                                    \
            s.pAPIerror("test", __LINE__);                                                         \
    } while (false)
struct MFnStringData {
    MObject create(const char* value, MStatus* status = nullptr) {
        const MStatus result = operation("string.create");
        if (status)
            *status = result;
        return result ? MObject{value} : MObject::kNullObj;
    }
};
struct NumericPoint : MFnNumericAttribute {
    MObject createPoint(const char* name, const char* shortName, MStatus* status = nullptr) {
        return bind(name, shortName, "point", status);
    }
};
#define MFnNumericAttribute NumericPoint
struct DOpenVDBData {
    static constexpr MFnData::Type id = MFnData::kAny;
};
#include "vdb_edit_init.inc"

int main(int argc, char** argv) {
    assert(argc == 2);
    const char* names[] = {"DOpenVDBTransform", "DOpenVDBVectorSplit", "DOpenVDBCopy"};
    MStatus (*initializers[])() = {DOpenVDBTransform::initialize, DOpenVDBVectorSplit::initialize,
                                   DOpenVDBCopy::initialize};
    const int counts[] = {39, 21, 22};
    std::string trace;
    for (int i = 0; i < 3; ++i)
        trace += schemaTrace(names[i], initializers[i]);
    if (std::string(argv[1]) == "--emit") {
        std::cout << trace;
        return 0;
    }
    std::ifstream file(argv[1]);
    assert(file.good());
    assert(trace == std::string((std::istreambuf_iterator<char>(file)), {}));
    for (int i = 0; i < 3; ++i) {
        const auto init = initializers[i];
        const std::string baseline = schemaTrace(names[i], init);
        testFailures(names[i], init, counts[i]);
        for (int failure = 0; failure < counts[i]; ++failure) {
            resetCalls();
            attributeThrowAt = failure;
            assert(init() == MS::kFailure && callCount == failure + 1);
            assert(schemaTrace(names[i], init) == baseline);
        }
    }
    std::cout
        << "VDB edit: frozen 82-operation schema, 164 failed statuses and 82 exceptions passed\n";
}
