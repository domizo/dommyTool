//
//  LeafGen.cpp
//  dommyTool
//
//  Created by yuzo on 2017/09/06.
//

#include "common/utility/DMayaEntryPointGuard.h"
#include "LeafGen.h"
#include "common/utility/DMayaDefaultRamp.h"
#include "common/utility/DStatusOperation.h"

#include <maya/MDataBlock.h>
#include <maya/MDataHandle.h>
#include <maya/MFloatArray.h>
#include <maya/MFnData.h>
#include <maya/MFnEnumAttribute.h>
#include <maya/MFnGenericAttribute.h>
#include <maya/MFnMesh.h>
#include <maya/MFnMeshData.h>
#include <maya/MFnNumericAttribute.h>
#include <maya/MFnTypedAttribute.h>
#include <maya/MGlobal.h>
#include <maya/MIntArray.h>
#include <maya/MMatrix.h>
#include <maya/MPlug.h>
#include <maya/MPointArray.h>
#include <maya/MRampAttribute.h>
#include <maya/MTransformationMatrix.h>
#include <maya/MVector.h>

#include <algorithm>
#include <cmath>

#include "common/utility/DComputeGuard.h"
#include "common/utility/DCurveUVMapping.h"
#include "common/utility/DLeafDeformation.h"
#include "common/utility/DMayaRampSampling.h"
#include "common/utility/DProceduralMeshPlans.h"
#include "common/utility/DRandomValueGeneration.h"

#include "general_ids.h"


#include "perlin.h"

#include "dommyMath.h"


MTypeId LeafGen::typeId( ID_GENERAL_LEAF_GEN );


// Attributes
MObject LeafGen::aLeafLength;
MObject LeafGen::aLeafWidth;
MObject LeafGen::aLeafDepth;

MObject LeafGen::aLengthSampling;
MObject LeafGen::aWidthSampling;

MObject LeafGen::aUseWidthBias;

MObject LeafGen::aLeftRamp;
MObject LeafGen::aIsSymmetry;
MObject LeafGen::aRightRamp;

MObject LeafGen::aDepthWidthRamp;
MObject LeafGen::aDepthLengthRamp;


//noise
MObject LeafGen::aIsNoiseEdge;
MObject LeafGen::aSeed;
MObject LeafGen::aNoiseAmplitude;
MObject LeafGen::aNoiseFrequency;
MObject LeafGen::aNoiseWidthRamp;
MObject LeafGen::aNoiseLengthRamp;

//bend
MObject LeafGen::aBend;
MObject LeafGen::aBendRamp;



MObject LeafGen::outMesh;


void *LeafGen::creator()
{
    return ::dommy::maya::createObject("LeafGen", [&]() {
        return new LeafGen();
    });
}


