//
//  DPMeshDuplicator.cpp
//  dommyTool
//
//  Created by yuzo on 3/1/16.
//
//

#include "DPMeshDuplicator.h"
#include "common/utility/DMayaEntryPointGuard.h"
#include "common/utility/DStatusOperation.h"
#include "DMeshDuplicationPlan.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DGenerationLimits.h"
#include "common/utility/DMayaPluginData.h"
#include "common/utility/DMayaPluginDataOutput.h"
#include "common/utility/DMayaValueValidation.h"
#include "common/utility/DRandomValueGeneration.h"
#include "common/utility/DUvParameter.h"
#include "common/utility/DPointPayloadBudget.h"


#include "dupMesh.h"
#include "DPointDataHeaders.h"
#include "DPAttributeAccess.h"
#include "DMeshDataHeaders.h"



#include "dommyMath.h"



#include "DMayaHeaders.h"
#include "common/utility/DEvaluationContext.h"


#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <map>
#include <utility>
#include <vector>

namespace {

void preflightDuplicationAttributes(const DPData& input, const MStringArray& requested,
                                    dommy::point::PointPayloadBudget& workspace) {
    std::size_t work = 0U;
    if (requested.length() > dommy::generation::kMaxGeneratedStringCount ||
        !dommy::generation::checkedProduct(requested.length(), input.numberOfPoints(),
                                           dommy::generation::kMaxWorkItemCount, work))
        throw std::length_error("DPMeshDuplicator: attribute transfer exceeds its work budget");
    std::map<std::string, DPAttributeData::ConstPtr> selected;
    for (unsigned int index = 0U; index < requested.length(); ++index) {
        const char* const bytes = requested[index].asChar();
        workspace.add(std::strlen(bytes), sizeof(char));
        const std::string name = bytes;
        const auto field = input.getAttr(name);
        // Preserve missing-field and unsupported-array warning/skip behavior.
        if (!field || field->getAttrType() == DP_ATTR_INT_ARRAY) continue;
        if (dommy::point::pointAttributeStorageBytes(field->getAttrType()) == 0U)
            throw std::invalid_argument("DPMeshDuplicator: invalid transfer attribute type");
        selected.emplace(name, field);
    }
    dommy::point::preflightAttributePayload(selected.begin(), selected.end(),
                                           input.numberOfPoints(), workspace, false);
}

} // namespace





const MTypeId DPMeshDuplicator::typeId( ID_DP_MESH_DUPLICATOR );


//Attribute
MObject DPMeshDuplicator::inDP;

MObject DPMeshDuplicator::inDM;

MObject DPMeshDuplicator::aInObjects;


MObject DPMeshDuplicator::aObjectType;
MObject DPMeshDuplicator::aObjectNumber;
MObject DPMeshDuplicator::aSeed;
MObject DPMeshDuplicator::aObjectIdAttribute;


//normal and tangent
MObject DPMeshDuplicator::aAimVector;
MObject DPMeshDuplicator::aAxisVector;
MObject DPMeshDuplicator::aNormalVector;
MObject DPMeshDuplicator::aNormalAttribute;
MObject DPMeshDuplicator::aTangentVector;
MObject DPMeshDuplicator::aTangentAttribute;

//animation matrix
MObject DPMeshDuplicator::aAnimationType;
MObject DPMeshDuplicator::aAnimMatrixAttribute;
MObject DPMeshDuplicator::aInAnimMatrix;
MObject DPMeshDuplicator::aMatrixAssignType;

MObject DPMeshDuplicator::aLocalTranslate;
MObject DPMeshDuplicator::aGlobalTranslate;
MObject DPMeshDuplicator::aLocalRotate;
MObject DPMeshDuplicator::aGlobalRotate;
MObject DPMeshDuplicator::aGlobalScale;
MObject DPMeshDuplicator::aUniformScale;

MObject DPMeshDuplicator::aIsOutUV;
MObject DPMeshDuplicator::aUVType;

//Attribute
MObject DPMeshDuplicator::aLocalTranslateAttribute;
MObject DPMeshDuplicator::aGlobalTranslateAttribute;
MObject DPMeshDuplicator::aLocalRotateAttribute;
MObject DPMeshDuplicator::aGlobalRotateAttribute;
MObject DPMeshDuplicator::aGlobalScaleAttribute;
MObject DPMeshDuplicator::aUniformScaleAttribute;

//random

MObject DPMeshDuplicator::aLocalTranslateRandomMin;
MObject DPMeshDuplicator::aLocalTranslateRandomMax;
MObject DPMeshDuplicator::aGlobalTranslateRandomMin;
MObject DPMeshDuplicator::aGlobalTranslateRandomMax;
MObject DPMeshDuplicator::aLocalRotateRandomMin;
MObject DPMeshDuplicator::aLocalRotateRandomMax;
MObject DPMeshDuplicator::aGlobalRotateRandomMin;
MObject DPMeshDuplicator::aGlobalRotateRandomMax;
MObject DPMeshDuplicator::aGlobalScaleRandomMin;
MObject DPMeshDuplicator::aGlobalScaleRandomMax;
MObject DPMeshDuplicator::aUniformScaleRandomMinMax;

//transfer
MObject DPMeshDuplicator::aTransferAttributes;


MObject DPMeshDuplicator::aExpression;

MObject DPMeshDuplicator::outDM;



void *DPMeshDuplicator::creator() {
    return ::dommy::maya::createObject("DPMeshDuplicator", [&]() {
        return new DPMeshDuplicator();
    });
}

