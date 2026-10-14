#include "motion_rotation_doubles.h"
#include "motion_rotation_methods.inc"

namespace {
void vectorInput(const char* name, double x, double y, double z) {
    auto& v = inputs[name].vector;
    v[0] = x;
    v[1] = y;
    v[2] = z;
}
void configure(bool motion, int scenario) {
    resetRun();
    inputs.clear();
    inputData = DPData();
    stagedData = DPData();
    publishedData = DPData();
    vectorInput("ForwardVector", 0, 0, 1);
    vectorInput("UpVector", 0, 1, 0);
    vectorInput("SpaceForwardAxis", 0, 1, 0);
    vectorInput("SpaceUpAxis", 1, 0, 0);
    vectorInput("Rotate", 13, 27, -8);
    vectorInput("TargetAxis", 0, 1, 0);
    vectorInput("TargetVec", 0, 0, 1);
    vectorInput("UpAxis", 1, 0, 0);
    vectorInput("UpVec", 0, 1, 0);
    inputs["GlobalOffset"].number = 0.25;
    inputs["OutFowardAttribute"].string = "fwvec";
    inputs["OutUpAttribute"].string = "upvec";
    inputs["OutMatrixAttribute"].string = "rotmat";
    // Preserve an existing result to check overwrite behavior, and unrelated data.
    inputData.fields["fwvec"] = std::make_shared<DPVec3dAttr>(
        std::vector<Imath::V3d>{{2, 3, 4}, {5, 6, 7}, {8, 9, 10}});
    inputData.fields["custom"] = std::make_shared<DPVec3dAttr>(
        std::vector<Imath::V3d>{{0, 1, 0}, {1, 0, 0}, {0, 0, 1}});
    inputData.fields["wrongType"] = std::make_shared<DPMat4Attr>(std::vector<MMatrix>(3));
    if (motion) {
        inputs["AnimationMatrix"].connected = scenario != 0;
        inputs["IsAxisAlign"].number = scenario >= 2;
        inputs["IsUseUpVector"].number = scenario >= 3;
        inputs["OffsetType"].number = scenario % 3;
        inputs["StretchType"].number = (scenario / 2) % 3;
        if (scenario >= 4) {
            inputs["TargetType"].number = inputs["UpType"].number = 1;
            inputs["TargetAtttribute"].string =
                scenario == 4 ? "custom" : (scenario == 5 ? "missing" : "wrongType");
            inputs["UpAtttribute"].string = "custom";
        }
        if (scenario == 7) {
            inputData.points.clear();
            inputData.fields.clear();
        }
    } else {
        inputs["RotateType"].number = (scenario / 12) % 2;
        inputs["RotateSpace"].number = (scenario / 6) % 2;
        inputs["IsUseUpVector"].number = (scenario / 3) % 2;
        inputs["OutputType"].number = scenario % 3;
        inputs["RotateOrder"].number = scenario % 6;
        inputs["RotationMatrix"].connected = scenario < 24;
        inputs["Rotate"].connected = scenario % 3 == 0;
        inputs["Rotate"].childConnected = scenario % 3 == 1;
        inputs["OffsetType"].number = scenario % 3;
        inputs["StretchType"].number = (scenario / 2) % 3;
        if (scenario >= 24) {
            inputs["RotateType"].number = scenario == 27;
            inputs["ForwardVectorAttribute"].string = scenario == 25 ? "wrongType" : "custom";
            inputs["UpVectorAttribute"].string = "custom";
            inputs["RotateAttribute"].string =
                scenario == 24 ? "custom" : (scenario == 25 ? "wrongType" : "missing");
        }
        if (scenario == 28)
            inputs["OutFowardAttribute"].string = inputs["OutUpAttribute"].string = "";
        if (scenario == 29) {
            inputData.points.clear();
            inputData.fields.clear();
        }
    }
    resetRun();
}
MStatus compute(bool motion) {
    MDataBlock block;
    if (motion) {
        DPMotionCreate node;
        return node.compute(MPlug(node.thisMObject(), DPMotionCreate::outDP), block);
    }
    DPRotation node;
    return node.compute(MPlug(node.thisMObject(), DPRotation::outDP), block);
}
std::string trace() {
    std::ostringstream s;
    s << std::setprecision(17);
    for (const auto& p : publishedData.points)
        s << "p " << p.x << ' ' << p.y << ' ' << p.z << '\n';
    for (const auto& a : publishedData.fields) {
        if (a.second->getAttrType() != DP_ATTR_VEC3D) continue;
        s << "v " << a.first << '\n';
        for (const auto& v : publishedData.values<Imath::V3d>(a.first))
            s << v.x << ' ' << v.y << ' ' << v.z << '\n';
    }
    for (const auto& a : publishedData.fields) {
        if (a.second->getAttrType() != DP_ATTR_MATRIX) continue;
        s << "m " << a.first << '\n';
        for (const auto& m : publishedData.values<MMatrix>(a.first)) {
            for (const auto& row : m.values)
                for (double v : row)
                    s << v << ' ';
            s << '\n';
        }
    }
    s << "times";
    for (double t : sampledTimes)
        s << ' ' << t;
    s << '\n';
    return s.str();
}
std::string normalTraces() {
    std::string result;
    for (bool motion : {true, false})
        for (int scenario = 0; scenario < (motion ? 8 : 30); ++scenario) {
            configure(motion, scenario);
            assert(compute(motion) == MS::kSuccess);
            assert(publications == 1 && cleanCalls == 1);
            result +=
                (motion ? "motion " : "rotation ") + std::to_string(scenario) + "\n" + trace();
        }
    return result;
}
void noResult() {
    assert(publications == 0 && cleanCalls == 0 && outOfBounds == 0);
}
int failureCases(bool motion, int scenario) {
    configure(motion, scenario);
    assert(compute(motion) == MS::kSuccess);
    assert(workerCopies == 0);
    if (motion) assert(dataCopies == 1 && boundsUpdates == 1);
    const auto baseline = operations;
    const std::string expected = trace();
    int tested = 0;
    for (int i = 0; i < static_cast<int>(baseline.size()); ++i) {
        for (auto code : {MS::kFailure, MS::kInvalidParameter}) {
            configure(motion, scenario);
            failAt = i;
            injectedStatus = code;
            const MStatus status = compute(motion);
            // Legacy void attribute readers translate their helper failure into
            // an exception; direct Maya status APIs must retain the exact code.
            assert(status == (baseline[i].find("attribute ") == 0 ? MS::kFailure : code));
            assert(outOfBounds == 0);
            if (baseline[i].find("input ") == 0 || baseline[i].find("output ") == 0 ||
                baseline[i].find("plug.") == 0)
                assert(callCount == i + 1);
            // Clean failure follows a successful publication; there is no rollback promise.
            if (baseline[i] == "clean")
                assert(publications == 1 && cleanCalls == 0);
            else
                noResult();
            configure(motion, scenario);
            assert(compute(motion) == MS::kSuccess);
            assert(trace() == expected);
            ++tested;
        }
        if (baseline[i].find(".allocate") != std::string::npos ||
            baseline[i].find(".copy") != std::string::npos) {
            configure(motion, scenario);
            shortAt = i;
            const auto status = compute(motion);
            if (shortApplied) {
                assert(status != MS::kSuccess);
                noResult();
                ++tested;
            } else
                assert(status == MS::kSuccess);
        }
        configure(motion, scenario);
        throwAt = i;
        assert(compute(motion) != MS::kSuccess);
        if (baseline[i] == "clean")
            assert(publications == 1 && cleanCalls == 0);
        else
            noResult();
        ++tested;
    }
    return tested;
}
void invalidCases() {
    for (bool motion : {true, false}) {
        configure(motion, 0);
        inputData.valid = false;
        assert(compute(motion) == MS::kInvalidParameter);
        noResult();
        configure(motion, 0);
        inputData.fakeCount = dommy::generation::kMaxGeneratedTransformCount + 1;
        assert(compute(motion) == MS::kInvalidParameter);
        noResult();
        assert(std::find(operations.begin(), operations.end(), "plugin.create") ==
               operations.end());
        configure(motion, 0);
        throwDataCopy = true;
        assert(compute(motion) == MS::kFailure);
        noResult();
        configure(motion, 0);
        nullCreated = true;
        assert(compute(motion) == MS::kFailure);
        noResult();
        for (const char* name : {"OffsetType", "StretchType"})
            for (double v : {-1., 3.}) {
                configure(motion, 0);
                inputs[name].number = v;
                assert(compute(motion) == MS::kInvalidParameter);
                noResult();
            }
        configure(motion, 0);
        inputs["GlobalOffset"].number = std::numeric_limits<double>::infinity();
        assert(compute(motion) == MS::kInvalidParameter);
        noResult();
        configure(motion, motion ? 4 : 24);
        shortAttribute = true;
        assert(compute(motion) == MS::kFailure);
        noResult();
    }
    for (const char* name : {"TargetType", "UpType"})
        for (double v : {-1., 2.}) {
            configure(true, 3);
            inputs[name].number = v;
            assert(compute(true) == MS::kInvalidParameter);
            noResult();
        }
    configure(true, 3);
    failReplacement = true;
    assert(compute(true) == MS::kFailure);
    noResult();
    configure(true, 1);
    sampledTranslationScale = 1e50;
    assert(compute(true) == MS::kInvalidParameter);
    noResult();
    for (const char* name : {"TargetAxis", "TargetVec", "UpAxis", "UpVec"}) {
        configure(true, 3);
        inputs[name].vector[0] = std::numeric_limits<double>::quiet_NaN();
        assert(compute(true) == MS::kInvalidParameter);
        noResult();
    }
    for (const char* name : {"RotateType", "RotateSpace", "RotateOrder", "OutputType"})
        for (double v : {-1., 99.}) {
            configure(false, 0);
            inputs[name].number = v;
            assert(compute(false) == MS::kInvalidParameter);
            noResult();
        }
    configure(false, 2);
    inputs["OutMatrixAttribute"].string = "";
    assert(compute(false) == MS::kInvalidParameter);
    noResult();
    for (const char* name : {"ForwardVector", "UpVector", "Rotate"}) {
        configure(false, 0);
        inputs[name].vector[0] = std::numeric_limits<double>::quiet_NaN();
        assert(compute(false) == MS::kInvalidParameter);
        noResult();
    }
    configure(true, 0);
    MDataBlock block;
    DPMotionCreate motion;
    DPRotation rotation;
    const MPlug unknown(MObject{"node"}, MObject{"unknown"});
    assert(motion.compute(unknown, block) == MS::kUnknownParameter);
    assert(rotation.compute(unknown, block) == MS::kUnknownParameter);
    assert(callCount == 0);
}
void outputAttributeCases() {
    configure(false, 1);
    inputs["OutFowardAttribute"].string = inputs["OutUpAttribute"].string = "shared";
    assert(compute(false) == MS::kInvalidParameter);
    noResult();
    assert(std::find(operations.begin(), operations.end(), "plugin.create") == operations.end());
    for (int mode : {0, 2}) {
        configure(false, mode);
        inputs["OutFowardAttribute"].string = inputs["OutUpAttribute"].string = "shared";
        assert(compute(false) == MS::kSuccess); // Unused names cannot collide.
    }
    for (const char* name : {"OutFowardAttribute", "OutUpAttribute", "OutMatrixAttribute"}) {
        const int mode = std::string(name) == "OutMatrixAttribute" ? 2 : 1;
        for (const std::string& invalid : {
                std::string(dommy::generation::kMaxGeneratedStringByteCount + 1U, 'x')}) {
            configure(false, mode); inputs[name].string = invalid.c_str();
            assert(compute(false) == MS::kInvalidParameter); noResult();
            assert(dataCopies == 0);
            configure(false, 29); inputs[name].string = invalid.c_str();
            assert(compute(false) == MS::kSuccess); // Empty input bypasses output controls.
        }
        configure(false, mode); inputs[name].string = "newField";
        if (mode != 2)
            inputs[std::string(name) == "OutUpAttribute" ? "OutFowardAttribute" : "OutUpAttribute"].string = "";
        rejectAttribute = true;
        assert(compute(false) == MS::kFailure); noResult();
        assert(!inputData.getAttr("newField"));
        resetRun(); assert(compute(false) == MS::kSuccess);
        const auto field = publishedData.getAttr("newField");
        assert(field && field->attrSize() == 3 &&
               field->getAttrType() == (mode == 2 ? DP_ATTR_MATRIX : DP_ATTR_VEC3D));
    }
    for (int mode : {0, 1, 2}) {
        configure(false, mode);
        const char* output = mode == 2 ? "OutMatrixAttribute" : "OutFowardAttribute";
        inputs[output].string = mode == 2 ? "custom" : "wrongType";
        const auto source = inputData.getAttr(inputs[output].string.asChar());
        assert(compute(false) == MS::kSuccess);
        const auto result = publishedData.getAttr(inputs[output].string.asChar());
        assert(result && result != source);
        assert(result->getAttrType() == (mode == 2 ? DP_ATTR_MATRIX : DP_ATTR_VEC3D));
        assert(inputData.getAttr(inputs[output].string.asChar()) == source);
        if (mode != 2) {
            const auto typed = std::dynamic_pointer_cast<const DPVec3dAttr>(result);
            const auto value = typed->getDefaultValue();
            assert(value.x == 0 && value.y == 0 && value.z == 0);
        }
    }
    configure(false, 1); inputs["OutFowardAttribute"].string = inputs["OutUpAttribute"].string = "";
    assert(compute(false) == MS::kSuccess && publishedData.fields.size() == inputData.fields.size());
    // Demonstrate faithful legacy writer refusal, independently of the fix.
    DPData owner;
    owner.addAttr("same", DPVec3dAttr(std::vector<Imath::V3d>(3)));
    owner.addAttr("same", DPMat4Attr(std::vector<MMatrix>(3)));
    assert(owner.getAttr("same")->getAttrType() == DP_ATTR_VEC3D);
}
void alignmentCases() {
    for (int type : {0, 1})
        for (int order = 0; order < 6; ++order)
            for (int output = 0; output < 3; ++output) {
                configure(false, 0);
                inputs["RotateType"].number = type;
                inputs["RotateSpace"].number = 1;
                inputs["RotateOrder"].number = order;
                inputs["OutputType"].number = output;
                inputs["Rotate"].connected = inputs["Rotate"].childConnected = false;
                inputs["RotationMatrix"].connected = false;
                vectorInput("Rotate", 0, 0, 0);
                inputs["IsUseUpVector"].number = 1;
                assert(compute(false) == MS::kSuccess);
                const std::string aligned = trace();
                // A zero animation rotation must preserve the intended point
                // frame independently of Euler order and input rotation mode.
                if (output < 2) {
                    const auto& f = publishedData.values<Imath::V3d>("fwvec")[0];
                    assert(f.x == 0 && f.y == 0 && f.z == 1);
                    if (output == 1) {
                        const auto& u = publishedData.values<Imath::V3d>("upvec")[0];
                        assert(u.x == 0 && u.y == 1 && u.z == 0);
                    }
                } else {
                    const auto& m = publishedData.values<MMatrix>("rotmat")[0];
                    assert(m[1][0] == 0 && m[1][1] == 0 && m[1][2] == 1);
                    assert(m[0][0] == 0 && m[0][1] == 1 && m[0][2] == 0);
                }
                for (double scale : {1e-200, 1e200}) {
                    vectorInput("SpaceForwardAxis", 0, scale, 0);
                    vectorInput("SpaceUpAxis", scale, 0, 0);
                    vectorInput("ForwardVector", 0, 0, scale);
                    vectorInput("UpVector", 0, scale, 0);
                    resetRun();
                    assert(compute(false) == MS::kSuccess);
                    assert(trace() == aligned);
                }
                vectorInput("SpaceForwardAxis", 0, 1, 0);
                vectorInput("SpaceUpAxis", 1, 0, 0);
                vectorInput("ForwardVector", 0, 0, 1);
                vectorInput("UpVector", 0, 1, 0);
                inputs["IsUseUpVector"].number = 0;
                resetRun();
                assert(compute(false) == MS::kSuccess);
                const std::string forwardOnly = trace();
                vectorInput("UpVector", 0, 0, 1); // Parallel to point forward.
                resetRun();
                assert(compute(false) == MS::kSuccess);
                assert(trace() == forwardOnly); // Disabled means ignored in both modes.
                inputs["IsUseUpVector"].number = 1;
                resetRun();
                assert(compute(false) == MS::kInvalidParameter);
                noResult();
                vectorInput("UpVector", 0, 1, 0);
                vectorInput("SpaceUpAxis", 0, 1, 0); // Parallel source axes.
                resetRun();
                assert(compute(false) == MS::kInvalidParameter);
                noResult();
                vectorInput("SpaceUpAxis", 1, 0, 0);
                resetRun();
                assert(compute(false) == MS::kSuccess);
                assert(trace() == aligned);
            }
}
std::string readFile(const char* path) {
    std::ifstream f(path);
    assert(f);
    return {std::istreambuf_iterator<char>(f), {}};
}
} // namespace

