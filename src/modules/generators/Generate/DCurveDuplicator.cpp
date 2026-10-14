//
//  DCurveDuplicator.cpp
//  dommyTool
//
//  Created by yuzo on 2016/09/25.
//
//

#include "common/utility/DMayaEntryPointGuard.h"
#include "DCurveDuplicator.h"
#include "common/utility/DStatusOperation.h"

#include <maya/MArrayDataHandle.h>
#include <maya/MDataBlock.h>
#include <maya/MDataHandle.h>
#include <maya/MFloatArray.h>
#include <maya/MFloatPointArray.h>
#include <maya/MFloatVectorArray.h>
#include <maya/MFnData.h>
#include <maya/MFnDependencyNode.h>
#include <maya/MFnEnumAttribute.h>
#include <maya/MFnGenericAttribute.h>
#include <maya/MFnMesh.h>
#include <maya/MFnMeshData.h>
#include <maya/MFnNumericAttribute.h>
#include <maya/MFnNurbsCurve.h>
#include <maya/MFnTypedAttribute.h>
#include <maya/MGlobal.h>
#include <maya/MIntArray.h>
#include <maya/MPlug.h>
#include <maya/MPoint.h>
#include <maya/MQuaternion.h>
#include <maya/MTransformationMatrix.h>
#include <maya/MVector.h>

#include <algorithm>
#include <cmath>
#include <vector>

#include "common/utility/DComputeGuard.h"
#include "common/utility/DGenerationLimits.h"
#include "common/utility/DMayaValueValidation.h"
#include "dupMesh.h"

