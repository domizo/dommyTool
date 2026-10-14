//
//  DPRay.cpp
//  dommyTool
//
//  Created by yuzo on 2016/07/07.
//
//

#include "common/utility/DMayaEntryPointGuard.h"
#include "DPRay.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DGeometryLayout.h"
#include "common/utility/DMayaPluginDataOutput.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DGroupPublication.h"
#include "../Common/DPointForceOutput.h"
#include "common/utility/DAttributeArrayConversion.h"

#include <utility>


#include "DMayaHeaders.h"

#include "DPointDataHeaders.h"
#include "DCurveDataHeaders.h"
#include "DMeshDataHeaders.h"
#include "common/utility/DMeshSelection.h"
#include "common/utility/DMayaValueValidation.h"



const MTypeId DPRay::typeId( ID_DP_RAY );

MObject DPRay::inDD;
MObject DPRay::inObject;

MObject DPRay::aMethod;

MObject DPRay::aRayDirection;

MObject DPRay::aIsTransform;
MObject DPRay::aIsFarthest;
//    MObject DPRay::aPointIntersectNormal;
//    MObject DPRay::aPointIntersectDistance;

MObject DPRay::aIsReverseRay;

MObject DPRay::aRayTolerance;
MObject DPRay::aScale;
MObject DPRay::aLift;
//    MObject DPRay::aBias;

MObject DPRay::aIsMaxDistance;
MObject DPRay::aMaxDistance;

MObject DPRay::aIsCreateRayGroup;
MObject DPRay::aRayGroupName;

MObject DPRay::aVectorAttribute;
MObject DPRay::aFaceGroup;

//MObject DPRay::aHitGridSize;


MObject DPRay::outDD;


void *DPRay::creator() {
    return ::dommy::maya::createObject("DPRay", [&]() {
        return new DPRay();
    });
}

MStatus DPRay::evalRayParms(MDataBlock& data, DPRayParams& parms) {
    
    MStatus stat;
    
    MDataHandle aMethodHandle = data.inputValue(aMethod, &stat);
    if (!stat) return stat;
    parms.m_type = aMethodHandle.asShort();
    
    MDataHandle aIsReverseRayHandle = data.inputValue(aIsReverseRay, &stat);
    if (!stat) return stat;
    parms.mIsReverse = aIsReverseRayHandle.asBool();
    MDataHandle aIsTransformHandle = data.inputValue(aIsTransform, &stat);
    if (!stat) return stat;
    parms.mIsTransform = aIsTransformHandle.asBool();
    MDataHandle aIsFarthestHandle = data.inputValue(aIsFarthest, &stat);
    if (!stat) return stat;
    parms.mIsFarthest = aIsFarthestHandle.asBool();
    
    MDataHandle aRayToleranceHandle = data.inputValue(aRayTolerance, &stat);
    if (!stat) return stat;
    parms.mRayTolerance = aRayToleranceHandle.asDouble();
    MDataHandle aScaleHandle = data.inputValue(aScale, &stat);
    if (!stat) return stat;
    parms.mScale = aScaleHandle.asFloat();
    MDataHandle aLiftHandle = data.inputValue(aLift, &stat);
    if (!stat) return stat;
    parms.mLift = aLiftHandle.asFloat();
    
    MDataHandle aIsMaxDistanceHandle = data.inputValue(aIsMaxDistance, &stat);
    if (!stat) return stat;
    parms.mIsMaxDistance = aIsMaxDistanceHandle.asBool();
    MDataHandle aMaxDistanceHandle = data.inputValue(aMaxDistance, &stat);
    if (!stat) return stat;
    parms.mMaxDist = aMaxDistanceHandle.asFloat();
    
    
    MDataHandle aIsCreateRayGroupHandle = data.inputValue(aIsCreateRayGroup, &stat);
    if (!stat) return stat;
    parms.mIsCreateRayGrp =
        aIsCreateRayGroupHandle.asBool();
    
    
    //projection
    MDataHandle aRayDirectionHandle = data.inputValue(aRayDirection, &stat);
    if (!stat) return stat;
    const float3 &raidirVal =
        aRayDirectionHandle.asFloat3();
    MVector rayVec(raidirVal[0],raidirVal[1],raidirVal[2]);

    if (parms.m_type < 0 ||
        parms.m_type >= DP_RAY_COUNT ||
        !std::isfinite(parms.mRayTolerance) ||
        parms.mRayTolerance < 0.0 ||
        !std::isfinite(parms.mScale) ||
        !std::isfinite(parms.mLift) ||
        !std::isfinite(parms.mMaxDist) ||
        (parms.mIsMaxDistance && parms.mMaxDist < 0.0f) ||
        !dommy::maya::isFiniteVector(rayVec)) {
        return MS::kInvalidParameter;
    }

    if (parms.m_type == DP_RAY_PROJECTION) {
        MVector normalizedRay;
        if (!dommy::maya::normalizedVector(rayVec, normalizedRay)) {
            return MS::kInvalidParameter;
        }
        rayVec = parms.mIsReverse ? -normalizedRay : normalizedRay;
    }
    parms.mRayVec = rayVec;
    
    
//    parms.mHitGridSize = data.inputValue(aHitGridSize).asInt();
    
    
    
    return MS::kSuccess;
}

