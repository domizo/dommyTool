//
//  DPAddPoint.cpp
//  dommyTool
//
//  Created by yuzo on 2016/11/19.
//
//

#include "common/utility/DMayaEntryPointGuard.h"
#include "DPAddPoint.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DGenerationLimits.h"
#include "common/utility/DMayaPluginDataOutput.h"


#include "DMayaHeaders.h"


#include "DPData.h"
#include "dpoint_ids.h"

#include <cmath>

const MTypeId DPAddPoint::typeId( ID_DP_ADD_POINT );


MObject DPAddPoint::aPoint;

MObject DPAddPoint::outDP;


void *DPAddPoint::creator() {
    return ::dommy::maya::createObject("DPAddPoint", [&]() {
        return new DPAddPoint();
    });
}

MStatus DPAddPoint::compute( const MPlug& plug, MDataBlock& data ) try
{
    MStatus stat;
    

    
    if( plug == outDP )
    {
        
        DPointList newPtArray;
        
        //アトリビュート値の取得
        MArrayDataHandle arrayHandle = data.inputArrayValue(aPoint, &stat);
        CHECK_MSTATUS_AND_RETURN_IT(stat);
        const unsigned int count = arrayHandle.elementCount(&stat);
        CHECK_MSTATUS_AND_RETURN_IT(stat);
        if (static_cast<std::size_t>(count) >
            dommy::generation::kMaxGeneratedPointCount) {
            return MS::kInvalidParameter;
        }
        newPtArray.reserve(static_cast<std::size_t>(count));
        
        
        for(unsigned int i = 0; i < count; ++i) {
            stat = arrayHandle.jumpToArrayElement(i);
            CHECK_MSTATUS_AND_RETURN_IT(stat);
            const MDataHandle pointHandle =
                arrayHandle.inputValue(&stat);
            CHECK_MSTATUS_AND_RETURN_IT(stat);
            const float3 &pt = pointHandle.asFloat3();
            if (!std::isfinite(pt[0]) ||
                !std::isfinite(pt[1]) ||
                !std::isfinite(pt[2])) {
                return MS::kInvalidParameter;
            }
            
            newPtArray.push_back(DPoint(pt[0],pt[1],pt[2]));
        }
        
        
        ///output data
        stat = dommy::maya::setPluginDataOutput<DPData>(
            data,
            outDP,
            DPData::id,
            [&newPtArray](DPData& output) {
                output.setPosition(newPtArray);
                output.updateBBox();
            });
        CHECK_MSTATUS_AND_RETURN_IT(stat);
        return data.setClean(plug);
    }
    
    return MS::kUnknownParameter;
}
DOMMY_COMPUTE_CATCH("DPAddPoint")

MStatus DPAddPoint::initialize() try {
    MStatus stat;
    MFnNumericAttribute nAttr;
    MFnGenericAttribute gAttr;
    MFnTypedAttribute tAttr;

    aPoint = nAttr.create("Point", "pt", MFnNumericData::k3Float, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0, 0.0, 0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setArray(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aPoint));

    // create custom data
    outDP = tAttr.create("outDP", "odp", DPData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(outDP));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(aPoint, outDP));

    return MS::kSuccess;
} catch (...) {
    return dommy::maya::reportEntryPointException("DPAddPoint", "initialize",
                                                  std::current_exception());
}
