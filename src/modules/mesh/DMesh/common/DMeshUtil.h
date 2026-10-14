//
//  DMeshUtil.hpp
//  dommyTool
//
//  Created by yuzo on 2/9/17.
//
//



#pragma once

#include <cstddef>

#include <maya/MDataBlock.h>
#include <maya/MDataHandle.h>

#include "DMeshData.h"
#include "DPAttributeType.h"
#include "common/utility/DAttributeArrayConversion.h"
#include "common/utility/DMayaCheckedArray.h"
#include "common/utility/DMayaAttributeArray.h"
#include <stdexcept>


const DMeshData* getInputDM(const MObject& mesh, MDataBlock& data);
MStatus getValidInputDM(const MObject& mesh,
                        MDataBlock& data,
                        const DMeshData*& inputData);
const DMeshData* getInputDMfromPlug(const MPlug &plg, MObject &obj);
const DMeshData* getInputDMfromHandle(MDataHandle hnd);
//const DMeshData* getInputDM(MObject& data);

enum class DMeshPointSpace { Local, Transformed };

// Display/topology callers retain local coordinates. Output converters must
// explicitly bake the primitive transform, since mesh data carries no matrix.
MStatus generateMMeshFromDmesh(const DMeshInternalData &mesh, MObject &newMesh,
    DMeshPointSpace space = DMeshPointSpace::Local);
MStatus generateMMeshFromDmeshWithUV(const DMeshInternalData &mesh, MObject &newMesh,
    DMeshPointSpace space = DMeshPointSpace::Local);

// Arithmetic world-space vertex centers; the float overload checks narrowing.
MStatus DMGeneratePrimitiveCenterList(const DMeshData &dmdata, DPointList &centerList);
MStatus DMGeneratePrimitiveCenterList(const DMeshData &dmdata, MVectorArray &centerList);

//attribute method
void checkDMeshPointAttribute(const std::string attrName,  DMeshData &outputDMData, bool delAttr);
void checkDMeshAttribute(const std::string attrName,  DMeshData &outputDMData, bool delAttr);

//attribute add
MStatus DMAddAttrScalar(DMeshData &dmdata, const bool &isOutAttr, const bool &isDeleteOrg, const std::string &outAttrName, const std::string &originalAttrName, const DScalarDataList &result);
MStatus DMAddAttrVec2(DMeshData &dmdata, const bool &isOutAttr, const bool &isDeleteOrg, const std::string &outAttrName, const std::string &originalAttrName, const DVec2DataList &result);
MStatus DMAddAttrVec3(DMeshData &dmdata, const bool &isOutAttr, const bool &isDeleteOrg, const std::string &outAttrName, const std::string &originalAttrName, const DVec3DataList &result);

MStatus DMPAddAttrScalar(DMItem &mesh, const bool &isOutAttr, const bool &isDeleteOrg, const std::string &outAttrName, const std::string &originalAttrName, const  DScalarDataList &result);
MStatus DMPAddAttrVec2(DMItem &mesh, const bool &isOutAttr, const bool &isDeleteOrg, const std::string &outAttrName, const std::string &originalAttrName, const DVec2DataList &result);
MStatus DMPAddAttrVec3(DMItem &mesh, const bool &isOutAttr, const bool &isDeleteOrg, const std::string &outAttrName, const std::string &originalAttrName, const DVec3DataList &result);





#pragma mark DMesh prim attribute method

template <typename T>
inline void getDMStringList(const DMeshData &inputData, const std::string &name, DStringList &newValues)
{
    std::vector<T> srcList = inputData.attrListValue<T>(name);
	
	newValues.resize(srcList.size());
	
    for (std::size_t i=0; i<srcList.size(); i++) {
        newValues[i] = srcList[i];
    }
}

template <typename T>
inline void getDMMatrixList(const DMeshData &inputData, const std::string &name, DMatrixArray &newValues)
{
    std::vector<T> srcList = inputData.attrListValue<T>(name);
	
	newValues.resize(srcList.size());
	
    for (std::size_t i=0; i<srcList.size(); i++) {
        newValues[i] = srcList[i];
    }
}
template <typename T>
inline void getDMVectorList(const DMeshData &inputData, const std::string &name, DVec3DataList &newValues)
{
    std::vector<T> srcList = inputData.attrListValue<T>(name);
	
	newValues.resize(srcList.size());
	
    for (std::size_t i=0; i<srcList.size(); i++) {
        T val= srcList[i];
        newValues[i] = DVec3Data(val[0],val[1],val[2]);
    }
}