MStatus DPRay::compute(const MPlug& plug, MDataBlock& data) try
{
    if (plug != outDD) return MS::kUnknownParameter;
    MStatus stat;
    MPlug objectPlug(thisMObject(), inObject);
    const bool connected = objectPlug.isConnected(&stat);
    if (!stat) return stat;
    if (!connected) return MS::kFailure;

    DPRayParams params;
    DOMMY_RETURN_IF_FAILURE(evalRayParms(data, params));
    std::string rayGroupName;
    if (params.mIsCreateRayGrp) {
        const MDataHandle groupHandle = data.inputValue(aRayGroupName, &stat);
        if (!stat) return stat;
        rayGroupName = groupHandle.asString().asChar();
        if (!dommy::attribute::validAttributeName(rayGroupName))
            return MS::kInvalidParameter;
    }

    MDataHandle objectHandle = data.inputValue(inObject, &stat);
    if (!stat) return stat;
    params.m_object = objectHandle.data();
    params.m_objectType = objectHandle.type();
    if (params.m_object.isNull() ||
        (params.m_objectType != MFnData::kNurbsCurve &&
         params.m_objectType != MFnData::kNurbsSurface &&
         params.m_objectType != MFnData::kMesh))
        return MS::kInvalidParameter;
    if (params.m_type == DP_RAY_PROJECTION && params.m_objectType == MFnData::kNurbsCurve)
        return MS::kFailure;

    // Mesh projection takes a float tolerance. Curve/surface and minimum
    // queries keep their historical double input range.
    if (params.m_type == DP_RAY_PROJECTION && params.m_objectType == MFnData::kMesh) {
        float meshTolerance = 0.0f;
        if (!dommy::numeric::checkedFloatCast(params.mRayTolerance, meshTolerance))
            return MS::kInvalidParameter;
    }

    MDataHandle inputHandle = data.inputValue(inDD, &stat);
    if (!stat) return stat;
    dommy::point::PointForceInput input;
    DOMMY_RETURN_IF_FAILURE(input.bind(inputHandle));
    if (!input.points && !input.curves && !input.meshes) return MS::kInvalidParameter;
    std::string vecAttrName, faceGrpAttrStr;
    if (input.meshes && input.partitionCount != 0U) {
        MDataHandle vectorHandle = data.inputValue(aVectorAttribute, &stat);
        if (!stat) return stat;
        vecAttrName = vectorHandle.asString().asChar();
        MDataHandle faceHandle = data.inputValue(aFaceGroup, &stat);
        if (!stat) return stat;
        faceGrpAttrStr = faceHandle.asString().asChar();
    }

    if (input.points) {
        stat = dommy::maya::setPluginDataOutput<DPData>(
            data, outDD, DPData::id, [&](DPData& candidate) -> MStatus {
            const DPData* inputDPData = input.points;
            DPData* outputDPData = &candidate;
            outputDPData->setDPData(*inputDPData);

            size_t ptCount = outputDPData->numberOfPoints();

            //get point
            dommy::geometry::StagedPositionEdits<DPData>
                positionEdits;
            if (!positionEdits.add(*outputDPData)) {
                return MS::kFailure;
            }
            std::vector<DPointList *> pointsList;
            if (!positionEdits.mutablePositionPointers(pointsList)) {
                return MS::kFailure;
            }

            std::vector<DPIdList > hitIdsList;
            DPIdList values(ptCount, false);
            hitIdsList.push_back(values);

            dommy::ray::Generator raygen(
                params, pointsList, hitIdsList);

            //run process
            stat = raygen();
            if (!stat) return stat;
            if (!positionEdits.commit()) {
                return MS::kFailure;
            }

            if (params.mIsTransform) {
                outputDPData->updateBBox();
            }

            //add ray group
            if (params.mIsCreateRayGrp) {
                if (hitIdsList.size() != 1U ||
                    !outputDPData->replaceGrp(rayGroupName, hitIdsList[0])) {
                    return MS::kInvalidParameter;
                }
            }
            return MS::kSuccess;
        });
    } else if (input.curves) {
        stat = dommy::maya::setPluginDataOutput<DCurveData>(
            data, outDD, DCurveData::id, [&](DCurveData& candidate) -> MStatus {
            const DCurveData* inputDCData = input.curves;
            DCurveData* outDCrvs = &candidate;
            DOMMY_RETURN_IF_FAILURE(outDCrvs->copyFrom(*inputDCData));

            if (outDCrvs->numberOfCurves()) {

                dommy::geometry::StagedPositionEdits<
                    DCurveInternalData> positionEdits;
                if (!positionEdits.reserve(
                        outDCrvs->numberOfCurves())) {
                    return MS::kFailure;
                }
                std::vector<DPointList *> pointsList;
                std::vector<DPIdList > hitIdsList;

                //get position and set size
                for (auto cIt=outDCrvs->beginCurves(),ecIt=outDCrvs->endCurves(); cIt!=ecIt; ++cIt) {

                    if (!*cIt || !positionEdits.add(**cIt)) {
                        return MS::kFailure;
                    }

                    DPIdList values((*cIt)->numberOfPoints(), false);
                    hitIdsList.push_back(values);
                }//loop curve
                if (!positionEdits.mutablePositionPointers(
                        pointsList)) {
                    return MS::kFailure;
                }

//                        cout << "hitIdsList:" << hitIdsList.size() << endl;
//                        cout << "pointsList:" << pointsList.size() << endl;

                dommy::ray::Generator raygen(
                    params, pointsList, hitIdsList);

                //run process
                stat = raygen();
                if (!stat) return stat;
                if (!positionEdits.commit()) {
                    return MS::kFailure;
                }

                if (params.mIsTransform) {
                    outDCrvs->updateBBox();
                }

                //add ray group
                if (params.mIsCreateRayGrp) {
                    if (!dommy::group::replacePointGroupPartitions(
                            outDCrvs->beginCurves(), outDCrvs->endCurves(),
                            rayGroupName, hitIdsList)) {
                        return MS::kInvalidParameter;
                    }
                }//check add group

            }//curve count
            return MS::kSuccess;
        });
    } else {
        stat = dommy::maya::setPluginDataOutput<DMeshData>(
            data, outDD, DMeshData::id, [&](DMeshData& candidate) -> MStatus {
            const DMeshData* inputDMData = input.meshes;
            DMeshData* outDMeshes = &candidate;
            DOMMY_RETURN_IF_FAILURE(outDMeshes->copyFrom(*inputDMData));

            if (outDMeshes->numberOfMesh()) {

                dommy::geometry::StagedPositionEdits<
                    DMeshInternalData> positionEdits;
                if (!positionEdits.reserve(
                        outDMeshes->numberOfMesh())) {
                    return MS::kFailure;
                }
                std::vector<DPointList *> pointsList;
                std::vector<DPIdList > hitIdsList;
                std::vector<DPIdList > pointSelectionList;
                std::vector<DVec3DataList> vecList;

                //get position and set size
                auto cIt=outDMeshes->beginMeshes();
                auto eIt=outDMeshes->endMeshes();
                for (; cIt!=eIt; ++cIt) {

                    if (!*cIt || !positionEdits.add(**cIt)) {
                        return MS::kFailure;
                    }

                    DPIdList values((*cIt)->numberOfPoints(), false);
                    hitIdsList.push_back(values);

                    DPIdList pointSelection((*cIt)->numberOfPoints(), true);
                    if ((*cIt)->isExistCompGrp(D_COMP_FACE, faceGrpAttrStr)) {
                        DPGroupData::ConstPtr grpdata = (*cIt)->getCompGrp(D_COMP_FACE, faceGrpAttrStr);
								if (!grpdata) {
									EchoWarning(name().asChar(), "Failed to read the DMesh face group.");
									return MS::kFailure;
								}

								const DPIdList& faceSelection = grpdata->getGroupIds();
								const MIntArray faceCountsArray = (*cIt)->getFaceList();
								const MIntArray connectionsArray = (*cIt)->getConnectList();
								std::vector<size_t> faceCounts;
								std::vector<size_t> connections;
								faceCounts.reserve(faceCountsArray.length());
								connections.reserve(connectionsArray.length());
								for (unsigned int faceIndex = 0; faceIndex < faceCountsArray.length();
								     ++faceIndex) {
									if (faceCountsArray[faceIndex] < 0) {
										EchoWarning(name().asChar(), "DMesh contains a negative face vertex count.");
										return MS::kFailure;
									}
									faceCounts.push_back(static_cast<size_t>(faceCountsArray[faceIndex]));
								}
								for (unsigned int connectionIndex = 0;
								     connectionIndex < connectionsArray.length(); ++connectionIndex) {
									if (connectionsArray[connectionIndex] < 0) {
										EchoWarning(name().asChar(), "DMesh contains a negative point index.");
										return MS::kFailure;
									}
									connections.push_back(
										static_cast<size_t>(connectionsArray[connectionIndex]));
								}

								if (!dommy::mesh::buildPointSelectionFromFaces(
								        (*cIt)->numberOfPoints(), faceCounts, connections, faceSelection,
								        pointSelection)) {
									EchoWarning(name().asChar(),
									            "DMesh topology and face group have incompatible sizes.");
									return MS::kFailure;
								}
                    }

                    pointSelectionList.push_back(pointSelection);

                    if((*cIt)->isExistAttr(vecAttrName)) {
                        DVec3DataList directions;
                        if (!dommy::attribute::convertVector3Attribute(
                                (*cIt)->getAttr(vecAttrName),
                                (*cIt)->numberOfPoints(), directions)) {
                            return MS::kInvalidParameter;
                        }
                        vecList.push_back(std::move(directions));
                    }

                }//loop mesh
                if (!positionEdits.mutablePositionPointers(
                        pointsList)) {
                    return MS::kFailure;
                }

                if (vecList.size() == outDMeshes->numberOfMesh()) {

                    //use intersection vector
                    dommy::ray::Generator raygen(
                        params,
                        pointsList,
                        hitIdsList,
                        pointSelectionList,
                        vecList);

                    //run process
                    stat = raygen();
                    if (!stat) return stat;

                } else {
                    dommy::ray::Generator raygen(
                        params,
                        pointsList,
                        hitIdsList,
                        pointSelectionList);

                    //run process
                    stat = raygen();
                    if (!stat) return stat;
                }
                if (!positionEdits.commit()) {
                    return MS::kFailure;
                }

                if (params.mIsTransform) {
                    outDMeshes->updateBBox();
                }

                //add ray group
                if (params.mIsCreateRayGrp) {
                    if (!dommy::group::replacePointGroupPartitions(
                            outDMeshes->beginMeshes(), outDMeshes->endMeshes(),
                            rayGroupName, hitIdsList)) {
                        return MS::kInvalidParameter;
                    }
                }//check add group

            }//curve count
            return MS::kSuccess;
        });
    }
    return stat ? data.setClean(plug) : stat;
}
DOMMY_COMPUTE_CATCH("DPRay")

