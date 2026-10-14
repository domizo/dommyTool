#include "point_uv_mesh_init_doubles.h"
#include "point_uv_mesh_init.inc"

int main(int argc, char** argv) {
    assert(argc == 2);
    const std::string trace = schemaTrace("DPUVGen", DPUVGen::initialize);
    if (std::string(argv[1]) == "--emit") {
        std::cout << trace;
        return 0;
    }
    std::ifstream file(argv[1]);
    assert(file);
    assert(trace == std::string(std::istreambuf_iterator<char>(file), {}));
    testFailures("DPUVGen", DPUVGen::initialize, 29);
    for (int at = 0; at < 29; ++at) {
        resetCalls(); attributeThrowAt = at;
        assert(!DPUVGen::initialize() && callCount == at + 1);
        assert(schemaTrace("DPUVGen", DPUVGen::initialize) == trace);
    }
    std::cout << "DPUVGen: 29 frozen operations, 58 status failures and 29 exceptions with recovery passed\n";
}
