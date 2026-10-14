//
//  DPnPopCommon.cpp
//  dommyTool
//
//  Created by yuzo on 2018/02/21.
//

#include "DPnPopCommon.h"
#include "common/utility/DMayaEntryPointGuard.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DGenerationLimits.h"


#include "DPnPopBase.h"


MStatus runPopOperation(const MObject& node, const MObject& inPopObj) try
{
    const MPlug popPlug(node, inPopObj);
    MStatus status;
    const bool connected = popPlug.isConnected(&status);
    DOMMY_RETURN_IF_FAILURE(status);
    if (!connected) return MS::kSuccess;

    MPlugArray popConnections;
    popPlug.connectedTo(popConnections, true, true, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const unsigned int count = popConnections.length();
    if (count > dommy::generation::kMaxConnectedGeometryCount)
        return MS::kInvalidParameter;

    for (unsigned int j = 0U; j < count; ++j) {
        const MObject target = popConnections[j].node(&status);
        DOMMY_RETURN_IF_FAILURE(status);
        MFnDependencyNode fnNode(target, &status);
        DOMMY_RETURN_IF_FAILURE(status);
        MPxNode* const userNode = fnNode.userNode(&status);
        DOMMY_RETURN_IF_FAILURE(status);
        DPnPopBase* const popNode = dynamic_cast<DPnPopBase*>(userNode);
        if (popNode) {
            DOMMY_RETURN_IF_FAILURE(popNode->runProcess());
        }
    }
    return MS::kSuccess;
} catch (...) {
    return dommy::maya::reportEntryPointException(
        "DPnPopCommon", "runPopOperation", std::current_exception());
}
