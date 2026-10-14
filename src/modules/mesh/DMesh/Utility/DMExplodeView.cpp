//
//  DMExplodeView.cpp
//  dommyTool
//
//  Created by yuzo on 2017/04/29.
//
//

#include "common/utility/DMayaEntryPointGuard.h"
#include "DMExplodeView.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DMayaExplodeNode.h"
#include "common/utility/DMayaPluginDataOutput.h"
#include "common/utility/DComputeGuard.h"




#include "DMayaHeaders.h"
#include "DMayaDebug.h"


#include "DMeshDataHeaders.h"





const MTypeId DMExplodeView::typeId( ID_DM_EXPLODE_VIEW );


MObject DMExplodeView::inDM;

MObject DMExplodeView::aEnable;
MObject DMExplodeView::aScale;
MObject DMExplodeView::aCenter;
MObject DMExplodeView::aPieceCenterAttributeName;

MObject DMExplodeView::aVectorType;
MObject DMExplodeView::aVectorAttributeName;

MObject DMExplodeView::aPieceGroup;

MObject DMExplodeView::outDM;



void *DMExplodeView::creator()
{
    return ::dommy::maya::createObject("DMExplodeView", [&]() {
        return new DMExplodeView();
    });
}






MStatus DMExplodeView::compute(const MPlug& plug, MDataBlock& data) try
{
    return dommy::maya::computeExplodeNode<DMExplodeView, DMeshData, true>(
        plug, data, inDM, outDM);
}
DOMMY_COMPUTE_CATCH("DMExplodeView")



MStatus DMExplodeView::initialize() try {
    MStatus stat;
    MFnNumericAttribute nAttr;
    MFnGenericAttribute gAttr;
    MFnTypedAttribute tAttr;
    MFnStringData fnStringData;
    inDM = tAttr.create("inDM", "idm", DMeshData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inDM));

    aEnable = nAttr.create( "Enable", "enbl", MFnNumericData::kBoolean, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(true));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aEnable));

    aScale = nAttr.create( "Scale", "scl", MFnNumericData::kFloat, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(5.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aScale));

    aCenter = nAttr.create( "Center", "ctr", MFnNumericData::k3Float, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0,0.0,0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aCenter));

    const MObject aPieceCenterAttributeNameDefault = fnStringData.create("center", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aPieceCenterAttributeName = tAttr.create("PieceCenterAttributeName", "pccattrnm", MFnData::kString, aPieceCenterAttributeNameDefault, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aPieceCenterAttributeName));

    MFnEnumAttribute opEnumFn;
    aVectorType = opEnumFn.create( "VectorType", "vty", 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(opEnumFn.addField( "Center to piece", 0 ));
    DOMMY_RETURN_IF_FAILURE(opEnumFn.addField( "From attribute", 1 ));
    DOMMY_RETURN_IF_FAILURE(opEnumFn.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aVectorType));

    const MObject aVectorAttributeNameDefault = fnStringData.create("", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aVectorAttributeName = tAttr.create("VectorAttributeName", "vattrnm", MFnData::kString, aVectorAttributeNameDefault, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aVectorAttributeName));

    const MObject aPieceGroupDefault = fnStringData.create("piece.*", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aPieceGroup = tAttr.create("PieceGroup", "pcgrp", MFnData::kString, aPieceGroupDefault, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aPieceGroup));
    outDM = tAttr.create("outDM", "odm", DMeshData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(outDM));

    DOMMY_RETURN_IF_FAILURE(attributeAffects( inDM, outDM ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aEnable, outDM ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aScale, outDM ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aCenter, outDM ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aPieceCenterAttributeName, outDM ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aVectorType, outDM ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aVectorAttributeName, outDM ));

    DOMMY_RETURN_IF_FAILURE(attributeAffects( aPieceGroup, outDM ));

    return MS::kSuccess;
}
catch (...) {
    return dommy::maya::reportEntryPointException("DMExplodeView", "initialize", std::current_exception());
}
