//
//  DPDirectionGen.cpp
//  dommyTool
//
//  Created by yuzo on 6/26/16.
//
//

#include "common/utility/DMayaEntryPointGuard.h"
#include "DPDirection.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DMayaPluginDataOutput.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DDirectionMath.h"
#include "common/utility/DAttributeArrayConversion.h"
#include "common/utility/DLegacyDrawGuard.h"


#include "DMayaHeaders.h"
#include "DMayaDebug.h"

#include "DPData.h"
#include "DPointCommon.h"
#include "DPAttributeCommon.h"
#include "DPoint/Common/DPAttributeAccess.h"
#include "DCurveData.h"
#include "DCurveUtil.h"
#include "DMeshData.h"
#include "DMeshUtil.h"
#include "dmesh_ids.h"
#include "dpoint_ids.h"


#include "DNodeOperation.h"


#include <tbb/blocked_range.h>
#include <tbb/parallel_reduce.h>
#include <tbb/parallel_for.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <limits>




#pragma mark DPDirectionManip

MManipData DPDirectionManip::plugToManipConversion(unsigned int)
{
		MStatus status;
		MFnStateManip stateManip(fStateManip, &status);
		if (!status) return MManipData();

		MFnManip3D baseVecManipFn(fBaseVectorManip, &status);
		if (!status) return MManipData();
		MFnManip3D targetVecManipFn(fTargetVectorManip, &status);
		if (!status) return MManipData();
		MFnManip3D useDistManipFn(fUseDistManip, &status);
		if (!status) return MManipData();
		MFnManip3D distanceManipFn(fDistanceManip, &status);
		if (!status) return MManipData();
		MFnManip3D scaleManipFn(fScaleManip, &status);
		if (!status) return MManipData();

		status = baseVecManipFn.setVisible(false);
		if (!status) return MManipData();
		status = targetVecManipFn.setVisible(false);
		if (!status) return MManipData();
		status = useDistManipFn.setVisible(false);
		if (!status) return MManipData();
		status = distanceManipFn.setVisible(false);
		if (!status) return MManipData();
		status = scaleManipFn.setVisible(false);
		if (!status) return MManipData();

		const unsigned int mode = stateManip.state(&status);
		if (!status) return MManipData();
		if (mode == 0U) {
			status = baseVecManipFn.setVisible(true);
		} else if (mode == 1U) {
			status = targetVecManipFn.setVisible(true);
		} else if (mode == 2U) {
			status = useDistManipFn.setVisible(true);
			if (status) status = distanceManipFn.setVisible(true);
		} else if (mode == 3U) {
			status = scaleManipFn.setVisible(true);
		}
		if (!status) return MManipData();

	    MFnNumericData numData;
	    const MObject numDataObj = numData.create(MFnNumericData::k3Double, &status);
	    if (!status || numDataObj.isNull()) return MManipData();
	    const MVector origin = manipulatorOrigin();
	    status = numData.setData(origin.x, origin.y, origin.z);
	    return status ? MManipData(numDataObj) : MManipData();
}


MVector DPDirectionManip::manipulatorOrigin() const
{
	    // A dependency node has no DAG transform. Its vector values are
	    // represented relative to the stable world origin.
	    return MVector::zero;
}

MTypeId DPDirectionManip::id( ID_DP_DIR_MANIP );


void* DPDirectionManip::creator()
{
    return ::dommy::maya::createObject("DPDirectionManip", [&]() {
        return new DPDirectionManip();
    });
}

MStatus DPDirectionManip::initialize()
{
    return MPxManipContainer::initialize();
}

MStatus DPDirectionManip::createChildren()
{
		fBaseVectorManip = addFreePointTriadManip("baseVectorManip", "basevec");
		fTargetVectorManip = addFreePointTriadManip("targetVectorManip", "targetvec");
		fUseDistManip = addToggleManip("useDistManip", "usedist");
		fDistanceManip = addDistanceManip("distanceManip", "distance");
		fScaleManip = addDistanceManip("scaleManip", "scale");
		fStateManip = addStateManip("stateManip", "state");

		if (!fBaseVectorManip.isValid() || !fTargetVectorManip.isValid() ||
			!fUseDistManip.isValid() || !fDistanceManip.isValid() ||
			!fScaleManip.isValid() || !fStateManip.isValid()) {
			return MS::kFailure;
		}

		MStatus status;
		MPoint startPoint = MPoint::origin;
		MVector direction = MVector::yAxis;

		MFnToggleManip toggleManipFn(fUseDistManip, &status);
		if (!status) return status;
		status = toggleManipFn.setStartPoint(startPoint);
		if (!status) return status;
		status = toggleManipFn.setDirection(direction);
		if (!status) return status;

		MFnDistanceManip distanceManipFn(fDistanceManip, &status);
		if (!status) return status;
		status = distanceManipFn.setStartPoint(startPoint);
		if (!status) return status;
		status = distanceManipFn.setDirection(direction);
		if (!status) return status;

		MFnDistanceManip scaleManipFn(fScaleManip, &status);
		if (!status) return status;
		status = scaleManipFn.setStartPoint(startPoint);
		if (!status) return status;
		status = scaleManipFn.setDirection(direction);
		if (!status) return status;

		MFnStateManip stateManip(fStateManip, &status);
		if (!status) return status;
		status = stateManip.setMaxStates(4U);
		if (!status) return status;
	    return stateManip.setInitialState(0U);
}