MStatus DPMeshDuplicator::evalAnimMatrixParms(MDataBlock& data, DTransformParams& parms) {
    // Preserve unrelated fields and commit only after every input is available.
    DTransformParams candidate = parms;
    MStatus status;
    const auto read = [&](const MObject& attribute) {
        return data.inputValue(attribute, &status);
    };
    MDataHandle handle = read(aAnimationType);
    DOMMY_RETURN_IF_FAILURE(status);
    candidate.mAnimMatrixType = handle.asShort();
    handle = read(aAnimMatrixAttribute);
    DOMMY_RETURN_IF_FAILURE(status);
    candidate.mAnimationAttribute = handle.asString().asChar();
    candidate.mNodeObj = thisMObject();
    candidate.mAnim_Mat_InputData_Object = aInAnimMatrix;
    candidate.mAnimationInputIsData = false;

    if (candidate.mAnimMatrixType == MAT_ANIM_INPUT_DATA) {
        const MPlug matrixPlug(thisMObject(), aInAnimMatrix);
        const bool connected = matrixPlug.isConnected(&status);
        DOMMY_RETURN_IF_FAILURE(status);
        if (!connected) {
            candidate.mAnimMatrixType = MAT_ANIM_NONE;
            EchoWarning(name().asChar(), "Not connected anim mat plug.");
        } else {
            handle = read(aInAnimMatrix);
            DOMMY_RETURN_IF_FAILURE(status);
            if (handle.type() != MFnData::kMatrixArray || handle.data().isNull())
                return MS::kInvalidParameter;
            candidate.mAnim_Mat_InputData_Object = handle.data();
            candidate.mAnimationInputIsData = true;
        }
    }

    handle = read(aAimVector);
    DOMMY_RETURN_IF_FAILURE(status);
    candidate.mNormalBaseAxis = handle.asVector();
    handle = read(aAxisVector);
    DOMMY_RETURN_IF_FAILURE(status);
    candidate.mTangentBaseAxis = handle.asVector();
    handle = read(aNormalVector);
    DOMMY_RETURN_IF_FAILURE(status);
    candidate.mNormalVec = handle.asVector();
    handle = read(aTangentVector);
    DOMMY_RETURN_IF_FAILURE(status);
    candidate.mTangentVec = handle.asVector();
    handle = read(aNormalAttribute);
    DOMMY_RETURN_IF_FAILURE(status);
    candidate.mNormalVecAttribute = handle.asString().asChar();
    handle = read(aTangentAttribute);
    DOMMY_RETURN_IF_FAILURE(status);
    candidate.mTangentVecAttribute = handle.asString().asChar();

    handle = read(aSeed);
    DOMMY_RETURN_IF_FAILURE(status);
    candidate.mSeed = handle.asInt();
    handle = read(aLocalTranslate);
    DOMMY_RETURN_IF_FAILURE(status);
    candidate.mLocalTranslate = MVector(handle.asDouble3());
    handle = read(aGlobalTranslate);
    DOMMY_RETURN_IF_FAILURE(status);
    candidate.mGlobalTranslate = MVector(handle.asDouble3());
    handle = read(aLocalRotate);
    DOMMY_RETURN_IF_FAILURE(status);
    candidate.mLocalRot = MVector(handle.asDouble3());
    handle = read(aGlobalRotate);
    DOMMY_RETURN_IF_FAILURE(status);
    candidate.mGlobalRot = MVector(handle.asDouble3());
    handle = read(aGlobalScale);
    DOMMY_RETURN_IF_FAILURE(status);
    candidate.mGlobalScale = MVector(handle.asDouble3());
    handle = read(aUniformScale);
    DOMMY_RETURN_IF_FAILURE(status);
    candidate.mUniformScale = handle.asDouble();

    handle = read(aLocalTranslateAttribute);
    DOMMY_RETURN_IF_FAILURE(status);
    candidate.mLocalTransAttribute = handle.asString().asChar();
    handle = read(aGlobalTranslateAttribute);
    DOMMY_RETURN_IF_FAILURE(status);
    candidate.mGlobalTransAttribute = handle.asString().asChar();
    handle = read(aLocalRotateAttribute);
    DOMMY_RETURN_IF_FAILURE(status);
    candidate.mLocalRotAttribute = handle.asString().asChar();
    handle = read(aGlobalRotateAttribute);
    DOMMY_RETURN_IF_FAILURE(status);
    candidate.mGlobalRotAttribute = handle.asString().asChar();
    handle = read(aGlobalScaleAttribute);
    DOMMY_RETURN_IF_FAILURE(status);
    candidate.mGlobalScaleAttribute = handle.asString().asChar();
    handle = read(aUniformScaleAttribute);
    DOMMY_RETURN_IF_FAILURE(status);
    candidate.mUniformScaleAttribute = handle.asString().asChar();

    handle = read(aLocalTranslateRandomMin);
    DOMMY_RETURN_IF_FAILURE(status);
    candidate.mLocalTransRndMin = MVector(handle.asDouble3());
    handle = read(aLocalTranslateRandomMax);
    DOMMY_RETURN_IF_FAILURE(status);
    candidate.mLocalTransRndMax = MVector(handle.asDouble3());
    handle = read(aGlobalTranslateRandomMin);
    DOMMY_RETURN_IF_FAILURE(status);
    candidate.mGlobalTransRndMin = MVector(handle.asDouble3());
    handle = read(aGlobalTranslateRandomMax);
    DOMMY_RETURN_IF_FAILURE(status);
    candidate.mGlobalTransRndMax = MVector(handle.asDouble3());
    handle = read(aLocalRotateRandomMin);
    DOMMY_RETURN_IF_FAILURE(status);
    candidate.mLocalRotRndMin = MVector(handle.asDouble3());
    handle = read(aLocalRotateRandomMax);
    DOMMY_RETURN_IF_FAILURE(status);
    candidate.mLocalRotRndMax = MVector(handle.asDouble3());
    handle = read(aGlobalRotateRandomMin);
    DOMMY_RETURN_IF_FAILURE(status);
    candidate.mGlobalRotRndMin = MVector(handle.asDouble3());
    handle = read(aGlobalRotateRandomMax);
    DOMMY_RETURN_IF_FAILURE(status);
    candidate.mGlobalRotRndMax = MVector(handle.asDouble3());
    handle = read(aGlobalScaleRandomMin);
    DOMMY_RETURN_IF_FAILURE(status);
    candidate.mGlobalScaleRndMin = MVector(handle.asDouble3());
    handle = read(aGlobalScaleRandomMax);
    DOMMY_RETURN_IF_FAILURE(status);
    candidate.mGlobalScaleRndMax = MVector(handle.asDouble3());
    handle = read(aUniformScaleRandomMinMax);
    DOMMY_RETURN_IF_FAILURE(status);
    const double2& uniformScaleRange = handle.asDouble2();
    candidate.mUniformScaleMinMax = MVector(uniformScaleRange[0], uniformScaleRange[1], 0.0);

    parms = std::move(candidate);
    return MS::kSuccess;
}


