//
//  DPColor.cpp
//  dommyTool
//
//  Created by yuzo on 2016/06/23.
//
//

#include "common/utility/DMayaEntryPointGuard.h"
#include "DPColor.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DMayaPluginDataOutput.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DColorMode.h"
#include "common/utility/DColorPalette.h"
#include "common/utility/DColorPublication.h"
#include "common/utility/DColorBounds.h"
#include "common/utility/DPointArrayConversion.h"
#include "DPointDataAccess.h"
#include <atomic>
#include "common/utility/DExpressionResult.h"

#include "DMayaHeaders.h"
#include "DMayaDebug.h"

#include "DPointDataHeaders.h"
#include "common/utility/DMayaRampSampling.h"


#include "DGlobalTex.h"
#include "DNoiseOp.h"
#include "common/utility/DEvaluationContext.h"

#include "DRandomGenerator.h"

const MTypeId DPColor::typeId( ID_DP_COLOR );

MObject DPColor::inDP;

MObject DPColor::aColorType;
MObject DPColor::aColorName;
MObject DPColor::aUVAttrName;

MObject DPColor::aSolidColor;
MObject DPColor::aTexColor;
MObject DPColor::aRampColor;
MObject DPColor::aSeed;
MObject DPColor::aIs3DCalc;
MObject DPColor::aMinRange;
MObject DPColor::aMaxRange;
MObject DPColor::aExpression;

MObject DPColor::aPaletteType;
MObject DPColor::aPaletteColor;

MObject DPColor::outDP;



void *DPColor::creator() {
    return ::dommy::maya::createObject("DPColor", [&]() {
        return new DPColor();
    });
}

void DPColor::postConstructor()
{
//    MPlug frampPlug( thisMObject(), aRampColor );
//    MRampAttribute frampAttr( frampPlug );
//    if (frampAttr.getNumEntries()==0) {
//        MFloatArray pos;
//        MColorArray val;
//
//        pos.append(0.0f);
//        pos.append(1.0f);
//
//        val.append(MColor(0.0,0.0,0.0,1.0));
//        val.append(MColor(1.0,1.0,1.0,1.0));
//
//        MIntArray inp;
//        inp.append(MRampAttribute::kLinear);
//        inp.append(MRampAttribute::kLinear);
//        CHECK_MSTATUS(frampAttr.setRamp(val, pos, inp));
//    }
    
}





