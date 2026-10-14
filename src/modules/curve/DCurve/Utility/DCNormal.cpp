//
//  DCNormal.cpp
//  dommyTool
//
//  Created by yuzo on 2/24/17.
//
//

#include "common/utility/DMayaEntryPointGuard.h"
#include "DCNormal.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DMayaPluginDataOutput.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DMayaRampSampling.h"
#include "common/utility/DCurveEvaluation.h"
#include "common/utility/DMayaValueValidation.h"





#include "DMayaHeaders.h"
#include "DMayaDebug.h"


#include "DCurveData.h"
#include "DCurveUtil.h"
#include "dcurve_ids.h"
#include "DPAttributeCommon.h"

#include "dommyMath.h"
#include "DNoiseOp.h"

#include <cmath>

enum {
	DC_NORMAL_VECTOR = 0,
	DC_NORMAL_FROM_CURVE,
    DC_NORMAL_FROM_CURVE_TANGENT,
	DC_NORMAL_FROM_CURVE_AND_VECTOR,
	DC_NORMAL_FROM_CURVE_RANDOM,
	DC_NORMAL_FULL_RANDOM,
	DC_NORMAL_GEN_TYPE_COUNT
};typedef int DC_NORMAL_GEN_TYPE;



const MTypeId DCNormal::typeId( ID_DC_NORMAL );


MObject DCNormal::inDC;

MObject DCNormal::aNormalAttributeName;

MObject DCNormal::aGenerateType;

MObject DCNormal::aBaseVector;

MObject DCNormal::aTwistRamp;
MObject DCNormal::aTwistScale;


MObject DCNormal::aSeed;
MObject DCNormal::aRandomMin;
MObject DCNormal::aRandomMax;

MObject DCNormal::outDC;


void *DCNormal::creator()
{
    return ::dommy::maya::createObject("DCNormal", [&]() {
        return new DCNormal();
    });
}