MStatus DPMeshDuplicator::compute( const MPlug& plug, MDataBlock& data ) try
{
	MStatus stat;
    

	if(plug == outDM) {
		
#pragma mark get attribute value アトリビュート値の取得
        const MDataHandle objectTypeHandle = data.inputValue(aObjectType, &stat);
        if (!stat) return stat;
        const short objType = objectTypeHandle.asShort();
        const MDataHandle objectNumberHandle = data.inputValue(aObjectNumber, &stat);
        if (!stat) return stat;
        const int objNumber = objectNumberHandle.asInt();
        
        const MDataHandle seedHandle = data.inputValue(aSeed, &stat);
        if (!stat) return stat;
        const int seed = seedHandle.asInt();
        
        const MDataHandle objectAttributeHandle = data.inputValue(aObjectIdAttribute, &stat);
        if (!stat) return stat;
        const std::string objAttrName = objectAttributeHandle.asString().asChar();
        const MDataHandle matrixAssignmentHandle = data.inputValue(aMatrixAssignType, &stat);
        if (!stat) return stat;
        const short matrixAssignType = matrixAssignmentHandle.asShort();

        if (objType < 0 || objType > 2 ||
            matrixAssignType < 0 || matrixAssignType > 1) {
            MGlobal::displayError(
                "DPMeshDuplicator: invalid object or matrix assignment mode.");
            return MS::kInvalidParameter;
        }
        
        const MDataHandle transferHandle = data.inputValue(aTransferAttributes, &stat);
        if (!stat) return stat;
        const MString transAttrStr = transferHandle.asString();
        
        
        MStringArray transAttrNameArray;

        dommy::point::PointPayloadBudget transferNameStorage;
        transferNameStorage.add(std::strlen(transAttrStr.asChar()), sizeof(char));
        DOMMY_RETURN_IF_FAILURE(transAttrStr.split(' ', transAttrNameArray));
        
        
		const MDataHandle outputUVHandle = data.inputValue(aIsOutUV, &stat);
		if (!stat) return stat;
		const bool aIsOutUVVal = outputUVHandle.asBool();
		const MDataHandle uvTypeHandle = data.inputValue(aUVType, &stat);
		if (!stat) return stat;
		const int aUVTypeVal = uvTypeHandle.asInt();
		if (aUVTypeVal < 0 || aUVTypeVal > 2) {
			MGlobal::displayError("DPMeshDuplicator: invalid UV layout mode.");
			return MS::kInvalidParameter;
		}
        
        
        
#pragma mark input duplicate Mesh 複製するメッシュの取得
			std::vector<dupMesh> dupShapeArray;
        
        MPlug indmPlug(thisMObject(), inDM);
        const bool hasMeshInput = indmPlug.isConnected(&stat);
        if (!stat) return stat;
        if (hasMeshInput) {
            
            const DMeshData* inputDMData = nullptr;
            DOMMY_RETURN_IF_FAILURE(getValidInputDM(inDM, data, inputDMData));
            
            if (inputDMData && inputDMData->numberOfMesh()) {
                
                for (auto mIt=inputDMData->beginPrims(),emIt=inputDMData->endPrims(); mIt!=emIt; ++mIt) {
					dupShapeArray.emplace_back(*mIt);
					if (!dupShapeArray.back().isValid()) {
						MGlobal::displayError(
							"DPMeshDuplicator: DMesh input contains invalid or empty topology.");
						return MS::kInvalidParameter;
					}
                }
                
            }
            
            
        } else {
            //get inObject plug
            
            MArrayDataHandle arrayHandle = data.inputArrayValue(aInObjects, &stat);
            if (!stat) return stat;
            const unsigned int count = arrayHandle.elementCount(&stat);
            if (!stat) return stat;
            
            for(uint i=0;i<count;i++) {
				if (i > 0U) {
					stat = arrayHandle.next();
					if (!stat) return stat;
				}
                MDataHandle dupObjHnd = arrayHandle.inputValue(&stat);
				if (!stat) return stat;
                if (!dupObjHnd.data().isNull()) {
					dupShapeArray.emplace_back(dupObjHnd.data());
					if (!dupShapeArray.back().isValid()) {
						MGlobal::displayError(
							"DPMeshDuplicator: Maya mesh input is invalid or empty.");
						return MS::kInvalidParameter;
					}
                }
            }
        }
		
		
		
			if (dupShapeArray.size()<1) {
            EchoWarning(name().asChar(), "Duplicate mesh is zero. check in inObjects plug data.");
			return MS::kFailure;
		}
		if (dupShapeArray.size() >
			static_cast<size_t>(std::numeric_limits<int>::max())) {
			MGlobal::displayError(
				"DPMeshDuplicator: too many source meshes.");
			return MS::kInvalidParameter;
		}
		
		
#pragma mark inDPの取得
		
		const DPData* inputDPData = nullptr;
		DOMMY_RETURN_IF_FAILURE(getValidInputDP(inDP, data, inputDPData));
		if (!inputDPData) {
			MGlobal::displayError(
				"DPMeshDuplicator: input point data is missing or invalid.");
			return MS::kInvalidParameter;
		}
		
		{
            
            
#pragma mark Get parameter
            DTransformParams params;
            
            stat = evalAnimMatrixParms(data, params);
            if (!stat) {
                EchoWarning(name().asChar(), "Failed read parameter.");
                return stat;
            }
            
#pragma mark outDMの作成
            MFnPluginData pluginData;
            DMeshData* outDMeshes = NULL;
            DOMMY_RETURN_IF_FAILURE(dommy::maya::createPluginData(
                pluginData, DMeshData::id, outDMeshes));
            
            if (outDMeshes) {
                
                //get position
                const DPointList *pts = inputDPData->getPositionPtrConst();
                if (!pts) return MS::kFailure;
                const size_t totalCount = pts->size();
                if (totalCount >
                    dommy::generation::kMaxGeneratedTransformCount) {
                    MGlobal::displayError(
                        "DPMeshDuplicator: point count exceeds the safe output limit.");
                    return MS::kInvalidParameter;
                }
                
                
                if (totalCount) {
                    
#pragma mark Generate Matrix
                    // The generator admits and builds its detached result.
                    // A pre-sized destination would retain a second matrix array.
                    DMatrixArrayPtr matrixArray(new std::vector<MMatrix>());
                    
                    
                    DOMMY_RETURN_IF_FAILURE(
                        DGenerateTransformMatrixFromDP(
                            inputDPData, params, *matrixArray));
                    
                    
#pragma mark eval expression

                    const MDataHandle expressionHandle = data.inputValue(aExpression, &stat);
                    CHECK_MSTATUS_AND_RETURN_IT(stat);
                    const std::string exprStr = expressionHandle.asString().asChar();

                    std::vector<MVector> scaleExprArray(totalCount, MVector(1,1,1));
                    
                    if (!exprStr.empty()) {
                        
                        //create expression
                        DPointSeExpr expr(exprStr);
                        
                        MTime currentTime;
                        DOMMY_RETURN_IF_FAILURE(dommy::maya::getEvaluationTime(data, currentTime));
                        DOMMY_RETURN_IF_FAILURE(dommy::maya::setExpressionTime(expr, currentTime));

                        if (!expr.isValid()) {
                            EchoWarning(
                                name().asChar(),
                                std::string("Expression is invalid: ") +
                                    expr.parseError());
                            return MS::kInvalidParameter;
                        }
                        if (expr.usesPointDataFunctions()) {
                            MGlobal::displayError("DPMeshDuplicator: expression does not support "
                                                  "point-data functions.");
                            return MS::kInvalidParameter;
                        }

                        for (size_t i = 0U; i < totalCount; ++i) {
                            expr.setId(i);
                            expr.setPos((*pts)[i]);
                            SeVec3d expressionValue;
                            if (!dommy::expression::evaluateChecked(expr, expressionValue)) {
                                MGlobal::displayError(
                                    "DPMeshDuplicator: expression evaluation failed.");
                                return MS::kInvalidParameter;
                            }

                            const DPointSeExpr::LocalVarTable& localMap = expr.getLocalVars();
                            const DPointSeExpr::LocalVarTable::const_iterator scale =
                                localMap.find("scale");
                            if (scale == localMap.end()) continue;

                            MVector value;
                            if (!dommy::expression::readScale(scale->second, value)) {
                                MGlobal::displayError(
                                    "DPMeshDuplicator: expression produced a non-finite scale.");
                                return MS::kInvalidParameter;
                            }
                            scaleExprArray[i] = value;
                        }
                    }
                    
                    
                    
                    
                    
                    
#pragma mark メッシュの複製
#pragma mark run duplicate
                    const double alongUVRatio = 1.0 / static_cast<double>(totalCount);
                    const size_t numTileU = static_cast<size_t>(
                        std::ceil(std::sqrt(static_cast<double>(totalCount))));
                    const size_t numTileV = numTileU;
                    
                    
                    std::vector<int> objTypeIds(
                        totalCount,
                        dommy::mesh::clampedDuplicationSourceIndex(
                            objNumber, dupShapeArray.size()));
                    
                    if (objType==1 && dupShapeArray.size()>1) {
                        dommy::random::generateIntValues(
                            seed, 0,
                            static_cast<int>(dupShapeArray.size() - 1U),
                            totalCount, objTypeIds);
                        
                    } else if (objType==2) {
                        
                        if (inputDPData->isExistAttr(objAttrName)) {
                            MIntArray sourceIds;
                            if (!getCheckedMIntArrayFromAttribute(
                                    *inputDPData, objAttrName, totalCount,
                                    sourceIds)) {
                                MGlobal::displayError(
                                    "DPMeshDuplicator: object ID attribute has an incompatible type or size.");
                                return MS::kInvalidParameter;
                            }
                            for (size_t index = 0U; index < totalCount; ++index) {
                                objTypeIds[index] =
                                    sourceIds[static_cast<unsigned int>(index)];
                            }
                        }
                    }

                    if (!dommy::mesh::normalizeDuplicationSourceIndices(
                            dupShapeArray.size(), objTypeIds)) {
                        return MS::kInvalidParameter;
                    }

                    std::vector<dommy::mesh::DuplicationSourceSize> sourceSizes;
                    sourceSizes.reserve(dupShapeArray.size());
                    for (std::vector<dupMesh>::const_iterator source =
                             dupShapeArray.begin();
                         source != dupShapeArray.end(); ++source) {
                        sourceSizes.push_back(
                            dommy::mesh::DuplicationSourceSize(
                                source->getTotalVertexNum(),
                                source->getTotalPolygonNum(),
                                source->getTotalConnectionNum(),
                                source->getTotalUVNum()));
                    }

                    dommy::mesh::MeshDuplicationBudget duplicationBudget;
                    dommy::point::PointPayloadBudget retained = transferNameStorage;
                    retained.add(totalCount, sizeof(MMatrix) + sizeof(MVector) + sizeof(int));
                    preflightDuplicationAttributes(*inputDPData, transAttrNameArray, retained);
                    if (!dommy::mesh::buildMeshDuplicationBudget(
                            sourceSizes, objTypeIds, aIsOutUVVal,
                            duplicationBudget, retained.estimatedBytes())) {
                        MGlobal::displayError(
                            "DPMeshDuplicator: requested mesh output exceeds the safe generation budget.");
                        return MS::kInvalidParameter;
                    }

                    DMeshData::DMeshList outputMeshes;
                    outputMeshes.reserve(duplicationBudget.meshCount);
                    
                    
                    
                    for(size_t i=0;i<totalCount;i++)
                    {
                        //set randam input duplicate object
                        const size_t type =
                            static_cast<size_t>(objTypeIds[i]);
                        
                        
#pragma mark Get base mesh data　頂点データのセット
                        
                        const MFloatPointArray& baseVtxArray = dupShapeArray[type].getVertexArray();
                        DPointList outPtsArray(baseVtxArray.length());
                        
                        DMayaVecArrayToVecVectorList(baseVtxArray, outPtsArray);
#pragma mark Set final vetex List　頂点データのセット
                        MMatrix mat = (*matrixArray)[i];
                        MTransformationMatrix expressionScale;
                        const double expressionScaleValues[3] = {
                            scaleExprArray[i].x,
                            scaleExprArray[i].y,
                            scaleExprArray[i].z};
                        DOMMY_RETURN_IF_FAILURE(expressionScale.setScale(
                            expressionScaleValues, MSpace::kObject));
                        mat = expressionScale.asMatrix() * mat;
                        if (!dommy::maya::isFiniteMatrix(mat)) {
                            MGlobal::displayError(
                                "DPMeshDuplicator: generated transform is not finite.");
                            return MS::kInvalidParameter;
                        }
                        
                        if (matrixAssignType == 1) {
                            // Keep Maya's point multiplication semantics, but
                            // admit float-backed coordinates before narrowing.
                            for (unsigned int p = 0U; p < baseVtxArray.length(); ++p) {
                                const MPoint pt = MPoint(baseVtxArray[p]) * mat;
                                if (!dommy::maya::isFloatRepresentablePoint(pt)) {
                                    MGlobal::displayError(
                                        "DPMeshDuplicator: transformed vertex cannot be "
                                        "represented as a finite float.");
                                    return MS::kInvalidParameter;
                                }
                                for (unsigned int j = 0U; j < 3U; ++j) {
                                    outPtsArray[p][j] = static_cast<float>(pt[j]);
                                }
                            }
                        }
                        
                        
                        
#pragma mark set Poly Count List Set 頂点数のセット
                        const MIntArray& pCList = dupShapeArray[type].getPolyCountsList();
                        
                        
#pragma mark set poly connect list　コネクトリストのセット
                        const MIntArray& pConeList = dupShapeArray[type].getPolyConnectList();
                        
                        
#pragma mark set UV array　UVのセット
                        MFloatArray finUCoords;
                        MFloatArray finVCoords;
                        MIntArray uvIDList;
                        MIntArray uvCList;
                        const bool isUV =
                            aIsOutUVVal && dupShapeArray[type].hasUVData();
                        if (isUV) {
                            const MFloatArray& newUCoords = dupShapeArray[type].getUCoords();
                            const MFloatArray& newVCoords = dupShapeArray[type].getVCoords();
                            
                            
                            
                            for(unsigned int p = 0U, un = newUCoords.length();
                                p < un; ++p)
                            {
                                float outputU = newUCoords[p];
                                float outputV = newVCoords[p];
                                switch (aUVTypeVal) {
                                    case 1://along U
                                    {
                                        outputU = static_cast<float>(
                                            (static_cast<double>(newUCoords[p]) /
                                             static_cast<double>(totalCount)) +
                                            alongUVRatio * static_cast<double>(i));
                                    }
                                        break;
                                    case 2://tile
                                    {
                                        const size_t tileU = i % numTileU;
                                        const size_t tileV = i / numTileU;
                                        if (!dommy::uv::mapTileUV(
                                                newUCoords[p], newVCoords[p],
                                                tileU, tileV, numTileU, numTileV,
                                                outputU, outputV)) return MS::kFailure;
                                    }
                                        break;
                                    default://original
                                        break;
                                }

                                if (!std::isfinite(outputU) ||
                                    !std::isfinite(outputV) ||
                                    finUCoords.append(outputU) != MS::kSuccess ||
                                    finVCoords.append(outputV) != MS::kSuccess) {
                                    return MS::kFailure;
                                }
                            }
                            
                            
                            
                            DOMMY_RETURN_IF_FAILURE(uvCList.copy(dupShapeArray[type].getUVCountsArray()));
                            DOMMY_RETURN_IF_FAILURE(uvIDList.copy(dupShapeArray[type].getUVIdsArray()));
                        }
                        
                        if(isUV) {
                            
                            DMItem item = DMItem(new DMeshInternalData(
                                outPtsArray, pCList, pConeList, finUCoords,
                                finVCoords, uvCList, uvIDList));
                            if (matrixAssignType==0) {
                                //attach motion attach
                                item->setTransformMatrix(mat);
                            }
                            
                            if (!item->hasValidMeshData()) return MS::kFailure;
                            outputMeshes.push_back(item);
                            
                        } else {
                            
                            
                            DMItem item = DMItem(new DMeshInternalData(
                                outPtsArray, pCList, pConeList));
                            
                            if (matrixAssignType==0) {
                                //attach motion attach
                                item->setTransformMatrix(mat);
                            }
                            
                            if (!item->hasValidMeshData()) return MS::kFailure;
                            outputMeshes.push_back(item);
                            
                        }
                        
                        
                    }

                    if (outputMeshes.size() != totalCount ||
                        !outDMeshes->insert(outputMeshes)) {
                        MGlobal::displayError(
                            "DPMeshDuplicator: failed to commit generated meshes.");
                        return MS::kFailure;
                    }
                    
                    if (transAttrNameArray.length()) {
                        
                        bool isNotSupportType = false;
                        
                        for (size_t a=0,AN=transAttrNameArray.length(); a<AN; a++) {
                            
                            if(inputDPData->isExistAttr(transAttrNameArray[a].asChar())){
                                
                                switch (inputDPData->getAttr(transAttrNameArray[a].asChar())->getAttrType()) {
                                    case DP_ATTR_BOOL:
                                        outDMeshes->addDPAttributeValues(transAttrNameArray[a].asChar(), inputDPData->attrListValue<bool>(transAttrNameArray[a].asChar()));
                                        break;
                                    case DP_ATTR_INT:
                                        outDMeshes->addDPAttributeValues(transAttrNameArray[a].asChar(), inputDPData->attrListValue<int32_t>(transAttrNameArray[a].asChar()));
                                        break;
                                    case DP_ATTR_UINT32:
                                        outDMeshes->addDPAttributeValues(transAttrNameArray[a].asChar(), inputDPData->attrListValue<uint32_t>(transAttrNameArray[a].asChar()));
                                        break;
                                    case DP_ATTR_INT64:
                                        outDMeshes->addDPAttributeValues(transAttrNameArray[a].asChar(), inputDPData->attrListValue<int64_t>(transAttrNameArray[a].asChar()));
                                        break;
                                    case DP_ATTR_FLOAT:
                                        outDMeshes->addDPAttributeValues(transAttrNameArray[a].asChar(), inputDPData->attrListValue<float>(transAttrNameArray[a].asChar()));
                                        break;
                                    case DP_ATTR_DOUBLE:
                                        outDMeshes->addDPAttributeValues(transAttrNameArray[a].asChar(), inputDPData->attrListValue<double>(transAttrNameArray[a].asChar()));
                                        break;
                                    case DP_ATTR_VEC2I:
                                        outDMeshes->addDPAttributeValues(transAttrNameArray[a].asChar(), inputDPData->attrListValue<Imath::V2i>(transAttrNameArray[a].asChar()));
                                        break;
                                    case DP_ATTR_VEC2F:
                                        outDMeshes->addDPAttributeValues(transAttrNameArray[a].asChar(), inputDPData->attrListValue<Imath::V2f>(transAttrNameArray[a].asChar()));
                                        break;
                                    case DP_ATTR_VEC2D:
                                        outDMeshes->addDPAttributeValues(transAttrNameArray[a].asChar(), inputDPData->attrListValue<Imath::V2d>(transAttrNameArray[a].asChar()));
                                        break;
                                    case DP_ATTR_VEC3I:
                                        outDMeshes->addDPAttributeValues(transAttrNameArray[a].asChar(), inputDPData->attrListValue<Imath::V3i>(transAttrNameArray[a].asChar()));
                                        break;
                                    case DP_ATTR_VEC3F:
                                        outDMeshes->addDPAttributeValues(transAttrNameArray[a].asChar(), inputDPData->attrListValue<Imath::V3f>(transAttrNameArray[a].asChar()));
                                        break;
                                    case DP_ATTR_VEC3D:
                                        outDMeshes->addDPAttributeValues(transAttrNameArray[a].asChar(), inputDPData->attrListValue<Imath::V3d>(transAttrNameArray[a].asChar()));
                                        break;
                                    case DP_ATTR_STRING:
                                        outDMeshes->addDPAttributeValues(transAttrNameArray[a].asChar(), inputDPData->attrListValue<std::string>(transAttrNameArray[a].asChar()));
                                        break;
                                    case DP_ATTR_MATRIX:
                                        outDMeshes->addDPAttributeValues(transAttrNameArray[a].asChar(), inputDPData->attrListValue<MMatrix>(transAttrNameArray[a].asChar()));
                                        break;
                                    default:
                                        isNotSupportType =true;
                                        break;
                                }
                                
                                
                                
                                
                            }//check attribute exist
                            
                        }//loop transfer attribute name
                        
                        
                        if(isNotSupportType) {
                            EchoWarning(name().asChar(), "Some attribute not support type attribute.");
                        }
                        
                        
                        
                    }
                    
                    outDMeshes->updateBBox();
                }
                
#pragma mark Create output mesh
                if (!outDMeshes->hasValidMeshTopology()) {
                    MGlobal::displayError(
                        "DPMeshDuplicator: generated output topology is invalid.");
                    return MS::kFailure;
                }

                MDataHandle outHandle = data.outputValue(outDM, &stat);
                if (!stat) return stat;
                DOMMY_RETURN_IF_FAILURE(
                    dommy::maya::publishTypedPluginDataObject<DMeshData>(
                        outHandle, pluginData.object(), DMeshData::id));
                return data.setClean(plug);
            }
            
        }
		}
	
	return MS::kUnknownParameter;
}
DOMMY_COMPUTE_CATCH("DPMeshDuplicator")






