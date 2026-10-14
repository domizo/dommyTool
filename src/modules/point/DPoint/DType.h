//
//  Dtype.h
//  dommyTool
//
//  Created by yuzo on 2017/02/24.
//
//

#pragma once

#include <vector>
#include <map>
#include <memory>
#include <unordered_map>
#include <string>

#include <ImathVec.h>

#include <maya/MFnData.h>
#include <maya/MMatrix.h>
#include <maya/MFloatArray.h>
#include <maya/MDoubleArray.h>
#include <maya/MPointArray.h>
#include <maya/MVectorArray.h>
#include <maya/MFloatPointArray.h>
#include <maya/MMatrixArray.h>
#include <maya/MIntArray.h>


enum { D_COMP_POINT = 0, D_COMP_EDGE, D_COMP_FACE, D_COMP_PRIM, D_COMP_PER_VTX, D_COMP_COUNT };
typedef short DComponentType;

enum { DP_ATTR_CLASS_POINT = 0, DP_ATTR_CLASS_PRIMITIVE, DP_ATTR_CLASS_TYPE_COUNT };
typedef short DP_ATTRIBUTE_CLASS_TYPE;

enum { DP_ATTR_PRE_FLOAT = 0, DP_ATTR_PRE_DOUBLE, DP_ATTR_PRECISION_COUNT };
typedef short DP_ATTR_PRECISION;

enum { DP_ATTR_DIM_SCALAR = 0, DP_ATTR_DIM_VEC2D, DP_ATTR_DIM_VEC3D, DP_ATTR_DIM_TYPE_COUNT };
typedef short DP_ATTR_DIM_TYPE;

enum {
    D_X_AXIS = 0,
    D_Y_AXIS,
    D_Z_AXIS,
    D_X_AXIS_MINUS,
    D_Y_AXIS_MINUS,
    D_Z_AXIS_MINUS,
    D_AXIS_TYPE_COUNT
};
typedef short D_AXIS_TYPE;

enum {
    DA_CREATE_FLOAT = 0,
    DA_CREATE_INT,
    DA_CREATE_VECTOR,
    DA_CREATE_STRING,
    DA_CREATE_MATRIX,
    DA_CREATE_BOOL,
    DA_CREATE_TYPE_COUNT
};
typedef short DA_CREATE_TYPE;

#define ROTATE_ORDER_XYZ 0
#define ROTATE_ORDER_YZX 1
#define ROTATE_ORDER_ZXY 2
#define ROTATE_ORDER_XZY 3
#define ROTATE_ORDER_YXZ 4
#define ROTATE_ORDER_ZYX 5

// typedef Imath::V3d DPDouble;

// 2D
typedef Imath::V2f DPoint2D;
typedef std::vector<DPoint2D> DPoint2DList;
typedef DPoint2DList::iterator DPoint2DIter;
typedef DPoint2DList::const_iterator ConstDPoint2DIter;

// 3D
typedef Imath::V3f DPoint;
typedef std::vector<DPoint> DPointList;
typedef DPointList::iterator DPointIter;
typedef DPointList::const_iterator ConstDPointIter;

typedef std::shared_ptr<DPointList> DPointListPtr;

// convert point
inline MPoint DPToMP(DPoint p) {
    return MPoint(p[0], p[1], p[2]);
}
inline MPoint DPToMFP(DPoint p) {
    return MFloatPoint(p[0], p[1], p[2]);
}
inline MVector DPToMV(DPoint p) {
    return MVector(p[0], p[1], p[2]);
}
inline DPoint MVToDP(MVector p) {
    return DPoint(p[0], p[1], p[2]);
}
inline DPoint MPToDP(MPoint p) {
    return DPoint(p[0], p[1], p[2]);
}
inline DPoint MFPToDP(MFloatPoint p) {
    return DPoint(p[0], p[1], p[2]);
}

//	typedef std::map<std::string, DPGroupData::Ptr> DPGrpList;

typedef std::vector<size_t> DPIndexList; // index data 0 1 2 3 4 5 etc..
typedef std::vector<bool> DPIdList;      // 0 or 1

