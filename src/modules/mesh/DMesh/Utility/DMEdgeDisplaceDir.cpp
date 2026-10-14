//
//  DMEdgeDisplaceDir.cpp
//  dommyTool
//
//  Created by yuzo on 2017/10/31.
//

#include "common/utility/DMayaEntryPointGuard.h"
#include "DMEdgeDisplaceDir.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DMayaPluginDataOutput.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DMeshInputValidation.h"
#include "common/utility/DMayaValueValidation.h"
#include <cmath>




#include "DMayaHeaders.h"
#include "DMayaDebug.h"


#include "DMeshDataHeaders.h"

#include "dommyMath.h"
#include "DPointOp.h"
#include "DDataOp.h"
#include "DUtility.h"


const MTypeId DMEdgeDisplaceDir::typeId( ID_DM_EDGE_DISPLACE_DIR );


MObject DMEdgeDisplaceDir::inDM;

MObject DMEdgeDisplaceDir::aInObject;

MObject DMEdgeDisplaceDir::aAttributeName;
MObject DMEdgeDisplaceDir::aMinRatio;
MObject DMEdgeDisplaceDir::outDM;




void *DMEdgeDisplaceDir::creator()
{
    return ::dommy::maya::createObject("DMEdgeDisplaceDir", [&]() {
        return new DMEdgeDisplaceDir();
    });
}