MStatus DPMeshDuplicator::initialize() try {
	
	MStatus stat;
	MFnNumericAttribute nAttr;
	MFnGenericAttribute gAttr;
	MFnTypedAttribute tAttr;
	MFnEnumAttribute enumFn;
	MFnStringData fnStringData;
    
    
#pragma mark input attribute
	// ----------------------- INPUTS --------------------------
	inDP = tAttr.create("inDP", "idp", DPData::id, MObject::kNullObj, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(tAttr.setReadable(false));
	DOMMY_RETURN_IF_FAILURE(tAttr.setStorable( false ));
	DOMMY_RETURN_IF_FAILURE(addAttribute(inDP));
    
    
    inDM = tAttr.create("inDM", "idm", DMeshData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable( false ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inDM));
	
	
	aInObjects = gAttr.create( "inObjects", "ino" , &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(gAttr.addDataAccept( MFnData::kMesh ));
	DOMMY_RETURN_IF_FAILURE(gAttr.setStorable(false));
	DOMMY_RETURN_IF_FAILURE(gAttr.setReadable( false ));
	DOMMY_RETURN_IF_FAILURE(gAttr.setArray( true ));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aInObjects));
	
	
    
    MFnEnumAttribute objEnumFn;
    aObjectType = objEnumFn.create( "ObjectType", "objty", 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(objEnumFn.addField( "Object Number", 0 ));
    DOMMY_RETURN_IF_FAILURE(objEnumFn.addField( "Random", 1 ));
    DOMMY_RETURN_IF_FAILURE(objEnumFn.addField( "From Attribute", 2 ));
    DOMMY_RETURN_IF_FAILURE(objEnumFn.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aObjectType));
    
    
    aObjectNumber = nAttr.create( "ObjectNumber", "objnum", MFnNumericData::kInt , 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(10));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aObjectNumber));
    
    aSeed = nAttr.create( "Seed", "se", MFnNumericData::kInt , 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aSeed));
    
    
    const MObject aObjectIdAttributeDefaultData = fnStringData.create("oid", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aObjectIdAttribute = tAttr.create("ObjectIdAttribute", "objidattr", MFnData::kString, aObjectIdAttributeDefaultData, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aObjectIdAttribute));
    
    