template <typename T>
inline void getDM2DVectorList(const DMeshData &inputData, const std::string &name, DVec2DataList &newValues)
{
    std::vector<T> srcList = inputData.attrListValue<T>(name);
	
	newValues.resize(srcList.size());
	
//    newValues(srcList.begin(),srcList.end());
    
    for (std::size_t i=0; i<srcList.size(); i++) {
        T val= srcList[i];
        newValues[i] = DVec2Data(val[0],val[1]);
    }
}


#pragma mark DMesh prim attribute vec1
template <typename T>
inline void getDMSingleValueToFloat(const DMeshData &inputData, const std::string &name, MDoubleArray &finValues)
{
    dommy::attribute::requireArrayStatus(
        dommy::attribute::copyTypedMayaArray<T, double>(
            inputData.getAttr(name), inputData.numberOfMesh(), finValues));
}

template <typename T>
inline void getDMSingleValueToFloat(const DMeshData &inputData, const std::string &name, DScalarDataList &finValues)
{
    dommy::attribute::requireTypedArray<T>(
        inputData.getAttr(name), inputData.numberOfMesh(), finValues);
}



#pragma mark DMesh prim attribute vec2
template <typename T>
inline void getDMTwoArray(const DMeshData &inputData, const std::string &name, MDoubleArray &firstArray, MDoubleArray &secondArray)
{
    dommy::attribute::requireArrayStatus(
        dommy::attribute::copyTypedMayaUvArrays<T>(
            inputData.getAttr(name), inputData.numberOfMesh(), firstArray, secondArray));
}


template <typename T>
inline void getDMVec2ToVectorArray(const DMeshData &inputData, const std::string &name, DVec2DataList &vecArray)
{
    dommy::attribute::requireTypedArray<T>(
        inputData.getAttr(name), inputData.numberOfMesh(), vecArray);
}

template <typename T>
inline void getDMVec2ToVectorArray(const DMeshData &inputData, const std::string &name, MVectorArray &vecArray)
{
    dommy::attribute::requireArrayStatus(
        dommy::attribute::copyTypedMayaArray<T, Imath::V3d>(
            inputData.getAttr(name), inputData.numberOfMesh(),
            [](const Imath::V3d& value) { return MVector(value[0], value[1], value[2]); },
            vecArray));
}


#pragma mark DMesh prim attribute vec3
template <typename T>
inline void getDMVec3ToVectorArray(const DMeshData& inputData, const std::string& name, DVec3DataList& vecArray)
{
    if (!dommy::attribute::convertTypedArray<T>(inputData.getAttr(name),
            inputData.numberOfMesh(), vecArray))
        throw std::invalid_argument("Invalid primitive Vec3 attribute");
}

template <typename T>
inline void getDMVec3ToVectorArray(const DMeshData& inputData, const std::string& name, MVectorArray& vecArray)
{
    std::vector<Imath::V3d> values;
    if (!dommy::attribute::convertTypedArray<T>(inputData.getAttr(name),
            inputData.numberOfMesh(), values))
        throw std::invalid_argument("Invalid primitive Vec3 attribute");
    const MStatus status = dommy::maya::buildCheckedArray(values.size(),
        [&](unsigned int i, MVector& value) -> MStatus {
            value = MVector(values[i][0], values[i][1], values[i][2]);
            return MS::kSuccess;
        }, vecArray);
    if (!status) throw std::runtime_error("Unable to construct primitive vector array");
}


#pragma mark ----------------------------------------
#pragma mark DMesh Point attribute method

template <typename T>
inline void getDMPStringList(const DMItem &mesh, const std::string &name, std::vector<std::string> &newValues)
{
    std::vector<T> srcList = (*mesh).attrListValue<T>(name);
	
	newValues.resize(srcList.size());
	
    for (std::size_t i=0; i<srcList.size(); i++) {
        newValues[i] = srcList[i];
    }
}