MStatus DMEdgeDisplaceDir::compute(const MPlug& plug, MDataBlock& data) try
{
    if (plug != outDM) return MS::kUnknownParameter;
    MStatus stat;
    MPlug meshPlug(thisMObject(), aInObject);
    const bool meshConnected = meshPlug.isConnected(&stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    if (!meshConnected) return MS::kFailure;
    MDataHandle objectHandle = data.inputValue(aInObject, &stat);
    if (!stat) return stat;
    if (objectHandle.type() != MFnData::kMesh) return MS::kInvalidParameter;
    const DMeshData* inputMeshes = nullptr;
    DOMMY_RETURN_IF_FAILURE(getValidInputDM(inDM, data, inputMeshes));
    if (!inputMeshes) return MS::kFailure;

    MFloatPointArray referencePoints;
    PointCloud<double> referenceCloud;
    std::string attrName;
    double minRatio = 0.0;
    if (inputMeshes->numberOfMesh() != 0U) {
        MFnMesh reference(objectHandle.data(), &stat);
        if (!stat) return stat;
        const int referenceCount = reference.numVertices(&stat);
        if (!stat) return stat;
        std::size_t queryCount = 0U;
        // Counts must be checked before either Maya arrays or the search cloud are acquired.
        if (referenceCount < 0 || !inputMeshes->getCheckedTotalVertex(queryCount) ||
            !dommy::mesh::validPointSearchStorage(
                static_cast<std::size_t>(referenceCount), queryCount)) return MS::kInvalidParameter;
        const MDataHandle nameHandle = data.inputValue(aAttributeName, &stat);
        if (!stat) return stat;
        attrName = nameHandle.asString().asChar();
        const MDataHandle ratioHandle = data.inputValue(aMinRatio, &stat);
        if (!stat) return stat;
        minRatio = ratioHandle.asDouble();
        if (!std::isfinite(minRatio) || minRatio < 0.0 || minRatio > 1.0)
            return MS::kInvalidParameter;
        DOMMY_RETURN_IF_FAILURE(reference.getPoints(referencePoints, MSpace::kWorld));
        if (referencePoints.length() != static_cast<unsigned int>(referenceCount))
            return MS::kFailure;
        for (unsigned int i = 0U; i < referencePoints.length(); ++i) {
            const MFloatPoint& point = referencePoints[i];
            if (!std::isfinite(point.x) || !std::isfinite(point.y) ||
                !std::isfinite(point.z) || !std::isfinite(point.w)) return MS::kInvalidParameter;
        }
        CreateSearchDataMFP(referencePoints, referenceCloud);
    }

    MObject outputObject;
    DMeshData* outputMeshes = nullptr;
    DOMMY_RETURN_IF_FAILURE(dommy::maya::createPluginDataObject<DMeshData>(
        DMeshData::id, outputObject, outputMeshes));
    if (!outputMeshes) return MS::kFailure;
    DOMMY_RETURN_IF_FAILURE(outputMeshes->copyFrom(*inputMeshes));
    if (outputMeshes->numberOfMesh() != 0U) {
        DPkdtree index(3, referenceCloud, nanoflann::KDTreeSingleIndexAdaptorParams(10));
        index.buildIndex();
        checkDMeshPointAttribute(attrName, *outputMeshes, true);
        for (auto mesh = outputMeshes->beginPrims(); mesh != outputMeshes->endPrims(); ++mesh) {
            const DPointList* points = (*mesh)->getPositionPtrConst();
            DVec3DataList directions(points->size(), DVec3Data(0, 0, 0));
            IDsListArray ids(points->size());
            DScalarListArray distances(points->size());
            PointSearchKDTreeRadiusWithDistanceOp search(index, *points, 1, 10.0, ids, distances);
            tbb::parallel_for(tbb::blocked_range<std::size_t>(0, points->size(), 10000), search);
            for (std::size_t i = 0U; i < points->size(); ++i) {
                if (ids[i].size() != distances[i].size()) return MS::kFailure;
                if (ids[i].empty()) continue;
                const auto nearest = std::min_element(distances[i].begin(), distances[i].end());
                const std::size_t slot = static_cast<std::size_t>(std::distance(distances[i].begin(), nearest));
                const std::size_t referenceId = ids[i][slot];
                if (referenceId >= referencePoints.length() || !std::isfinite(*nearest) || *nearest < 0.0)
                    return MS::kFailure;
                // Preserve the historical point-space formula and distance weighting.
                MVector direction = MPoint(referencePoints[static_cast<unsigned int>(referenceId)]) -
                    DPToMP((*points)[i]);
                const double length = direction.length();
                if (!std::isfinite(length)) return MS::kInvalidParameter;
                if (length > 0.001) {
                    const double ratio = *nearest < 1.0 ? *nearest * minRatio : 1.0;
                    direction = direction.normal() * ratio;
                } else {
                    direction = MVector(0, 0, 0);
                }
                if (!dommy::maya::isFloatRepresentableVector(direction)) return MS::kInvalidParameter;
                directions[i] = MVToDP(direction);
            }
            (*mesh)->addAttrWithValues(attrName, directions);
        }
    }
    MDataHandle output = data.outputValue(outDM, &stat);
    if (!stat) return stat;
    DOMMY_RETURN_IF_FAILURE(dommy::maya::publishTypedPluginDataObject<DMeshData>(
        output, outputObject, DMeshData::id));
    return data.setClean(plug);
}
DOMMY_COMPUTE_CATCH("DMEdgeDisplaceDir")
            
            
            
            
            




MStatus DMEdgeDisplaceDir::initialize() try {
    MStatus stat;
    MFnNumericAttribute nAttr;
    MFnGenericAttribute gAttr;
    MFnTypedAttribute tAttr;
    MFnStringData fnStringData;
    
    // ----------------------- INPUTS --------------------------
    inDM = tAttr.create("inDM", "idm", DMeshData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inDM));
    
    
   
    aInObject = gAttr.create( "inObject", "ino",  &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(gAttr.addDataAccept(MFnData::kMesh));
    DOMMY_RETURN_IF_FAILURE(gAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(gAttr.setWritable( true ));
    DOMMY_RETURN_IF_FAILURE(gAttr.setReadable( false ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aInObject));
    
    
    const MObject aAttributeNameDefault = fnStringData.create("dir", &stat);
    
    
    DOMMY_RETURN_IF_FAILURE(stat);
    
    
    aAttributeName = tAttr.create("AttributeName", "attrnm", MFnData::kString, aAttributeNameDefault, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aAttributeName));
    
    
    aMinRatio = nAttr.create( "MinRatio", "minrto", MFnNumericData::kDouble, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.2));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMax(1.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aMinRatio));
    
    
    //create custom data
    outDM = tAttr.create("outDM", "odm", DMeshData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(outDM));
    
    
    
    
    
    DOMMY_RETURN_IF_FAILURE(attributeAffects( inDM, outDM ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aInObject, outDM ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aAttributeName, outDM ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aMinRatio, outDM ));
    
    
    return MS::kSuccess;
}
catch (...) {
    return dommy::maya::reportEntryPointException("DMEdgeDisplaceDir", "initialize", std::current_exception());
}
