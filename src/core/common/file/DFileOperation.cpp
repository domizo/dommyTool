//
//  DFileOperation.cpp
//  dommyTool
//

#include "DFileOperation.h"
#include "DSequencePath.h"

bool getSequenceFileName(
    const MString& sourceFileName, double frame, MString& outputFileName)
{
    std::string resolved;
    if (!dommy::file::replaceLegacyFrameToken(
            sourceFileName.asChar(), frame, resolved)) {
        outputFileName.clear();
        return false;
    }
    outputFileName = resolved.c_str();
    return true;
}
