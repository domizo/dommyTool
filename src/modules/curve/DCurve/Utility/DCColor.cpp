//
//  DCColor.cpp
//  dommyTool
//
//  Created by yuzo on 2017/02/01.
//
//

#include "common/utility/DMayaEntryPointGuard.h"
#include "DCColor.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DMayaPluginDataOutput.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DColorMode.h"
#include "common/utility/DColorPalette.h"
#include "common/utility/DColorPublication.h"
#include "common/utility/DColorBounds.h"
#include "common/utility/DPointArrayConversion.h"
#include "common/utility/DMayaRampSampling.h"


#include "DMayaHeaders.h"
#include "DMayaDebug.h"


#include "DCurveDataHeaders.h"

#include "DGlobalTex.h"
#include "DNoiseOp.h"
#include "DRandomGenerator.h"

const MTypeId DCColor::typeId( ID_DC_COLOR );


MObject DCColor::inDC;

MObject DCColor::aAssigneType;
MObject DCColor::aColorType;
MObject DCColor::aColorName;
MObject DCColor::aUVAttrName;
//solid
MObject DCColor::aSolidColor;
//ramp
MObject DCColor::aRampColor;
//texture
MObject DCColor::aIs3DCalc;
MObject DCColor::aTexColor;

//random
MObject DCColor::aSeed;
MObject DCColor::aMinRange;
MObject DCColor::aMaxRange;

//expression
MObject DCColor::aExpression;

//palette
MObject DCColor::aPaletteType;
MObject DCColor::aPaletteColor;


MObject DCColor::outDC;




void *DCColor::creator()
{
    return ::dommy::maya::createObject("DCColor", [&]() {
        return new DCColor();
    });
}

