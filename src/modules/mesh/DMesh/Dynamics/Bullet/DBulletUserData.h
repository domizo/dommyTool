#pragma once

#include <cstddef>
#include <limits>
#include <string>

#include <maya/MUuid.h>

struct customDBData
{
    customDBData()
        : index(-1)
        , ptIndex(std::numeric_limits<std::size_t>::max())
    {
    }

    MUuid nodeUuid;
    std::string prefixName;
    int index;
    std::size_t ptIndex;
};