namespace {

// Keep the legacy float rounding after each transform stage, but reject values
// outside float storage before conversion. Commit only a complete valid point.
bool transformCurveDuplicatePoint(const MFloatPoint& source, const MVector& scale,
		const MVector& rotation, const MQuaternion& yRotation,
		const MQuaternion& xRotation, const MPoint& position,
		const MVector& offset, MFloatPoint& output)
{
	MFloatPoint candidate(source);
	if (!dommy::numeric::checkedFloatCast(static_cast<double>(source.x) * scale[0], candidate.x) ||
		!dommy::numeric::checkedFloatCast(static_cast<double>(source.y) * scale[1], candidate.y) ||
		!dommy::numeric::checkedFloatCast(static_cast<double>(source.z) * scale[2], candidate.z)) return false;
	const double angles[] = {rotation[0], rotation[1], rotation[2]};
	const MVector rotated = MVector(candidate).rotateBy(angles, MTransformationMatrix::kYZX);
	if (!dommy::maya::isFloatRepresentableVector(rotated)) return false;
	candidate.setCast(rotated);
	const MVector aligned = MVector(candidate).rotateBy(yRotation).rotateBy(xRotation);
	if (!dommy::maya::isFloatRepresentableVector(aligned)) return false;
	candidate.setCast(aligned);
	if (!dommy::numeric::checkedFloatCast(candidate.x + (position.x + offset[0]), candidate.x) ||
		!dommy::numeric::checkedFloatCast(candidate.y + (position.y + offset[1]), candidate.y) ||
		!dommy::numeric::checkedFloatCast(candidate.z + (position.z + offset[2]), candidate.z)) return false;
	output = candidate;
	return true;
}

struct DuplicateMeshInput {
	MObject object;
	dupMesh::ImportPlan plan;
};

MStatus validateDuplicateBudget(const std::vector<DuplicateMeshInput>& inputs,
		std::size_t copies, std::size_t inputSlots)
{
	dommy::mesh::MeshMergePlan total;
	std::size_t points = 0U, faces = 0U, corners = 0U, uvs = 0U;
	std::size_t extraBytes = 0U, part = 0U;
	const std::size_t maximum = dommy::mesh::kMaxMeshOperationBytes;
	// Include retained source descriptors, snapshots and per-copy curve frames.
	if (!dommy::generation::checkedProduct(inputSlots,
			sizeof(DuplicateMeshInput) + sizeof(dupMesh), maximum, extraBytes) ||
		!dommy::generation::checkedProduct(copies,
			2U * sizeof(MQuaternion) + sizeof(MPoint), maximum, part) ||
		!dommy::generation::checkedSum(extraBytes, part, maximum, extraBytes))
		return MS::kInvalidParameter;
	for (const DuplicateMeshInput& input : inputs) {
		const auto& geometry = input.plan.geometry;
		if (!dommy::mesh::appendMeshMergePlan(geometry.points, geometry.faces,
				geometry.connections, geometry.uvs, total) ||
			!dommy::generation::checkedSum(extraBytes, input.plan.extraBytes, maximum, extraBytes))
			return MS::kInvalidParameter;
		points = std::max(points, geometry.points);
		faces = std::max(faces, geometry.faces);
		corners = std::max(corners, geometry.connections);
		uvs = std::max(uvs, geometry.uvs);
	}
	// Retain the legacy maximum-source policy. The current node selects source
	// zero, but any future source selection must stay inside this envelope.
	if (!dommy::generation::checkedProduct(points, copies, dommy::generation::kMaxGeneratedPointCount, points) ||
		!dommy::generation::checkedProduct(faces, copies, dommy::generation::kMaxGeneratedFaceCount, faces) ||
		!dommy::generation::checkedProduct(corners, copies, dommy::generation::kMaxWorkItemCount, corners) ||
		!dommy::generation::checkedProduct(uvs, copies, dommy::generation::kMaxWorkItemCount, uvs) ||
		!dommy::mesh::appendMeshMergePlan(points, faces, corners, uvs, total) ||
		!dommy::mesh::validMeshOperationStorage(total, extraBytes)) return MS::kInvalidParameter;
	return MS::kSuccess;
}

MStatus collectDuplicateMeshes(MArrayDataHandle& arrayHandle, std::size_t copies,
		std::vector<dupMesh>& output)
{
	MStatus status;
	const unsigned int count = arrayHandle.elementCount(&status);
	DOMMY_RETURN_IF_FAILURE(status);
	if (count > dommy::generation::kMaxConnectedGeometryCount) return MS::kInvalidParameter;
	std::vector<DuplicateMeshInput> inputs;
	DOMMY_RETURN_IF_FAILURE(validateDuplicateBudget(inputs, copies, count));
	inputs.reserve(count);
	for (unsigned int index = 0U; index < count; ++index) {
		DOMMY_RETURN_IF_FAILURE(arrayHandle.jumpToArrayElement(index));
		MDataHandle handle = arrayHandle.inputValue(&status);
		DOMMY_RETURN_IF_FAILURE(status);
		DuplicateMeshInput input;
		input.object = handle.data();
		if (input.object.isNull()) continue;
		DOMMY_RETURN_IF_FAILURE(dupMesh::preflight(input.object, input.plan));
		inputs.push_back(input);
	}
	if (inputs.empty()) return MS::kInvalidParameter;
	DOMMY_RETURN_IF_FAILURE(validateDuplicateBudget(inputs, copies, count));
	std::vector<dupMesh> candidate;
	candidate.reserve(inputs.size());
	for (const DuplicateMeshInput& input : inputs) {
		candidate.emplace_back(input.object);
		if (!candidate.back().matches(input.plan)) return MS::kInvalidParameter;
	}
	output.swap(candidate);
	return MS::kSuccess;
}

MStatus publishEmptyCurveDuplicateMesh(MDataBlock& data, const MPlug& plug)
{
	MStatus status;
	MFnMeshData meshData;
	const MObject emptyMesh = meshData.create(&status);
	if (status != MS::kSuccess) {
		return status;
	}
	MDataHandle output = data.outputValue(DCurveDuplicator::outMesh, &status);
	if (status != MS::kSuccess) {
		return status;
	}
	status = output.set(emptyMesh);
	return status == MS::kSuccess ? data.setClean(plug) : status;
}

bool isFiniteVector(const MVector& value)
{
	return std::isfinite(value[0]) && std::isfinite(value[1]) &&
	       std::isfinite(value[2]);
}

bool isFinitePoint(const MPoint& value)
{
	return std::isfinite(value.x) && std::isfinite(value.y) &&
	       std::isfinite(value.z) && std::isfinite(value.w);
}

} // namespace



