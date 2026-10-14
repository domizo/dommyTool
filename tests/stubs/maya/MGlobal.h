#pragma once

#include "MStatus.h"
#include "MString.h"
#include "MTime.h"

class MGlobal
{
public:
    static void displayError(const MString&) {}
    static void displayWarning(const MString&) {}

    static MStatus viewFrame(const MTime& time)
    {
        MTime::currentValue() = time.value();
        return MS::kSuccess;
    }
};
