#pragma once

#include "DPAttributeData.h"

#include <string>

bool isSingleValueAttrType(const std::string& attrTypeName);
bool isSingleValueAttrType(const DP_ATTR_DATA_TYPE& type);
bool isIntSingleValueAttrType(const DP_ATTR_DATA_TYPE& type);
bool isSingleRealAttrType(const std::string& attrTypeName);
bool isSingleRealAttrType(const DP_ATTR_DATA_TYPE& type);
bool isVector2DAttrType(const std::string& attrTypeName);
bool isVector2DAttrType(const DP_ATTR_DATA_TYPE& type);
bool isVector3DAttrType(const std::string& attrTypeName);
bool isVector3DAttrType(const DP_ATTR_DATA_TYPE& type);
bool isMatrixAttrType(const std::string& attrTypeName);
bool isMatrixAttrType(const DP_ATTR_DATA_TYPE& type);
bool isStringAttrType(const std::string& attrTypeName);
bool isStringAttrType(const DP_ATTR_DATA_TYPE& type);
