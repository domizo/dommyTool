//
//  DDataUtility.hpp
//  dommyTool
//
//  Created by yuzo on 2017/08/26.
//

#pragma once


#include <maya/MDataBlock.h>
#include <maya/MDataHandle.h>

#include "DPAttributeCommon.h"
#include "DMeshData.h"
#include "DCurveData.h"
#include "DPData.h"




// Attribute methods are intentionally limited to the two data models that
// define the required primitive/attribute contract. Unsupported data types
// fail at compile time instead of turning into a silent no-op.
void checkDDataPointAttribute(const std::string attrName,
                              DCurveData& outputDData,
                              bool delAttr);
void checkDDataPointAttribute(const std::string attrName,
                              DMeshData& outputDData,
                              bool delAttr);
template <typename T>
void checkDDataPointAttribute(const std::string, T&, bool) = delete;

void checkDDataAttribute(const std::string attrName,
                         DCurveData& outputDData,
                         bool delAttr);
void checkDDataAttribute(const std::string attrName,
                         DMeshData& outputDData,
                         bool delAttr);
template <typename T>
void checkDDataAttribute(const std::string, T&, bool) = delete;
