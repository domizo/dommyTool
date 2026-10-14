
#include "DRandomGenerator.h"
#include "common/utility/DMayaEntryPointGuard.h"
#include "common/utility/DStatusOperation.h"
#include "surfaceDuplicater.h"
#include "DLegacyDuplicaterTypeIds.h"
#include "common/utility/DMayaCheckedArray.h"
#include "common/utility/DEvaluationContext.h"
#include "common/utility/DMayaValueValidation.h"
#include "common/utility/DUvParameter.h"
#include "DGlobalTex.h"

#include <maya/MPlug.h>
#include <maya/MPlugArray.h>
#include <maya/MDataBlock.h>
#include <maya/MDataHandle.h>
#include <maya/MFnDependencyNode.h>
#include <maya/MFnGenericAttribute.h>
#include <maya/MArrayDataHandle.h>
#include <maya/MFnNumericAttribute.h>
#include <maya/MFnEnumAttribute.h>

#include <maya/MPointArray.h>
#include <maya/MFloatVector.h>
#include <maya/MFloatArray.h>
#include <maya/MFloatPointArray.h>
#include <maya/MFloatVectorArray.h>
#include <maya/MMatrix.h>
#include <maya/MQuaternion.h>
#include <maya/MFnMatrixAttribute.h>
#include <maya/MTime.h>

#include <maya/MFnMesh.h>
#include <maya/MFnMeshData.h>

#include <maya/MFnNurbsSurface.h>

#include <maya/MFnAnimCurve.h>
#include <maya/MAnimControl.h>
#include <maya/MFnMatrixData.h>


#include <maya/MThreadUtils.h>

#include <algorithm>
#include <cmath>
#include <math.h>

#include "common/utility/DComputeGuard.h"
#include "common/utility/DGenerationLimits.h"


const MTypeId surfaceDuplicater::typeId(ID_LEGACY_SURFACE_DUPLICATER);

namespace {

MStatus connectedSourceNode(const MPlug& destinationPlug,
                            bool& connected,
                            MFnDependencyNode& sourceNode)
{
	MStatus status;
	MPlugArray sources;
	connected = destinationPlug.connectedTo(
		sources, true, false, &status);
	if (!status) return status;
	if (!connected) return MS::kSuccess;
	if (sources.length() == 0U) return MS::kFailure;
	return sourceNode.setObject(sources[0].node());
}

bool validTextureSamples(const bool connected,
                         const MVectorArray& samples,
                         const unsigned int expectedCount)
{
	if (!connected) return true;
	if (samples.length() != expectedCount) return false;
	for (unsigned int index = 0U; index < samples.length(); ++index) {
		if (!dommy::maya::isFiniteVector(samples[index])) return false;
	}
	return true;
}

bool validGeneratedMeshArrays(
	const MFloatPointArray& points,
	const MFloatArray& uCoordinates,
	const MFloatArray& vCoordinates,
	const MIntArray& polygonCounts,
	const MIntArray& polygonConnections,
	const MIntArray& uvCounts,
	const MIntArray& uvIds,
	const std::size_t maximumVertexCount,
	const std::size_t maximumFaceCount)
{
	if (points.length() > maximumVertexCount ||
		polygonCounts.length() > maximumFaceCount ||
		uCoordinates.length() != vCoordinates.length() ||
		uvCounts.length() != polygonCounts.length()) {
		return false;
	}

	for (unsigned int index = 0U; index < points.length(); ++index) {
		const MFloatPoint& point = points[index];
		if (!std::isfinite(point.x) || !std::isfinite(point.y) ||
			!std::isfinite(point.z) || !std::isfinite(point.w)) {
			return false;
		}
	}
	for (unsigned int index = 0U; index < uCoordinates.length(); ++index) {
		if (!std::isfinite(uCoordinates[index]) ||
			!std::isfinite(vCoordinates[index])) {
			return false;
		}
	}

	std::size_t connectionCount = 0U;
	std::size_t uvIdCount = 0U;
	for (unsigned int face = 0U; face < polygonCounts.length(); ++face) {
		if (polygonCounts[face] < 0 || uvCounts[face] < 0 ||
			!dommy::generation::checkedSum(
				connectionCount,
				static_cast<std::size_t>(polygonCounts[face]),
				dommy::generation::kMaxWorkItemCount,
				connectionCount) ||
			!dommy::generation::checkedSum(
				uvIdCount, static_cast<std::size_t>(uvCounts[face]),
				dommy::generation::kMaxWorkItemCount, uvIdCount)) {
			return false;
		}
	}
	if (connectionCount != polygonConnections.length() ||
		uvIdCount != uvIds.length()) {
		return false;
	}

	for (unsigned int index = 0U;
		 index < polygonConnections.length(); ++index) {
		if (polygonConnections[index] < 0 ||
			static_cast<unsigned int>(polygonConnections[index]) >=
				points.length()) {
			return false;
		}
	}
	for (unsigned int index = 0U; index < uvIds.length(); ++index) {
		if (uvIds[index] < 0 ||
			static_cast<unsigned int>(uvIds[index]) >=
				uCoordinates.length()) {
			return false;
		}
	}
	return true;
}

} // namespace

void *surfaceDuplicater::creator()
{
    return ::dommy::maya::createObject("surfaceDuplicater", [&]() {
        return new surfaceDuplicater();
    });
}


MObject surfaceDuplicater::duplicateObjects;
MObject surfaceDuplicater::typeSeed;
MObject surfaceDuplicater::sourcePlane;
MObject surfaceDuplicater::numU;
MObject surfaceDuplicater::numV;

MObject surfaceDuplicater::stepU;
MObject surfaceDuplicater::stepV;

MObject surfaceDuplicater::randamU;
MObject surfaceDuplicater::randamV;

MObject surfaceDuplicater::aimNormal;
MObject surfaceDuplicater::normalSet;

MObject surfaceDuplicater::offsetU;
MObject surfaceDuplicater::offsetV;

MObject surfaceDuplicater::tiltU;
MObject surfaceDuplicater::tiltV;


MObject surfaceDuplicater::tiltColor;
MObject surfaceDuplicater::tiltColorR;
MObject surfaceDuplicater::tiltColorG;
MObject surfaceDuplicater::tiltColorB;


MObject surfaceDuplicater::randamRotX;
MObject surfaceDuplicater::randamRotY;
MObject surfaceDuplicater::randamRotZ;

MObject surfaceDuplicater::scaleX;
MObject surfaceDuplicater::scaleY;
MObject surfaceDuplicater::scaleZ;

MObject surfaceDuplicater::randamScaleX;
MObject surfaceDuplicater::randamScaleY;
MObject surfaceDuplicater::randamScaleZ;

MObject surfaceDuplicater::baldnessColor;
MObject surfaceDuplicater::baldnessColorR;
MObject surfaceDuplicater::baldnessColorG;
MObject surfaceDuplicater::baldnessColorB;

MObject surfaceDuplicater::globalHeightOffset;
MObject surfaceDuplicater::heightColor;
MObject surfaceDuplicater::heightColorR;
MObject surfaceDuplicater::heightColorG;
MObject surfaceDuplicater::heightColorB;

MObject surfaceDuplicater::scaleColorX;
MObject surfaceDuplicater::scaleColorY;
MObject surfaceDuplicater::scaleColorZ;
MObject surfaceDuplicater::scaleColor;
MObject surfaceDuplicater::scaleColorR;
MObject surfaceDuplicater::scaleColorG;
MObject surfaceDuplicater::scaleColorB;

MObject surfaceDuplicater::UVsize;
MObject surfaceDuplicater::minU;
MObject surfaceDuplicater::maxU;
MObject surfaceDuplicater::minV;
MObject surfaceDuplicater::maxV;
MObject surfaceDuplicater::UVType;

MObject surfaceDuplicater::animMatrix;
MObject surfaceDuplicater::timeMapColor;
MObject surfaceDuplicater::timeMapColorR;
MObject surfaceDuplicater::timeMapColorG;
MObject surfaceDuplicater::timeMapColorB;

MObject surfaceDuplicater::timeOffsetMapColor;
MObject surfaceDuplicater::timeOffsetMapColorR;
MObject surfaceDuplicater::timeOffsetMapColorG;
MObject surfaceDuplicater::timeOffsetMapColorB;

MObject surfaceDuplicater::outPutMesh;


