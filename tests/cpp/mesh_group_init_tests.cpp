#include "mesh_emitter_doubles.h"
#include "mesh_group_init.inc"

int main(int argc, char** argv) {
    assert(argc == 2);
    const std::string trace = schemaTrace("DMToMesh", DMToMesh::initialize) +
                              schemaTrace("DMGroupToIndex", DMGroupToIndex::initialize) +
                              schemaTrace("DMGroupFromDP", DMGroupFromDP::initialize) +
                              schemaTrace("DMGroupDelete", DMGroupDelete::initialize);
    std::ifstream file(argv[1]);
    assert(file.good());
    const std::string expected((std::istreambuf_iterator<char>(file)), {});
    assert(trace == expected);
    const int failures = testFailures("DMToMesh", DMToMesh::initialize, 24) +
                         testFailures("DMGroupToIndex", DMGroupToIndex::initialize, 14) +
                         testFailures("DMGroupFromDP", DMGroupFromDP::initialize, 31) +
                         testFailures("DMGroupDelete", DMGroupDelete::initialize, 34);
    int exceptions = 0;
    for (auto initialize : {DMToMesh::initialize, DMGroupToIndex::initialize,
                            DMGroupFromDP::initialize, DMGroupDelete::initialize}) {
        resetCalls();
        assert(initialize());
        const int count = callCount;
        for (int i = 0; i < count; ++i) {
            resetCalls();
            attributeThrowAt = i;
            assert(initialize() == MS::kFailure && callCount == i + 1);
            resetCalls();
            assert(initialize());
            ++exceptions;
        }
    }
    std::cout << "Unchanged 103-operation group schemas: " << failures << " status failures and "
              << exceptions << " exceptions passed\n";
}