template <typename T>
inline void getDMPMatrixList(const DMItem &mesh, const std::string &name, std::vector<MMatrix> &newValues)
{
    std::vector<T> srcList = (*mesh).attrListValue<T>(name);
	
	newValues.resize(srcList.size());
	
    for (std::size_t i=0; i<srcList.size(); i++) {
        newValues[i] = srcList[i];
    }
}
template <typename T>
inline void getDMPVectorList(const DMItem &mesh, const std::string &name, DVec3DataList &newValues)
{
    std::vector<T> srcList = (*mesh).attrListValue<T>(name);
	
	newValues.resize(srcList.size());
	
    for (std::size_t i=0; i<srcList.size(); i++) {
        T val= srcList[i];
        newValues[i] = DVec3Data(val[0],val[1],val[2]);
    }
}

template <typename T>
inline void getDMP2DVectorList(const DMItem &mesh, const std::string &name, DVec2DataList &newValues)
{
    std::vector<T> srcList = (*mesh).attrListValue<T>(name);
	
	newValues.resize(srcList.size());
	
    for (std::size_t i=0; i<srcList.size(); i++) {
        T val= srcList[i];
        newValues[i] = DVec2Data(val[0],val[1]);
    }
}


#pragma mark DMesh Point attribute vec1
template <typename T>
inline void getDMPSingleValueToFloat(const DMItem &mesh, const std::string &name, MDoubleArray &finValues)
{
    if (!mesh) throw std::invalid_argument("Null mesh attribute source");
    dommy::attribute::requireArrayStatus(
        dommy::attribute::copyTypedMayaArray<T, double>(
            mesh->getAttr(name), mesh->numberOfPoints(), finValues));
}

template <typename T>
inline void getDMPSingleValueToFloat(const DMItem &mesh, const std::string &name, DScalarDataList &finValues)
{
    if (!mesh) throw std::invalid_argument("Null mesh attribute source");
    dommy::attribute::requireTypedArray<T>(
        mesh->getAttr(name), mesh->numberOfPoints(), finValues);
}



#pragma mark DMesh Point attribute vec2
template <typename T>
inline void getDMPTwoArray(const DMItem &mesh, const std::string &name, MDoubleArray &firstArray, MDoubleArray &secondArray)
{
    if (!mesh) throw std::invalid_argument("Null mesh attribute source");
    dommy::attribute::requireArrayStatus(
        dommy::attribute::copyTypedMayaUvArrays<T>(
            mesh->getAttr(name), mesh->numberOfPoints(), firstArray, secondArray));
}

template <typename T>
inline void getDMPVec2ToVectorArray(const DMItem &mesh, const std::string &name, DVec2DataList &vecArray)
{
    if (!mesh) throw std::invalid_argument("Null mesh attribute source");
    dommy::attribute::requireTypedArray<T>(
        mesh->getAttr(name), mesh->numberOfPoints(), vecArray);
}

template <typename T>
inline void getDMPVec2ToVectorArray(const DMItem &mesh, const std::string &name, MVectorArray &vecArray)
{
    if (!mesh) throw std::invalid_argument("Null mesh attribute source");
    dommy::attribute::requireArrayStatus(
        dommy::attribute::copyTypedMayaArray<T, Imath::V3d>(
            mesh->getAttr(name), mesh->numberOfPoints(),
            [](const Imath::V3d& value) { return MVector(value[0], value[1], value[2]); },
            vecArray));
}

#pragma mark DMesh Point attribute vec3
template <typename T>
inline void getDMPVec3ToVectorArray(const DMItem &mesh, const std::string &name, DVec3DataList &vecArray)
{
    if (!mesh) throw std::invalid_argument("Null mesh attribute source");
    dommy::attribute::requireTypedArray<T>(
        mesh->getAttr(name), mesh->numberOfPoints(), vecArray);
}

template <typename T>
inline void getDMPVec3ToVectorArray(const DMItem &mesh, const std::string &name, MVectorArray &vecArray)
{
    if (!mesh) throw std::invalid_argument("Null mesh attribute source");
    dommy::attribute::requireArrayStatus(
        dommy::attribute::copyTypedMayaArray<T, Imath::V3d>(
            mesh->getAttr(name), mesh->numberOfPoints(),
            [](const Imath::V3d& value) { return MVector(value[0], value[1], value[2]); },
            vecArray));
}