MStatus surfaceDuplicater::compute( const MPlug& plug, MDataBlock& block )
{
	try {
		MStatus stat;
	
	if( plug == outPutMesh )
	{

		
		// Scratch data belongs to this evaluation, including failure paths.
		MFloatPointArray colPoints;
		MFloatArray colUCoords, colVCoords;
		MFloatArray preUCoords, preVCoords;
		
#pragma mark get attribute value
		const MDataHandle typeSeedHandle = block.inputValue(typeSeed, &stat);
		CHECK_MSTATUS_AND_RETURN_IT(stat);
		int typeSeedVal = typeSeedHandle.asInt();
		const MDataHandle numUHandle = block.inputValue(numU, &stat);
		CHECK_MSTATUS_AND_RETURN_IT(stat);
		int numUVal = numUHandle.asInt();
		const MDataHandle numVHandle = block.inputValue(numV, &stat);
		CHECK_MSTATUS_AND_RETURN_IT(stat);
		int numVVal = numVHandle.asInt();
		
		const MDataHandle stepUHandle = block.inputValue(stepU, &stat);
		CHECK_MSTATUS_AND_RETURN_IT(stat);
		double stepUVal = stepUHandle.asDouble();
		const MDataHandle stepVHandle = block.inputValue(stepV, &stat);
		CHECK_MSTATUS_AND_RETURN_IT(stat);
		double stepVVal = stepVHandle.asDouble();
		
		const MDataHandle randamUHandle = block.inputValue(randamU, &stat);
		CHECK_MSTATUS_AND_RETURN_IT(stat);
		double randamUVal = randamUHandle.asDouble();
		const MDataHandle randamVHandle = block.inputValue(randamV, &stat);
		CHECK_MSTATUS_AND_RETURN_IT(stat);
		double randamVVal = randamVHandle.asDouble();
		
		const MDataHandle aimNormalHandle = block.inputValue(aimNormal, &stat);
		CHECK_MSTATUS_AND_RETURN_IT(stat);
		bool aimNormalVal = aimNormalHandle.asBool();
		const MDataHandle normalSetHandle = block.inputValue(normalSet, &stat);
		CHECK_MSTATUS_AND_RETURN_IT(stat);
		short normalSetVal = normalSetHandle.asShort();
		
		const MDataHandle offsetUHandle = block.inputValue(offsetU, &stat);
		CHECK_MSTATUS_AND_RETURN_IT(stat);
		double offsetUVal = offsetUHandle.asDouble();
		const MDataHandle offsetVHandle = block.inputValue(offsetV, &stat);
		CHECK_MSTATUS_AND_RETURN_IT(stat);
		double offsetVVal = offsetVHandle.asDouble();
		
		const MDataHandle tiltUHandle = block.inputValue(tiltU, &stat);
		CHECK_MSTATUS_AND_RETURN_IT(stat);
		double tiltUVal = tiltUHandle.asDouble();
		const MDataHandle tiltVHandle = block.inputValue(tiltV, &stat);
		CHECK_MSTATUS_AND_RETURN_IT(stat);
		double tiltVVal = tiltVHandle.asDouble();
		
		const MDataHandle randamRotXHandle = block.inputValue(randamRotX, &stat);
		CHECK_MSTATUS_AND_RETURN_IT(stat);
		double randamRotXVal = randamRotXHandle.asDouble();
		const MDataHandle randamRotYHandle = block.inputValue(randamRotY, &stat);
		CHECK_MSTATUS_AND_RETURN_IT(stat);
		double randamRotYVal = randamRotYHandle.asDouble();
		const MDataHandle randamRotZHandle = block.inputValue(randamRotZ, &stat);
		CHECK_MSTATUS_AND_RETURN_IT(stat);
		double randamRotZVal = randamRotZHandle.asDouble();
		
		const MDataHandle scaleXHandle = block.inputValue(scaleX, &stat);
		CHECK_MSTATUS_AND_RETURN_IT(stat);
		double scaleXVal = scaleXHandle.asDouble();
		const MDataHandle scaleYHandle = block.inputValue(scaleY, &stat);
		CHECK_MSTATUS_AND_RETURN_IT(stat);
		double scaleYVal = scaleYHandle.asDouble();
		const MDataHandle scaleZHandle = block.inputValue(scaleZ, &stat);
		CHECK_MSTATUS_AND_RETURN_IT(stat);
		double scaleZVal = scaleZHandle.asDouble();
		
		const MDataHandle randamScaleXHandle = block.inputValue(randamScaleX, &stat);
		CHECK_MSTATUS_AND_RETURN_IT(stat);
		double randamScaleXVal = randamScaleXHandle.asDouble();
		const MDataHandle randamScaleYHandle = block.inputValue(randamScaleY, &stat);
		CHECK_MSTATUS_AND_RETURN_IT(stat);
		double randamScaleYVal = randamScaleYHandle.asDouble();
		const MDataHandle randamScaleZHandle = block.inputValue(randamScaleZ, &stat);
		CHECK_MSTATUS_AND_RETURN_IT(stat);
		double randamScaleZVal = randamScaleZHandle.asDouble();
		
		const MDataHandle globalHeightOffsetHandle = block.inputValue(globalHeightOffset, &stat);
		CHECK_MSTATUS_AND_RETURN_IT(stat);
		double globalHeightOffsetVal = globalHeightOffsetHandle.asDouble();
		
		const MDataHandle scaleColorXHandle = block.inputValue(scaleColorX, &stat);
		CHECK_MSTATUS_AND_RETURN_IT(stat);
		short scaleColorXVal = scaleColorXHandle.asShort();
		const MDataHandle scaleColorYHandle = block.inputValue(scaleColorY, &stat);
		CHECK_MSTATUS_AND_RETURN_IT(stat);
		short scaleColorYVal = scaleColorYHandle.asShort();
		const MDataHandle scaleColorZHandle = block.inputValue(scaleColorZ, &stat);
		CHECK_MSTATUS_AND_RETURN_IT(stat);
		short scaleColorZVal = scaleColorZHandle.asShort();
		
		
		const MDataHandle UVsizeHandle = block.inputValue(UVsize, &stat);
		CHECK_MSTATUS_AND_RETURN_IT(stat);
		double UVsizeVal = UVsizeHandle.asDouble();
		const MDataHandle minUHandle = block.inputValue(minU, &stat);
		CHECK_MSTATUS_AND_RETURN_IT(stat);
		double minUVal = minUHandle.asDouble();
		const MDataHandle maxUHandle = block.inputValue(maxU, &stat);
		CHECK_MSTATUS_AND_RETURN_IT(stat);
		double maxUVal = maxUHandle.asDouble();
		const MDataHandle minVHandle = block.inputValue(minV, &stat);
		CHECK_MSTATUS_AND_RETURN_IT(stat);
		double minVVal = minVHandle.asDouble();
		const MDataHandle maxVHandle = block.inputValue(maxV, &stat);
		CHECK_MSTATUS_AND_RETURN_IT(stat);
		double maxVVal = maxVHandle.asDouble();
	        const MDataHandle UVTypeHandle = block.inputValue(UVType, &stat);
	        CHECK_MSTATUS_AND_RETURN_IT(stat);
	        short UVTypeVal = UVTypeHandle.asShort();

			std::size_t checkedNumU = 0;
			std::size_t checkedNumV = 0;
			std::size_t placementCount = 0;
			if (!dommy::generation::checkedCount(
					numUVal, dommy::generation::kMaxGeneratedPointCount,
					checkedNumU) ||
				!dommy::generation::checkedCount(
					numVVal, dommy::generation::kMaxGeneratedPointCount,
					checkedNumV) ||
				checkedNumU == 0U || checkedNumV == 0U ||
				!dommy::generation::checkedProduct(
					checkedNumU, checkedNumV,
					dommy::generation::kMaxWorkItemCount, placementCount)) {
				MGlobal::displayError(
					"surfaceDuplicater: sample count exceeds the generation budget.");
				return MS::kFailure;
			}
			const double finiteInputs[] = {
				stepUVal, stepVVal, randamUVal, randamVVal, offsetUVal,
				offsetVVal, tiltUVal, tiltVVal, randamRotXVal, randamRotYVal,
				randamRotZVal, scaleXVal, scaleYVal, scaleZVal, randamScaleXVal,
				randamScaleYVal, randamScaleZVal, globalHeightOffsetVal, UVsizeVal,
				minUVal, maxUVal, minVVal, maxVVal};
			for (std::size_t inputIndex = 0;
			     inputIndex < sizeof(finiteInputs) / sizeof(finiteInputs[0]);
			     ++inputIndex) {
				if (!std::isfinite(finiteInputs[inputIndex])) {
					MGlobal::displayError("surfaceDuplicater: inputs must be finite.");
					return MS::kFailure;
				}
			}
			if (normalSetVal < 0 || normalSetVal > 3 ||
				scaleColorXVal < 0 || scaleColorXVal > 4 ||
				scaleColorYVal < 0 || scaleColorYVal > 4 ||
				scaleColorZVal < 0 || scaleColorZVal > 4 ||
				UVTypeVal < 0 || UVTypeVal > 2 ||
				maxUVal <= minUVal || maxVVal <= minVVal ||
				!std::isfinite(maxUVal - minUVal) ||
				!std::isfinite(maxVVal - minVVal) ||
				(UVTypeVal == 1 && UVsizeVal == 0.0)) {
				MGlobal::displayError(
					"surfaceDuplicater: invalid enum or UV range.");
				return MS::kInvalidParameter;
			}
        
        
        
#pragma mark 乱数初期化
		
		dommy::math::RandomGenerator randomGenerator(static_cast<unsigned int>(typeSeedVal));

	#pragma mark thisNode
	        MObject thisNode = this->thisMObject();
	        MFnDependencyNode thiFn(thisNode, &stat);
		CHECK_MSTATUS_AND_RETURN_IT(stat);

#pragma mark get color map info
#pragma mark get baldnessColor
		
		MPlug bColPlug(thisNode,baldnessColor);
		
		MFnDependencyNode baldnessTexNodeFn;
		bool baldnessColPlugIsConnected = false;
		DOMMY_RETURN_IF_FAILURE(connectedSourceNode(
			bColPlug, baldnessColPlugIsConnected, baldnessTexNodeFn));
        
#pragma mark get tilt color Plug
		
		MPlug tColPlug(thisNode,tiltColor);
		
		MFnDependencyNode tiltTexNodeFn;
		bool tiltColPlugIsConnected = false;
		DOMMY_RETURN_IF_FAILURE(connectedSourceNode(
			tColPlug, tiltColPlugIsConnected, tiltTexNodeFn));
        
#pragma mark get height color Plug
		
		MPlug hColPlug(thisNode,heightColor);
		MFnDependencyNode heightTexNodeFn;
		bool heightColPlugIsConnected = false;
		DOMMY_RETURN_IF_FAILURE(connectedSourceNode(
			hColPlug, heightColPlugIsConnected, heightTexNodeFn));

#pragma mark get scale color Plug
		
		
		MPlug sColPlug(thisNode,scaleColor);
		MFnDependencyNode scaleTexNodeFn;
		bool scaleColPlugIsConnected = false;
		DOMMY_RETURN_IF_FAILURE(connectedSourceNode(
			sColPlug, scaleColPlugIsConnected, scaleTexNodeFn));
        
#pragma mark get time color Tex Node
		
		
        MPlug timeColPlug(thisNode,timeMapColor);
		
		MFnDependencyNode timeTexNodeFn;
		bool timeColPlugIsConnected = false;
		DOMMY_RETURN_IF_FAILURE(connectedSourceNode(
			timeColPlug, timeColPlugIsConnected, timeTexNodeFn));
		
		
#pragma mark get time Offset color Tex Node
		
		
		MPlug timeOffsetColPlug(thisNode,timeOffsetMapColor);
		
		MFnDependencyNode timeOffsetTexNodeFn;
		bool timeOffsetColPlugIsConnected = false;
		DOMMY_RETURN_IF_FAILURE(connectedSourceNode(
			timeOffsetColPlug, timeOffsetColPlugIsConnected,
			timeOffsetTexNodeFn));
		
		
		
		
#pragma mark input sourceNurbe Surface ベースのナーブスサーフェイスの取得
		MDataHandle sourcePlaneHnd = block.inputValue( sourcePlane, &stat );
		CHECK_MSTATUS_AND_RETURN_IT(stat);
		if(sourcePlaneHnd.type() != MFnData::kNurbsSurface) {
			return MS::kUnknownParameter;
		}
		MFnNurbsSurface soucePlaneFn(sourcePlaneHnd.data(), &stat);
		CHECK_MSTATUS_AND_RETURN_IT(stat);
		
		
		
		
		
#pragma mark input duplicate Mesh 複製するメッシュの取得
		std::vector<dupMesh> dupShapeArray;
		
		
        
		MArrayDataHandle arrayHandle = block.inputArrayValue(duplicateObjects, &stat);
		CHECK_MSTATUS_AND_RETURN_IT(stat);
		unsigned count = arrayHandle.elementCount();
		
			unsigned int dupObj;
		for(dupObj=0;dupObj<count;dupObj++) {
			stat = arrayHandle.jumpToArrayElement(dupObj);
			if (!stat) return stat;
			MDataHandle dupObjHnd = arrayHandle.inputValue(&stat);
			if (!stat) return stat;
					if (!dupObjHnd.data().isNull()) {
					dupShapeArray.emplace_back(dupObjHnd.data());
					if (!dupShapeArray.back().isValid()) {
						MGlobal::displayError(
							"surfaceDuplicater: duplicate mesh data is invalid or empty.");
						return MS::kInvalidParameter;
					}
				}
			}
		
			if (dupShapeArray.empty()) {
				MGlobal::displayWarning(
					"surfaceDuplicater: no duplicate mesh is connected.");
				return MS::kFailure;
			}

			std::size_t maximumVertexCount = 0;
			std::size_t maximumFaceCount = 0;
			for (std::vector<dupMesh>::iterator item = dupShapeArray.begin();
			     item != dupShapeArray.end(); ++item) {
				maximumVertexCount =
					std::max(maximumVertexCount,
					         static_cast<std::size_t>(item->getTotalVertexNum()));
				maximumFaceCount =
					std::max(maximumFaceCount,
					         static_cast<std::size_t>(item->getTotalPolygonNum()));
			}
			std::size_t outputVertexCount = 0;
			std::size_t outputFaceCount = 0;
			if (!dommy::generation::checkedProduct(
					placementCount, maximumVertexCount,
					dommy::generation::kMaxGeneratedPointCount, outputVertexCount) ||
				!dommy::generation::checkedProduct(
					placementCount, maximumFaceCount,
					dommy::generation::kMaxGeneratedFaceCount, outputFaceCount)) {
				MGlobal::displayError(
					"surfaceDuplicater: duplicated mesh exceeds the generation budget.");
				return MS::kFailure;
			}
#pragma mark poly data init
        
		dommy::maya::CheckedArray<MFloatPointArray> finVertArray;
		dommy::maya::CheckedArray<MFloatVectorArray> finNormalArray;
		dommy::maya::CheckedArray<MFloatArray> finUCoords, finVCoords;
		dommy::maya::CheckedArray<MIntArray> finUVIdsArray, finUVCountsArray;
		dommy::maya::CheckedArray<MIntArray> finPolyCountArray,
			finPolyConnectArray;
		
		MVectorArray planeNormArray;
		MVectorArray uVecArray,vVecArray;
		
		MFloatArray randSXArray,randSYArray,randSZArray;//randam Scale XYZ
		MFloatArray randRotXArray,randRotYArray,randRotZArray;//randam Rotate Y
		
		//set source plane division
		double uLength = maxUVal - minUVal;
		double vLength = maxVVal - minVVal;
		
		double uDiv = uLength / (double)numUVal;
		double vDiv = vLength / (double)numVVal;
		
		double uMidPos = uDiv / 2.0;
		double vMidPos = vDiv / 2.0;
		
		
		int i,j;
		for(i = 0;i<numUVal;i++) {
			//set vStep width
			double vStep;
			if(i%2 == 0)
				vStep = stepVVal;
			else
				vStep = 0.0;
			
			for(j = 0;j<numVVal;j++) {
				//set uStep width
				double uStep;
				if(j%2 == 0)
					uStep = stepUVal;
				else
					uStep = 0.0;
				
				
				//set source UV　ベースUVの取得
				double preUVal = minUVal+uMidPos+(uDiv*i);
				double preVVal = minVVal+vMidPos+(vDiv*j);
				
				double UVal,VVal;
				
				
				//set UV Random  UVのランダム
				double randU=0.0;
				if (randamUVal!=0.0) {
					randU = randomGenerator.uniform(-randamUVal,randamUVal);
				}
				double randV=0.0;
				if (randamVVal!=0.0) {
					randV = randomGenerator.uniform(-randamVVal,randamVVal);
				}
				
				
				
				
				
				if (!dommy::uv::wrapOffsetParameter(
						preUVal, offsetUVal, uStep, randU, minUVal, maxUVal, UVal) ||
					!dommy::uv::wrapOffsetParameter(
						preVVal, offsetVVal, vStep, randV, minVVal, maxVVal, VVal)) {
					return MS::kInvalidParameter;
				}
				
				
				
                
				MPoint uvPos;
				MVector uvNormal,uVec,vVec;
//              UV上での法線の取得
				DOMMY_RETURN_IF_FAILURE(soucePlaneFn.getDerivativesAtParm(
					UVal, VVal, uvPos, uVec, vVec, MSpace::kWorld));
				uvNormal = soucePlaneFn.normal(
					UVal, VVal, MSpace::kWorld, &stat);
				CHECK_MSTATUS_AND_RETURN_IT(stat);
				if (!dommy::maya::isFloatRepresentablePoint(uvPos) ||
					!dommy::maya::isFloatRepresentableVector(uvNormal) ||
					!dommy::maya::isFloatRepresentableVector(uVec) ||
					!dommy::maya::isFloatRepresentableVector(vVec)) {
					return MS::kFailure;
				}
				
				
				
                //set color UVCoords　UV上でのカラーをセット
				DOMMY_RETURN_IF_FAILURE(
					colPoints.append((MFloatPoint)uvPos));
				DOMMY_RETURN_IF_FAILURE(colUCoords.append(UVal));
				DOMMY_RETURN_IF_FAILURE(colVCoords.append(VVal));
				DOMMY_RETURN_IF_FAILURE(preUCoords.append(preUVal));
				DOMMY_RETURN_IF_FAILURE(preVCoords.append(preVVal));
				
				DOMMY_RETURN_IF_FAILURE(planeNormArray.append(uvNormal));
				DOMMY_RETURN_IF_FAILURE(uVecArray.append(uVec));
				DOMMY_RETURN_IF_FAILURE(vVecArray.append(vVec));
				
				//set randam scale ランダムスケールのセット
				double randSX=0.0;
				double randSY=0.0;
				double randSZ=0.0;
				
				if (randamScaleXVal!=0.0)
				{
					randSX = randomGenerator.uniform(0.0,randamScaleXVal);
				}
				if (randamScaleYVal!=0.0)
				{
					randSY = randomGenerator.uniform(0.0,randamScaleYVal);
				}
				if (randamScaleZVal!=0.0)
				{
					randSZ = randomGenerator.uniform(0.0,randamScaleZVal);
				}
				DOMMY_RETURN_IF_FAILURE(randSXArray.append(randSX));
				DOMMY_RETURN_IF_FAILURE(randSYArray.append(randSY));
				DOMMY_RETURN_IF_FAILURE(randSZArray.append(randSZ));
				
				//set randam  rotate　ランダム回転のセット
				double randRX=0.0;
				double randRY=0.0;
				double randRZ=0.0;
				
				if (randamRotXVal!=0.0)
				{
					randRX = randomGenerator.uniform(-randamRotXVal,randamRotXVal);
				}
				
				if (randamRotYVal!=0.0)
				{
					randRY = randomGenerator.uniform(-randamRotYVal,randamRotYVal);
				}
				
				if (randamRotZVal!=0.0)
				{
					randRZ = randomGenerator.uniform(-randamRotZVal,randamRotZVal);
				}
				
				DOMMY_RETURN_IF_FAILURE(randRotXArray.append(randRX));
				DOMMY_RETURN_IF_FAILURE(randRotYArray.append(randRY));
				DOMMY_RETURN_IF_FAILURE(randRotZArray.append(randRZ));
				
			}
		}
		
		
		
		
		
		
#pragma mark Color data ----------
#pragma mark get tilt color map data
		MVectorArray tColArray;
		MDoubleArray tTransArray;
		if(tiltColPlugIsConnected) {
			stat = getColorFromTex(
				tiltTexNodeFn.object(), colUCoords, colVCoords,
				tColArray, tTransArray);
			CHECK_MSTATUS_AND_RETURN_IT(stat);
		}
        
#pragma mark get baldness color map data
		MVectorArray bColArray;
		MDoubleArray bTransArray;
		
		if(baldnessColPlugIsConnected) {
			stat = getColorFromTex(
				baldnessTexNodeFn.object(), colUCoords, colVCoords,
				bColArray, bTransArray);
			CHECK_MSTATUS_AND_RETURN_IT(stat);
		}
        
#pragma mark set height map data
		MVectorArray hColArray;
		MDoubleArray hTransArray;
		
		if(heightColPlugIsConnected) {
			stat = getColorFromTex(
				heightTexNodeFn.object(), colUCoords, colVCoords,
				hColArray, hTransArray);
			CHECK_MSTATUS_AND_RETURN_IT(stat);
		}
        
#pragma mark get scale map data
		MVectorArray sColArray;
		MDoubleArray sTransArray;
		if(scaleColPlugIsConnected) {
			stat = getColorFromTex(
				scaleTexNodeFn.object(), colUCoords, colVCoords,
				sColArray, sTransArray);
			CHECK_MSTATUS_AND_RETURN_IT(stat);
		}
        
#pragma mark get animMatrix
		MVectorArray timeColArray;
		MDoubleArray timeTransArray;
		
	        MPlug animMatPlug = thiFn.findPlug(animMatrix, true, &stat);
		CHECK_MSTATUS_AND_RETURN_IT(stat);
		const bool animMatrixConnected = animMatPlug.isConnected(&stat);
		CHECK_MSTATUS_AND_RETURN_IT(stat);
	        if (animMatrixConnected && timeColPlugIsConnected) {
			
			stat = getColorFromTex(
				timeTexNodeFn.object(), colUCoords, colVCoords,
				timeColArray, timeTransArray);
			CHECK_MSTATUS_AND_RETURN_IT(stat);
        }
#pragma mark get tine offset color
		MVectorArray timeOffsetColArray;
		MDoubleArray timeOffsetTransArray;
		
		if (animMatrixConnected && timeOffsetColPlugIsConnected) {
			stat = getColorFromTex(
				timeOffsetTexNodeFn.object(), colUCoords, colVCoords,
				timeOffsetColArray, timeOffsetTransArray);
			CHECK_MSTATUS_AND_RETURN_IT(stat);
		}

		const unsigned int colorSampleCount = colPoints.length();
		if (colorSampleCount != placementCount ||
			!validTextureSamples(
				tiltColPlugIsConnected, tColArray, colorSampleCount) ||
			!validTextureSamples(
				baldnessColPlugIsConnected, bColArray, colorSampleCount) ||
			!validTextureSamples(
				heightColPlugIsConnected, hColArray, colorSampleCount) ||
			!validTextureSamples(
				scaleColPlugIsConnected, sColArray, colorSampleCount) ||
			!validTextureSamples(
				animMatrixConnected && timeColPlugIsConnected,
				timeColArray, colorSampleCount) ||
			!validTextureSamples(
				animMatrixConnected && timeOffsetColPlugIsConnected,
				timeOffsetColArray, colorSampleCount)) {
			MGlobal::displayError(
				"surfaceDuplicater: texture sampling returned invalid data.");
			return MS::kFailure;
		}
		
		
		//set final mesh data
		long fin;
		long x=0;
		long xuv=0;
		
		dommy::maya::CheckedArray<MIntArray> typeArray;
		
		
		MTime::Unit fps = MTime::uiUnit();
		
		MTime currentTime;
		DOMMY_RETURN_IF_FAILURE(dommy::maya::getEvaluationTime(block, currentTime));
		if (!std::isfinite(currentTime.value())) return MS::kFailure;
		
		
		for(fin=0;fin<colPoints.length();fin++)
		{
			//set randam input duplicate object
			int type = 0;
			if (dupShapeArray.size()>1) {
				type = static_cast<int>(randomGenerator.index(dupShapeArray.size()));
			}
			
			
			
#pragma mark Set baldness data 禿げデータのセット
			if(baldnessColPlugIsConnected)
			{
				MColor bColor( bColArray[fin].x, bColArray[fin].y, bColArray[fin].z );
				float hVal, sVal, vVal;
				bColor.get( MColor::kHSV, hVal, sVal, vVal );
				
				if(vVal == 0.0)
					continue;
			}
			
			typeArray.append(type);
			
			
			
#pragma mark Set tilt data　傾きデータのセット
			MVector tiltVec(0.0,0.0,0.0);
			if(tiltColPlugIsConnected)
			{
				MColor tColor( tColArray[fin].x, tColArray[fin].y, tColArray[fin].z );
				tiltVec.x = tColor.r;
				tiltVec.y = tColor.g;
				tiltVec.z = tColor.b;
				
			}
			
			
#pragma mark Set height data　高さデータのセット
			float heightVal = 0.0;
            if(heightColPlugIsConnected)
			{
				MColor hColor( hColArray[fin].x, hColArray[fin].y, hColArray[fin].z );
				float hVal, sVal, vVal;
				hColor.get( MColor::kHSV,hVal, sVal, vVal );
				heightVal = vVal;
				
			}
			
			
#pragma mark Set scale data スケールデータのセット
			float scxVal = 1.0;
			float scyVal = 1.0;
			float sczVal = 1.0;
			if(scaleColPlugIsConnected)
			{
				MColor sColor( sColArray[fin].x, sColArray[fin].y, sColArray[fin].z );
				float hVal, sVal, vVal;
				sColor.get( MColor::kHSV,hVal, sVal, vVal );
				
				if (scaleColorXVal == 0) {scxVal = vVal;}
				else if(scaleColorXVal == 1) {scxVal = sColor.r;}
				else if(scaleColorXVal == 2) {scxVal = sColor.g;}
				else if(scaleColorXVal == 3) {scxVal = sColor.b;}
				else if(scaleColorXVal == 4) {scxVal = 1.0;}
				
				if (scaleColorYVal == 0) {scyVal = vVal;}
				else if(scaleColorYVal == 1) {scyVal = sColor.r;}
				else if(scaleColorYVal == 2) {scyVal = sColor.g;}
				else if(scaleColorYVal == 3) {scyVal = sColor.b;}
				else if(scaleColorYVal == 4) {scyVal = 1.0;}
				
				if (scaleColorZVal == 0) {sczVal = vVal;}
				else if(scaleColorZVal == 1) {sczVal = sColor.r;}
				else if(scaleColorZVal == 2) {sczVal = sColor.g;}
				else if(scaleColorZVal == 3) {sczVal = sColor.b;}
				else if(scaleColorZVal == 4) {sczVal = 1.0;}
			}
            
#pragma mark Set animMatrix
			
            MTransformationMatrix transformMat;
            
            if (animMatrixConnected && (timeColPlugIsConnected || timeOffsetColPlugIsConnected)) {
				
				float timeVal = 0.0;
				
				if (timeColPlugIsConnected) {
					MColor timeColor( timeColArray[fin].x, timeColArray[fin].y, timeColArray[fin].z );
					float hVal, sVal;
					timeColor.get( MColor::kHSV,hVal, sVal, timeVal );
					
					
				} else {
					timeVal = currentTime.value();
				}
				
				
				float offsetTinme =0.0;
				if (timeOffsetColPlugIsConnected) {
					MColor tOffsetColor( timeOffsetColArray[fin].x, timeOffsetColArray[fin].y, timeOffsetColArray[fin].z );
					float hVal, sVal;
					tOffsetColor.get( MColor::kHSV,hVal, sVal, offsetTinme );
					
				}
				const double animationTimeValue =
					static_cast<double>(timeVal) -
					static_cast<double>(offsetTinme);
				if (!std::isfinite(animationTimeValue)) {
					return MS::kFailure;
				}
                MTime animTime(animationTimeValue, fps);
				
				
                MObject matObj;
                
                MDGContext context(animTime);
                MDGContextGuard contextGuard( context );
                DOMMY_RETURN_IF_FAILURE(animMatPlug.getValue(matObj));
				if (matObj.isNull()) return MS::kFailure;
                
                MFnMatrixData worldMatrixData( matObj, &stat );
				CHECK_MSTATUS_AND_RETURN_IT(stat);
                MMatrix transMatVal = worldMatrixData.matrix(&stat);
				CHECK_MSTATUS_AND_RETURN_IT(stat);
				if (!dommy::maya::isFiniteMatrix(transMatVal)) {
					return MS::kFailure;
				}
                
				transformMat = MTransformationMatrix(transMatVal);
				
				
                
            }
			
			
#pragma mark Set final vetex List　頂点データのセット
			MFloatPointArray newArray;
			DOMMY_RETURN_IF_FAILURE(newArray.copy(dupShapeArray[type].getVertexArray()));
			
			MQuaternion xQuat,yQuat;
			long p;
			for(p=0;p<newArray.length();p++)
			{
				//set scale
				newArray[p].x *= scaleXVal*(scxVal+randSXArray[fin]);
				newArray[p].y *= scaleYVal*(scyVal+randSYArray[fin]);
				newArray[p].z *= scaleZVal*(sczVal+randSZArray[fin]);
				
				
				
				
				
				//set rotate
				MVector preRot(newArray[p]);
				
				double randRotXYZ[3] = {(tiltUVal+randRotXArray[fin]+tiltVec.x),(randRotYArray[fin]+tiltVec.y),(tiltVVal+randRotZArray[fin]+tiltVec.z)};
				preRot = preRot.rotateBy(randRotXYZ,MTransformationMatrix::kYZX);
				newArray[p].setCast(preRot);
				
				
				//set Animation
				const MPoint animatedPoint =
					MPoint(newArray[p]) * transformMat.asMatrix();
				if (!dommy::maya::isFloatRepresentablePoint(animatedPoint)) {
					return MS::kFailure;
				}
				newArray[p] = MFloatPoint(animatedPoint);
				
				
				if(aimNormalVal)
				{
					MVector nowVec(newArray[p]);
					
					if(p==0)
					{
						MVector yAxisVec(0.0,1.0,0.0);
						MVector xAxisVec(1.0,0.0,0.0);
						yQuat = yAxisVec.rotateTo(planeNormArray[fin]);
						MVector xPostPosVec = xAxisVec.rotateBy(yQuat);
						xQuat = xPostPosVec.rotateTo(uVecArray[fin]);
					}
					
					
					newArray[p].setCast(nowVec.rotateBy(yQuat).rotateBy(xQuat));
				}
				
				
				
				//set translate (offset)
				
				newArray[p].x += colPoints[fin].x;
				newArray[p].y += colPoints[fin].y + heightVal + globalHeightOffsetVal;
				newArray[p].z += colPoints[fin].z;
				if (!std::isfinite(newArray[p].x) ||
					!std::isfinite(newArray[p].y) ||
					!std::isfinite(newArray[p].z)) {
					return MS::kFailure;
				}
				
				
				
				
                finVertArray.append(newArray[p]);
				
			}
			
			
			//set Poly Count List Set 頂点数のセット
			const MIntArray& pCList = dupShapeArray[type].getPolyCountsList();
			for(p=0;p<pCList.length();p++)
			{
				finPolyCountArray.append(pCList[p]);
			}
			
			
			//set poly connect list　コネクトリストのセット
			const MIntArray& pConeList = dupShapeArray[type].getPolyConnectList();
			for(p=0;p<pConeList.length();p++)
			{
				finPolyConnectArray.append(pConeList[p]+x);
			}
			
			
			
			//set Normal　法線のセット
			MFloatVectorArray newNormArray;
			DOMMY_RETURN_IF_FAILURE(newNormArray.copy(dupShapeArray[type].getNormalArray()));
			for(p=0;p<newNormArray.length();p++)
			{
				//set rotate
				
				MVector preRot(newNormArray[p]);
				
				double randRotXYZ[3] = {(tiltUVal+randRotXArray[fin]+tiltVec.x),(randRotYArray[fin]+tiltVec.y),(tiltVVal+randRotZArray[fin]+tiltVec.z)};
				preRot = preRot.rotateBy(randRotXYZ,MTransformationMatrix::kYZX);
				newNormArray[p] = preRot;
				
				
				if(aimNormalVal)
				{
					
					MVector nowVec(newNormArray[p]);
					
					if(p==0)
					{
						MVector yAxisVec(0.0,1.0,0.0);
						MVector xAxisVec(1.0,0.0,0.0);
						yQuat = yAxisVec.rotateTo(planeNormArray[fin]);
						MVector xPostPosVec = xAxisVec.rotateBy(yQuat);
						xQuat = xPostPosVec.rotateTo(uVecArray[fin]);
					}
					
					
					newNormArray[p] = nowVec.rotateBy(yQuat).rotateBy(xQuat);
				}
				
				
				
				finNormalArray.append(newNormArray[p]);
			}
			
			
			
			

			//set UV array　UVのセット
			const MFloatArray& newUCoords = dupShapeArray[type].getUCoords();
			const MFloatArray& newVCoords = dupShapeArray[type].getVCoords();
			
			
			
			for(p=0;p<newUCoords.length();p++)
			{
				
                switch (UVTypeVal) {
                    case 1://dot
                    {
                        finUCoords.append((newUCoords[p]/UVsizeVal)+preUCoords[fin]);
                        finVCoords.append((newVCoords[p]/UVsizeVal)+preVCoords[fin]);
                    }
                        break;
					case 2://tile
					{
						// Placement generation iterates U outside V. Keep each
						// surviving object in its original cell after thinning.
						const std::size_t tileU = static_cast<std::size_t>(fin) / numVVal;
						const std::size_t tileV = static_cast<std::size_t>(fin) % numVVal;
						float u = 0.0f, v = 0.0f;
						if (!dommy::uv::mapTileUV(newUCoords[p], newVCoords[p],
							tileU, tileV, numUVal, numVVal, u, v)) return MS::kFailure;
						finUCoords.append(u);
						finVCoords.append(v);
					}
						break;
                    default://base keep
                    {
                        finUCoords.append(newUCoords[p]);
                        finVCoords.append(newVCoords[p]);
                    }
                        break;
                }
				
			}
			
			
			const MIntArray& uvCList = dupShapeArray[type].getUVCountsArray();
			const MIntArray& uvIDList = dupShapeArray[type].getUVIdsArray();
			
			for(p=0;p<uvIDList.length();p++)
			{
				finUVIdsArray.append(uvIDList[p] + xuv);
			}
			for(p=0;p<uvCList.length();p++)
			{
				finUVCountsArray.append(uvCList[p]);
			}
			
			
			
			xuv += newUCoords.length();
			x += newArray.length();
		}
		
		
		
		if (!validGeneratedMeshArrays(
				finVertArray, finUCoords, finVCoords, finPolyCountArray,
				finPolyConnectArray, finUVCountsArray, finUVIdsArray,
				outputVertexCount, outputFaceCount)) {
			MGlobal::displayError(
				"surfaceDuplicater: generated mesh arrays are inconsistent.");
			return MS::kFailure;
		}

		int totalPoints = static_cast<int>(finVertArray.length());
		int totalPolygon = static_cast<int>(finPolyCountArray.length());
		
		
		
		
		//Create new mesh
		MFnMeshData meshDataFn;
		MObject newOutMeshData = meshDataFn.create(&stat);
		CHECK_MSTATUS_AND_RETURN_IT(stat);
		if (newOutMeshData.isNull()) return MS::kFailure;

		MFnMesh newMeshFn;
		MObject newMeshObject;
		if (totalPoints > 0) {
			newMeshObject = newMeshFn.create(
				totalPoints, totalPolygon, finVertArray,
				finPolyCountArray, finPolyConnectArray,
				finUCoords, finVCoords, newOutMeshData, &stat);
			CHECK_MSTATUS_AND_RETURN_IT(stat);
			if (newMeshObject.isNull()) return MS::kFailure;
		}
		//aassign UV Id 
		if (!newMeshObject.isNull()) {
			DOMMY_RETURN_IF_FAILURE(
				newMeshFn.assignUVs(finUVCountsArray,finUVIdsArray));
		}
		
		
		if (!newMeshObject.isNull()) {
			switch( normalSetVal )
			{
			case 0 : //normal is no set.
				
				break;
				case 1 : //all normal is hard.
					
					int h;
					for(h=0;h<newMeshFn.numEdges();h++)
					{
						DOMMY_RETURN_IF_FAILURE(
							newMeshFn.setEdgeSmoothing(h,false));
					}
					break;
			case 2 : //all normal is soft.
				
					int s;
					for(s=0;s<newMeshFn.numEdges();s++)
					{
						DOMMY_RETURN_IF_FAILURE(
							newMeshFn.setEdgeSmoothing(s,true));
				}
				break;
			case 3 : // Normal is set from input Object.
				
				unsigned int e=0U;
				unsigned int ae=0U;
				int en = 0;
				int totalE = 0; //total vertex normal num
				int pfn =0; // polygon number
				int normalNum=0;
				
				
				
				//set per vertex normal;
				for(ae=0;ae<typeArray.length();ae++)
				{
					//set per vertex normal
					const MIntArray& pCountList = dupShapeArray[typeArray[ae]].getPolyCountsList();
					const MIntArray& normIdList = dupShapeArray[typeArray[ae]].getNormIdsArray();
					for(e=0;e<pCountList.length();e++)
					{
						int ec;
						for(ec=0;ec<pCountList[e];ec++)
						{
							MVector normVec(finNormalArray[normIdList[en]+normalNum].x,finNormalArray[normIdList[en]+normalNum].y,finNormalArray[normIdList[en]+normalNum].z);
							
							DOMMY_RETURN_IF_FAILURE(
								newMeshFn.setFaceVertexNormal(
									normVec, pfn,
									finPolyConnectArray[totalE],
									MSpace::kObject));
							en++;
							totalE++;
						}
						pfn++;
					}
					en = 0;
					normalNum += dupShapeArray[typeArray[ae]].getNormalNum();
				}
					break;
			}
		}
		
		if (!newMeshObject.isNull()) {
			DOMMY_RETURN_IF_FAILURE(newMeshFn.updateSurface());
		}
		MDataHandle outputMeshHnd = block.outputValue(outPutMesh, &stat);
		CHECK_MSTATUS_AND_RETURN_IT(stat);
		DOMMY_RETURN_IF_FAILURE(outputMeshHnd.set(newOutMeshData));
		
		
		
		//clear duplicate mesh data
		
		if( dupShapeArray.size() != 0 ) dupShapeArray.clear();
		
		
		stat = block.setClean(plug);
		if (!stat) {
			stat.perror("SurfaceDuplicator::compute setClean");
			return stat;
		}
		
	}
	else 
	{
		
		stat  = MS::kUnknownParameter;
	}
	
	
	return stat;
	} DOMMY_COMPUTE_CATCH("surfaceDuplicater")
}