int main(int argc, char** argv) {
    const std::string schema = schemaTrace("DPMotionCreate", DPMotionCreate::initialize) +
                               schemaTrace("DPRotation", DPRotation::initialize);
    if (argc == 2 && std::string(argv[1]) == "--budget") {
        for(bool motion:{false,true}) {
            configure(motion,2);inputData.metadataBytes=3000U;resetRun();
            assert(compute(motion)==MS::kFailure && dataCopies==0 && publications==0);
            assert(std::find(operations.begin(),operations.end(),"plugin.create")==operations.end());
            assert(inputData.points[0].x==1 && inputData.metadataBytes==3000U);
            inputData.metadataBytes=0U;resetRun();
            assert(compute(motion)==MS::kSuccess && publications==1 && dataCopies==1);
            publishedData.points[0].x=99;assert(inputData.points[0].x==1);
        }
        std::cout<<"Animation aggregate budget, pre-copy rejection and retry: OK\n";return 0;
    }
    if (argc == 2 && std::string(argv[1]) == "--schema") {
        std::cout << schema;
        return 0;
    }
    if (argc == 2 && std::string(argv[1]) == "--trace") {
        std::cout << normalTraces();
        return 0;
    }
    assert(argc == 3);
    assert(schema == readFile(argv[1]));
    assert(normalTraces() == readFile(argv[2]));
    const int initializationCases =
        testFailures("DPMotionCreate", DPMotionCreate::initialize, 100) +
        testFailures("DPRotation", DPRotation::initialize, 136);
    int computeCases = 0;
    for (int scenario : {0, 3, 4, 7})
        computeCases += failureCases(true, scenario);
    for (int scenario : {0, 4, 7, 11, 13, 19, 23, 24, 25, 26, 27, 29})
        computeCases += failureCases(false, scenario);
    invalidCases();
    outputAttributeCases();
    alignmentCases();
    std::cout << "Motion/rotation: " << initializationCases << " initializer and " << computeCases
              << " compute failure cases; 38 normal traces, two schemas and 36 alignment/flag "
                 "combinations passed.\n";
}
