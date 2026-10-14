#pragma once

#include "MFnPlugin.h"
#include "MStatus.h"

typedef unsigned long MCallbackId;

class MMessage
{
public:
    static MStatus removeCallback(MCallbackId)
    {
        maya_stub::pluginEvents().push_back("removeCallback");
        return MS::kSuccess;
    }
};