MStatus DPColor::compute( const MPlug& plug, MDataBlock& data ) try
{
    MStatus stat;
    
    if( plug == outDP ) {
        
			const MDataHandle colorTypeHandle =
                data.inputValue(aColorType, &stat);
            CHECK_MSTATUS_AND_RETURN_IT(stat);
            const short colorAddTypeVal = colorTypeHandle.asShort();
            if (!dommy::color::isValidMode(colorAddTypeVal)) {
                MGlobal::displayError("DPColor: unsupported ColorType value.");
                return MS::kInvalidParameter;
            }

            const MDataHandle uvNameHandle =
                data.inputValue(aUVAttrName, &stat);
            CHECK_MSTATUS_AND_RETURN_IT(stat);
			const std::string uvAttrNameStr =
                uvNameHandle.asString().asChar();
		
#pragma mark inDPの取得

        
        
		
        const DPData* inputDPData = nullptr;
        stat = getValidInputDP(inDP, data, inputDPData);
        if (!stat) return stat;

        if (!inputDPData ||
            !dommy::attribute::validArrayConversionBudget(inputDPData->numberOfPoints(),
                                                          sizeof(Imath::V3d))) {
            EchoWarning(name().asChar(), "Failed input data inDP");
            return MS::kFailure;
        }

        MFnPluginData pluginData;
        pluginData.create(DPData::id, &stat);
        
        if (stat != MS::kSuccess) {
            EchoWarning(name().asChar(), "Failed to create a new DPData object");
            return stat;
        }
        
        DPData* outputDPData = nullptr;
        stat = dommy::maya::getPluginData(pluginData, outputDPData);
        if (!stat) return stat;
        outputDPData->setDPData(*inputDPData);
        
        
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
            const MVector solidColorVal = solidColorHandle.asFloatVector();
            if (!dommy::color::isFiniteTriple(
                    solidColorVal[0], solidColorVal[1],
                    solidColorVal[2])) {
                MGlobal::displayError(
                    "DPColor: SolidColor must contain finite values.");
                return MS::kInvalidParameter;
            }

            if (!dommy::color::publishAttribute(
                    *outputDPData, colAttrName,
                    DPVec3fAttr(DVec3Data(solidColorVal[0], solidColorVal[1], solidColorVal[2]),
                                DVec3Data(0.0, 0.0, 0.0), outputDPData->numberOfPoints()),
                    outputDPData->numberOfPoints()))
                return MS::kInvalidParameter;

        } else if(colorAddTypeVal==dommy::color::kRandom) {//Random Color
#pragma mark Random Color
            const MDataHandle seedHandle = data.inputValue(aSeed, &stat);
            CHECK_MSTATUS_AND_RETURN_IT(stat);
            const unsigned int seedVal = seedHandle.asInt();
            
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
                    "DPColor: Random color ranges must be finite and "
                    "MinRange must not exceed MaxRange.");
                return MS::kInvalidParameter;
            }
            
            DVec3DataList colorArray;
            dommy::math::RandomGenerator randomGenerator(seedVal);
            for (uint i=0; i<outputDPData->numberOfPoints(); i++) {
                colorArray.push_back(DVec3Data(randomGenerator.uniform(minVal[0], maxVal[0]),
                                               randomGenerator.uniform(minVal[1], maxVal[1]),
                                               randomGenerator.uniform(minVal[2], maxVal[2])));
            }
            
