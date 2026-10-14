#pragma once

#include "PluginFeatures.h"

#include <ai.h>
#include <maya/MStatus.h>

#include <string>

namespace dommy {
namespace arnold {

// Keep the Arnold 5-era API and its integer success code. A boolean test would
// invert the result: successful ASS writes return AI_SUCCESS (zero).
inline MStatus writeAssFile(const std::string& filename, bool binary,
                            const char* ownerName)
{
#if DOMMY_WITH_DG_FILE_WRITERS
    const int result = AiASSWrite(filename.c_str(), AI_NODE_ALL, false, binary);
    if (result != AI_SUCCESS) {
        AiMsgError("[dommyTool] %s could not write the ASS file: %s (code %d)",
                   ownerName, filename.c_str(), result);
        return MS::kFailure;
    }
    return MS::kSuccess;
#else
    (void)filename;
    (void)binary;
    AiMsgError("[dommyTool] %s: ASS export is disabled by this build profile.",
               ownerName);
    return MS::kFailure;
#endif
}

} // namespace arnold
} // namespace dommy
