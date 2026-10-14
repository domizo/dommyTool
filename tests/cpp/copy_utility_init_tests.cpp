#include "group_transfer_init_doubles.h"
// The shared registration double uses kAny as its plugin id token.
struct CopyGenericAttribute : MFnGenericAttribute {
    MStatus addAccept(MFnData::Type type) { return addDataAccept(type); }
};
#define MFnGenericAttribute CopyGenericAttribute
#include "copy_utility_init.inc"

int main(int argc, char** argv) {
    assert(argc == 2);
    const std::string trace = schemaTrace("DPDuplicator", DPDuplicator::initialize) +
        schemaTrace("DPCopyTransform", DPCopyTransform::initialize) +
        schemaTrace("DPBlend", DPBlend::initialize);
    if (std::string(argv[1]) == "--emit") { std::cout << trace; return 0; }
    std::ifstream file(argv[1]); assert(file.good());
    const std::string expected((std::istreambuf_iterator<char>(file)), {});
    assert(trace == expected);
    int failures = 0, exceptions = 0;
    for (auto initialize : {DPDuplicator::initialize, DPCopyTransform::initialize, DPBlend::initialize}) {
        resetCalls(); assert(initialize()); const int count = callCount;
        for (int i = 0; i < count; ++i) {
            for (auto code : {MS::kFailure, MS::kInvalidParameter}) {
                resetCalls(i); injectedStatus = code;
                assert(initialize() == code && callCount == i + 1); ++failures;
            }
            resetCalls(); attributeThrowAt = i;
            assert(initialize() == MS::kFailure && callCount == i + 1); ++exceptions;
        }
    }
    std::cout << "Copy utilities: frozen schema, " << failures << " status failures, "
              << exceptions << " exceptions passed\n";
}