MStatus DCColor::compute(const MPlug& plug, MDataBlock& data) try
{
    
    MStatus stat;
    
    if (plug == outDC) {

        const MDataHandle colorTypeHandle =
            data.inputValue(aColorType, &stat);
        CHECK_MSTATUS_AND_RETURN_IT(stat);
        const short colorAddTypeVal = colorTypeHandle.asShort();
        if (!dommy::color::isValidMode(colorAddTypeVal)) {
            MGlobal::displayError("DCColor: unsupported ColorType value.");
            return MS::kInvalidParameter;
        }
        if (colorAddTypeVal == dommy::color::kExpression) {
            MGlobal::displayError(
                "DCColor: Expression mode is retained for scene "
                "compatibility but is not implemented.");
            return MS::kInvalidParameter;
        }
        const MDataHandle assignmentHandle =
            data.inputValue(aAssigneType, &stat);
        CHECK_MSTATUS_AND_RETURN_IT(stat);
        const short assigneType = assignmentHandle.asShort();
        if (!dommy::color::isValidAssignment(assigneType)) {
            MGlobal::displayError(
                "DCColor: unsupported AssigneType value.");
            return MS::kInvalidParameter;
        }
        if (dommy::color::requiresPointAssignment(colorAddTypeVal) &&
            assigneType != dommy::color::kPoint) {
            MGlobal::displayError(
                "DCColor: the selected ColorType supports Point "
                "assignment only.");
            return MS::kInvalidParameter;
        }

        const MDataHandle colorNameHandle = data.inputValue(aColorName, &stat);
        CHECK_MSTATUS_AND_RETURN_IT(stat);
        const std::string colAttrName = colorNameHandle.asString().asChar();
        if (!dommy::attribute::validAttributeName(colAttrName) ||
            colAttrName.size() > dommy::generation::kMaxGeneratedStringByteCount) {
            MGlobal::displayError("DCColor: ColorName must be a valid attribute name.");
            return MS::kInvalidParameter;
        }

        
        
#pragma mark get input dcurve
        const DCurveData* inputDCData = nullptr;
        stat = getValidInputDC(inDC, data, inputDCData);
        if (!stat) return stat;
        
        if (inputDCData) {
            
            MFnPluginData pluginData;
            pluginData.create(DCurveData::id, &stat);
            CHECK_MSTATUS_AND_RETURN_IT(stat);
            
            
            DCurveData* const outDCrvs(dommy::maya::pluginDataPointer<DCurveData>(pluginData, &stat));
            
            if (outDCrvs) {
                
                
                DOMMY_RETURN_IF_FAILURE(outDCrvs->copyFrom(*inputDCData));
                
                
                uint curveCount = outDCrvs->numberOfCurves();
                
                if(curveCount!=0) {
                    
                    const MDataHandle uvNameHandle =
                        data.inputValue(aUVAttrName, &stat);
                    CHECK_MSTATUS_AND_RETURN_IT(stat);
                    const std::string uvAttrNameStr =
                        uvNameHandle.asString().asChar();
                    
                    // Keep existing fields available until each color result is complete.
                    
                    
                    if (colorAddTypeVal==dommy::color::kSolid) {
#pragma mark Solid Color
                        const MDataHandle solidColorHandle =
                            data.inputValue(aSolidColor, &stat);
                        CHECK_MSTATUS_AND_RETURN_IT(stat);
                        const MVector solidColorVal =
                            solidColorHandle.asFloatVector();
                        if (!dommy::color::isFiniteTriple(
                                solidColorVal[0], solidColorVal[1],
                                solidColorVal[2])) {
                            MGlobal::displayError(
                                "DCColor: SolidColor must contain finite values.");
                            return MS::kInvalidParameter;
                        }
                        
                        DVec3Data baseCol(solidColorVal[0],solidColorVal[1],solidColorVal[2]);
                        DVec3Data bgCol(0.0,0.0,0.0);
                        
                        if(assigneType==dommy::color::kPrimitive) {
                            
                            if (!dommy::color::publishAttribute(
                                    *outDCrvs, colAttrName,
                                    DPVec3fAttr(baseCol, bgCol, curveCount), curveCount))
                                return MS::kInvalidParameter;
                            
                        } else {
                            
                            for (auto cIt=outDCrvs->beginCurves(); cIt!=outDCrvs->endCurves(); ++cIt) {
                                if (!dommy::color::publishAttribute(
                                        *(*cIt), colAttrName,
                                        DPVec3fAttr(baseCol, bgCol, (*cIt)->numberOfPoints()),
                                        (*cIt)->numberOfPoints()))
                                    return MS::kInvalidParameter;
                            }
                            
                        }
                        
                        
                    } else if(colorAddTypeVal==dommy::color::kRandom) {//Random Color
#pragma mark Random Color
                        const MDataHandle seedHandle =
                            data.inputValue(aSeed, &stat);
                        CHECK_MSTATUS_AND_RETURN_IT(stat);
                        const int seedVal = seedHandle.asInt();
                        
                        const MDataHandle minRangeHandle =
                            data.inputValue(aMinRange, &stat);
                        CHECK_MSTATUS_AND_RETURN_IT(stat);
                        const MDataHandle maxRangeHandle =
                            data.inputValue(aMaxRange, &stat);
                        CHECK_MSTATUS_AND_RETURN_IT(stat);
                        const float3& minVal = minRangeHandle.asFloat3();
                        const float3& maxVal = maxRangeHandle.asFloat3();
                        if (!dommy::color::isOrderedFiniteRange(
                                minVal[0], minVal[1], minVal[2],
                                maxVal[0], maxVal[1], maxVal[2])) {
                            MGlobal::displayError(
                                "DCColor: Random color ranges must be finite and "
                                "MinRange must not exceed MaxRange.");
                            return MS::kInvalidParameter;
                        }
                        
                        if (assigneType==dommy::color::kPrimitive) {
                            DVec3DataList colorArray;
                            DGenerateRandomValue3(seedVal, minVal, maxVal, outDCrvs->numberOfCurves(), colorArray);
                            
                            if (!dommy::color::publishValues(
                                    *outDCrvs, colAttrName, colorArray, curveCount))
                                return MS::kInvalidParameter;
                            
                        } else {
                            size_t offsetRand = 0;
                            for (auto cIt=outDCrvs->beginCurves(); cIt!=outDCrvs->endCurves(); ++cIt) {
                                
                                DVec3DataList colorArray;
                                DGenerateRandomValue3(dommy::random::requireOffsetNonnegativeSeed(seedVal, offsetRand), minVal, maxVal, (*cIt)->numberOfPoints(), colorArray);
                                
                                if (!dommy::color::publishValues(
                                        *(*cIt), colAttrName, colorArray, (*cIt)->numberOfPoints()))
                                    return MS::kInvalidParameter;
                                
                                offsetRand++;
                            }
                            
                        }
                        
                    } else if(colorAddTypeVal==dommy::color::kRamp) {//Ramp Color
#pragma mark Ramp Color
                        
                        //Point only
                        for (auto cIt=outDCrvs->beginCurves();
                             cIt!=outDCrvs->endCurves(); ++cIt) {
                            if (!(*cIt)->isExistAttr(uvAttrNameStr)) {
                                MGlobal::displayError(
                                    "DCColor: Ramp UV attribute does not exist "
                                    "on every curve.");
                                return MS::kInvalidParameter;
                            }
                            const DP_ATTR_DATA_TYPE type =
                                (*cIt)->getAttr(uvAttrNameStr)->getAttrType();
                            if (!isSingleValueAttrType(type) &&
                                !isVector2DAttrType(type) &&
                                !isVector3DAttrType(type)) {
                                MGlobal::displayError(
                                    "DCColor: Ramp UV attribute has an "
                                    "unsupported type.");
                                return MS::kInvalidParameter;
                            }
                            MDoubleArray uArray;
                            getDCPAttributeMDoubleArray(
                                *cIt, uvAttrNameStr, uArray);
                            if (uArray.length() != (*cIt)->numberOfPoints()) {
                                MGlobal::displayError(
                                    "DCColor: Ramp UV attribute count does not "
                                    "match the curve point count.");
                                return MS::kInvalidParameter;
                            }
                        }

                        for (auto cIt=outDCrvs->beginCurves(); cIt!=outDCrvs->endCurves(); ++cIt) {
                            
                            if ((*cIt)->isExistAttr(uvAttrNameStr)) {
                                
                                MDoubleArray uArray;
                                
                                //UV データの取得
                                getDCPAttributeMDoubleArray(*cIt, uvAttrNameStr, uArray);
                                
                                uint curveCount = (*cIt)->numberOfPoints();
                                
                                if (uArray.length() == curveCount) {
                                    
                                    //ランプデータの取得
                                    MPlug colorRampPlug( thisMObject(), aRampColor );
                                    MRampAttribute colorRampPlugAttr( colorRampPlug );
                                    
                                    DVec3DataList newColArray(curveCount, DVec3Data(0,0,0));
                                    
                                    for (uint i=0; i<curveCount; i++) {
                                        MColor col;
                                        if (!dommy::maya::sampleFiniteColorRamp(
                                                colorRampPlugAttr, uArray[i], col)) {
                                            return MS::kFailure;
                                        }
                                        newColArray[i] = DVec3Data(col[0],col[1],col[2]);
                                    }
                                    
                                    if (!dommy::color::publishValues(
                                            *(*cIt), colAttrName, newColArray, (*cIt)->numberOfPoints()))
                                        return MS::kInvalidParameter;
                                    
                                    
                                } else {
                                    return MS::kInvalidParameter;
                                }
                                
                                
                            } else {
                                return MS::kInvalidParameter;
                            }
                            
                            
                            
                        }
                        
                    } else if(colorAddTypeVal==dommy::color::kTexture) {//Texture Color
#pragma mark Texture Color
                        const MDataHandle is3DHandle = data.inputValue(aIs3DCalc, &stat);
                        CHECK_MSTATUS_AND_RETURN_IT(stat);
                        const bool is3D = is3DHandle.asBool();
                        
                        //Point only
                        
                        {
                            for (auto cIt=outDCrvs->beginCurves();
                                 cIt!=outDCrvs->endCurves(); ++cIt) {
                                if (!(*cIt)->isExistAttr(uvAttrNameStr)) {
                                    MGlobal::displayError(
                                        "DCColor: Texture UV attribute does not "
                                        "exist on every curve.");
                                    return MS::kInvalidParameter;
                                }
                                const DP_ATTR_DATA_TYPE type =
                                    (*cIt)->getAttr(uvAttrNameStr)->getAttrType();
                                if (!isVector2DAttrType(type) &&
                                    !isVector3DAttrType(type)) {
                                    MGlobal::displayError(
                                        "DCColor: Texture UV attribute has an "
                                        "unsupported type.");
                                    return MS::kInvalidParameter;
                                }
                                MDoubleArray uArray;
                                MDoubleArray vArray;
                                if (type == DP_ATTR_VEC2I) {
                                    getDCPTwoArray<Imath::V2i>(
                                        *cIt, uvAttrNameStr, uArray, vArray);
                                } else if (type == DP_ATTR_VEC2F) {
                                    getDCPTwoArray<Imath::V2f>(
                                        *cIt, uvAttrNameStr, uArray, vArray);
                                } else if (type == DP_ATTR_VEC2D) {
                                    getDCPTwoArray<Imath::V2d>(
                                        *cIt, uvAttrNameStr, uArray, vArray);
                                } else if (type == DP_ATTR_VEC3I) {
                                    getDCPTwoArray<Imath::V3i>(
                                        *cIt, uvAttrNameStr, uArray, vArray);
                                } else if (type == DP_ATTR_VEC3F) {
                                    getDCPTwoArray<Imath::V3f>(
                                        *cIt, uvAttrNameStr, uArray, vArray);
                                } else {
                                    getDCPTwoArray<Imath::V3d>(
                                        *cIt, uvAttrNameStr, uArray, vArray);
                                }
                                const unsigned int pointCount =
                                    (*cIt)->numberOfPoints();
                                if (uArray.length() != pointCount ||
                                    vArray.length() != pointCount) {
                                    MGlobal::displayError(
                                        "DCColor: Texture UV attribute count does "
                                        "not match the curve point count.");
                                    return MS::kInvalidParameter;
                                }
                            }
                            
                            
                            for (auto cIt=outDCrvs->beginCurves(); cIt!=outDCrvs->endCurves(); ++cIt) {
                                
                                if ((*cIt)->isExistAttr(uvAttrNameStr)) {
                                    
                                    DP_ATTR_DATA_TYPE type = (*cIt)->getAttr(uvAttrNameStr)->getAttrType();
                                    
                                    MDoubleArray uArray,vArray;
                                    
                                    if (type == DP_ATTR_VEC2I) {
                                        getDCPTwoArray<Imath::V2i>(*cIt, uvAttrNameStr, uArray,vArray);
                                    } else if (type == DP_ATTR_VEC2F) {
                                        getDCPTwoArray<Imath::V2f>(*cIt, uvAttrNameStr, uArray,vArray);
                                    } else if (type == DP_ATTR_VEC2D) {
                                        getDCPTwoArray<Imath::V2d>(*cIt, uvAttrNameStr, uArray,vArray);
                                    } else if (type == DP_ATTR_VEC3I) {
                                        getDCPTwoArray<Imath::V3i>(*cIt, uvAttrNameStr, uArray,vArray);
                                    } else if (type == DP_ATTR_VEC3F) {
                                        getDCPTwoArray<Imath::V3f>(*cIt, uvAttrNameStr, uArray,vArray);
                                    }else if (type == DP_ATTR_VEC3D) {
                                        getDCPTwoArray<Imath::V3d>(*cIt, uvAttrNameStr, uArray,vArray);
                                    }else {
                                        return MS::kInvalidParameter;
                                    }
                                    
                                    uint curvePtCount = (*cIt)->numberOfPoints();
                                    
                                    if ((uArray.length() == curvePtCount) &&
                                        (vArray.length() == curvePtCount) ) {
                                        
                                        
                                        if (is3D) {
                                            
                                            MFloatPointArray points;
                                            DOMMY_RETURN_IF_FAILURE(
                                                dommy::geometry::copyPointsToMayaArray(
                                                    *(*cIt)->getPositionPtrConst(), points));
                                            
                                            
                                            MFloatVectorArray colorArray,alphaArray;
                                            stat = getColorFromShaderNetwork(thisMObject(), aTexColor, points, uArray, vArray, colorArray, alphaArray);
                                            CHECK_MSTATUS_AND_RETURN_IT(stat);
                                            if (colorArray.length() != curvePtCount) {
                                                return MS::kFailure;
                                            }
                                            
                                            //add color attribute
                                            
                                            
                                            DVec3DataList newColArray(colorArray.length(), DVec3Data(0,0,0));
                                            
                                            for (uint i=0; i<colorArray.length(); i++) {
                                                MVector col(colorArray[i]);
                                                newColArray[i] = DVec3Data(col[0],col[1],col[2]);
                                            }
                                            
                                            if (!dommy::color::publishValues(
                                                    *(*cIt), colAttrName, newColArray, curvePtCount))
                                                return MS::kInvalidParameter;
                                            
                                            
                                        } else {
                                            MVectorArray colorArray;
                                            MDoubleArray alphaArray;
                                            stat = getColorFromTex(thisMObject(), aTexColor, uArray, vArray, colorArray, alphaArray);
                                            CHECK_MSTATUS_AND_RETURN_IT(stat);
                                            if (colorArray.length() != curvePtCount) {
                                                return MS::kFailure;
                                            }
                                            
                                            DVec3DataList newColArray(colorArray.length(), DVec3Data(0,0,0));
                                            for (uint i=0; i<colorArray.length(); i++) {
                                                MVector col(colorArray[i]);
                                                newColArray[i] = DVec3Data(col[0],col[1],col[2]);
                                            }
                                            
                                            if (!dommy::color::publishValues(
                                                    *(*cIt), colAttrName, newColArray, curvePtCount))
                                                return MS::kInvalidParameter;
                                            
                                        }
                                        
                                    } else {
                                        return MS::kInvalidParameter;
                                    }
                                    
                                } else {
                                    return MS::kInvalidParameter;
                                }
                                
                                
                                
                            }
                            
                        }
                        
                        
                        
                    } else if(colorAddTypeVal==dommy::color::kBoundingBox) {
#pragma mark bounding box
                        
                        //point only
                        
                        Imath::V3d minimum(0, 0, 0), maximum(0, 0, 0);
                        if (!dommy::color::transformedPointBounds(
                                outDCrvs->beginCurves(), outDCrvs->endCurves(), minimum, maximum))
                            return MS::kInvalidParameter;
                        
                        for (auto cIt=outDCrvs->beginCurves(); cIt!=outDCrvs->endCurves(); ++cIt) {
                            
                            
                            const DPointList* pts = (*cIt)->getPositionPtrConst();
                            DVec3DataList newColArray;
                            if (!dommy::color::transformedBoundingColors(
                                    *pts, (*cIt)->getTransformMatrix(), minimum, maximum, newColArray))
                                return MS::kInvalidParameter;
                            
                            if (!dommy::color::publishValues(
                                    *(*cIt), colAttrName, newColArray, (*cIt)->numberOfPoints()))
                                return MS::kInvalidParameter;
                            
                            
                            
                        }
                        
                        
                    } else if(colorAddTypeVal==dommy::color::kPalette) {
#pragma mark palette color
                        
                        const MDataHandle paletteTypeHandle =
                            data.inputValue(aPaletteType, &stat);
                        CHECK_MSTATUS_AND_RETURN_IT(stat);
                        const short paletteType = paletteTypeHandle.asShort();
                        if (paletteType < 0 || paletteType > 1) {
                            return MS::kInvalidParameter;
                        }
                        const MDataHandle seedHandle =
                            data.inputValue(aSeed, &stat);
                        CHECK_MSTATUS_AND_RETURN_IT(stat);
                        const int seedVal = seedHandle.asInt();
                        
                        
                        
                        
                        //パレットカラーの取得
                        MColorArray plColArray;
                        DOMMY_RETURN_IF_FAILURE(dommy::color::readPalette(
                            data, aPaletteColor, plColArray));

                        if (assigneType==dommy::color::kPrimitive) {//primitive
                            
                            uint curveCount = outDCrvs->numberOfCurves();
                            
                            DVec3DataList newColArray(curveCount, DVec3Data(0,0,0));//color data
                            
                            if (plColArray.length()>0) {
                                dommy::math::RandomGenerator randomGenerator(static_cast<unsigned int>(seedVal));
                                
                                uint stepIndex=0;
                                
                                for (uint i=0; i<curveCount; i++) {
                                    MColor col;
                                    
                                    if (paletteType==0) {
                                        //step
                                        col = plColArray[stepIndex];
                                        
                                        if (stepIndex==(plColArray.length()-1)) {
                                            stepIndex=0;
                                        } else {
                                            stepIndex++;
                                        }
                                        
                                    } else {
                                        //random
                                        int type;
                                        if (plColArray.length()>1) {
                                            type = static_cast<int>(randomGenerator.index(plColArray.length()));
                                            col = plColArray[type];
                                            
                                        } else if (plColArray.length()==1) {
                                            col = plColArray[0];
                                        }
                                        
                                    }
                                    
                                    
                                    newColArray[i]=DVec3Data(col[0],col[1],col[2]);
                                }
                            }
                            
                            if (!dommy::color::publishValues(
                                    *outDCrvs, colAttrName, newColArray, curveCount))
                                return MS::kInvalidParameter;
                            
                            
                        } else {
                            
                            
                            dommy::math::RandomGenerator randomGenerator(static_cast<unsigned int>(seedVal));
                            for (auto cIt=outDCrvs->beginCurves(); cIt!=outDCrvs->endCurves(); ++cIt) {
                                
                                uint ptCount = (*cIt)->numberOfPoints();
                                
                                DVec3DataList newColArray(ptCount, DVec3Data(0,0,0));//color data
                                
                                if (plColArray.length()>0) {
                                    
                                    
                                    uint stepIndex=0;
                                    
                                    for (uint i=0; i<ptCount; i++) {
                                        MColor col;
                                        
                                        if (paletteType==0) {
                                            //step
                                            col = plColArray[stepIndex];
                                            
                                            if (stepIndex==(plColArray.length()-1)) {
                                                stepIndex=0;
                                            } else {
                                                stepIndex++;
                                            }
                                            
                                        } else {
                                            //random
                                            int type;
                                            if (plColArray.length()>1) {
                                                type = static_cast<int>(randomGenerator.index(plColArray.length()));
                                                col = plColArray[type];
                                                
                                            } else if (plColArray.length()==1) {
                                                col = plColArray[0];
                                            }
                                            
                                        }
                                        
                                        
                                        newColArray[i]=DVec3Data(col[0],col[1],col[2]);
                                    }
                                }
                                
                                if (!dommy::color::publishValues(
                                        *(*cIt), colAttrName, newColArray, ptCount))
                                    return MS::kInvalidParameter;
                                
                                
                                
                                
                            }
                            
                            
                            
                        }
                        
                        
                        
                        
                    }
                    
                }
                
                
                MStatus outputStatus;
                MDataHandle output = data.outputValue(outDC, &outputStatus);
                CHECK_MSTATUS_AND_RETURN_IT(outputStatus);
                DOMMY_RETURN_IF_FAILURE(
                    dommy::maya::publishTypedPluginDataObject<DCurveData>(
                        output, pluginData.object(), DCurveData::id));
                output.setClean();
                
                return data.setClean(plug);
            } else {
                return stat ? MS::kFailure : stat;
            }
            
        } else {
            MGlobal::displayError("DCColor: failed to read inDC.");
            return MS::kFailure;
        }
		
		
    }
    return MS::kUnknownParameter;
}
DOMMY_COMPUTE_CATCH("DCColor")

