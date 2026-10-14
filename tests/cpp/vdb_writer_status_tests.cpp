#include "maya_attribute_doubles.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DStatusOperation.h"
#include "common/file/DExportPath.h"
#include "modules/volume/VDB/Common/DVDBTimeMetadata.h"
#include <map>
#include <memory>
#include <set>
#include <iostream>

// Boundary doubles: status-bearing APIs can return valid-looking failed handles.
int invalidAccesses = 0, publications = 0, infoWrites = 0, fileWrites = 0, commits = 0;
int cleanRequests = 0;
struct WriterTime {
    enum Unit { kSeconds, kFilm };
    double frame;
    explicit WriterTime(double value = 0, Unit unit = kFilm) : frame(unit == kSeconds ? value * 24 : value) {}
    static Unit uiUnit() { return kFilm; }
    double as(Unit) const { return frame; }
};
#define MTime WriterTime
struct Input { double number = 0; MString text{"archive.vdb"}; };
struct MDataHandle {
    Input* input;
    bool valid;
    void check() const { if (!valid) ++invalidAccesses; }
    bool asBool() const { check(); return input->number != 0; }
    int asInt() const { check(); return static_cast<int>(input->number); }
    short asShort() const { check(); return static_cast<short>(input->number); }
    MTime asTime() const { check(); return MTime(input->number); }
    MString asString() const { check(); return input->text; }
    void set(const MString& value) { check(); input->text = value; ++infoWrites; }
    void setClean() { check(); ++cleanRequests; }
};
struct MPlug {
    MObject attribute;
    MPlug(const MObject& value) : attribute(value) {}
    bool operator==(const MObject& other) const { return attribute.name == other.name; }
};
struct MDataBlock {
    std::map<std::string, Input> inputs;
    MDataHandle inputValue(const MObject& attribute, MStatus* status = nullptr) {
        const auto result = operation("input " + attribute.name);
        if (status) *status = result;
        return {&inputs[attribute.name], bool(result)};
    }
    MDataHandle outputValue(const MObject& attribute, MStatus* status = nullptr) {
        const auto result = operation("output " + attribute.name);
        if (status) *status = result;
        return {&inputs[attribute.name], bool(result)};
    }
    MStatus setClean(const MObject& attribute) {
        ++cleanRequests;
        return operation("clean " + attribute.name);
    }
    MStatus setClean(const MPlug& plug) { return setClean(plug.attribute); }
};
struct DOpenVDBData {
    static const int id = 24;
    MStatus copyFrom(const DOpenVDBData&) { return operation("payload.copy"); }
};
DOpenVDBData inputData, stagedData;
struct MFnPluginData { MObject object() const { return {"staged"}; } };
namespace dommy { namespace maya {
MStatus getPluginDataFromHandle(const MDataHandle& handle, int, const DOpenVDBData*& out) {
    handle.check(); const auto status = operation("payload.bind");
    if (status) out = &inputData;
    return status;
}
MStatus createPluginData(MFnPluginData&, int, DOpenVDBData*& out) {
    const auto status = operation("payload.create");
    if (status) out = &stagedData;
    return status;
}
template <typename Data> MStatus publishTypedPluginDataObject(MDataHandle& handle, const MObject&, int) {
    handle.check(); const auto status = operation("payload.publish");
    if (status) ++publications;
    return status;
}
}}
namespace openvdb {
struct Exception : std::runtime_error { using std::runtime_error::runtime_error; };
struct GridBase {
    using Ptr = std::shared_ptr<GridBase>;
    using ConstPtr = std::shared_ptr<const GridBase>;
    Ptr copyGrid() const { return std::make_shared<GridBase>(); }
    void removeMeta(const char*) {}
};
template <typename Grid> typename Grid::Ptr ConstPtrCast(typename Grid::Ptr pointer) { return pointer; }
using GridPtrSet = std::set<GridBase::Ptr>;
struct StringMetadata { explicit StringMetadata(const char*) {} };
struct DoubleMetadata { explicit DoubleMetadata(double) {} };
struct Int32Metadata { explicit Int32Metadata(std::int32_t) {} };
struct MetaMap { template <typename Metadata> void insertMeta(const char*, const Metadata&) {} };
namespace io {
const std::uint32_t COMPRESS_ZIP = 1, COMPRESS_BLOSC = 2;
struct File {
    explicit File(const std::string&) { if (!operation("file.open")) throw std::runtime_error("open"); }
    std::uint32_t compression() const { return COMPRESS_ZIP | COMPRESS_BLOSC; }
    void setCompression(std::uint32_t) {}
    void write(const GridPtrSet&, const MetaMap&) {
        if (!operation("file.write")) throw std::runtime_error("write");
        ++fileWrites;
    }
    void close() { if (!operation("file.close")) throw std::runtime_error("close"); }
};
}
}
using Grid = openvdb::GridBase;
using GridPtr = Grid::Ptr;
using GridCPtrVec = std::vector<Grid::ConstPtr>;
using GridCPtrVecIter = GridCPtrVec::iterator;
using StringSet = std::set<std::string>;
void getGrids(GridCPtrVec& grids, const DOpenVDBData&, const char*) {
    grids.push_back(std::make_shared<Grid>());
}
void reportFloatPrecisionConflicts(const StringSet&) {}
void printGridInfo(std::ostream& stream, const DOpenVDBData&) { stream << "one grid"; }
bool insertFrameNumber(std::string&, const MTime&, int) { return true; }
namespace dommy { namespace file {
class PendingOutputFile {
public:
    enum class Access { Handle, TrustedPath };
    explicit PendingOutputFile(const std::string&, Access) {
        if (!operation("file.reserve")) throw std::runtime_error("reserve");
    }
    std::string temporaryPath() const { return "test-seam-no-real-file"; }
    std::string trustedTemporaryPath() const { return "test-seam-no-real-file"; }
    bool commit() { const auto status = operation("file.commit"); if (status) ++commits; return bool(status); }
};
bool fileSizeWithinLimit(const std::string&, unsigned long long) { return bool(operation("file.size")); }
}}
namespace dommy { namespace vdb { const std::size_t kMaxVdbPayloadBytes = 1024; }}
#define DOMMY_VDB_COMPUTE_CATCH(name) DOMMY_COMPUTE_CATCH(name)
#include "vdb_writer_status.inc"

