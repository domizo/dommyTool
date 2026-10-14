#pragma once
#include <maya/MStatus.h>

// Shared edit operation, usable by a DG node or a direct command.
class basePolyModifierFty {
public:
    basePolyModifierFty();
    virtual ~basePolyModifierFty();
    virtual MStatus doIt() = 0;
};
