#pragma once

#include <maya/MFnDependencyNode.h>
#include <maya/MPxNode.h>
#include <maya/MStatus.h>
#include <maya/MTypeId.h>

namespace dommy {
namespace maya {

// MFnDependencyNode::userNode() may legitimately return null while a node is
// being created or destroyed. Check both Maya status and the registered type
// before narrowing the pointer to a concrete MPxNode implementation.
template <typename NodeType>
NodeType* checkedUserNode(const MFnDependencyNode& functionSet,
                          const MTypeId& expectedTypeId,
                          MStatus* returnStatus = NULL)
{
    MStatus status;
    if (functionSet.typeId(&status) != expectedTypeId || status != MS::kSuccess) {
        if (returnStatus != NULL) {
            *returnStatus = status == MS::kSuccess ? MS::kInvalidParameter : status;
        }
        return NULL;
    }

    MPxNode* node = functionSet.userNode(&status);
    if (status != MS::kSuccess || node == NULL) {
        if (returnStatus != NULL) {
            *returnStatus = status == MS::kSuccess ? MS::kFailure : status;
        }
        return NULL;
    }

    if (returnStatus != NULL) {
        *returnStatus = MS::kSuccess;
    }
    return static_cast<NodeType*>(node);
}

} // namespace maya
} // namespace dommy