MStatus DPDirectionManip::connectToDependNode(const MObject &node)
{
	    MStatus status;
	    MFnDependencyNode nodeFn(node, &status);
	    if (!status) return status;

		MFnFreePointTriadManip baseVecManipFn(fBaseVectorManip, &status);
		if (!status) return status;
		MPlug baseVecPlug = nodeFn.findPlug("baseVector", true, &status);
		if (!status) return status;
		status = baseVecManipFn.connectToPointPlug(baseVecPlug);
		if (!status) return status;

		MFnFreePointTriadManip targetVecManipFn(fTargetVectorManip, &status);
		if (!status) return status;
		MPlug targetVecPlug = nodeFn.findPlug("targetVector", true, &status);
		if (!status) return status;
		status = targetVecManipFn.connectToPointPlug(targetVecPlug);
		if (!status) return status;

		MFnToggleManip toggleManipFn(fUseDistManip, &status);
		if (!status) return status;
		MPlug togglePlug = nodeFn.findPlug("UseDistance", true, &status);
		if (!status) return status;
		status = toggleManipFn.connectToTogglePlug(togglePlug);
		if (!status) return status;
		unsigned int startPointIndex = toggleManipFn.startPointIndex(&status);
		if (!status) return status;
		addPlugToManipConversion(startPointIndex);

		MFnDistanceManip distanceManipFn(fDistanceManip, &status);
		if (!status) return status;
		MPlug distancePlug = nodeFn.findPlug("Distance", true, &status);
		if (!status) return status;
		status = distanceManipFn.connectToDistancePlug(distancePlug);
		if (!status) return status;
		startPointIndex = distanceManipFn.startPointIndex(&status);
		if (!status) return status;
		addPlugToManipConversion(startPointIndex);

		MFnDistanceManip scaleManipFn(fScaleManip, &status);
		if (!status) return status;
		MPlug scalePlug = nodeFn.findPlug("Scale", true, &status);
		if (!status) return status;
		status = scaleManipFn.connectToDistancePlug(scalePlug);
		if (!status) return status;
		startPointIndex = scaleManipFn.startPointIndex(&status);
		if (!status) return status;
		addPlugToManipConversion(startPointIndex);

		MFnStateManip stateManip(fStateManip, &status);
		if (!status) return status;
		status = stateManip.setTranslation(
			MVector(2.0, 0.0, 0.0), MSpace::kObject);
		if (!status) return status;
		const unsigned int positionIndex = stateManip.positionIndex(&status);
		if (!status) return status;
		addPlugToManipConversion(positionIndex);

	    status = finishAddingManips();
	    if (!status) return status;
	    return MPxManipContainer::connectToDependNode(node);
}

void DPDirectionManip::draw(M3dView & view, const MDagPath &path,
                                 M3dView::DisplayStyle style,
                                 M3dView::DisplayStatus status) try
{
    MPxManipContainer::draw(view, path, style, status);
    dommy::maya::ScopedLegacyGL drawing(view);
    
	    MPoint textPos = manipulatorOrigin();
		
		MFnStateManip stateManip(fStateManip);
		const unsigned int mode = stateManip.state();
		const MString distanceText(
			mode == 0U ? "Base Vector" :
			mode == 1U ? "Target Vector" :
			mode == 2U ? "Distance" :
			mode == 3U ? "Scale" : "Unknown");
	    view.drawText(distanceText, textPos, M3dView::kLeft);
    
}
DOMMY_DRAW_CATCH("DPDirectionManip")

// Viewport 2.0 manipulator draw overrides
void DPDirectionManip::preDrawUI(const M3dView&)
{
	    fTextPosition = manipulatorOrigin();
}

void DPDirectionManip::drawUI(MHWRender::MUIDrawManager& drawManager,
	                              const MHWRender::MFrameContext&) const
{
    drawManager.beginDrawable();
    
    drawManager.setColor( MColor( 0.0f, 1.0f, 0.1f ) );
	
	MFnStateManip stateManip(fStateManip);
	
	int mode = stateManip.state();
	if (mode==0) {
		drawManager.text( fTextPosition, "Base Vector", MHWRender::MUIDrawManager::kLeft );
	} else if (mode==1) {
		drawManager.text( fTextPosition, "Target Vector", MHWRender::MUIDrawManager::kLeft );
	} else if (mode==2) {
		drawManager.text( fTextPosition, "Distance", MHWRender::MUIDrawManager::kLeft );
	} else if (mode==3) {
		drawManager.text( fTextPosition, "Scale", MHWRender::MUIDrawManager::kLeft );
	}
	
	
	
    drawManager.endDrawable();
}





#pragma mark Utilities

namespace {

bool validDirectionBudget(std::size_t queryCount, std::size_t partitionCount,
                          short directionType, short targetCalculationType,
                          std::size_t targetCount)
{
    using namespace dommy::generation;
    if (partitionCount > kMaxConnectedGeometryCount || targetCount > kMaxGeneratedPointCount ||
        !dommy::attribute::validArrayConversionBudget(queryCount, sizeof(DVec3Data)))
        return false;
    // One aggregate budget for all point/curve/mesh partitions, including the
    // final adjustment pass. Match ID is linear, not a nearest-neighbor search.
    std::size_t work = queryCount;
    return (directionType != 3 || targetCalculationType == 2 ||
            checkedProduct(queryCount, targetCount, kMaxWorkItemCount, work)) &&
        checkedSum(work, queryCount, kMaxWorkItemCount, work);
}

bool isSelectedByDistance(
    const dommy::direction::Delta& delta,
    bool useDistance,
    double maximumDistance)
{
    return !useDistance || delta.length < maximumDistance;
}

template <typename FromVector, typename ToVector>
MStatus assignDirectionToTarget(
    const FromVector& from,
    const ToVector& to,
    bool useDistance,
    double maximumDistance,
    DVec3Data& output)
{
    dommy::direction::Delta delta;
    if (!dommy::direction::difference(from, to, delta)) {
        return MS::kFailure;
    }
    if (!isSelectedByDistance(delta, useDistance, maximumDistance)) {
        return MS::kSuccess;
    }

    DVec3Data value;
    if (!dommy::direction::assignDelta(delta, value)) {
        return MS::kFailure;
    }
    output = value;
    return MS::kSuccess;
}

template <typename FromVector, typename ToVector, typename DirectionVector>
MStatus assignDirectionAtTarget(
    const FromVector& from,
    const ToVector& to,
    const DirectionVector& direction,
    bool useDistance,
    double maximumDistance,
    DVec3Data& output)
{
    dommy::direction::Delta delta;
    if (!dommy::direction::difference(from, to, delta)) {
        return MS::kFailure;
    }
    if (!isSelectedByDistance(delta, useDistance, maximumDistance)) {
        return MS::kSuccess;
    }

    DVec3Data value;
    if (!dommy::direction::assignFinite(direction, value)) {
        return MS::kFailure;
    }
    output = value;
    return MS::kSuccess;
}

#pragma mark LookDirOp
class LookDirOp
{
public:
    LookDirOp(
        bool useDistance,
        double distance,
        const DPoint& targetPoint,
        const DPointList& points,
        DVec3DataList& values,
        std::atomic<bool>& valid)
        : m_useDistance(useDistance)
        , m_distance(distance)
        , m_targetPoint(targetPoint)
        , m_points(&points)
        , m_values(&values)
        , m_valid(&valid)
    {}