const MTypeId DCurveDuplicator::typeId( 0x00340 );

// Attributes
MObject DCurveDuplicator::inObjects;

MObject DCurveDuplicator::inCurve;
MObject DCurveDuplicator::inTangentCurve;

MObject DCurveDuplicator::duplicateMode;
MObject DCurveDuplicator::count;

MObject DCurveDuplicator::curveLength;
MObject DCurveDuplicator::curveOffset;
MObject DCurveDuplicator::curveStep;

MObject DCurveDuplicator::tangentMode;
MObject DCurveDuplicator::tangentVector;

MObject DCurveDuplicator::offsetTranslate;


MObject DCurveDuplicator::globalRotate;
MObject DCurveDuplicator::globalScale;

MObject DCurveDuplicator::outMesh;



void *DCurveDuplicator::creator()
{
    return ::dommy::maya::createObject("DCurveDuplicator", [&]() {
        return new DCurveDuplicator();
    });
}


MStatus DCurveDuplicator::compute( const MPlug& plug, MDataBlock& data )
{
	try {
			MStatus stat;
	
	
	if ( plug == outMesh )
	{
//		const int duplicateModeVal = data.inputValue(duplicateMode, &stat).asInt();
		const MDataHandle curveLengthValHandle = data.inputValue(curveLength, &stat);
		DOMMY_RETURN_IF_FAILURE(stat);
		const double curveLengthVal = curveLengthValHandle.asDouble();
			const MDataHandle rawCountHandle = data.inputValue(count, &stat);
			DOMMY_RETURN_IF_FAILURE(stat);
			const int rawCount = rawCountHandle.asInt();
			std::size_t countVal = 0;
			if (!dommy::generation::checkedCount(
					rawCount, dommy::generation::kMaxGeneratedPointCount,
					countVal)) {
				MGlobal::displayError(
					"DCurveDuplicator: count exceeds the generation budget.");
				return MS::kFailure;
			}
		const MDataHandle curveOffsetValHandle = data.inputValue(curveOffset, &stat);
		DOMMY_RETURN_IF_FAILURE(stat);
		const double curveOffsetVal = curveOffsetValHandle.asDouble();
//		const double curveStepVal = data.inputValue(curveStep, &stat).asDouble();
		const MDataHandle tangentModeValHandle = data.inputValue(tangentMode, &stat);
		DOMMY_RETURN_IF_FAILURE(stat);
		const int tangentModeVal = tangentModeValHandle.asInt();
		const MDataHandle tangentVecValHandle = data.inputValue(tangentVector, &stat);
		DOMMY_RETURN_IF_FAILURE(stat);
		const double* tangentVecValRaw = tangentVecValHandle.asDouble3();
		if (!tangentVecValRaw) return MS::kFailure;
		const MVector tangentVecVal(tangentVecValRaw);
		
		const MDataHandle offsetTransValHandle = data.inputValue(offsetTranslate, &stat);
		DOMMY_RETURN_IF_FAILURE(stat);
		const double* offsetTransValRaw = offsetTransValHandle.asDouble3();
		if (!offsetTransValRaw) return MS::kFailure;
		const MVector offsetTransVal(offsetTransValRaw);
		const MDataHandle gRotateValHandle = data.inputValue(globalRotate, &stat);
		DOMMY_RETURN_IF_FAILURE(stat);
		const double* gRotateValRaw = gRotateValHandle.asDouble3();
		if (!gRotateValRaw) return MS::kFailure;
		const MVector gRotateVal(gRotateValRaw);
		const MDataHandle gScaleValHandle = data.inputValue(globalScale, &stat);
		DOMMY_RETURN_IF_FAILURE(stat);
		const double* gScaleValRaw = gScaleValHandle.asDouble3();
		if (!gScaleValRaw) return MS::kFailure;
		const MVector gScaleVal(gScaleValRaw);
		
		
				if (!std::isfinite(curveLengthVal) || curveLengthVal < 0.0 ||
					!std::isfinite(curveOffsetVal) || tangentModeVal < 0 ||
					tangentModeVal > 2 ||
				!isFiniteVector(tangentVecVal) || !isFiniteVector(offsetTransVal) ||
				!isFiniteVector(gRotateVal) || !isFiniteVector(gScaleVal)) {
				MGlobal::displayError("DCurveDuplicator: inputs must be finite.");
				return MS::kFailure;
			}
			if (countVal == 0U) {
				return publishEmptyCurveDuplicateMesh(data, plug);
		}
		
#pragma mark input duplicate Mesh 複製するメッシュの取得
		std::vector<dupMesh> dupShapeArray;
		
		MArrayDataHandle arrayHandle = data.inputArrayValue(inObjects, &stat);
			if (stat != MS::kSuccess) return stat;
		DOMMY_RETURN_IF_FAILURE(collectDuplicateMeshes(arrayHandle, countVal, dupShapeArray));
		
#pragma mark thisNode
		MFnDependencyNode thisFn(thisMObject());
		
		
#pragma mark Get input curve 入力カーブの取得
		MDataHandle dataHnd = data.inputValue(inCurve,&stat);
		DOMMY_RETURN_IF_FAILURE(stat);
		MPlug inCrvPlug(thisMObject(), inCurve);
		const bool curveConnected = inCrvPlug.isConnected(&stat);
		DOMMY_RETURN_IF_FAILURE(stat);
		if (!curveConnected) {
			MGlobal::displayWarning("DCurveDuplicator :Input curve is not connected.");
			return MS::kFailure;
		} else if (dataHnd.type() != MFnData::kNurbsCurve) {
			MGlobal::displayWarning("DCurveDuplicator :Input is not curve.");
			return MS::kFailure;
		}
		
		MObject crvObj = dataHnd.data();
		
			MFnNurbsCurve crvFn;
			DOMMY_RETURN_IF_FAILURE(crvFn.setObject(crvObj));
		
#pragma mark Get input object 入力タンジェントカーブの取得
		MPlug tangentPlug(thisMObject(), inTangentCurve);
		
		MFnNurbsCurve tanCrvFn;
		bool useTangent = false;
		const bool tangentConnected = tangentPlug.isConnected(&stat);
		DOMMY_RETURN_IF_FAILURE(stat);
		if (tangentConnected) {
			MDataHandle tanHnd = data.inputValue(inTangentCurve, &stat);
			DOMMY_RETURN_IF_FAILURE(stat);
			if (tanHnd.type() != MFnData::kNurbsCurve) {
				MGlobal::displayWarning("DCurveDuplicator :Input tangent is not curve.");
				return MS::kFailure;
			}
			
				DOMMY_RETURN_IF_FAILURE(tanCrvFn.setObject(tanHnd.data()));
			
			useTangent =true;
		}
		
	#pragma mark Get curve info
		double start, end;
			DOMMY_RETURN_IF_FAILURE(crvFn.getKnotDomain(start, end));
			double length = crvFn.length(kMFnNurbsEpsilon, &stat);
			CHECK_MSTATUS_AND_RETURN_IT(stat);
			if (!std::isfinite(start) || !std::isfinite(end) || end <= start ||
				!std::isfinite(length) || length < 0.0) return MS::kInvalidParameter;
			length *= curveLengthVal;
			if (!std::isfinite(length)) return MS::kInvalidParameter;
		
		const std::size_t totalSampling = countVal;
		double stepLength;
		if (totalSampling==1) {
			stepLength =0.0;
		} else {
			stepLength = length/(double)(totalSampling-1);
		}
		if (!std::isfinite(stepLength)) return MS::kInvalidParameter;
		double tangentStart = 0.0, tangentEnd = 0.0;
		if (useTangent && tangentModeVal == 2) {
			DOMMY_RETURN_IF_FAILURE(tanCrvFn.getKnotDomain(tangentStart, tangentEnd));
			if (!std::isfinite(tangentStart) || !std::isfinite(tangentEnd) ||
				tangentEnd <= tangentStart) return MS::kInvalidParameter;
		}
		
		MVector oldBiNormal;
		
#pragma mark create transform info
		MQuaternion xQuat,yQuat;//,zQuat;
		MVector xAxisVec(1.0,0.0,0.0);
		MVector yAxisVec(0.0,1.0,0.0);
		
			std::vector<MQuaternion> xQuatArray;
			std::vector<MQuaternion> yQuatArray;
			std::vector<MPoint> dupPointArray;
			xQuatArray.reserve(countVal);
			yQuatArray.reserve(countVal);
			dupPointArray.reserve(countVal);
		
		
			for (std::size_t c = 0; c < totalSampling; ++c) {
				MPoint cv;
				const double sampleDistance = stepLength * static_cast<double>(c);
				if (!std::isfinite(sampleDistance) || sampleDistance < 0.0) return MS::kInvalidParameter;
				double curveParam = crvFn.findParamFromLength(sampleDistance, &stat);
				if (stat != MS::kSuccess || !std::isfinite(curveParam)) {
					return MS::kFailure;
				}
				if (curveParam < start || curveParam > end) return MS::kInvalidParameter;
			
			double offsetedParam = curveParam+curveOffsetVal;
			if (!std::isfinite(offsetedParam)) return MS::kInvalidParameter;
			
			offsetedParam = offsetedParam<start?start:offsetedParam;
			offsetedParam = offsetedParam>end?end:offsetedParam;
			
			
				DOMMY_RETURN_IF_FAILURE(
					crvFn.getPointAtParam(offsetedParam, cv, MSpace::kWorld));
				if (!isFinitePoint(cv)) {
					return MS::kFailure;
				}
			
				MVector	normal;
				MVector	tangent = crvFn.tangent(offsetedParam , MSpace::kWorld , &stat);
				CHECK_MSTATUS_AND_RETURN_IT(stat);
			
			if (tangentModeVal==1) {//Simple vector
				normal= MVector(tangentVecVal);
				
//				if (c==0) {
//					normal= MVector(tangentVecVal);
//				} else {
//					normal = oldBiNormal ^ tangent;
//				}
			} else if (tangentModeVal==2 && useTangent) {//input curve
				MPoint targetCV;
				double targetParam = 0.0;
					const MPoint closest = tanCrvFn.closestPoint(
						cv, &targetParam, kMFnNurbsEpsilon, MSpace::kWorld, &stat);
					CHECK_MSTATUS_AND_RETURN_IT(stat);
					if (!isFinitePoint(closest) || !std::isfinite(targetParam) ||
						targetParam < tangentStart || targetParam > tangentEnd) return MS::kInvalidParameter;
					DOMMY_RETURN_IF_FAILURE(
						tanCrvFn.getPointAtParam(
							targetParam, targetCV, MSpace::kWorld));
					normal = targetCV-cv;
			} else {
					if (c==0) {
						normal= crvFn.normal( curveParam , MSpace::kWorld , &stat );
						CHECK_MSTATUS_AND_RETURN_IT(stat);
					} else {
						normal = oldBiNormal ^ tangent;
					}
				}
			
				const double normalLength = normal.length();
				const double tangentLength = tangent.length();
				if (!std::isfinite(normalLength) || !std::isfinite(tangentLength) ||
					normalLength <= kMFnNurbsEpsilon ||
					tangentLength <= kMFnNurbsEpsilon) {
					MGlobal::displayError(
						"DCurveDuplicator: curve frame contains a degenerate vector.");
					return MS::kFailure;
				}
				normal /= normalLength;
				tangent /= tangentLength;
				
				MVector biNormal((tangent ^ normal));//targetZ
				const double biNormalLength = biNormal.length();
				if (!std::isfinite(biNormalLength) ||
					biNormalLength <= kMFnNurbsEpsilon) {
					MGlobal::displayError(
						"DCurveDuplicator: tangent and normal vectors are parallel.");
					return MS::kFailure;
				}
				biNormal /= biNormalLength;
			
			yQuat = yAxisVec.rotateTo(tangent);
			MVector xPostPosVec = xAxisVec.rotateBy(yQuat);
			xQuat = xPostPosVec.rotateTo(normal);
			
			xQuatArray.push_back(xQuat);
			yQuatArray.push_back(yQuat);
			dupPointArray.push_back(cv);
			
			oldBiNormal = biNormal;
		}
		
	#pragma mark poly data init
		
		MFloatPointArray finVertArray;
		MFloatVectorArray finNormalArray;
		//MIntArray finNormCountsArray,finNormIdsArray;
		MFloatArray finUCoords, finVCoords;
		MIntArray finUVIdsArray,finUVCountsArray;
		MIntArray finPolyCountArray,finPolyConnectArray;
		
//		MVectorArray uVecArray,vVecArray;
		
#pragma mark run duplicate
		long currentCount=0;
        long xuv=0;
//        long uCount = 0;
//        long vCount = 0;
		
		for (std::size_t i = 0; i < totalSampling; ++i)
		{
			//set randam input duplicate object
			int type = 0;
//			if (dupShapeArray.size()>1) {
//				type = rand() % dupShapeArray.size();
//			}
			
			
			
#pragma mark Set final vetex List　頂点データのセット
			MFloatPointArray newArray;
			DOMMY_RETURN_IF_FAILURE(newArray.copy(dupShapeArray[type].getVertexArray()));
			
			for(uint p=0;p<newArray.length();p++)
			{
				if (!transformCurveDuplicatePoint(newArray[p], gScaleVal, gRotateVal,
					yQuatArray[i], xQuatArray[i], dupPointArray[i], offsetTransVal, newArray[p])) {
					return MS::kInvalidParameter;
				}
				if (finVertArray.append(newArray[p]) != MS::kSuccess) {
					return MS::kFailure;
				}
			}
			
			
	#pragma mark set Poly Count List Set 頂点数のセット
			const MIntArray& pCList = dupShapeArray[type].getPolyCountsList();
			for(uint p=0;p<pCList.length();p++)
			{
				if (finPolyCountArray.append(pCList[p]) != MS::kSuccess) {
					return MS::kFailure;
				}
			}
			
	#pragma mark set poly connect list　コネクトリストのセット
			const MIntArray& pConeList = dupShapeArray[type].getPolyConnectList();
			for(uint p=0;p<pConeList.length();p++)
			{
				if (finPolyConnectArray.append(pConeList[p] + currentCount) !=
					MS::kSuccess) {
					return MS::kFailure;
				}
			}
			
#pragma mark set UV array　UVのセット
			const MFloatArray& newUCoords = dupShapeArray[type].getUCoords();
			const MFloatArray& newVCoords = dupShapeArray[type].getVCoords();
			
				for(uint p=0;p<newUCoords.length();p++)
				{
							if (finUCoords.append(newUCoords[p]) != MS::kSuccess ||
								finVCoords.append(newVCoords[p]) != MS::kSuccess) {
								return MS::kFailure;
							}
				}
			
//			uCount++;
//			if (uCount>(numUVal-1)) {
//				vCount++;
//				uCount=0;
//			}
			
			const MIntArray& uvCList = dupShapeArray[type].getUVCountsArray();
			const MIntArray& uvIDList = dupShapeArray[type].getUVIdsArray();
			
			for(uint p=0;p<uvIDList.length();p++)
			{
				if (finUVIdsArray.append(uvIDList[p] + xuv) != MS::kSuccess) {
					return MS::kFailure;
				}
			}
			for(uint p=0;p<uvCList.length();p++)
			{
				if (finUVCountsArray.append(uvCList[p]) != MS::kSuccess) {
					return MS::kFailure;
				}
			}
			
			xuv += newUCoords.length();
			
			currentCount += newArray.length();
			
		}
		
		//clear duplicate mesh
		if( dupShapeArray.size() != 0 ) dupShapeArray.clear();
		
        
        
//        if(VERBOSE)cout << "finUCoords count:" << finUCoords.length() <<  endl;
//        if(VERBOSE)cout << "finVCoords count:" << finVCoords.length() <<  endl;
//        
//        if(VERBOSE)cout << "finUCoords :" << finUCoords <<  endl;
//        if(VERBOSE)cout << "finVCoords :" << finVCoords <<  endl;
//		
//        if(VERBOSE)cout << "finUVCountsArray count :" << finUVCountsArray.length() <<  endl;
//        if(VERBOSE)cout << "finUVCountsArray :" << finUVCountsArray <<  endl;
//        if(VERBOSE)cout << "finUVIdsArray count :" << finUVIdsArray.length() <<  endl;
//        if(VERBOSE)cout << "finUVIdsArray :" << finUVIdsArray <<  endl;
        
#pragma mark set Create Output mesh data
			MFnMeshData dataCreator;
			MObject newMeshData = dataCreator.create( &stat );
			CHECK_MSTATUS_AND_RETURN_IT(stat);
//		MFnMesh meshFn;
		MFnMesh newMeshFn;
//		newMeshFn.create(finVertArray.length(), finPolyCountArray.length(), finVertArray, finPolyCountArray, finPolyConnectArray,newMeshData,&stat );
			newMeshFn.create(finVertArray.length(),finPolyCountArray.length(),finVertArray,finPolyCountArray,finPolyConnectArray,finUCoords, finVCoords,newMeshData,&stat);
			CHECK_MSTATUS_AND_RETURN_IT(stat);
        
        
        //aassign UV Id
	        DOMMY_RETURN_IF_FAILURE(
	            newMeshFn.assignUVs(finUVCountsArray,finUVIdsArray));
        
        
        
		MDataHandle outHandle =
			data.outputValue(DCurveDuplicator::outMesh, &stat);
			CHECK_MSTATUS_AND_RETURN_IT(stat);
			DOMMY_RETURN_IF_FAILURE(outHandle.set(newMeshData));
		
		return data.setClean(plug);
		
		} else {
			return MS::kUnknownParameter;
		}
	} DOMMY_COMPUTE_CATCH("DCurveDuplicator")
}


