//
//  DCollisionDeformer.cpp
//  dommyTool
//
//  Created by yuzo on 2016/05/09.
//
//

#include "DCollisionDeformer.h"
#include "common/utility/DMayaEntryPointGuard.h"
#include "common/utility/DMayaDefaultRamp.h"
#include "common/utility/DStatusOperation.h"

#include "DMayaDebug.h"
#include "general_ids.h"
#include "common/utility/DGenerationLimits.h"
#include "common/utility/DDeformerGuard.h"
#include "common/utility/DDeformerMath.h"
#include "common/utility/DDirectionMath.h"
#include "common/utility/DMayaDeformerGeometry.h"
#include "common/utility/DMayaMeshClosestPoint.h"
#include "common/utility/DMayaRampSampling.h"
#include "common/utility/DMayaValueValidation.h"

#include <maya/MArrayDataHandle.h>
#include <maya/MBoundingBox.h>
#include <maya/MDataBlock.h>
#include <maya/MDataHandle.h>
#include <maya/MFloatArray.h>
#include <maya/MFloatPointArray.h>
#include <maya/MFnData.h>
#include <maya/MFnEnumAttribute.h>
#include <maya/MFnGenericAttribute.h>
#include <maya/MFnMatrixAttribute.h>
#include <maya/MFnMesh.h>
#include <maya/MFnNumericAttribute.h>
#include <maya/MGlobal.h>
#include <maya/MIntArray.h>
#include <maya/MItGeometry.h>
#include <maya/MItMeshVertex.h>
#include <maya/MMatrix.h>
#include <maya/MPlug.h>
#include <maya/MPointArray.h>
#include <maya/MRampAttribute.h>

#include <cmath>
#include <vector>

namespace {

const double kMinimumTransformDeterminant = 1.0e-12;

class CollisionIntersectionCacheGuard {
  public:
    explicit CollisionIntersectionCacheGuard(MFnMesh& meshFn) : meshFn_(meshFn) {}
    ~CollisionIntersectionCacheGuard() noexcept {
        meshFn_.freeCachedIntersectionAccelerator();
    }

  private:
    CollisionIntersectionCacheGuard(const CollisionIntersectionCacheGuard&);
    CollisionIntersectionCacheGuard& operator=(const CollisionIntersectionCacheGuard&);
    MFnMesh& meshFn_;
};

} // namespace

MTypeId DCollisionDeformer::typeId(ID_DEFORMER_COLLISION);
MString DCollisionDeformer::typeName("DCollisionDeformer");

MObject DCollisionDeformer::inObject;
MObject DCollisionDeformer::inObjectMatrix;

MObject DCollisionDeformer::offset;
MObject DCollisionDeformer::calcType;

MObject DCollisionDeformer::isFalloffEffect;
MObject DCollisionDeformer::distance;
MObject DCollisionDeformer::amplitude;
MObject DCollisionDeformer::falloffRamp;
MObject DCollisionDeformer::surfaceAdjustType;
MObject DCollisionDeformer::smIterate;

MObject DCollisionDeformer::detectType;
MObject DCollisionDeformer::gridSize;

void* DCollisionDeformer::creator() {
    return ::dommy::maya::createObject("DCollisionDeformer", [&]() {
        return new DCollisionDeformer();
    });
}

void DCollisionDeformer::postConstructor() {
    dommy::maya::initializeNodeRamps("DCollisionDeformer", [&]() -> MStatus {
        DOMMY_RETURN_IF_FAILURE(dommy::maya::initializeDefaultValueRamp(
            thisMObject(), falloffRamp, {1.0f, 0.0f}, {1.0f, 0.0f},
            {MRampAttribute::kSpline, MRampAttribute::kSpline}));
        return MS::kSuccess;
    });
}