MStatus DPRay::initialize() try {
    MStatus stat;
    MFnNumericAttribute nAttr;
    MFnGenericAttribute gAttr;
    MFnTypedAttribute tAttr;

    // ----------------------- INPUTS --------------------------

    inDD = gAttr.create( "inDD", "idd",  &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(gAttr.addAccept(DPData::id));
    DOMMY_RETURN_IF_FAILURE(gAttr.addAccept(DCurveData::id));
    DOMMY_RETURN_IF_FAILURE(gAttr.addAccept(DMeshData::id));
    DOMMY_RETURN_IF_FAILURE(gAttr.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(gAttr.setStorable( false ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inDD));

    inObject = gAttr.create( "inObject", "ino",  &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(gAttr.addDataAccept(MFnData::kMesh));
    DOMMY_RETURN_IF_FAILURE(gAttr.addDataAccept(MFnData::kNurbsCurve));
    DOMMY_RETURN_IF_FAILURE(gAttr.addDataAccept(MFnData::kNurbsSurface));
    DOMMY_RETURN_IF_FAILURE(gAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(gAttr.setWritable( true ));
    DOMMY_RETURN_IF_FAILURE(gAttr.setReadable( false ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inObject));

    MFnEnumAttribute enumFn;
    aMethod = enumFn.create( "Method", "met", DP_RAY_PROJECTION, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(enumFn.addField( "Ray Projection", DP_RAY_PROJECTION ));
    DOMMY_RETURN_IF_FAILURE(enumFn.addField( "Minimum Distance", DP_RAY_MINIMUM ));
    DOMMY_RETURN_IF_FAILURE(enumFn.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(enumFn.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aMethod));

    aRayDirection = nAttr.create( "aRayDirection", "rdir", MFnNumericData::k3Float, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aRayDirection));

    aIsTransform = nAttr.create( "IsTransform", "istran", MFnNumericData::kBoolean, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aIsTransform));

    aIsFarthest = nAttr.create( "IsFarthest", "isfar", MFnNumericData::kBoolean, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aIsFarthest));

    aIsReverseRay = nAttr.create( "IsReverseRay", "isrev", MFnNumericData::kBoolean, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aIsReverseRay));

    aRayTolerance = nAttr.create( "RayTolerance", "rtol", MFnNumericData::kDouble, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.01));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aRayTolerance));

    aScale = nAttr.create( "Scale", "sc", MFnNumericData::kFloat, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(1.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aScale));

    aLift = nAttr.create( "Lift", "lif", MFnNumericData::kFloat, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(1.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aLift));

    aIsMaxDistance = nAttr.create( "IsMaxDistance", "isma", MFnNumericData::kBoolean, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aIsMaxDistance));

    aMaxDistance = nAttr.create( "MaxDistance", "mdist", MFnNumericData::kFloat, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(1.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aMaxDistance));

    aIsCreateRayGroup = nAttr.create( "IsCreateRayGroup", "isgrp", MFnNumericData::kBoolean, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aIsCreateRayGroup));

    MFnStringData fnStringData;
    MObject attrOffName = fnStringData.create("rayHitGroup", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aRayGroupName = tAttr.create("RayGroupName", "rgrpattr", MFnData::kString, attrOffName, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aRayGroupName));

    const MObject aVectorAttributeDefault = fnStringData.create("", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aVectorAttribute = tAttr.create("VectorAttribute", "vecattr", MFnData::kString, aVectorAttributeDefault, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aVectorAttribute));

    const MObject aFaceGroupDefault = fnStringData.create("", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aFaceGroup = tAttr.create("FaceGroup", "fcattr", MFnData::kString, aFaceGroupDefault, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aFaceGroup));

    //create custom data
    outDD = gAttr.create( "outDD", "odd", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(gAttr.addAccept(DPData::id));
    DOMMY_RETURN_IF_FAILURE(gAttr.addAccept(DCurveData::id));
    DOMMY_RETURN_IF_FAILURE(gAttr.addAccept(DMeshData::id));
    DOMMY_RETURN_IF_FAILURE(gAttr.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(gAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(outDD));

    DOMMY_RETURN_IF_FAILURE(attributeAffects( inDD, outDD ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( inObject, outDD ));

    DOMMY_RETURN_IF_FAILURE(attributeAffects( aMethod, outDD ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aRayDirection, outDD ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aIsTransform, outDD ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aIsFarthest, outDD ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aIsReverseRay, outDD ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aRayTolerance, outDD ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aScale, outDD ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aLift, outDD ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aIsMaxDistance, outDD ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aMaxDistance, outDD ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aIsCreateRayGroup, outDD ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aRayGroupName, outDD ));

    DOMMY_RETURN_IF_FAILURE(attributeAffects( aVectorAttribute, outDD ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aFaceGroup, outDD ));

    return MS::kSuccess;
}
catch (...) {
    return dommy::maya::reportEntryPointException(
        "DPRay", "initialize", std::current_exception());
}
