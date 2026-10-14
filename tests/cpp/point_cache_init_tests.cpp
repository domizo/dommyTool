#include "group_transfer_init_doubles.h"
static bool registrationThrows = false;
void registerDPointCacheAttributeTypes() {
    if (registrationThrows)
        throw std::runtime_error("registry failure");
}
#include "point_cache_init.inc"

int main(int argc, char** argv) {
    assert(argc == 2);
    const std::string trace = schemaTrace("DPCache", DPCache::initialize);
    std::ifstream input(argv[1]);
    assert(input.good());
    assert(trace == std::string((std::istreambuf_iterator<char>(input)), {}));
    assert(testFailures("DPCache", DPCache::initialize, 33) == 66);
    for (int i = 0; i < 33; ++i) {
        resetCalls();
        attributeThrowAt = i;
        assert(DPCache::initialize() == MS::kFailure && callCount == i + 1);
        assert(schemaTrace("DPCache", DPCache::initialize) == trace);
    }
    resetCalls();
    registrationThrows = true;
    assert(DPCache::initialize() == MS::kFailure && callCount == 0);
    registrationThrows = false;
    assert(schemaTrace("DPCache", DPCache::initialize) == trace);
    std::cout
        << "DPCache: frozen 33-operation schema, 66 failed statuses and 34 exceptions passed\n";
}