    void operator()(const tbb::blocked_range<size_t>& range) const
    {
        for (size_t i = range.begin(); i != range.end(); ++i) {
            if (!assignDirectionToTarget(
                    (*m_points)[i],
                    m_targetPoint,
                    m_useDistance,
                    m_distance,
                    (*m_values)[i])) {
                m_valid->store(false, std::memory_order_relaxed);
            }
        }
    }

private:
    const bool m_useDistance;
    const double m_distance;
    const DPoint m_targetPoint;
    const DPointList* m_points;
    DVec3DataList* m_values;
    std::atomic<bool>* m_valid;
};

#pragma mark CurveDirOp
class CurveDirOp
{
public:
    CurveDirOp(
        const MObject& object,
        bool useDistance,
        double distance,
        short calculationType,
        short normalType,
        const DPointList& points,
        DVec3DataList& values)
        : m_object(object)
        , m_useDistance(useDistance)
        , m_distance(distance)
        , m_calculationType(calculationType)
        , m_normalType(normalType)
        , m_points(&points)
        , m_values(&values)
    {}

    MStatus operator()(const tbb::blocked_range<size_t>& range) const
    {
        MStatus status;
        MFnNurbsCurve curveFn(m_object, &status);
        if (!status) return status;

        for (size_t i = range.begin(); i != range.end(); ++i) {
            const DPoint& point = (*m_points)[i];
            const MPoint source(point[0], point[1], point[2]);
            double parameter = 0.0;
            const MPoint closest = curveFn.closestPoint(
                source,
                &parameter,
                kMFnNurbsEpsilon,
                MSpace::kWorld,
                &status);
            if (!status) return status;

            if (m_calculationType == 0) {
                status = assignDirectionToTarget(
                    point,
                    closest,
                    m_useDistance,
                    m_distance,
                    (*m_values)[i]);
            } else if (m_calculationType == 1) {
                const MVector direction =
                    m_normalType == 0
                        ? curveFn.normal(parameter, MSpace::kWorld, &status)
                        : curveFn.tangent(parameter, MSpace::kWorld, &status);
                if (!status) return status;
                status = assignDirectionAtTarget(
                    point,
                    closest,
                    direction,
                    m_useDistance,
                    m_distance,
                    (*m_values)[i]);
            } else {
                return MS::kFailure;
            }
            if (!status) return status;
        }
        return MS::kSuccess;
    }

private:
    const MObject m_object;
    const bool m_useDistance;
    const double m_distance;
    const short m_calculationType;
    const short m_normalType;
    const DPointList* m_points;
    DVec3DataList* m_values;
};

#pragma mark SurfaceDirOp
class SurfaceDirOp
{
public:
    SurfaceDirOp(
        const MObject& object,
        bool useDistance,
        double distance,
        short calculationType,
        short normalType,
        const DPointList& points,
        DVec3DataList& values)
        : m_object(object)
        , m_useDistance(useDistance)
        , m_distance(distance)
        , m_calculationType(calculationType)
        , m_normalType(normalType)
        , m_points(&points)
        , m_values(&values)
    {}

    MStatus operator()(const tbb::blocked_range<size_t>& range) const
    {
        MStatus status;
        MFnNurbsSurface surfaceFn(m_object, &status);
        if (!status) return status;

        for (size_t i = range.begin(); i != range.end(); ++i) {
            const DPoint& point = (*m_points)[i];
            const MPoint source(point[0], point[1], point[2]);
            double parameterU = 0.0;
            double parameterV = 0.0;
            const MPoint closest = surfaceFn.closestPoint(
                source,
                &parameterU,
                &parameterV,
                false,
                0.01,
                MSpace::kWorld,
                &status);
            if (!status) return status;

            if (m_calculationType == 0) {
                status = assignDirectionToTarget(
                    point,
                    closest,
                    m_useDistance,
                    m_distance,
                    (*m_values)[i]);
            } else if (m_calculationType == 1) {
                MVector direction;
                if (m_normalType == 0) {
                    direction = surfaceFn.normal(
                        parameterU,
                        parameterV,
                        MSpace::kWorld,
                        &status);
                } else {
                    MVector tangentU;
                    MVector tangentV;
                    status = surfaceFn.getTangents(
                        parameterU,
                        parameterV,
                        tangentU,
                        tangentV,
                        MSpace::kWorld);
                    direction = m_normalType == 1 ? tangentU : tangentV;
                }
                if (!status) return status;
                status = assignDirectionAtTarget(
                    point,
                    closest,
                    direction,
                    m_useDistance,
                    m_distance,
                    (*m_values)[i]);
            } else {
                return MS::kFailure;
            }
            if (!status) return status;
        }
        return MS::kSuccess;
    }

private:
    const MObject m_object;
    const bool m_useDistance;
    const double m_distance;
    const short m_calculationType;
    const short m_normalType;
    const DPointList* m_points;
    DVec3DataList* m_values;
};

#pragma mark MeshDirOp
class MeshDirOp
{
public:
    MeshDirOp(
        const MObject& object,
        bool useDistance,
        double distance,
        short calculationType,
        const DPointList& points,
        DVec3DataList& values)
        : m_object(object)
        , m_useDistance(useDistance)
        , m_distance(distance)
        , m_calculationType(calculationType)
        , m_points(&points)
        , m_values(&values)
    {}