MStatus DCollisionDeformer::deform(MDataBlock& data, MItGeometry& iter, const MMatrix& localToWorld,
                                   unsigned int geomIndex) try {

    MStatus stat;

    MDataHandle envHandle = data.inputValue(envelope, &stat);
    CHECK_MSTATUS_AND_RETURN_IT(stat);
    const float env = envHandle.asFloat();
    if (!std::isfinite(env)) {
        return MS::kFailure;
    }
    if (env == 0.0) // Deformer has no effect
        return MS::kSuccess;

#pragma mark Check base object デフォームオブジェクトの確認
    MArrayDataHandle inputDataH = data.inputArrayValue(input, &stat);
    CHECK_MSTATUS_AND_RETURN_IT(stat);
    DOMMY_RETURN_IF_FAILURE(inputDataH.jumpToElement(geomIndex));
    MDataHandle iGeoHndElement = inputDataH.inputValue(&stat);
    CHECK_MSTATUS_AND_RETURN_IT(stat);
    MDataHandle iGeoHnd = iGeoHndElement.child(inputGeom);

    if (iGeoHnd.type() == MFnData::kNurbsSurface || iGeoHnd.type() == MFnData::kNurbsCurve) {
        MGlobal::displayWarning("Not support input object type.");
        return MS::kUnknownParameter;
    } else if (iGeoHnd.type() == MFnData::kComponentList) {
        MGlobal::displayWarning("Not support component deform.");
        return MS::kUnknownParameter;
    }

    if (iGeoHnd.type() != MFnData::kMesh) {
        MGlobal::displayWarning("DCollisionDeformer supports mesh input only.");
        return MS::kUnknownParameter;
    }
    MFnMesh thisMeshFn(iGeoHnd.data(), &stat);
    CHECK_MSTATUS_AND_RETURN_IT(stat);

#pragma mark Get input object 入力オブジェクトの取得
    MDataHandle dataHnd = data.inputValue(inObject, &stat);
    CHECK_MSTATUS_AND_RETURN_IT(stat);
    if (dataHnd.type() != MFnData::kMesh) {
        EchoWarning(name().asChar(), "Input is not mesh.");
        return MS::kFailure;
    }

    MFnMesh colMeshFn(dataHnd.data(), &stat);
    CHECK_MSTATUS_AND_RETURN_IT(stat);
    CollisionIntersectionCacheGuard cacheGuard(colMeshFn);
    dommy::maya::MeshClosestPointQuery closestPointQuery;
    DOMMY_RETURN_IF_FAILURE(closestPointQuery.create(dataHnd.data()));

    MDataHandle colliderToWorldHandle = data.inputValue(inObjectMatrix, &stat);
    CHECK_MSTATUS_AND_RETURN_IT(stat);
    const MMatrix colliderToWorld = colliderToWorldHandle.asMatrix();
    const double colliderDeterminant = colliderToWorld.det4x4();
    if (!dommy::maya::isFiniteMatrix(colliderToWorld) || !std::isfinite(colliderDeterminant) ||
        std::abs(colliderDeterminant) <= kMinimumTransformDeterminant) {
        MGlobal::displayError("DCollisionDeformer inObjectMatrix must be finite and invertible.");
        return MS::kFailure;
    }
    const MMatrix worldToCollider = colliderToWorld.inverse();
    const double localDeterminant = localToWorld.det4x4();
    if (!dommy::maya::isFiniteMatrix(localToWorld) || !std::isfinite(localDeterminant) ||
        std::abs(localDeterminant) <= kMinimumTransformDeterminant) {
        MGlobal::displayError(
            "DCollisionDeformer geometry transform must be finite and invertible.");
        return MS::kFailure;
    }
    const MMatrix worldToLocal = localToWorld.inverse();
    if (!dommy::maya::isFiniteMatrix(worldToCollider) ||
        !dommy::maya::isFiniteMatrix(worldToLocal))
        return MS::kInvalidParameter;
    MBoundingBox colBBox = colMeshFn.boundingBox(&stat);
    CHECK_MSTATUS_AND_RETURN_IT(stat);
    colBBox.transformUsing(colliderToWorld);
    if (!dommy::maya::isFiniteBoundingBox(colBBox))
        return MS::kInvalidParameter;

#pragma mark Get attribute アトリビュートの取得

    MDataHandle offsetValHandle = data.inputValue(offset, &stat);
    CHECK_MSTATUS_AND_RETURN_IT(stat);
    const double offsetVal = offsetValHandle.asDouble();
    MDataHandle isFalloffEffectValHandle = data.inputValue(isFalloffEffect, &stat);
    CHECK_MSTATUS_AND_RETURN_IT(stat);
    const bool isFalloffEffectVal = isFalloffEffectValHandle.asBool();
    MDataHandle distanceValHandle = data.inputValue(distance, &stat);
    CHECK_MSTATUS_AND_RETURN_IT(stat);
    const double distanceVal = distanceValHandle.asDouble();
    MDataHandle amplitudeValHandle = data.inputValue(amplitude, &stat);
    CHECK_MSTATUS_AND_RETURN_IT(stat);
    const double amplitudeVal = amplitudeValHandle.asDouble();
    MDataHandle calcTypeValHandle = data.inputValue(calcType, &stat);
    CHECK_MSTATUS_AND_RETURN_IT(stat);
    const short calcTypeVal = calcTypeValHandle.asShort();
    MDataHandle surfaceAdjustTypeValHandle = data.inputValue(surfaceAdjustType, &stat);
    CHECK_MSTATUS_AND_RETURN_IT(stat);
    const short surfaceAdjustTypeVal = surfaceAdjustTypeValHandle.asShort();
    MDataHandle weightIterateValHandle = data.inputValue(smIterate, &stat);
    CHECK_MSTATUS_AND_RETURN_IT(stat);
    const long weightIterateVal = weightIterateValHandle.asInt();
    MDataHandle detectTypeValHandle = data.inputValue(detectType, &stat);
    CHECK_MSTATUS_AND_RETURN_IT(stat);
    const short detectTypeVal = detectTypeValHandle.asShort();
    if (!std::isfinite(offsetVal) || !std::isfinite(distanceVal) || !std::isfinite(amplitudeVal) ||
        distanceVal < 0.0 || weightIterateVal < 0 || calcTypeVal < 0 || calcTypeVal > 1 ||
        surfaceAdjustTypeVal < 0 || surfaceAdjustTypeVal > 2 || detectTypeVal < 0 ||
        detectTypeVal > 1) {
        return MS::kFailure;
    }

    MPlug falloffRampPlug(thisMObject(), falloffRamp);
    MRampAttribute falloffRampPlugAttr(falloffRampPlug, &stat);
    CHECK_MSTATUS_AND_RETURN_IT(stat);

#pragma mark Get edit object 編集オブジェクトのデータ取得
    const int vertexCount = thisMeshFn.numVertices(&stat);
    CHECK_MSTATUS_AND_RETURN_IT(stat);
    if (vertexCount < 0) {
        return MS::kFailure;
    }
    std::size_t smoothingWork = 0U;
    if (weightIterateVal > 0 &&
        !dommy::generation::checkedProduct(static_cast<std::size_t>(vertexCount),
                                           static_cast<std::size_t>(weightIterateVal),
                                           dommy::generation::kMaxWorkItemCount, smoothingWork)) {
        return MS::kFailure;
    }
    if (static_cast<std::size_t>(vertexCount) > dommy::generation::kMaxGeneratedPointCount)
        return MS::kInvalidParameter;
    MMeshIsectAccelParams mmAccelParams; // = colMeshFn.autoUniformGridParams();
    if (detectTypeVal == 0) {            // Auto
        mmAccelParams = colMeshFn.autoUniformGridParams();
    } else {
        MDataHandle gridSizeHandle = data.inputValue(gridSize, &stat);
        CHECK_MSTATUS_AND_RETURN_IT(stat);
        const int* gridSizeValue = gridSizeHandle.asInt3();
        if (!gridSizeValue || gridSizeValue[0] <= 0 || gridSizeValue[1] <= 0 ||
            gridSizeValue[2] <= 0) {
            return MS::kFailure;
        }
        std::size_t gridXY = 0U;
        std::size_t gridCellCount = 0U;
        if (!dommy::generation::checkedProduct(static_cast<std::size_t>(gridSizeValue[0]),
                                               static_cast<std::size_t>(gridSizeValue[1]),
                                               dommy::generation::kMaxGeneratedPointCount,
                                               gridXY) ||
            !dommy::generation::checkedProduct(gridXY, static_cast<std::size_t>(gridSizeValue[2]),
                                               dommy::generation::kMaxGeneratedPointCount,
                                               gridCellCount)) {
            return MS::kFailure;
        }
        mmAccelParams =
            colMeshFn.uniformGridParams(gridSizeValue[0], gridSizeValue[1], gridSizeValue[2]);
    }

    MPointArray meshPoints;
    DOMMY_RETURN_IF_FAILURE(thisMeshFn.getPoints(meshPoints, MSpace::kObject));
    if (meshPoints.length() != static_cast<unsigned int>(vertexCount))
        return MS::kFailure;
    std::vector<MPoint> points(meshPoints.length());
    std::vector<MVector> normals(points.size());
    std::vector<double> weights(points.size(), 0.0);
    std::vector<bool> active(points.size(), false), contained(points.size(), false);
    for (unsigned int i = 0U; i < meshPoints.length(); ++i) {
        if (!dommy::maya::isFinitePoint(meshPoints[i]))
            return MS::kInvalidParameter;
        points[i] = meshPoints[i];
    }
    DOMMY_RETURN_IF_FAILURE(dommy::maya::visitDeformerPoints(
        iter, [&](unsigned int index, const MPoint& source) -> MStatus {
            if (index >= points.size())
                return MS::kInvalidParameter;
            points[index] = source;
            weights[index] = weightValue(data, geomIndex, index);
            if (!std::isfinite(weights[index]))
                return MS::kInvalidParameter;
            if (weights[index] == 0.0)
                return MS::kSuccess;
            active[index] = true;
            MVector normal = iter.normal(MSpace::kObject, &stat);
            if (!stat)
                return stat;
            normal = normal.transformAsNormal(localToWorld);
            if (!dommy::deformer::normalizeVector(normal))
                return MS::kInvalidParameter;
            normals[index] = normal;
            return MS::kSuccess;
        }));

    // Cache adjacency once, with an edge-visit budget as well as a vertex budget.
    std::vector<std::vector<unsigned int>> neighbors;
    if (weightIterateVal > 0) {
        neighbors.resize(points.size());
        MObject meshObj = iGeoHnd.data();
        MItMeshVertex vertexIt(meshObj, &stat);
        CHECK_MSTATUS_AND_RETURN_IT(stat);
        std::size_t adjacencyWork = points.size();
        for (;;) {
            const bool done = vertexIt.isDone(&stat);
            CHECK_MSTATUS_AND_RETURN_IT(stat);
            if (done)
                break;
            const int index = vertexIt.index(&stat);
            CHECK_MSTATUS_AND_RETURN_IT(stat);
            if (index < 0 || static_cast<std::size_t>(index) >= points.size())
                return MS::kInvalidParameter;
            if (active[index]) {
                MIntArray adjacent;
                DOMMY_RETURN_IF_FAILURE(vertexIt.getConnectedVertices(adjacent));
                if (!dommy::generation::checkedSum(adjacencyWork, adjacent.length(),
                                                   dommy::generation::kMaxWorkItemCount,
                                                   adjacencyWork) ||
                    !dommy::generation::checkedProduct(adjacencyWork, weightIterateVal,
                                                       dommy::generation::kMaxWorkItemCount,
                                                       smoothingWork))
                    return MS::kInvalidParameter;
                for (unsigned int j = 0U; j < adjacent.length(); ++j) {
                    if (adjacent[j] < 0 || static_cast<std::size_t>(adjacent[j]) >= points.size())
                        return MS::kInvalidParameter;
                    neighbors[index].push_back(static_cast<unsigned int>(adjacent[j]));
                }
            }
            DOMMY_RETURN_IF_FAILURE(vertexIt.next());
        }
    }

    double maxDisplacement = 0.0;
    for (std::size_t index = 0U; index < points.size(); ++index) {
        if (!active[index] || normals[index].length() <= 1.0e-12)
            continue;
        const MPoint worldPoint = points[index] * localToWorld;
        const MPoint colliderPoint = worldPoint * worldToCollider;
        if (!dommy::maya::isFinitePoint(worldPoint) || !dommy::maya::isFinitePoint(colliderPoint))
            return MS::kInvalidParameter;
        if ((colBBox.min().x - distanceVal) > worldPoint.x ||
            (colBBox.max().x + distanceVal) < worldPoint.x ||
            (colBBox.min().y - distanceVal) > worldPoint.y ||
            (colBBox.max().y + distanceVal) < worldPoint.y ||
            (colBBox.min().z - distanceVal) > worldPoint.z ||
            (colBBox.max().z + distanceVal) < worldPoint.z)
            continue;
        MVector rayDirection = -normals[index] * worldToCollider;
        if (!dommy::deformer::normalizeVector(rayDirection) || rayDirection.length() <= 1.0e-12 ||
            !dommy::maya::isFloatRepresentablePoint(colliderPoint))
            return MS::kInvalidParameter;
        MFloatPointArray hits;
        MFloatArray parameters;
        const bool hit = colMeshFn.allIntersections(
            MFloatPoint(colliderPoint), MFloatVector(rayDirection), nullptr, nullptr, true,
            MSpace::kObject, 9999, false, &mmAccelParams, false, hits, &parameters, nullptr,
            nullptr, nullptr, nullptr, 1.0e-6f, &stat);
        CHECK_MSTATUS_AND_RETURN_IT(stat);
        if (!hit || hits.length() % 2U == 0U)
            continue;
        MPoint hitPointObject;
        if (calcTypeVal == 0)
            hitPointObject = hits[0];
        else
            DOMMY_RETURN_IF_FAILURE(
                closestPointQuery.getClosestPoint(colliderPoint, hitPointObject));
        const MPoint hitPoint = hitPointObject * colliderToWorld;
        MPoint target;
        if (!dommy::deformer::blendPoint(worldPoint, hitPoint, 1.0 - offsetVal, target))
            return MS::kInvalidParameter;
        dommy::direction::Delta displacement;
        if (!dommy::direction::difference(worldPoint, target, displacement))
            return MS::kInvalidParameter;
        maxDisplacement = std::max(maxDisplacement, displacement.length);
        points[index] = target * worldToLocal;
        if (!dommy::maya::isFinitePoint(points[index]))
            return MS::kInvalidParameter;
        contained[index] = true;
    }

    if (isFalloffEffectVal && maxDisplacement > 0.0 && amplitudeVal > 0.0) {
        for (std::size_t index = 0U; index < points.size(); ++index) {
            if (!active[index] || contained[index] || normals[index].length() <= 1.0e-12)
                continue;
            const MPoint worldPoint = points[index] * localToWorld;
            const MPoint colliderPoint = worldPoint * worldToCollider;
            if (!dommy::maya::isFinitePoint(colliderPoint))
                return MS::kInvalidParameter;
            MPoint hitPointObject;
            DOMMY_RETURN_IF_FAILURE(
                closestPointQuery.getClosestPoint(colliderPoint, hitPointObject));
            const MPoint hitPoint = hitPointObject * colliderToWorld;
            dommy::direction::Delta delta;
            if (!dommy::direction::difference(worldPoint, hitPoint, delta))
                return MS::kInvalidParameter;
            if (delta.length < distanceVal) {
                float ramp = 0.0f;
                if (!dommy::maya::sampleFiniteRamp(falloffRampPlugAttr, delta.length / distanceVal,
                                                   ramp))
                    return MS::kInvalidParameter;
                MPoint target;
                if (!dommy::deformer::offsetPoint(worldPoint, normals[index], maxDisplacement,
                                                  amplitudeVal, 1.0, 1.0, ramp, target))
                    return MS::kInvalidParameter;
                points[index] = target * worldToLocal;
                if (!dommy::maya::isFinitePoint(points[index]))
                    return MS::kInvalidParameter;
            }
        }
    }
    if (weightIterateVal > 0 && !dommy::deformer::smoothCollisionPoints(
                                    points, neighbors, active, contained, surfaceAdjustTypeVal,
                                    static_cast<unsigned int>(weightIterateVal)))
        return MS::kInvalidParameter;

    return dommy::maya::editDeformerPoints(
        iter, [&](unsigned int index, const MPoint& source, MPoint& target) -> MStatus {
            if (index >= points.size())
                return MS::kInvalidParameter;
            return dommy::deformer::blendPoint(source, points[index], weights[index] * double(env),
                                               target)
                       ? MS::kSuccess
                       : MS::kInvalidParameter;
        });
}
DOMMY_DEFORMER_CATCH("DCollisionDeformer")

