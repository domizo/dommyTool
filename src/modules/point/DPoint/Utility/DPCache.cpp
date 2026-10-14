//
//  DPCache.cpp
//  dommyTool
//
//  Created by yuzo on 5/26/17.
//
//

#include "common/utility/DMayaEntryPointGuard.h"
#include "DPCache.h"
#include "common/utility/DStatusOperation.h"

#include "DMayaHeaders.h"
#include "DMayaDebug.h"

#include "DPointDataHeaders.h"

#include "DNodeOperation.h"
#include "DFileOperation.h"
#include "common/file/DAtomicFile.h"
#include "common/file/DExportPath.h"
#include "common/file/DFileSize.h"
#include "common/utility/DGenerationLimits.h"
#include "common/utility/DMayaPluginData.h"
#include "common/utility/DMayaPluginDataOutput.h"
#include "common/utility/DAttributeArrayConversion.h"
#include "common/utility/DPointPayloadBudget.h"
#include "common/utility/DComputeGuard.h"

#include <openvdb/openvdb.h>
#include <openvdb/points/PointConversion.h>
#include <openvdb/points/PointCount.h>
#include "DOpenVDBDataHeaders.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <exception>
#include <fstream>
#include <limits>
#include <stdexcept>
#include "../../../volume/VDB/Common/DVDBComputeGuard.h"

