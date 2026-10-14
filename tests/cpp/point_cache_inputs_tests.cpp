#include "group_transfer_init_doubles.h"
struct MStringArray {};
struct CacheString : MString {
    using MString::MString;
    MStatus split(char delimiter, MStringArray&) const {
        assert(delimiter == ' ');
        return operation("split");
    }
};
struct CacheHandle : MDataHandle {
    MTime asTime() const {
        read();
        return MTime();
    }
    CacheString asString() const {
        read();
        return {"path"};
    }
};
struct CacheBlock {
    CacheHandle inputValue(const MObject& attr, MStatus* status) {
        *status = operation("input " + attr.name);
        CacheHandle handle;
        handle.valid = bool(*status);
        return handle;
    }
};
struct MFileObject {
    MStatus setRawFullName(const char*) {
        return operation("file.path");
    }
};
#define MDataHandle CacheHandle
#define MDataBlock CacheBlock
#define MString CacheString
struct CachePoints {
    static constexpr MFnData::Type id = MFnData::kAny;
    bool valid = true;
    bool hasValidPointTopology() const { return valid; }
};
CachePoints cachePoints;
bool nullPoints = false;
namespace dommy { namespace maya {
template<class T> MStatus getPluginDataFromHandle(const CacheHandle& handle,
                                                 MFnData::Type, const T*& output) {
    handle.read();
    const auto status = operation("plugin.read");
    if (status) output = nullPoints ? nullptr : &cachePoints;
    return status;
}
} }
#define DPData CachePoints
MObject inDP{"inDP"};
MObject aEnableCache{"enable"}, aAttributeNames{"names"}, aCachePath{"path"};
MObject aCurrentTime{"time"}, aCreateMissingCache{"create"};
#include "point_cache_inputs.inc"

int main() {
    MDataBlock data;
    int failures = 0;
    for (auto reader :
         {read_enabled, read_names, read_path, read_time, read_create, read_split, read_filename}) {
        resetCalls();
        invalidReads = 0;
        assert(reader(data) && callCount == 1);
        for (auto status : {MS::kFailure, MS::kInvalidParameter}) {
            resetCalls(0);
            injectedStatus = status;
            assert(reader(data) == status && invalidReads == 0 && callCount == 1);
            ++failures;
        }
        resetCalls();
        attributeThrowAt = 0;
        assert(reader(data) == MS::kFailure && invalidReads == 0);
        ++failures;
        resetCalls();
        assert(reader(data));
    }
    for (auto reader : {read_points_0, read_points_1}) {
        resetCalls(); assert(reader(data) == MS::kSuccess && callCount == 2);
        for (int index : {0,1}) for (auto code : {MS::kFailure,MS::kInvalidParameter}) {
            resetCalls(index); injectedStatus = code;
            assert(reader(data) == code && callCount == index + 1 && invalidReads == 0);
        }
        resetCalls(); cachePoints.valid = false;
        assert(reader(data) == MS::kInvalidParameter);
        cachePoints.valid = true; nullPoints = true; resetCalls();
        assert(reader(data) == MS::kInvalidParameter);
        nullPoints = false; resetCalls(); assert(reader(data) == MS::kSuccess);
    }
    std::cout << "DPCache: five input readers, string split and file path, " << failures
              << " failed statuses/exceptions stopped before dependent access\n";
}