#pragma mark normal and tangent attribute
    
    aAimVector = nAttr.create( "AimVector", "aimvec", MFnNumericData::k3Double , 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0,1.0,0.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aAimVector));
    
    aAxisVector = nAttr.create( "AxisVector", "axsvec", MFnNumericData::k3Double , 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1.0,0.0,0.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aAxisVector));
    

    aNormalVector = nAttr.create( "NormalVector", "nvec", MFnNumericData::k3Double , 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0,1.0,0.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aNormalVector));
    
    const MObject aNormalAttributeDefaultData = fnStringData.create("N", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aNormalAttribute = tAttr.create("NormalAttribute", "nattr", MFnData::kString, aNormalAttributeDefaultData, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aNormalAttribute));
    
    aTangentVector = nAttr.create( "TangentVector", "tvec", MFnNumericData::k3Double , 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1.0,0.0,0.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aTangentVector));
    
    const MObject aTangentAttributeDefaultData = fnStringData.create("T", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aTangentAttribute = tAttr.create("TangentAttribute", "tattr", MFnData::kString, aTangentAttributeDefaultData, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aTangentAttribute));
	
	
	
	
#pragma mark animation attribute
	
	
	
	aAnimationType = enumFn.create( "AnimationType", "anit", 0, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(enumFn.addField( "None", 0 ));
	DOMMY_RETURN_IF_FAILURE(enumFn.addField( "From Attribute", 1 ));
	DOMMY_RETURN_IF_FAILURE(enumFn.addField( "Input Data", 2 ));
	DOMMY_RETURN_IF_FAILURE(enumFn.setKeyable( true ));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aAnimationType));
	
	MObject aniAttrName = fnStringData.create("amat", &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	aAnimMatrixAttribute = tAttr.create("AnimMatrixAttribute", "amata", MFnData::kString, aniAttrName, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aAnimMatrixAttribute));
	
	aInAnimMatrix = gAttr.create( "InAnimMatrix", "inamat",  &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(gAttr.addDataAccept(MFnData::kMatrixArray));
	DOMMY_RETURN_IF_FAILURE(gAttr.setStorable( false ));
	DOMMY_RETURN_IF_FAILURE(gAttr.setWritable( true ));
	DOMMY_RETURN_IF_FAILURE(gAttr.setReadable( false ));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aInAnimMatrix));
	
    MFnEnumAttribute animMatEnumFn;
    aMatrixAssignType = animMatEnumFn.create( "MatrixAssignType", "matasty", 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(animMatEnumFn.addField( "Attach Motion Matrix", 0 ));
    DOMMY_RETURN_IF_FAILURE(animMatEnumFn.addField( "Freeze Mesh", 1 ));
    DOMMY_RETURN_IF_FAILURE(animMatEnumFn.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aMatrixAssignType));
	
