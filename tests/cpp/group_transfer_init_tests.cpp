#include "group_transfer_init_doubles.h"
#include "group_transfer_init.inc"

template <typename Node> static int checkedReads() {
    resetCalls(); invalidReads = 0;
    MDataBlock data; DGroupParams params;
    assert(Node().evalParms(data, params));
    const int count = callCount;
    for (int i = 0; i < count; ++i) {
        for (const auto status : {MS::kFailure, MS::kInvalidParameter}) {
            resetCalls(i); invalidReads = 0; injectedStatus = status;
            DGroupParams candidate;
            assert(Node().evalParms(data, candidate) == status);
            assert(invalidReads == 0 && callCount == i + 1);
        }
    }
    resetCalls(); invalidReads = 0;
    assert(Node().evalParms(data, params) && !invalidReads);
    return count * 2;
}
static MStatus objectParameters(MDataBlock& data,DGroupParams& result) try {
    return DPGroup().evalParms(data,result);
} DOMMY_COMPUTE_CATCH("group object input")
static void objectBounds() {
    const auto setup=[]() {
        resetCalls();invalidReads=0;objectBoundsEnabled=true;
        groupBoundType=DGRP_BOUND_OBJECT;groupObjectConnected=true;
        groupBoundActionAttribute=DPGroup::aIsBoundingAction.name;
        groupBoundTypeAttribute=DPGroup::aBoundingType.name;
    };
    MDataBlock data;DGroupParams result;
    setup();assert(objectParameters(data,result)==MS::kSuccess && result.m_objHnd.valid);
    const int count=callCount;
    for(int index=0;index<count;++index) {
        for(auto code:{MS::kFailure,MS::kInvalidParameter}) {
            setup();failAt=index;injectedStatus=code;DGroupParams candidate;
            assert(objectParameters(data,candidate)==code && callCount==index+1 && invalidReads==0);
        }
        setup();attributeThrowAt=index;
        assert(objectParameters(data,result)==MS::kFailure && callCount==index+1);
        setup();assert(objectParameters(data,result)==MS::kSuccess);
    }
    setup();groupObjectConnected=false;result=DGroupParams();
    assert(objectParameters(data,result)==MS::kInvalidParameter);
    for(bool enabled:{false,true}) {
        setup();objectBoundsEnabled=enabled;groupBoundType=DGRP_BOUND_BOX;
        assert(objectParameters(data,result)==MS::kSuccess);
        for(const auto& operation:operations)assert(operation!="connected "+DPGroup::aInObject.name && operation!="input "+DPGroup::aInObject.name);
    }
    objectBoundsEnabled=false;groupBoundType=0;
}
int main(int argc, char** argv) {
    assert(argc == 2);
    const std::string trace = schemaTrace("DPGroup", DPGroup::initialize) +
        schemaTrace("DCGroup", DCGroup::initialize) + schemaTrace("DMGroup", DMGroup::initialize) +
        schemaTrace("DPAttributeTransfer", DPAttributeTransfer::initialize) +
        schemaTrace("DCAttributeTransfer", DCAttributeTransfer::initialize) +
        schemaTrace("DMAttributeTransfer", DMAttributeTransfer::initialize);
    if (std::string(argv[1]) == "--emit") {std::cout << trace; return 0;}
    std::ifstream file(argv[1]); assert(file.good());
    const std::string expected((std::istreambuf_iterator<char>(file)), {});
    assert(trace == expected);
    int failures = 0, exceptions = 0;
    for (auto initialize : {DPGroup::initialize, DCGroup::initialize, DMGroup::initialize,
                           DPAttributeTransfer::initialize, DCAttributeTransfer::initialize,
                           DMAttributeTransfer::initialize}) {
        resetCalls(); assert(initialize()); const int count = callCount;
        for (int i = 0; i < count; ++i) {
            for (const auto code : {MS::kFailure, MS::kInvalidParameter}) {
                resetCalls(i); injectedStatus = code;
                assert(initialize() == code && callCount == i + 1); ++failures;
            }
            resetCalls(); attributeThrowAt = i;
            assert(initialize() == MS::kFailure && callCount == i + 1); ++exceptions;
            resetCalls(); assert(initialize());
        }
    }
    const int reads = checkedReads<DPGroup>() + checkedReads<DCGroup>() + checkedReads<DMGroup>();
    objectBounds();
    std::cout << "Group/transfer: frozen schema; " << failures << " initialization status failures, "
              << exceptions << " exceptions and " << reads << " failed reads passed\n";
}