void LeafGen::postConstructor() {
    dommy::maya::initializeNodeRamps("LeafGen", [&]() -> MStatus {
        DOMMY_RETURN_IF_FAILURE(dommy::maya::initializeDefaultValueRamp(
            thisMObject(), aLeftRamp, {0.0f, 1.0f, 0.0f}, {0.0f, 0.3f, 1.0f},
            {MRampAttribute::kSpline, MRampAttribute::kSpline, MRampAttribute::kSpline}));
        DOMMY_RETURN_IF_FAILURE(dommy::maya::initializeDefaultValueRamp(
            thisMObject(), aRightRamp, {0.0f, 1.0f, 0.0f}, {0.0f, 0.3f, 1.0f},
            {MRampAttribute::kSpline, MRampAttribute::kSpline, MRampAttribute::kSpline}));
        DOMMY_RETURN_IF_FAILURE(dommy::maya::initializeDefaultValueRamp(
            thisMObject(), aDepthWidthRamp, {0.0f, 1.0f, 0.0f}, {0.0f, 0.5f, 1.0f},
            {MRampAttribute::kSpline, MRampAttribute::kSpline, MRampAttribute::kSpline}));
        DOMMY_RETURN_IF_FAILURE(dommy::maya::initializeDefaultValueRamp(
            thisMObject(), aDepthLengthRamp, {0.0f, 1.0f, 0.0f}, {0.1f, 0.5f, 0.9f},
            {MRampAttribute::kSpline, MRampAttribute::kSpline, MRampAttribute::kSpline}));
        DOMMY_RETURN_IF_FAILURE(dommy::maya::initializeDefaultValueRamp(
            thisMObject(), aNoiseWidthRamp, {0.0f, 1.0f}, {0.0f, 1.0f},
            {MRampAttribute::kSpline, MRampAttribute::kSpline}));
        DOMMY_RETURN_IF_FAILURE(dommy::maya::initializeDefaultValueRamp(
            thisMObject(), aNoiseLengthRamp, {0.0f, 1.0f, 0.0f}, {0.1f, 0.5f, 0.9f},
            {MRampAttribute::kSpline, MRampAttribute::kSpline, MRampAttribute::kSpline}));
        DOMMY_RETURN_IF_FAILURE(dommy::maya::initializeDefaultValueRamp(
            thisMObject(), aBendRamp, {1.0f}, {0.0f}, {MRampAttribute::kSpline}));
        return MS::kSuccess;
    });
}