#pragma mark global transform attribute
    
    aLocalTranslate = nAttr.create( "LocalTranslate", "ltrans", MFnNumericData::k3Double , 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0,0.0,0.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aLocalTranslate));
    
    aGlobalTranslate = nAttr.create( "GlobalTranslate", "gtrans", MFnNumericData::k3Double , 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0,0.0,0.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aGlobalTranslate));
    
    aLocalRotate = nAttr.create( "LocalRotate", "lrot", MFnNumericData::k3Double , 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0,0.0,0.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aLocalRotate));
    
    
    aGlobalRotate = nAttr.create( "GlobalRotate", "grot", MFnNumericData::k3Double , 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0,0.0,0.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aGlobalRotate));
    
    aGlobalScale = nAttr.create( "GlobalScale", "gscal", MFnNumericData::k3Double , 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1.0,1.0,1.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aGlobalScale));
    
    
    aUniformScale = nAttr.create( "UniformScale", "uscal", MFnNumericData::kDouble , 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(1.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aUniformScale));
    
    
#pragma mark attribute
    const MObject aLocalTranslateAttributeDefaultData = fnStringData.create("", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aLocalTranslateAttribute = tAttr.create("LocalTranslateAttribute", "ltattr", MFnData::kString, aLocalTranslateAttributeDefaultData, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aLocalTranslateAttribute));
    
    const MObject aGlobalTranslateAttributeDefaultData = fnStringData.create("", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aGlobalTranslateAttribute = tAttr.create("GlobalTranslateAttribute", "gtattr", MFnData::kString, aGlobalTranslateAttributeDefaultData, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aGlobalTranslateAttribute));
    
    const MObject aLocalRotateAttributeDefaultData = fnStringData.create("", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aLocalRotateAttribute = tAttr.create("LocalRotateAttribute", "lrattr", MFnData::kString, aLocalRotateAttributeDefaultData, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aLocalRotateAttribute));
    
    const MObject aGlobalRotateAttributeDefaultData = fnStringData.create("", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aGlobalRotateAttribute = tAttr.create("GlobalRotateAttribute", "grattr", MFnData::kString, aGlobalRotateAttributeDefaultData, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aGlobalRotateAttribute));
    
    const MObject aGlobalScaleAttributeDefaultData = fnStringData.create("", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aGlobalScaleAttribute = tAttr.create("GlobalScaleAttribute", "gsattr", MFnData::kString, aGlobalScaleAttributeDefaultData, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aGlobalScaleAttribute));
    
    const MObject aUniformScaleAttributeDefaultData = fnStringData.create("", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aUniformScaleAttribute = tAttr.create("UniformScaleAttribute", "usattr", MFnData::kString, aUniformScaleAttributeDefaultData, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aUniformScaleAttribute));
    
