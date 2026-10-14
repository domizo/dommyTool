#include "dupMesh.h"
#include "DMeshUtil.h"
#include "DCurveData.h"

#include <type_traits>
#include <utility>

// Snapshot reads and container growth must not allocate/copy Maya arrays.
static_assert(std::is_nothrow_copy_constructible<dupMesh>::value,
              "dupMesh copies must share the immutable snapshot");
static_assert(std::is_nothrow_copy_assignable<dupMesh>::value,
              "dupMesh assignment must share the immutable snapshot");

#define CHECK_SNAPSHOT_VIEW(method, Array) \
    static_assert(std::is_same<decltype(std::declval<const dupMesh&>().method()), \
                  const Array&>::value, "snapshot getter must return a const reference")

CHECK_SNAPSHOT_VIEW(getVertexArray, MFloatPointArray);
CHECK_SNAPSHOT_VIEW(getPolyCountsList, MIntArray);
CHECK_SNAPSHOT_VIEW(getPolyConnectList, MIntArray);
CHECK_SNAPSHOT_VIEW(getNormalArray, MFloatVectorArray);
CHECK_SNAPSHOT_VIEW(getNormIdsArray, MIntArray);
CHECK_SNAPSHOT_VIEW(getUCoords, MFloatArray);
CHECK_SNAPSHOT_VIEW(getVCoords, MFloatArray);
CHECK_SNAPSHOT_VIEW(getUVCountsArray, MIntArray);
CHECK_SNAPSHOT_VIEW(getUVIdsArray, MIntArray);
CHECK_SNAPSHOT_VIEW(getEdgeArray, MIntArray);

#undef CHECK_SNAPSHOT_VIEW

static_assert(std::is_same<decltype(std::declval<const DMeshInternalData&>().getUvData(
                  std::declval<MFloatArray&>(), std::declval<MFloatArray&>(),
                  std::declval<MIntArray&>(), std::declval<MIntArray&>())),
              MStatus>::value, "UV copy failure must remain observable");
static_assert(std::is_same<decltype(DMGeneratePrimitiveCenterList(
                  std::declval<const DMeshData&>(), std::declval<DPointList&>())),
              MStatus>::value, "point center failures must remain observable");
static_assert(std::is_same<decltype(DMGeneratePrimitiveCenterList(
                  std::declval<const DMeshData&>(), std::declval<MVectorArray&>())),
              MStatus>::value, "Maya center array failures must remain observable");

static_assert(std::is_same<decltype(std::declval<DMeshData&>().copyFrom(
                  std::declval<const DMeshData&>())), MStatus>::value,
              "internal mesh copy failure must remain observable");
static_assert(std::is_same<decltype(std::declval<DCurveData&>().copyFrom(
                  std::declval<const DCurveData&>())), MStatus>::value,
              "internal curve copy failure must remain observable");
static_assert(std::is_same<decltype(DMeshInternalData::createMerged(
                  std::declval<const std::vector<const DMeshInternalData*>&>(),
                  std::declval<DMeshInternalData::Ptr&>())), MStatus>::value,
              "bulk merge failure must remain observable");
static_assert(std::is_same<decltype(generateMMeshFromDmeshWithUV(
                  std::declval<const DMeshInternalData&>(), std::declval<MObject&>(),
                  DMeshPointSpace::Transformed)), MStatus>::value,
              "mesh output conversion must support explicit transform baking");