// Attribute Type
typedef bool DA_BOOL;
typedef float DA_FLOAT;
typedef double DA_DOUBLE;
typedef int32_t DA_INT;
typedef uint32_t DA_UINT;
typedef int64_t DA_INT64;
typedef Imath::V2i DA_VEC2I;
typedef Imath::V2f DA_VEC2F;
typedef Imath::V2d DA_VEC2D;
typedef Imath::V3i DA_VEC3I;
typedef Imath::V3f DA_VEC3F;
typedef Imath::V3d DA_VEC3D;
typedef std::string DA_STRING;
typedef MMatrix DA_MATRIX;

typedef std::vector<DA_BOOL> DABoolList;
typedef std::vector<DA_FLOAT> DAFloatList;
typedef std::vector<DA_DOUBLE> DADoubleList;
typedef std::vector<DA_INT> DAIntList;
typedef std::vector<DA_UINT> DAUIntList;
typedef std::vector<DA_INT64> DAInt64List;
typedef std::vector<DA_VEC2I> DAVec2iList;
typedef std::vector<DA_VEC2F> DAVec2fList;
typedef std::vector<DA_VEC2D> DAVec2dList;
typedef std::vector<DA_VEC3I> DAVec3iList;
typedef std::vector<DA_VEC3F> DAVec3fList;
typedef std::vector<DA_VEC3D> DAVec3dList;
typedef std::vector<DA_STRING> DAStringList;
typedef std::vector<DA_MATRIX> DAMatrixList;

// color
typedef Imath::V3f DColor;
typedef std::vector<DColor> DColorList;
typedef std::shared_ptr<DColorList> DColorListPtr;

// string
typedef std::string DString;
typedef std::vector<DString> DStringList;
typedef std::shared_ptr<DStringList> DStringListPtr;

// for conversion
typedef float DReal;
typedef float DScalarData;
typedef std::vector<DScalarData> DScalarDataList;
typedef std::shared_ptr<DScalarDataList> DScalarDataListPtr;

typedef Imath::V2f DVec2Data;
typedef std::vector<DVec2Data> DVec2DataList;

typedef Imath::V3f DVec3Data;
// typedef Imath::V3f DVec3fData;

typedef std::vector<DVec3Data> DVec3DataList;
// typedef std::vector<DVec3fData> DVec3fDataList;

#pragma mark Maya Utilities
typedef std::vector<int32_t> DIntArray;
typedef std::vector<float> DFloatArray;
typedef std::vector<double> DDoubleArray;

typedef std::unique_ptr<DIntArray> DIntArrayPtr;
typedef std::unique_ptr<DFloatArray> DFloatArrayPtr;
typedef std::unique_ptr<DDoubleArray> DDoubleArrayPtr;
typedef std::unique_ptr<DVec2DataList> DVec2DataListPtr;
typedef std::unique_ptr<DVec3DataList> DVec3DataListPtr;
// typedef std::shared_ptr<DVec3fDataList> DVec3fDataListPtr;

typedef std::vector<MMatrix> DMatrixArray;
typedef std::shared_ptr<DMatrixArray> DMatrixArrayPtr;
typedef std::shared_ptr<std::vector<std::vector<MMatrix>>> DDMatrixArrayPtr;

typedef std::shared_ptr<std::vector<MQuaternion>> MQuaternionArrayPtr;

typedef std::unique_ptr<MDoubleArray> MDoubleArrayPtr;
typedef std::shared_ptr<MFloatArray> MFloatArrayPtr;
typedef std::shared_ptr<MPointArray> MPointArrayPtr;
typedef std::shared_ptr<MFloatPointArray> MFloatPointArrayPtr;
typedef std::shared_ptr<MIntArray> MIntArrayPtr;
typedef std::shared_ptr<MVectorArray> MVectorArrayPtr;
typedef std::shared_ptr<MMatrixArray> MMatrixArrayPtr;

typedef std::shared_ptr<std::vector<MPointArray>> MMPointArrayPtr;