    MStatus operator()(const tbb::blocked_range<size_t>& range) const
    {
        MStatus status;
        MFnMesh meshFn(m_object, &status);
        if (!status) return status;
        MMeshIsectAccelParams accelerator = MFnMesh::autoUniformGridParams();

        for (size_t i = range.begin(); i != range.end(); ++i) {
            const DPoint& point = (*m_points)[i];
            const MPoint source(point[0], point[1], point[2]);
            MPoint closest;
            MVector normal;
            status = meshFn.getClosestPointAndNormal(
                source,
                closest,
                normal,
                MSpace::kWorld,
                nullptr,
                &accelerator);
            if (!status) return status;

            if (m_calculationType == 0) {
                status = assignDirectionToTarget(
                    point,
                    closest,
                    m_useDistance,
                    m_distance,
                    (*m_values)[i]);
            } else if (m_calculationType == 1) {
                status = assignDirectionAtTarget(
                    point,
                    closest,
                    normal,
                    m_useDistance,
                    m_distance,
                    (*m_values)[i]);
            } else {
                return MS::kFailure;
            }
            if (!status) return status;
        }
        return MS::kSuccess;
    }

private:
    const MObject m_object;
    const bool m_useDistance;
    const double m_distance;
    const short m_calculationType;
    const DPointList* m_points;
    DVec3DataList* m_values;
};

#pragma mark DPDirOp
class DPDirOp
{
public:
    DPDirOp(
        const DPointList& targetPoints,
        short targetCalculationType,
        bool useDistance,
        double distance,
        const DPointList& points,
        DVec3DataList& values,
        const MVectorArray& vectorArray,
        std::atomic<bool>& valid)
        : m_targetPoints(&targetPoints)
        , m_targetCalculationType(targetCalculationType)
        , m_useDistance(useDistance)
        , m_distance(distance)
        , m_points(&points)
        , m_values(&values)
        , m_vectorArray(&vectorArray)
        , m_valid(&valid)
    {}

    void operator()(const tbb::blocked_range<size_t>& range) const
    {
        if (m_targetCalculationType == 2) {
            const size_t matchCount =
                std::min(m_points->size(), m_targetPoints->size());
            for (size_t i = range.begin(); i != range.end() && i < matchCount; ++i) {
                if (!assignDirectionToTarget(
                        (*m_points)[i],
                        (*m_targetPoints)[i],
                        false,
                        0.0,
                        (*m_values)[i])) {
                    m_valid->store(false, std::memory_order_relaxed);
                }
            }
            return;
        }

        for (size_t i = range.begin(); i != range.end(); ++i) {
            double nearestDistance = std::numeric_limits<double>::max();
            size_t nearestIndex = m_targetPoints->size();

            for (size_t j = 0U; j < m_targetPoints->size(); ++j) {
                dommy::direction::Delta delta;
                if (!dommy::direction::difference(
                        (*m_points)[i], (*m_targetPoints)[j], delta)) {
                    m_valid->store(false, std::memory_order_relaxed);
                    continue;
                }
                if (isSelectedByDistance(delta, m_useDistance, m_distance) &&
                    delta.length < nearestDistance) {
                    nearestDistance = delta.length;
                    nearestIndex = j;
                }
            }

            if (nearestIndex == m_targetPoints->size()) continue;

            MStatus status;
            if (m_targetCalculationType == 0) {
                status = assignDirectionToTarget(
                    (*m_points)[i],
                    (*m_targetPoints)[nearestIndex],
                    false,
                    0.0,
                    (*m_values)[i]);
            } else if (m_targetCalculationType == 1) {
                status = assignDirectionAtTarget(
                    (*m_points)[i],
                    (*m_targetPoints)[nearestIndex],
                    (*m_vectorArray)[static_cast<unsigned int>(nearestIndex)],
                    false,
                    0.0,
                    (*m_values)[i]);
            } else {
                status = MS::kFailure;
            }
            if (!status) {
                m_valid->store(false, std::memory_order_relaxed);
            }
        }
    }

private:
    const DPointList* m_targetPoints;
    const short m_targetCalculationType;
    const bool m_useDistance;
    const double m_distance;
    const DPointList* m_points;
    DVec3DataList* m_values;
    const MVectorArray* m_vectorArray;
    std::atomic<bool>* m_valid;
};

#pragma mark DirectionGen
class DirectionGen
{
public:
    DirectionGen(
        short type,
        const std::vector<const DPointList*>& points,
        std::vector<DVec3DataList>& values)
        : m_type(type)
        , m_targetPoint(0.0f, 0.0f, 0.0f)
        , m_distance(0.0)
        , m_useDistance(false)
        , m_calculationType(0)
        , m_curveNormalType(0)
        , m_surfaceNormalType(0)
        , m_targetCalculationType(0)
        , m_objectType(MFnData::kInvalid)
        , m_object(MObject::kNullObj)
        , m_targetPoints(nullptr)
        , m_points(&points)
        , m_values(&values)
    {}

    MStatus operator()()
    {
        if (!std::isfinite(m_distance) ||
            (m_useDistance && m_distance < 0.0) ||
            !m_points ||
            !m_values ||
            m_points->size() != m_values->size()) {
            return MS::kFailure;
        }
        std::size_t queryCount = 0U;
        for (size_t i = 0U; i < m_points->size(); ++i) {
            if (!(*m_points)[i] ||
                (*m_points)[i]->size() != (*m_values)[i].size() ||
                !dommy::generation::checkedSum(queryCount, (*m_points)[i]->size(),
                    dommy::generation::kMaxGeneratedPointCount, queryCount)) {
                return MS::kFailure;
            }
        }
        if (!validDirectionBudget(queryCount, m_points->size(), m_type,
                m_targetCalculationType, m_targetPoints ? m_targetPoints->size() : 0U))
            return MS::kInvalidParameter;

        if (m_type == 0) return MS::kSuccess;

        if (m_type == 1) {
            std::atomic<bool> valid(true);
            for (size_t i = 0U; i < m_points->size(); ++i) {
                LookDirOp operation(
                    m_useDistance,
                    m_distance,
                    m_targetPoint,
                    *(*m_points)[i],
                    (*m_values)[i],
                    valid);
                tbb::parallel_for(
                    tbb::blocked_range<size_t>(
                        0U, (*m_points)[i]->size(), 10000U),
                    operation);
            }
            return valid.load(std::memory_order_relaxed)
                ? MS::kSuccess
                : MS::kFailure;
        }

        if (m_type == 2) {
            if (m_object.isNull() ||
                m_calculationType < 0 ||
                m_calculationType > 1 ||
                m_curveNormalType < 0 ||
                m_curveNormalType > 1 ||
                m_surfaceNormalType < 0 ||
                m_surfaceNormalType > 2) {
                return MS::kFailure;
            }

            for (size_t i = 0U; i < m_points->size(); ++i) {
                const tbb::blocked_range<size_t> range(
                    0U, (*m_points)[i]->size(), 10000U);
                MStatus status;
                if (m_objectType == MFnData::kNurbsCurve) {
                    status = CurveDirOp(
                        m_object,
                        m_useDistance,
                        m_distance,
                        m_calculationType,
                        m_curveNormalType,
                        *(*m_points)[i],
                        (*m_values)[i])(range);
                } else if (m_objectType == MFnData::kNurbsSurface) {
                    status = SurfaceDirOp(
                        m_object,
                        m_useDistance,
                        m_distance,
                        m_calculationType,
                        m_surfaceNormalType,
                        *(*m_points)[i],
                        (*m_values)[i])(range);
                } else if (m_objectType == MFnData::kMesh) {
                    status = MeshDirOp(
                        m_object,
                        m_useDistance,
                        m_distance,
                        m_calculationType,
                        *(*m_points)[i],
                        (*m_values)[i])(range);
                } else {
                    return MS::kFailure;
                }
                if (!status) return status;
            }
            return MS::kSuccess;
        }

        if (m_type == 3) {
            if (!m_targetPoints ||
                m_targetCalculationType < 0 ||
                m_targetCalculationType > 2 ||
                (m_targetCalculationType == 1 &&
                 m_vectorArray.length() != m_targetPoints->size())) {
                return MS::kFailure;
            }

            std::atomic<bool> valid(true);
            for (size_t i = 0U; i < m_points->size(); ++i) {
                DPDirOp operation(
                    *m_targetPoints,
                    m_targetCalculationType,
                    m_useDistance,
                    m_distance,
                    *(*m_points)[i],
                    (*m_values)[i],
                    m_vectorArray,
                    valid);
                tbb::parallel_for(
                    tbb::blocked_range<size_t>(
                        0U, (*m_points)[i]->size(), 10000U),
                    operation);
            }
            return valid.load(std::memory_order_relaxed)
                ? MS::kSuccess
                : MS::kFailure;
        }

        return MS::kFailure;
    }