MStatus LeafGen::compute( const MPlug& plug, MDataBlock& data )
{
	try {
	    MStatus stat;
    
    if (plug != outMesh) return MS::kUnknownParameter;
        
        
        //get attribute
        const MDataHandle lengthHandle = data.inputValue(aLeafLength, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        const double length = lengthHandle.asDouble();
        const MDataHandle widthHandle = data.inputValue(aLeafWidth, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        const double width = widthHandle.asDouble();
        const MDataHandle depthHandle = data.inputValue(aLeafDepth, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        const double depth = depthHandle.asDouble();
        
        const MDataHandle lengthSamplingHandle = data.inputValue(aLengthSampling, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        const int lengthSampling = lengthSamplingHandle.asInt();
        const MDataHandle widthSamplingHandle = data.inputValue(aWidthSampling, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        const int widthSampling = widthSamplingHandle.asInt();
        
        const MDataHandle isWidthBiasHandle = data.inputValue(aUseWidthBias, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        const bool isWidthBias = isWidthBiasHandle.asBool();
        
        
        //shape ramp
        MRampAttribute leafRampAttr(
            MPlug(thisMObject(), aLeftRamp), &stat);
        CHECK_MSTATUS_AND_RETURN_IT(stat);
        
        const MDataHandle isSymmetryHandle = data.inputValue(aIsSymmetry, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        const bool isSymmetry = isSymmetryHandle.asBool();
        
        MRampAttribute rightRampAttr(
            MPlug(thisMObject(), aRightRamp), &stat);
        CHECK_MSTATUS_AND_RETURN_IT(stat);
        
        MRampAttribute depthRampAttr(
            MPlug(thisMObject(), aDepthWidthRamp), &stat);
        CHECK_MSTATUS_AND_RETURN_IT(stat);
        
        MRampAttribute depthLenRampAttr(
            MPlug(thisMObject(), aDepthLengthRamp), &stat);
        CHECK_MSTATUS_AND_RETURN_IT(stat);
        
        
        //noise attribute
        const MDataHandle isNoiseHandle = data.inputValue(aIsNoiseEdge, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        const bool isNoise = isNoiseHandle.asBool();
        const MDataHandle seedHandle = data.inputValue(aSeed, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        const int seed = seedHandle.asInt();
        const MDataHandle noiseAmpHandle = data.inputValue(aNoiseAmplitude, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        const double noiseAmp = noiseAmpHandle.asDouble();
        const MDataHandle noiseFreqHandle = data.inputValue(aNoiseFrequency, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        const double noiseFreq = noiseFreqHandle.asDouble();
        
        MRampAttribute noiseWRampAttr(
            MPlug(thisMObject(), aNoiseWidthRamp), &stat);
        CHECK_MSTATUS_AND_RETURN_IT(stat);
        
        MRampAttribute noiseLRampAttr(
            MPlug(thisMObject(), aNoiseLengthRamp), &stat);
        CHECK_MSTATUS_AND_RETURN_IT(stat);
        
        noise::module::Perlin noiseMod;
        noiseMod.SetSeed(seed);
        noiseMod.SetOctaveCount(1);
        noiseMod.SetFrequency(noiseFreq);
        noiseMod.SetLacunarity(2.0);
        noiseMod.SetPersistence(0.5);
        noiseMod.SetNoiseQuality(noise::QUALITY_BEST);
        
        
        
        //Bend
	        const MDataHandle bendHandle = data.inputValue(aBend, &stat);
	        DOMMY_RETURN_IF_FAILURE(stat);
	        const double bend = bendHandle.asDouble();
	        MRampAttribute brampAttr(
                MPlug(thisMObject(), aBendRamp), &stat);
            CHECK_MSTATUS_AND_RETURN_IT(stat);

            dommy::generation::LeafMeshPlan plan;
            int rightNoiseSeed = 0;
            if (!dommy::generation::buildLeafMeshPlan(
                    lengthSampling, widthSampling, plan) ||
                !dommy::random::nextNonnegativeSeed(seed, rightNoiseSeed) ||
                !std::isfinite(length) || !std::isfinite(width) ||
                !std::isfinite(depth) || !std::isfinite(noiseAmp) ||
                !std::isfinite(noiseFreq) || !std::isfinite(bend) ||
                length <= 0.0 || width <= 0.0 || noiseFreq < 0.0) {
                MGlobal::displayError(
                    "LeafGen: inputs exceed the finite mesh-generation limits.");
                return MS::kInvalidParameter;
            }
        
        
        
        
        
        
        const double baseHalfWidth = width*.5;
        
        
        
        //calc uv aspect
        const double aspect = 1.0/std::max(length, width);
        if (!std::isfinite(aspect)) {
            return MS::kInvalidParameter;
        }
        
        double offsetU = 0.5;
        double offsetV = 0.0;
        
        
        
        
        //polygon data init
        MPointArray pointArray;
        MIntArray faceCountArray, faceConnectArray;
        
        MFloatArray uCoords,vCoords;
        MIntArray uvCountsArray;
        MIntArray uvIdsArray;

        const unsigned int vertexCount =
            static_cast<unsigned int>(plan.vertexCount);
        const unsigned int faceCount =
            static_cast<unsigned int>(plan.faceCount);
        const unsigned int connectionCount =
            static_cast<unsigned int>(plan.connectionCount);
        DOMMY_RETURN_IF_FAILURE(pointArray.setLength(vertexCount));
        DOMMY_RETURN_IF_FAILURE(uCoords.setLength(vertexCount));
        DOMMY_RETURN_IF_FAILURE(vCoords.setLength(vertexCount));
        DOMMY_RETURN_IF_FAILURE(faceCountArray.setLength(faceCount));
        DOMMY_RETURN_IF_FAILURE(
            faceConnectArray.setLength(connectionCount));
        DOMMY_RETURN_IF_FAILURE(uvCountsArray.setLength(faceCount));
        DOMMY_RETURN_IF_FAILURE(uvIdsArray.setLength(connectionCount));

        std::size_t pointWriteIndex = 0U;
        
        
        for (int i=0; i<lengthSampling; i++) {
            
            
            //base shape calc
            double rowPosition = 0.0;
            if (!dommy::curve::pointUVPosition(i, plan.lengthSampleCount, rowPosition)) {
                return MS::kInvalidParameter;
            }
            const double lengthPosition = 1.0 - rowPosition;
            const double height = length * lengthPosition;
            float widthRatio = 0.0f;
            if (!dommy::maya::sampleFiniteRamp(
                    leafRampAttr, lengthPosition, widthRatio)) {
                return MS::kFailure;
            }
            double halfWidth = baseHalfWidth*widthRatio;
            double halfWidthFromHeight = isWidthBias?halfWidth:baseHalfWidth;
            double stratWidth = -halfWidth;
            double widthStep = halfWidth/(double)(widthSampling-1);
            
            float depthLengthRatio = 0.0f;
            if (!dommy::maya::sampleFiniteRamp(
                    depthLenRampAttr, lengthPosition, depthLengthRatio)) {
                return MS::kFailure;
            }
            
            
            
            //noise calc
            double noiseValue=noiseAmp;
            
            if (isNoise) {
                float noiseLengthRatio = 0.0f;
                if (!dommy::maya::sampleFiniteRamp(
                        noiseLRampAttr, lengthPosition, noiseLengthRatio)) {
                    return MS::kFailure;
                }
                
                noiseValue *=noiseLengthRatio;
            }
            if (!std::isfinite(halfWidth) ||
                !std::isfinite(halfWidthFromHeight) ||
                !std::isfinite(widthStep) || !std::isfinite(noiseValue)) {
                return MS::kInvalidParameter;
            }
            
            
            ////////////////////left Point
            for (int w=0; w<widthSampling; w++) {
                
                //calc depth
	                float dpethRatio = 0;
					float rampPosition = 0.0f;
					if (std::abs(halfWidthFromHeight) > 1.0e-12) {
						rampPosition = static_cast<float>(
							(widthStep * ((widthSampling - 1) - w)) /
							halfWidthFromHeight);
					}
	                if (!std::isfinite(rampPosition)) {
                        return MS::kInvalidParameter;
                    }
	                rampPosition = std::max(0.0f, std::min(1.0f, rampPosition));
                    if (!dommy::maya::sampleFiniteRamp(
                            depthRampAttr, rampPosition, dpethRatio)) {
                        return MS::kFailure;
                    }
                
                
                MPoint p(stratWidth+widthStep*w, height, depth*dpethRatio*depthLengthRatio);
                
                
                //uv coord
                //calc noise
                if (isNoise) {
                    float noiseDepthRatio = 0;
                    if (!dommy::maya::sampleFiniteRamp(
                            noiseWRampAttr, rampPosition, noiseDepthRatio)) {
                        return MS::kFailure;
                    }
                    
                    noiseMod.SetSeed(seed);
                    double n = noiseMod.GetValue(0.0, p.y/length, 0.0);
                    p.z += noiseValue*noiseDepthRatio*n;
                }
                
                
                if (pointWriteIndex >= plan.vertexCount ||
                    !std::isfinite(p.x) || !std::isfinite(p.y) ||
                    !std::isfinite(p.z)) {
                    return MS::kInvalidParameter;
                }
                pointArray[static_cast<unsigned int>(pointWriteIndex)] = p;
                uCoords[static_cast<unsigned int>(pointWriteIndex)] =
                    static_cast<float>(p.x * aspect + offsetU);
                vCoords[static_cast<unsigned int>(pointWriteIndex)] =
                    static_cast<float>(p.y * aspect + offsetV);
                ++pointWriteIndex;
            }
            
            
            ////////////////////check symmetry
            if (!isSymmetry) {
                float rightWidthRatio = 0.0f;
                if (!dommy::maya::sampleFiniteRamp(
                        rightRampAttr, lengthPosition, rightWidthRatio)) {
                    return MS::kFailure;
                }
                
                halfWidth = baseHalfWidth*rightWidthRatio;
                halfWidthFromHeight = isWidthBias?halfWidth:baseHalfWidth;
                widthStep = halfWidth/(double)(widthSampling-1);
                if (!std::isfinite(halfWidth) ||
                    !std::isfinite(halfWidthFromHeight) ||
                    !std::isfinite(widthStep)) {
                    return MS::kInvalidParameter;
                }
            }
            
            ////////////////////right point
            for(int w=1; w<widthSampling; w++) {
                
                
                //calc depth
	                float dpethRatio = 0;
					float rampPosition = 0.0f;
					if (std::abs(halfWidthFromHeight) > 1.0e-12) {
						rampPosition = static_cast<float>(
							(widthStep * w) / halfWidthFromHeight);
					}
	                if (!std::isfinite(rampPosition)) {
                        return MS::kInvalidParameter;
                    }
	                rampPosition = std::max(0.0f, std::min(1.0f, rampPosition));
                    if (!dommy::maya::sampleFiniteRamp(
                            depthRampAttr, rampPosition, dpethRatio)) {
                        return MS::kFailure;
                    }
                
                
                
                
                MPoint p(widthStep*w, height, depth*dpethRatio*depthLengthRatio);
                
                
                //uv coord
                //calc noise
                if (isNoise) {
                    float noiseDepthRatio = 0;
                    if (!dommy::maya::sampleFiniteRamp(
                            noiseWRampAttr, rampPosition, noiseDepthRatio)) {
                        return MS::kFailure;
                    }
                    
                    noiseMod.SetSeed(rightNoiseSeed);
                    double n = noiseMod.GetValue(0.0, p.y/length, 0.0);
                    p.z += noiseValue*noiseDepthRatio*n;
                }
                
                
                
                if (pointWriteIndex >= plan.vertexCount ||
                    !std::isfinite(p.x) || !std::isfinite(p.y) ||
                    !std::isfinite(p.z)) {
                    return MS::kInvalidParameter;
                }
                pointArray[static_cast<unsigned int>(pointWriteIndex)] = p;
                uCoords[static_cast<unsigned int>(pointWriteIndex)] =
                    static_cast<float>(p.x * aspect + offsetU);
                vCoords[static_cast<unsigned int>(pointWriteIndex)] =
                    static_cast<float>(p.y * aspect + offsetV);
                ++pointWriteIndex;
            }
            
            
            
            
            
            
            
            
            
        }

        if (pointWriteIndex != plan.vertexCount) return MS::kFailure;

        if (bend != 0.0) {
            dommy::generation::LeafBendTransform laterBends;
            // Build suffix transforms from the bottom row toward the tip.
            // Each vertex receives its complete historical bend exactly once.
            for (std::size_t row = plan.lengthSampleCount - 1U; row-- > 0U;) {
                double position = 0.0;
                double nextPosition = 0.0;
                if (!dommy::curve::pointUVPosition(row, plan.lengthSampleCount, position) ||
                    !dommy::curve::pointUVPosition(row + 1U, plan.lengthSampleCount, nextPosition)) {
                    return MS::kInvalidParameter;
                }
                float bendRatio = 0.0f;
                if (!dommy::maya::sampleFiniteRamp(brampAttr, 1.0 - position, bendRatio)) {
                    return MS::kFailure;
                }
                const double radians = dommy::math::degreesToRadians(
                    (bend * bendRatio) / static_cast<double>(plan.lengthSampleCount));
                const double pivotY = length * (1.0 - nextPosition);
                if (!dommy::generation::prependLeafBendStep(radians, pivotY, laterBends)) {
                    return MS::kInvalidParameter;
                }
                const std::size_t first = row * plan.pointsPerRow;
                for (std::size_t column = 0U; column < plan.pointsPerRow; ++column) {
                    MPoint& point = pointArray[static_cast<unsigned int>(first + column)];
                    if (!dommy::generation::applyLeafBend(
                            laterBends, point.y, point.z, point.y, point.z)) {
                        return MS::kInvalidParameter;
                    }
                }
            }
        }

        // Face and UV connectivity use the same per-vertex UV indices.
        
        
        const size_t faceOffset = plan.pointsPerRow;
        std::size_t faceWriteIndex = 0U;
        std::size_t connectionWriteIndex = 0U;
        for (std::size_t i=0U; i+1U<plan.lengthSampleCount; ++i) {
            
            for (std::size_t w=0U; w+1U<plan.pointsPerRow; ++w) {
                size_t startId =faceOffset*i+w;
                if (faceWriteIndex >= plan.faceCount ||
                    connectionWriteIndex + 4U > plan.connectionCount) {
                    return MS::kFailure;
                }
                faceCountArray[static_cast<unsigned int>(faceWriteIndex)] = 4;
                uvCountsArray[static_cast<unsigned int>(faceWriteIndex)] = 4;
                ++faceWriteIndex;
                const int ids[4] = {
                    static_cast<int>(startId+1U),
                    static_cast<int>(startId),
                    static_cast<int>(startId+faceOffset),
                    static_cast<int>(startId+1U+faceOffset)};
                for (std::size_t corner=0U; corner<4U; ++corner) {
                    faceConnectArray[static_cast<unsigned int>(
                        connectionWriteIndex)] = ids[corner];
                    uvIdsArray[static_cast<unsigned int>(
                        connectionWriteIndex)] = ids[corner];
                    ++connectionWriteIndex;
                }
                
            }
            
        }
        
        if (faceWriteIndex != plan.faceCount ||
            connectionWriteIndex != plan.connectionCount) {
            return MS::kFailure;
        }
        
        
        
        MFnMeshData meshDataFn;
        MObject newOutMeshData = meshDataFn.create(&stat);
        CHECK_MSTATUS_AND_RETURN_IT(stat);
        if (newOutMeshData.isNull()) return MS::kFailure;
        
        MFnMesh newMeshFn;
        const MObject meshObject = newMeshFn.create(
            static_cast<int>(plan.vertexCount),
            static_cast<int>(plan.faceCount), pointArray, faceCountArray,
            faceConnectArray, uCoords, vCoords, newOutMeshData, &stat);
        CHECK_MSTATUS_AND_RETURN_IT(stat);
        if (meshObject.isNull()) return MS::kFailure;
        stat = newMeshFn.assignUVs(uvCountsArray,uvIdsArray);
        CHECK_MSTATUS_AND_RETURN_IT(stat);
        
        MDataHandle outputMeshHnd = data.outputValue(outMesh, &stat);
        CHECK_MSTATUS_AND_RETURN_IT(stat);
        DOMMY_RETURN_IF_FAILURE(outputMeshHnd.set(newOutMeshData));
        outputMeshHnd.setClean();
        return data.setClean(plug);
	} DOMMY_COMPUTE_CATCH("LeafGen")
}


MStatus LeafGen::initialize() try
{
    MFnNumericAttribute nAttr;
    MFnTypedAttribute   tAttr;
    MFnGenericAttribute gAttr;
    MFnEnumAttribute eAttr;
    
    MStatus                stat;
    
    
    aLeafLength = nAttr.create( "LeafLength", "llen", MFnNumericData::kDouble, 0.0, &stat );
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 10.0 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin( 0.1 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax( 50.0 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin( 0.01 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aLeafLength));
    
    aLeafWidth = nAttr.create( "LeafWidth", "lw", MFnNumericData::kDouble, 0.0, &stat );
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 6.0 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin( 0.1 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax( 50.0 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin( 0.01 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aLeafWidth));
    
    
    aLeafDepth = nAttr.create( "LeafDepth", "ld", MFnNumericData::kDouble, 0.0, &stat );
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 0.5 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin( 0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax( 5.0 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aLeafDepth));
    
    
    aLengthSampling = nAttr.create( "LengthSampling", "lensmp", MFnNumericData::kInt, 0.0, &stat );
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 8 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin( 3 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aLengthSampling));
    
    aWidthSampling= nAttr.create( "WidthSampling", "wsmp", MFnNumericData::kInt, 0.0, &stat );
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 3 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(2));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aWidthSampling));
    
    
    
    aUseWidthBias = nAttr.create( "UseWidthBias", "uswbs", MFnNumericData::kBoolean, 0.0, &stat );
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( true ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aUseWidthBias));
    
    
    
    
    
    
    //shape ramp
    aLeftRamp = MRampAttribute::createCurveRamp("LeftRamp", "lrmp", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(addAttribute(aLeftRamp));
    
    aIsSymmetry = nAttr.create( "IsSymmetry", "issym", MFnNumericData::kBoolean, 0.0, &stat );
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( true ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aIsSymmetry));
    
    aRightRamp = MRampAttribute::createCurveRamp("RightRamp", "rrmp", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(addAttribute(aRightRamp));
    
    aDepthWidthRamp = MRampAttribute::createCurveRamp("DepthWidthRamp", "dwrmp", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(addAttribute(aDepthWidthRamp));
    
    aDepthLengthRamp = MRampAttribute::createCurveRamp("DepthLengthRamp", "dlrmp", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(addAttribute(aDepthLengthRamp));
    
    
    //noise
    aIsNoiseEdge = nAttr.create( "IsNoiseEdge", "isno", MFnNumericData::kBoolean, 0.0, &stat );
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( false ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aIsNoiseEdge));
    
    
    aSeed = nAttr.create( "Seed", "se", MFnNumericData::kInt, 0.0, &stat );
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aSeed));
    
    
    aNoiseAmplitude = nAttr.create( "NoiseAmplitude", "namp", MFnNumericData::kDouble, 0.0, &stat );
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 0.5 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin( 0.0 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax( 5.0 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aNoiseAmplitude));
    
    
    aNoiseFrequency = nAttr.create( "NoiseFrequency", "nfreq", MFnNumericData::kDouble, 0.0, &stat );
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(2.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aNoiseFrequency));
    
    
    aNoiseWidthRamp = MRampAttribute::createCurveRamp("NoiseWidthRamp", "nwramp", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(addAttribute(aNoiseWidthRamp));
    
    aNoiseLengthRamp = MRampAttribute::createCurveRamp("NoiseLengthRamp", "nlramp", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(addAttribute(aNoiseLengthRamp));
    
    
    
    
    //Bend
    aBend = nAttr.create( "Bend", "b", MFnNumericData::kDouble, 0.0, &stat );
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault( 0 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin( -180.0 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax( 180.0 ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aBend));
    
    
    
    aBendRamp = MRampAttribute::createCurveRamp("BendRamp", "bndramp", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(addAttribute(aBendRamp));
    
    
    ///output
    outMesh = tAttr.create("outMesh", "om", MFnData::kMesh, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setReadable(true));
    DOMMY_RETURN_IF_FAILURE(tAttr.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(outMesh));
    
    
    
    
    
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aLeafLength, outMesh ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aLeafWidth, outMesh ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aLeafDepth, outMesh ));
    
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aLengthSampling, outMesh ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aWidthSampling, outMesh ));
    
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aUseWidthBias, outMesh ));
    
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aLeftRamp, outMesh ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aIsSymmetry, outMesh ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aRightRamp, outMesh ));
    
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aDepthWidthRamp, outMesh ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aDepthLengthRamp, outMesh ));
    
    
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aIsNoiseEdge, outMesh ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aSeed, outMesh ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aNoiseAmplitude, outMesh ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aNoiseFrequency, outMesh ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aNoiseWidthRamp, outMesh ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aNoiseLengthRamp, outMesh ));
    
    
    
    
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aBend, outMesh ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aBendRamp, outMesh ));
    
    
    return MS::kSuccess;
}
catch (...) {
    return dommy::maya::reportEntryPointException(
        "LeafGen", "initialize", std::current_exception());
}
