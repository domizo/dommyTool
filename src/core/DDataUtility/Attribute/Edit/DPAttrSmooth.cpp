//
// DPAttrSmooth.cpp
// Attribute smoothing over the historical OpenVDB neighborhood selection.
//
#include "common/utility/DMayaEntryPointGuard.h"
#include "DPAttrSmooth.h"
#include "DMayaHeaders.h"
#include "DPointDataHeaders.h"
#include "DCurveDataHeaders.h"
#include "DMeshDataHeaders.h"
#include "DPointVdbSearch.h"
#include "DPoint/Common/DPointForceOutput.h"
#include "common/utility/DAttributeSmoothing.h"
#include "common/utility/DPointPayloadBudget.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DComputeGuard.h"
#include "../../../../modules/volume/VDB/Common/DVDBComputeGuard.h"

#include <tbb/parallel_for.h>
#include <cmath>
#include <stdexcept>

namespace {
std::size_t smoothingComponents(DP_ATTR_DATA_TYPE type) {
    switch (type) {
    case DP_ATTR_INT:
    case DP_ATTR_UINT32:
    case DP_ATTR_INT64:
    case DP_ATTR_FLOAT:
    case DP_ATTR_DOUBLE:
        return 1U;
    case DP_ATTR_VEC2I:
    case DP_ATTR_VEC2F:
    case DP_ATTR_VEC2D:
        return 2U;
    case DP_ATTR_VEC3I:
    case DP_ATTR_VEC3F:
    case DP_ATTR_VEC3D:
        return 3U;
    default:
        return 0U;
    }
}

struct SmoothingRequest {
    std::string name;
    double pointRadius, searchRadius, gridSize;
    std::size_t iterations, samples;
};

void preflightTopologyStorage(const DPData&, dommy::point::PointPayloadBudget&) {}
void preflightTopologyStorage(const DCurveInternalData& curve,
                              dommy::point::PointPayloadBudget& budget) {
    // Validated knot count is bounded by CV count plus degree plus one.
    budget.add(curve.numberOfPoints(), sizeof(double));
    budget.add(static_cast<std::size_t>(curve.getDegree()) + 1U, sizeof(double));
    budget.add(1U, sizeof(MMatrix));
}
void preflightTopologyStorage(const DMeshInternalData& mesh,
                              dommy::point::PointPayloadBudget& budget) {
    budget.add(static_cast<std::size_t>(mesh.numFace()), 2U * sizeof(int));
    budget.add(mesh.numberOfConnections(), 2U * sizeof(int));
    budget.add(mesh.numberOfUVs(), 2U * sizeof(float));
    budget.add(1U, sizeof(MMatrix));
    dommy::point::preflightAttributePayload(mesh.beginFaceAttr(), mesh.endFaceAttr(),
                                            mesh.numFace(), budget);
    dommy::point::preflightAttributePayload(mesh.beginPVtxAttr(), mesh.endPVtxAttr(),
                                            mesh.numberOfConnections(), budget);
    dommy::point::preflightGroupPayload(mesh.beginFaceGrp(), mesh.endFaceGrp(), budget);
    dommy::point::preflightGroupPayload(mesh.beginEdgeGrp(), mesh.endEdgeGrp(), budget);
}

template <typename Owner>
void preflightSmoothingPartition(const Owner& input, const SmoothingRequest& request,
                                 dommy::point::PointPayloadBudget& budget, std::size_t& work) {
    const std::size_t count = input.numberOfPoints();
    budget.add(count, sizeof(DPoint));
    dommy::point::preflightMetadataPayload(input, count, budget);
    preflightTopologyStorage(input, budget);
    const auto field = input.getAttr(request.name);
    const std::size_t components = field ? smoothingComponents(field->getAttrType()) : 0U;
    if (count == 0U || request.iterations == 0U || components == 0U)
        return;
    std::size_t neighbors = 0U;
    if (!dommy::attribute::addSmoothingWork(count, request.samples, request.iterations, components,
                                            work, neighbors))
        throw std::length_error("Attribute smoothing exceeds its work budget");
    budget.add(count, sizeof(openvdb::Vec3R) + sizeof(double) + sizeof(IDs));
    budget.add(neighbors, sizeof(std::uint32_t));
    budget.add(count, 2U * dommy::point::pointAttributeStorageBytes(field->getAttrType()));
}

template <typename Container>
void preflightSmoothingContainer(const Container& input, std::size_t count,
                                 const SmoothingRequest& request,
                                 dommy::point::PointPayloadBudget& budget, std::size_t& work) {
    budget.add(count, sizeof(void*) + sizeof(MMatrix));
    dommy::point::preflightMetadataPayload(input, count, budget);
    for (auto it = input.beginPrims(); it != input.endPrims(); ++it) {
        if (!*it)
            throw std::invalid_argument("Null smoothing primitive");
        preflightSmoothingPartition(**it, request, budget, work);
    }
}

struct ParallelSmoothingPass {
    template <typename Row> void operator()(std::size_t count, Row row) const {
        tbb::parallel_for(tbb::blocked_range<std::size_t>(0U, count, 10000U),
                          [&](const tbb::blocked_range<std::size_t>& range) {
                              for (std::size_t i = range.begin(); i < range.end(); ++i)
                                  row(i);
                          });
    }
};

template <typename T, typename Owner>
void smoothTypedAttribute(Owner& output, const SmoothingRequest& request,
                          const IDsListArray& neighbors) {
    const auto field =
        std::dynamic_pointer_cast<const TypedDPAttributeData<T>>(output.getAttr(request.name));
    if (!field || field->value().size() != output.numberOfPoints())
        throw std::invalid_argument("Invalid smoothing attribute");
    std::vector<T> values = field->value(), next;
    for (std::size_t iteration = 0U; iteration < request.iterations; ++iteration) {
        if (!dommy::attribute::smoothNeighborValues(values, neighbors, request.samples, next,
                                                    ParallelSmoothingPass()))
            throw std::invalid_argument("Invalid smoothing mean or neighbor");
        values.swap(next);
    }
    // Preserve the historical reset to a zero default. Same-type replacement
    // stages a copy before replacing the field, without an erase-first window.
    output.addAttr(request.name, TypedDPAttributeData<T>(
                                     values, dommy::attribute::SmoothingComponents<T>::zero()));
}

template <typename Owner> void smoothAttribute(Owner& output, const SmoothingRequest& request) {
    const auto field = output.getAttr(request.name);
    const std::size_t count = output.numberOfPoints();
    if (count == 0U || request.iterations == 0U || !field ||
        smoothingComponents(field->getAttrType()) == 0U)
        return;
    std::vector<openvdb::Vec3R> points;
    points.reserve(count);
    for (const auto& point : *output.getPositionPtrConst())
        points.push_back(openvdb::Vec3R(point[0], point[1], point[2]));
    std::vector<double> radii(count, request.pointRadius);
    DPointVdbParticleList particles(points, radii);
    ParticleAtlas atlas;
    atlas.construct(particles, request.gridSize);
    IDsListArray neighbors(count);
    PointSearchOp search(atlas, particles, request.samples, request.searchRadius, neighbors,
                         points);
    tbb::parallel_for(tbb::blocked_range<std::size_t>(0U, count, 10000U), search);
    switch (field->getAttrType()) {
    case DP_ATTR_INT:
        smoothTypedAttribute<int32_t>(output, request, neighbors);
        break;
    case DP_ATTR_UINT32:
        smoothTypedAttribute<uint32_t>(output, request, neighbors);
        break;
    case DP_ATTR_INT64:
        smoothTypedAttribute<int64_t>(output, request, neighbors);
        break;
    case DP_ATTR_FLOAT:
        smoothTypedAttribute<float>(output, request, neighbors);
        break;
    case DP_ATTR_DOUBLE:
        smoothTypedAttribute<double>(output, request, neighbors);
        break;
    case DP_ATTR_VEC2I:
        smoothTypedAttribute<Imath::V2i>(output, request, neighbors);
        break;
    case DP_ATTR_VEC2F:
        smoothTypedAttribute<Imath::V2f>(output, request, neighbors);
        break;
    case DP_ATTR_VEC2D:
        smoothTypedAttribute<Imath::V2d>(output, request, neighbors);
        break;
    case DP_ATTR_VEC3I:
        smoothTypedAttribute<Imath::V3i>(output, request, neighbors);
        break;
    case DP_ATTR_VEC3F:
        smoothTypedAttribute<Imath::V3f>(output, request, neighbors);
        break;
    case DP_ATTR_VEC3D:
        smoothTypedAttribute<Imath::V3d>(output, request, neighbors);
        break;
    default:
        break;
    }
}

template <typename Container>
MStatus smoothContainer(Container& output, const Container& input,
                        const SmoothingRequest& request) {
    DOMMY_RETURN_IF_FAILURE(output.copyFrom(input));
    for (auto it = output.beginPrims(); it != output.endPrims(); ++it)
        smoothAttribute(**it, request);
    return MS::kSuccess;
}
} // namespace

