//
//  DMCharactor.cpp
//  dommyTool
//
//  Created by yuzo on 4/28/17.
//

#include "common/utility/DMayaEntryPointGuard.h"
#include "DMCharactor.h"
#include "common/utility/DStatusOperation.h"

#include "common/utility/DComputeGuard.h"
#include "common/utility/DGenerationLimits.h"
#include "common/utility/DMayaPluginDataOutput.h"
#include "common/utility/DMayaValueValidation.h"
#include "common/utility/DMayaMeshTraversal.h"
#include "common/utility/DAttributePublication.h"

#include "DMayaHeaders.h"
#include "DMeshDataHeaders.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <string>


namespace {

const double kPi = 3.14159265358979323846;

} // namespace


const MTypeId DMCharactor::typeId(ID_DM_CHARACTOR);

MObject DMCharactor::inDM;

MObject DMCharactor::aAddCenter;
MObject DMCharactor::aCenterAttributeName;

MObject DMCharactor::aAddCurvature;
MObject DMCharactor::aCurvatureAttributeName;
MObject DMCharactor::aColorMap;
MObject DMCharactor::aSmoothIterate;
MObject DMCharactor::aCurvatureOutputType;

MObject DMCharactor::outDM;


void* DMCharactor::creator()
{
    return ::dommy::maya::createObject("DMCharactor", [&]() {
        return new DMCharactor();
    });
}


