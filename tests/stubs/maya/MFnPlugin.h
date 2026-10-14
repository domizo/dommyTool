#pragma once

#include <string>
#include <vector>

#include "MStatus.h"
#include "MString.h"
#include "MTypeId.h"

namespace maya_stub {
inline std::vector<std::string>& pluginEvents()
{
    static std::vector<std::string> events;
    return events;
}
}

class MFnPlugin
{
public:
    template<typename Name, typename... Args>
    MStatus registerNode(const Name&, const MTypeId&, Args&&...)
    {
        maya_stub::pluginEvents().push_back("registerNode");
        return MS::kSuccess;
    }

    MStatus deregisterNode(const MTypeId&)
    {
        maya_stub::pluginEvents().push_back("deregisterNode");
        return MS::kSuccess;
    }

    template<typename... Args>
    MStatus registerCommand(const MString&, Args&&...)
    {
        maya_stub::pluginEvents().push_back("registerCommand");
        return MS::kSuccess;
    }

    MStatus deregisterCommand(const MString&)
    {
        maya_stub::pluginEvents().push_back("deregisterCommand");
        return MS::kSuccess;
    }

    template<typename... Args>
    MStatus registerData(const MString&, const MTypeId&, Args&&...)
    {
        maya_stub::pluginEvents().push_back("registerData");
        return MS::kSuccess;
    }

    MStatus deregisterData(const MTypeId&)
    {
        maya_stub::pluginEvents().push_back("deregisterData");
        return MS::kSuccess;
    }
};
