//
//  DMultiCurveUtil.cpp
//  dommyTool
//
//  Created by yuzo on 5/1/16.
//
//

#include "DCurveUtil.h"
#include "common/utility/DAttributeMathPublication.h"
#include "common/utility/DCheckedSize.h"
#include "common/utility/DMayaPluginData.h"
#include "common/utility/DMayaCurveKnots.h"
#include "common/utility/DGroupPattern.h"
#include "common/utility/DPointTransform.h"
#include "common/utility/DMayaValueValidation.h"


#include <maya/MObject.h>
#include <maya/MFnNurbsCurve.h>
#include <maya/MFnNurbsCurveData.h>

#include <stdexcept>
#include <limits>


const DCurveData* getInputDC(const MObject& crv, MDataBlock& data)
{
	MStatus stat;
	MDataHandle inputHandle = data.inputValue(crv, &stat);
	if (!stat) return NULL;

	const DCurveData* inputData = NULL;
	stat = dommy::maya::getPluginDataFromHandle(
		inputHandle, DCurveData::id, inputData);
	return stat && inputData && inputData->hasValidCurveTopology()
	           ? inputData
	           : NULL;
}

MStatus getValidInputDC(const MObject& curve,
                        MDataBlock& data,
                        const DCurveData*& inputData)
{
	inputData = NULL;
	MStatus status;
	const MDataHandle inputHandle = data.inputValue(curve, &status);
	if (!status) return status;

	status = dommy::maya::getPluginDataFromHandle(
		inputHandle, DCurveData::id, inputData);
	if (!status) return status;
	if (!inputData || !inputData->hasValidCurveTopology()) {
		inputData = NULL;
		return MS::kInvalidParameter;
	}
	return MS::kSuccess;
}

const DCurveData* getInputDCfromPlug(const MPlug &plg, MObject &obj) {
    const DCurveData* inputData = NULL;
    const MStatus status = dommy::maya::getPluginDataFromPlug(
        plg, DCurveData::id, obj, inputData);
    return status && inputData && inputData->hasValidCurveTopology()
               ? inputData
               : NULL;
}




const DCurveData* getInputDCfromHandle(MDataHandle hnd) {
	const DCurveData* inputData = NULL;
	const MStatus status = dommy::maya::getPluginDataFromHandle(
		hnd, DCurveData::id, inputData);
	return status && inputData && inputData->hasValidCurveTopology()
	           ? inputData
	           : NULL;
}
#pragma mark generate curve

MStatus generateMCurveFromDcurve(const DCItem &crv, MObject &newCurve) {
    ConstDCItem source = crv;
    return generateMCurveFromDcurve(source, newCurve);
}


MStatus generateMCurveFromDcurve(ConstDCItem &crv, MObject &newCurve) {
    if (!crv || !crv->hasValidCurveTopology())
        return MS::kInvalidParameter;

    MStatus status;
    MObject candidate = MFnNurbsCurveData().create(&status);
    if (!status) return status;
    // A null owner would make MFnNurbsCurve::create add a node to the DAG.
    if (candidate.isNull()) return MS::kFailure;

    MFnNurbsCurve curveFn;
    const MFnNurbsCurve::Form form = crv->isCurveOpen()
        ? MFnNurbsCurve::kOpen : MFnNurbsCurve::kClosed;
    const MObject geometry = curveFn.create(
        crv->getCvs(), crv->getKnots(), crv->getDegree(), form,
        false, false, candidate, &status);
    if (!status) return status;
    if (geometry.isNull()) return MS::kFailure;

    newCurve = candidate;
    return MS::kSuccess;
}