void reset(int failure = -1) {
    resetCalls(failure);
    invalidAccesses = publications = infoWrites = fileWrites = commits = cleanRequests = 0;
}
void cases(bool writeFile, const MObject& requested) {
    DWriteOpenVDB node; MDataBlock block;
    block.inputs["aWriteFile"].number = writeFile;
    block.inputs["aInputTime"].number = 12;
    block.inputs["aCompression"].number = 2;
    reset(); assert(node.compute(requested, block));
    assert(invalidAccesses == 0 && publications == 1 && infoWrites == 1);
    assert(commits == (writeFile ? 1 : 0));
    const auto trace = operations;
    for (std::size_t index = 0; index < trace.size(); ++index) {
        for (auto code : {MS::kFailure, MS::kInvalidParameter}) {
            reset(static_cast<int>(index)); injectedStatus = code;
            const auto result = node.compute(requested, block);
            const bool fileBoundary = trace[index].find("file.") == 0;
            assert(result == (fileBoundary ? MS::kFailure : code));
            assert(invalidAccesses == 0 && callCount == static_cast<int>(index) + 1);
            if (trace[index].find("input ") == 0) assert(commits == 0);
        }
        reset(); attributeThrowAt = static_cast<int>(index);
        assert(node.compute(requested, block) == MS::kFailure && invalidAccesses == 0);
        assert(callCount == static_cast<int>(index) + 1);
    }
    reset(); assert(node.compute(requested, block));
    assert(invalidAccesses == 0 && infoWrites == 1);
}
int main() {
    for (auto state : {std::ios::failbit, std::ios::badbit}) {
        MDataBlock block;
        block.inputs["aNodeInfo"].text = "prior";
        std::stringstream stream;
        stream << "partial";
        stream.setstate(state);
        reset();
        assert(updateNodeInfo(stream, block, DWriteOpenVDB::aNodeInfo) == MS::kFailure);
        assert(callCount == 0 && infoWrites == 0 && cleanRequests == 0);
        assert(std::string(block.inputs["aNodeInfo"].text.asChar()) == "prior");
        assert(stream.rdstate() == state);
        std::stringstream recovered;
        recovered << "complete";
        reset();
        assert(updateNodeInfo(recovered, block, DWriteOpenVDB::aNodeInfo));
        assert(infoWrites == 1 && cleanRequests == 1);
        assert(std::string(block.inputs["aNodeInfo"].text.asChar()) == "complete");
    }
    for (bool writeFile : {false, true})
        for (const auto& requested : {DWriteOpenVDB::aVdbOutput, DWriteOpenVDB::aNodeInfo})
            cases(writeFile, requested);
    DWriteOpenVDB node; MDataBlock block; reset();
    assert(node.compute(MObject{"other"}, block) == MS::kUnknownParameter && callCount == 0);
    for (int compression : {-1, 3}) {
        block.inputs["aWriteFile"].number = 1;
        block.inputs["aCompression"].number = compression;
        reset(); assert(node.compute(DWriteOpenVDB::aVdbOutput, block) == MS::kInvalidParameter);
        assert(commits == 0 && fileWrites == 0 && invalidAccesses == 0);
    }
    block.inputs["aCompression"].number = 2;
    for (const char* path : {"../archive.vdb", "archive.txt", "archive\n.vdb"}) {
        block.inputs["aVdbFilePath"].text = path;
        reset(); assert(node.compute(DWriteOpenVDB::aVdbOutput, block) == MS::kInvalidParameter);
        assert(commits == 0 && fileWrites == 0);
    }
    std::cout << "Complete VDB writer: checked reads, NodeInfo output/clean, failures and recovery; IO seams only\n";
}
