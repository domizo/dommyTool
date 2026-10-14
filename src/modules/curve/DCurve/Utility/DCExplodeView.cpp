//
//  DCExplodeView.cpp
//  dommyTool
//
//  Created by yuzo on 2017/11/28.
//

#include "common/utility/DMayaEntryPointGuard.h"
#include "DCExplodeView.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DMayaExplodeNode.h"
#include "common/utility/DMayaPluginDataOutput.h"
#include "common/utility/DComputeGuard.h"




#include "DMayaHeaders.h"
#include "DMayaDebug.h"


#include "DCurveData.h"
#include "dcurve_ids.h"





const MTypeId DCExplodeView::typeId( ID_DC_EXPLODE_VIEW );


MObject DCExplodeView::inDC;

MObject DCExplodeView::aEnable;
MObject DCExplodeView::aScale;
MObject DCExplodeView::aCenter;
MObject DCExplodeView::aPieceCenterAttributeName;

MObject DCExplodeView::aVectorType;
MObject DCExplodeView::aVectorAttributeName;

MObject DCExplodeView::aPieceGroup;

MObject DCExplodeView::outDC;



void *DCExplodeView::creator()
{
    return ::dommy::maya::createObject("DCExplodeView", [&]() {
        return new DCExplodeView();
    });
}






MStatus DCExplodeView::compute(const MPlug& plug, MDataBlock& data) try
{
    return dommy::maya::computeExplodeNode<DCExplodeView, DCurveData, false>(
        plug, data, inDC, outDC);
}
DOMMY_COMPUTE_CATCH("DCExplodeView")



MStatus DCExplodeView::initialize() {
    MStatus stat;
    MFnNumericAttribute nAttr;
    MFnTypedAttribute tAttr;
    MFnStringData fnStringData;
    inDC = tAttr.create("inDC", "idc", DCurveData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inDC));

    aEnable = nAttr.create("Enable", "enbl", MFnNumericData::kBoolean, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(true));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aEnable));

    aScale = nAttr.create("Scale", "scl", MFnNumericData::kDouble, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(5.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aScale));

    aCenter = nAttr.create("Center", "ctr", MFnNumericData::k3Double, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0, 0.0, 0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aCenter));

    const MObject aPieceCenterAttributeNameDefault = fnStringData.create("center", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aPieceCenterAttributeName =
        tAttr.create("PieceCenterAttributeName", "pccattrnm", MFnData::kString,
                     aPieceCenterAttributeNameDefault, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aPieceCenterAttributeName));

    MFnEnumAttribute opEnumFn;
    aVectorType = opEnumFn.create("VectorType", "vty", 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(opEnumFn.addField("Center to piece", 0));
    DOMMY_RETURN_IF_FAILURE(opEnumFn.addField("From attribute", 1));
    DOMMY_RETURN_IF_FAILURE(opEnumFn.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aVectorType));

    const MObject aVectorAttributeNameDefault = fnStringData.create("", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aVectorAttributeName = tAttr.create("VectorAttributeName", "vattrnm", MFnData::kString,
                                        aVectorAttributeNameDefault, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aVectorAttributeName));

    const MObject aPieceGroupDefault = fnStringData.create("piece.*", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aPieceGroup = tAttr.create("PieceGroup", "pcgrp", MFnData::kString, aPieceGroupDefault, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aPieceGroup));

    outDC = tAttr.create("outDC", "odc", DCurveData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(inDC, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aEnable, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aScale, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aCenter, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aPieceCenterAttributeName, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aVectorType, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aVectorAttributeName, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aPieceGroup, outDC));
    return MS::kSuccess;
}