    short m_type;
    DPoint m_targetPoint;
    double m_distance;
    bool m_useDistance;
    short m_calculationType;
    short m_curveNormalType;
    short m_surfaceNormalType;
    short m_targetCalculationType;
    MFnData::Type m_objectType;
    MObject m_object;
    const DPointList* m_targetPoints;
    MVectorArray m_vectorArray;

private:
    const std::vector<const DPointList*>* m_points;
    std::vector<DVec3DataList>* m_values;
};

#pragma mark DirectionAdjustOp
class DirectionAdjustOp
{
public:
    DirectionAdjustOp(
        bool invert,
        bool normalize,
        double scale,
        DVec3DataList& values,
        std::atomic<bool>& valid)
        : m_invert(invert)
        , m_normalize(normalize)
        , m_scale(scale)
        , m_values(&values)
        , m_valid(&valid)
    {}

    void operator()(const tbb::blocked_range<size_t>& range) const
    {
        for (size_t i = range.begin(); i != range.end(); ++i) {
            DVec3Data adjusted;
            if (dommy::direction::adjust(
                    (*m_values)[i],
                    m_invert,
                    m_normalize,
                    m_scale,
                    adjusted)) {
                (*m_values)[i] = adjusted;
            } else {
                m_valid->store(false, std::memory_order_relaxed);
            }
        }
    }

private:
    const bool m_invert;
    const bool m_normalize;
    const double m_scale;
    DVec3DataList* m_values;
    std::atomic<bool>* m_valid;
};

MStatus adjustDirections(
    bool invert,
    bool normalize,
    double scale,
    std::vector<DVec3DataList>& values)
{
    if (!std::isfinite(scale)) return MS::kFailure;

    std::atomic<bool> valid(true);
    for (DVec3DataList& valueList : values) {
        DirectionAdjustOp operation(
            invert, normalize, scale, valueList, valid);
        tbb::parallel_for(
            tbb::blocked_range<size_t>(0U, valueList.size(), 10000U),
            operation);
    }
    return valid.load(std::memory_order_relaxed)
        ? MS::kSuccess
        : MS::kFailure;
}

MStatus loadTargetDirections(
    const DPData& targetData,
    const std::string& attributeName,
    size_t expectedCount,
    MVectorArray& directions)
{
    if (expectedCount > std::numeric_limits<unsigned int>::max() ||
        !targetData.isExistAttr(attributeName)) {
        return MS::kFailure;
    }

    const DPAttributeData::ConstPtr attribute = targetData.getAttr(attributeName);
    if (!attribute || !isVector3DAttrType(attribute->getAttrType())) {
        return MS::kFailure;
    }

    if (!getCheckedMVectorArrayFromAttribute(
            targetData, attributeName, expectedCount, directions))
        return MS::kFailure;

    for (unsigned int i = 0U; i < directions.length(); ++i) {
        DVec3Data checked;
        if (!dommy::direction::assignFinite(directions[i], checked)) {
            return MS::kFailure;
        }
    }
    return MS::kSuccess;
}

};//Utilities


#pragma mark DPDirection

MTypeId DPDirection::typeId( ID_DP_DIRECTION );

MObject DPDirection::inDD;

MObject DPDirection::dirAttrName;

MObject DPDirection::aDirectionType;
MObject DPDirection::baseVector;
MObject DPDirection::targetVector;

MObject DPDirection::inObject;
MObject DPDirection::inTargetDP;

MObject DPDirection::aCalcType;
MObject DPDirection::aUseDistance;
MObject DPDirection::aDistance;

MObject DPDirection::aCurveNormalType;
MObject DPDirection::aSurfaceNormalType;

MObject DPDirection::aTargetDPCalcType;
MObject DPDirection::aTargetDPAttrName;

MObject DPDirection::aInvert;
MObject DPDirection::aNormalize;
MObject DPDirection::aScale;

MObject DPDirection::aAttributeClass;

MObject DPDirection::outDD;


void *DPDirection::creator() {
    return ::dommy::maya::createObject("DPDirection", [&]() {
        return new DPDirection();
    });
}

