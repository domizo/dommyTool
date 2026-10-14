//
//  DDataUtility.cpp
//  dommyTool
//
//  Created by yuzo on 2017/08/26.
//

#include "DDataUtility.h"




void checkDDataPointAttribute(const std::string attrName,
                              DCurveData& outputDData,
                              bool delAttr) {
    
    for (auto cIt=outputDData.beginPrims(); cIt!=outputDData.endPrims(); ++cIt) {
        if ((*cIt)->isExistAttr(attrName) && delAttr) {
            (*cIt)->removeAttr(attrName);
        }
    }
    
}

void checkDDataPointAttribute(const std::string attrName,
                              DMeshData& outputDData,
                              bool delAttr) {
    
    for (auto cIt=outputDData.beginPrims(); cIt!=outputDData.endPrims(); ++cIt) {
        if ((*cIt)->isExistAttr(attrName) && delAttr) {
            (*cIt)->removeAttr(attrName);
        }
    }
    
}


void checkDDataAttribute(const std::string attrName,
                         DCurveData& outputDData,
                         bool delAttr) {
    if (outputDData.isExistAttr(attrName) && delAttr) {
        outputDData.removeAttr(attrName);
    }
}

void checkDDataAttribute(const std::string attrName,
                         DMeshData& outputDData,
                         bool delAttr) {
    if (outputDData.isExistAttr(attrName) && delAttr) {
        outputDData.removeAttr(attrName);
    }
}
