#include "primitive_metadata_compute_doubles.h"
#include "primitive_metadata_compute.inc"

static void fixture(std::size_t count = 3U) {
    resetCompute();
    groupInput = DMeshData(); groupOutput = DMeshData();
    curveInput = DCurveData(); curveOutput = DCurveData();
    targetInput = DPData();
    nearestSelections.clear(); distances.assign(count, 0.0);
    for (std::size_t i = 0; i < count; ++i) {
        auto owner = std::make_shared<GroupMesh>();
        owner->center = {{double(i), 0, 0}};
        owner->points["remove"] = std::make_shared<GroupMask>(DPIdList(2U, true));
        owner->points["keep"] = std::make_shared<GroupMask>(DPIdList(2U, false));
        groupInput.meshes.push_back(owner);
        curveInput.meshes.push_back(std::make_shared<GroupMesh>(*owner));
        targetInput.points.push_back(owner->center);
        nearestSelections.push_back(i);
    }
    if (count) {
        curveInput.groups["piece0"] = std::make_shared<GroupMask>(DPIdList(count, true));
        curveInput.groups["keep"] = std::make_shared<GroupMask>(DPIdList(count, true));
    }
}
static MDataBlock inputs() {
    MDataBlock data;
    data.inputs["aGroupName"].string = "piece";
    data.inputs["aDistance"].number = 1.0;
    data.inputs["aIsEnable"].number = 1.0;
    data.inputs["aPrimitiveGroupName"].string = "piece0 missing piece0";
    data.inputs["aPointGroupName"].string = " remove missing remove ";
    data.inputs["aAttributeNames"].string = " value missing value ";
    return data;
}
static MStatus compute(int node, MDataBlock& data, bool unknown = false) {
    const MObject other{"other"};
    switch (node) {
    case 0: return DCGroupFromDP().compute(MPlug(unknown ? other : DCGroupFromDP::outDC), data);
    case 1: return DCGroupDelete().compute(MPlug(unknown ? other : DCGroupDelete::outDC), data);
    case 2: return DMAttributeCopy().compute(MPlug(unknown ? other : DMAttributeCopy::outDM), data);
    default: return DCAttributeCopy().compute(MPlug(unknown ? other : DCAttributeCopy::outDC), data);
    }
}
template <typename T> static void source(const T& value, const T& defaultValue, std::size_t count = 6U) {
    targetInput.points.resize(count);
    targetInput.attributes["value"] = std::make_shared<TypedDPAttributeData<T>>(
        std::vector<T>(count, value), defaultValue);
}
static const DMeshData& output(int node) { return node == 2 ? groupOutput : curveOutput; }
template <typename T> static bool sameValue(const T& a, const T& b) { return a == b; }
template <typename T> static bool sameValue(const Imath::Vec2<T>& a, const Imath::Vec2<T>& b) {
    return a[0] == b[0] && a[1] == b[1];
}
template <typename T> static bool sameValue(const Imath::Vec3<T>& a, const Imath::Vec3<T>& b) {
    return a[0] == b[0] && a[1] == b[1] && a[2] == b[2];
}
static bool sameValue(const MMatrix& a, const MMatrix& b) {
    for (unsigned int row = 0; row < 4; ++row)
        for (unsigned int column = 0; column < 4; ++column)
            if (a[row][column] != b[row][column]) return false;
    return true;
}
template <typename T> static bool allValues(const std::vector<T>& values, std::size_t count, const T& value) {
    if (values.size() != count) return false;
    for (const T item : values) if (!sameValue(item, value)) return false;
    return true;
}
static void groupSemantics() {
    fixture();
    nearestSelections = {2U, 0U, 2U};
    auto data = inputs();
    assert(compute(0, data) && additions == 2);
    assert(curveOutput.groups.at("piece0")->ids == DPIdList({false, true, false}));
    assert(curveOutput.groups.at("piece1")->ids == DPIdList({true, false, true}));
    assert(curveInput.groups.at("piece0")->ids == DPIdList(3U, true));
    assert(curveOutput.groups.count("keep"));
    fixture(); data = inputs(); distances = {0, 1, 2};
    data.inputs["aUseDistance"].number = 1;
    assert(compute(0, data) && additions == 2 && !curveOutput.groups.count("piece2"));
    fixture(); data = inputs(); data.inputs["aGroupName"].string = u8"部品";
    assert(compute(0, data) && curveOutput.groups.count(u8"部品2"));
    fixture(); data = inputs();
    assert(compute(1, data) && !curveOutput.groups.count("piece0"));
    assert(curveInput.groups.count("piece0") && curveOutput.groups.count("keep"));
    for (const auto& owner : curveOutput.meshes)
        assert(owner->points.size() == 1U && owner->points.count("keep"));
    for (const auto& owner : curveInput.meshes) assert(owner->points.count("remove"));
    fixture(); data = inputs(); data.inputs["aIsEnable"].number = 0;
    assert(compute(1, data) && curveOutput.groups.count("piece0"));
    assert(std::find(operations.begin(), operations.end(), "string.split") == operations.end());
    fixture(10001U); data = inputs();
    assert(!compute(0, data) && copies == 0 && additions == 0 && publications == 0);
    // Generated masks alone fit; retained metadata makes the final plan exceed
    // the budget. Rejection must still precede the output copy.
    fixture(10000U); data = inputs();
    assert(!compute(0, data) && copies == 0 && additions == 0 && publications == 0);
    fixture(10001U); data = inputs();
    std::fill(nearestSelections.begin(), nearestSelections.end(), 0U);
    assert(compute(0, data) && additions == 1);
    fixture(501U); data = inputs();
    data.inputs["aGroupName"].string = MString(std::string(250000U, 'x').c_str());
    rejectNameBuild = true;
    assert(!compute(0, data) && copies == 0 && nameBuilds == 0);
    fixture(1U); data = inputs(); curveInput.groups.clear(); curveInput.reportedCount = 1000000U;
    std::string names;
    for (int i = 0; i < 101; ++i) names += "remove ";
    data.inputs["aPointGroupName"].string = MString(names.c_str());
    assert(!compute(1, data) && copies == 0);
}
template <typename T> static void checkType(const T& value, const T& defaultValue) {
    for (int node : {2, 3}) {
        fixture(); source(value, defaultValue);
        // Replacing an existing field of another type is historical behavior.
        auto& input = node == 2 ? groupInput : static_cast<DMeshData&>(curveInput);
        input.meshes[0]->attributes["value"] = std::make_shared<TypedDPAttributeData<std::string>>(
            std::vector<std::string>(2U, "old"), "old-default");
        const auto old = input.meshes[0]->getAttr("value");
        auto data = inputs();
        assert(compute(node, data) && publications == 1);
        assert(input.meshes[0]->getAttr("value") == old);
        assert(std::count(operations.begin(), operations.end(), "attribute.add") == 2);
        for (const auto& owner : output(node).meshes) {
            const auto field = std::dynamic_pointer_cast<TypedDPAttributeData<T>>(owner->getAttr("value"));
            assert(field && allValues(field->value(), 2U, value));
            assert(sameValue(field->getDefaultValue(), defaultValue));
            field->incrementDefaultValue(1U);
            const T appended = field->value()[2];
            assert(sameValue(appended, defaultValue));
        }
        const auto original = std::dynamic_pointer_cast<const TypedDPAttributeData<T>>(targetInput.getAttr("value"));
        assert(allValues(original->value(), 6U, value) && sameValue(original->getDefaultValue(), defaultValue));
    }
}
static void attributeSemantics() {
    checkType<bool>(false, true);
    checkType<int32_t>(-2, 7); checkType<uint32_t>(4000000000U, 7U);
    checkType<int64_t>(9007199254740993LL, -9007199254740993LL);
    checkType<float>(1.25f, 7.5f); checkType<double>(1.234567890123456, -7.25);
    checkType<Imath::V2i>(Imath::V2i(1, 2), Imath::V2i(7, 8));
    checkType<Imath::V2f>(Imath::V2f(1, 2), Imath::V2f(7, 8));
    checkType<Imath::V2d>(Imath::V2d(1, 2), Imath::V2d(7, 8));
    checkType<Imath::V3i>(Imath::V3i(1, 2, 3), Imath::V3i(7, 8, 9));
    checkType<Imath::V3f>(Imath::V3f(1, 2, 3), Imath::V3f(7, 8, 9));
    checkType<Imath::V3d>(Imath::V3d(1, 2, 3), Imath::V3d(7, 8, 9));
    checkType<std::string>(u8"属性", "default");
    MMatrix value, defaultValue; value[0][0] = 2.0; defaultValue[3][0] = 7.0;
    checkType<MMatrix>(value, defaultValue);
    checkType<DIntArray>({1, 2}, {7, 8, 9});
    for (int node : {2, 3}) {
        fixture(); source<int32_t>(3, 7, 5); auto data = inputs();
        assert(compute(node, data) && !output(node).meshes[0]->getAttr("value"));
        assert(std::find(operations.begin(), operations.end(), "string.split") == operations.end());
        fixture(); source<int32_t>(3, 7); data = inputs(); data.inputs["aAttributeNames"].string = "";
        assert(compute(node, data) && !output(node).meshes[0]->getAttr("value"));
        // Nonuniform and empty partitions preserve order and the source default.
        fixture(); source<int32_t>(3, 7, 3); data = inputs();
        auto values = std::dynamic_pointer_cast<TypedDPAttributeData<int32_t>>(targetInput.attributes["value"]);
        assert(values->replaceValues({10, 20, 30}, 3U));
        auto& owner = node == 2 ? groupInput : static_cast<DMeshData&>(curveInput);
        owner.meshes[0]->pointCount = 0; owner.meshes[1]->pointCount = 1; owner.meshes[2]->pointCount = 2;
        assert(compute(node, data));
        const auto last = std::dynamic_pointer_cast<TypedDPAttributeData<int32_t>>(output(node).meshes[2]->getAttr("value"));
        assert(last->value() == std::vector<int32_t>({20, 30}) && last->getDefaultValue() == 7);
        const auto empty = std::dynamic_pointer_cast<TypedDPAttributeData<int32_t>>(output(node).meshes[0]->getAttr("value"));
        assert(empty->value().empty() && empty->getDefaultValue() == 7);
    }
}
static void partitionFailures() {
    using namespace dommy::attribute;
    for (int variant = 0; variant < 7; ++variant) {
        fixture(); source<int32_t>(3, 7);
        ValidationBudget budget;
        auto field = targetInput.getAttr("value");
        std::string name = "value";
        if (variant == 0) groupInput.meshes[0].reset();
        if (variant == 1) groupInput.meshes[0]->pointCount = 1;
        if (variant == 2) groupInput.meshes[0]->pointCount = 7;
        if (variant == 3) budget.workItems = dommy::generation::kMaxWorkItemCount;
        if (variant == 4) name = "";
        if (variant == 5) field.reset();
        if (variant == 6) groupInput.meshes[2]->rejectAttribute = true;
        assert(!copyAttributePartitions(field, name, groupInput.beginPrims(), groupInput.endPrims(), 6, budget));
        if (variant != 6) assert(std::find(operations.begin(), operations.end(), "attribute.add") == operations.end());
    }
    // Repeating a dynamic default is budgeted before committing any partition.
    fixture(); source<std::string>("", "default");
    ValidationBudget budget;
    budget.stringBytes = dommy::generation::kMaxGeneratedStringByteCount - 30U;
    assert(!copyAttributePartitions(targetInput.getAttr("value"), "value",
        groupInput.beginPrims(), groupInput.endPrims(), 6, budget));
    assert(std::find(operations.begin(), operations.end(), "attribute.add") == operations.end());
    assert(budget.stringBytes == dommy::generation::kMaxGeneratedStringByteCount - 30U);
    fixture(); source<double>(3, 7);
    assert(!copyTypedAttributePartitions<int32_t>(targetInput.getAttr("value"), "value",
        groupInput.beginPrims(), groupInput.endPrims(), 6, budget));
}
static void failureInjection() {
    unsigned int tested = 0;
    for (int node = 0; node < 4; ++node) {
        fixture(); if (node >= 2) source<int32_t>(3, 7);
        auto data = inputs(); assert(compute(node, data)); const auto trace = operations;
        for (std::size_t i = 0; i < trace.size(); ++i) {
            for (int kind = 0; kind < 3; ++kind) {
                fixture(); if (node >= 2) source<int32_t>(3, 7); data = inputs();
                if (kind == 2) throwAt = int(i);
                else { failAt = int(i); injectedStatus = kind ? MS::kInvalidParameter : MS::kFailure; }
                assert(!compute(node, data));
                assert(callCount == int(i) + 1 && !invalidRead);
                if (trace[i] != "output.clean") assert(publications == 0);
                assert(curveInput.groups.count("piece0"));
                for (const auto& owner : curveInput.meshes) assert(owner->points.count("remove") && owner->attributes.empty());
                for (const auto& owner : groupInput.meshes) assert(owner->attributes.empty());
                ++tested;
                fixture(); if (node >= 2) source<int32_t>(3, 7); data = inputs();
                assert(compute(node, data));
            }
        }
        for (int variant = 0; variant < 7; ++variant) {
            if (node == 1 && variant >= 5) continue;
            fixture(); if (node >= 2) source<int32_t>(3, 7); data = inputs();
            if (variant == 0) nullInput = true;
            if (variant == 1) { groupInput.valid = false; curveInput.valid = false; }
            if (variant == 2) nullOutput = true;
            if (variant == 3) badCopy = true;
            if (variant == 4) badFinal = true;
            if (variant == 5) nullTarget = true;
            if (variant == 6) targetInput.valid = false;
            assert(!compute(node, data) && publications == 0);
        }
        fixture(0U); data = inputs(); assert(compute(node, data));
        resetCompute(); assert(compute(node, data, true) == MS::kUnknownParameter && callCount == 0);
    }
    std::cout << "Primitive metadata: all 15 types/defaults, curve groups, bounded staging; "
              << tested << " status/exception injections passed\n";
}
int main() {
    groupSemantics(); attributeSemantics(); partitionFailures(); failureInjection();
}
