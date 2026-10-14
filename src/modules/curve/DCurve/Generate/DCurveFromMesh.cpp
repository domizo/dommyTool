//
//  DCurveFromMesh.cpp
//  dommyTool
//
//  Created by yuzo on 2016/05/05.
//
//

#include "DCurveFromMesh.h"
#include "common/utility/DMayaEntryPointGuard.h"
#include "common/utility/DMayaPluginDataOutput.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DMayaMeshInput.h"
#include "DCurveSourceGenerationPlan.h"


#include "DMayaHeaders.h"
#include "DMayaDebug.h"

#include "DCurveUtil.h"
#include "dcurve_ids.h"
#include "DNodeOperation.h"


#include <memory>


const MTypeId DCurveFromMesh::typeId( ID_DC_FROM_MESH_GEN );


MObject DCurveFromMesh::inMesh;

MObject DCurveFromMesh::outDC;



void *DCurveFromMesh::creator()
{
    return ::dommy::maya::createObject("DCurveFromMesh", [&]() {
        return new DCurveFromMesh();
    });
}




MStatus DCurveFromMesh::compute(const MPlug& plug, MDataBlock& data) try
{
    if (plug != outDC) return MS::kUnknownParameter;

    MObject meshObject;
    DOMMY_RETURN_IF_FAILURE(dommy::maya::getMeshObject(inMesh, data, meshObject));
    MStatus status;
    MFnMesh mesh(meshObject, &status);
    if (!status) return status;

    const int rawPointCount = mesh.numVertices(&status);
    if (!status) return status;
    const int rawEdgeCount = mesh.numEdges(&status);
    if (!status) return status;
    std::size_t pointCount = 0U, edgeCount = 0U, cvCount = 0U;
    if (!dommy::generation::checkedCount(rawPointCount,
            dommy::generation::kMaxGeneratedPointCount, pointCount) ||
        !dommy::generation::checkedCount(rawEdgeCount,
            dommy::generation::kMaxGeneratedCurveCount, edgeCount) ||
        !dommy::generation::checkedProduct(edgeCount, 2U,
            dommy::generation::kMaxGeneratedPointCount, cvCount)) {
        return MS::kInvalidParameter;
    }

    // A data-block mesh is geometry data, not a DAG path. Incoming worldMesh
    // values already contain their transform; never apply world space again.
    std::size_t estimatedBytes = 0U;
    if (!dommy::curve::checkedMeshCurveStorage(pointCount, edgeCount, estimatedBytes))
        return MS::kInvalidParameter;
    MPointArray points;
    DOMMY_RETURN_IF_FAILURE(mesh.getPoints(points, MSpace::kObject));
    if (points.length() != pointCount || !dommy::maya::areFinitePoints(points))
        return MS::kInvalidParameter;

    DCurveData::DCurveList curves;
    curves.reserve(edgeCount);
    MPointArray cvs;
    DOMMY_RETURN_IF_FAILURE(cvs.setLength(2U));
    for (int edge = 0; edge < rawEdgeCount; ++edge) {
        int2 indices = {-1, -1};
        DOMMY_RETURN_IF_FAILURE(mesh.getEdgeVertices(edge, indices));
        if (!dommy::mesh::validEdgeVertexIndices(indices[0], indices[1], pointCount))
            return MS::kInvalidParameter;
        cvs[0] = points[indices[0]];
        cvs[1] = points[indices[1]];
        curves.push_back(std::make_shared<DCurveInternalData>(cvs, 1));
    }

    DOMMY_RETURN_IF_FAILURE(dommy::maya::setPluginDataOutput<DCurveData>(
        data, outDC, DCurveData::id, [&](DCurveData& output) -> MStatus {
            return output.insert(curves) ? MStatus(MS::kSuccess) : MStatus(MS::kFailure);
        }));
    return data.setClean(plug);
}
DOMMY_COMPUTE_CATCH("DCurveFromMesh")

MStatus DCurveFromMesh::initialize() try
{
    MStatus stat;
    
    MFnNumericAttribute nAttr;
    MFnGenericAttribute gAttr;
    MFnTypedAttribute tAttr;
    
    inMesh = gAttr.create( "inMesh", "inm",  &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(gAttr.addDataAccept(MFnData::kMesh));
    DOMMY_RETURN_IF_FAILURE(gAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(gAttr.setWritable( true ));
    DOMMY_RETURN_IF_FAILURE(gAttr.setReadable( false ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inMesh));
    
    
    
    
    outDC = tAttr.create("outDC", "odc", DCurveData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(outDC));
    
    DOMMY_RETURN_IF_FAILURE( attributeAffects( inMesh, outDC ) );
    
    return MS::kSuccess;
}
catch (...) {
    return dommy::maya::reportEntryPointException("DCurveFromMesh", "initialize", std::current_exception());
}