namespace {

const std::size_t kMaxDPointCacheFileBytes = 512U * 1024U * 1024U;
const unsigned int kMaxDPointCacheAttributeCount = 1024U;
const unsigned int kMaxDPointCacheAttributeTextBytes = 1024U * 1024U;

// Both Maya and OpenVDB matrices index rows, not flattened scalar elements.
// Stage all 16 finite values before replacing the destination. No transpose.
template <typename Source, typename Destination>
bool copyCacheMatrix(const Source& source, Destination& destination) {
    Destination candidate;
    for (unsigned int row = 0U; row < 4U; ++row) {
        for (unsigned int column = 0U; column < 4U; ++column) {
            const double value = source[row][column];
            if (!std::isfinite(value))
                return false;
            candidate[row][column] = value;
        }
    }
    destination = candidate;
    return true;
}

// Aggregate the host-side payload before allocation. Reuse the common 256 MiB
// conversion budget (including its four-copy allowance). OpenVDB's tree and
// decompression overhead remain subject to the separate grid limit; this is
// not a process-wide peak-memory guarantee.
using CacheWorkspace = dommy::point::PointPayloadBudget;

void reserveCachePoints(std::size_t count, CacheWorkspace& workspace) {
    if (count > dommy::generation::kMaxGeneratedPointCount)
        throw std::length_error("DPCache point count exceeds its safety limit");
    workspace.add(count,
                  sizeof(DPoint) + sizeof(openvdb::Vec3R) + sizeof(openvdb::Index) + sizeof(bool));
}

void preflightCacheInput(const DPData& input) {
    CacheWorkspace workspace;
    dommy::point::preflightPointPayload(
        input, sizeof(DPoint) + sizeof(openvdb::Vec3R) + sizeof(openvdb::Index) + sizeof(bool),
        workspace);
}

std::size_t cacheReadAttributeBytes(const openvdb::points::AttributeArray& data) {
    if (data.isType<DVDBAttrBool>())
        return sizeof(bool);
    if (data.isType<DVDBAttrInt>())
        return sizeof(int32_t);
    if (data.isType<DVDBAttrInt32>())
        return sizeof(uint32_t);
    if (data.isType<DVDBAttrInt64>())
        return sizeof(int64_t);
    if (data.isType<DVDBAttrFloat>())
        return sizeof(float);
    if (data.isType<DVDBAttrDouble>())
        return sizeof(double);
    if (data.isType<DVDBAttrVec2i>())
        return sizeof(Imath::V2i);
    if (data.isType<DVDBAttrVec2f>())
        return sizeof(Imath::V2f);
    if (data.isType<DVDBAttrVec2d>())
        return sizeof(Imath::V2d);
    if (data.isType<DVDBAttrVec3i>())
        return sizeof(Imath::V3i);
    if (data.isType<DVDBAttrVec3f>())
        return sizeof(Imath::V3f);
    if (data.isType<DVDBAttrVec3d>())
        return sizeof(Imath::V3d);
    if (data.isType<DVDBAttrString>())
        return sizeof(std::string);
    if (data.isType<DVDBAttrMat>())
        return sizeof(MMatrix);
    return 0U; // Preserve the existing unsupported-field omission.
}

void preflightCacheRead(std::size_t count, const openvdb::points::PointDataGrid& grid,
                        const MStringArray& names, CacheWorkspace& workspace) {
    reserveCachePoints(count, workspace);
    const auto leaf = grid.tree().cbeginLeaf();
    if (!leaf)
        return;
    const auto& attributes = leaf->attributeSet();
    for (unsigned int i = 0U; i < names.length(); ++i) {
        const std::string name = names[i].asChar();
        if (attributes.find(name) == openvdb::points::AttributeSet::INVALID_POS)
            continue;
        const std::size_t width = cacheReadAttributeBytes(leaf->constAttributeArray(name));
        workspace.add(count, width);
        workspace.add(1U, width);
        workspace.add(name.size(), sizeof(char));
    }
}

template <typename T> void validateCacheValue(const T& value, CacheWorkspace&) {
    if (!dommy::attribute::isValidValue(value))
        throw std::invalid_argument("DPCache attribute contains an invalid value");
}

template <>
void validateCacheValue<std::string>(const std::string& value, CacheWorkspace& workspace) {
    if (!dommy::attribute::isValidValue(value))
        throw std::invalid_argument("DPCache string attribute contains a null byte");
    workspace.add(value.size(), sizeof(char));
}

template <typename T> void registerDPointCacheAttributeType() {
    typedef openvdb::points::TypedAttributeArray<T, openvdb::points::NullCodec> AttributeArray;
    if (!AttributeArray::isRegistered()) {
        AttributeArray::registerType();
    }
}

void registerDPointCacheAttributeTypes() {
    registerDPointCacheAttributeType<openvdb::Index>();
    registerDPointCacheAttributeType<bool>();
    registerDPointCacheAttributeType<int32_t>();
    registerDPointCacheAttributeType<uint32_t>();
    registerDPointCacheAttributeType<int64_t>();
    registerDPointCacheAttributeType<float>();
    registerDPointCacheAttributeType<double>();
    registerDPointCacheAttributeType<Imath::V2i>();
    registerDPointCacheAttributeType<Imath::V2f>();
    registerDPointCacheAttributeType<Imath::V2d>();
    registerDPointCacheAttributeType<Imath::V3i>();
    registerDPointCacheAttributeType<Imath::V3f>();
    registerDPointCacheAttributeType<Imath::V3d>();
    registerDPointCacheAttributeType<std::string>();
    registerDPointCacheAttributeType<openvdb::Mat4d>();
}

bool isCacheGridWithinBudget(const openvdb::GridBase& grid) {
    return static_cast<unsigned long long>(grid.memUsage()) <= kMaxDPointCacheFileBytes;
}

void writeCacheAtomically(const std::string& destination, const openvdb::GridPtrVec& grids) {
    dommy::file::PendingOutputFile pending(destination,
        dommy::file::PendingOutputFile::Access::TrustedPath);
    {
        openvdb::io::File file(pending.trustedTemporaryPath());
        file.write(grids);
        file.close();
    }
    if (!dommy::file::fileSizeWithinLimit(pending.trustedTemporaryPath(), kMaxDPointCacheFileBytes)) {
        throw std::length_error("DPCache output exceeds the cache file-size limit");
    }
    if (!pending.commit()) {
        throw std::runtime_error("DPCache could not commit its temporary cache file");
    }
}

MStatus publishDPointCopy(MDataBlock& data, const MPlug& plug, const MObject& outputAttribute,
                          const DPData* inputData) {
    if (!inputData) {
        return MS::kFailure;
    }

    preflightCacheInput(*inputData);

    MFnPluginData pluginData;
    DPData* outputData = nullptr;
    MStatus status = dommy::maya::createPluginData(pluginData, DPData::id, outputData);
    if (!status || !outputData) {
        return status ? MStatus(MS::kFailure) : status;
    }
    outputData->setDPData(*inputData);

    MDataHandle output = data.outputValue(outputAttribute, &status);
    if (!status) {
        return status;
    }
    status =
        dommy::maya::publishTypedPluginDataObject<DPData>(output, pluginData.object(), DPData::id);
    if (!status) {
        return status;
    }
    return data.setClean(plug);
}

} // namespace

const MTypeId DPCache::typeId(ID_DP_CACHE);

MObject DPCache::inDP;

MObject DPCache::aEnableCache;
MObject DPCache::aCreateMissingCache;
MObject DPCache::aCachePath;

MObject DPCache::aAttributeNames;

MObject DPCache::aCurrentTime;

MObject DPCache::outDP;

void* DPCache::creator() {
    return ::dommy::maya::createObject("DPCache", [&]() {
        return new DPCache();
    });
}

