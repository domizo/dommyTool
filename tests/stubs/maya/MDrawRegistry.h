#pragma once

#include "MFnPlugin.h"

namespace MHWRender {
class MDrawRegistry
{
public:
    template<typename Creator>
    static MStatus registerGeometryOverrideCreator(const MString&,
                                                   const MString&,
                                                   Creator)
    {
        maya_stub::pluginEvents().push_back("registerGeometryOverride");
        return MS::kSuccess;
    }

    static MStatus deregisterGeometryOverrideCreator(const MString&, const MString&)
    {
        maya_stub::pluginEvents().push_back("deregisterGeometryOverride");
        return MS::kSuccess;
    }

    template<typename Creator>
    static MStatus registerDrawOverrideCreator(const MString&,
                                               const MString&,
                                               Creator)
    {
        maya_stub::pluginEvents().push_back("registerDrawOverride");
        return MS::kSuccess;
    }

    static MStatus deregisterDrawOverrideCreator(const MString&, const MString&)
    {
        maya_stub::pluginEvents().push_back("deregisterDrawOverride");
        return MS::kSuccess;
    }
};
}