#pragma mark random attribute
    
    aLocalTranslateRandomMin = nAttr.create( "LocalTranslateRandomMin", "ltransrndmin", MFnNumericData::k3Double , 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0,0.0,0.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aLocalTranslateRandomMin));
    
    aLocalTranslateRandomMax = nAttr.create( "LocalTranslateRandomMax", "ltransrndmax", MFnNumericData::k3Double , 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0,0.0,0.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aLocalTranslateRandomMax));
    
    
    aGlobalTranslateRandomMin = nAttr.create( "GlobalTranslateRandomMin", "gtransrndmin", MFnNumericData::k3Double , 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0,0.0,0.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aGlobalTranslateRandomMin));
    
    aGlobalTranslateRandomMax = nAttr.create( "GlobalTranslateRandomMax", "gtransrndmax", MFnNumericData::k3Double , 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0,0.0,0.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aGlobalTranslateRandomMax));
    
    
    
    aLocalRotateRandomMin = nAttr.create( "LocalRotateRandomMin", "lrotrndmin", MFnNumericData::k3Double , 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0,0.0,0.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aLocalRotateRandomMin));
    
    aLocalRotateRandomMax = nAttr.create( "LocalRotateRandomMax", "lrotrndmax", MFnNumericData::k3Double , 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0,0.0,0.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aLocalRotateRandomMax));
    
    aGlobalRotateRandomMin = nAttr.create( "GlobalRotateRandomMin", "grotrndmin", MFnNumericData::k3Double , 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0,0.0,0.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aGlobalRotateRandomMin));
    
    aGlobalRotateRandomMax = nAttr.create( "GlobalRotateRandomMax", "grotrndmax", MFnNumericData::k3Double , 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0,0.0,0.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aGlobalRotateRandomMax));
    
    
    aGlobalScaleRandomMin = nAttr.create( "GlobalScaleRandomMin", "gscalrndmin", MFnNumericData::k3Double , 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0,0.0,0.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aGlobalScaleRandomMin));
    
    aGlobalScaleRandomMax = nAttr.create( "GlobalScaleRandomMax", "gscalrndmax", MFnNumericData::k3Double , 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0,0.0,0.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aGlobalScaleRandomMax));
    
    aUniformScaleRandomMinMax = nAttr.create( "UniformScaleRandomMinMax", "usckmima", MFnNumericData::k2Double , 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0,0.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aUniformScaleRandomMinMax));
    
    
