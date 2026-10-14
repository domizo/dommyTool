//
//  DpointCommon.hpp
//  dommyTool
//
//  Created by yuzo on 2016/03/01.
//
//

#pragma once

#include <stdio.h>
#include <string>
#include <vector>

#include <maya/MDataBlock.h>
#include <maya/MDoubleArray.h>
#include <maya/MFloatArray.h>
#include <maya/MFn.h>
#include <maya/MMatrixArray.h>
#include <maya/MObject.h>
#include <maya/MPointArray.h>
#include <maya/MStatus.h>
#include <maya/MVectorArray.h>

#include "DDataOp.h"
#include "DPointDataAccess.h"
#include "common/utility/DMayaAttributeArray.h"

#include "DPData.h"
#include "DPrimBaseData.h"

MStatus DOutputDPData(MDataBlock &data, const MObject &dirtPlug, const MObject &output, const DPointList &pointlist);



//Data Method
MFn::Type getDpointType(const MObject &node, const MObject &plug, MObject &pointData, MStatus  &stat);

// Checked, bounded readers. Point/vector/matrix and DScalarDataList readers
// append; Maya float readers replace; the Maya double reader replaces double
// input and appends float input for compatibility. Discard outputs on failure.
MStatus getVectorArrayFromData(const MObject &node, const MObject &plug, MVectorArray  &vArray);
MStatus getPointArrayFromData(const MObject &node, const MObject &plug, MPointArray  &pArray);
MStatus getMatrixArrayFromData(const MObject &node, const MObject &plug, MMatrixArray  &matArray);
MStatus getDoubleArrayFromData(const MObject &node, const MObject &plug, DScalarDataList  &dArray);
MStatus getDoubleArrayFromData(const MObject &node, const MObject &plug, MDoubleArray  &dArray);
MStatus getFloatArrayFromData(const MObject &node, const MObject &plug, MFloatArray  &dArray);

// Compute callers use the supplied block. Object overloads consume a borrowed
// typed-data snapshot synchronously; neither the block nor its data may escape.
MStatus getVectorArrayFromData(const MObject& object, MVectorArray& output);
MStatus getPointArrayFromData(const MObject& object, MPointArray& output);
MStatus getMatrixArrayFromData(const MObject& object, MMatrixArray& output);
MStatus getDoubleArrayFromData(const MObject& object, DScalarDataList& output);
MStatus getDoubleArrayFromData(const MObject& object, MDoubleArray& output);
MStatus getFloatArrayFromData(const MObject& object, MFloatArray& output);
MStatus getVectorArrayFromData(MDataBlock& block, const MObject& attribute, MVectorArray& output);
MStatus getPointArrayFromData(MDataBlock& block, const MObject& attribute, MPointArray& output);
MStatus getMatrixArrayFromData(MDataBlock& block, const MObject& attribute, MMatrixArray& output);
MStatus getDoubleArrayFromData(MDataBlock& block, const MObject& attribute, DScalarDataList& output);
MStatus getDoubleArrayFromData(MDataBlock& block, const MObject& attribute, MDoubleArray& output);
MStatus getFloatArrayFromData(MDataBlock& block, const MObject& attribute, MFloatArray& output);





//Attribute
void checkDPAttribute(const std::string attrName, DPData &outputDPData, bool delAttr);


//attribute add
//template <typename T>
//void DPAddAttr(DPData &dpdata, const bool &isOutAttr,const bool &isDeleteOrg,const std::string &outAttrName,const std::string &originalAttrName, const T &result);
MStatus DPAddAttrScalar(DPData &dpdata, const bool &isOutAttr,const bool &isDeleteOrg,const std::string &outAttrName,const std::string &originalAttrName,const  DScalarDataList &result);
MStatus DPAddAttrVec2(DPData &dpdata, const bool &isOutAttr, const bool &isDeleteOrg, const std::string &outAttrName, const std::string &originalAttrName, const DVec2DataList &result);
MStatus DPAddAttrVec3(DPData &dpdata, const bool &isOutAttr, const bool &isDeleteOrg, const std::string &outAttrName, const std::string &originalAttrName, const DVec3DataList &result);




// Legacy void APIs retain their signatures; failures are raised to the
// existing compute exception boundaries rather than silently publishing data.
template <typename T>
inline void getDPStringList(const DPData& inputData, const std::string& name,
                            std::vector<std::string>& values)
{
    dommy::attribute::requireTypedArray<T>(inputData.getAttr(name), inputData.numberOfPoints(), values);
}

template <typename T>
inline void getDPMatrixList(const DPData& inputData, const std::string& name,
                            std::vector<MMatrix>& values)
{
    dommy::attribute::requireTypedArray<T>(inputData.getAttr(name), inputData.numberOfPoints(), values);
}