template <typename T>
void writeDPAttributeToOpnVDBPointAttribute(const DPData& dpoint,
                                            openvdb::points::PointDataGrid::Ptr& grid,
                                            openvdb::tools::PointIndexGrid::Ptr& idxGrid,
                                            const std::string& attrName) {

    openvdb::NamePair dAttribute =
        openvdb::points::TypedAttributeArray<T, openvdb::points::NullCodec>::attributeType();

    // get attribute values
    const auto attribute =
        std::dynamic_pointer_cast<const TypedDPAttributeData<T>>(dpoint.getAttr(attrName));
    if (!attribute)
        throw std::invalid_argument("DPCache attribute type mismatch");
    const std::vector<T>& values = attribute->value();
    if (values.size() != dpoint.numberOfPoints()) {
        throw std::invalid_argument("DPCache attribute length does not match its point count");
    }
    const T defaultValue = attribute->getDefaultValue();

    openvdb::points::appendAttribute(grid->tree(), attrName, dAttribute, 1, true,
                                     openvdb::TypedMetadata<T>(defaultValue).copy());
    openvdb::points::PointAttributeVector<T> dWrapper(values);

    openvdb::points::populateAttribute<openvdb::points::PointDataTree,
                                       openvdb::tools::PointIndexTree,
                                       openvdb::points::PointAttributeVector<T>>(
        grid->tree(), idxGrid->tree(), attrName, dWrapper);
}

template <>
void writeDPAttributeToOpnVDBPointAttribute<MMatrix>(const DPData& dpoint,
                                                     openvdb::points::PointDataGrid::Ptr& grid,
                                                     openvdb::tools::PointIndexGrid::Ptr& idxGrid,
                                                     const std::string& attrName) {
    openvdb::NamePair dAttribute =
        openvdb::points::TypedAttributeArray<openvdb::Mat4d,
                                             openvdb::points::NullCodec>::attributeType();

    // get attribute values
    const auto attribute =
        std::dynamic_pointer_cast<const TypedDPAttributeData<MMatrix>>(dpoint.getAttr(attrName));
    if (!attribute)
        throw std::invalid_argument("DPCache matrix attribute type mismatch");
    const std::vector<MMatrix>& values = attribute->value();
    if (values.size() != dpoint.numberOfPoints()) {
        throw std::invalid_argument(
            "DPCache matrix attribute length does not match its point count");
    }
    const MMatrix defaultValue = attribute->getDefaultValue();

    std::vector<openvdb::Mat4d> finValues(values.size());
    for (std::size_t i = 0U; i < values.size(); ++i) {
        if (!copyCacheMatrix(values[i], finValues[i]))
            throw std::invalid_argument("DPCache matrix attribute must be finite");
    }
    openvdb::Mat4d finDdefaultValue;
    if (!copyCacheMatrix(defaultValue, finDdefaultValue))
        throw std::invalid_argument("DPCache matrix default must be finite");

    openvdb::points::appendAttribute(
        grid->tree(), attrName, dAttribute, 1, true,
        openvdb::TypedMetadata<openvdb::Mat4d>(finDdefaultValue).copy());
    openvdb::points::PointAttributeVector<openvdb::Mat4d> dWrapper(finValues);

    openvdb::points::populateAttribute<openvdb::points::PointDataTree,
                                       openvdb::tools::PointIndexTree,
                                       openvdb::points::PointAttributeVector<openvdb::Mat4d>>(
        grid->tree(), idxGrid->tree(), attrName, dWrapper);
}

template <typename T>
void writeOpenVDBPointAttributeToDPAttribute(DPData& dpoint, const size_t& count,
                                             const std::string& attrName,
                                             const openvdb::points::PointDataGrid::Ptr& grid,
                                             const openvdb::points::AttributeSet& attrsets,
                                             CacheWorkspace& workspace) {

    std::vector<T> valuse(count);
    auto leafIter = grid->tree().cbeginLeaf();

    for (; leafIter; ++leafIter) {

        // id
        const openvdb::points::AttributeArray& iArray = leafIter->constAttributeArray("id");
        openvdb::points::AttributeHandle<openvdb::Index32> idxHandle(iArray);

        // data
        const openvdb::points::AttributeArray& darray = leafIter->constAttributeArray(attrName);
        openvdb::points::AttributeHandle<T> dHandle(darray);

        for (auto indexIter = leafIter->beginIndexOn(); indexIter; ++indexIter) {

            openvdb::Index32 rIdx = idxHandle.get(*indexIter);
            if (static_cast<size_t>(rIdx) >= valuse.size())
                throw std::invalid_argument("DPCache attribute index is invalid");
            const T value = dHandle.get(*indexIter);
            validateCacheValue(value, workspace);
            valuse[rIdx] = value;
        }
    }

    T defaultValue = attrsets.descriptor().getDefaultValue<T>(attrName);
    validateCacheValue(defaultValue, workspace);

    dpoint.addAttr(attrName, TypedDPAttributeData<T>(valuse, defaultValue));
}