//            outputDPData->addAttr(colAttrName, DPVec3dAttr(colorArray, Imath::V3d(0.0,0.0,0.0)));
            if (!dommy::color::publishValues(*outputDPData, colAttrName, colorArray,
                                             outputDPData->numberOfPoints()))
                return MS::kInvalidParameter;

        } else if(colorAddTypeVal==dommy::color::kRamp) {//Ramp Color
#pragma mark Ramp Color
			if (outputDPData->isExistAttr(uvAttrNameStr)) {
				
				DP_ATTR_DATA_TYPE type = outputDPData->getAttr(uvAttrNameStr)->getAttrType();
				size_t pointCount = outputDPData->numberOfPoints();
                
				if (isSingleValueAttrType(type) ||
                    isVector2DAttrType(type) ||
                    isVector3DAttrType(type)) {
					
					MDoubleArray uArray,vArray;
					
					//UV データの取得
					if (type == DP_ATTR_INT) {
                        getDPSingleValueToFloat<int32_t>(*inputDPData, uvAttrNameStr, uArray);
					} else if (type == DP_ATTR_BOOL) {
                        getDPSingleValueToFloat<bool>(*inputDPData, uvAttrNameStr, uArray);
					} else if (type == DP_ATTR_UINT32) {
                        getDPSingleValueToFloat<uint32_t>(*inputDPData, uvAttrNameStr, uArray);
					} else if (type == DP_ATTR_INT64) {
                        getDPSingleValueToFloat<int64_t>(*inputDPData, uvAttrNameStr, uArray);
					} else if (type == DP_ATTR_FLOAT) {
                        getDPSingleValueToFloat<float>(*inputDPData, uvAttrNameStr, uArray);
					} else if (type == DP_ATTR_DOUBLE) {
                        getDPSingleValueToFloat<double>(*inputDPData, uvAttrNameStr, uArray);
                    } else if (type == DP_ATTR_VEC2I) {
                        getDPTwoArray<Imath::V2i>(*inputDPData, uvAttrNameStr, uArray,vArray);
                    } else if (type == DP_ATTR_VEC2F) {
                        getDPTwoArray<Imath::V2f>(*inputDPData, uvAttrNameStr, uArray,vArray);
                    } else if (type == DP_ATTR_VEC2D) {
                        getDPTwoArray<Imath::V2d>(*inputDPData, uvAttrNameStr, uArray,vArray);
                    } else if (type == DP_ATTR_VEC3I) {
                        getDPTwoArray<Imath::V3i>(*inputDPData, uvAttrNameStr, uArray,vArray);
                    } else if (type == DP_ATTR_VEC3F) {
                        getDPTwoArray<Imath::V3f>(*inputDPData, uvAttrNameStr, uArray,vArray);
                    }else if (type == DP_ATTR_VEC3D) {
                        getDPTwoArray<Imath::V3d>(*inputDPData, uvAttrNameStr, uArray,vArray);
					}else {
						MGlobal::displayError("DPColor: Ramp UV attribute has an unsupported type.");
                        return MS::kInvalidParameter;
					}
					
					
					if (uArray.length() == pointCount) {
						
						//ランプデータの取得
						MPlug colorRampPlug( thisMObject(), aRampColor );
						MRampAttribute colorRampPlugAttr( colorRampPlug );
                        
                        
						DVec3DataList newColArray(pointCount, DVec3Data(0,0,0));
						
						for (size_t i=0; i<pointCount; ++i) {
							MColor col;
							if (!dommy::maya::sampleFiniteColorRamp(
									colorRampPlugAttr, uArray[i], col)) {
								return MS::kFailure;
							}
							newColArray[i] = DVec3Data(col[0],col[1],col[2]);
						}
                        
//                        for (size_t i=0; i<pointCount; i++) {
//                            MColor col;
//                            colorRampPlugAttr.getColorAtPosition(uArray[i], col);
//                            newColArray[i] = Imath::V3d(col[0],col[1],col[2]);
//                        }
						
//                        outputDPData->addAttr(colAttrName, DPVec3dAttr(newColArray, Imath::V3d(0.0,0.0,0.0)));
                        if (!dommy::color::publishValues(*outputDPData, colAttrName, newColArray,
                                                         outputDPData->numberOfPoints()))
                            return MS::kInvalidParameter;
                    } else {
						MGlobal::displayError("DPColor: Ramp UV attribute count does not match the point count.");
                        return MS::kInvalidParameter;
					}
					
				} else {
                    MGlobal::displayError(
                        "DPColor: Ramp UV attribute has an unsupported type.");
                    return MS::kInvalidParameter;
                }
				
			} else {
				MGlobal::displayError("DPColor: Ramp UV attribute does not exist.");
                return MS::kInvalidParameter;
			}
			
        } else if(colorAddTypeVal==dommy::color::kTexture) {//Texture Color
#pragma mark Texture Color
			const MDataHandle is3DHandle =
                data.inputValue(aIs3DCalc, &stat);
            CHECK_MSTATUS_AND_RETURN_IT(stat);
            const bool is3DCalc = is3DHandle.asBool();
			if (outputDPData->isExistAttr(uvAttrNameStr)) {
				
                DP_ATTR_DATA_TYPE type = outputDPData->getAttr(uvAttrNameStr)->getAttrType();
                size_t pointCount = outputDPData->numberOfPoints();
                
                if (isVector2DAttrType(type) ||
                    isVector3DAttrType(type)) {
					
					MDoubleArray uArray,vArray;
					if (type == DP_ATTR_VEC2I) {
                        getDPTwoArray<Imath::V2i>(*inputDPData, uvAttrNameStr, uArray,vArray);
					} else if (type == DP_ATTR_VEC2F) {
                        getDPTwoArray<Imath::V2f>(*inputDPData, uvAttrNameStr, uArray,vArray);
					} else if (type == DP_ATTR_VEC2D) {
                        getDPTwoArray<Imath::V2d>(*inputDPData, uvAttrNameStr, uArray,vArray);
                    } else if (type == DP_ATTR_VEC3I) {
                        getDPTwoArray<Imath::V3i>(*inputDPData, uvAttrNameStr, uArray,vArray);
                    } else if (type == DP_ATTR_VEC3F) {
                        getDPTwoArray<Imath::V3f>(*inputDPData, uvAttrNameStr, uArray,vArray);
                    }else if (type == DP_ATTR_VEC3D) {
                        getDPTwoArray<Imath::V3d>(*inputDPData, uvAttrNameStr, uArray,vArray);
                    }else {
                        MGlobal::displayError(
                            "DPColor: Texture UV attribute has an unsupported type.");
                        return MS::kInvalidParameter;
                    }
                    
//                    cout << "uArray:" << uArray.length() << " vArray:" << vArray.length() << endl;
					
					if ((uArray.length() == pointCount) && (vArray.length() == pointCount) ) {
						
						//getColor
						if (is3DCalc) {
							
							const DPointList *ptsPtr = outputDPData->getPositionPtrConst();
							MFloatPointArray points;
                            DOMMY_RETURN_IF_FAILURE(
                                dommy::geometry::copyPointsToMayaArray(*ptsPtr, points));
                            
                            
							MFloatVectorArray colorArray,alphaArray;
								stat = getColorFromShaderNetwork(thisMObject(), aTexColor, points, uArray, vArray, colorArray, alphaArray);
								CHECK_MSTATUS_AND_RETURN_IT(stat);
                                if (colorArray.length() != pointCount) {
                                    return MS::kFailure;
                                }
                            
                            
                            if (stat == MS::kSuccess) {
                                
                                DVec3DataList newColArray(pointCount, DVec3Data(0,0,0));
                                
                                for (size_t i=0; i<pointCount; ++i) {
                                    const MFloatVector col = colorArray[static_cast<unsigned int>(i)];
                                    newColArray[i] = DVec3Data(col[0],col[1],col[2]);
                                }

                                if (!dommy::color::publishValues(*outputDPData, colAttrName,
                                                                 newColArray,
                                                                 outputDPData->numberOfPoints()))
                                    return MS::kInvalidParameter;
                            }
							
						} else {
//                            cout << "calc texture" << endl;
							MVectorArray colorArray;
							MDoubleArray alphaArray;
								stat = getColorFromTex(thisMObject(), aTexColor, uArray, vArray, colorArray, alphaArray);
	                            CHECK_MSTATUS_AND_RETURN_IT(stat);
                                if (colorArray.length() != pointCount) {
                                    return MS::kFailure;
                                }
//                            cout << "colorArray:" << colorArray.length() << " alphaArray:" << alphaArray.length() << endl;
                            if (stat == MS::kSuccess) {
                                DVec3DataList newColArray(pointCount, DVec3Data(0,0,0));
                                
                                for (size_t i=0; i<pointCount; ++i) {
                                    const MVector col = colorArray[static_cast<unsigned int>(i)];
                                    newColArray[i] = DVec3Data(col[0],col[1],col[2]);
                                }
                                
//                                cout << "newColArray:" << newColArray.size() << endl;

                                if (!dommy::color::publishValues(*outputDPData, colAttrName,
                                                                 newColArray,
                                                                 outputDPData->numberOfPoints()))
                                    return MS::kInvalidParameter;
                            }
                            
						}
						
						
					} else {
						MGlobal::displayError("DPColor: Texture UV attribute count does not match the point count.");
                        return MS::kInvalidParameter;
					}
				} else {
                    MGlobal::displayError(
                        "DPColor: Texture UV attribute has an unsupported type.");
                    return MS::kInvalidParameter;
                }
                
                
                
				
            } else if (is3DCalc) {
				
                
                const DPointList *ptsPtr = outputDPData->getPositionPtrConst();
                size_t count = ptsPtr->size();
                MFloatPointArray points;
                DOMMY_RETURN_IF_FAILURE(
                    dommy::geometry::copyPointsToMayaArray(*ptsPtr, points));
                
                
                MDoubleArray uArray, vArray;
                const auto fillZero = [](unsigned int, double& value) -> MStatus {
                    value = 0.0;
                    return MS::kSuccess;
                };
                DOMMY_RETURN_IF_FAILURE(dommy::maya::buildCheckedArray(count, fillZero, uArray));
                DOMMY_RETURN_IF_FAILURE(dommy::maya::buildCheckedArray(count, fillZero, vArray));
                
                MFloatVectorArray colorArray,alphaArray;
                stat = getColorFromShaderNetwork(thisMObject(), aTexColor, points, uArray, vArray, colorArray, alphaArray);
                CHECK_MSTATUS_AND_RETURN_IT(stat);
                if (colorArray.length() != count) {
                    return MS::kFailure;
                }
                
                
                
                
                DColorList newColArray(colorArray.length(), DColor(0,0,0));
                
                for (uint i=0; i<colorArray.length(); i++) {
                    MVector col(colorArray[i]);
                    newColArray[i] = DColor(col[0],col[1],col[2]);
                }
                
//                outputDPData->addAttr(colAttrName, DPVec3dAttr(newColArray, Imath::V3d(0.0,0.0,0.0)));
                if (!dommy::color::publishValues(*outputDPData, colAttrName, newColArray,
                                                 outputDPData->numberOfPoints()))
                    return MS::kInvalidParameter;

            } else {
				MGlobal::displayError("DPColor: Texture UV attribute does not exist.");
                return MS::kInvalidParameter;
			}
			
        } else if(colorAddTypeVal==dommy::color::kBoundingBox) {
#pragma mark bounding box
            const DPointList* ptsPtr = outputDPData->getPositionPtrConst();
            const size_t pointCount = ptsPtr->size();
            Imath::V3d minimum(0, 0, 0), maximum(0, 0, 0);
            if (!dommy::color::pointBounds(*ptsPtr, minimum, maximum))
                return MS::kInvalidParameter;
            DVec3DataList newColArray(pointCount, DVec3Data(0, 0, 0));
            std::atomic<bool> valid(true);
            if (pointCount > 0) {
                tbb::blocked_range<size_t> range = tbb::blocked_range<size_t>(0, pointCount, DGetGlobalProcessNum(pointCount));
                tbb::parallel_for(range, [=, &newColArray, &valid](const tbb::blocked_range<size_t>& r){
                    for (size_t i = r.begin(); i != r.end(); i++) {
                        if (!dommy::color::normalizeBoundingColor(
                                (*ptsPtr)[i], minimum, maximum, newColArray[i]))
                            valid.store(false, std::memory_order_relaxed);
                    }
                });
            }
            if (!valid.load(std::memory_order_relaxed))
                return MS::kInvalidParameter;
            if (!dommy::color::publishValues(*outputDPData, colAttrName, newColArray,
                                             outputDPData->numberOfPoints()))
                return MS::kInvalidParameter;

        } else if(colorAddTypeVal==dommy::color::kExpression) {
#pragma mark expression
            const MDataHandle expressionHandle = data.inputValue(aExpression, &stat);
            CHECK_MSTATUS_AND_RETURN_IT(stat);
            const std::string exprStr = expressionHandle.asString().asChar();
            DPointSeExpr expression(exprStr);
            if (exprStr.empty() || !expression.isValid()) {
                DOMMY_MAYA_WARNING("DPColor", "expression failed: " << expression.parseError());
                return MS::kInvalidParameter;
            }
            MTime currentTime;
            DOMMY_RETURN_IF_FAILURE(dommy::maya::getEvaluationTime(data, currentTime));
            DOMMY_RETURN_IF_FAILURE(dommy::maya::setExpressionTime(expression, currentTime));
            expression.setDPData(outputDPData);
            std::vector<Imath::V3d> colors;
            if (!dommy::expression::evaluateResults(expression,
                                                    *outputDPData->getPositionPtrConst(), colors) ||
                !dommy::expression::publishResult(*outputDPData, colAttrName, 3, colors)) {
                MGlobal::displayError("DPColor: expression evaluation or color conversion failed.");
                return MS::kInvalidParameter;
            }
        } else if(colorAddTypeVal==dommy::color::kPalette) {
#pragma mark palette color
            const MDataHandle paletteTypeHandle =
                data.inputValue(aPaletteType, &stat);
            CHECK_MSTATUS_AND_RETURN_IT(stat);
            const int paletteType = paletteTypeHandle.asInt();
            if (paletteType < 0 || paletteType > 1) {
                return MS::kInvalidParameter;
            }
            const MDataHandle seedHandle = data.inputValue(aSeed, &stat);
            CHECK_MSTATUS_AND_RETURN_IT(stat);
            const unsigned int seedVal = seedHandle.asInt();
            
            
            
            //パレットカラーの取得
            MColorArray plColArray;
            DOMMY_RETURN_IF_FAILURE(dommy::color::readPalette(
                data, aPaletteColor, plColArray));

            size_t palletCount = plColArray.length();
            
            
            
            const DPointList *ptsPtr = outputDPData->getPositionPtrConst();
            size_t pointCount = ptsPtr->size();
            DColorList newColArray(pointCount, DColor(0,0,0));//color data
            
            switch (paletteType) {
                    case 0://ordr
                    {
                        for (size_t i=0; i<pointCount; ++i) {
                            const MColor col = plColArray[static_cast<unsigned int>(i%palletCount)];
                            for (int j=0; j<3; ++j) {
                                newColArray[i][j] = col[j];
                            }
                        }
                    }
                        break;
                    case 1://random
                    {
                        std::vector<int> palletIndexList;
                        DGenerateRandomValue(seedVal, int(0), int(plColArray.length()-1), pointCount, palletIndexList);
                        
                        for (size_t i=0; i<pointCount; ++i) {
                            const MColor col = plColArray[static_cast<unsigned int>(palletIndexList[i])];
                            for (int j=0; j<3; ++j) {
                                newColArray[i][j] = col[j];
                            }
                        }
                    }
                        break;
                    default:
                        break;
            }
            
            
//            outputDPData->addAttr(colAttrName, DPVec3dAttr(newColArray, Imath::V3d(0.0,0.0,0.0)));
            if (!dommy::color::publishValues(*outputDPData, colAttrName, newColArray,
                                             outputDPData->numberOfPoints()))
                return MS::kInvalidParameter;
        }
		
		
		
#pragma mark Create output
		MDataHandle output = data.outputValue(outDP, &stat);
		if (!stat) return stat;
		stat = dommy::maya::publishTypedPluginDataObject<DPData>(
			output, pluginData.object(), DPData::id);
		if (!stat) return stat;
		
		return data.setClean(plug);
    }
    
    return MS::kUnknownParameter;
}
DOMMY_COMPUTE_CATCH("DPColor")

MStatus DPColor::initialize() try {
    MStatus stat;
    MFnNumericAttribute nAttr;
    MFnGenericAttribute gAttr;
    MFnTypedAttribute tAttr;
    inDP = tAttr.create("inDP", "idp", DPData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inDP));
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
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1.0, 1.0, 1.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(nAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(nAttr.setArray(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aPaletteColor));
    outDP = tAttr.create("outDP", "odp", DPData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(inDP, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aColorType, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aColorName, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aUVAttrName, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aSolidColor, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aTexColor, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aRampColor, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aSeed, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aMinRange, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aMaxRange, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aIs3DCalc, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aExpression, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aPaletteType, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aPaletteColor, outDP));
    return MS::kSuccess;
} catch (...) {
    return dommy::maya::reportEntryPointException("DPColor", "initialize",
                                                  std::current_exception());
}
