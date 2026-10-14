//
//  DDeformMath.h
//  dommyTool
//
//  Created by yuzo on 2018/07/03.
//


#pragma once

#include "DMayaHeaders.h"

bool deformBend(double factor, MVector* point);
// On false, discard the private CV array: a failed final Maya copy may have
// partially modified it. Exceptions propagate to the node's compute boundary.
bool deformBendWithCvs(
    double angle,
    double weight,
    const MDoubleArray& weights,
    MPointArray& cvs);
