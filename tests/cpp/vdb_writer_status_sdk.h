#pragma once
// Native Maya API; OpenVDB/payload functions are declaration-only boundaries.
#include <maya/MDataBlock.h>
#include <maya/MDataHandle.h>
#include <maya/MTime.h>
#include <maya/MPlug.h>
#include <maya/MPxData.h>
#include "common/utility/DComputeGuard.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DMayaPluginDataOutput.h"
#include "common/file/DAtomicFile.h"
#include "common/file/DExportPath.h"
#include "common/file/DFileSize.h"
#include "modules/volume/VDB/Common/DVDBTimeMetadata.h"
#include "modules/volume/VDB/Common/DOpenVDBLimits.h"
#include <memory>
#include <vector>
#include <set>

class DOpenVDBData : public MPxData {
public:
    static const MTypeId id;
    MStatus copyFrom(const DOpenVDBData&);
    bool hasValidData() const;
};
namespace openvdb {
class Exception : public std::runtime_error { using std::runtime_error::runtime_error; };
struct GridBase {
    using Ptr = std::shared_ptr<GridBase>;
    using ConstPtr = std::shared_ptr<const GridBase>;
    Ptr copyGrid() const;
    void removeMeta(const char*);
};
template <typename Grid> typename Grid::Ptr ConstPtrCast(typename Grid::Ptr);
using GridPtrSet = std::set<GridBase::Ptr>;
struct StringMetadata { explicit StringMetadata(const char*); };
struct DoubleMetadata { explicit DoubleMetadata(double); };
struct Int32Metadata { explicit Int32Metadata(std::int32_t); };
struct MetaMap { template <typename Metadata> void insertMeta(const char*, const Metadata&); };
namespace io {
const std::uint32_t COMPRESS_ZIP = 1, COMPRESS_BLOSC = 2;
struct File {
    explicit File(const std::string&);
    std::uint32_t compression() const;
    void setCompression(std::uint32_t);
    void write(const GridPtrSet&, const MetaMap&);
    void close();
};
}
}
using Grid = openvdb::GridBase;
using GridPtr = Grid::Ptr;
using GridCPtrVec = std::vector<Grid::ConstPtr>;
using GridCPtrVecIter = GridCPtrVec::iterator;
using StringSet = std::set<std::string>;
void getGrids(GridCPtrVec&, const DOpenVDBData&, const char*);
void reportFloatPrecisionConflicts(const StringSet&);
void printGridInfo(std::ostream&, const DOpenVDBData&);
bool insertFrameNumber(std::string&, const MTime&, int);
#define DOMMY_VDB_COMPUTE_CATCH(name) DOMMY_COMPUTE_CATCH(name)