MStatus DCNormal::compute(const MPlug& plug, MDataBlock& data) try
{
	
	MStatus stat;
	
	if (plug == outDC) {
		//        const bool VERBOSE = true;
		
#pragma mark get input dcurve
		const DCurveData* inputDCData = nullptr;
        DOMMY_RETURN_IF_FAILURE(getValidInputDC(inDC, data, inputDCData));
		
        if(inputDCData) {
            
            MFnPluginData pluginData;
            DCurveData* outDCrvs = nullptr;
            DOMMY_RETURN_IF_FAILURE(dommy::maya::createPluginData(
                pluginData, DCurveData::id, outDCrvs));
            if (!outDCrvs) return MS::kFailure;
            
            
            
            
            if (outDCrvs) {
                
                
                
                DOMMY_RETURN_IF_FAILURE(outDCrvs->copyFrom(*inputDCData));
                
                
                uint curveCount = outDCrvs->numberOfCurves();
                
                if(curveCount) {
                    
#pragma mark get attribute
                    
                    const MDataHandle generateTypeHandle =
                        data.inputValue(aGenerateType, &stat);
                    if (!stat) return stat;
                    const DC_NORMAL_GEN_TYPE genType =
                        generateTypeHandle.asShort();
                    if (genType < DC_NORMAL_VECTOR ||
                        genType >= DC_NORMAL_GEN_TYPE_COUNT) {
                        return MS::kInvalidParameter;
                    }

                    const MDataHandle baseVectorHandle =
                        data.inputValue(aBaseVector, &stat);
                    if (!stat) return stat;
                    const double3 &baseVectorValue =
                        baseVectorHandle.asDouble3();

                    const MDataHandle normalAttributeHandle =
                        data.inputValue(aNormalAttributeName, &stat);
                    if (!stat) return stat;
                    std::string normalAttrName =
                        normalAttributeHandle.asString().asChar();
                    
                    
                    const MDataHandle seedHandle =
                        data.inputValue(aSeed, &stat);
                    if (!stat) return stat;
                    const int seed = seedHandle.asInt();

                    const MDataHandle randomMinHandle =
                        data.inputValue(aRandomMin, &stat);
                    if (!stat) return stat;
                    const double3 &randMin = randomMinHandle.asDouble3();

                    const MDataHandle randomMaxHandle =
                        data.inputValue(aRandomMax, &stat);
                    if (!stat) return stat;
                    const double3 &randMax = randomMaxHandle.asDouble3();

                    for (unsigned int axis = 0; axis < 3; ++axis) {
                        if (!std::isfinite(baseVectorValue[axis]) ||
                            !std::isfinite(randMin[axis]) ||
                            !std::isfinite(randMax[axis])) {
                            return MS::kInvalidParameter;
                        }
                    }
                    
                    MPlug twistPlug( thisMObject(), aTwistRamp );
                    MRampAttribute twistRampAttr;
                    if (genType != DC_NORMAL_FROM_CURVE_TANGENT) {
                        twistRampAttr = MRampAttribute(twistPlug, &stat);
                        DOMMY_RETURN_IF_FAILURE(stat);
                    }
                    
                    const MDataHandle twistScaleHandle =
                        data.inputValue(aTwistScale, &stat);
                    if (!stat) return stat;
                    const double twistScale = twistScaleHandle.asDouble();
                    if (!std::isfinite(twistScale)) {
                        return MS::kInvalidParameter;
                    }
                    
                    
                    
                    if (normalAttrName.empty()) {
                        normalAttrName = "N";
                    }
                    
                    //check attribute
                    checkDCurvePointAttribute(normalAttrName, *outDCrvs, true);
                    
                    
                    size_t curveOffset=0;
                    for (auto cIt=outDCrvs->beginCurves(), eIt=outDCrvs->endCurves(); cIt!=eIt; ++cIt) {
                        
                        
                        //read curve
                        MObject crvObj;
                        stat = generateMCurveFromDcurve(*cIt, crvObj);
                        if (!stat) return stat;
                        MFnNurbsCurve crvFn(crvObj, &stat);
                        if (!stat) return stat;
                        
                        
                        const size_t cvCount = crvFn.numCVs(&stat);
                        if (!stat) return stat;
                        double totalLength = 0.0;
                        if (genType != DC_NORMAL_FROM_CURVE_TANGENT) {
                            totalLength = crvFn.length(kMFnNurbsEpsilon, &stat);
                            if (!stat) return stat;
                            if (!std::isfinite(totalLength) ||
                                totalLength <= kMFnNurbsEpsilon) {
                                return MS::kInvalidParameter;
                            }
                        }
                        
                        //twist and width
                        MFloatArray twistValueArray;
                        
                        
                        MVector oldBiNormal;
                        
                        MPointArray cvs;
                        stat = crvFn.getCVs(cvs, MSpace::kObject);
                        if (!stat) return stat;
                        if (cvs.length() != cvCount) {
                            return MS::kFailure;
                        }
                        
                        DVec3DataList calcnorms(cvCount,DVec3Data(0.0,0.0,0.0));
                        
                        std::vector<MVector> tangents(cvCount);
                        
                        for (size_t c=0; c<cvCount; c++) {
                            MPoint closest;
                            double curveParam = 0.0;
                            stat = dommy::curve::closestPointAndParameter(
                                crvFn, cvs[c], kMFnNurbsEpsilon,
                                MSpace::kObject, closest, curveParam);
                            if (!stat) return stat;
                            MVector tangent;
                            stat = dommy::curve::directionAtParameter(
                                crvFn, curveParam, false, MSpace::kObject, tangent);
                            if (!stat) return stat;
                            if (!dommy::curve::normalizeDirection(tangent)) {
                                return MS::kInvalidParameter;
                            }
                            tangents[c] = tangent;
                            // Tangent output does not need a normal, frame or twist ramp.
                            if (genType == DC_NORMAL_FROM_CURVE_TANGENT) {
                                calcnorms[c] = DVec3Data(tangent[0], tangent[1], tangent[2]);
                                continue;
                            }
                            const double len =
                                crvFn.findLengthFromParam(curveParam, &stat);
                            if (!stat) return stat;
                            if (!std::isfinite(len)) {
                                return MS::kInvalidParameter;
                            }
                            float pos = 0.0f;
                            if (!dommy::numeric::checkedFloatCast(len / totalLength, pos))
                                return MS::kInvalidParameter;
                            float tval = 0.0f;
                            if (!dommy::maya::sampleFiniteRamp(
                                    twistRampAttr, pos, tval)) {
                                return MS::kInvalidParameter;
                            }
                            stat = twistValueArray.append(tval);
                            if (!stat) return stat;
                            
                            // These modes only use the tangent as the twist axis.
                            if (genType == DC_NORMAL_VECTOR ||
                                genType == DC_NORMAL_FULL_RANDOM) {
                                continue;
                            }
                            MVector normal;
                            if (genType==DC_NORMAL_FROM_CURVE) {
                                normal= crvFn.normal( curveParam , MSpace::kObject , &stat );
                                if (!stat) return stat;
                            } else {
                                if (c==0) {
                                    normal= crvFn.normal( curveParam , MSpace::kObject , &stat );
                                    if (!stat) return stat;
                                } else {
                                    normal = oldBiNormal ^ tangent;
                                }
                            }
                            
                            
                            if (!dommy::curve::normalizeDirection(normal)) {
                                return MS::kInvalidParameter;
                            }
                            
                            calcnorms[c] = DVec3Data(normal[0],normal[1],normal[2]);
                            tangents[c] = tangent;
                            MVector biNormal((tangent ^ normal));//targetZ
                            if (!dommy::curve::normalizeDirection(biNormal)) {
                                return MS::kInvalidParameter;
                            }
                            
                            oldBiNormal = biNormal;
                        }
                        
                        
                        
                        DVec3DataList normals;
                        
                        
                        switch (genType) {
                            case DC_NORMAL_VECTOR:
                            {
                                if (!dommy::numeric::canRepresentAsFloat(baseVectorValue[0]) ||
                                    !dommy::numeric::canRepresentAsFloat(baseVectorValue[1]) ||
                                    !dommy::numeric::canRepresentAsFloat(baseVectorValue[2]))
                                    return MS::kInvalidParameter;
                                normals = DVec3DataList(
                                    cvCount,
                                    DVec3Data(
                                        baseVectorValue[0],
                                        baseVectorValue[1],
                                        baseVectorValue[2]));
                                
                                
                            }
                                break;
                            case DC_NORMAL_FROM_CURVE:
                            case DC_NORMAL_FROM_CURVE_TANGENT:
                            case DC_NORMAL_FROM_CURVE_AND_VECTOR:
                            {
                                normals = calcnorms;
                                
                            }
                                break;
                            case DC_NORMAL_FROM_CURVE_RANDOM:
                            {
                                DVec3DataList rotRands;
                                DGenerateRandomValue3(dommy::random::requireOffsetNonnegativeSeed(seed, curveOffset), randMin, randMax, (*cIt)->numberOfPoints(), rotRands);
                                if (rotRands.size() != cvCount) {
                                    return MS::kFailure;
                                }
                                
                                normals = DVec3DataList(cvCount,DVec3Data(0.0,0.0,0.0));
                                
                                size_t n = 0;
                                for (auto nIt=rotRands.begin(); nIt!=rotRands.end(); ++nIt) {
                                    
                                    MVector baseVec(calcnorms[n][0],calcnorms[n][1],calcnorms[n][2]);
                                    double rot[3] = {dommy::math::degreesToRadians((*nIt)[0]),dommy::math::degreesToRadians((*nIt)[1]),dommy::math::degreesToRadians((*nIt)[2])};
                                    baseVec = baseVec.rotateBy(rot, MTransformationMatrix::RotationOrder::kXYZ);
                                    if (!dommy::curve::normalizeDirection(baseVec))
                                        return MS::kInvalidParameter;
                                    
                                    normals[n++] = DVec3Data(baseVec[0],baseVec[1],baseVec[2]);
                                }
                            }
                                break;
                            case DC_NORMAL_FULL_RANDOM:
                            {
                                DVec3DataList rotRands;
                                DGenerateRandomValue3(dommy::random::requireOffsetNonnegativeSeed(seed, curveOffset), randMin, randMax, (*cIt)->numberOfPoints(), rotRands);
                                if (rotRands.size() != cvCount) {
                                    return MS::kFailure;
                                }
                                
                                normals = DVec3DataList(cvCount,DVec3Data(0.0,0.0,0.0));
                                
                                size_t n = 0;
                                for (auto nIt=rotRands.begin(); nIt!=rotRands.end(); ++nIt) {
                                    
                                    MVector randomBaseVector(
                                        baseVectorValue[0],
                                        baseVectorValue[1],
                                        baseVectorValue[2]);
                                    if (!dommy::curve::normalizeDirection(randomBaseVector))
                                        return MS::kInvalidParameter;
                                    double rot[3] = {dommy::math::degreesToRadians((*nIt)[0]),dommy::math::degreesToRadians((*nIt)[1]),dommy::math::degreesToRadians((*nIt)[2])};
                                    randomBaseVector =
                                        randomBaseVector.rotateBy(
                                            rot,
                                            MTransformationMatrix::RotationOrder::kXYZ);
                                    if (!dommy::curve::normalizeDirection(randomBaseVector)) {
                                        return MS::kInvalidParameter;
                                    }
                                    
                                    normals[n++] = DVec3Data(
                                        randomBaseVector[0],
                                        randomBaseVector[1],
                                        randomBaseVector[2]);
                                }
                                
                            }
                                break;
                            default:
                                break;
                        }
                        
                        
                        
                        if (genType!=DC_NORMAL_FROM_CURVE_TANGENT) {
                            //calc twist
                            size_t n=0;
                            for (auto nIt=normals.begin(); nIt!=normals.end(); ++nIt) {
                                
                                MVector nVec((*nIt)[0],(*nIt)[1],(*nIt)[2]);
                                MVector tanVec = tangents[n];
                                
                                const double rotation = dommy::math::degreesToRadians(
                                    twistValueArray[n] * twistScale);
                                if (!std::isfinite(rotation)) return MS::kInvalidParameter;
                                double cos_theta = cos(rotation);
                                double sin_theta = sin(rotation);
                                
                                MVector rotated = (nVec * cos_theta) + ((tanVec^nVec) * sin_theta) + (tanVec * (tanVec*nVec)) * (1 - cos_theta);
                                if (!dommy::maya::isFloatRepresentableVector(rotated))
                                    return MS::kInvalidParameter;
                                
                                (*nIt) = DVec3Data(rotated[0],rotated[1],rotated[2]);
                                
                                n++;
                            }
                        }
                        
                        
                        
                        DOMMY_RETURN_IF_FAILURE(DCPAddAttrVec3(
                            *cIt, true, false, normalAttrName, "", normals));
                        
                        curveOffset++;
                    }//finish loop curve
                }
                
                
                
                MStatus outputStatus;
                MDataHandle output = data.outputValue(outDC, &outputStatus);
                CHECK_MSTATUS_AND_RETURN_IT(outputStatus);
                DOMMY_RETURN_IF_FAILURE(
                    dommy::maya::publishTypedPluginDataObject<DCurveData>(
                        output, pluginData.object(), DCurveData::id));
                output.setClean();
                
                return data.setClean(plug);
                
            }
            
        }
        
	}
	
	return MS::kUnknownParameter;
}
DOMMY_COMPUTE_CATCH("DCNormal")

