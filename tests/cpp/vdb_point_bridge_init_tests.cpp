#include "group_transfer_init_doubles.h"
struct DOpenVDBData {
    static constexpr MFnData::Type id = MFnData::kAny;
};
#include "vdb_point_bridge_init.inc"

int main(int argc, char** argv) {
    assert(argc == 2);
    const std::string trace = schemaTrace("DPAttributeVDB", DPAttributeVDB::initialize) +
                              schemaTrace("DPClosestPointVDB", DPClosestPointVDB::initialize);
    if (std::string(argv[1]) == "--emit") {
        std::cout << trace;
        return 0;
    }
    std::ifstream file(argv[1]);
    assert(file.good());
    assert(trace == std::string((std::istreambuf_iterator<char>(file)), {}));
    int failures = testFailures("DPAttributeVDB", DPAttributeVDB::initialize, 18) +
                   testFailures("DPClosestPointVDB", DPClosestPointVDB::initialize, 38);
    int exceptions = 0;
    for (auto initialize : {DPAttributeVDB::initialize, DPClosestPointVDB::initialize}) {
        resetCalls();
        assert(initialize());
        const int count = callCount;
        for (int i = 0; i < count; ++i) {
            resetCalls();
            attributeThrowAt = i;
            assert(initialize() == MS::kFailure && callCount == i + 1);
            ++exceptions;
            resetCalls();
            assert(initialize());
        }
    }
    std::cout << "VDB point bridges: frozen 56-operation schema, " << failures
              << " failed statuses and " << exceptions << " exceptions passed\n";
}
