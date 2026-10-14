// Full production compute/read helpers. Maya ownership/publication and noise
// sampling are doubles, not evidence of native memory or noise performance.
#include "point_force_compute_doubles.h"
#include "common/utility/DAttributePublication.h"
#include <perlin.h>
#include "point_coordinate_output.inc"

static int retainedFieldCopies = 0;
template <typename T> struct ObservedField : TypedDPAttributeData<T> {
    using TypedDPAttributeData<T>::TypedDPAttributeData;
    DPAttributeData::Ptr copy() const override {
        voidApi("metadata.clone");
        ++retainedFieldCopies;
        return std::make_shared<ObservedField>(*this);
    }
};
void setup() {
    resetCompute(); retainedFieldCopies = 0;
    inputPoints = DPData(); outputPoints = DPData();
    inputPoints.points = {{1, 2, 3}, {4, 5, 6}};
    inputPoints.attributes["keep"] = std::make_shared<ObservedField<int32_t>>(
        std::vector<int32_t>{7, 8}, 9);
    auto group = std::make_shared<Group>(); group->ids = {true, false};
    inputPoints.groups["keep"] = group;
    outputPoints.points = {{-99, 0, 0}};
}
MDataBlock block(short deform = NOISE_DEFROM_VECTOR, bool noiseField = true) {
    MDataBlock data;
    data.inputs["inDP"].payload = &inputPoints; data.inputs["inDP"].type = DPData::id;
    data.inputs["aIsNoiseAttribute"].number = noiseField;
    data.inputs["aDeformType"].number = deform;
    data.inputs["aDeformVector"].vector = Imath::V3d(1, 2, 3);
    data.inputs["aNoiseAttributeName"].text = "noise";
    data.inputs["aVectorAttributeName"].text = "vector";
    data.inputs["aAmplitude"].number = 2;
    data.inputs["aOctave"].number = data.inputs["aTurbulenceRoughnesss"].number = 1;
    data.inputs["aScale"].vector = Imath::V3d(1, 1, 1);
    data.inputs["aUniformScale"].number = data.inputs["aFrequency"].number = 1;
    return data;
}
std::string state(const DPData& data) {
    std::ostringstream stream;
    for (const auto& point : data.points)
        for (unsigned axis = 0; axis < 3; ++axis) stream << point[axis] << ',';
    for (const auto& field : data.attributes) stream << field.first << ':' << field.second->str();
    for (const auto& group : data.groups) {
        stream << group.first;
        for (bool value : group.second->ids) stream << value;
    }
    return stream.str();
}
MStatus compute(MDataBlock& data) { return DPNoise().compute(MPlug(DPNoise::outDP), data); }
bool called(const char* label) {
    return std::find(operations.begin(), operations.end(), label) != operations.end();
}
void valuesAndOwnership() {
    for (short deform : {NOISE_DEFROM_NONE, NOISE_DEFROM_VECTOR, NOISE_DEFROM_ATTRIBUTE})
        for (bool noiseField : {false, true}) {
            setup(); auto data = block(deform, noiseField);
            const DPointList vectors{{2, 0, 0}, {3, 0, 0}};
            inputPoints.attributes["vector"] = std::make_shared<ObservedField<Imath::V3f>>(vectors);
            const auto before = state(inputPoints);
            assert(compute(data) && publications == 1 && retainedFieldCopies == 2 && geometryCopies == 1);
            assert(std::count(operations.begin(), operations.end(), "bounds.update") == 1);
            assert(!called("point.copy") && !called("point.replace") && called("point.batch"));
            for (std::size_t index = 0; index < inputPoints.points.size(); ++index)
                for (unsigned axis = 0; axis < 3; ++axis) {
                    const float direction = deform == NOISE_DEFROM_ATTRIBUTE ? vectors[index][axis] : float(axis + 1);
                    const float offset = deform == NOISE_DEFROM_NONE ? 0 : direction * float(index + 1) * 0.5f;
                    assert(outputPoints.points[index][axis] == inputPoints.points[index][axis] + offset);
                }
            assert(state(inputPoints) == before);
            assert(outputPoints.attributes.at("keep") != inputPoints.attributes.at("keep"));
            assert(outputPoints.groups.at("keep") != inputPoints.groups.at("keep"));
            const auto field = std::dynamic_pointer_cast<const DPIntAttr>(outputPoints.getAttr("keep"));
            assert(field && field->defaultValue() == 9 && field->value() == std::vector<int32_t>({7, 8}));
            assert(outputPoints.isExistAttr("noise") == noiseField);
            if (noiseField) {
                const auto noise = std::dynamic_pointer_cast<const DPFloatAttr>(outputPoints.getAttr("noise"));
                assert(noise && noise->value() == std::vector<float>({0.25f, 0.5f}));
            }
        }
    setup(); inputPoints.points.clear();
    inputPoints.attributes["keep"] = std::make_shared<ObservedField<int32_t>>(std::vector<int32_t>(), 9);
    inputPoints.groups["keep"]->ids.clear(); auto empty = block();
    assert(compute(empty) && outputPoints.points.empty() && retainedFieldCopies == 1);
    assert(outputPoints.isExistAttr("keep") && outputPoints.isExistAttr("noise"));
    // Read an input vector completely before replacing that same output name
    // with a scalar signal. Existing differing stored types remain replaceable.
    setup(); inputPoints.attributes["vector"] = std::make_shared<ObservedField<Imath::V3f>>(
        DPointList{{2, 0, 0}, {3, 0, 0}});
    auto alias = block(NOISE_DEFROM_ATTRIBUTE); alias.inputs["aNoiseAttributeName"].text = "vector";
    const auto before = state(inputPoints);
    assert(compute(alias) && state(inputPoints) == before && outputPoints.getAttr("vector")->getAttrType() == DP_ATTR_FLOAT);
    assert(outputPoints.points[0][0] == 2 && outputPoints.points[1][0] == 7);
    setup(); inputPoints.attributes["noise"] = std::make_shared<DPStringAttr>(std::vector<std::string>{"a", "b"});
    auto replacement = block(); const auto old = inputPoints.getAttr("noise");
    assert(compute(replacement) && outputPoints.getAttr("noise")->getAttrType() == DP_ATTR_FLOAT);
    assert(inputPoints.getAttr("noise") == old && old->getAttrType() == DP_ATTR_STRING);
}
void failures() {
    for (bool noiseField : {false, true}) {
        setup(); auto data = block(NOISE_DEFROM_VECTOR, noiseField); assert(compute(data));
        const auto trace = operations; const int calls = callCount;
        for (int index = 0; index < calls; ++index) {
            for (MS::MStatusCode failure : {MS::kFailure, MS::kInvalidParameter}) {
                setup(); data = block(NOISE_DEFROM_VECTOR, noiseField);
                const auto source = state(inputPoints), previous = state(outputPoints);
                resetCompute(index); injectedStatus = failure;
                assert(!compute(data) && state(inputPoints) == source);
                if (trace[index] != "output.clean") assert(publications == 0 && state(outputPoints) == previous);
                resetCompute();
                assert(compute(data) && publications == 1 && state(inputPoints) == source);
            }
            setup(); data = block(NOISE_DEFROM_VECTOR, noiseField);
            const auto source = state(inputPoints), previous = state(outputPoints);
            resetCompute(); attributeThrowAt = index;
            assert(!compute(data) && state(inputPoints) == source);
            if (trace[index] != "output.clean") assert(publications == 0 && state(outputPoints) == previous);
            resetCompute();
            assert(compute(data) && publications == 1 && state(inputPoints) == source);
        }
    }
    setup(); auto data = block();
    for (int bad = 0; bad < 4; ++bad) {
        setup(); data = block(); const auto previous = state(outputPoints);
        if (bad == 0) nullCreated = true;
        if (bad == 1) rejectFinal = true;
        if (bad == 2) data.inputs["aAmplitude"].number = INFINITY;
        if (bad == 3) data.inputs["aNoiseAttributeName"].text = "";
        assert(!compute(data) && publications == 0 && state(outputPoints) == previous);
    }
    setup(); data = block();
    assert(DPNoise().compute(MPlug(MObject{"other"}), data) == MS::kUnknownParameter && operations.empty());
}
void reducedBudgets() {
    for (int variant = 0; variant < 4; ++variant) {
        setup(); auto data = block();
        if (variant == 0) inputPoints.attributes["large"] = std::make_shared<DPStringAttr>(std::vector<std::string>(2, std::string(256, 'x')));
        if (variant == 1) inputPoints.attributes["large"] = std::make_shared<DPIntArrayAttr>(std::vector<DIntArray>(2, DIntArray(100, 1)));
        if (variant == 2) for (int field = 0; field < 30; ++field)
            inputPoints.attributes["field" + std::to_string(field)] = std::make_shared<DPFloatAttr>(std::vector<float>{1, 2});
        if (variant == 3) for (int group = 0; group < 10; ++group) {
            auto value = std::make_shared<Group>(); value->ids = {true, false};
            inputPoints.groups[std::string(100, 'g') + std::to_string(group)] = value;
        }
        const auto previous = state(outputPoints), source = state(inputPoints);
        assert(!compute(data) && publications == 0 && geometryCopies == 0 && !called("noise.sample") && !called("output.create"));
        assert(state(outputPoints) == previous && state(inputPoints) == source);
    }
    // Source/staging fit alone; adding the requested output field crosses the
    // aggregate ceiling. No large native allocation is needed for this check.
    setup(); bool found = false;
    for (std::size_t padding = 0; padding <= 256U; ++padding) {
        inputPoints.attributes["padding"] = std::make_shared<DPStringAttr>(std::vector<std::string>(2, std::string(padding, 'x')));
        dommy::point::PointPayloadBudget budget;
        try {
            dommy::point::preflightPointPayload(inputPoints,
                sizeof(DPoint) + sizeof(DScalarData) + sizeof(Imath::V3d), budget);
        } catch (const std::length_error&) { continue; }
        try {
            budget.add(2U, sizeof(DScalarData)); budget.add(1U, sizeof(DScalarData));
            budget.add(5U, sizeof(char));
        } catch (const std::length_error&) { found = true; break; }
    }
    assert(found);
    auto data = block(NOISE_DEFROM_NONE, false);
    assert(compute(data)); resetCompute(); data.inputs["aIsNoiseAttribute"].number = 1;
    assert(!compute(data) && !called("noise.sample") && !called("output.create"));
}
int main(int argc, char**) {
    if (argc > 1) reducedBudgets(); else { valuesAndOwnership(); failures(); }
    std::cout << "Point coordinate output: owning copy, bounds, failure retention and payload checks passed\n";
}