MStatus DCColor::initialize() try {
    MStatus stat;
    MFnNumericAttribute nAttr;
    MFnGenericAttribute gAttr;
    MFnTypedAttribute tAttr;
    inDC = tAttr.create("inDC", "idc", DCurveData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inDC));
    MFnEnumAttribute assignEnumFn;
    aAssigneType = assignEnumFn.create("AssigneType", "asgnty", 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(assignEnumFn.addField("Primitive", 0));
    DOMMY_RETURN_IF_FAILURE(assignEnumFn.addField("Point", 1));
    DOMMY_RETURN_IF_FAILURE(assignEnumFn.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(assignEnumFn.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aAssigneType));
    MFnEnumAttribute enumFn;
    aColorType = enumFn.create("ColorType", "colt", 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(enumFn.addField("Solid Color", 0));
    DOMMY_RETURN_IF_FAILURE(enumFn.addField("Random Color", 1));
    DOMMY_RETURN_IF_FAILURE(enumFn.addField("Ramp Color", 2));
    DOMMY_RETURN_IF_FAILURE(enumFn.addField("Texture Color", 3));
    DOMMY_RETURN_IF_FAILURE(enumFn.addField("Bounding Box", 4));
    DOMMY_RETURN_IF_FAILURE(enumFn.addField("Expression", 5));
    DOMMY_RETURN_IF_FAILURE(enumFn.addField("Palette Color", 6));
    DOMMY_RETURN_IF_FAILURE(enumFn.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(enumFn.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aColorType));
    MFnStringData fnStringData;
    const MObject aColorNameDefault = fnStringData.create("Cd", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aColorName = tAttr.create("ColorName", "coln", MFnData::kString, aColorNameDefault, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aColorName));
    MObject uvName = fnStringData.create("uv", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aUVAttrName = tAttr.create("UVAttrName", "uvn", MFnData::kString, uvName, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aUVAttrName));
    aSolidColor = nAttr.createColor("SolidColor", "scol", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1.0, 1.0, 1.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aSolidColor));
    MRampAttribute colAttr;
    aRampColor = colAttr.createColorRamp("RampColor", "rcol", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(addAttribute(aRampColor));
    aTexColor = nAttr.createColor("TexColor", "tcol", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1.0, 1.0, 1.0, 1.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aTexColor));
    aSeed = nAttr.create("Seed", "se", MFnNumericData::kInt, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aSeed));
    aMinRange = nAttr.create("MinRange", "mnr", MFnNumericData::k3Float, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0, 0.0, 0.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aMinRange));
    aMaxRange = nAttr.create("MaxRange", "mxr", MFnNumericData::k3Float, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1.0, 1.0, 1.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aMaxRange));
    aIs3DCalc = nAttr.create("Is3DCalc", "i3d", MFnNumericData::kBoolean, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aIs3DCalc));
    MObject exAttrName = fnStringData.create("", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aExpression = tAttr.create("Expression", "expn", MFnData::kString, exAttrName, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aExpression));
    MFnEnumAttribute plEnumFn;
    aPaletteType = plEnumFn.create("PaletteType", "plt", 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(plEnumFn.addField("Order", 0));
    DOMMY_RETURN_IF_FAILURE(plEnumFn.addField("Random", 1));
    DOMMY_RETURN_IF_FAILURE(plEnumFn.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(plEnumFn.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aPaletteType));
    aPaletteColor = nAttr.createColor("PaletteColor", "plcol", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(nAttr.setArray(true));
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1.0, 1.0, 1.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aPaletteColor));
    outDC = tAttr.create("outDC", "odc", DCurveData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(inDC, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aAssigneType, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aColorType, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aColorName, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aUVAttrName, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aSolidColor, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aTexColor, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aRampColor, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aSeed, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aMinRange, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aMaxRange, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aIs3DCalc, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aExpression, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aPaletteType, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aPaletteColor, outDC));
    return MS::kSuccess;
} catch (...) {
    return dommy::maya::reportEntryPointException("DCColor", "initialize",
                                                  std::current_exception());
}
