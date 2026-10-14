#include "mesh_metadata_fixture.h"
#include "mesh_copy.inc"

static MStatus compute() {
    MDataBlock block;
    return DMCopyTransform().compute(MPlug{DMCopyTransform::outDM}, block);
}
static void verifyCopies(const DMeshData& output, const DMeshData& input, std::size_t copies) {
    assert(output.hasValidMeshTopology() && output.meshItemArray.size() == input.numberOfMesh() * copies);
    auto rows = std::dynamic_pointer_cast<DPIntAttr>(output.mAttributeList.at("v"));
    auto original = std::dynamic_pointer_cast<DPIntAttr>(input.mAttributeList.at("v"));
    assert(rows != original && rows->defaultValue() == original->defaultValue());
    const auto& groups = output.mGroupList.at("g")->getGroupIds();
    assert(output.mGroupList.at("g") != input.mGroupList.at("g"));
    for (std::size_t i = 0; i < output.numberOfMesh(); ++i) {
        const auto& mesh = output.meshItemArray[i];
        const auto& source = input.meshItemArray[i % input.numberOfMesh()];
        assert(mesh != source && mesh->geometry == source->geometry && mesh->uvIds == source->uvIds);
        assert(mesh->uvs == source->uvs && mesh->pointAttrs.size() == source->pointAttrs.size());
        assert(mesh->pointAttrs.at("p") != source->pointAttrs.at("p"));
        assert(mesh->faceAttrs.at("f") != source->faceAttrs.at("f"));
        assert(mesh->cornerAttrs.at("N") != source->cornerAttrs.at("N"));
        assert(std::dynamic_pointer_cast<DPIntAttr>(mesh->pointAttrs.at("p"))->value() ==
               std::dynamic_pointer_cast<DPIntAttr>(source->pointAttrs.at("p"))->value());
        assert(std::dynamic_pointer_cast<DPIntAttr>(mesh->faceAttrs.at("f"))->value() ==
               std::dynamic_pointer_cast<DPIntAttr>(source->faceAttrs.at("f"))->value());
        const auto& normals = std::dynamic_pointer_cast<DPVec3fAttr>(mesh->cornerAttrs.at("N"))->value();
        const auto& sourceNormals = std::dynamic_pointer_cast<DPVec3fAttr>(source->cornerAttrs.at("N"))->value();
        for (std::size_t row = 0; row < normals.size(); ++row)
            for (int axis = 0; axis < 3; ++axis) assert(normals[row][axis] == sourceNormals[row][axis]);
        assert(mesh->groups.at("g") != source->groups.at("g"));
        assert(mesh->faceGroups.at("g") != source->faceGroups.at("g"));
        assert(mesh->edgeGroups.at("g") != source->edgeGroups.at("g"));
        assert(rows->value()[i] == original->value()[i % input.numberOfMesh()]);
        assert(groups[i] == input.mGroupList.at("g")->getGroupIds()[i % input.numberOfMesh()]);
        for (std::size_t j = 0; j < i; ++j) assert(output.meshItemArray[j] != mesh);
    }
}
static void successCases() {
    for (int count : {0, 1, 3}) {
        inputData = twoMeshes(); const auto saved = inputData;
        reset(); requestedCount = count; translation = MVector(2, 3, 4); scaling = MVector(2, 1, 1);
        assert(compute()); const int effective = count == 0 ? 1 : count;
        assert(meshCopies == 2 * effective && boundsCalls == 1 && creations == 1 && publications == 1 && cleanCalls == 1);
        verifyCopies(publishedData, inputData, effective); unchanged(inputData, saved);
        assert(matrixCalls == 5 * 2 * (effective - 1) && setMatrixCalls == 2 * (effective - 1));
        for (int i = 0; i < effective; ++i) for (int j = 0; j < 2; ++j) {
            const auto matrix = publishedData.meshItemArray[2 * i + j]->matrix;
            assert(matrix[3][0] == 2 * i && matrix[0][0] == std::pow(2, i));
        }
        auto outputField = std::dynamic_pointer_cast<DPIntAttr>(publishedData.meshItemArray[0]->pointAttrs.at("p"));
        assert(outputField->setValueAt(0, 999));
        assert(std::dynamic_pointer_cast<DPIntAttr>(inputData.meshItemArray[0]->pointAttrs.at("p"))->value()[0] == 1);
    }
    inputData = twoMeshes();
    inputData.meshItemArray[0]->matrix[3][0] = 5;
    inputData.meshItemArray[1]->pointAttrs.clear(); inputData.meshItemArray[1]->clearUVs();
    reset(); requestedCount = 3; translation = MVector(2, 0, 0);
    assert(compute() && meshCopies == 6 && boundsCalls == 1);
    for (int i = 0; i < 3; ++i) {
        assert(publishedData.meshItemArray[2 * i]->matrix[3][0] == 5 + 2 * i);
        assert(publishedData.meshItemArray[2 * i]->hasUV());
        assert(publishedData.meshItemArray[2 * i + 1]->pointAttrs.empty());
        assert(!publishedData.meshItemArray[2 * i + 1]->hasUV());
    }
    assert(inputData.meshItemArray[0]->matrix[3][0] == 5 && inputData.meshItemArray[1]->pointAttrs.empty());
    inputData = fixture(); reset(); requestedCount = 3; freezeTransform = true; translation = MVector(3, 0, 0);
    assert(compute() && bakeCalls == 2 && setMatrixCalls == 0 && meshCopies == 3 && boundsCalls == 1);
    assert(publishedData.meshItemArray[0]->transform == 1 && publishedData.meshItemArray[1]->transform == 3 &&
           publishedData.meshItemArray[2]->transform == 6);
    DMeshData empty;
    empty.mAttributeList["v"] = attr<int>({}, -4);
    empty.mGroupList["g"] = std::make_shared<DPGroupData>(DPIdList{});
    inputData = empty; reset(); requestedCount = 1000000;
    uniformScale = std::numeric_limits<double>::infinity(); rotationOrder = -1;
    assert(compute() && meshCopies == 0 && matrixCalls == 0 && boundsCalls == 1);
    assert(publishedData.mAttributeList.count("v") && publishedData.mGroupList.count("g"));
    inputData = fixture(); reset(); requestedCount = 0;
    uniformScale = std::numeric_limits<double>::infinity(); rotationOrder = -1;
    assert(compute() && meshCopies == 1 && matrixCalls == 0);
    inputData = fixture();
    inputData.mAttributeList["b"] = attr<bool>({false}, true);
    inputData.mAttributeList["ia"] = attr<DIntArray>({DIntArray{1, 2}}, DIntArray{9});
    MMatrix matrix; matrix[3][0] = 7;
    inputData.mAttributeList["m"] = attr<MMatrix>({matrix});
    reset(); requestedCount = 3; assert(compute());
    assert(std::dynamic_pointer_cast<TypedDPAttributeData<bool>>(publishedData.mAttributeList.at("b"))->value() ==
           std::vector<bool>({false, false, false}));
    const auto arrays = std::dynamic_pointer_cast<TypedDPAttributeData<DIntArray>>(publishedData.mAttributeList.at("ia"));
    assert(arrays->defaultValue() == DIntArray{9} && arrays->value() == std::vector<DIntArray>({{1, 2}, {1, 2}, {1, 2}}));
    const auto matrices = std::dynamic_pointer_cast<TypedDPAttributeData<MMatrix>>(publishedData.mAttributeList.at("m"));
    assert(matrices->value().size() == 3 && matrices->value()[2][3][0] == 7);
}
static void admissionCases() {
    inputData = fixture();
    inputData.mAttributeList["v"] = attr<std::string>({std::string(55, 'a')}, std::string(6, 'd'));
    publishedData = fixture(7); const auto saved = publishedData;
    reset(); requestedCount = 2; assert(!compute()); unchanged(publishedData, saved);
    assert(creations == 0 && meshCopies == 0 && attributeCopies == 0 && boundsCalls == 0);
    auto output = saved;
    reset(); assert(!output.copyRepeatedFrom(inputData, 0)); unchanged(output, saved); assert(meshCopies == 0);
    reset(); assert(!output.copyRepeatedFrom(inputData, 2)); unchanged(output, saved);
    assert(meshCopies == 0 && attributeCopies == 0 && boundsCalls == 0);
    inputData.mAttributeList["v"] = attr<std::string>({std::string(54, 'a')}, std::string(6, 'd'));
    reset(); requestedCount = 2; assert(compute() && meshCopies == 2 && boundsCalls == 1);
    const auto strings = std::dynamic_pointer_cast<DPStringAttr>(publishedData.mAttributeList.at("v"));
    assert(strings->value().size() == 2 && strings->defaultValue() == std::string(6, 'd'));
    inputData = fixture(); inputData.mAttributeList["v"] = attr<DIntArray>({DIntArray(230, 1)});
    reset(); requestedCount = 2; assert(!compute() && creations == 0 && meshCopies == 0);
    inputData = fixture();
    inputData.meshItemArray[0]->pointAttrs["s"] = attr<std::string>({std::string(30, 'a'), std::string(30, 'b'), std::string(30, 'c')});
    reset(); requestedCount = 2; assert(!compute() && creations == 0 && meshCopies == 0);
    inputData = fixture();
    for (int count : {-1, 1000001, 1000000}) {
        reset(); requestedCount = count; assert(!compute() && creations == 0 && meshCopies == 0);
    }
    for (int mode = 0; mode < 6; ++mode) {
        reset(); requestedCount = 1;
        if (mode == 0) translation.x = std::numeric_limits<double>::quiet_NaN();
        if (mode == 1) rotation.z = std::numeric_limits<double>::infinity();
        if (mode == 2) scaling.y = std::numeric_limits<double>::quiet_NaN();
        if (mode == 3) pivotValue.x = std::numeric_limits<double>::infinity();
        if (mode == 4) uniformScale = std::numeric_limits<double>::infinity();
        if (mode == 5) rotationOrder = 6;
        assert(!compute() && creations == 0 && meshCopies == 0);
    }
}
static void rollbackCases() {
    inputData = fixture(); publishedData = fixture(7); const auto saved = publishedData;
    reset(); assert(compute()); const int totalCalls = hostCalls;
    for (int call = 0; call < totalCalls; ++call) for (bool throws : {false, true}) {
        reset(); publishedData = saved;
        if (throws) hostThrowAt = call; else hostFailAt = call;
        assert(!compute() && cleanCalls == 0);
        if (call < totalCalls - 1) {assert(publications == 0); unchanged(publishedData, saved);}
    }
    for (int mode : {1, 2, 3}) for (int index = 0; index < 3; ++index) {
        reset(); publishedData = saved; copyFailure = mode; failCopyAt = index;
        assert(!compute() && publications == 0 && cleanCalls == 0); unchanged(publishedData, saved);
    }
    for (int mode : {1, 2, 3}) for (int index = 1; index <= 10; ++index) for (bool group : {false, true}) {
        reset(); publishedData = saved; metadataFailure = mode;
        if (group) failGroupAt = index; else failAttributeAt = index;
        assert(!compute() && publications == 0); unchanged(publishedData, saved);
    }
    for (bool bake : {false, true}) {
        reset(); publishedData = saved; freezeTransform = bake; failBake = bake; failBounds = !bake;
        assert(!compute() && publications == 0); unchanged(publishedData, saved);
    }
    reset(); publishedData = saved; scaling.x = std::numeric_limits<double>::max();
    assert(!compute() && publications == 0); unchanged(publishedData, saved);
    reset(); auto alias = twoMeshes(); const auto original = alias;
    assert(alias.copyRepeatedFrom(alias, 3)); verifyCopies(alias, original, 3);
    assert(meshCopies == 6 && boundsCalls == 1);
    auto destination = saved; reset();
    assert(!destination.copyRepeatedFrom(inputData, 3, [](DMeshInternalData& mesh, std::size_t, std::size_t) {
        mesh.geometry[0] = -1; return MS::kFailure;
    })); unchanged(destination, saved); assert(inputData.meshItemArray[0]->geometry[0] == 1);
    reset();
    assert(!destination.copyRepeatedFrom(inputData, 2, [](DMeshInternalData& mesh, std::size_t, std::size_t) {
        mesh.pointAttrs["s"] = attr<std::string>({std::string(40, 'a'), std::string(40, 'b'), std::string(40, 'c')});
        return MS::kSuccess;
    })); unchanged(destination, saved);
    reset(); MDataBlock block;
    assert(DMCopyTransform().compute(MPlug{MObject{"unknown"}}, block) == MS::kUnknownParameter && hostCalls == 0);
    reset(); assert(compute() && publications == 1 && cleanCalls == 1);
}
int main() {
    successCases(); admissionCases(); rollbackCases();
    std::cout << "Complete mesh copy: metadata admission, single clones/bounds, ordering, controls and rollback passed\n";
}