const MTypeId DPAttrSmooth::typeId(ID_DP_ATTR_SMOOTH);
MObject DPAttrSmooth::inDD;
MObject DPAttrSmooth::aSmoothAttribute;
MObject DPAttrSmooth::aPointRadius;
MObject DPAttrSmooth::aSearchRadius;
MObject DPAttrSmooth::aSearchGridSize;
MObject DPAttrSmooth::aIterate;
MObject DPAttrSmooth::aMaxSampleCount;
MObject DPAttrSmooth::outDD;
void* DPAttrSmooth::creator() {
    return ::dommy::maya::createObject("DPAttrSmooth", [&]() {
        return new DPAttrSmooth();
    });
}

MStatus DPAttrSmooth::compute(const MPlug& plug, MDataBlock& data) try {
    if (plug != outDD)
        return MS::kUnknownParameter;
    MStatus status;
    const MDataHandle inputHandle = data.inputValue(inDD, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    dommy::point::PointForceInput input;
    DOMMY_RETURN_IF_FAILURE(input.bind(inputHandle));
    SmoothingRequest request;
    MDataHandle handle = data.inputValue(aSmoothAttribute, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    request.name = handle.asString().asChar();
    handle = data.inputValue(aPointRadius, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    request.pointRadius = handle.asDouble();
    handle = data.inputValue(aSearchRadius, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    request.searchRadius = handle.asDouble();
    handle = data.inputValue(aSearchGridSize, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    request.gridSize = handle.asDouble();
    handle = data.inputValue(aIterate, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const int iterations = handle.asInt();
    handle = data.inputValue(aMaxSampleCount, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const int samples = handle.asInt();
    if (iterations < 0 || samples < 1 || !std::isfinite(request.pointRadius) ||
        request.pointRadius < 0.0 || !std::isfinite(request.searchRadius) ||
        request.searchRadius < 0.0 || !std::isfinite(request.gridSize) || request.gridSize <= 0.0)
        return MS::kInvalidParameter;
    request.iterations = static_cast<std::size_t>(iterations);
    request.samples = static_cast<std::size_t>(samples);

    dommy::point::PointPayloadBudget budget;
    budget.add(request.name.size(), sizeof(char));
    std::size_t work = 0U;
    if (input.points) {
        preflightSmoothingPartition(*input.points, request, budget, work);
        status =
            dommy::maya::setPluginDataOutput<DPData>(data, outDD, DPData::id, [&](DPData& output) {
                output.setDPData(*input.points);
                smoothAttribute(output, request);
                return MStatus(MS::kSuccess);
            });
    } else if (input.curves) {
        preflightSmoothingContainer(*input.curves, input.partitionCount, request, budget, work);
        status = dommy::maya::setPluginDataOutput<DCurveData>(
            data, outDD, DCurveData::id,
            [&](DCurveData& output) { return smoothContainer(output, *input.curves, request); });
    } else if (input.meshes) {
        preflightSmoothingContainer(*input.meshes, input.partitionCount, request, budget, work);
        status = dommy::maya::setPluginDataOutput<DMeshData>(
            data, outDD, DMeshData::id,
            [&](DMeshData& output) { return smoothContainer(output, *input.meshes, request); });
    } else
        return MS::kInvalidParameter;
    DOMMY_RETURN_IF_FAILURE(status);
    return data.setClean(plug);
}
DOMMY_VDB_COMPUTE_CATCH("DPAttrSmooth")

MStatus DPAttrSmooth::initialize() try {
    MStatus stat;
    MFnNumericAttribute nAttr;
    MFnGenericAttribute gAttr;
    MFnTypedAttribute tAttr;
    MFnStringData fnStringData;

    inDD = gAttr.create("inDD", "idd", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(gAttr.addAccept(DPData::id));
    DOMMY_RETURN_IF_FAILURE(gAttr.addAccept(DCurveData::id));
    DOMMY_RETURN_IF_FAILURE(gAttr.addAccept(DMeshData::id));
    DOMMY_RETURN_IF_FAILURE(gAttr.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(gAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inDD));
    const MObject defaultName = fnStringData.create("", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);

    aSmoothAttribute =
        tAttr.create("SmoothAttribute", "smatr", MFnData::kString, defaultName, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aSmoothAttribute));

    aPointRadius = nAttr.create("PointRadius", "prad", MFnNumericData::kDouble, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(50.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.001));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aPointRadius));

    aSearchRadius = nAttr.create("SearchRadius", "srhrad", MFnNumericData::kDouble, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(10.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aSearchRadius));

    aSearchGridSize =
        nAttr.create("SearchGridSize", "srchgrsz", MFnNumericData::kDouble, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0.01));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0.001));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(5.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.1));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aSearchGridSize));

    aIterate = nAttr.create("Iterate", "itr", MFnNumericData::kInt, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(10));
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aIterate));

    aMaxSampleCount = nAttr.create("MaxSampleCount", "mxsmpcnt", MFnNumericData::kInt, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(1));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(1));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(50));
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(10));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aMaxSampleCount));

    outDD = gAttr.create("outDD", "odd", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(gAttr.addAccept(DPData::id));
    DOMMY_RETURN_IF_FAILURE(gAttr.addAccept(DCurveData::id));
    DOMMY_RETURN_IF_FAILURE(gAttr.addAccept(DMeshData::id));
    DOMMY_RETURN_IF_FAILURE(gAttr.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(gAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(outDD));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(inDD, outDD));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aSmoothAttribute, outDD));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aPointRadius, outDD));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aSearchRadius, outDD));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aSearchGridSize, outDD));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aIterate, outDD));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aMaxSampleCount, outDD));

    return MS::kSuccess;
} catch (...) {
    return dommy::maya::reportEntryPointException("DPAttrSmooth", "initialize",
                                                  std::current_exception());
}
