//
//  DMConvertFromMeshes.cpp
//  dommyTool
//
//  Created by yuzo on 2/9/17.
//

#include "common/utility/DMayaEntryPointGuard.h"
#include "DMConvertFromMeshes.h"
#include "common/utility/DStatusOperation.h"

#include "common/utility/DComputeGuard.h"
#include "common/utility/DGenerationLimits.h"
#include "common/utility/DMayaPluginDataOutput.h"
#include "common/utility/DMayaValueValidation.h"
#include "common/utility/DMayaMeshImport.h"
#include "common/utility/DMayaMeshTraversal.h"

#include "DMayaHeaders.h"
#include "DMeshDataHeaders.h"

#include <maya/MDagPath.h>
#include <maya/MFnMatrixAttribute.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <string>
#include <utility>
#include <vector>


namespace {

enum DMConvertMeshType {
    kConvertFull = 0,
    kConvertFullWithTransform = 1,
    // Value 2 was reserved by the original implementation.
    kConvertPerPolygon = 3
};

struct MeshInput {
    MObject meshObject;
    MMatrix worldMatrix;
    bool hasWorldMatrix;
    dommy::mesh::MeshMergePlan importPlan;

    MeshInput()
        : meshObject(),
          worldMatrix(MMatrix::identity),
          hasWorldMatrix(false)
    {
    }
};

MStatus getLegacyWorldMatrix(const MObject& node,
                          const MObject& meshAttribute,
                          unsigned int logicalIndex,
                          MMatrix& matrix,
                          bool& found)
{
    found = false;
    MStatus status;
    MPlug arrayPlug(node, meshAttribute);
    MPlug elementPlug =
        arrayPlug.elementByLogicalIndex(logicalIndex, &status);
    DOMMY_RETURN_IF_FAILURE(status);

    MPlug sourcePlug = elementPlug.source(&status);
    DOMMY_RETURN_IF_FAILURE(status);
    if (sourcePlug.isNull()) return MS::kSuccess;

    const MObject sourceNode = sourcePlug.node(&status);
    DOMMY_RETURN_IF_FAILURE(status);
    if (sourceNode.isNull()) return MS::kFailure;
    // Data from a non-DAG node has no legacy world transform to discover.
    if (!sourceNode.hasFn(MFn::kDagNode)) return MS::kSuccess;

    MDagPath dagPath;
    status = MDagPath::getAPathTo(sourceNode, dagPath);
    DOMMY_RETURN_IF_FAILURE(status);

    const MMatrix candidate = dagPath.inclusiveMatrix(&status);
    DOMMY_RETURN_IF_FAILURE(status);
    if (!dommy::maya::isFiniteMatrix(candidate)) return MS::kInvalidParameter;
    matrix = candidate;
    found = true;
    return MS::kSuccess;
}

MStatus collectMeshInputs(const MObject& node,
                          MDataBlock& data,
                          const MObject& meshAttribute,
                          const MObject& matrixAttribute,
                          std::vector<MeshInput>& inputs)
{
    MStatus status;
    MArrayDataHandle meshArray = data.inputArrayValue(meshAttribute, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const unsigned int meshCount = meshArray.elementCount(&status);
    DOMMY_RETURN_IF_FAILURE(status);
    if (meshCount > dommy::generation::kMaxConnectedGeometryCount)
        return MS::kInvalidParameter;

    MArrayDataHandle matrixArray = data.inputArrayValue(matrixAttribute, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const unsigned int matrixCount = matrixArray.elementCount(&status);
    DOMMY_RETURN_IF_FAILURE(status);
    if (matrixCount > dommy::generation::kMaxConnectedGeometryCount)
        return MS::kInvalidParameter;

    // Enumerate only indices: a failed jumpToElement cannot distinguish an
    // absent sparse element from a host failure. Do not read unused values.
    using MatrixElement = std::pair<unsigned int, unsigned int>;
    std::vector<MatrixElement> matrixElements;
    matrixElements.reserve(matrixCount);
    for (unsigned int physical = 0U; physical < matrixCount; ++physical) {
        DOMMY_RETURN_IF_FAILURE(matrixArray.jumpToArrayElement(physical));
        const unsigned int logical = matrixArray.elementIndex(&status);
        DOMMY_RETURN_IF_FAILURE(status);
        matrixElements.emplace_back(logical, physical);
    }
    std::sort(matrixElements.begin(), matrixElements.end());
    for (std::size_t i = 1U; i < matrixElements.size(); ++i) {
        if (matrixElements[i - 1U].first == matrixElements[i].first)
            return MS::kInvalidParameter;
    }

    std::vector<MeshInput> candidate;
    candidate.reserve(meshCount);
    for (unsigned int physical = 0U; physical < meshCount; ++physical) {
        DOMMY_RETURN_IF_FAILURE(meshArray.jumpToArrayElement(physical));
        const unsigned int logicalIndex = meshArray.elementIndex(&status);
        DOMMY_RETURN_IF_FAILURE(status);
        MDataHandle meshHandle = meshArray.inputValue(&status);
        DOMMY_RETURN_IF_FAILURE(status);
        const MObject meshObject = meshHandle.data();
        if (meshObject.isNull()) continue;
        if (meshHandle.type() != MFnData::kMesh) return MS::kInvalidParameter;

        MeshInput input;
        input.meshObject = meshObject;
        const auto matrix = std::lower_bound(
            matrixElements.begin(), matrixElements.end(), MatrixElement(logicalIndex, 0U));
        if (matrix != matrixElements.end() && matrix->first == logicalIndex) {
            DOMMY_RETURN_IF_FAILURE(matrixArray.jumpToArrayElement(matrix->second));
            const MDataHandle handle = matrixArray.inputValue(&status);
            DOMMY_RETURN_IF_FAILURE(status);
            const MMatrix value = handle.asMatrix();
            if (!dommy::maya::isFiniteMatrix(value)) return MS::kInvalidParameter;
            input.worldMatrix = value;
            input.hasWorldMatrix = true;
        } else {
            DOMMY_RETURN_IF_FAILURE(getLegacyWorldMatrix(
                node, meshAttribute, logicalIndex, input.worldMatrix, input.hasWorldMatrix));
        }
        candidate.push_back(input);
    }
    inputs.swap(candidate);
    return MS::kSuccess;
}

MStatus applyMatrix(MPointArray& points, const MMatrix& matrix)
{
    if (!dommy::maya::isFiniteMatrix(matrix)) {
        return MS::kInvalidParameter;
    }
    for (unsigned int index = 0U; index < points.length(); ++index) {
        points[index] *= matrix;
        if (!dommy::maya::isFinitePoint(points[index])) {
            return MS::kFailure;
        }
    }
    return MS::kSuccess;
}

MStatus polygonNormal(const MPointArray& points, MVector& result)
{
    if (points.length() < 3U) return MS::kInvalidParameter;

    // Work around a local origin at unit scale. Newell's unnormalized normal
    // has area units: applying an absolute length epsilon or float range check
    // to that vector incorrectly discards small and large nondegenerate faces.
    const MPoint& origin = points[0U];
    long double scale = 0.0L;
    for (unsigned int index = 0U; index < points.length(); ++index) {
        const MPoint& point = points[index];
        if (!dommy::maya::isFloatRepresentablePoint(point)) return MS::kInvalidParameter;
        scale = std::max(scale, std::fabs(static_cast<long double>(point.x) - origin.x));
        scale = std::max(scale, std::fabs(static_cast<long double>(point.y) - origin.y));
        scale = std::max(scale, std::fabs(static_cast<long double>(point.z) - origin.z));
    }
    if (scale == 0.0L) {
        result = MVector::zero;
        return MS::kSuccess;
    }
    if (!std::isfinite(scale)) return MS::kInvalidParameter;

    long double normal[3] = {}, correction[3] = {};
    for (unsigned int index = 0U; index < points.length(); ++index) {
        const MPoint& current = points[index];
        const MPoint& next = points[(index + 1U) % points.length()];
        const long double a[] = {
            (static_cast<long double>(current.x) - origin.x) / scale,
            (static_cast<long double>(current.y) - origin.y) / scale,
            (static_cast<long double>(current.z) - origin.z) / scale};
        const long double b[] = {
            (static_cast<long double>(next.x) - origin.x) / scale,
            (static_cast<long double>(next.y) - origin.y) / scale,
            (static_cast<long double>(next.z) - origin.z) / scale};
        for (unsigned int axis = 0U; axis < 3U; ++axis) {
            const unsigned int j = (axis + 1U) % 3U, k = (axis + 2U) % 3U;
            const long double term = (a[j] - b[j]) * (a[k] + b[k]) - correction[axis];
            const long double sum = normal[axis] + term;
            correction[axis] = (sum - normal[axis]) - term;
            normal[axis] = sum;
        }
    }
    const long double maximum = std::max(std::fabs(normal[0]),
        std::max(std::fabs(normal[1]), std::fabs(normal[2])));
    if (!std::isfinite(maximum)) return MS::kFailure;
    if (maximum == 0.0L) {
        result = MVector::zero; // Preserve the zero-area/cancelling-face policy.
        return MS::kSuccess;
    }
    for (unsigned int axis = 0U; axis < 3U; ++axis) normal[axis] /= maximum;
    const long double length = std::sqrt(
        normal[0] * normal[0] + normal[1] * normal[1] + normal[2] * normal[2]);
    const MVector candidate(static_cast<double>(normal[0] / length),
                            static_cast<double>(normal[1] / length),
                            static_cast<double>(normal[2] / length));
    if (!dommy::maya::isFiniteVector(candidate)) return MS::kFailure;
    result = candidate;
    return MS::kSuccess;
}

MStatus createFullMeshItem(const MeshInput& input,
                           bool bakeTransform,
                           DMItem& item)
{
    DMItem candidate;
    DOMMY_RETURN_IF_FAILURE(DMeshInternalData::createFromMayaObject(
        input.meshObject, candidate, MSpace::kObject,
        DMeshNormalImportPolicy::PreserveFaceVertex));
    if (input.hasWorldMatrix) {
        if (bakeTransform) {
            DOMMY_RETURN_IF_FAILURE(candidate->bakeTransform(input.worldMatrix));
        } else {
            candidate->setTransformMatrix(input.worldMatrix);
        }
    }
    item.swap(candidate);
    return MS::kSuccess;
}

MStatus validateInputBudget(std::vector<MeshInput>& inputs,
                            DMConvertMeshType convertType,
                            std::size_t& outputMeshCount)
{
    if (inputs.size() >
        dommy::generation::kMaxConnectedGeometryCount) {
        return MS::kFailure;
    }

    dommy::mesh::MeshMergePlan total;
    for (std::vector<MeshInput>::iterator input = inputs.begin();
         input != inputs.end(); ++input) {
        MStatus status;
        MFnMesh mesh(input->meshObject, &status);
        if (!status) return status;

        DOMMY_RETURN_IF_FAILURE(dommy::maya::preflightMeshImport(mesh, input->importPlan));
        const auto& plan = input->importPlan;
        const bool separate = convertType == kConvertPerPolygon;
        // Separate faces duplicate both positions and any assigned UVs per corner.
        if (!dommy::mesh::appendMeshMergePlan(
                separate ? plan.connections : plan.points, plan.faces, plan.connections,
                separate && plan.uvs != 0U ? plan.connections : plan.uvs, total))
            return MS::kInvalidParameter;
        if (separate) total.meshes = total.faces;
        if (total.meshes > dommy::generation::kMaxGeneratedMeshCount ||
            !dommy::mesh::validMeshOperationStorage(total)) return MS::kInvalidParameter;
    }
    outputMeshCount = total.meshes;
    return MS::kSuccess;
}

} // namespace


const MTypeId DMConvertFromMeshes::typeId(ID_DM_CONVERT_FROM_MESH);

MObject DMConvertFromMeshes::inMeshes;
MObject DMConvertFromMeshes::aInMatrices;
MObject DMConvertFromMeshes::aConvertType;
MObject DMConvertFromMeshes::aAddFaceNormalAttribute;
MObject DMConvertFromMeshes::aNormalAttributeName;
MObject DMConvertFromMeshes::aCurrentTime;
MObject DMConvertFromMeshes::outDM;


void* DMConvertFromMeshes::creator()
{
    return ::dommy::maya::createObject("DMConvertFromMeshes", [&]() {
        return new DMConvertFromMeshes();
    });
}


MStatus DMConvertFromMeshes::compute(
    const MPlug& plug, MDataBlock& data) try
{
    if (plug != outDM) return MS::kUnknownParameter;

    MStatus status;
    const MDataHandle timeHandle =
        data.inputValue(aCurrentTime, &status);
    if (!status) return status;
    (void)timeHandle.asTime();

    const MDataHandle convertTypeHandle =
        data.inputValue(aConvertType, &status);
    if (!status) return status;
    const DMConvertMeshType convertType =
        static_cast<DMConvertMeshType>(convertTypeHandle.asShort());
    if (convertType != kConvertFull &&
        convertType != kConvertFullWithTransform &&
        convertType != kConvertPerPolygon) {
        MGlobal::displayError(
            "DMConvertFromMeshes: unsupported conversion type.");
        return MS::kInvalidParameter;
    }

    const MDataHandle addNormalHandle =
        data.inputValue(aAddFaceNormalAttribute, &status);
    if (!status) return status;
    const bool addFaceNormal =
        addNormalHandle.asBool() && convertType == kConvertPerPolygon;

    const MDataHandle normalNameHandle =
        data.inputValue(aNormalAttributeName, &status);
    if (!status) return status;
    const std::string normalAttributeName =
        normalNameHandle.asString().asChar();

    std::vector<MeshInput> inputs;
    DOMMY_RETURN_IF_FAILURE(collectMeshInputs(
        thisMObject(), data, inMeshes, aInMatrices, inputs));

    if (inputs.empty()) {
        MGlobal::displayWarning(
            "DMConvertFromMeshes: inMeshes contains no mesh data.");
        return MS::kInvalidParameter;
    }

    std::size_t outputMeshCount = 0U;
    status = validateInputBudget(inputs, convertType, outputMeshCount);
    if (!status) {
        MGlobal::displayError(
            "DMConvertFromMeshes: output exceeds the safety budget.");
        return status;
    }

    MObject outputObject;
    DMeshData* outputMeshes = NULL;
    status = dommy::maya::createPluginDataObject<DMeshData>(
        DMeshData::id, outputObject, outputMeshes);
    if (!status || !outputMeshes) {
        return status ? MStatus(MS::kFailure) : status;
    }

    DMeshData::DMeshList generatedMeshes;
    generatedMeshes.reserve(outputMeshCount);
    DVec3DataList faceNormals;
    const bool generateFaceNormals = convertType == kConvertPerPolygon &&
        addFaceNormal && !normalAttributeName.empty();
    if (generateFaceNormals) faceNormals.reserve(outputMeshCount);
    if (convertType == kConvertFull ||
        convertType == kConvertFullWithTransform) {
        const bool bakeTransform = convertType == kConvertFull;
        for (std::vector<MeshInput>::const_iterator input = inputs.begin();
             input != inputs.end(); ++input) {
            DMItem item;
            status = createFullMeshItem(*input, bakeTransform, item);
            if (!status) return status;
            generatedMeshes.push_back(item);
        }
    } else {
        for (std::vector<MeshInput>::const_iterator input = inputs.begin();
             input != inputs.end(); ++input) {
            MItMeshPolygon iterator(input->meshObject, &status);
            if (!status) return status;

            std::size_t remainingCorners = input->importPlan.connections;
            status = dommy::maya::visitMeshPolygons(
                iterator, input->importPlan.faces,
                [&](MItMeshPolygon& polygonIterator) -> MStatus {

                MPointArray points;
                polygonIterator.getPoints(points, MSpace::kObject, &status);
                if (!status) return status;
                if (points.length() < 3U) return MS::kInvalidParameter;
                if (points.length() > remainingCorners) return MS::kFailure;
                remainingCorners -= points.length();
                if (input->hasWorldMatrix) {
                    status = applyMatrix(points, input->worldMatrix);
                    if (!status) return status;
                }

                MIntArray faceCounts;
                status = faceCounts.append(
                    static_cast<int>(points.length()));
                if (!status) return status;

                MIntArray connections;
                for (unsigned int vertex = 0U;
                     vertex < points.length(); ++vertex) {
                    status = connections.append(
                        static_cast<int>(vertex));
                    if (!status) return status;
                }

                DMItem item(new DMeshInternalData(
                    points, faceCounts, connections));

                const bool hasUVs = polygonIterator.hasUVs(&status);
                if (!status) return status;
                if (hasUVs && input->importPlan.uvs == 0U) return MS::kFailure;
                if (hasUVs) {
                    MFloatArray uValues;
                    MFloatArray vValues;
                    MIntArray uvCounts;
                    MIntArray uvConnections;
                    status = uvCounts.append(
                        static_cast<int>(points.length()));
                    if (!status) return status;

                    for (unsigned int vertex = 0U;
                         vertex < points.length(); ++vertex) {
                        int uvIndex = 0;
                        float u = 0.0f;
                        float v = 0.0f;
                        status = polygonIterator.getUVIndex(
                            static_cast<int>(vertex), uvIndex,
                            u, v);
                        if (!status) return status;
                        status = uvConnections.append(
                            static_cast<int>(vertex));
                        if (!status) return status;
                        status = uValues.append(u);
                        if (!status) return status;
                        status = vValues.append(v);
                        if (!status) return status;
                    }
                    if (!item->setUvData(
                            uValues, vValues, uvCounts, uvConnections)) {
                        return MS::kFailure;
                    }
                }

                if (!item->hasValidPolygonTopology()) {
                    return MS::kFailure;
                }
                generatedMeshes.push_back(item);

                if (generateFaceNormals) {
                    MVector normal;
                    DOMMY_RETURN_IF_FAILURE(polygonNormal(points, normal));
                    faceNormals.push_back(DVec3Data(
                        normal.x, normal.y, normal.z));
                }
                return MS::kSuccess;
            });
            if (!status) return status;
            if (remainingCorners != 0U) return MS::kFailure;
        }

    }

    if (generatedMeshes.size() != outputMeshCount ||
        !outputMeshes->insert(generatedMeshes)) {
        return MS::kFailure;
    }
    if (generateFaceNormals) {
        if (faceNormals.size() != outputMeshes->numberOfMesh()) {
            return MS::kFailure;
        }
        outputMeshes->addDPAttributeValues(normalAttributeName, faceNormals);
    }

    if (!outputMeshes->hasValidMeshTopology()) {
        MGlobal::displayError(
            "DMConvertFromMeshes: generated invalid DMesh topology.");
        return MS::kFailure;
    }
    outputMeshes->updateBBox();

    MDataHandle outputHandle = data.outputValue(outDM, &status);
    if (!status) return status;
    status = dommy::maya::publishPluginDataObject(
        outputHandle, outputObject, *outputMeshes);
    if (!status) return status;
    outputHandle.setClean();
    return data.setClean(plug);
}
DOMMY_COMPUTE_CATCH("DMConvertFromMeshes")


MStatus DMConvertFromMeshes::initialize() try
{
    MStatus status;
    MFnGenericAttribute genericAttribute;
    MFnNumericAttribute numericAttribute;
    MFnTypedAttribute typedAttribute;
    MFnStringData stringData;

    inMeshes = genericAttribute.create(
        "inMeshes", "inms", &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(
        genericAttribute.addDataAccept(MFnData::kMesh));
    DOMMY_RETURN_IF_FAILURE(genericAttribute.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(genericAttribute.setWritable(true));
    DOMMY_RETURN_IF_FAILURE(genericAttribute.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(genericAttribute.setArray(true));
    DOMMY_RETURN_IF_FAILURE(genericAttribute.setUsesArrayDataBuilder(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inMeshes));

    MFnMatrixAttribute matrixAttribute;
    aInMatrices = matrixAttribute.create(
        "inMatrices", "inmats", MFnMatrixAttribute::kDouble, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(matrixAttribute.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(matrixAttribute.setWritable(true));
    DOMMY_RETURN_IF_FAILURE(matrixAttribute.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(matrixAttribute.setArray(true));
    DOMMY_RETURN_IF_FAILURE(matrixAttribute.setUsesArrayDataBuilder(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aInMatrices));

    MFnEnumAttribute convertTypeAttribute;
    aConvertType = convertTypeAttribute.create(
        "ConvertType", "cnvty", kConvertFull, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(
        convertTypeAttribute.addField("Full Mesh", kConvertFull));
    DOMMY_RETURN_IF_FAILURE(convertTypeAttribute.addField(
        "Full Mesh with Transform", kConvertFullWithTransform));
    DOMMY_RETURN_IF_FAILURE(convertTypeAttribute.addField(
        "Per Polygon", kConvertPerPolygon));
    DOMMY_RETURN_IF_FAILURE(convertTypeAttribute.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aConvertType));

    aAddFaceNormalAttribute = numericAttribute.create(
        "AddFaceNormalAttribute", "afnattr",
        MFnNumericData::kBoolean, false, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(
        addAttribute(aAddFaceNormalAttribute));

    const MObject defaultNormalName = stringData.create("N", &status);
    DOMMY_RETURN_IF_FAILURE(status);
    aNormalAttributeName = typedAttribute.create(
        "NormalAttributeName", "nattrnm", MFnData::kString,
        defaultNormalName, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(typedAttribute.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aNormalAttributeName));

    MFnUnitAttribute unitAttribute;
    aCurrentTime = unitAttribute.create(
        "CurrentTime", "time", MFnUnitAttribute::kTime, 0.0, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(addAttribute(aCurrentTime));

    outDM = typedAttribute.create(
        "outDM", "odm", DMeshData::id, MObject::kNullObj, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(typedAttribute.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(typedAttribute.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(outDM));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(inMeshes, outDM));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aInMatrices, outDM));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aConvertType, outDM));
    DOMMY_RETURN_IF_FAILURE(
        attributeAffects(aAddFaceNormalAttribute, outDM));
    DOMMY_RETURN_IF_FAILURE(
        attributeAffects(aNormalAttributeName, outDM));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aCurrentTime, outDM));

    return MS::kSuccess;
} catch (...) {
    return dommy::maya::reportEntryPointException(
        "DMConvertFromMeshes", "initialize", std::current_exception());
}