MStatus surfaceDuplicater::initialize()
try {
	MStatus stat;
	
	MFnNumericAttribute nAttr;
	MFnGenericAttribute gAttr;
	MFnEnumAttribute enumFn;
	duplicateObjects = gAttr.create( "duplicateObjects", "dpo" , &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(gAttr.addDataAccept( MFnData::kMesh ));
	DOMMY_RETURN_IF_FAILURE(gAttr.setHidden( true ));
	DOMMY_RETURN_IF_FAILURE(gAttr.setArray( true ));
	
	typeSeed = nAttr.create( "typeSeed", "ts", MFnNumericData::kInt , 0, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 1 ));
	DOMMY_RETURN_IF_FAILURE(nAttr.setMin( 1 ));
	DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
	
	sourcePlane = gAttr.create( "sourcePlane", "sp" , &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(gAttr.addDataAccept( MFnData::kNurbsSurface ));
	DOMMY_RETURN_IF_FAILURE(gAttr.setHidden( true ));
	
	numU = nAttr.create( "numU", "nu", MFnNumericData::kInt , 0, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 3 ));
	DOMMY_RETURN_IF_FAILURE(nAttr.setMin( 1 ));
	DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
	
	numV = nAttr.create( "numV", "nv", MFnNumericData::kInt , 0, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 3 ));
	DOMMY_RETURN_IF_FAILURE(nAttr.setMin( 1 ));
	DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
	
	
	stepU = nAttr.create( "stepU", "su", MFnNumericData::kDouble , 0, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 0.0 ));
	DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
	
	stepV = nAttr.create( "stepV", "sv", MFnNumericData::kDouble , 0, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 0.0 ));
	DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
	
	randamU = nAttr.create( "randamU", "ru", MFnNumericData::kDouble , 0, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 0.0 ));
	DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
	
	randamV = nAttr.create( "randamV", "rv", MFnNumericData::kDouble , 0, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 0.0 ));
	DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
	
	
	aimNormal = nAttr.create( "aimNormal", "an", MFnNumericData::kBoolean , 0, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 0 ));
	DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
	
	normalSet = enumFn.create( "normalSet", "ns", 0, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(enumFn.addField( "None", 0 ));
	DOMMY_RETURN_IF_FAILURE(enumFn.addField( "Hard", 1 ));
	DOMMY_RETURN_IF_FAILURE(enumFn.addField( "Soft", 2 ));
	DOMMY_RETURN_IF_FAILURE(enumFn.addField( "fromSource", 3 ));
	DOMMY_RETURN_IF_FAILURE(enumFn.setKeyable( true ));
	
	
	offsetU = nAttr.create( "offsetU", "ou", MFnNumericData::kDouble , 0, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 0.0 ));
	DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
	
	offsetV = nAttr.create( "offsetV", "ov", MFnNumericData::kDouble , 0, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 0.0 ));
	DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
	
	
	tiltU = nAttr.create( "tiltU", "tu", MFnNumericData::kDouble , 0, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 0.0 ));
	DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
	
	tiltV = nAttr.create( "tiltV", "tv", MFnNumericData::kDouble , 0, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 0.0 ));
	DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
	
	
	tiltColorR = nAttr.create( "tiltColorR", "tcr", MFnNumericData::kFloat, 0, &stat );
	DOMMY_RETURN_IF_FAILURE( stat );
	DOMMY_RETURN_IF_FAILURE( nAttr.setKeyable( true ) );
	DOMMY_RETURN_IF_FAILURE( nAttr.setStorable( true ) );
	DOMMY_RETURN_IF_FAILURE( nAttr.setDefault( 0.0f ) );
	
	tiltColorG = nAttr.create( "tiltColorG", "tcg", MFnNumericData::kFloat, 0, &stat );
	DOMMY_RETURN_IF_FAILURE( stat );
	DOMMY_RETURN_IF_FAILURE( nAttr.setKeyable( true ) );
	DOMMY_RETURN_IF_FAILURE( nAttr.setStorable( true ) );
	DOMMY_RETURN_IF_FAILURE( nAttr.setDefault( 0.0f ) );
	
	tiltColorB = nAttr.create( "tiltColorB", "tcb", MFnNumericData::kFloat, 0, &stat );
	DOMMY_RETURN_IF_FAILURE( stat);
	DOMMY_RETURN_IF_FAILURE( nAttr.setKeyable( true ) );
	DOMMY_RETURN_IF_FAILURE( nAttr.setStorable( true ) );
	DOMMY_RETURN_IF_FAILURE( nAttr.setDefault( 0.0f ) );
	
	tiltColor = nAttr.create( "tiltColor", "tc", tiltColorR, tiltColorG, tiltColorB, &stat );
	DOMMY_RETURN_IF_FAILURE( stat );
	DOMMY_RETURN_IF_FAILURE( nAttr.setKeyable( true ) );
	DOMMY_RETURN_IF_FAILURE( nAttr.setStorable( true ) );
	DOMMY_RETURN_IF_FAILURE( nAttr.setDefault( 0.0f, 0.0f, 0.0f ) );
	DOMMY_RETURN_IF_FAILURE( nAttr.setUsedAsColor( true ) );
	randamRotX = nAttr.create( "randamRotX", "rrx", MFnNumericData::kDouble , 0, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 0.0 ));
	DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
	
	randamRotY = nAttr.create( "randamRotY", "rry", MFnNumericData::kDouble , 0, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 0.0 ));
	DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
	
	randamRotZ = nAttr.create( "randamRotZ", "rrz", MFnNumericData::kDouble , 0, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 0.0 ));
	DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
	
	
	
	scaleX = nAttr.create( "scaleX", "sx", MFnNumericData::kDouble , 0, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 1.0 ));
	DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
	
	scaleY = nAttr.create( "scaleY", "sy", MFnNumericData::kDouble , 0, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 1.0 ));
	DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
	
	scaleZ = nAttr.create( "scaleZ", "sz", MFnNumericData::kDouble , 0, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 1.0 ));
	DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
	
	
	randamScaleX = nAttr.create( "randamScaleX", "rsx", MFnNumericData::kDouble , 0, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 0.0 ));
	DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
	
	randamScaleY = nAttr.create( "randamScaleY", "rsy", MFnNumericData::kDouble , 0, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 0.0 ));
	DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
	
	randamScaleZ = nAttr.create( "randamScaleZ", "rsz", MFnNumericData::kDouble , 0, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 0.0 ));
	DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
	
	
	
	
	
	
	
	baldnessColorR = nAttr.create( "baldnessColorR", "bcr", MFnNumericData::kFloat, 0, &stat );
	DOMMY_RETURN_IF_FAILURE( stat );
	DOMMY_RETURN_IF_FAILURE( nAttr.setKeyable( true ) );
	DOMMY_RETURN_IF_FAILURE( nAttr.setStorable( true ) );
	DOMMY_RETURN_IF_FAILURE( nAttr.setDefault( 0.0f ) );
	
	baldnessColorG = nAttr.create( "baldnessColorG", "bcg", MFnNumericData::kFloat, 0, &stat );
	DOMMY_RETURN_IF_FAILURE( stat );
	DOMMY_RETURN_IF_FAILURE( nAttr.setKeyable( true ) );
	DOMMY_RETURN_IF_FAILURE( nAttr.setStorable( true ) );
	DOMMY_RETURN_IF_FAILURE( nAttr.setDefault( 0.0f ) );
	
	baldnessColorB = nAttr.create( "baldnessColorB", "bcb", MFnNumericData::kFloat, 0, &stat );
	DOMMY_RETURN_IF_FAILURE( stat);
	DOMMY_RETURN_IF_FAILURE( nAttr.setKeyable( true ) );
	DOMMY_RETURN_IF_FAILURE( nAttr.setStorable( true ) );
	DOMMY_RETURN_IF_FAILURE( nAttr.setDefault( 0.0f ) );
	
	baldnessColor = nAttr.create( "baldnessColor", "bc", baldnessColorR, baldnessColorG, baldnessColorB, &stat );
	DOMMY_RETURN_IF_FAILURE( stat );
	DOMMY_RETURN_IF_FAILURE( nAttr.setKeyable( true ) );
	DOMMY_RETURN_IF_FAILURE( nAttr.setStorable( true ) );
	DOMMY_RETURN_IF_FAILURE( nAttr.setDefault( 0.0f, 0.0f, 0.0f ) );
	DOMMY_RETURN_IF_FAILURE( nAttr.setUsedAsColor( true ) );
	
	
	
	globalHeightOffset = nAttr.create( "globalHeightOffset", "gho", MFnNumericData::kDouble , 0, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 0.0 ));
	DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
	
	
	heightColorR = nAttr.create( "heightColorR", "hcr", MFnNumericData::kFloat, 0, &stat );
	DOMMY_RETURN_IF_FAILURE( stat );
	DOMMY_RETURN_IF_FAILURE( nAttr.setKeyable( true ) );
	DOMMY_RETURN_IF_FAILURE( nAttr.setStorable( true ) );
	DOMMY_RETURN_IF_FAILURE( nAttr.setDefault( 0.0f ) );
	
	heightColorG = nAttr.create( "heightColorG", "hcg", MFnNumericData::kFloat, 0, &stat );
	DOMMY_RETURN_IF_FAILURE( stat );
	DOMMY_RETURN_IF_FAILURE( nAttr.setKeyable( true ) );
	DOMMY_RETURN_IF_FAILURE( nAttr.setStorable( true ) );
	DOMMY_RETURN_IF_FAILURE( nAttr.setDefault( 0.0f ) );
	
	heightColorB = nAttr.create( "heightColorB", "hcb", MFnNumericData::kFloat, 0, &stat );
	DOMMY_RETURN_IF_FAILURE( stat);
	DOMMY_RETURN_IF_FAILURE( nAttr.setKeyable( true ) );
	DOMMY_RETURN_IF_FAILURE( nAttr.setStorable( true ) );
	DOMMY_RETURN_IF_FAILURE( nAttr.setDefault( 0.0f ) );
	
	heightColor = nAttr.create( "heightColor", "hc", heightColorR, heightColorG, heightColorB, &stat );
	DOMMY_RETURN_IF_FAILURE( stat );
	DOMMY_RETURN_IF_FAILURE( nAttr.setKeyable( true ) );
	DOMMY_RETURN_IF_FAILURE( nAttr.setStorable( true ) );
	DOMMY_RETURN_IF_FAILURE( nAttr.setDefault( 0.0f, 0.0f, 0.0f ) );
	DOMMY_RETURN_IF_FAILURE( nAttr.setUsedAsColor( true ) );
	
	
	
	
	scaleColorX = enumFn.create( "scaleColorX", "scx", 0, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(enumFn.addField( "Luminance", 0 ));
	DOMMY_RETURN_IF_FAILURE(enumFn.addField( "R", 1 ));
	DOMMY_RETURN_IF_FAILURE(enumFn.addField( "G", 2 ));
	DOMMY_RETURN_IF_FAILURE(enumFn.addField( "B", 3 ));
	DOMMY_RETURN_IF_FAILURE(enumFn.addField( "None", 4 ));
	DOMMY_RETURN_IF_FAILURE(enumFn.setKeyable( true ));
	
	scaleColorY = enumFn.create( "scaleColorY", "scy", 0, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(enumFn.addField( "Luminance", 0 ));
	DOMMY_RETURN_IF_FAILURE(enumFn.addField( "R", 1 ));
	DOMMY_RETURN_IF_FAILURE(enumFn.addField( "G", 2 ));
	DOMMY_RETURN_IF_FAILURE(enumFn.addField( "B", 3 ));
	DOMMY_RETURN_IF_FAILURE(enumFn.addField( "None", 4 ));
	DOMMY_RETURN_IF_FAILURE(enumFn.setKeyable( true ));
	
	scaleColorZ = enumFn.create( "scaleColorZ", "scz", 0, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(enumFn.addField( "Luminance", 0 ));
	DOMMY_RETURN_IF_FAILURE(enumFn.addField( "R", 1 ));
	DOMMY_RETURN_IF_FAILURE(enumFn.addField( "G", 2 ));
	DOMMY_RETURN_IF_FAILURE(enumFn.addField( "B", 3 ));
	DOMMY_RETURN_IF_FAILURE(enumFn.addField( "None", 4 ));
	DOMMY_RETURN_IF_FAILURE(enumFn.setKeyable( true ));
	
	
	scaleColorR = nAttr.create( "scaleColorR", "scr", MFnNumericData::kFloat, 0, &stat );
	DOMMY_RETURN_IF_FAILURE( stat );
	DOMMY_RETURN_IF_FAILURE( nAttr.setKeyable( true ) );
	DOMMY_RETURN_IF_FAILURE( nAttr.setStorable( true ) );
	DOMMY_RETURN_IF_FAILURE( nAttr.setDefault( 0.0f ) );
	
	scaleColorG = nAttr.create( "scaleColorG", "scg", MFnNumericData::kFloat, 0, &stat );
	DOMMY_RETURN_IF_FAILURE( stat );
	DOMMY_RETURN_IF_FAILURE( nAttr.setKeyable( true ) );
	DOMMY_RETURN_IF_FAILURE( nAttr.setStorable( true ) );
	DOMMY_RETURN_IF_FAILURE( nAttr.setDefault( 0.0f ) );
	
	scaleColorB = nAttr.create( "scaleColorB", "scb", MFnNumericData::kFloat, 0, &stat );
	DOMMY_RETURN_IF_FAILURE( stat);
	DOMMY_RETURN_IF_FAILURE( nAttr.setKeyable( true ) );
	DOMMY_RETURN_IF_FAILURE( nAttr.setStorable( true ) );
	DOMMY_RETURN_IF_FAILURE( nAttr.setDefault( 0.0f ) );
	
	scaleColor = nAttr.create( "scaleColor", "sc", scaleColorR, scaleColorG, scaleColorB, &stat );
	DOMMY_RETURN_IF_FAILURE( stat );
	DOMMY_RETURN_IF_FAILURE( nAttr.setKeyable( true ) );
	DOMMY_RETURN_IF_FAILURE( nAttr.setStorable( true ) );
	DOMMY_RETURN_IF_FAILURE( nAttr.setDefault( 0.0f, 0.0f, 0.0f ) );
	DOMMY_RETURN_IF_FAILURE( nAttr.setUsedAsColor( true ) );
	
	
	
	
	UVsize = nAttr.create( "UVsize", "uvs", MFnNumericData::kDouble , 0, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 1000.0 ));
	DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
	
	minU = nAttr.create( "minU", "miu", MFnNumericData::kDouble , 0, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 0.0 ));
	DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
	
	maxU = nAttr.create( "maxU", "mau", MFnNumericData::kDouble , 0, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 1.0 ));
	DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
	
	minV = nAttr.create( "minV", "miv", MFnNumericData::kDouble , 0, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 0.0 ));
	DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
	
	maxV = nAttr.create( "maxV", "mav", MFnNumericData::kDouble , 0, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 1.0 ));
	DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
	
	
	
    UVType = enumFn.create( "UVType", "uvt", 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(enumFn.addField( "Keep", 0 ));
    DOMMY_RETURN_IF_FAILURE(enumFn.addField( "Dot", 1 ));
	DOMMY_RETURN_IF_FAILURE(enumFn.addField( "Tile", 2 ));
    DOMMY_RETURN_IF_FAILURE(enumFn.setKeyable( true ));
    
    
    
    
    //Animation matrix attribute
    
    MFnMatrixAttribute matAttr;
    
    animMatrix = matAttr.create( "animMatrix", "amat",MFnMatrixAttribute::kDouble , &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(matAttr.setStorable( true ));
    DOMMY_RETURN_IF_FAILURE(matAttr.setKeyable( false ));
    DOMMY_RETURN_IF_FAILURE(matAttr.setReadable( true ));
    DOMMY_RETURN_IF_FAILURE(matAttr.setWritable( true ));
    DOMMY_RETURN_IF_FAILURE(matAttr.setCached( false ));
	
	
	
	timeMapColorR = nAttr.create( "timeMapColorR", "tmcr", MFnNumericData::kFloat, 0, &stat );
	DOMMY_RETURN_IF_FAILURE( stat );
	DOMMY_RETURN_IF_FAILURE( nAttr.setKeyable( true ) );
	DOMMY_RETURN_IF_FAILURE( nAttr.setStorable( true ) );
	DOMMY_RETURN_IF_FAILURE( nAttr.setDefault( 0.0f ) );
	
	timeMapColorG = nAttr.create( "timeMapColorG", "tmcg", MFnNumericData::kFloat, 0, &stat );
	DOMMY_RETURN_IF_FAILURE( stat );
	DOMMY_RETURN_IF_FAILURE( nAttr.setKeyable( true ) );
	DOMMY_RETURN_IF_FAILURE( nAttr.setStorable( true ) );
	DOMMY_RETURN_IF_FAILURE( nAttr.setDefault( 0.0f ) );
	
	timeMapColorB = nAttr.create( "timeMapColorB", "tmcb", MFnNumericData::kFloat, 0, &stat );
	DOMMY_RETURN_IF_FAILURE( stat);
	DOMMY_RETURN_IF_FAILURE( nAttr.setKeyable( true ) );
	DOMMY_RETURN_IF_FAILURE( nAttr.setStorable( true ) );
	DOMMY_RETURN_IF_FAILURE( nAttr.setDefault( 0.0f ) );
    
	timeMapColor = nAttr.create( "timeMapColor", "tmc", timeMapColorR, timeMapColorG, timeMapColorB, &stat );
	DOMMY_RETURN_IF_FAILURE( stat );
	DOMMY_RETURN_IF_FAILURE( nAttr.setKeyable( true ) );
	DOMMY_RETURN_IF_FAILURE( nAttr.setStorable( true ) );
	DOMMY_RETURN_IF_FAILURE( nAttr.setDefault( 0.0f, 0.0f, 0.0f ) );
	DOMMY_RETURN_IF_FAILURE( nAttr.setUsedAsColor( true ) );
	timeOffsetMapColorR = nAttr.create( "timeOffsetMapColorR", "tocr", MFnNumericData::kFloat, 0, &stat );
	DOMMY_RETURN_IF_FAILURE( stat );
	DOMMY_RETURN_IF_FAILURE( nAttr.setKeyable( true ) );
	DOMMY_RETURN_IF_FAILURE( nAttr.setStorable( true ) );
	DOMMY_RETURN_IF_FAILURE( nAttr.setDefault( 0.0f ) );
	
	timeOffsetMapColorG = nAttr.create( "timeOffsetMapColorG", "tocg", MFnNumericData::kFloat, 0, &stat );
	DOMMY_RETURN_IF_FAILURE( stat );
	DOMMY_RETURN_IF_FAILURE( nAttr.setKeyable( true ) );
	DOMMY_RETURN_IF_FAILURE( nAttr.setStorable( true ) );
	DOMMY_RETURN_IF_FAILURE( nAttr.setDefault( 0.0f ) );
	
	timeOffsetMapColorB = nAttr.create( "timeOffsetMapColorB", "tocb", MFnNumericData::kFloat, 0, &stat );
	DOMMY_RETURN_IF_FAILURE( stat);
	DOMMY_RETURN_IF_FAILURE( nAttr.setKeyable( true ) );
	DOMMY_RETURN_IF_FAILURE( nAttr.setStorable( true ) );
	DOMMY_RETURN_IF_FAILURE( nAttr.setDefault( 0.0f ) );
	
	timeOffsetMapColor = nAttr.create( "timeOffsetMapColor", "toc", timeOffsetMapColorR, timeOffsetMapColorG, timeOffsetMapColorB, &stat );
	DOMMY_RETURN_IF_FAILURE( stat );
	DOMMY_RETURN_IF_FAILURE( nAttr.setKeyable( true ) );
	DOMMY_RETURN_IF_FAILURE( nAttr.setStorable( true ) );
	DOMMY_RETURN_IF_FAILURE( nAttr.setDefault( 0.0f, 0.0f, 0.0f ) );
	DOMMY_RETURN_IF_FAILURE( nAttr.setUsedAsColor( true ) );
	
	
	 //data single mesh type
	outPutMesh = gAttr.create( "outPutMesh", "opm" , &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(gAttr.addDataAccept( MFnData::kMesh ));
	DOMMY_RETURN_IF_FAILURE(gAttr.setHidden( true ));
	DOMMY_RETURN_IF_FAILURE(gAttr.setReadable( true ));
	DOMMY_RETURN_IF_FAILURE(gAttr.setWritable( false ));
	DOMMY_RETURN_IF_FAILURE(gAttr.setStorable( false ));
	
	DOMMY_RETURN_IF_FAILURE(addAttribute( duplicateObjects ));
	DOMMY_RETURN_IF_FAILURE(addAttribute( typeSeed ));
	DOMMY_RETURN_IF_FAILURE(addAttribute( sourcePlane ));
	DOMMY_RETURN_IF_FAILURE(addAttribute( numU ));
	DOMMY_RETURN_IF_FAILURE(addAttribute( numV ));
	
	DOMMY_RETURN_IF_FAILURE(addAttribute( stepU ));
	DOMMY_RETURN_IF_FAILURE(addAttribute( stepV ));
	
	DOMMY_RETURN_IF_FAILURE(addAttribute( randamU ));
	DOMMY_RETURN_IF_FAILURE(addAttribute( randamV ));
	
	DOMMY_RETURN_IF_FAILURE(addAttribute( aimNormal ));
	DOMMY_RETURN_IF_FAILURE(addAttribute( normalSet ));
	
	DOMMY_RETURN_IF_FAILURE(addAttribute( offsetU ));
	DOMMY_RETURN_IF_FAILURE(addAttribute( offsetV ));
	
	DOMMY_RETURN_IF_FAILURE(addAttribute( tiltU ));
	DOMMY_RETURN_IF_FAILURE(addAttribute( tiltV ));
	
	DOMMY_RETURN_IF_FAILURE(addAttribute( tiltColor ));
	
	DOMMY_RETURN_IF_FAILURE(addAttribute( randamRotX ));
	DOMMY_RETURN_IF_FAILURE(addAttribute( randamRotY ));
	DOMMY_RETURN_IF_FAILURE(addAttribute( randamRotZ ));
	
	
	DOMMY_RETURN_IF_FAILURE(addAttribute( scaleX ));
	DOMMY_RETURN_IF_FAILURE(addAttribute( scaleY ));
	DOMMY_RETURN_IF_FAILURE(addAttribute( scaleZ ));
	
	
	DOMMY_RETURN_IF_FAILURE(addAttribute( randamScaleX ));
	DOMMY_RETURN_IF_FAILURE(addAttribute( randamScaleY ));
	DOMMY_RETURN_IF_FAILURE(addAttribute( randamScaleZ ));
	
	DOMMY_RETURN_IF_FAILURE(addAttribute( baldnessColor ));
	
	DOMMY_RETURN_IF_FAILURE(addAttribute( globalHeightOffset ));
	DOMMY_RETURN_IF_FAILURE(addAttribute( heightColor ));
	
	
	DOMMY_RETURN_IF_FAILURE(addAttribute( scaleColorX ));
	DOMMY_RETURN_IF_FAILURE(addAttribute( scaleColorY ));
	DOMMY_RETURN_IF_FAILURE(addAttribute( scaleColorZ ));
	DOMMY_RETURN_IF_FAILURE(addAttribute( scaleColor ));
	
	DOMMY_RETURN_IF_FAILURE(addAttribute( UVsize ));
	DOMMY_RETURN_IF_FAILURE(addAttribute( minU ));
	DOMMY_RETURN_IF_FAILURE(addAttribute( maxU ));
	DOMMY_RETURN_IF_FAILURE(addAttribute( minV ));
	DOMMY_RETURN_IF_FAILURE(addAttribute( maxV ));
    DOMMY_RETURN_IF_FAILURE(addAttribute( UVType ));
    
    DOMMY_RETURN_IF_FAILURE(addAttribute(animMatrix));
    DOMMY_RETURN_IF_FAILURE(addAttribute(timeMapColor));
	DOMMY_RETURN_IF_FAILURE(addAttribute(timeOffsetMapColor));
	
	
	DOMMY_RETURN_IF_FAILURE(addAttribute( outPutMesh ));
	
	
	DOMMY_RETURN_IF_FAILURE( attributeAffects( typeSeed, outPutMesh ) );
	DOMMY_RETURN_IF_FAILURE( attributeAffects( sourcePlane, outPutMesh ) );
	DOMMY_RETURN_IF_FAILURE( attributeAffects( numU, outPutMesh ) );
	DOMMY_RETURN_IF_FAILURE( attributeAffects( numV, outPutMesh ) );
	
	DOMMY_RETURN_IF_FAILURE( attributeAffects( stepU, outPutMesh ) );
	DOMMY_RETURN_IF_FAILURE( attributeAffects( stepV, outPutMesh ) );
	
	DOMMY_RETURN_IF_FAILURE( attributeAffects( randamU, outPutMesh ) );
	DOMMY_RETURN_IF_FAILURE( attributeAffects( randamV, outPutMesh ) );
	
	
	DOMMY_RETURN_IF_FAILURE( attributeAffects( aimNormal, outPutMesh ) );
	DOMMY_RETURN_IF_FAILURE( attributeAffects( normalSet, outPutMesh ) );
	
	DOMMY_RETURN_IF_FAILURE( attributeAffects( offsetU, outPutMesh ) );
	DOMMY_RETURN_IF_FAILURE( attributeAffects( offsetV, outPutMesh ) );
	
	DOMMY_RETURN_IF_FAILURE( attributeAffects( tiltU, outPutMesh ) );
	DOMMY_RETURN_IF_FAILURE( attributeAffects( tiltV, outPutMesh ) );
	
	DOMMY_RETURN_IF_FAILURE( attributeAffects( tiltColor, outPutMesh ) );
	DOMMY_RETURN_IF_FAILURE( attributeAffects( tiltColorR, outPutMesh ) );
	DOMMY_RETURN_IF_FAILURE( attributeAffects( tiltColorG, outPutMesh ) );
	DOMMY_RETURN_IF_FAILURE( attributeAffects( tiltColorB, outPutMesh ) );
	
	DOMMY_RETURN_IF_FAILURE( attributeAffects( randamRotX, outPutMesh ) );
	DOMMY_RETURN_IF_FAILURE( attributeAffects( randamRotY, outPutMesh ) );
	DOMMY_RETURN_IF_FAILURE( attributeAffects( randamRotZ, outPutMesh ) );
	
	DOMMY_RETURN_IF_FAILURE( attributeAffects( scaleX, outPutMesh ) );
	DOMMY_RETURN_IF_FAILURE( attributeAffects( scaleY, outPutMesh ) );
	DOMMY_RETURN_IF_FAILURE( attributeAffects( scaleZ, outPutMesh ) );
	
	DOMMY_RETURN_IF_FAILURE( attributeAffects( randamScaleX, outPutMesh ) );
	DOMMY_RETURN_IF_FAILURE( attributeAffects( randamScaleY, outPutMesh ) );
	DOMMY_RETURN_IF_FAILURE( attributeAffects( randamScaleZ, outPutMesh ) );
	
	DOMMY_RETURN_IF_FAILURE( attributeAffects( baldnessColor, outPutMesh ) );
	DOMMY_RETURN_IF_FAILURE( attributeAffects( baldnessColorR, outPutMesh ) );
	DOMMY_RETURN_IF_FAILURE( attributeAffects( baldnessColorG, outPutMesh ) );
	DOMMY_RETURN_IF_FAILURE( attributeAffects( baldnessColorB, outPutMesh ) );
	
	DOMMY_RETURN_IF_FAILURE( attributeAffects( globalHeightOffset, outPutMesh ) );
	DOMMY_RETURN_IF_FAILURE( attributeAffects( heightColor, outPutMesh ) );
	DOMMY_RETURN_IF_FAILURE( attributeAffects( heightColorR, outPutMesh ) );
	DOMMY_RETURN_IF_FAILURE( attributeAffects( heightColorG, outPutMesh ) );
	DOMMY_RETURN_IF_FAILURE( attributeAffects( heightColorB, outPutMesh ) );
	
	DOMMY_RETURN_IF_FAILURE( attributeAffects( scaleColorX, outPutMesh ) );
	DOMMY_RETURN_IF_FAILURE( attributeAffects( scaleColorY, outPutMesh ) );
	DOMMY_RETURN_IF_FAILURE( attributeAffects( scaleColorZ, outPutMesh ) );
	DOMMY_RETURN_IF_FAILURE( attributeAffects( scaleColor, outPutMesh ) );
	DOMMY_RETURN_IF_FAILURE( attributeAffects( scaleColorR, outPutMesh ) );
	DOMMY_RETURN_IF_FAILURE( attributeAffects( scaleColorG, outPutMesh ) );
	DOMMY_RETURN_IF_FAILURE( attributeAffects( scaleColorB, outPutMesh ) );
	
	DOMMY_RETURN_IF_FAILURE(attributeAffects( UVsize, outPutMesh ));
	DOMMY_RETURN_IF_FAILURE(attributeAffects( minU, outPutMesh ));
	DOMMY_RETURN_IF_FAILURE(attributeAffects( maxU, outPutMesh ));
	DOMMY_RETURN_IF_FAILURE(attributeAffects( minV, outPutMesh ));
	DOMMY_RETURN_IF_FAILURE(attributeAffects( maxV, outPutMesh ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(UVType, outPutMesh));
    
    DOMMY_RETURN_IF_FAILURE(attributeAffects(animMatrix, outPutMesh));
	
    DOMMY_RETURN_IF_FAILURE( attributeAffects( timeMapColor, outPutMesh ));
	DOMMY_RETURN_IF_FAILURE( attributeAffects( timeMapColorR, outPutMesh ) );
	DOMMY_RETURN_IF_FAILURE( attributeAffects( timeMapColorG, outPutMesh ) );
	DOMMY_RETURN_IF_FAILURE( attributeAffects( timeMapColorB, outPutMesh ) );
	
	
	DOMMY_RETURN_IF_FAILURE( attributeAffects( timeOffsetMapColor, outPutMesh ) );
	DOMMY_RETURN_IF_FAILURE( attributeAffects( timeOffsetMapColorR, outPutMesh ) );
	DOMMY_RETURN_IF_FAILURE( attributeAffects( timeOffsetMapColorG, outPutMesh ) );
	DOMMY_RETURN_IF_FAILURE( attributeAffects( timeOffsetMapColorB, outPutMesh ) );
	
	return MS::kSuccess;
}
catch (...) {
    return dommy::maya::reportEntryPointException(
        "surfaceDuplicater", "initialize", std::current_exception());
}


MStatus surfaceDuplicater::getColorFromTex(
	MObject texNode, const MFloatArray& colUCoords,
	const MFloatArray& colVCoords, MVectorArray& colorArray,
	MDoubleArray& transArray) {
	if (colUCoords.length() != colVCoords.length()) {
		return MS::kInvalidParameter;
	}

	MDoubleArray uCoordinates;
	MDoubleArray vCoordinates;
	MStatus status = uCoordinates.setLength(colUCoords.length());
	if (!status) return status;
	status = vCoordinates.setLength(colVCoords.length());
	if (!status) return status;
	for (unsigned int index = 0U; index < colUCoords.length(); ++index) {
		status = uCoordinates.set(colUCoords[index], index);
		if (!status) return status;
		status = vCoordinates.set(colVCoords[index], index);
		if (!status) return status;
	}

	return evaluate2dTextureInBatches(
		texNode, uCoordinates, vCoordinates, colorArray, transArray);
}