#pragma mark transfer attribute
    
    
    const MObject aTransferAttributesDefaultData = fnStringData.create("", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aTransferAttributes = tAttr.create("TransferAttributes", "transattr", MFnData::kString, aTransferAttributesDefaultData, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aTransferAttributes));
	
#pragma mark uv attribute
	
	aIsOutUV = nAttr.create( "IsOutUV", "isouv", MFnNumericData::kBoolean , 0, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(false));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aIsOutUV));
	
	
	aUVType = enumFn.create( "UVType", "uvt", 0, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(enumFn.addField( "Original", 0 ));
	DOMMY_RETURN_IF_FAILURE(enumFn.addField( "Along U", 1 ));
	DOMMY_RETURN_IF_FAILURE(enumFn.addField( "Tile", 2 ));
	DOMMY_RETURN_IF_FAILURE(enumFn.setKeyable( true ));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aUVType));

#pragma mark expression attribute
	MObject expAttrName = fnStringData.create("", &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	aExpression = tAttr.create("Expression", "expr", MFnData::kString, expAttrName, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aExpression));
	
    


    
	
#pragma mark outDM attribute
    outDM = tAttr.create("outDM", "odm", DMeshData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(outDM));
	
	
#pragma mark affect attribute
	//outMesh
	DOMMY_RETURN_IF_FAILURE(attributeAffects( inDP, outDM ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( inDM, outDM ));
	DOMMY_RETURN_IF_FAILURE(attributeAffects( aInObjects, outDM ));
    
    
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aObjectType, outDM ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aObjectNumber, outDM ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aSeed, outDM ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aObjectIdAttribute, outDM ));
	
    
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aAimVector, outDM ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aAxisVector, outDM ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aNormalVector, outDM ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aNormalAttribute, outDM ));
                                   
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aTangentVector, outDM ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aTangentAttribute, outDM ));
	
	DOMMY_RETURN_IF_FAILURE(attributeAffects( aAnimationType, outDM ));
	DOMMY_RETURN_IF_FAILURE(attributeAffects( aAnimMatrixAttribute, outDM ));
	DOMMY_RETURN_IF_FAILURE(attributeAffects( aInAnimMatrix, outDM ));
	DOMMY_RETURN_IF_FAILURE(attributeAffects( aMatrixAssignType, outDM ));
	
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aLocalTranslate, outDM ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aGlobalTranslate, outDM ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aLocalRotate, outDM ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aGlobalRotate, outDM ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aGlobalScale, outDM ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aUniformScale, outDM ));
    
    //Attribute
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aLocalTranslateAttribute, outDM ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aGlobalTranslateAttribute, outDM ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aLocalRotateAttribute, outDM ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aGlobalRotateAttribute, outDM ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aGlobalScaleAttribute, outDM ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aUniformScaleAttribute, outDM ));
    
    //random
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aLocalTranslateRandomMin, outDM ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aLocalTranslateRandomMax, outDM ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aGlobalTranslateRandomMin, outDM ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aGlobalTranslateRandomMax, outDM ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aLocalRotateRandomMin, outDM ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aLocalRotateRandomMax, outDM ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aGlobalRotateRandomMin, outDM ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aGlobalRotateRandomMax, outDM ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aGlobalScaleRandomMin, outDM ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aGlobalScaleRandomMax, outDM ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aUniformScaleRandomMinMax, outDM ));
    
    //transfer Attributes
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aTransferAttributes, outDM ));
    
    
    
	DOMMY_RETURN_IF_FAILURE(attributeAffects( aIsOutUV, outDM ));
	DOMMY_RETURN_IF_FAILURE(attributeAffects( aUVType, outDM ));
	
	DOMMY_RETURN_IF_FAILURE(attributeAffects( aExpression, outDM ));
	
	
	
	return MS::kSuccess;
}
catch (...) {
    return dommy::maya::reportEntryPointException(
        "DPMeshDuplicator", "initialize", std::current_exception());
}