#pragma mark DMesh prim attribute useful method


inline void getDMAttributeMDoubleArrayFromScalar(const DMeshData &dmdata, std::string name, MDoubleArray &newArray) {
	
	if (dmdata.isExistAttr(name)) {
		
		DP_ATTR_DATA_TYPE type = dmdata.getAttr(name)->getAttrType();
		
		if (isSingleRealAttrType(type)) {
			
			if (type == DP_ATTR_FLOAT) {
                getDMSingleValueToFloat<float>(dmdata, name, newArray);
			} else if (type == DP_ATTR_DOUBLE) {
                getDMSingleValueToFloat<double>(dmdata, name, newArray);
			} else if (type == DP_ATTR_INT) {
                getDMSingleValueToFloat<int32_t>(dmdata, name, newArray);
			} else if (type == DP_ATTR_UINT32) {
                getDMSingleValueToFloat<uint32_t>(dmdata, name, newArray);
			} else if (type == DP_ATTR_INT64) {
                getDMSingleValueToFloat<int64_t>(dmdata, name, newArray);
			}
		}
	}
}

inline void getDMAttributeMDoubleArrayFromScalar(const DMeshData &dmdata, std::string name, DScalarDataList &newArray) {
	
	if (dmdata.isExistAttr(name)) {
		
		DP_ATTR_DATA_TYPE type = dmdata.getAttr(name)->getAttrType();
		
		if (isSingleRealAttrType(type)) {
			
			if (type == DP_ATTR_FLOAT) {
                getDMSingleValueToFloat<float>(dmdata, name, newArray);
			} else if (type == DP_ATTR_DOUBLE) {
                getDMSingleValueToFloat<double>(dmdata, name, newArray);
			} else if (type == DP_ATTR_INT) {
                getDMSingleValueToFloat<int32_t>(dmdata, name, newArray);
			} else if (type == DP_ATTR_UINT32) {
                getDMSingleValueToFloat<uint32_t>(dmdata, name, newArray);
			} else if (type == DP_ATTR_INT64) {
                getDMSingleValueToFloat<int64_t>(dmdata, name, newArray);
			}
		}
	}
}




inline void getDMAttributeMDoubleArrayFromVec2(const DMeshData &dmdata, std::string &name,
											   MDoubleArray &valuesA, MDoubleArray &valuesB) {
	
	if (dmdata.isExistAttr(name)) {
		
		DP_ATTR_DATA_TYPE type = dmdata.getAttr(name)->getAttrType();
		
		if (isVector2DAttrType(type)) {
			
			if (type == DP_ATTR_VEC2I) {
                getDMTwoArray<Imath::V2i>(dmdata, name, valuesA,valuesB);
			} else if (type == DP_ATTR_VEC2F) {
                getDMTwoArray<Imath::V2f>(dmdata, name, valuesA,valuesB);
			} else if (type == DP_ATTR_VEC2D) {
                getDMTwoArray<Imath::V2d>(dmdata, name, valuesA,valuesB);
			}
		}
	}
}


inline void getDMAttributeMVectorArrayFromVec2(const DMeshData &dmdata, std::string name, MVectorArray &newArray) {
    
    if (dmdata.isExistAttr(name)) {
        
        DP_ATTR_DATA_TYPE type = dmdata.getAttr(name)->getAttrType();
        
        if (isVector2DAttrType(type)) {
            
            if (type == DP_ATTR_VEC2I) {
                getDMVec2ToVectorArray<Imath::V2i>(dmdata, name, newArray);
            } else if (type == DP_ATTR_VEC2F) {
                getDMVec2ToVectorArray<Imath::V2f>(dmdata, name, newArray);
            } else if (type == DP_ATTR_VEC2D) {
                getDMVec2ToVectorArray<Imath::V2d>(dmdata, name, newArray);
            }
        }
    }
}

