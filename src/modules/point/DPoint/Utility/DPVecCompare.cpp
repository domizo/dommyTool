//
//  DPCompare.cpp
//  dommyTool
//
//  Created by yuzo on 3/2/16.
//
//

#include "common/utility/DMayaEntryPointGuard.h"
#include "DPVecCompare.h"
#include "common/utility/DComputeGuard.h"

#include "DPointCommon.h"
#include "dpoint_ids.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DMayaArrayOutput.h"
#include "common/utility/DMayaValueValidation.h"
#include "DMayaHeaders.h"

const MTypeId DPVecCompare::typeId(ID_DP_COMPARE);

MObject DPVecCompare::inputArray1;
MObject DPVecCompare::inputArray2;

MObject DPVecCompare::compareType;

MObject DPVecCompare::value;
MObject DPVecCompare::valueX;
MObject DPVecCompare::valueY;
MObject DPVecCompare::valueZ;

MObject DPVecCompare::resultArray;

void* DPVecCompare::creator() {
    return ::dommy::maya::createObject("DPVecCompare", [&]() {
        return new DPVecCompare();
    });
}

MStatus DPVecCompare::compute(const MPlug& plug, MDataBlock& block) try {
    MStatus stat;

    if (plug == resultArray) {

        // attribute
        const MDataHandle compareHandle = block.inputValue(compareType, &stat);
        if (!stat)
            return stat;
        const DP_VEC_COMPARE_TYPE compTypeVal = compareHandle.asInt();
        if (compTypeVal < DP_V_COMP_NO_OP || compTypeVal >= DP_VEC_COMPARE_TYPE_COUNT)
            return MS::kInvalidParameter;

        MVectorArray inArray1, inArray2;

        DOMMY_RETURN_IF_FAILURE(getVectorArrayFromData(block, inputArray1, inArray1));

        MPlug inArray2Plug(thisMObject(), inputArray2);

        const bool secondConnected = inArray2Plug.isConnected(&stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        if (secondConnected) {
            DOMMY_RETURN_IF_FAILURE(getVectorArrayFromData(block, inputArray2, inArray2));
        }

        if (inArray1.length() == 0) {
            //			cout << "inputArray1 is zero length array." << endl;
            return MS::kUnknownParameter;
        }

        //		cout << "array1 size:" << inArray1.length() << "array2 size:" << inArray2.length()
        //<< endl;

        // create new array
        MVectorArray newVecArray;
        if (inArray2.length() != 0U && inArray1.length() != inArray2.length())
            return MS::kInvalidParameter;
        const auto validValue = [compTypeVal](const MVector& value) {
            if (!dommy::maya::isFiniteVector(value))
                return false;
            if (compTypeVal == DP_V_COMP_LUM_MIN || compTypeVal == DP_V_COMP_LUM_MAX)
                return dommy::maya::isFloatRepresentableVector(value);
            return compTypeVal == DP_V_COMP_NO_OP || std::isfinite(value.length());
        };
        for (unsigned int index = 0U; index < inArray1.length(); ++index) {
            if (!validValue(inArray1[index]) ||
                (inArray2.length() != 0U && !validValue(inArray2[index])))
                return MS::kInvalidParameter;
        }

        if (inArray2.length() == 0) {
            const MDataHandle valueHandle = block.inputValue(value, &stat);
            if (!stat)
                return stat;
            const double3& valueVal = valueHandle.asDouble3();

            MVector valueVec(valueVal[0], valueVal[1], valueVal[2]);
            if (!validValue(valueVec))
                return MS::kInvalidParameter;

            switch (compTypeVal) {
            case DP_V_COMP_MAG_MIN:
            case DP_V_COMP_MAG_MAX: {
                for (long i = 0; i < inArray1.length(); i++) {

                    if (compTypeVal == DP_V_COMP_MAG_MIN) {
                        DOMMY_RETURN_IF_FAILURE(newVecArray.append(
                            valueVec.length() < inArray1[i].length() ? valueVec : inArray1[i]));
                    } else {
                        DOMMY_RETURN_IF_FAILURE(newVecArray.append(
                            valueVec.length() > inArray1[i].length() ? valueVec : inArray1[i]));
                    }
                }
            } break;
            case DP_V_COMP_LUM_MIN:
            case DP_V_COMP_LUM_MAX: {
                MColor valColor(valueVec.x, valueVec.y, valueVec.z);
                float hVal, sVal, vVal;
                if (!valColor.get(MColor::kHSV, hVal, sVal, vVal) || !std::isfinite(vVal))
                    return MS::kInvalidParameter;

                for (long i = 0; i < inArray1.length(); i++) {
                    MColor aColor(inArray1[i].x, inArray1[i].y, inArray1[i].z);
                    float ahVal, asVal, avVal;
                    if (!aColor.get(MColor::kHSV, ahVal, asVal, avVal) || !std::isfinite(avVal))
                        return MS::kInvalidParameter;

                    if (compTypeVal == DP_V_COMP_LUM_MIN) {
                        DOMMY_RETURN_IF_FAILURE(
                            newVecArray.append(vVal < avVal ? valueVec : inArray1[i]));
                    } else {
                        DOMMY_RETURN_IF_FAILURE(
                            newVecArray.append(vVal > avVal ? valueVec : inArray1[i]));
                    }
                }
            } break;
            default: {
                for (long i = 0; i < inArray1.length(); i++) {
                    DOMMY_RETURN_IF_FAILURE(newVecArray.append(inArray1[i]));
                }
            } break;
            }

        } else {
            if (inArray1.length() == inArray2.length()) {

                switch (compTypeVal) {
                case DP_V_COMP_MAG_MIN:
                case DP_V_COMP_MAG_MAX: {
                    for (long i = 0; i < inArray1.length(); i++) {
                        if (compTypeVal == DP_V_COMP_MAG_MIN) {
                            DOMMY_RETURN_IF_FAILURE(newVecArray.append(
                                inArray1[i].length() < inArray2[i].length() ? inArray1[i]
                                                                            : inArray2[i]));
                        } else {
                            DOMMY_RETURN_IF_FAILURE(newVecArray.append(
                                inArray1[i].length() > inArray2[i].length() ? inArray1[i]
                                                                            : inArray2[i]));
                        }
                    }
                } break;
                case DP_V_COMP_LUM_MIN:
                case DP_V_COMP_LUM_MAX: {

                    for (long i = 0; i < inArray1.length(); i++) {
                        MColor aColor(inArray1[i].x, inArray1[i].y, inArray1[i].z);
                        float hVal1, sVal1, vVal1;
                        if (!aColor.get(MColor::kHSV, hVal1, sVal1, vVal1) || !std::isfinite(vVal1))
                            return MS::kInvalidParameter;

                        MColor bColor(inArray2[i].x, inArray2[i].y, inArray2[i].z);
                        float hVal2, sVal2, vVal2;
                        if (!bColor.get(MColor::kHSV, hVal2, sVal2, vVal2) || !std::isfinite(vVal2))
                            return MS::kInvalidParameter;

                        if (compTypeVal == DP_V_COMP_LUM_MIN) {
                            DOMMY_RETURN_IF_FAILURE(
                                newVecArray.append(vVal1 < vVal2 ? inArray1[i] : inArray2[i]));
                        } else {
                            DOMMY_RETURN_IF_FAILURE(
                                newVecArray.append(vVal1 > vVal2 ? inArray1[i] : inArray2[i]));
                        }
                    }
                } break;
                default: {
                    for (long i = 0; i < inArray1.length(); i++) {
                        DOMMY_RETURN_IF_FAILURE(newVecArray.append(inArray1[i]));
                    }
                } break;
                }
            }
        }

        if (newVecArray.length() != inArray1.length())
            return MS::kFailure;
        return DOutputDataMayaVectorArray(block, plug, resultArray, newVecArray);
    } else {
        return MS::kUnknownParameter;
    }

    return stat;
}
DOMMY_COMPUTE_CATCH("DPVecCompare")

MStatus DPVecCompare::initialize() {
    MStatus stat;
    MFnNumericAttribute nAttr;

    MFnGenericAttribute inarray1Attr;
    inputArray1 = inarray1Attr.create("inputArray1", "in1", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(inarray1Attr.addDataAccept(MFnData::kPointArray));
    DOMMY_RETURN_IF_FAILURE(inarray1Attr.addDataAccept(MFnData::kVectorArray));
    DOMMY_RETURN_IF_FAILURE(inarray1Attr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(inarray1Attr.setWritable(true));
    DOMMY_RETURN_IF_FAILURE(inarray1Attr.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inputArray1));

    MFnGenericAttribute inarray2Attr;
    inputArray2 = inarray2Attr.create("inputArray2", "in2", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(inarray2Attr.addDataAccept(MFnData::kPointArray));
    DOMMY_RETURN_IF_FAILURE(inarray2Attr.addDataAccept(MFnData::kVectorArray));
    DOMMY_RETURN_IF_FAILURE(inarray2Attr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(inarray2Attr.setWritable(true));
    DOMMY_RETURN_IF_FAILURE(inarray2Attr.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inputArray2));

    MFnEnumAttribute enumFn;
    compareType = enumFn.create("compareType", "ct", 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(enumFn.addField("No_Operation", DP_V_COMP_NO_OP));
    DOMMY_RETURN_IF_FAILURE(enumFn.addField("Magnitude Min", DP_V_COMP_MAG_MIN));
    DOMMY_RETURN_IF_FAILURE(enumFn.addField("Magnitude Max", DP_V_COMP_MAG_MAX));
    DOMMY_RETURN_IF_FAILURE(enumFn.addField("Luminance Min", DP_V_COMP_LUM_MIN));
    DOMMY_RETURN_IF_FAILURE(enumFn.addField("Luminance Max", DP_V_COMP_LUM_MAX));
    DOMMY_RETURN_IF_FAILURE(enumFn.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(enumFn.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(compareType));

    valueX = nAttr.create("valueX", "valx", MFnNumericData::kDouble, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    valueY = nAttr.create("valueY", "valy", MFnNumericData::kDouble, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    valueZ = nAttr.create("valueZ", "valz", MFnNumericData::kDouble, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));

    value = nAttr.create("value", "val", valueX, valueY, valueZ, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0, 0.0, 0.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(value));

    MFnGenericAttribute outAttr;
    resultArray = outAttr.create("resultArray", "ra", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(outAttr.addDataAccept(MFnData::kPointArray));
    DOMMY_RETURN_IF_FAILURE(outAttr.addDataAccept(MFnData::kVectorArray));
    DOMMY_RETURN_IF_FAILURE(outAttr.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(outAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(resultArray));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(inputArray1, resultArray));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(inputArray2, resultArray));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(compareType, resultArray));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(value, resultArray));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(valueX, resultArray));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(valueY, resultArray));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(valueZ, resultArray));

    return MS::kSuccess;
}