MStatus DCurveDuplicator::initialize() try
{
	MFnNumericAttribute nAttr;
	MFnGenericAttribute gAttr;
	MFnTypedAttribute tAttr;
	MFnEnumAttribute enumFn;
	MStatus stat;
	
	
	inObjects = gAttr.create( "inObjects", "ino", &stat );
    DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(gAttr.addDataAccept( MFnData::kMesh ));
	DOMMY_RETURN_IF_FAILURE(gAttr.setStorable(false));
	DOMMY_RETURN_IF_FAILURE(gAttr.setReadable( false ));
	DOMMY_RETURN_IF_FAILURE(gAttr.setArray( true ));
	DOMMY_RETURN_IF_FAILURE(addAttribute(inObjects));
	
	
	inCurve = gAttr.create( "inCurve", "inc",  &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(gAttr.addDataAccept(MFnData::kNurbsCurve));
	DOMMY_RETURN_IF_FAILURE(gAttr.setStorable(false));
	DOMMY_RETURN_IF_FAILURE(gAttr.setWritable( true ));
	DOMMY_RETURN_IF_FAILURE(gAttr.setReadable( false ));
	DOMMY_RETURN_IF_FAILURE(addAttribute(inCurve));
	
	
	inTangentCurve = gAttr.create( "inTangentCurve", "intc",  &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(gAttr.addDataAccept(MFnData::kNurbsCurve));
	DOMMY_RETURN_IF_FAILURE(gAttr.setStorable(false));
	DOMMY_RETURN_IF_FAILURE(gAttr.setWritable( true ));
	DOMMY_RETURN_IF_FAILURE(gAttr.setReadable( false ));
	DOMMY_RETURN_IF_FAILURE(addAttribute(inTangentCurve));
	
	
	duplicateMode = enumFn.create( "duplicateMode", "dpm", 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(enumFn.addField( "Samplin", 0 ));
	DOMMY_RETURN_IF_FAILURE(enumFn.addField( "Step", 1 ));
	DOMMY_RETURN_IF_FAILURE(enumFn.setKeyable( true ));
	DOMMY_RETURN_IF_FAILURE(addAttribute(duplicateMode));
	
	count = nAttr.create( "count", "co", MFnNumericData::kInt, 0.0, &stat );
    DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 10 ));
	DOMMY_RETURN_IF_FAILURE(nAttr.setMin( 0 ));
	DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
	DOMMY_RETURN_IF_FAILURE(addAttribute(count));
	
	curveLength = nAttr.create( "curveLength", "cl", MFnNumericData::kDouble, 0.0, &stat );
    DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 1.0 ));
	DOMMY_RETURN_IF_FAILURE(nAttr.setMin( 0.0 ));
	DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax( 1.0 ));
	DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
	DOMMY_RETURN_IF_FAILURE(addAttribute(curveLength));
	
	curveOffset = nAttr.create( "curveOffset", "off", MFnNumericData::kDouble, 0.0, &stat );
    DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 0.0 ));
	DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
	DOMMY_RETURN_IF_FAILURE(addAttribute(curveOffset));
	
	curveStep = nAttr.create( "curveStep", "cst", MFnNumericData::kDouble, 0.0, &stat );
    DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 1.0 ));
	DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
	DOMMY_RETURN_IF_FAILURE(addAttribute(curveStep));
	
	
	
	tangentMode = enumFn.create( "tangentMode", "tam", 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(enumFn.addField( "From Curve", 0 ));
	DOMMY_RETURN_IF_FAILURE(enumFn.addField( "Simple Vector", 1 ));
	DOMMY_RETURN_IF_FAILURE(enumFn.addField( "input Curve", 2 ));
	DOMMY_RETURN_IF_FAILURE(enumFn.setKeyable( true ));
	DOMMY_RETURN_IF_FAILURE(addAttribute(tangentMode));
	
	tangentVector = nAttr.create( "tangentVector", "tanvec", MFnNumericData::k3Double, 0.0, &stat );
    DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0,1.0,0.0));
	DOMMY_RETURN_IF_FAILURE(addAttribute(tangentVector));
	
	offsetTranslate = nAttr.create( "offsetTranslate", "offtrans", MFnNumericData::k3Double, 0.0, &stat );
    DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0,0.0,0.0));
	DOMMY_RETURN_IF_FAILURE(addAttribute(offsetTranslate));
	
	globalRotate = nAttr.create( "globalRotate", "grot", MFnNumericData::k3Double, 0.0, &stat );
    DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0,0.0,0.0));
	DOMMY_RETURN_IF_FAILURE(addAttribute(globalRotate));
	
	globalScale = nAttr.create( "globalScale", "gscal", MFnNumericData::k3Double, 0.0, &stat );
    DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1.0,1.0,1.0));
	DOMMY_RETURN_IF_FAILURE(addAttribute(globalScale));
	
	
	
    outMesh = tAttr.create("outMesh", "om", MFnNurbsCurveData::kMesh, MObject::kNullObj, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(tAttr.setWritable(false));
	DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
	DOMMY_RETURN_IF_FAILURE(addAttribute(outMesh));
	
	
	
	DOMMY_RETURN_IF_FAILURE(attributeAffects( inObjects, outMesh ));
	DOMMY_RETURN_IF_FAILURE(attributeAffects( inCurve, outMesh ));
	DOMMY_RETURN_IF_FAILURE(attributeAffects( inTangentCurve, outMesh ));
	DOMMY_RETURN_IF_FAILURE(attributeAffects( duplicateMode, outMesh ));
	DOMMY_RETURN_IF_FAILURE(attributeAffects( count, outMesh ));
	DOMMY_RETURN_IF_FAILURE(attributeAffects( curveLength, outMesh ));
	DOMMY_RETURN_IF_FAILURE(attributeAffects( curveOffset, outMesh ));
	DOMMY_RETURN_IF_FAILURE(attributeAffects( curveStep, outMesh ));
	DOMMY_RETURN_IF_FAILURE(attributeAffects( tangentMode, outMesh ));
	DOMMY_RETURN_IF_FAILURE(attributeAffects( tangentVector, outMesh ));
	DOMMY_RETURN_IF_FAILURE(attributeAffects( offsetTranslate, outMesh ));
	DOMMY_RETURN_IF_FAILURE(attributeAffects( globalRotate, outMesh ));
	DOMMY_RETURN_IF_FAILURE(attributeAffects( globalScale, outMesh ));
	
	
	return MS::kSuccess;
}
catch (...) {
    return dommy::maya::reportEntryPointException(
        "DCurveDuplicator", "initialize", std::current_exception());
}