MStatus generateWorldMCurveFromDcurve(ConstDCItem curve, MObject& newCurve) {
    if (!curve || !curve->hasValidCurveTopology()) return MS::kInvalidParameter;
    const MMatrix matrix = curve->getTransformMatrix();
    if (!dommy::numeric::isFiniteAffineTransform(matrix)) return MS::kInvalidParameter;

    MObject candidate;
    MStatus status = generateMCurveFromDcurve(curve, candidate);
    if (!status) return status;
    MFnNurbsCurve function(candidate, &status);
    if (!status) return status;
    MPointArray local, world;
    status = function.getCVs(local, MSpace::kObject);
    if (!status) return status;
    if (local.length() != curve->numberOfPoints()) return MS::kFailure;
    status = dommy::maya::buildCheckedArray(local.length(),
        [&](unsigned int i, MPoint& point) -> MStatus {
            std::array<double, 3> transformed;
            if (!dommy::maya::isFinitePoint(local[i]) ||
                !dommy::numeric::checkedTransformPoint(local[i], matrix, transformed))
                return MS::kInvalidParameter;
            point = MPoint(transformed[0], transformed[1], transformed[2]);
            return dommy::maya::isFloatRepresentablePoint(point)
                ? MStatus(MS::kSuccess) : MStatus(MS::kInvalidParameter);
        }, world);
    if (!status) return status;
    status = function.setCVs(world, MSpace::kObject);
    if (!status) return status;
    status = function.updateCurve();
    if (!status) return status;
    newCurve = candidate;
    return MS::kSuccess;
}

#pragma mark generate knot

void GenerateDCurveKnots(const size_t& cvCount, const int& degree,
                         MDoubleArray& knots, bool isLoop)
{
	if (dommy::maya::buildCurveKnots(cvCount, degree, isLoop, knots) != MS::kSuccess)
		throw std::runtime_error("Curve knot array allocation or copy failed");
}






#pragma mark DCurve Attribute method

void checkDCurvePointAttribute(const std::string attrName,  DCurveData &outputDCData, bool delAttr) {
	
    for (auto cIt=outputDCData.beginCurves(),ecIt=outputDCData.endCurves(); cIt!=ecIt; ++cIt) {
		
        if ((*cIt)->isExistAttr(attrName) && delAttr) {
            (*cIt)->removeAttr(attrName);
        }
		
    }
	
}

void checkDCurveAttribute(const std::string attrName,  DCurveData &outputDCData, bool delAttr) {
	
    if (outputDCData.isExistAttr(attrName) && delAttr) {
        outputDCData.removeAttr(attrName);
    }
}


#pragma mark add method

MStatus DCAddAttrScalar(DCurveData &dcdata, const bool &isOutAttr, const bool &isDeleteOrg, const std::string &outAttrName, const std::string &originalAttrName, const DScalarDataList &result) {
    return DPublishAttributeValues(dcdata, isOutAttr, isDeleteOrg,
        outAttrName, originalAttrName, result);
}

MStatus DCAddAttrVec2(DCurveData &dcdata, const bool &isOutAttr, const bool &isDeleteOrg, const std::string &outAttrName, const std::string &originalAttrName, const DVec2DataList &result) {
    return DPublishAttributeValues(dcdata, isOutAttr, isDeleteOrg,
        outAttrName, originalAttrName, result);
}

MStatus DCAddAttrVec3(DCurveData &dcdata, const bool &isOutAttr, const bool &isDeleteOrg, const std::string &outAttrName, const std::string &originalAttrName, const DVec3DataList &result) {
    return DPublishAttributeValues(dcdata, isOutAttr, isDeleteOrg,
        outAttrName, originalAttrName, result);
}


MStatus DCPAddAttrScalar(DCItem &crv, const bool &isOutAttr, const bool &isDeleteOrg, const std::string &outAttrName, const std::string &originalAttrName, const DScalarDataList &result) {
    return DPublishAttributeValues(*crv, isOutAttr, isDeleteOrg,
        outAttrName, originalAttrName, result);
}

MStatus DCPAddAttrVec2(DCItem &crv, const bool &isOutAttr, const bool &isDeleteOrg, const std::string &outAttrName, const std::string &originalAttrName, const DVec2DataList &result) {
    return DPublishAttributeValues(*crv, isOutAttr, isDeleteOrg,
        outAttrName, originalAttrName, result);
}

MStatus DCPAddAttrVec3(DCItem &crv, const bool &isOutAttr, const bool &isDeleteOrg, const std::string &outAttrName, const std::string &originalAttrName, const DVec3DataList &result) {
    return DPublishAttributeValues(*crv, isOutAttr, isDeleteOrg,
        outAttrName, originalAttrName, result);
}




#pragma mark dcurve group method

void getDCurveMatchedGroups(const DCurveData &dcurve, const std::string& patternStr, std::vector<std::string> &matchedGrpNames) {
    if (!dommy::group::appendMatchedGroupNames(dcurve, patternStr, matchedGrpNames))
        throw std::invalid_argument("DCurve group pattern is invalid or exceeds its input budget");
}