MStatus DCNormal::initialize() {
    MStatus stat;

    MFnNumericAttribute nAttr;
    MFnGenericAttribute gAttr;
    MFnTypedAttribute tAttr;
    MFnStringData fnStringData;

    inDC = tAttr.create("inDC", "idc", DCurveData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inDC));

    const MObject defaultNormalName = fnStringData.create("N", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aNormalAttributeName =
        tAttr.create("NormalAttributeName", "nrmattrn", MFnData::kString, defaultNormalName, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aNormalAttributeName));

    MFnEnumAttribute genEnum;
    aGenerateType = genEnum.create("GenerateType", "genty", 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(genEnum.addField("Base Vector", DC_NORMAL_VECTOR));
    DOMMY_RETURN_IF_FAILURE(genEnum.addField("From Curve Normal", DC_NORMAL_FROM_CURVE));
    DOMMY_RETURN_IF_FAILURE(genEnum.addField("From Curve Tangent", DC_NORMAL_FROM_CURVE_TANGENT));
    DOMMY_RETURN_IF_FAILURE(
        genEnum.addField("From Curve(use start normal calc)", DC_NORMAL_FROM_CURVE_AND_VECTOR));
    DOMMY_RETURN_IF_FAILURE(
        genEnum.addField("Random(from curve normal)", DC_NORMAL_FROM_CURVE_RANDOM));
    DOMMY_RETURN_IF_FAILURE(genEnum.addField("Full Random", DC_NORMAL_FULL_RANDOM));
    DOMMY_RETURN_IF_FAILURE(genEnum.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aGenerateType));

    aBaseVector = nAttr.create("BaseVector", "bvec", MFnNumericData::k3Double, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0, 1.0, 0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aBaseVector));

    aTwistRamp = MRampAttribute::createCurveRamp("TwistRamp", "twr", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(addAttribute(aTwistRamp));

    aTwistScale = nAttr.create("TwistScale", "twscl", MFnNumericData::kDouble, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(360.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aTwistScale));

    aSeed = nAttr.create("Seed", "se", MFnNumericData::kInt, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(1));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aSeed));

    aRandomMin = nAttr.create("RandomMin", "rmin", MFnNumericData::k3Double, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(-180.0, -180.0, -180.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aRandomMin));

    aRandomMax = nAttr.create("RandomMax", "rmax", MFnNumericData::k3Double, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(180.0, 180.0, 180.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aRandomMax));

    outDC = tAttr.create("outDC", "odc", DCurveData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(outDC));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(inDC, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aNormalAttributeName, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aGenerateType, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aBaseVector, outDC));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(aTwistRamp, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aTwistScale, outDC));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(aSeed, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aRandomMin, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aRandomMax, outDC));

    return MS::kSuccess;
}