inline void getDMAttributeMVectorArrayFromVec2(const DMeshData &dmdata, std::string name, DVec2DataList &newArray) {
	
	if (dmdata.isExistAttr(name)) {
		
		DP_ATTR_DATA_TYPE type = dmdata.getAttr(name)->getAttrType();
		
		if (isVector2DAttrType(type)) {
			
			if (type == DP_ATTR_VEC2I) {
                getDMVec2ToVectorArray<Imath::V2i>(dmdata, name, newArray);
			} else if (type == DP_ATTR_VEC2F) {
                getDMVec2ToVectorArray<Imath::V2f>(dmdata, name, newArray);
			} else if (type == DP_ATTR_VEC2D) {
                getDMVec2ToVectorArray<Imath::V2d>(dmdata, name, newArray);
			}
		}
	}
}





inline void getDMAttributeMVectorArrayFromVec3(const DMeshData& dmdata, std::string name, MVectorArray& newArray)
{
    const auto attribute = dmdata.getAttr(name);
    if (!attribute) throw std::invalid_argument("Missing primitive Vec3 attribute");
    switch (attribute->getAttrType()) {
    case DP_ATTR_VEC3I: return getDMVec3ToVectorArray<Imath::V3i>(dmdata, name, newArray);
    case DP_ATTR_VEC3F: return getDMVec3ToVectorArray<Imath::V3f>(dmdata, name, newArray);
    case DP_ATTR_VEC3D: return getDMVec3ToVectorArray<Imath::V3d>(dmdata, name, newArray);
    default: throw std::invalid_argument("Primitive attribute must be Vec3");
    }
}

inline void getDMAttributeMVectorArrayFromVec3(const DMeshData& dmdata, std::string name, DVec3DataList& newArray)
{
    const auto attribute = dmdata.getAttr(name);
    if (!attribute) throw std::invalid_argument("Missing primitive Vec3 attribute");
    switch (attribute->getAttrType()) {
    case DP_ATTR_VEC3I: return getDMVec3ToVectorArray<Imath::V3i>(dmdata, name, newArray);
    case DP_ATTR_VEC3F: return getDMVec3ToVectorArray<Imath::V3f>(dmdata, name, newArray);
    case DP_ATTR_VEC3D: return getDMVec3ToVectorArray<Imath::V3d>(dmdata, name, newArray);
    default: throw std::invalid_argument("Primitive attribute must be Vec3");
    }
}


#pragma mark DMesh Point Useful



inline void getDMPAttributeMDoubleArrayFromScalar(const DMItem &msh, std::string name, MDoubleArray &newArray) {
	
	if (msh->isExistAttr(name)) {
		
		DP_ATTR_DATA_TYPE type = msh->getAttr(name)->getAttrType();
		
		if (isSingleRealAttrType(type)) {
			
			if (type == DP_ATTR_FLOAT) {
				getDMPSingleValueToFloat<float>(msh, name, newArray);
			} else if (type == DP_ATTR_DOUBLE) {
				getDMPSingleValueToFloat<double>(msh, name, newArray);
			} else if (type == DP_ATTR_INT) {
				getDMPSingleValueToFloat<int32_t>(msh, name, newArray);
			} else if (type == DP_ATTR_UINT32) {
				getDMPSingleValueToFloat<uint32_t>(msh, name, newArray);
			} else if (type == DP_ATTR_INT64) {
				getDMPSingleValueToFloat<int64_t>(msh, name, newArray);
			}
			
		}
	}
}

inline void getDMPAttributeFromScalar(const DMItem &msh, std::string name, DScalarDataList &newArray) {
	
	if (msh->isExistAttr(name)) {
		
		DP_ATTR_DATA_TYPE type = msh->getAttr(name)->getAttrType();
		
		if (isSingleRealAttrType(type)) {
			
			if (type == DP_ATTR_FLOAT) {
				getDMPSingleValueToFloat<float>(msh, name, newArray);
			} else if (type == DP_ATTR_DOUBLE) {
				getDMPSingleValueToFloat<double>(msh, name, newArray);
			} else if (type == DP_ATTR_INT) {
				getDMPSingleValueToFloat<int32_t>(msh, name, newArray);
			} else if (type == DP_ATTR_UINT32) {
				getDMPSingleValueToFloat<uint32_t>(msh, name, newArray);
			} else if (type == DP_ATTR_INT64) {
				getDMPSingleValueToFloat<int64_t>(msh, name, newArray);
			}
			
		}
	}
}


