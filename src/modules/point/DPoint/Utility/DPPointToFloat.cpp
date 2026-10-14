//
//  DPPointToFloat.cpp
//  dommyTool
//
//  Created by yuzo on 3/11/16.
//
//

#include "common/utility/DMayaEntryPointGuard.h"
#include "DPPointToFloat.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DNumericValidation.h"

#include "DPointCommon.h"
#include "dpoint_ids.h"

#include <maya/MFnNumericAttribute.h>
#include <maya/MFnGenericAttribute.h>
#include <maya/MFnTypedAttribute.h>
#include <maya/MFnEnumAttribute.h>

#include <maya/MFnDependencyNode.h>

#include <maya/MFnPointArrayData.h>
#include <maya/MFnVectorArrayData.h>
#include <maya/MFnFloatArrayData.h>

#include <maya/MColor.h>

#include <maya/MPlug.h>
#include <maya/MGlobal.h>

#include <cmath>

const MTypeId DPPointToFloat::typeId(ID_DP_TO_FLOAT);

MObject DPPointToFloat::inputPointArray;
MObject DPPointToFloat::convertType;

MObject DPPointToFloat::outDataArray;

void* DPPointToFloat::creator() {
    return ::dommy::maya::createObject("DPPointToFloat", [&]() {
        return new DPPointToFloat();
    });
}

MStatus DPPointToFloat::compute(const MPlug& plug, MDataBlock& block) try {
    MStatus stat;

    if (plug == outDataArray) {

        // get Attribute
        const MDataHandle convertTypeHandle = block.inputValue(convertType, &stat);
        CHECK_MSTATUS_AND_RETURN_IT(stat);
        const DP_PTF_TYPE convertTypeVal = convertTypeHandle.asShort();

        MPointArray pts;

        DOMMY_RETURN_IF_FAILURE(getPointArrayFromData(block, inputPointArray, pts));

        if (!pts.length()) {
            MGlobal::displayError("DPPointToFloat: input point array is empty.");
            return MS::kFailure;
        }

        MFloatArray newDataArray;

        switch (convertTypeVal) {
        case DP_PTF_MAGNITUDE: {
            for (unsigned long i = 0; i < pts.length(); i++) {
                float value = 0.0f;
                if (!dommy::numeric::checkedFloatCast(MVector(pts[i]).length(), value)) {
                    return MS::kInvalidParameter;
                }
                DOMMY_RETURN_IF_FAILURE(newDataArray.append(value));
            }
        } break;
        case DP_PTF_LUMINANCE: {

            for (unsigned long i = 0; i < pts.length(); i++) {
                if (!std::isfinite(pts[i].x) || !std::isfinite(pts[i].y) ||
                    !std::isfinite(pts[i].z)) {
                    return MS::kInvalidParameter;
                }
                float red = 0.0f, green = 0.0f, blue = 0.0f;
                if (!dommy::numeric::checkedFloatCast(pts[i].x, red) ||
                    !dommy::numeric::checkedFloatCast(pts[i].y, green) ||
                    !dommy::numeric::checkedFloatCast(pts[i].z, blue))
                    return MS::kInvalidParameter;
                MColor color(red, green, blue);
                float hVal, sVal, vVal;
                if (!color.get(MColor::kHSV, hVal, sVal, vVal))
                    return MS::kFailure;

                if (!std::isfinite(vVal)) {
                    return MS::kInvalidParameter;
                }
                DOMMY_RETURN_IF_FAILURE(newDataArray.append(vVal));
            }
        } break;
        default: {
            for (unsigned long i = 0; i < pts.length(); i++) {
                DOMMY_RETURN_IF_FAILURE(newDataArray.append(0.0f));
            }
        } break;
        }

        //        cout << "DPPointToFloat: size " << newDataArray.length() << endl;

#pragma mark Create output point array

        MDataHandle outputData = block.outputValue(outDataArray, &stat);
        CHECK_MSTATUS_AND_RETURN_IT(stat);

        MFnFloatArrayData uData;
        MObject outplug = uData.create(newDataArray, &stat);
        CHECK_MSTATUS_AND_RETURN_IT(stat);

        DOMMY_RETURN_IF_FAILURE(outputData.set(outplug));

        outputData.setClean();
        stat = block.setClean(plug);

        if (!stat) {
            stat.perror("DPPointToFloat::compute setClean");
            return stat;
        }
    } else {
        return MS::kUnknownParameter;
    }

    return stat;
}
DOMMY_COMPUTE_CATCH("DPPointToFloat")

MStatus DPPointToFloat::initialize() {
    MStatus stat;
    MFnNumericAttribute nAttr;
    MFnGenericAttribute gAttr;
    MFnTypedAttribute tAttr;

    MFnGenericAttribute outAttr;

    inputPointArray = gAttr.create("inputPointArray", "ipa", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(gAttr.addDataAccept(MFnData::kPointArray));
    DOMMY_RETURN_IF_FAILURE(gAttr.addDataAccept(MFnData::kVectorArray));
    DOMMY_RETURN_IF_FAILURE(gAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(gAttr.setWritable(true));
    DOMMY_RETURN_IF_FAILURE(gAttr.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inputPointArray));

    MFnEnumAttribute enumFn;
    convertType = enumFn.create("convertType", "cvt", 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(enumFn.addField("magunitude", DP_PTF_MAGNITUDE));
    DOMMY_RETURN_IF_FAILURE(enumFn.addField("luminance", DP_PTF_LUMINANCE));
    DOMMY_RETURN_IF_FAILURE(enumFn.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(enumFn.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(convertType));

    outDataArray =
        tAttr.create("outDataArray", "out", MFnData::kFloatArray, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(outDataArray));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(inputPointArray, outDataArray));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(convertType, outDataArray));

    return MS::kSuccess;
}
