//
//  DFileOperation.h
//  dommyTool
//

#ifndef DFileOperation_h
#define DFileOperation_h

#include <maya/MString.h>

/// Resolve legacy $F, $F<n>, and $FF sequence tokens without narrowing the
/// evaluation time. The output is unchanged when the source has no token.
bool getSequenceFileName(
    const MString& sourceFileName, double frame, MString& outputFileName);

#endif /* DFileOperation_h */