inline void getDMPAttributeMDoubleArrayFromVec2(const DMItem &msh, std::string &name,
												MDoubleArray &valuesA, MDoubleArray &valuesB) {
	
	if (msh->isExistAttr(name)) {
		
		DP_ATTR_DATA_TYPE type = msh->getAttr(name)->getAttrType();
		
		if (isVector2DAttrType(type)) {
			
			if (type == DP_ATTR_VEC2I) {
				getDMPTwoArray<Imath::V2i>(msh, name, valuesA,valuesB);
			} else if (type == DP_ATTR_VEC2F) {
				getDMPTwoArray<Imath::V2f>(msh, name, valuesA,valuesB);
			} else if (type == DP_ATTR_VEC2D) {
				getDMPTwoArray<Imath::V2d>(msh, name, valuesA,valuesB);
			}
			
		}
	}
}


inline void getDMPAttributeMVectorArrayFromVec2(const DMItem &msh, std::string name, MVectorArray &newArray) {
    
    if (msh->isExistAttr(name)) {
        
        DP_ATTR_DATA_TYPE type = msh->getAttr(name)->getAttrType();
        
        if (isVector2DAttrType(type)) {
            
            if (type == DP_ATTR_VEC2I) {
                getDMPVec2ToVectorArray<Imath::V2i>(msh, name, newArray);
            } else if (type == DP_ATTR_VEC2F) {
                getDMPVec2ToVectorArray<Imath::V2f>(msh, name, newArray);
            } else if (type == DP_ATTR_VEC2D) {
                getDMPVec2ToVectorArray<Imath::V2d>(msh, name, newArray);
            }
        }
    }
}

inline void getDMPAttributeMVectorArrayFromVec2(const DMItem &msh, std::string name, DVec2DataList &newArray) {
	
	if (msh->isExistAttr(name)) {
		
		DP_ATTR_DATA_TYPE type = msh->getAttr(name)->getAttrType();
		
		if (isVector2DAttrType(type)) {
			
			if (type == DP_ATTR_VEC2I) {
				getDMPVec2ToVectorArray<Imath::V2i>(msh, name, newArray);
			} else if (type == DP_ATTR_VEC2F) {
				getDMPVec2ToVectorArray<Imath::V2f>(msh, name, newArray);
			} else if (type == DP_ATTR_VEC2D) {
				getDMPVec2ToVectorArray<Imath::V2d>(msh, name, newArray);
			}
		}
	}
}


inline void getDMPAttributeMVectorArrayFromVec3(const DMItem &msh, std::string name, MVectorArray &newArray) {
	
	if (msh->isExistAttr(name)) {
		DP_ATTR_DATA_TYPE type = msh->getAttr(name)->getAttrType();
		
		if (isVector3DAttrType(type)) {
			
			if (type == DP_ATTR_VEC3F) {
				getDMPVec3ToVectorArray<Imath::V3f>(msh, name, newArray);
			} else if (type == DP_ATTR_VEC3D) {
				getDMPVec3ToVectorArray<Imath::V3d>(msh, name, newArray);
			} else if (type == DP_ATTR_VEC3I) {
				getDMPVec3ToVectorArray<Imath::V3i>(msh, name, newArray);
			}
		}
	}
}

inline void getDMPAttributeMVectorArrayFromVec3(const DMItem &msh, std::string name, DVec3DataList &newArray) {
	
	if (msh->isExistAttr(name)) {
		DP_ATTR_DATA_TYPE type = msh->getAttr(name)->getAttrType();
		
		if (isVector3DAttrType(type)) {
			
			if (type == DP_ATTR_VEC3F) {
				getDMPVec3ToVectorArray<Imath::V3f>(msh, name, newArray);
			} else if (type == DP_ATTR_VEC3D) {
				getDMPVec3ToVectorArray<Imath::V3d>(msh, name, newArray);
			} else if (type == DP_ATTR_VEC3I) {
				getDMPVec3ToVectorArray<Imath::V3i>(msh, name, newArray);
			}
		}
	}
}



#pragma mark group method


void getDMeshMatchedGroups(const DMeshData &dmesh, const std::string& patternStr, std::vector<std::string> &matchedGrpNames);