template <>
void writeOpenVDBPointAttributeToDPAttribute<MMatrix>(
    DPData& dpoint, const size_t& count, const std::string& attrName,
    const openvdb::points::PointDataGrid::Ptr& grid, const openvdb::points::AttributeSet& attrsets,
    CacheWorkspace& workspace) {

    std::vector<MMatrix> valuse(count);
    auto leafIter = grid->tree().cbeginLeaf();

    for (; leafIter; ++leafIter) {

        // id
        const openvdb::points::AttributeArray& iArray = leafIter->constAttributeArray("id");
        openvdb::points::AttributeHandle<openvdb::Index32> idxHandle(iArray);

        // data
        const openvdb::points::AttributeArray& darray = leafIter->constAttributeArray(attrName);
        openvdb::points::AttributeHandle<openvdb::Mat4d> dHandle(darray);

        for (auto indexIter = leafIter->beginIndexOn(); indexIter; ++indexIter) {

            openvdb::Index32 rIdx = idxHandle.get(*indexIter);

            openvdb::Mat4d mat = dHandle.get(*indexIter);

            if (static_cast<size_t>(rIdx) >= valuse.size())
                throw std::invalid_argument("DPCache matrix attribute index is invalid");
            if (!copyCacheMatrix(mat, valuse[rIdx]))
                throw std::invalid_argument("DPCache cached matrix must be finite");
        }
    }

    openvdb::Mat4d defaultValueMat =
        attrsets.descriptor().getDefaultValue<openvdb::Mat4d>(attrName);
    MMatrix mmat;
    if (!copyCacheMatrix(defaultValueMat, mmat))
        throw std::invalid_argument("DPCache cached matrix default must be finite");
    (void)workspace; // Fixed-size matrix payloads were charged by preflight.
    dpoint.addAttr(attrName, TypedDPAttributeData<MMatrix>(valuse, mmat));
}

