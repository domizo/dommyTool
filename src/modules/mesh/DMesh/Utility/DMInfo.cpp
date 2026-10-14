//
//  DMInfo.cpp
//  dommyTool
//
//  Created by yuzo on 2018/04/17.
//

#include "common/utility/DMayaEntryPointGuard.h"
#include "DMInfo.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DMayaPluginDataOutput.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DMeshDeformationInfo.h"



#include "DMayaHeaders.h"


#include "DMeshDataHeaders.h"

#include <cmath>
#include <vector>


const MTypeId DMInfo::typeId( ID_DM_INFO );


MObject DMInfo::inDM;
MObject DMInfo::targetDM;
MObject DMInfo::aVectorScale;
MObject DMInfo::aNormalize;
MObject DMInfo::outDM;




void *DMInfo::creator()
{
    return ::dommy::maya::createObject("DMInfo", [&]() {
        return new DMInfo();
    });
}


namespace {

MStatus readWorldFaceAreas(const DMeshInternalData& mesh, MObject& object,
                          std::vector<double>& areas)
{
    DOMMY_RETURN_IF_FAILURE(generateMMeshFromDmesh(mesh, object, DMeshPointSpace::Transformed));
    const std::size_t count = static_cast<std::size_t>(mesh.numFace());
    if (!dommy::attribute::validArrayConversionBudget(count, sizeof(double)))
        return MS::kInvalidParameter;
    areas.assign(count, 0.0);
    MStatus status;
    MItMeshPolygon faces(object, &status);
    if (!status) return status;
    std::size_t visited = 0U;
    for (;;) {
        const bool done = faces.isDone(&status);
        if (!status) return status;
        if (done) break;
        const int index = faces.index(&status);
        if (!status) return status;
        if (index < 0 || static_cast<std::size_t>(index) >= count || ++visited > count)
            return MS::kFailure;
        double area = 0.0;
        DOMMY_RETURN_IF_FAILURE(faces.getArea(area));
        if (!std::isfinite(area) || area < 0.0) return MS::kInvalidParameter;
        areas[static_cast<std::size_t>(index)] = area;
        DOMMY_RETURN_IF_FAILURE(faces.next());
    }
    return visited == count ? MS::kSuccess : MS::kFailure;
}

} // namespace

MStatus DMInfo::compute(const MPlug& plug, MDataBlock& data) try
{
    if (plug != outDM) return MS::kUnknownParameter;
    const DMeshData* base = nullptr;
    const DMeshData* target = nullptr;
    DOMMY_RETURN_IF_FAILURE(getValidInputDM(inDM, data, base));
    DOMMY_RETURN_IF_FAILURE(getValidInputDM(targetDM, data, target));
    MStatus status;
    const MDataHandle vectorScaleHandle = data.inputValue(aVectorScale, &status);
    if (!status) return status;
    const double vectorScale = vectorScaleHandle.asDouble();
    if (!std::isfinite(vectorScale)) return MS::kInvalidParameter;
    const MDataHandle normalizeHandle = data.inputValue(aNormalize, &status);
    if (!status) return status;
    const bool normalize = normalizeHandle.asBool();

    DOMMY_RETURN_IF_FAILURE(dommy::maya::setPluginDataOutput<DMeshData>(
        data, outDM, DMeshData::id, [&](DMeshData& output) -> MStatus {
            DOMMY_RETURN_IF_FAILURE(output.copyFrom(*target));
            // Preserve historical pass-through when primitive counts differ.
            if (base->numberOfMesh() != target->numberOfMesh()) return MS::kSuccess;
            std::size_t index = 0U, areaWork = 0U;
            for (auto it = output.beginMeshes(); it != output.endMeshes(); ++it, ++index) {
                const auto from = base->getMeshPtrConst(index);
                const auto to = target->getMeshPtrConst(index);
                if (!from || !to || !*it) return MS::kFailure;
                const std::size_t count = to->numberOfPoints();
                if (!dommy::attribute::validArrayConversionBudget(
                        count, sizeof(double) + 2U * sizeof(Imath::V3f)))
                    return MS::kInvalidParameter;
                dommy::mesh::DeformationInfo info;
                info.distances.assign(count, 0.0);
                info.vectors.assign(count, Imath::V3f(0.0f));
                std::vector<Imath::V3f> areaValues(count, Imath::V3f(0.0f));
                // Vertex-count mismatch retains the legacy zero-field result.
                if (from->numberOfPoints() == count) {
                    if (!dommy::mesh::buildDeformationInfo(
                            *from->getPositionPtrConst(), from->getTransformMatrix(),
                            *to->getPositionPtrConst(), to->getTransformMatrix(),
                            vectorScale, normalize, info)) return MS::kInvalidParameter;
                    if (count != 0U && from->numFace() == to->numFace() && from->numFace() != 0U) {
                        MObject baseObject, targetObject;
                        std::vector<double> baseAreas, targetAreas;
                        DOMMY_RETURN_IF_FAILURE(readWorldFaceAreas(*from, baseObject, baseAreas));
                        DOMMY_RETURN_IF_FAILURE(readWorldFaceAreas(*to, targetObject, targetAreas));
                        MStatus vertexStatus;
                        MItMeshVertex vertex(baseObject, &vertexStatus);
                        if (!vertexStatus) return vertexStatus;
                        std::size_t visited = 0U;
                        for (;;) {
                            const bool done = vertex.isDone(&vertexStatus);
                            if (!vertexStatus) return vertexStatus;
                            if (done) break;
                            const int vertexIndex = vertex.index(&vertexStatus);
                            if (!vertexStatus) return vertexStatus;
                            if (vertexIndex < 0 || static_cast<std::size_t>(vertexIndex) >= count ||
                                ++visited > count) return MS::kFailure;
                            MIntArray faces;
                            DOMMY_RETURN_IF_FAILURE(vertex.getConnectedFaces(faces));
                            if (!dommy::generation::checkedSum(areaWork, faces.length(),
                                    dommy::generation::kMaxWorkItemCount, areaWork) ||
                                !dommy::mesh::vertexAreaChange(baseAreas, targetAreas,
                                    faces, faces.length(), vectorScale,
                                    areaValues[static_cast<std::size_t>(vertexIndex)]))
                                return MS::kInvalidParameter;
                            DOMMY_RETURN_IF_FAILURE(vertex.next());
                        }
                        if (visited != count) return MS::kFailure;
                    }
                }
                if (!dommy::mesh::publishDeformationInfo(**it, info, areaValues, count))
                    return MS::kFailure;
            }
            return MS::kSuccess;
        }));
    return data.setClean(plug);
}
DOMMY_COMPUTE_CATCH("DMInfo")

MStatus DMInfo::initialize() try {
    MStatus stat;
    MFnNumericAttribute nAttr;
    MFnTypedAttribute tAttr;

    inDM = tAttr.create("inDM", "idm", DMeshData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inDM));

    targetDM = tAttr.create("targetDM", "tdm", DMeshData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(targetDM));

    aVectorScale = nAttr.create("VectorScale", "vscl", MFnNumericData::kDouble, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(5.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aVectorScale));

    aNormalize = nAttr.create("Normalize", "nrm", MFnNumericData::kBoolean, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aNormalize));

    outDM = tAttr.create("outDM", "odm", DMeshData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(outDM));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(inDM, outDM));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(targetDM, outDM));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(aVectorScale, outDM));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aNormalize, outDM));

    return MS::kSuccess;
} catch (...) {
    return dommy::maya::reportEntryPointException("DMInfo", "initialize", std::current_exception());
}
