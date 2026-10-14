//
//  DCurveShape.cpp
//  dommyTool
//
//  Created by yuzo on 3/25/17.
//
//

#include "DCurveShape.h"
#include "common/utility/DMayaEntryPointGuard.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DMayaPlugDataHandle.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DViewportCallbackGuard.h"

#include "DCurveDataHeaders.h"

MTypeId DCurveShape::id(ID_DC_SHAPE);

MObject DCurveShape::inDC;

MObject DCurveShape::aIsMultiEndKnot;

MObject DCurveShape::aCurveWidthType;
MObject DCurveShape::aConstantWidth;
MObject DCurveShape::aWidthAttributeName;

MObject DCurveShape::aExportCurveAttribute;
MObject DCurveShape::aExportPointAttribute;

// for arnold
MObject DCurveShape::aRenderMode;
MObject DCurveShape::aBasisMode;
MObject DCurveShape::aMinPixelWidth;

MObject DCurveShape::aOrientType;
MObject DCurveShape::aOrientAttributeName;

DCurveShape::DCurveShape() : dCurveGeom(new DCurveData) {}

DCurveShape::~DCurveShape() = default;

void* DCurveShape::creator() {
    return ::dommy::maya::createObject("DCurveShape", [&]() {
        return new DCurveShape();
    });
}

void DCurveShape::postConstructor()
{
    // Enable shading-group membership once Maya has attached the native node.
    setRenderable(true);
}

MStatus DCurveShape::compute(const MPlug& /*plug*/, MDataBlock& /*datablock*/) try {
    return MS::kUnknownParameter;
}
DOMMY_COMPUTE_CATCH("DCurveShape")

bool DCurveShape::isBounded() const {
    return true;
}

MBoundingBox DCurveShape::boundingBox() const try {
    const DCurveData* geom = geometry();
    return geom ? geom->getBoundingBox() : MBoundingBox();
}
DOMMY_VIEWPORT_BOUNDING_BOX_CATCH("DCurveShape")

MStatus DCurveShape::initialize() try {
    MStatus stat;
    MFnNumericAttribute nAttr;
    MFnTypedAttribute tAttr;
    MFnStringData fnStringData;

    inDC = tAttr.create("inDC", "indc", DCurveData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inDC));

    aIsMultiEndKnot = nAttr.create("IsMultiEndKnot", "ismltknt", MFnNumericData::kBoolean, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aIsMultiEndKnot));

    MFnEnumAttribute widthEnum;
    aCurveWidthType = widthEnum.create("CurveWidthType", "crvwty", 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(widthEnum.addField("Constant", 0));
    DOMMY_RETURN_IF_FAILURE(widthEnum.addField("From Attribute", 1));
    DOMMY_RETURN_IF_FAILURE(widthEnum.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aCurveWidthType));

    aConstantWidth = nAttr.create("ConstantWidth", "cw", MFnNumericData::kFloat, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.1));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aConstantWidth));

    const MObject aWidthAttributeNameDefaultData = fnStringData.create("width", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aWidthAttributeName = tAttr.create("WidthAttributeName", "wattr", MFnData::kString,
                                       aWidthAttributeNameDefaultData, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aWidthAttributeName));

    const MObject aExportCurveAttributeDefaultData = fnStringData.create("", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aExportCurveAttribute = tAttr.create("ExportCurveAttribute", "excrvattr", MFnData::kString,
                                         aExportCurveAttributeDefaultData, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aExportCurveAttribute));

    const MObject aExportPointAttributeDefaultData = fnStringData.create("", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aExportPointAttribute = tAttr.create("ExportPointAttribute", "exptattr", MFnData::kString,
                                         aExportPointAttributeDefaultData, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aExportPointAttribute));

#pragma mark arnold attribute

    MFnEnumAttribute modeEnumFn;
    aRenderMode = modeEnumFn.create("RenderMode", "renmd", 1, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(modeEnumFn.addField("ribbon", 0));
    DOMMY_RETURN_IF_FAILURE(modeEnumFn.addField("thick", 1));
    DOMMY_RETURN_IF_FAILURE(modeEnumFn.addField("oriented", 2));
    DOMMY_RETURN_IF_FAILURE(modeEnumFn.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aRenderMode));

    MFnEnumAttribute basisEnumFn;
    aBasisMode = basisEnumFn.create("BasisMode", "bmode", 1, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(basisEnumFn.addField("Linear", 0));
    DOMMY_RETURN_IF_FAILURE(basisEnumFn.addField("B-Spline", 1));
    DOMMY_RETURN_IF_FAILURE(basisEnumFn.addField("Catmull-Rom", 2));
    DOMMY_RETURN_IF_FAILURE(basisEnumFn.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aBasisMode));

    aMinPixelWidth = nAttr.create("MinPixelWidth", "mpxw", MFnNumericData::kFloat, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.001));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aMinPixelWidth));

    MFnEnumAttribute oriEnumFn;
    aOrientType = oriEnumFn.create("OrientType", "ority", 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(oriEnumFn.addField("None", 0));
    DOMMY_RETURN_IF_FAILURE(oriEnumFn.addField("Curve Attribute", 1));
    DOMMY_RETURN_IF_FAILURE(oriEnumFn.addField("Point Attribute", 2));
    DOMMY_RETURN_IF_FAILURE(oriEnumFn.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aOrientType));

    const MObject aOrientAttributeNameDefaultData = fnStringData.create("", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aOrientAttributeName = tAttr.create("OrientAttributeName", "orattr", MFnData::kString,
                                        aOrientAttributeNameDefaultData, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aOrientAttributeName));

    return stat;
}
catch (...) {
    return dommy::maya::reportEntryPointException(
        "DCurveShape", "initialize", std::current_exception());
}

const DCurveData* DCurveShape::geometry() const
{
    // Read the current input through a scoped plug handle, then refresh the
    // shape-owned snapshot. Failed reads/copies do not expose a stale snapshot.
    MObject this_object = thisMObject();
    MPlug dcPlug(this_object, inDC);
    dommy::maya::ScopedPlugDataHandle dCurveHndOwner;
    MDataHandle& dCurveHnd = dCurveHndOwner.handle();
    if (!dCurveHndOwner.read(dcPlug)) {
        return nullptr;
    }
    const DCurveData* const dCurves(getInputDCfromHandle(dCurveHnd));
    if (!dCurves) return nullptr;
    if (!dCurveGeom->copyFrom(*dCurves)) {
        MGlobal::displayError("DCurveShape: unable to copy input geometry.");
        return nullptr;
    }

    return dCurveGeom.get();
}
