//
//  DBBoxOp.hpp
//  dommyTool
//
//  Created by yuzo on 2017/07/06.
//

#pragma once

#include <maya/MBoundingBox.h>
#include <maya/MIntArray.h>
#include <maya/MPoint.h>
#include <maya/MPointArray.h>
#include <maya/MVectorArray.h>

#include "DType.h"


void CalBBoxFromPointArray( const DPointList &pts, MBoundingBox &bbox, DPoint &center);
void CalBBoxFromPointArray(const DPointList& pts, MBoundingBox& bbox);
void CalBBoxFromMPointArray( const MVectorArray &pts, MBoundingBox &bbox);

MStatus DGenerateDisplayBBoxDataFromMinMaxPoint(const MPoint &minPt, const MPoint &maxPt, MPointArray &pts, MIntArray &connectArray);
