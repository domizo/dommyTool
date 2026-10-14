#pragma once
#include <maya/MFnMesh.h>
#include <maya/MFnMeshData.h>
#include <maya/MItMeshEdge.h>
#include <maya/MItMeshPolygon.h>
#include <maya/MItMeshVertex.h>
#include <maya/MDataBlock.h>
#include <maya/MDataHandle.h>
#include <maya/MPlug.h>
#include <maya/MObjectArray.h>
#include <maya/MPointArray.h>
#include <maya/MFloatPointArray.h>
#include <maya/MIntArray.h>
#include "src/modules/point/DPoint/mesh/DPVoronoi.h"
#include "common/utility/DMeshIntersection.h"
#include "common/utility/DMayaMeshTraversal.h"
#include "common/utility/DMayaMeshImport.h"
#include "common/utility/DMayaValueValidation.h"
#include <array>
#include <memory>
#include <vector>

// Declaration-only custom-data and Voro++ boundaries. Maya classes and the
// production shared helpers above are real; no native linking is performed.
using DPoint=std::array<double,3>;
using DPointList=std::vector<DPoint>;
struct DCurveInternalData {
    DCurveInternalData(const MPointArray&,int);
    bool hasValidCurveTopology()const;
};
using DCItem=std::shared_ptr<DCurveInternalData>;
struct DCurveData {
    static const MTypeId id;
    bool insert(const DCItem&);
    bool hasValidCurveTopology()const;
    void updateBBox();
};
struct DMeshInternalData;
using DMItem=std::shared_ptr<DMeshInternalData>;
struct DMeshInternalData {
    static MStatus createFromMayaObject(const MObject&,DMItem&);
    bool hasValidMeshData()const;
    std::size_t numberOfPoints()const;
    int numFace()const;
    const MIntArray& getConnectList()const;
    std::size_t numberOfConnections()const;
    std::size_t numberOfUVs()const;
};
struct DMeshData {
    using DMeshList=std::vector<DMItem>;
    static const MTypeId id;
    bool insert(const DMeshList&);
    bool hasValidMeshTopology()const;
    void updateBBox();
};
namespace voro {class container;}
namespace dommy {
namespace voronoi {
bool extractCellMeshes(voro::container&,double,std::vector<MObject>&,
    std::size_t,std::size_t,std::size_t,std::size_t);
}
namespace maya {
template<class T>MStatus createPluginDataObject(const MTypeId&,MObject&,T*&);
template<class T>MStatus publishPluginDataObject(MDataHandle&,const MObject&,const T&);
}}
