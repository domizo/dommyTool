// Complete callback, real typed metadata; native curve/ramp APIs are seams.
#define DOMMY_TYPED_CURVE_ATTRIBUTES
#include "curve_utility_doubles.h"
#include "curve_character_attribute.inc"

DCurveData inputData;
void setup() {
    reset();
    inputs.clear();
    geometries.clear();
    inputData = DCurveData();
    rejectTypedAttribute = false;
    auto curve = std::make_shared<DCurveInternalData>(
        MPointArray{{5, 0, 0}, {6, 0, 0}, {8, 0, 0}, {9, 0, 0}}, MDoubleArray{5, 5, 5, 9, 9, 9}, 3,
        false);
    curve->useGeometryFixture = true;
    curve->geometryFixture.mapClosestToDomain = true;
    curve->geometryFixture.points = curve->points;
    inputData.curves.push_back(curve);
    inputs["inDC"].plugin = &inputData;
    inputs["aAddLengthAttribute"].number = 1;
    inputs["aLengthAttributeName"].string = "len";
    inputs["aAddWidthAttribute"].number = 1;
    inputs["aWidthAttributeName"].string = "width";
    inputs["aAddParamAttribute"].number = 1;
    inputs["aParamAttributeName"].string = "param";
    inputs["aBaseWidth"].number = .05;
    inputs["aTipWidth"].number = .01;
    inputs["aWidthScale"].number = 1;
    inputs["aIsNormalizedParam"].number = 1;
}
MStatus compute() {
    DCCharactor node;
    MDataBlock block;
    return node.compute(MPlug({}, DCCharactor::outDC), block);
}
template <class T> const std::vector<T>& values(const DPAttributeData::ConstPtr& field) {
    const auto typed = std::dynamic_pointer_cast<const TypedDPAttributeData<T>>(field);
    assert(typed);
    return typed->value();
}
int main() {
    for (int mode : {0, 1}) {
        setup();
        inputs["aWidthType"].number = mode;
        inputData.fields["width"] = std::make_shared<DPIntAttr>(std::vector<int32_t>{7});
        inputData.curves[0]->fields["width"] =
            std::make_shared<DPStringAttr>(std::vector<std::string>(4, "old"));
        inputData.fields["len"] = std::make_shared<DPStringAttr>(std::vector<std::string>{"old"});
        inputData.curves[0]->fields["param"] =
            std::make_shared<DPIntAttr>(std::vector<int32_t>(4, 8));
        assert(compute() == MS::kSuccess);
        assert(values<int32_t>(outputData.getAttr("width"))[0] == 7);
        assert(values<float>(outputData.getAttr("len"))[0] == 4);
        assert(values<float>(outputData.curves[0]->getAttr("width"))[0] == (mode ? 0 : float(.05)));
        assert(values<float>(outputData.curves[0]->getAttr("param"))[0] == 0);
        assert(values<std::string>(inputData.curves[0]->getAttr("width"))[0] == "old");
        assert(values<int32_t>(inputData.curves[0]->getAttr("param"))[0] == 8);
        assert(values<std::string>(inputData.getAttr("len"))[0] == "old");
    }
    setup();
    inputs["aLengthAttributeName"].string = "width";
    assert(compute() == MS::kSuccess);
    assert(values<float>(outputData.getAttr("width"))[0] == 4);
    assert(values<float>(outputData.curves[0]->getAttr("width"))[0] == float(.05));
    setup();
    inputs["aParamAttributeName"].string = "width";
    assert(compute() == MS::kInvalidParameter && published == 0);
    setup();
    inputs["aAddParamAttribute"].number = 0;
    inputs["aParamAttributeName"].string = "width";
    assert(compute() == MS::kSuccess);
    setup();
    inputs["aLengthAttributeName"].string = "param";
    assert(compute() == MS::kSuccess);
    assert(values<float>(outputData.getAttr("param"))[0] == 4);
    assert(values<float>(outputData.curves[0]->getAttr("param"))[0] == 0);
    setup();
    inputs["aWidthAttributeName"].string = "";
    assert(compute() == MS::kSuccess && !outputData.curves[0]->getAttr("width"));
    setup();
    inputs["aWidthAttributeName"].string.assign(
        dommy::generation::kMaxGeneratedStringByteCount + 1U, 'x');
    assert(compute() == MS::kInvalidParameter && published == 0);
    for (int output = 0; output < 4; ++output) {
        setup();
        inputs["aAddLengthAttribute"].number = output == 0;
        inputs["aAddWidthAttribute"].number = output == 1 || output == 2;
        inputs["aWidthType"].number = output == 2;
        inputs["aAddParamAttribute"].number = output == 3;
        rejectTypedAttribute = true;
        assert(compute() == MS::kFailure && published == 0);
    }
    setup();
    assert(compute() == MS::kSuccess);
    const auto trace = operations;
    for (std::size_t at = 0; at < trace.size(); ++at)
        for (int mode = 0; mode < 3; ++mode) {
            setup();
            if (mode == 2)
                throwAt = at;
            else {
                failAt = at;
                injectedStatus = mode ? MS::kInvalidParameter : MS::kFailure;
            }
            assert(compute() != MS::kSuccess);
            if (trace[at] != "clean")
                assert(published == 0);
            assert(inputData.curves[0]->fields.empty());
            setup();
            assert(compute() == MS::kSuccess);
        }
    setup();
    inputData.curves.clear();
    inputs["aParamAttributeName"].string = "width";
    assert(compute() == MS::kSuccess && outputData.curves.empty());
    std::cout << "Curve character: real typed Point/Primitive scope, replacement and name "
                 "collisions passed\n";
}
