// Native SDK value arithmetic only; does not link or start Maya.
#include "common/utility/DMayaPointBlend.h"
#include "maya_point_blend_cases.h"
#include <iostream>

int main()
{
    const unsigned cases = checkMayaPointBlendCases();
    std::cout << "Native SDK MPoint: " << cases
              << " production point-blend cases passed; no Maya host runtime\n";
}