MStatus DPDirection::compute(const MPlug& plug, MDataBlock& data) try
{
    if (plug != outDD) return MS::kUnknownParameter;

    MStatus status;
    const MDataHandle inputHandle = data.inputValue(inDD, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const MTypeId inputType = inputHandle.typeId();

    MDataHandle handle = data.inputValue(dirAttrName, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const MString nameValue = handle.asString();
    if (nameValue.length() > dommy::generation::kMaxGeneratedStringByteCount)
        return MS::kInvalidParameter;
    const std::string attributeName = nameValue.asChar();
    handle = data.inputValue(aDirectionType, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const short directionType = handle.asShort();

    handle = data.inputValue(baseVector, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DVec3Data base;
    if (!dommy::direction::assignFinite(handle.asDouble3(), base))
        return MS::kInvalidParameter;
    handle = data.inputValue(targetVector, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DVec3Data target;
    if (!dommy::direction::assignFinite(handle.asDouble3(), target))
        return MS::kInvalidParameter;

    handle = data.inputValue(aCalcType, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const short calculationType = handle.asShort();
    handle = data.inputValue(aUseDistance, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const bool useDistance = handle.asBool();
    handle = data.inputValue(aDistance, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const double distance = handle.asDouble();
    handle = data.inputValue(aCurveNormalType, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const short curveNormalType = handle.asShort();
    handle = data.inputValue(aSurfaceNormalType, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const short surfaceNormalType = handle.asShort();
    handle = data.inputValue(aInvert, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const bool invert = handle.asBool();
    handle = data.inputValue(aNormalize, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const bool normalize = handle.asBool();
    handle = data.inputValue(aScale, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const double scale = handle.asDouble();
    handle = data.inputValue(aAttributeClass, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const short attributeClass = handle.asShort();
    if (directionType < 0 || directionType > 3 ||
        calculationType < 0 || calculationType > 1 ||
        curveNormalType < 0 || curveNormalType > 1 ||
        surfaceNormalType < 0 || surfaceNormalType > 2 ||
        attributeClass < DP_ATTR_CLASS_POINT || attributeClass >= DP_ATTR_CLASS_TYPE_COUNT ||
        !std::isfinite(distance) || (useDistance && distance < 0.0) ||
        !std::isfinite(scale)) return MS::kInvalidParameter;

    const DPData* points = nullptr;
    const DCurveData* curves = nullptr;
    const DMeshData* meshes = nullptr;
    std::size_t queryCount = 0U, partitionCount = 0U;
    const bool primitiveAttribute = attributeClass == DP_ATTR_CLASS_PRIMITIVE;
    if (inputType == DPData::id) {
        DOMMY_RETURN_IF_FAILURE(dommy::maya::getPluginDataFromHandle(inputHandle, DPData::id, points));
        if (!points || !points->hasValidPointTopology()) return MS::kInvalidParameter;
        queryCount = points->numberOfPoints();
        partitionCount = queryCount == 0U ? 0U : 1U;
    } else if (inputType == DCurveData::id) {
        DOMMY_RETURN_IF_FAILURE(dommy::maya::getPluginDataFromHandle(inputHandle, DCurveData::id, curves));
        if (!curves || !curves->hasValidCurveTopology()) return MS::kInvalidParameter;
        partitionCount = curves->numberOfCurves();
        // DCurve historically always writes point attributes, including when
        // AttributeClass is Primitive. Keep that scene contract unchanged.
        for (auto curve = curves->beginCurves(); curve != curves->endCurves(); ++curve) {
            if (!*curve || !dommy::generation::checkedSum(queryCount, (*curve)->numberOfPoints(),
                    dommy::generation::kMaxGeneratedPointCount, queryCount))
                return MS::kInvalidParameter;
        }
    } else if (inputType == DMeshData::id) {
        DOMMY_RETURN_IF_FAILURE(dommy::maya::getPluginDataFromHandle(inputHandle, DMeshData::id, meshes));
        if (!meshes || !meshes->hasValidMeshTopology()) return MS::kInvalidParameter;
        if (primitiveAttribute) {
            queryCount = meshes->numberOfMesh();
            partitionCount = queryCount == 0U ? 0U : 1U;
        } else {
            partitionCount = meshes->numberOfMesh();
            for (auto mesh = meshes->beginMeshes(); mesh != meshes->endMeshes(); ++mesh) {
                if (!*mesh || !dommy::generation::checkedSum(queryCount, (*mesh)->numberOfPoints(),
                        dommy::generation::kMaxGeneratedPointCount, queryCount))
                    return MS::kInvalidParameter;
            }
        }
    } else {
        return MS::kInvalidParameter;
    }

    std::vector<const DPointList*> positions;
    std::vector<DVec3DataList> values;
    DPointList centers;
    DirectionGen generator(directionType, positions, values);
    generator.m_targetPoint = target;
    generator.m_useDistance = useDistance;
    generator.m_distance = distance;
    generator.m_calculationType = calculationType;
    generator.m_curveNormalType = curveNormalType;
    generator.m_surfaceNormalType = surfaceNormalType;
    const DPData* targets = nullptr;
    std::string targetAttributeName;

    // Inactive external inputs remain unread for empty containers/base-vector mode.
    if (partitionCount != 0U && directionType == 2) {
        handle = data.inputValue(inObject, &status);
        DOMMY_RETURN_IF_FAILURE(status);
        generator.m_objectType = handle.type();
        generator.m_object = handle.data();
    } else if (partitionCount != 0U && directionType == 3) {
        handle = data.inputValue(aTargetDPCalcType, &status);
        DOMMY_RETURN_IF_FAILURE(status);
        generator.m_targetCalculationType = handle.asShort();
        if (generator.m_targetCalculationType < 0 || generator.m_targetCalculationType > 2)
            return MS::kInvalidParameter;
        handle = data.inputValue(inTargetDP, &status);
        DOMMY_RETURN_IF_FAILURE(status);
        DOMMY_RETURN_IF_FAILURE(dommy::maya::getPluginDataFromHandle(handle, DPData::id, targets));
        if (!targets || !targets->hasValidPointTopology()) return MS::kInvalidParameter;
        generator.m_targetPoints = targets->getPositionPtrConst();
        if (generator.m_targetCalculationType == 1) {
            handle = data.inputValue(aTargetDPAttrName, &status);
            DOMMY_RETURN_IF_FAILURE(status);
            const MString targetName = handle.asString();
            if (targetName.length() > dommy::generation::kMaxGeneratedStringByteCount)
                return MS::kInvalidParameter;
            targetAttributeName = targetName.asChar();
        }
    }

    if (!validDirectionBudget(queryCount, partitionCount, directionType,
            generator.m_targetCalculationType, targets ? targets->numberOfPoints() : 0U))
        return MS::kInvalidParameter;

    // All partitions share one budget. Do not copy output or allocate direction
    // arrays until the complete nearest-neighbor workload has passed preflight.
    if (partitionCount != 0U) {
        positions.reserve(partitionCount);
        values.reserve(partitionCount);
        if (points) {
            positions.push_back(points->getPositionPtrConst());
        } else if (curves) {
            for (auto curve = curves->beginCurves(); curve != curves->endCurves(); ++curve)
                positions.push_back((*curve)->getPositionPtrConst());
        } else if (primitiveAttribute) {
            meshes->getPrimitiveCenterList(centers);
            if (centers.size() != queryCount) return MS::kFailure;
            positions.push_back(&centers);
        } else {
            for (auto mesh = meshes->beginMeshes(); mesh != meshes->endMeshes(); ++mesh)
                positions.push_back((*mesh)->getPositionPtrConst());
        }
        for (const DPointList* partition : positions) {
            if (!partition) return MS::kFailure;
            values.emplace_back(partition->size(), base);
        }
        if (targets && generator.m_targetCalculationType == 1)
            DOMMY_RETURN_IF_FAILURE(loadTargetDirections(*targets, targetAttributeName,
                targets->numberOfPoints(), generator.m_vectorArray));
        DOMMY_RETURN_IF_FAILURE(generator());
        DOMMY_RETURN_IF_FAILURE(adjustDirections(invert, normalize, scale, values));
    }

    if (points) {
        status = dommy::maya::setPluginDataOutput<DPData>(data, outDD, DPData::id,
            [&](DPData& output) {
                output.setDPData(*points);
                if (!values.empty()) {
                    checkDPAttribute(attributeName, output, true);
                    output.addAttrWithValues(attributeName, values[0]);
                }
                return MS::kSuccess;
            });
    } else if (curves) {
        status = dommy::maya::setPluginDataOutput<DCurveData>(data, outDD, DCurveData::id,
            [&](DCurveData& output) {
                DOMMY_RETURN_IF_FAILURE(output.copyFrom(*curves));
                if (!values.empty()) {
                    checkDCurvePointAttribute(attributeName, output, true);
                    std::size_t index = 0U;
                    for (auto curve = output.beginCurves(); curve != output.endCurves(); ++curve, ++index)
                        (*curve)->addAttrWithValues(attributeName, values[index]);
                }
                return MStatus(MS::kSuccess);
            });
    } else {
        status = dommy::maya::setPluginDataOutput<DMeshData>(data, outDD, DMeshData::id,
            [&](DMeshData& output) {
                DOMMY_RETURN_IF_FAILURE(output.copyFrom(*meshes));
                if (!values.empty()) {
                    if (primitiveAttribute) {
                        // Primitive and Point namespaces are independent. Never
                        // remove an existing Point field while replacing a Primitive field.
                        checkDMeshAttribute(attributeName, output, true);
                        output.addDPAttributeValues(attributeName, values[0]);
                    } else {
                        checkDMeshPointAttribute(attributeName, output, true);
                        std::size_t index = 0U;
                        for (auto mesh = output.beginMeshes(); mesh != output.endMeshes(); ++mesh, ++index)
                            (*mesh)->addAttrWithValues(attributeName, values[index]);
                    }
                }
                return MStatus(MS::kSuccess);
            });
    }
    return status ? data.setClean(plug) : status;
}
DOMMY_COMPUTE_CATCH("DPDirection")





MStatus DPDirection::initialize() try {
	MStatus stat;
	MFnNumericAttribute nAttr;
	MFnGenericAttribute gAttr;
	MFnTypedAttribute tAttr;
	
	MFnGenericAttribute outAttr;
	
	
	// ----------------------- INPUTS --------------------------
	inDD = gAttr.create( "inDD", "idd",  &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(gAttr.addAccept(DPData::id));
	DOMMY_RETURN_IF_FAILURE(gAttr.addAccept(DCurveData::id));
	DOMMY_RETURN_IF_FAILURE(gAttr.addAccept(DMeshData::id));
	DOMMY_RETURN_IF_FAILURE(gAttr.setReadable(false));
	DOMMY_RETURN_IF_FAILURE(gAttr.setStorable( false ));
    DOMMY_RETURN_IF_FAILURE(gAttr.setKeyable( true ));
	DOMMY_RETURN_IF_FAILURE(addAttribute(inDD));
	
	
	
	MFnStringData fnStringData;
	MObject attrName = fnStringData.create("N", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
	dirAttrName = tAttr.create("dirAttrName", "datn", MFnData::kString, attrName, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
	DOMMY_RETURN_IF_FAILURE(addAttribute(dirAttrName));
	
	
	MFnEnumAttribute enumFn;
	aDirectionType = enumFn.create( "DirectionType", "dirt", 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(enumFn.addField( "Attach Base Vector", 0 ));
	DOMMY_RETURN_IF_FAILURE(enumFn.addField( "Look Base Vector", 1 ));
	DOMMY_RETURN_IF_FAILURE(enumFn.addField( "Source Object", 2 ));
	DOMMY_RETURN_IF_FAILURE(enumFn.addField( "Target DP", 3 ));
	DOMMY_RETURN_IF_FAILURE(enumFn.setKeyable( true ));
	DOMMY_RETURN_IF_FAILURE(enumFn.setStorable(true));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aDirectionType));
	
	baseVector = nAttr.create( "baseVector", "bvec", MFnNumericData::k3Double , 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
	DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0, 1.0, 0.0));
	DOMMY_RETURN_IF_FAILURE(addAttribute(baseVector));
	
	targetVector = nAttr.create( "targetVector", "tvec", MFnNumericData::k3Double , 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
	DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0, 0.0, 0.0));
	DOMMY_RETURN_IF_FAILURE(addAttribute(targetVector));
	
	inObject = gAttr.create( "inObject", "ino",  &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(gAttr.addDataAccept(MFnData::kMesh));
	DOMMY_RETURN_IF_FAILURE(gAttr.addDataAccept(MFnData::kNurbsCurve));
	DOMMY_RETURN_IF_FAILURE(gAttr.addDataAccept(MFnData::kNurbsSurface));
	DOMMY_RETURN_IF_FAILURE(gAttr.setStorable(false));
	DOMMY_RETURN_IF_FAILURE(gAttr.setWritable( true ));
	DOMMY_RETURN_IF_FAILURE(gAttr.setReadable( false ));
	DOMMY_RETURN_IF_FAILURE(addAttribute(inObject));
	
	
	inTargetDP = tAttr.create("inTargetDP", "intdp", DPData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(tAttr.setReadable(false));
	DOMMY_RETURN_IF_FAILURE(tAttr.setStorable( false ));
	DOMMY_RETURN_IF_FAILURE(addAttribute(inTargetDP));
	
	
	
	MFnEnumAttribute calcEnumFn;
	aCalcType = calcEnumFn.create( "CalcType", "calt", 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(calcEnumFn.addField( "Nearest Point", 0 ));
	DOMMY_RETURN_IF_FAILURE(calcEnumFn.addField( "Nearest Normal", 1 ));
	DOMMY_RETURN_IF_FAILURE(calcEnumFn.setKeyable( true ));
	DOMMY_RETURN_IF_FAILURE(calcEnumFn.setStorable(true));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aCalcType));
	
	aUseDistance = nAttr.create( "UseDistance", "usd", MFnNumericData::kBoolean , 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( false ));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aUseDistance));
	
	aDistance = nAttr.create( "Distance", "dist", MFnNumericData::kDouble , 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0.0));
	DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(20.0));
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 5.0 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aDistance));
	
	MFnEnumAttribute crvNEnumFn;
	aCurveNormalType = crvNEnumFn.create( "CurveNormalType", "crvnt", 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(crvNEnumFn.addField( "Normal", 0 ));
	DOMMY_RETURN_IF_FAILURE(crvNEnumFn.addField( "Tangent", 1 ));
	DOMMY_RETURN_IF_FAILURE(crvNEnumFn.setKeyable( true ));
	DOMMY_RETURN_IF_FAILURE(crvNEnumFn.setStorable(true));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aCurveNormalType));
	
	MFnEnumAttribute srfNEnumFn;
	aSurfaceNormalType = srfNEnumFn.create( "SurfaceNormalType", "srfnt", 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(srfNEnumFn.addField( "Normal", 0 ));
	DOMMY_RETURN_IF_FAILURE(srfNEnumFn.addField( "TangentU", 1 ));
	DOMMY_RETURN_IF_FAILURE(srfNEnumFn.addField( "TangentV", 2 ));
	DOMMY_RETURN_IF_FAILURE(srfNEnumFn.setKeyable( true ));
	DOMMY_RETURN_IF_FAILURE(srfNEnumFn.setStorable(true));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aSurfaceNormalType));
	
	
	MFnEnumAttribute tarCalcEnumFn;
	aTargetDPCalcType = tarCalcEnumFn.create( "TargetDPCalcType", "tcalt", 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(tarCalcEnumFn.addField( "Nearest Point", 0 ));
	DOMMY_RETURN_IF_FAILURE(tarCalcEnumFn.addField( "Nearest Normal", 1 ));
	DOMMY_RETURN_IF_FAILURE(tarCalcEnumFn.addField( "Match ID", 2 ));
	DOMMY_RETURN_IF_FAILURE(tarCalcEnumFn.setKeyable( true ));
	DOMMY_RETURN_IF_FAILURE(tarCalcEnumFn.setStorable(true));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aTargetDPCalcType));
	
	
    MObject targetAttrNameStr = fnStringData.create("N", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aTargetDPAttrName = tAttr.create("TargetDPAttrName", "tdpan", MFnData::kString, targetAttrNameStr, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aTargetDPAttrName));
    
    
    
	aInvert = nAttr.create( "Invert", "inv", MFnNumericData::kBoolean , 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( false ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aInvert));
	
	aNormalize = nAttr.create( "Normalize", "norm", MFnNumericData::kBoolean , 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( false ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aNormalize));
	
	aScale = nAttr.create( "Scale", "sca", MFnNumericData::kDouble , 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(-10.0));
	DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(10.0));
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 1.0 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aScale));
    
    
    
    
    MFnEnumAttribute attrClassEnumFn;
    aAttributeClass = attrClassEnumFn.create( "AttributeClass", "attrcls", 0,&stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(attrClassEnumFn.addField( "Point", DP_ATTR_CLASS_POINT ));
    DOMMY_RETURN_IF_FAILURE(attrClassEnumFn.addField( "Primitive", DP_ATTR_CLASS_PRIMITIVE ));
    DOMMY_RETURN_IF_FAILURE(attrClassEnumFn.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aAttributeClass));
    
    
    
    
	
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
	DOMMY_RETURN_IF_FAILURE(attributeAffects( dirAttrName, outDD ));
	DOMMY_RETURN_IF_FAILURE(attributeAffects( aDirectionType, outDD ));
	DOMMY_RETURN_IF_FAILURE(attributeAffects( baseVector, outDD ));
	DOMMY_RETURN_IF_FAILURE(attributeAffects( targetVector, outDD ));
	
	DOMMY_RETURN_IF_FAILURE(attributeAffects( inObject, outDD ));
	DOMMY_RETURN_IF_FAILURE(attributeAffects( inTargetDP, outDD ));
	DOMMY_RETURN_IF_FAILURE(attributeAffects( aCalcType, outDD ));
	DOMMY_RETURN_IF_FAILURE(attributeAffects( aUseDistance, outDD ));
	DOMMY_RETURN_IF_FAILURE(attributeAffects( aDistance, outDD ));
	DOMMY_RETURN_IF_FAILURE(attributeAffects( aCurveNormalType, outDD ));
	DOMMY_RETURN_IF_FAILURE(attributeAffects( aSurfaceNormalType, outDD ));
	
	DOMMY_RETURN_IF_FAILURE(attributeAffects( aTargetDPCalcType, outDD ));
	DOMMY_RETURN_IF_FAILURE(attributeAffects( aTargetDPAttrName, outDD ));
    
	DOMMY_RETURN_IF_FAILURE(attributeAffects( aInvert, outDD ));
	DOMMY_RETURN_IF_FAILURE(attributeAffects( aNormalize, outDD ));
	DOMMY_RETURN_IF_FAILURE(attributeAffects( aScale, outDD ));
    
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aAttributeClass, outDD ));
	
    DOMMY_RETURN_IF_FAILURE(DPDirectionManip::addToManipConnectTable(typeId));
    
	return MS::kSuccess;
} catch (...) {
    return dommy::maya::reportEntryPointException(
        "DPDirection", "initialize", std::current_exception());
}
