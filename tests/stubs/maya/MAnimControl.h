#pragma once

#include "MTime.h"

class MAnimControl
{
public:
    static MTime currentTime()
    {
        return MTime(MTime::currentValue());
    }
};