MStatus DMCharactor::compute(const MPlug& plug, MDataBlock& data) try
{
    if (plug != outDM) return MS::kUnknownParameter;

    MStatus status;
    const DMeshData* inputMeshes = nullptr;
    DOMMY_RETURN_IF_FAILURE(getValidInputDM(inDM, data, inputMeshes));
    if (!inputMeshes) return MS::kInvalidParameter;

    const MDataHandle addCenterHandle = data.inputValue(aAddCenter, &status);
    if (!status) return status;
    const bool addCenter = addCenterHandle.asBool();

    const MDataHandle centerNameHandle =
        data.inputValue(aCenterAttributeName, &status);
    if (!status) return status;
    const std::string centerAttributeName =
        centerNameHandle.asString().asChar();

    const MDataHandle addCurvatureHandle =
        data.inputValue(aAddCurvature, &status);
    if (!status) return status;
    const bool addCurvature = addCurvatureHandle.asBool();

    const MDataHandle curvatureNameHandle =
        data.inputValue(aCurvatureAttributeName, &status);
    if (!status) return status;
    const std::string curvatureAttributeName =
        curvatureNameHandle.asString().asChar();

    const MDataHandle smoothingHandle =
        data.inputValue(aSmoothIterate, &status);
    if (!status) return status;
    const int rawSmoothingIterations = smoothingHandle.asInt();
    if (rawSmoothingIterations < 0) {
        MGlobal::displayError(
            "DMCharactor: smoothing iterations cannot be negative.");
        return MS::kInvalidParameter;
    }
    const std::size_t smoothingIterations =
        static_cast<std::size_t>(rawSmoothingIterations);

    const MDataHandle outputTypeHandle =
        data.inputValue(aCurvatureOutputType, &status);
    if (!status) return status;
    const short outputType = outputTypeHandle.asShort();
    if (outputType != 0 && outputType != 1) {
        MGlobal::displayError(
            "DMCharactor: unsupported curvature output type.");
        return MS::kInvalidParameter;
    }

    std::size_t totalVertexCount = 0U;
    if (!inputMeshes->getCheckedTotalVertex(totalVertexCount) ||
        totalVertexCount > dommy::generation::kMaxGeneratedPointCount) {
        MGlobal::displayError(
            "DMCharactor: vertex count exceeds the safety budget.");
        return MS::kFailure;
    }

    if (addCurvature && !curvatureAttributeName.empty()) {
        std::size_t smoothingWorkItems = 0U;
        if (!dommy::generation::checkedProduct(
                totalVertexCount, smoothingIterations,
                dommy::generation::kMaxWorkItemCount,
                smoothingWorkItems)) {
            MGlobal::displayError(
                "DMCharactor: smoothing work exceeds the safety budget.");
            return MS::kFailure;
        }
    }

    MObject outputObject;
    DMeshData* outputMeshes = NULL;
    status = dommy::maya::createPluginDataObject<DMeshData>(
        DMeshData::id, outputObject, outputMeshes);
    if (!status || !outputMeshes) {
        return status ? MStatus(MS::kFailure) : status;
    }
    DOMMY_RETURN_IF_FAILURE(outputMeshes->copyFrom(*inputMeshes));

    const std::size_t meshCount = outputMeshes->numberOfMesh();
    if (meshCount > 0U && addCenter && !centerAttributeName.empty()) {
        DVec3DataList centers(
            meshCount, DVec3Data(0.0, 0.0, 0.0));
        status = DMGeneratePrimitiveCenterList(*outputMeshes, centers);
        if (!status) return status;
        for (DVec3DataList::const_iterator center = centers.begin();
             center != centers.end(); ++center) {
            if (!std::isfinite((*center)[0]) ||
                !std::isfinite((*center)[1]) ||
                !std::isfinite((*center)[2])) {
                MGlobal::displayError(
                    "DMCharactor: generated a non-finite mesh center.");
                return MS::kFailure;
            }
        }
        if (!dommy::attribute::commitStagedAttribute(*outputMeshes, centerAttributeName,
                DPAttributeData::Ptr(new DPVec3fAttr(centers)), meshCount)) return MS::kFailure;
    }

    if (meshCount > 0U &&
        addCurvature && !curvatureAttributeName.empty()) {
        checkDMeshPointAttribute(
            curvatureAttributeName, *outputMeshes, true);

        MRampAttribute colorRamp;
        if (outputType == 0) {
            MPlug colorRampPlug(thisMObject(), aColorMap);
            colorRamp = MRampAttribute(colorRampPlug, &status);
            if (!status) return status;
        }

        for (DMeshData::DMeshIter meshIterator = outputMeshes->beginMeshes();
             meshIterator != outputMeshes->endMeshes(); ++meshIterator) {
            if (!meshIterator->get()) return MS::kFailure;

            MObject mayaMeshObject;
            status = generateMMeshFromDmeshWithUV(
                *meshIterator->get(), mayaMeshObject);
            if (!status) return status;

            MFnMesh meshFunction(mayaMeshObject, &status);
            if (!status) return status;
            const int rawMeshVertexCount =
                meshFunction.numVertices(&status);
            if (!status || rawMeshVertexCount < 0) {
                return status ? MStatus(MS::kFailure) : status;
            }
            const std::size_t meshVertexCount =
                static_cast<std::size_t>(rawMeshVertexCount);

            const DPointList* points =
                (*meshIterator)->getPositionPtrConst();
            if (!points || points->size() != meshVertexCount) {
                return MS::kFailure;
            }

            MItMeshVertex vertexIterator(mayaMeshObject, &status);
            if (!status) return status;
            MItMeshPolygon polygonIterator(mayaMeshObject, &status);
            if (!status) return status;

            MVectorArray meanNormals;
            DOMMY_RETURN_IF_FAILURE(meanNormals.setLength(static_cast<unsigned int>(meshVertexCount)));
            if (meanNormals.length() != meshVertexCount) return MS::kFailure;
            for (unsigned int index = 0U; index < meanNormals.length(); ++index)
                meanNormals[index] = MVector::zero;
            status = dommy::maya::visitMeshElements(vertexIterator, meshVertexCount,
                [&](MItMeshVertex&) -> MStatus {

                const int vertexIndex = vertexIterator.index(&status);
                DOMMY_RETURN_IF_FAILURE(status);
                if (vertexIndex < 0 ||
                    static_cast<std::size_t>(vertexIndex) >=
                        meshVertexCount) {
                    return MS::kFailure;
                }

                MIntArray faceIds;
                status = vertexIterator.getConnectedFaces(faceIds);
                if (!status) return status;

                MVector normalSum = MVector::zero;
                std::size_t validNormalCount = 0U;
                for (unsigned int face = 0U;
                     face < faceIds.length(); ++face) {
                    if (faceIds[face] < 0) return MS::kFailure;
                    int previousFaceId = 0;
                    status = polygonIterator.setIndex(
                        faceIds[face], previousFaceId);
                    if (!status) return status;

                    MVector faceNormal;
                    status = polygonIterator.getNormal(
                        faceNormal, MSpace::kObject);
                    if (!status) return status;
                    if (!dommy::maya::isFiniteVector(faceNormal)) {
                        return MS::kFailure;
                    }
                    normalSum += faceNormal;
                    ++validNormalCount;
                }

                MVector meanNormal;
                if (validNormalCount > 0U &&
                    dommy::maya::normalizedVector(
                        normalSum, meanNormal)) {
                    meanNormals[
                        static_cast<unsigned int>(vertexIndex)] =
                            meanNormal;
                }
                return MS::kSuccess;
            });
            if (!status) return status;

            DDoubleArray curvatureValues(meshVertexCount, 0.0);
            status = vertexIterator.reset();
            if (!status) return status;

            status = dommy::maya::visitMeshElements(vertexIterator, meshVertexCount,
                [&](MItMeshVertex&) -> MStatus {

                const int vertexIndex = vertexIterator.index(&status);
                DOMMY_RETURN_IF_FAILURE(status);
                if (vertexIndex < 0 ||
                    static_cast<std::size_t>(vertexIndex) >=
                        meshVertexCount) {
                    return MS::kFailure;
                }

                const MVector vertexNormal =
                    meanNormals[static_cast<unsigned int>(vertexIndex)];
                MVector normalizedNormal;
                if (!dommy::maya::normalizedVector(
                        vertexNormal, normalizedNormal)) {
                    return MS::kSuccess;
                }

                const DPoint& sourcePoint =
                    (*points)[static_cast<std::size_t>(vertexIndex)];
                const MPoint vertexPoint(
                    sourcePoint[0], sourcePoint[1], sourcePoint[2]);
                if (!dommy::maya::isFinitePoint(vertexPoint)) {
                    return MS::kFailure;
                }

                MIntArray faceIds;
                status = vertexIterator.getConnectedFaces(faceIds);
                if (!status) return status;

                double curvatureSum = 0.0;
                std::size_t validFaceCount = 0U;
                for (unsigned int face = 0U;
                     face < faceIds.length(); ++face) {
                    if (faceIds[face] < 0) return MS::kFailure;
                    int previousFaceId = 0;
                    status = polygonIterator.setIndex(
                        faceIds[face], previousFaceId);
                    if (!status) return status;

                    const MPoint faceCenter =
                        polygonIterator.center(MSpace::kObject, &status);
                    if (!status ||
                        !dommy::maya::isFinitePoint(faceCenter)) {
                        return status ? MStatus(MS::kFailure) : status;
                    }

                    MVector faceDirection;
                    if (!dommy::maya::normalizedVector(
                            faceCenter - vertexPoint, faceDirection)) {
                        continue;
                    }

                    const double angle =
                        normalizedNormal.angle(faceDirection);
                    if (!std::isfinite(angle)) continue;

                    const double curvature = std::max(
                        -1.0, std::min(1.0, (angle * 2.0 / kPi) - 1.0));
                    curvatureSum += curvature;
                    ++validFaceCount;
                }

                if (validFaceCount > 0U) {
                    curvatureValues[
                        static_cast<std::size_t>(vertexIndex)] =
                            curvatureSum /
                            static_cast<double>(validFaceCount);
                }
                return MS::kSuccess;
            });
            if (!status) return status;

            DDoubleArray finalCurvature(curvatureValues);
            for (std::size_t iteration = 0U;
                 iteration < smoothingIterations; ++iteration) {
                const DDoubleArray previousCurvature(finalCurvature);
                status = vertexIterator.reset();
                if (!status) return status;

                status = dommy::maya::visitMeshElements(vertexIterator, meshVertexCount,
                    [&](MItMeshVertex&) -> MStatus {

                    const int vertexIndex =
                        vertexIterator.index(&status);
                    DOMMY_RETURN_IF_FAILURE(status);
                    if (vertexIndex < 0 ||
                        static_cast<std::size_t>(vertexIndex) >=
                            meshVertexCount) {
                        return MS::kFailure;
                    }

                    MIntArray connectedVertices;
                    status = vertexIterator.getConnectedVertices(
                        connectedVertices);
                    if (!status) return status;

                    double curvatureSum = 0.0;
                    std::size_t validNeighbourCount = 0U;
                    for (unsigned int neighbour = 0U;
                         neighbour < connectedVertices.length();
                         ++neighbour) {
                        const int neighbourIndex =
                            connectedVertices[neighbour];
                        if (neighbourIndex < 0 ||
                            static_cast<std::size_t>(neighbourIndex) >=
                                meshVertexCount) {
                            return MS::kFailure;
                        }
                        const double neighbourValue =
                            previousCurvature[
                                static_cast<std::size_t>(
                                    neighbourIndex)];
                        if (!std::isfinite(neighbourValue)) continue;
                        curvatureSum += neighbourValue;
                        ++validNeighbourCount;
                    }

                    if (validNeighbourCount > 0U) {
                        finalCurvature[
                            static_cast<std::size_t>(vertexIndex)] =
                                curvatureSum /
                                static_cast<double>(
                                    validNeighbourCount);
                    }
                    return MS::kSuccess;
                });
                if (!status) return status;
            }

            for (DDoubleArray::const_iterator value =
                     finalCurvature.begin();
                 value != finalCurvature.end(); ++value) {
                if (!std::isfinite(*value)) return MS::kFailure;
            }

            if (outputType == 0) {
                DVec3DataList colors(
                    meshVertexCount, DVec3Data(0.0, 0.0, 0.0));
                for (std::size_t index = 0U;
                     index < finalCurvature.size(); ++index) {
                    const float rampPosition = static_cast<float>(
                        std::max(0.0, std::min(
                            1.0, (finalCurvature[index] + 1.0) * 0.5)));
                    MColor color;
                    colorRamp.getColorAtPosition(
                        rampPosition, color, &status);
                    if (!status || !dommy::maya::isFiniteColor(color)) {
                        return status ? MStatus(MS::kFailure) : status;
                    }
                    colors[index] =
                        DVec3Data(color.r, color.g, color.b);
                }
                (*meshIterator)->addAttrWithValues(
                    curvatureAttributeName, colors);
            } else {
                (*meshIterator)->addAttr(
                    curvatureAttributeName,
                    DPDoubleAttr(finalCurvature, 0.0));
            }
        }
    }

    if (!outputMeshes->hasValidMeshTopology()) return MS::kFailure;

    MDataHandle outputHandle = data.outputValue(outDM, &status);
    if (!status) return status;
    status = dommy::maya::publishPluginDataObject(
        outputHandle, outputObject, *outputMeshes);
    if (!status) return status;
    outputHandle.setClean();
    return data.setClean(plug);
}
DOMMY_COMPUTE_CATCH("DMCharactor")