MStatus DPCache::compute(const MPlug& plug, MDataBlock& data) try {
    MStatus stat;

    if (plug == outDP) {

#pragma mark Get attribute

        const MDataHandle enabledHandle = data.inputValue(aEnableCache, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        const bool isEnableCache = enabledHandle.asBool();

        const MDataHandle namesHandle = data.inputValue(aAttributeNames, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        const MString attributeNames = namesHandle.asString();
        if (attributeNames.length() > kMaxDPointCacheAttributeTextBytes) {
            MGlobal::displayError("DPCache attribute-name input exceeds the supported limit.");
            return MS::kFailure;
        }
        MStringArray attrNameArray;
        DOMMY_RETURN_IF_FAILURE(attributeNames.split(' ', attrNameArray));
        if (attrNameArray.length() > kMaxDPointCacheAttributeCount) {
            MGlobal::displayError("DPCache attribute count exceeds the supported limit.");
            return MS::kFailure;
        }
        const std::string cacheGridName = "dpoint";

#pragma mark use cache
        if (isEnableCache) {

            const MDataHandle pathHandle = data.inputValue(aCachePath, &stat);
            DOMMY_RETURN_IF_FAILURE(stat);
            const MString srcFileName = pathHandle.asString();
            if (srcFileName.length() == 0)
                return MS::kFailure;

            const MDataHandle timeHandle = data.inputValue(aCurrentTime, &stat);
            DOMMY_RETURN_IF_FAILURE(stat);
            const MTime currTime = timeHandle.asTime();

            MString resolvedPath;
            if (!getSequenceFileName(srcFileName, currTime.value(), resolvedPath)) {
                MGlobal::displayError("DPCache cache path contains an invalid frame token.");
                return MS::kFailure;
            }
            const std::string filename = resolvedPath.asChar();
            MFileObject fileObj;
            DOMMY_RETURN_IF_FAILURE(fileObj.setRawFullName(filename.c_str()));

            if (!fileObj.exists()) {
                const MDataHandle createHandle = data.inputValue(aCreateMissingCache, &stat);
                DOMMY_RETURN_IF_FAILURE(stat);
                const bool createMissingCache = createHandle.asBool();

                const DPData* inputDPData = nullptr;
                DOMMY_RETURN_IF_FAILURE(getValidInputDP(inDP, data, inputDPData));
                if (!createMissingCache) {
                    return publishDPointCopy(data, plug, outDP, inputDPData);
                }
                if (!dommy::file::isSafeExportPath(filename, ".vdb")) {
                    MGlobal::displayError("DPCache output path must be a bounded .vdb path "
                                          "without control characters or '..' components.");
                    return MS::kInvalidParameter;
                }

                // ファイルが存在しないのでキャッシュの作成

                if (inputDPData) {

                    preflightCacheInput(*inputDPData);
                    size_t pointCout = inputDPData->numberOfPoints();
                    if (pointCout > dommy::generation::kMaxGeneratedPointCount) {
                        MGlobal::displayError(
                            "DPCache input point count exceeds the supported limit.");
                        return MS::kFailure;
                    }
                    if (pointCout == 0) {
                        return publishDPointCopy(data, plug, outDP, inputDPData);
                    }

                    std::vector<openvdb::Vec3R> positions(pointCout);

                    // set point
                    size_t i = 0;
                    for (auto it = inputDPData->beginPt(), eit = inputDPData->endPt(); it != eit;
                         ++it) {
                        positions[i++] = openvdb::Vec3R((*it)[0], (*it)[1], (*it)[2]);
                    }

                    // set index
                    typedef openvdb::Index idIndexType;
                    std::vector<idIndexType> indexList(pointCout);
                    for (size_t n = 0; n < pointCout; n++) {
                        indexList[n] = n;
                    }

                    openvdb::points::PointAttributeVector<openvdb::Vec3R> positionsWrapper(
                        positions);

                    int pointsPerVoxel = 8;
                    float voxelSize =
                        openvdb::points::computeVoxelSize(positionsWrapper, pointsPerVoxel);
                    if (!std::isfinite(voxelSize) || voxelSize <= 0.0f) {
                        voxelSize = 1.0f;
                    }

                    openvdb::math::Transform::Ptr transform =
                        openvdb::math::Transform::createLinearTransform(voxelSize);

                    //                openvdb::points::PointDataGrid::Ptr grid =
                    //                openvdb::points::createPointDataGrid<openvdb::points::NullCodec,
                    //                openvdb::points::PointDataGrid>(positions, *transform);

                    openvdb::tools::PointIndexGrid::Ptr pointIndexGrid =
                        openvdb::tools::createPointIndexGrid<openvdb::tools::PointIndexGrid>(
                            positionsWrapper, *transform);

                    // create grid
                    openvdb::points::PointDataGrid::Ptr grid =
                        openvdb::points::createPointDataGrid<openvdb::points::NullCodec,
                                                             openvdb::points::PointDataGrid>(
                            *pointIndexGrid, positionsWrapper, *transform);

                    // create attribute
                    //                 using Codec =
                    //                 openvdb::points::FixedPointCodec</*1-byte=*/false,
                    //                 openvdb::points::UnitRange>; using Codec =
                    //                 openvdb::points::NullCodec</*1-byte=*/false>;
                    // openvdbで定義されていないデータタイプは登録するしなくてはいけない
                    openvdb::NamePair idxAttribute = openvdb::points::TypedAttributeArray<
                        idIndexType, openvdb::points::NullCodec>::attributeType();

                    //                openvdb::NamePair idxAttribute =
                    //                DVDBAttrInt32::attributeType();
                    openvdb::points::appendAttribute(grid->tree(), "id", idxAttribute);
                    //                openvdb::points::appendAttribute<uint32_t>(grid->tree(),
                    //                "rIndex");

                    // Create a wrapper around the index vector.
                    openvdb::points::PointAttributeVector<idIndexType> idxWrapper(indexList);

                    // Populate the "rIndex" attribute on the points
                    openvdb::points::populateAttribute<
                        openvdb::points::PointDataTree, openvdb::tools::PointIndexTree,
                        openvdb::points::PointAttributeVector<idIndexType>>(
                        grid->tree(), pointIndexGrid->tree(), "id", idxWrapper);

                    for (int i = 0; i < attrNameArray.length(); ++i) {
                        if (inputDPData->isExistAttr(attrNameArray[i].asChar())) {

                            std::string attrname = attrNameArray[i].asChar();

                            DP_ATTR_DATA_TYPE type = inputDPData->getAttr(attrname)->getAttrType();

                            switch (type) {
                            case DP_ATTR_INT: {
                                writeDPAttributeToOpnVDBPointAttribute<int32_t>(
                                    *inputDPData, grid, pointIndexGrid, attrname);
                            } break;
                            case DP_ATTR_BOOL: {
                                writeDPAttributeToOpnVDBPointAttribute<bool>(
                                    *inputDPData, grid, pointIndexGrid, attrname);
                            } break;
                            case DP_ATTR_UINT32: {
                                writeDPAttributeToOpnVDBPointAttribute<uint32_t>(
                                    *inputDPData, grid, pointIndexGrid, attrname);
                            } break;
                            case DP_ATTR_INT64: {
                                writeDPAttributeToOpnVDBPointAttribute<int64_t>(
                                    *inputDPData, grid, pointIndexGrid, attrname);
                            } break;
                            case DP_ATTR_FLOAT: {
                                writeDPAttributeToOpnVDBPointAttribute<float>(
                                    *inputDPData, grid, pointIndexGrid, attrname);
                            } break;
                            case DP_ATTR_DOUBLE: {
                                writeDPAttributeToOpnVDBPointAttribute<double>(
                                    *inputDPData, grid, pointIndexGrid, attrname);
                            } break;
                            case DP_ATTR_VEC2I: {
                                writeDPAttributeToOpnVDBPointAttribute<Imath::V2i>(
                                    *inputDPData, grid, pointIndexGrid, attrname);
                            } break;
                            case DP_ATTR_VEC2F: {
                                writeDPAttributeToOpnVDBPointAttribute<Imath::V2f>(
                                    *inputDPData, grid, pointIndexGrid, attrname);
                            } break;
                            case DP_ATTR_VEC2D: {
                                writeDPAttributeToOpnVDBPointAttribute<Imath::V2d>(
                                    *inputDPData, grid, pointIndexGrid, attrname);
                            } break;
                            case DP_ATTR_VEC3I: {
                                writeDPAttributeToOpnVDBPointAttribute<Imath::V3i>(
                                    *inputDPData, grid, pointIndexGrid, attrname);
                            } break;
                            case DP_ATTR_VEC3F: {
                                writeDPAttributeToOpnVDBPointAttribute<Imath::V3f>(
                                    *inputDPData, grid, pointIndexGrid, attrname);
                            } break;
                            case DP_ATTR_VEC3D: {
                                writeDPAttributeToOpnVDBPointAttribute<Imath::V3d>(
                                    *inputDPData, grid, pointIndexGrid, attrname);
                            } break;
                            case DP_ATTR_STRING: {
                                writeDPAttributeToOpnVDBPointAttribute<std::string>(
                                    *inputDPData, grid, pointIndexGrid, attrname);
                            } break;
                            case DP_ATTR_MATRIX: {
                                writeDPAttributeToOpnVDBPointAttribute<MMatrix>(
                                    *inputDPData, grid, pointIndexGrid, attrname);
                            } break;
                            default:
                                break;
                            }
                        }
                    }

                    grid->setName(cacheGridName);
                    if (!isCacheGridWithinBudget(*grid)) {
                        MGlobal::displayError("DPCache grid exceeds the in-memory cache limit.");
                        return MS::kFailure;
                    }

                    openvdb::GridPtrVec grids;
                    grids.push_back(grid);
                    writeCacheAtomically(filename, grids);
                    return publishDPointCopy(data, plug, outDP, inputDPData);
                }

            } else {

                // ファイルが存在するので読み込んで使用
                if (!isCacheFileWithinBudget(filename)) {
                    MGlobal::displayError(
                        "DPCache file is unreadable or exceeds the cache file-size limit.");
                    return MS::kFailure;
                }

                openvdb::io::File newFile(filename);
                newFile.open();

                // Read the grid by name.
                openvdb::GridBase::Ptr baseGrid = newFile.readGrid(cacheGridName);
                newFile.close();

                openvdb::points::PointDataGrid::Ptr grid =
                    openvdb::gridPtrCast<openvdb::points::PointDataGrid>(baseGrid);
                if (!grid) {
                    EchoWarning(name().asChar(), "The cache does not contain a PointDataGrid.");
                    return MS::kFailure;
                }
                if (!isCacheGridWithinBudget(*grid)) {
                    MGlobal::displayError("DPCache grid exceeds the in-memory cache limit.");
                    return MS::kFailure;
                }

                openvdb::Index64 count = openvdb::points::pointCount(grid->tree());
                if (count < 0 || static_cast<unsigned long long>(count) >
                                     static_cast<unsigned long long>(
                                         dommy::generation::kMaxGeneratedPointCount)) {
                    EchoWarning(name().asChar(),
                                "The cache point count exceeds the supported limit.");
                    return MS::kFailure;
                }
                CacheWorkspace cacheWorkspace;
                preflightCacheRead(static_cast<std::size_t>(count), *grid, attrNameArray,
                                   cacheWorkspace);
                DPointList pts(static_cast<std::size_t>(count));
                std::vector<bool> pointAssigned(static_cast<std::size_t>(count), false);

                for (auto leafIter = grid->tree().cbeginLeaf(); leafIter; ++leafIter) {
                    // Extract the position attribute from the leaf by name (P is position).
                    const openvdb::points::AttributeArray& positionArray =
                        leafIter->constAttributeArray("P");
                    // Extract the radius attribute from the leaf by name (pscale is radius).
                    const openvdb::points::AttributeArray& idxArray =
                        leafIter->constAttributeArray("id");

                    openvdb::points::AttributeHandle<openvdb::Vec3f> positionHandle(positionArray);
                    openvdb::points::AttributeHandle<openvdb::Index> idxHandle(idxArray);
                    // Iterate over the point indices in the leaf.
                    for (auto indexIter = leafIter->beginIndexOn(); indexIter; ++indexIter) {
                        // Extract the voxel-space position of the point.
                        openvdb::Vec3f voxelPosition = positionHandle.get(*indexIter);
                        // Extract the world-space position of the voxel.
                        openvdb::Vec3d xyz = indexIter.getCoord().asVec3d();
                        // Compute the world-space position of the point.
                        const openvdb::Vec3d worldPosition = grid->transform().indexToWorld(
                            openvdb::Vec3d(voxelPosition[0], voxelPosition[1], voxelPosition[2]) +
                            xyz);
                        // Extract the radius of the point.
                        openvdb::Index rIdx = idxHandle.get(*indexIter);
                        // Verify the index, world-space position and radius of the point.
                        //						std::cout << "* PointIndex=[" << *indexIter << "] ";
                        //						std::cout << "WorldPosition=" << worldPosition << "
                        //"; 						std::cout << "rIndex=" << rIdx << std::endl;

                        const size_t outputIndex = static_cast<size_t>(rIdx);
                        const double maxCoordinate =
                            static_cast<double>(std::numeric_limits<DReal>::max());
                        if (outputIndex >= pts.size() || pointAssigned[outputIndex] ||
                            !std::isfinite(worldPosition[0]) || !std::isfinite(worldPosition[1]) ||
                            !std::isfinite(worldPosition[2]) ||
                            std::fabs(worldPosition[0]) > maxCoordinate ||
                            std::fabs(worldPosition[1]) > maxCoordinate ||
                            std::fabs(worldPosition[2]) > maxCoordinate) {
                            MGlobal::displayError(
                                "DPCache contains an invalid point index or position.");
                            return MS::kFailure;
                        }
                        pts[outputIndex] =
                            DPoint(worldPosition[0], worldPosition[1], worldPosition[2]);
                        pointAssigned[outputIndex] = true;
                    }
                }
                if (std::find(pointAssigned.begin(), pointAssigned.end(), false) !=
                    pointAssigned.end()) {
                    MGlobal::displayError("DPCache is missing one or more point indices.");
                    return MS::kFailure;
                }

                MFnPluginData pluginData;
                pluginData.create(DPData::id, &stat);
                if (!stat)
                    return stat;

                DPData* const outputDPData(
                    dommy::maya::pluginDataPointer<DPData>(pluginData, &stat));
                if (!stat)
                    return stat;
                if (!outputDPData)
                    return MS::kFailure;

                outputDPData->setPosition(pts);

                // add Attribute
                for (int i = 0; i < attrNameArray.length(); ++i) {

                    std::string attrName = attrNameArray[i].asChar();

                    auto leafIter = grid->tree().cbeginLeaf();
                    if (!leafIter) {
                        break;
                    }
                    openvdb::points::AttributeSet attrsets = leafIter->attributeSet();

                    if (attrsets.find(attrName) != openvdb::points::AttributeSet::INVALID_POS &&
                        attrsets.find("id") != openvdb::points::AttributeSet::INVALID_POS) {

                        const openvdb::points::AttributeArray& dataarray =
                            leafIter->constAttributeArray(attrName);

                        if (dataarray.isType<DVDBAttrBool>()) {
                            writeOpenVDBPointAttributeToDPAttribute<bool>(
                                *outputDPData, count, attrName, grid, attrsets, cacheWorkspace);
                        } else if (dataarray.isType<DVDBAttrInt>()) {
                            writeOpenVDBPointAttributeToDPAttribute<int32_t>(
                                *outputDPData, count, attrName, grid, attrsets, cacheWorkspace);
                        } else if (dataarray.isType<DVDBAttrInt32>()) {
                            writeOpenVDBPointAttributeToDPAttribute<uint32_t>(
                                *outputDPData, count, attrName, grid, attrsets, cacheWorkspace);
                        } else if (dataarray.isType<DVDBAttrInt64>()) {
                            writeOpenVDBPointAttributeToDPAttribute<int64_t>(
                                *outputDPData, count, attrName, grid, attrsets, cacheWorkspace);
                        } else if (dataarray.isType<DVDBAttrFloat>()) {
                            writeOpenVDBPointAttributeToDPAttribute<float>(
                                *outputDPData, count, attrName, grid, attrsets, cacheWorkspace);
                        } else if (dataarray.isType<DVDBAttrDouble>()) {
                            writeOpenVDBPointAttributeToDPAttribute<double>(
                                *outputDPData, count, attrName, grid, attrsets, cacheWorkspace);
                        } else if (dataarray.isType<DVDBAttrVec2i>()) {
                            writeOpenVDBPointAttributeToDPAttribute<Imath::V2i>(
                                *outputDPData, count, attrName, grid, attrsets, cacheWorkspace);
                        } else if (dataarray.isType<DVDBAttrVec2f>()) {
                            writeOpenVDBPointAttributeToDPAttribute<Imath::V2f>(
                                *outputDPData, count, attrName, grid, attrsets, cacheWorkspace);
                        } else if (dataarray.isType<DVDBAttrVec2d>()) {
                            writeOpenVDBPointAttributeToDPAttribute<Imath::V2d>(
                                *outputDPData, count, attrName, grid, attrsets, cacheWorkspace);
                        } else if (dataarray.isType<DVDBAttrVec3i>()) {
                            writeOpenVDBPointAttributeToDPAttribute<Imath::V3i>(
                                *outputDPData, count, attrName, grid, attrsets, cacheWorkspace);
                        } else if (dataarray.isType<DVDBAttrVec3f>()) {
                            writeOpenVDBPointAttributeToDPAttribute<Imath::V3f>(
                                *outputDPData, count, attrName, grid, attrsets, cacheWorkspace);
                        } else if (dataarray.isType<DVDBAttrVec3d>()) {
                            writeOpenVDBPointAttributeToDPAttribute<Imath::V3d>(
                                *outputDPData, count, attrName, grid, attrsets, cacheWorkspace);
                        } else if (dataarray.isType<DVDBAttrString>()) {
                            writeOpenVDBPointAttributeToDPAttribute<std::string>(
                                *outputDPData, count, attrName, grid, attrsets, cacheWorkspace);
                        } else if (dataarray.isType<DVDBAttrMat>()) {
                            writeOpenVDBPointAttributeToDPAttribute<MMatrix>(
                                *outputDPData, count, attrName, grid, attrsets, cacheWorkspace);
                        }
                    }
                }

                //				outputDPData->setDPData(*inputDPData);
                outputDPData->updateBBox();

                MDataHandle output = data.outputValue(outDP, &stat);
                if (!stat)
                    return stat;
                DOMMY_RETURN_IF_FAILURE(dommy::maya::publishTypedPluginDataObject<DPData>(
                    output, pluginData.object(), DPData::id));
                return data.setClean(plug);
            }
        }

#pragma mark cant use cache キャッシュを利用できない、あるいは使用しなかった場合
        const DPData* inputDPData = nullptr;
        DOMMY_RETURN_IF_FAILURE(getValidInputDP(inDP, data, inputDPData));
        return publishDPointCopy(data, plug, outDP, inputDPData);

    } // check plug

    return plug == outDP ? MStatus(MS::kFailure) : MStatus(MS::kUnknownParameter);
}
DOMMY_VDB_COMPUTE_CATCH("DPCache")

MStatus DPCache::initialize() try {
    registerDPointCacheAttributeTypes();
    MStatus stat;
    MFnNumericAttribute nAttr;
    MFnGenericAttribute gAttr;
    MFnTypedAttribute tAttr;
    MFnStringData fnStringData;
    MFnGenericAttribute outAttr;

    // ----------------------- INPUTS --------------------------

    inDP = tAttr.create("inDP", "idp", DPData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inDP));

    aEnableCache = nAttr.create("EnableCache", "ebch", MFnNumericData::kBoolean, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(false));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aEnableCache));

    aCreateMissingCache =
        nAttr.create("CreateMissingCache", "cmc", MFnNumericData::kBoolean, false, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(false));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(nAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aCreateMissingCache));

    MObject defaultFileStringData = fnStringData.create("dpoint.vdb", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aCachePath = tAttr.create("CachePath", "chf", MFnData::kString, defaultFileStringData, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aCachePath));

    const MObject defaultNames = fnStringData.create("", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aAttributeNames =
        tAttr.create("AttributeNames", "attrnms", MFnData::kString, defaultNames, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aAttributeNames));

    MFnUnitAttribute uniAttr;
    aCurrentTime = uniAttr.create("CurrentTime", "ctm", MFnUnitAttribute::kTime, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(addAttribute(aCurrentTime));

    outDP = tAttr.create("outDP", "odp", DPData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(outDP));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(inDP, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aCachePath, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aEnableCache, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aCreateMissingCache, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aAttributeNames, outDP));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(aCurrentTime, outDP));

    return MS::kSuccess;
} catch (...) {
    return dommy::maya::reportEntryPointException("DPCache", "initialize",
                                                  std::current_exception());
}