template <typename T>
inline void getDPMatrixList(const DPData& inputData, const std::string& name,
                            MMatrixArray& values)
{
    dommy::attribute::requireArrayStatus(
        dommy::attribute::copyTypedMayaArray<T, MMatrix>(
            inputData.getAttr(name), inputData.numberOfPoints(), values));
}

template <typename T>
inline void getDPSingleValueToInt(const DPData& inputData, const std::string& name,
                                 std::vector<int>& values)
{
    dommy::attribute::requireTypedArray<T>(inputData.getAttr(name), inputData.numberOfPoints(), values);
}

// The MDoubleArray overload is a historical API name; it preserves doubles.
template <typename T>
inline void getDPSingleValueToFloat(const DPData& inputData, const std::string& name,
                                   MDoubleArray& values)
{
    dommy::attribute::requireArrayStatus(
        dommy::attribute::copyTypedMayaArray<T, double>(
            inputData.getAttr(name), inputData.numberOfPoints(), values));
}

template <typename T>
inline void getDPSingleValueToDouble(const DPData& inputData, const std::string& name,
                                    DDoubleArray& values)
{
    dommy::attribute::requireTypedArray<T>(inputData.getAttr(name), inputData.numberOfPoints(), values);
}

template <typename T>
inline void getDPSingleValueToFloat(const DPData& inputData, const std::string& name,
                                   MFloatArray& values)
{
    dommy::attribute::requireArrayStatus(
        dommy::attribute::copyTypedMayaArray<T, float>(
            inputData.getAttr(name), inputData.numberOfPoints(), values));
}

template <typename T>
inline void getDPSingleValueToScalar(const DPData& inputData, const std::string& name,
                                    DScalarDataList& values)
{
    dommy::attribute::requireTypedArray<T>(inputData.getAttr(name), inputData.numberOfPoints(), values);
}

template <typename T>
inline void getDPTwoArray(const DPData& inputData, const std::string& name,
                          MDoubleArray& firstArray, MDoubleArray& secondArray)
{
    // Existing callers accept both Vec2 UVs and the first two axes of Vec3.
    std::vector<Imath::V3d> values;
    dommy::attribute::requireTypedArray<T>(inputData.getAttr(name), inputData.numberOfPoints(), values);
    MDoubleArray first, second;
    dommy::attribute::requireArrayStatus(dommy::maya::buildCheckedArray(values.size(),
        [&](unsigned int i, double& value) -> MStatus { value = values[i][0]; return MS::kSuccess; },
        first));
    dommy::attribute::requireArrayStatus(dommy::maya::buildCheckedArray(values.size(),
        [&](unsigned int i, double& value) -> MStatus { value = values[i][1]; return MS::kSuccess; },
        second));
    // Both arrays are staged before either destination changes. As before,
    // aliased destinations end with the second component. Maya copy failures
    // are observable, but cannot guarantee rollback of an earlier copy.
    dommy::attribute::requireArrayStatus(dommy::maya::copyCheckedArray(first, firstArray));
    dommy::attribute::requireArrayStatus(dommy::maya::copyCheckedArray(second, secondArray));
}

template <typename T>
inline void getDPVec2ToVectorArray(const DPData& inputData, const std::string& name,
                                   MVectorArray& values)
{
    dommy::attribute::requireArrayStatus(
        dommy::attribute::copyTypedMayaArray<T, Imath::V3d>(
            inputData.getAttr(name), inputData.numberOfPoints(),
            [](const Imath::V3d& value) { return MVector(value[0], value[1], value[2]); },
            values));
}

template <typename T>
inline void getDPVec2ToVectorArray(const DPData& inputData, const std::string& name,
                                   DVec2DataList& values)
{
    dommy::attribute::requireTypedArray<T>(inputData.getAttr(name), inputData.numberOfPoints(), values);
}

template <typename T>
inline void getDPVec3ToVectorArray(const DPData& inputData, const std::string& name,
                                   DVec3DataList& values)
{
    dommy::attribute::requireTypedArray<T>(inputData.getAttr(name), inputData.numberOfPoints(), values);
}

template <typename T>
inline void getDPVec3ToVectorArray(const DPData& inputData, const std::string& name,
                                   MVectorArray& values)
{
    dommy::attribute::requireArrayStatus(
        dommy::attribute::copyTypedMayaArray<T, Imath::V3d>(
            inputData.getAttr(name), inputData.numberOfPoints(),
            [](const Imath::V3d& value) { return MVector(value[0], value[1], value[2]); },
            values));
}