MStatus DMCharactor::initialize() try
{
    MStatus status;
    MFnNumericAttribute numericAttribute;
    MFnTypedAttribute typedAttribute;
    MFnStringData stringData;

    inDM = typedAttribute.create(
        "inDM", "idm", DMeshData::id, MObject::kNullObj, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    DOMMY_RETURN_IF_FAILURE(typedAttribute.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(typedAttribute.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inDM));

    aAddCenter = numericAttribute.create(
        "AddCenter", "adcntattr", MFnNumericData::kBoolean, false, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aAddCenter));

    const MObject centerDefault = stringData.create("center", &status);
    DOMMY_RETURN_IF_FAILURE(status);
    aCenterAttributeName = typedAttribute.create(
        "CenterAttributeName", "antattrnm", MFnData::kString,
        centerDefault, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    DOMMY_RETURN_IF_FAILURE(typedAttribute.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aCenterAttributeName));

    aAddCurvature = numericAttribute.create(
        "AddCurvature", "adcrvattr", MFnNumericData::kBoolean, false,
        &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aAddCurvature));

    const MObject curvatureDefault = stringData.create("curvature", &status);
    DOMMY_RETURN_IF_FAILURE(status);
    aCurvatureAttributeName = typedAttribute.create(
        "CurvatureAttributeName", "crvattrnm", MFnData::kString,
        curvatureDefault, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    DOMMY_RETURN_IF_FAILURE(typedAttribute.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aCurvatureAttributeName));

    aColorMap = MRampAttribute::createColorRamp(
        "ColorMap", "cm", &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    DOMMY_RETURN_IF_FAILURE(addAttribute(aColorMap));

    aSmoothIterate = numericAttribute.create(
        "SmoothIterate", "sit", MFnNumericData::kInt, 0, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setMin(0));
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setSoftMax(5));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aSmoothIterate));

    MFnEnumAttribute enumAttribute;
    aCurvatureOutputType = enumAttribute.create(
        "CurvatureOutputType", "crvot", 0, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    DOMMY_RETURN_IF_FAILURE(enumAttribute.addField("Color Map", 0));
    DOMMY_RETURN_IF_FAILURE(enumAttribute.addField("Raw Value", 1));
    DOMMY_RETURN_IF_FAILURE(enumAttribute.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aCurvatureOutputType));

    outDM = typedAttribute.create(
        "outDM", "odm", DMeshData::id, MObject::kNullObj, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    DOMMY_RETURN_IF_FAILURE(typedAttribute.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(typedAttribute.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(outDM));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(inDM, outDM));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aAddCenter, outDM));
    DOMMY_RETURN_IF_FAILURE(
        attributeAffects(aCenterAttributeName, outDM));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aAddCurvature, outDM));
    DOMMY_RETURN_IF_FAILURE(
        attributeAffects(aCurvatureAttributeName, outDM));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aColorMap, outDM));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aSmoothIterate, outDM));
    DOMMY_RETURN_IF_FAILURE(
        attributeAffects(aCurvatureOutputType, outDM));

    return MS::kSuccess;
}
catch (...) {
    return dommy::maya::reportEntryPointException(
        "DMCharactor", "initialize", std::current_exception());
}
