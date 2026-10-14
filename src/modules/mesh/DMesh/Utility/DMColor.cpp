//
//  DMColor.cpp
//  dommyTool
//
//  Created by yuzo on 2017/02/12.
//
//

#include "common/utility/DMayaEntryPointGuard.h"
#include "DMColor.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DMayaPluginDataOutput.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DColorMode.h"
#include "common/utility/DColorPalette.h"
#include "common/utility/DColorPublication.h"
#include "common/utility/DColorBounds.h"
#include "common/utility/DPointArrayConversion.h"
#include "common/utility/DExpressionResult.h"
#include "common/utility/DMayaRampSampling.h"


#include "DMayaHeaders.h"
#include "DMayaDebug.h"


#include "DMeshDataHeaders.h"
#include "DPointSeExpr.h"

#include "DNoiseOp.h"
#include "DGlobalTex.h"
#include "common/utility/DEvaluationContext.h"
#include "DRandomGenerator.h"

namespace {

bool readMeshRampCoordinate(
    const DMItem& mesh,
    const std::string& attributeName,
    MDoubleArray& uCoordinates) {
    if (!mesh || !mesh->isExistAttr(attributeName)) return false;

    const DP_ATTR_DATA_TYPE type =
        mesh->getAttr(attributeName)->getAttrType();
    MDoubleArray unusedVCoordinates;
    if (type == DP_ATTR_INT) {
        getDMPSingleValueToFloat<int32_t>(
            mesh, attributeName, uCoordinates);
    } else if (type == DP_ATTR_BOOL) {
        getDMPSingleValueToFloat<bool>(mesh, attributeName, uCoordinates);
    } else if (type == DP_ATTR_UINT32) {
        getDMPSingleValueToFloat<uint32_t>(
            mesh, attributeName, uCoordinates);
    } else if (type == DP_ATTR_INT64) {
        getDMPSingleValueToFloat<int64_t>(
            mesh, attributeName, uCoordinates);
    } else if (type == DP_ATTR_FLOAT) {
        getDMPSingleValueToFloat<float>(mesh, attributeName, uCoordinates);
    } else if (type == DP_ATTR_DOUBLE) {
        getDMPSingleValueToFloat<double>(mesh, attributeName, uCoordinates);
    } else if (type == DP_ATTR_VEC2I) {
        getDMPTwoArray<Imath::V2i>(
            mesh, attributeName, uCoordinates, unusedVCoordinates);
    } else if (type == DP_ATTR_VEC2F) {
        getDMPTwoArray<Imath::V2f>(
            mesh, attributeName, uCoordinates, unusedVCoordinates);
    } else if (type == DP_ATTR_VEC2D) {
        getDMPTwoArray<Imath::V2d>(
            mesh, attributeName, uCoordinates, unusedVCoordinates);
    } else if (type == DP_ATTR_VEC3I) {
        getDMPTwoArray<Imath::V3i>(
            mesh, attributeName, uCoordinates, unusedVCoordinates);
    } else if (type == DP_ATTR_VEC3F) {
        getDMPTwoArray<Imath::V3f>(
            mesh, attributeName, uCoordinates, unusedVCoordinates);
    } else if (type == DP_ATTR_VEC3D) {
        getDMPTwoArray<Imath::V3d>(
            mesh, attributeName, uCoordinates, unusedVCoordinates);
    } else {
        return false;
    }
    return true;
}

bool readMeshTextureCoordinates(
    const DMItem& mesh,
    const std::string& attributeName,
    MDoubleArray& uCoordinates,
    MDoubleArray& vCoordinates) {
    if (!mesh || !mesh->isExistAttr(attributeName)) return false;

    const DP_ATTR_DATA_TYPE type =
        mesh->getAttr(attributeName)->getAttrType();
    if (type == DP_ATTR_VEC2I) {
        getDMPTwoArray<Imath::V2i>(
            mesh, attributeName, uCoordinates, vCoordinates);
    } else if (type == DP_ATTR_VEC2F) {
        getDMPTwoArray<Imath::V2f>(
            mesh, attributeName, uCoordinates, vCoordinates);
    } else if (type == DP_ATTR_VEC2D) {
        getDMPTwoArray<Imath::V2d>(
            mesh, attributeName, uCoordinates, vCoordinates);
    } else if (type == DP_ATTR_VEC3I) {
        getDMPTwoArray<Imath::V3i>(
            mesh, attributeName, uCoordinates, vCoordinates);
    } else if (type == DP_ATTR_VEC3F) {
        getDMPTwoArray<Imath::V3f>(
            mesh, attributeName, uCoordinates, vCoordinates);
    } else if (type == DP_ATTR_VEC3D) {
        getDMPTwoArray<Imath::V3d>(
            mesh, attributeName, uCoordinates, vCoordinates);
    } else {
        return false;
    }
    return true;
}

} // namespace