MStatus DCollisionDeformer::initialize() try {
    MStatus stat;
    MFnNumericAttribute nAttr;
    MFnGenericAttribute gAttr;
    MFnMatrixAttribute mAttr;
    //MFnTypedAttribute tAttr;
    MFnEnumAttribute eAttr;
	
    inObject = gAttr.create( "inObject", "iobj",  &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(gAttr.addDataAccept(MFnData::kMesh));
//    DOMMY_RETURN_IF_FAILURE(gAttr.addDataAccept(MFnData::kNurbsCurve));
//    DOMMY_RETURN_IF_FAILURE(gAttr.addDataAccept(MFnData::kNurbsSurface));
    DOMMY_RETURN_IF_FAILURE(gAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(gAttr.setWritable( true ));
    DOMMY_RETURN_IF_FAILURE(gAttr.setReadable( false ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inObject));

	inObjectMatrix = mAttr.create(
		"inObjectMatrix", "iomat", MFnMatrixAttribute::kDouble, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	CHECK_MSTATUS_AND_RETURN_IT(stat);
	DOMMY_RETURN_IF_FAILURE(mAttr.setDefault(MMatrix::identity));
	DOMMY_RETURN_IF_FAILURE(mAttr.setStorable(true));
	DOMMY_RETURN_IF_FAILURE(mAttr.setWritable(true));
	DOMMY_RETURN_IF_FAILURE(mAttr.setReadable(false));
	DOMMY_RETURN_IF_FAILURE(addAttribute(inObjectMatrix));
    
	offset  = nAttr.create( "offset", "off", MFnNumericData::kDouble , 0, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 0.0 ));
	DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
	DOMMY_RETURN_IF_FAILURE(addAttribute(offset));
	
	
	calcType = eAttr.create("calcType", "clt", 1, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(eAttr.addField("Normal", 0));
	DOMMY_RETURN_IF_FAILURE(eAttr.addField("Nearest", 1));
	DOMMY_RETURN_IF_FAILURE(eAttr.setConnectable(false));
	DOMMY_RETURN_IF_FAILURE(addAttribute(calcType));
	
	isFalloffEffect  = nAttr.create( "isFalloffEffect", "ife", MFnNumericData::kBoolean , 0, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( false ));
	DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
	DOMMY_RETURN_IF_FAILURE(addAttribute(isFalloffEffect));
	
	distance  = nAttr.create( "distance", "dist", MFnNumericData::kDouble , 0, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 3.0 ));
	DOMMY_RETURN_IF_FAILURE(nAttr.setMin( 0.0 ));
	DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
	DOMMY_RETURN_IF_FAILURE(addAttribute(distance));
	
	amplitude  = nAttr.create( "amplitude", "amp", MFnNumericData::kDouble , 0, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 1.0 ));
	DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
	DOMMY_RETURN_IF_FAILURE(addAttribute(amplitude));
	
	falloffRamp = MRampAttribute::createCurveRamp("falloffRamp", "framp", &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(addAttribute(falloffRamp));
	
	surfaceAdjustType = eAttr.create("surfaceAdjustType", "sdt", 0, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(eAttr.addField("Both", 0));
	DOMMY_RETURN_IF_FAILURE(eAttr.addField("Collide Only", 1));
	DOMMY_RETURN_IF_FAILURE(eAttr.addField("Other", 2));
	DOMMY_RETURN_IF_FAILURE(eAttr.setConnectable(false));
	DOMMY_RETURN_IF_FAILURE(addAttribute(surfaceAdjustType));
	
    smIterate  = nAttr.create( "smIterate", "sitr", MFnNumericData::kInt , 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 0 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin( 0 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(smIterate));
	
	
	
	detectType = eAttr.create("detectType", "det", 0, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(eAttr.addField("Auto", 0));
	DOMMY_RETURN_IF_FAILURE(eAttr.addField("Manual", 1));
	DOMMY_RETURN_IF_FAILURE(eAttr.setConnectable(false));
	DOMMY_RETURN_IF_FAILURE(addAttribute(detectType));
	
	
	
	
	gridSize = nAttr.create( "gridSize", "gs", MFnNumericData::k3Int, 0, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(100 ,100,100));
	DOMMY_RETURN_IF_FAILURE(addAttribute(gridSize));
	
    //set affectAttribute
    DOMMY_RETURN_IF_FAILURE(attributeAffects( inObject, outputGeom ));
	DOMMY_RETURN_IF_FAILURE(attributeAffects( inObjectMatrix, outputGeom ));
	
	DOMMY_RETURN_IF_FAILURE(attributeAffects( offset, outputGeom ));
	DOMMY_RETURN_IF_FAILURE(attributeAffects( calcType, outputGeom ));
	DOMMY_RETURN_IF_FAILURE(attributeAffects( isFalloffEffect, outputGeom ));
	DOMMY_RETURN_IF_FAILURE(attributeAffects( distance, outputGeom ));
	DOMMY_RETURN_IF_FAILURE(attributeAffects( amplitude, outputGeom ));
	DOMMY_RETURN_IF_FAILURE(attributeAffects( falloffRamp, outputGeom ));
	DOMMY_RETURN_IF_FAILURE(attributeAffects( surfaceAdjustType, outputGeom ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( smIterate, outputGeom ));
	
	DOMMY_RETURN_IF_FAILURE(attributeAffects( detectType, outputGeom ));
	DOMMY_RETURN_IF_FAILURE(attributeAffects( gridSize, outputGeom ));
	
    return MS::kSuccess;
}
catch (...) {
    return dommy::maya::reportEntryPointException(
        "DCollisionDeformer", "initialize", std::current_exception());
}
