#pragma once
// Native API signatures; procedural owners and OpenVDB are declarations only.
#include <maya/MPxData.h>
#include <maya/MVector.h>
#include <maya/MFloatVector.h>
#include <maya/MTime.h>
#include <maya/MFnMesh.h>
#include <maya/MPointArray.h>
#include <maya/MFloatPointArray.h>
#include <maya/MFloatArray.h>
#include <maya/MFileObject.h>
#include "common/utility/DMayaPluginDataOutput.h"
#include "common/utility/DMayaNameList.h"
#include "common/utility/DMayaValueValidation.h"
#include "common/utility/DComputeGuard.h"
#include "common/file/DFileOperation.h"
#include "common/file/DFileSize.h"
#include "modules/volume/VDB/Common/DOpenVDBLimits.h"
#include <fstream>
#include <memory>
#include <vector>

class DPData : public MPxData {
public:
    static const MTypeId id;
    bool hasValidPointTopology() const;
    void setDPData(const DPData&);
    bool isExistAttr(const std::string&) const;
    void removeAttr(const std::string&);
    bool isExistGrp(const std::string&) const;
    void removeGrp(const std::string&);
};
const DPData* getInputDP(const MObject&, MDataBlock&);
MStatus getValidInputDP(const MObject&, MDataBlock&, const DPData*&);
namespace openvdb {
class Exception : public std::runtime_error { using std::runtime_error::runtime_error; };
class GridBase;
using GridPtrVec = std::vector<std::shared_ptr<GridBase>>;
using GridPtrVecPtr = std::shared_ptr<GridPtrVec>;
namespace io {
class Stream {
public:
    explicit Stream(std::istream&);
    GridPtrVecPtr getGrids();
};
}
}
class DOpenVDBData : public MPxData {
public:
    static const MTypeId id;
    bool hasValidData() const;
    void insert(const openvdb::GridPtrVec&);
};
namespace dommy { namespace attribute { bool validAttributeName(const std::string&); }}
