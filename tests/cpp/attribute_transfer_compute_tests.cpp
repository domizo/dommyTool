#include "attribute_transfer_compute_doubles.h"
#include "attribute_transfer_compute.inc"

static void fixture() {
    resetCompute(); badRamp = badNeighbors = noNeighbors = false;
    pointInput = DPData(); pointOutput = DPData(); targetInput = DPData();
    groupInput = DMeshData(); groupOutput = DMeshData();
    curveInput = DCurveData(); curveOutput = DCurveData();
    pointInput.points.resize(4U); targetInput.points.resize(2U);
    targetInput.attributes["value"] = std::make_shared<TypedDPAttributeData<double>>(
        std::vector<double>{2.0, 4.0}, 7.0);
    targetInput.attributes["unsupported"] = std::make_shared<TypedDPAttributeData<std::string>>(
        std::vector<std::string>{"a", "b"}, "default");
    for (int i = 0; i < 2; ++i) {
        groupInput.meshes.push_back(std::make_shared<GroupMesh>());
        curveInput.meshes.push_back(std::make_shared<GroupMesh>());
    }
}
static MDataBlock inputs() {
    MDataBlock data;
    data.inputs["aDistance"].number = 2.0;
    data.inputs["aScale"].number = 1.0;
    data.inputs["aMaxSampleing"].number = -1;
    data.inputs["aAttributeNames"].string = " value missing value unsupported ";
    return data;
}
static MStatus compute(int node, MDataBlock& data, bool unknown = false) {
    const MObject other{"other"};
    if (node == 0) return DPAttributeTransfer().compute(MPlug(unknown ? other : DPAttributeTransfer::outDP), data);
    if (node == 1) return DCAttributeTransfer().compute(MPlug(unknown ? other : DCAttributeTransfer::outDC), data);
    return DMAttributeTransfer().compute(MPlug(unknown ? other : DMAttributeTransfer::outDM), data);
}
template <typename Owner> static double firstValue(const Owner& owner) {
    const auto field = std::dynamic_pointer_cast<const TypedDPAttributeData<double>>(owner.getAttr("value"));
    assert(field); return field->value().at(0);
}
static void semantics() {
    for (int node = 0; node < 3; ++node) {
        fixture(); auto data = inputs();
        assert(compute(node, data) && publications == 1);
        const int partitions = node == 0 ? 1 : 2;
        assert(std::count(operations.begin(), operations.end(), "attribute.add") == partitions);
        if (node == 0) assert(firstValue(pointOutput) == 3.0);
        else {
            const auto& output = node == 1 ? curveOutput : groupOutput;
            for (const auto& owner : output.meshes) assert(firstValue(*owner) == 3.0);
        }
        assert(firstValue(targetInput) == 2.0 && !pointInput.isExistAttr("value"));
        for (const auto& owner : groupInput.meshes) assert(!owner->isExistAttr("value"));
        for (const auto& owner : curveInput.meshes) assert(!owner->isExistAttr("value"));
        // Preserve scale/range weighting and zero values when no neighbor hits.
        fixture(); data = inputs(); data.inputs["aScale"].number = 0.5;
        assert(compute(node, data));
        assert(node == 0 ? firstValue(pointOutput) == 1.5 :
               firstValue(*(node == 1 ? curveOutput.meshes[0] : groupOutput.meshes[0])) == 1.5);
        fixture(); data = inputs(); noNeighbors = true;
        assert(compute(node, data));
        assert(node == 0 ? firstValue(pointOutput) == 0.0 :
               firstValue(*(node == 1 ? curveOutput.meshes[0] : groupOutput.meshes[0])) == 0.0);
        fixture(); data = inputs(); data.inputs["aAttributeNames"].string = "missing";
        assert(compute(node, data));
        assert(std::find(operations.begin(), operations.end(), "search.cloud") == operations.end());
        fixture(); data = inputs(); pointInput.points.clear(); groupInput.meshes.clear(); curveInput.meshes.clear();
        nullTarget = true;
        assert(compute(node, data));
        assert(std::find(operations.begin(), operations.end(), "input aTargetDP") == operations.end());
        resetCompute(); assert(compute(node, data, true) == MS::kUnknownParameter && callCount == 0);
    }
}
static void budgets() {
    using namespace dommy::transfer;
    AttributeTransferPlan plan; std::size_t work = 99U;
    assert(buildAttributeTransferPlan(1, 10, -1, 1, 0, 1, 1, plan));
    assert(buildAttributeApplicationBudget(plan, 2, 4, 3, work) && work == 86U);
    const auto prior = work;
    assert(!buildAttributeApplicationBudget(plan, 1, dommy::generation::kMaxWorkItemCount, 1, work) && work == prior);
    assert(!buildAttributeApplicationBudget(plan, 1000000U, 0, 1000000U, work));
    assert(buildAttributeTransferPlan(10000, 9999, -1, 1, 0, 1, 1, plan));
    assert(buildAttributeApplicationBudget(plan, 1, 1, 1, work) && work == 100000000U);
    assert(!buildAttributeApplicationBudget(plan, 2, 2, 1, work));
    assert(!buildAttributeApplicationBudget(plan, 1, 1, 2, work));
    // A cheap scalar count double proves all three nodes reject aggregate work
    // before copying output or allocating/searching huge native geometry.
    for (int node = 0; node < 3; ++node) {
        fixture(); auto data = inputs();
        targetInput.reportedCount = 10000U;
        targetInput.attributes.clear();
        targetInput.attributes["value"] = std::make_shared<TypedDPAttributeData<Imath::V3d>>(
            std::vector<Imath::V3d>(10000U, Imath::V3d(1, 2, 3)));
        pointInput.reportedCount = 9999U;
        groupInput.meshes[0]->pointCount = curveInput.meshes[0]->pointCount = 9996U;
        assert(!compute(node, data) && copies == 0 && publications == 0);
        assert(std::find(operations.begin(), operations.end(), "search.cloud") == operations.end());
    }
}
template <typename Value> static void transferType(const Value& input, const Value& expected) {
    fixture();
    targetInput.attributes["value"] = std::make_shared<TypedDPAttributeData<Value>>(
        std::vector<Value>(2U, input));
    pointInput.points.resize(1U);
    IDsListArray ids{{0U, 1U}};
    dommy::maya::AttributeTransferFactors factors{{1.0, 1.0}};
    assert(dommy::transfer::applyPointAttribute(targetInput, pointInput, "value", ids, factors) ==
           dommy::transfer::kAttributeApplySuccess);
    const auto field = std::dynamic_pointer_cast<const TypedDPAttributeData<Value>>(pointInput.getAttr("value"));
    assert(field && field->value().size() == 1U);
    using Traits = dommy::transfer::TransferValueTraits<Value>;
    for (std::size_t i = 0; i < Traits::kComponentCount; ++i)
        assert(Traits::component(field->value()[0], i) == Traits::component(expected, i));
}
static void types() {
    transferType<int32_t>(7, 7); transferType<uint32_t>(4000000000U, 4000000000U);
    transferType<int64_t>(9007199254740992LL, 9007199254740992LL);
    transferType<float>(1.25f, 1.25f); transferType<double>(1.25, 1.25);
    transferType<Imath::V2i>({1, 2}, {1, 2}); transferType<Imath::V2f>({1, 2}, {1, 2});
    transferType<Imath::V2d>({1, 2}, {1, 2}); transferType<Imath::V3i>({1, 2, 3}, {1, 2, 3});
    transferType<Imath::V3f>({1, 2, 3}, {1, 2, 3}); transferType<Imath::V3d>({1, 2, 3}, {1, 2, 3});
}
static void failures() {
    unsigned int tested = 0;
    for (int node = 0; node < 3; ++node) {
        fixture(); auto data = inputs(); assert(compute(node, data)); const auto trace = operations;
        for (std::size_t i = 0; i < trace.size(); ++i) {
            for (int kind = 0; kind < 3; ++kind) {
                fixture(); data = inputs();
                if (kind == 2) throwAt = int(i);
                else {failAt = int(i); injectedStatus = kind ? MS::kInvalidParameter : MS::kFailure;}
                assert(!compute(node, data));
                assert(callCount == int(i) + 1 && !invalidRead);
                if (trace[i] != "output.clean") assert(publications == 0);
                assert(firstValue(targetInput) == 2.0 && !pointInput.isExistAttr("value"));
                for (const auto& owner : groupInput.meshes) assert(owner->attributes.empty());
                for (const auto& owner : curveInput.meshes) assert(owner->attributes.empty());
                ++tested;
                fixture(); data = inputs(); assert(compute(node, data));
            }
        }
        for (int variant = 0; variant < 9; ++variant) {
            fixture(); data = inputs();
            if (variant == 0) nullInput = true;
            if (variant == 1) nullTarget = true;
            if (variant == 2) targetInput.valid = false;
            if (variant == 3) nullOutput = true;
            if (variant == 4) badCopy = true;
            if (variant == 5) badRamp = true;
            if (variant == 6) badNeighbors = true;
            if (variant == 7) data.inputs["aDistance"].number = NAN;
            if (variant == 8) data.inputs["aScale"].number = INFINITY;
            assert(!compute(node, data) && publications == 0);
        }
    }
    std::cout << "Attribute transfer: all 11 supported types, deduplication, aggregate budgets; "
              << tested << " status/exception injections passed\n";
}
int main() {semantics(); budgets(); types(); failures();}