const MTypeId DMColor::typeId( ID_DM_COLOR );


MObject DMColor::inDM;

MObject DMColor::aAssigneType;
MObject DMColor::aColorType;
MObject DMColor::aColorName;
MObject DMColor::aUVAttrName;
//solid
MObject DMColor::aSolidColor;
//ramp
MObject DMColor::aRampColor;
//texture
MObject DMColor::aIs3DCalc;
MObject DMColor::aTexColor;

//random
MObject DMColor::aSeed;
MObject DMColor::aMinRange;
MObject DMColor::aMaxRange;

//expression
MObject DMColor::aExpression;

//palette
MObject DMColor::aPaletteType;
MObject DMColor::aPaletteColor;


MObject DMColor::outDM;




void *DMColor::creator()
{
    return ::dommy::maya::createObject("DMColor", [&]() {
        return new DMColor();
    });
}

MStatus DMColor::compute(const MPlug& plug, MDataBlock& data) try
{
    
    MStatus stat;
    
    if (plug == outDM) {

        const MDataHandle colorTypeHandle =
            data.inputValue(aColorType, &stat);
        CHECK_MSTATUS_AND_RETURN_IT(stat);
        const short colorAddTypeVal = colorTypeHandle.asShort();
        if (!dommy::color::isValidMode(colorAddTypeVal)) {
            MGlobal::displayError("DMColor: unsupported ColorType value.");
            return MS::kInvalidParameter;
        }
        const MDataHandle assignmentHandle =
            data.inputValue(aAssigneType, &stat);
        CHECK_MSTATUS_AND_RETURN_IT(stat);
        const short assigneType = assignmentHandle.asShort();
        if (!dommy::color::isValidAssignment(assigneType)) {
            MGlobal::displayError(
                "DMColor: unsupported AssigneType value.");
            return MS::kInvalidParameter;
        }
        if (dommy::color::requiresPointAssignment(colorAddTypeVal) &&
            assigneType != dommy::color::kPoint) {
            MGlobal::displayError(
                "DMColor: the selected ColorType supports Point "
                "assignment only.");
            return MS::kInvalidParameter;
        }

        
        
#pragma mark get input dcurve
        const DMeshData* inputDMData = nullptr;
        stat = getValidInputDM(inDM, data, inputDMData);
        if (!stat) return stat;
        
        if (inputDMData) {
            
            MFnPluginData pluginData;
            pluginData.create(DMeshData::id, &stat);
            CHECK_MSTATUS_AND_RETURN_IT(stat);
            
            
            DMeshData* const outDMeshes(dommy::maya::pluginDataPointer<DMeshData>(pluginData, &stat));
            
            if (outDMeshes) {
                
                
                DOMMY_RETURN_IF_FAILURE(outDMeshes->copyFrom(*inputDMData));
                
                
                uint curveCount = outDMeshes->numberOfMesh();
                
                if(curveCount!=0) {
                    
                    const MDataHandle uvNameHandle =
                        data.inputValue(aUVAttrName, &stat);
                    CHECK_MSTATUS_AND_RETURN_IT(stat);
                    const std::string uvAttrNameStr =
                        uvNameHandle.asString().asChar();
                    
                    //check atribute name
                    const MDataHandle colorNameHandle =
                        data.inputValue(aColorName, &stat);
                    CHECK_MSTATUS_AND_RETURN_IT(stat);
                    const std::string colAttrName =
                        colorNameHandle.asString().asChar();

                    if (!dommy::attribute::validAttributeName(colAttrName) ||
                        colAttrName.size() > dommy::generation::kMaxGeneratedStringByteCount)
                        return MS::kInvalidParameter;

                    if (colorAddTypeVal==dommy::color::kSolid) {
#pragma mark Solid Color
                        const MDataHandle solidColorHandle =
                            data.inputValue(aSolidColor, &stat);
                        CHECK_MSTATUS_AND_RETURN_IT(stat);
                        const float3 &baseCol = solidColorHandle.asFloat3();
                        if (!dommy::color::isFiniteTriple(
                                baseCol[0], baseCol[1], baseCol[2])) {
                            MGlobal::displayError(
                                "DMColor: SolidColor must contain finite values.");
                            return MS::kInvalidParameter;
                        }
                        DVec3Data solidColorVal(baseCol[0],baseCol[1],baseCol[2]);
                        
                        if(assigneType==dommy::color::kPrimitive) {
                            if (!dommy::color::publishAttribute(
                                    *outDMeshes, colAttrName,
                                    DPVec3fAttr(solidColorVal, DVec3Data(0.0, 0.0, 0.0),
                                                outDMeshes->numberOfMesh()),
                                    outDMeshes->numberOfMesh()))
                                return MS::kInvalidParameter;

                        } else {
                            
                            for (auto cIt=outDMeshes->beginMeshes(); cIt!=outDMeshes->endMeshes(); ++cIt) {
                                if (!dommy::color::publishAttribute(
                                        *(*cIt), colAttrName,
                                        DPVec3dAttr(Imath::V3d(solidColorVal), Imath::V3d(0.0),
                                                    (*cIt)->numberOfPoints()),
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
                                "DMColor: Random color ranges must be finite and "
                                "MinRange must not exceed MaxRange.");
                            return MS::kInvalidParameter;
                        }
                        
                        if (assigneType==dommy::color::kPrimitive) {
                            DVec3DataList colorArray;
                            
                            DGenerateRandomValue3(seedVal, minVal, maxVal, outDMeshes->numberOfMesh(), colorArray);

                            if (!dommy::color::publishAttribute(
                                    *outDMeshes, colAttrName,
                                    DPVec3fAttr(colorArray, DVec3Data(0.0, 0.0, 0.0)),
                                    outDMeshes->numberOfMesh()))
                                return MS::kInvalidParameter;

                        } else {
                            size_t offsetRand = 0;
                            for (auto cIt=outDMeshes->beginMeshes(); cIt!=outDMeshes->endMeshes(); ++cIt) {
                                
                                DVec3DataList colorArray;
                                DGenerateRandomValue3(dommy::random::requireOffsetNonnegativeSeed(seedVal, offsetRand), minVal, maxVal, (*cIt)->numberOfPoints(), colorArray);

                                if (!dommy::color::publishAttribute(
                                        *(*cIt), colAttrName,
                                        DPVec3fAttr(colorArray, DVec3Data(0.0, 0.0, 0.0)),
                                        (*cIt)->numberOfPoints()))
                                    return MS::kInvalidParameter;
                                offsetRand++;
                            }
                            
                        }
                        
                    } else if(colorAddTypeVal==dommy::color::kRamp) {//Ramp Color
#pragma mark Ramp Color
                        
                        // Validate every mesh before mutating the copied output.
                        for (auto meshIt=outDMeshes->beginMeshes();
                             meshIt!=outDMeshes->endMeshes(); ++meshIt) {
                            MDoubleArray uCoordinates;
                            if (!readMeshRampCoordinate(
                                    *meshIt, uvAttrNameStr, uCoordinates)) {
                                MGlobal::displayError(
                                    "DMColor: Ramp UV attribute is missing or "
                                    "has an unsupported type.");
                                return MS::kInvalidParameter;
                            }
                            if (uCoordinates.length() !=
                                (*meshIt)->numberOfPoints()) {
                                MGlobal::displayError(
                                    "DMColor: Ramp UV attribute count does not "
                                    "match the mesh point count.");
                                return MS::kInvalidParameter;
                            }
                        }

                        MPlug colorRampPlug(thisMObject(), aRampColor);
                        MRampAttribute colorRampPlugAttr(colorRampPlug);
                        for (auto meshIt=outDMeshes->beginMeshes();
                             meshIt!=outDMeshes->endMeshes(); ++meshIt) {
                            MDoubleArray uCoordinates;
                            if (!readMeshRampCoordinate(
                                    *meshIt, uvAttrNameStr, uCoordinates)) {
                                return MS::kInvalidParameter;
                            }
                            const unsigned int pointCount =
                                (*meshIt)->numberOfPoints();
                            DVec3DataList colors(
                                pointCount, DVec3Data(0, 0, 0));
                            for (unsigned int index = 0;
                                 index < pointCount; ++index) {
                                MColor color;
                                if (!dommy::maya::sampleFiniteColorRamp(
                                        colorRampPlugAttr,
                                        uCoordinates[index], color)) {
                                    return MS::kFailure;
                                }
                                colors[index] = DVec3Data(
                                    color[0], color[1], color[2]);
                            }
                            if (!dommy::color::publishValues(*(*meshIt), colAttrName, colors,
                                                             (*meshIt)->numberOfPoints()))
                                return MS::kInvalidParameter;
                        }
                        
                        
                        
                    } else if(colorAddTypeVal==dommy::color::kTexture) {//Texture Color
#pragma mark Texture Color
                        
                        //Point only
                        
                        {
                            const MDataHandle is3DHandle =
                                data.inputValue(aIs3DCalc, &stat);
                            CHECK_MSTATUS_AND_RETURN_IT(stat);
                            const bool is3DCalc = is3DHandle.asBool();

                            // Validate every mesh before sampling any texture.
                            for (auto meshIt=outDMeshes->beginMeshes();
                                 meshIt!=outDMeshes->endMeshes(); ++meshIt) {
                                MDoubleArray uCoordinates;
                                MDoubleArray vCoordinates;
                                if (!readMeshTextureCoordinates(
                                        *meshIt, uvAttrNameStr,
                                        uCoordinates, vCoordinates)) {
                                    MGlobal::displayError(
                                        "DMColor: Texture UV attribute is missing "
                                        "or has an unsupported type.");
                                    return MS::kInvalidParameter;
                                }
                                const unsigned int pointCount =
                                    (*meshIt)->numberOfPoints();
                                if (uCoordinates.length() != pointCount ||
                                    vCoordinates.length() != pointCount) {
                                    MGlobal::displayError(
                                        "DMColor: Texture UV attribute count does "
                                        "not match the mesh point count.");
                                    return MS::kInvalidParameter;
                                }
                            }

                            for (auto meshIt=outDMeshes->beginMeshes();
                                 meshIt!=outDMeshes->endMeshes(); ++meshIt) {
                                MDoubleArray uCoordinates;
                                MDoubleArray vCoordinates;
                                if (!readMeshTextureCoordinates(
                                        *meshIt, uvAttrNameStr,
                                        uCoordinates, vCoordinates)) {
                                    return MS::kInvalidParameter;
                                }
                                const unsigned int pointCount =
                                    (*meshIt)->numberOfPoints();
                                DVec3DataList colors(
                                    pointCount, DVec3Data(0, 0, 0));

                                if (is3DCalc) {
                                    MFloatPointArray mayaPoints;
                                    DOMMY_RETURN_IF_FAILURE(
                                        dommy::geometry::copyPointsToMayaArray(
                                            *(*meshIt)->getPositionPtrConst(), mayaPoints));
                                    MFloatVectorArray sampledColors;
                                    MFloatVectorArray sampledAlpha;
                                    stat = getColorFromShaderNetwork(
                                        thisMObject(), aTexColor, mayaPoints,
                                        uCoordinates, vCoordinates,
                                        sampledColors, sampledAlpha);
                                    CHECK_MSTATUS_AND_RETURN_IT(stat);
                                    if (sampledColors.length() != pointCount) {
                                        return MS::kFailure;
                                    }
                                    for (unsigned int index = 0;
                                         index < pointCount; ++index) {
                                        const MFloatVector color =
                                            sampledColors[index];
                                        colors[index] = DVec3Data(
                                            color[0], color[1], color[2]);
                                    }
                                } else {
                                    MVectorArray sampledColors;
                                    MDoubleArray sampledAlpha;
                                    stat = getColorFromTex(
                                        thisMObject(), aTexColor,
                                        uCoordinates, vCoordinates,
                                        sampledColors, sampledAlpha);
                                    CHECK_MSTATUS_AND_RETURN_IT(stat);
                                    if (sampledColors.length() != pointCount) {
                                        return MS::kFailure;
                                    }
                                    for (unsigned int index = 0;
                                         index < pointCount; ++index) {
                                        const MVector color =
                                            sampledColors[index];
                                        colors[index] = DVec3Data(
                                            color[0], color[1], color[2]);
                                    }
                                }
                                if (!dommy::color::publishValues(*(*meshIt), colAttrName, colors,
                                                                 (*meshIt)->numberOfPoints()))
                                    return MS::kInvalidParameter;
                            }
                        }
                        
                        
                        
                    } else if(colorAddTypeVal==dommy::color::kBoundingBox) {
#pragma mark bounding box
                        
                        //point only
                        
                        Imath::V3d minimum(0, 0, 0), maximum(0, 0, 0);
                        if (!dommy::color::transformedPointBounds(
                                outDMeshes->beginMeshes(), outDMeshes->endMeshes(), minimum, maximum))
                            return MS::kInvalidParameter;
                        
                        for (auto cIt=outDMeshes->beginMeshes(); cIt!=outDMeshes->endMeshes(); ++cIt) {
                            
                            
                            const DPointList *pts = (*cIt)->getPositionPtrConst();
                            
                            
                            DVec3DataList newColArray;
                            if (!dommy::color::transformedBoundingColors(
                                    *pts, (*cIt)->getTransformMatrix(), minimum, maximum, newColArray))
                                return MS::kInvalidParameter;

                            if (!dommy::color::publishValues(*(*cIt), colAttrName, newColArray,
                                                             (*cIt)->numberOfPoints()))
                                return MS::kInvalidParameter;
                        }
                        
                        
                    } else if(colorAddTypeVal==dommy::color::kExpression) {
#pragma mark expression
                        const MDataHandle expressionHandle = data.inputValue(aExpression, &stat);
                        CHECK_MSTATUS_AND_RETURN_IT(stat);
                        const std::string exprStr = expressionHandle.asString().asChar();
                        DPointSeExpr expression(exprStr);
                        if (exprStr.empty() || !expression.isValid() ||
                            expression.usesPointDataFunctions()) {
                            MGlobal::displayError("DMColor: expression is invalid or uses "
                                                  "unsupported point-data functions.");
                            return MS::kInvalidParameter;
                        }
                        MTime currentTime;
                        DOMMY_RETURN_IF_FAILURE(dommy::maya::getEvaluationTime(data, currentTime));
                        DOMMY_RETURN_IF_FAILURE(
                            dommy::maya::setExpressionTime(expression, currentTime));
                        for (auto meshIt = outDMeshes->beginMeshes();
                             meshIt != outDMeshes->endMeshes(); ++meshIt) {
                            std::vector<Imath::V3d> colors;
                            if (!dommy::expression::evaluateResults(
                                    expression, *(*meshIt)->getPositionPtrConst(), colors) ||
                                !dommy::expression::publishResult(*(*meshIt), colAttrName, 3,
                                                                  colors)) {
                                MGlobal::displayError(
                                    "DMColor: expression evaluation or color conversion failed.");
                                return MS::kInvalidParameter;
                            }
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
                            
                            uint curveCount = outDMeshes->numberOfMesh();
                            
                            //                        std::vector<Imath::V3d> newColArray(curveCount, Imath::V3d(0,0,0));//color data
                            DVec3DataList newColArray(curveCount, DColor(0,0,0));
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

                            if (!dommy::color::publishValues(*outDMeshes, colAttrName, newColArray,
                                                             outDMeshes->numberOfMesh()))
                                return MS::kInvalidParameter;

                        } else {
                            
                            
                            dommy::math::RandomGenerator randomGenerator(static_cast<unsigned int>(seedVal));
                            for (auto cIt=outDMeshes->beginMeshes(); cIt!=outDMeshes->endMeshes(); ++cIt) {
                                
                                uint ptCount = (*cIt)->numberOfPoints();
                                
                                
                                DVec3DataList newColArray(ptCount, DVec3Data(0,0,0));
                                
                                
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

                                if (!dommy::color::publishValues(*(*cIt), colAttrName, newColArray,
                                                                 (*cIt)->numberOfPoints()))
                                    return MS::kInvalidParameter;
                            }
                            
                            
                            
                        }
                        
                        
                    }
                    
                }
                
                
                MStatus outputStatus;
                MDataHandle output = data.outputValue(outDM, &outputStatus);
                CHECK_MSTATUS_AND_RETURN_IT(outputStatus);
                DOMMY_RETURN_IF_FAILURE(
                    dommy::maya::publishTypedPluginDataObject<DMeshData>(
                        output, pluginData.object(), DMeshData::id));
                output.setClean();
                
                return data.setClean(plug);
            } else {
                if (!stat) return stat;
                return MS::kFailure;
            }
            
        } else {
            MGlobal::displayError("DMColor: failed to read inDM.");
            return MS::kFailure;
        }
        
        
    }
    return MS::kUnknownParameter;
}
DOMMY_COMPUTE_CATCH("DMColor")

MStatus DMColor::initialize() try {
    MStatus stat;
    MFnNumericAttribute nAttr;
    MFnTypedAttribute tAttr;

    inDM = tAttr.create("inDM", "idm", DMeshData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inDM));

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

    const MObject colorName = fnStringData.create("Cd", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aColorName = tAttr.create("ColorName", "coln", MFnData::kString, colorName, &stat);
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

    outDM = tAttr.create("outDM", "odm", DMeshData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(outDM));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(inDM, outDM));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(aAssigneType, outDM));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aColorType, outDM));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aColorName, outDM));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aUVAttrName, outDM));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aSolidColor, outDM));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aTexColor, outDM));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aRampColor, outDM));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aSeed, outDM));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aMinRange, outDM));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aMaxRange, outDM));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aIs3DCalc, outDM));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(aExpression, outDM));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(aPaletteType, outDM));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aPaletteColor, outDM));

    return MS::kSuccess;
} catch (...) {
    return dommy::maya::reportEntryPointException("DMColor", "initialize",
                                                  std::current_exception());
}
