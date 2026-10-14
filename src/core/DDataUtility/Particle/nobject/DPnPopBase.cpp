//
//  DPnPopBase.cpp
//  dommyTool
//
//  Created by yuzo on 2018/02/21.
//

#include "DPnPopBase.h"
#include "common/utility/DComputeGuard.h"



#include "DMayaDebug.h"

MObject DPnPopBase::inPop;
MObject DPnPopBase::outPop;


MStatus DPnPopBase::getParam() {
    return MS::kSuccess;
}


MStatus DPnPopBase::runProcess() {
    
    return MS::kUnknownParameter;
}

MStatus DPnPopBase::compute(const MPlug& plug, MDataBlock&) try
{
    if (plug != outPop) return MS::kUnknownParameter;

    MStatus status = getParam();
    if (!status) return status;
    return runProcess();
}
DOMMY_COMPUTE_CATCH("DPnPopBase")

