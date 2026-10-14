//
//  DPLightGen.cpp
//  dommyTool
//
//  Created by yuzo on 11/16/16.
//
//

#include "DPLightGen.h"
#include "common/utility/DStatusOperation.h"
#include "common/string/DStreamFormat.h"
#include <locale>
#include "common/utility/DMayaPlugDataHandle.h"
#include "PluginFeatures.h"
#include "common/file/DExportPath.h"
#include "common/utility/DMayaEntryPointGuard.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DViewportCallbackGuard.h"
#include "common/utility/DMayaValueValidation.h"
#include "common/utility/DMayaCheckedArray.h"
#include "DArnoldSession.h"
#include "DEvaluationContext.h"
#include "integrations/arnold/DArnoldResources.h"
#include "integrations/arnold/DArnoldAssWrite.h"
#include "integrations/arnold/DArnoldNumeric.h"


//maya
#include "DMayaHeaders.h"

//DPoint
#include "DPointDataHeaders.h"
#include "modules/point/DPoint/Common/DPAttributeAccess.h"




#include "DNodeOperation.h"
#include "DFileOperation.h"

#include "dommyMath.h"




#include <vector>
#include <cmath>
#include <sstream>
#include <string>
#include <limits>
#include <memory>
#include <utility>


#include <ai.h>







MTypeId DPLightGen::typeId( ID_DP_LIGHT_GEN );
const MString DPLightGen::typeName("DPLightGen");
MString DPLightGen::classification("drawdb/geometry/DPLightGen");
MString	DPLightGen::registrantId("DPLightGenPlugin");


MObject DPLightGen::inDP;
MObject DPLightGen::aNodeInfo;

MObject DPLightGen::aFilePath;
MObject DPLightGen::aExportMode;
MObject DPLightGen::aExportLog;

MObject DPLightGen::aLightVisType;
MObject DPLightGen::aColorVis;
MObject DPLightGen::aIntensityVis;
MObject DPLightGen::aIntensityNormalize;
MObject DPLightGen::aPointSize;

//type
MObject DPLightGen::aLightType;

//diatant
MObject DPLightGen::aDistantAngle;

//spot
MObject DPLightGen::aLensRadius;
MObject DPLightGen::aConeAngle;
MObject DPLightGen::aPenumbraAngle;
MObject DPLightGen::aCosinePower;
MObject DPLightGen::aAspectRatio;

MObject DPLightGen::aUseGobo;
MObject DPLightGen::aGoboTexture;
MObject DPLightGen::aGoboFilter;
MObject DPLightGen::aGoboDensity;
MObject DPLightGen::aGoboOffset;
MObject DPLightGen::aGoboScale;
MObject DPLightGen::aGoboWrap;

//area quad cylinder disk
MObject DPLightGen::aResolution;
MObject DPLightGen::aSpread; //disk
MObject DPLightGen::aLightTexture;
MObject DPLightGen::aRepeatTex;
MObject DPLightGen::aOffsetTex;

//photometric
MObject DPLightGen::aPhotometricFileName;

//common
MObject DPLightGen::aIntensity;
MObject DPLightGen::aExposure;
MObject DPLightGen::aColor;

MObject DPLightGen::aDecayType;
MObject DPLightGen::aSamples;
MObject DPLightGen::aLightRadius;
MObject DPLightGen::aNormalize;


MObject DPLightGen::aCastShadow;
MObject DPLightGen::aShadowColor;
MObject DPLightGen::aShadowDensity;

MObject DPLightGen::aAffectVolumetrics;
MObject DPLightGen::aCastVolumeShadow;
MObject DPLightGen::aVolumeSamples;

MObject DPLightGen::aAffectDiffuse;
MObject DPLightGen::aAffectSpecular;
MObject DPLightGen::aDiffuse;
MObject DPLightGen::aSpecular;
MObject DPLightGen::aSss;
MObject DPLightGen::aIndirect;
MObject DPLightGen::aVolume;

MObject DPLightGen::aMaxBounces;

MObject DPLightGen::aAOVLightGroup;

//direction
MObject DPLightGen::aDirectionType;
MObject DPLightGen::aBaseDirection;
MObject DPLightGen::aDirectionAttribute;

//attribute inherit
MObject DPLightGen::aIntensityAttribute;
MObject DPLightGen::aColorAttribute;
MObject DPLightGen::aSpotAngleAttribute;
MObject DPLightGen::aScaleAttribute;
MObject DPLightGen::aAnimMatrixAttribute;

//animation
MObject DPLightGen::aApplyAnimMatType;
MObject DPLightGen::aAnimLookPos;

//transform
MObject DPLightGen::aGlobalScale;




namespace {

MBoundingBox lightFallbackBounds()
{
    return MBoundingBox(MPoint(-1.0, -1.0, -1.0), MPoint(1.0, 1.0, 1.0));
}

bool validLightDrawData(const DPLightGenData& data)
{
    const unsigned int lightCount = data.matrixArray.length();
    const bool needsSpotAngles = data.lightVisType == 2 && data.lightType == 2;
    if (lightCount == 0U || data.lightType < 0 || data.lightType > 6 ||
        data.lightVisType < 0 || data.lightVisType > 2 ||
        !std::isfinite(data.pointSize) || data.pointSize <= 0.0f ||
        (data.colorVis && data.colorArray.length() != lightCount) ||
        (data.intensityVis && data.intensityArray.length() != lightCount) ||
        (needsSpotAngles && data.spotAngleArray.length() != lightCount)) {
        return false;
    }

    for (unsigned int index = 0U; index < lightCount; ++index) {
        if (!dommy::maya::isFiniteMatrix(data.matrixArray[index])) {
            return false;
        }
        if (data.colorVis &&
            !dommy::maya::isFloatRepresentableVector(data.colorArray[index])) {
            return false;
        }
        float halfAngle = 0.0f;
        if (needsSpotAngles && !dommy::numeric::checkedFloatCast(
                data.spotAngleArray[index] * 0.5, halfAngle)) {
            return false;
        }
    }
    return true;
}

// Stage transformed coordinates and observe Maya allocation/copy statuses.
// A finite matrix alone does not guarantee finite, float-representable points.
MStatus transformLightPoints(const MPointArray& source, const MMatrix& matrix,
                             MPointArray& output)
{
    return dommy::maya::buildCheckedArray(
        source.length(),
        [&source, &matrix](unsigned int index, MPoint& point) -> MStatus {
            point = source[index] * matrix;
            return dommy::maya::isFloatRepresentablePoint(point)
                ? MStatus(MS::kSuccess) : MStatus(MS::kInvalidParameter);
        }, output);
}

// Invalid intensity controls omit only the alpha modulation, not the light.
// Retain the historical float base-value rounding and the 0.01 alpha floor.
bool lightIntensityAlpha(double intensity, double normalization, float& alpha)
{
    if (!std::isfinite(intensity) || !std::isfinite(normalization) ||
        normalization <= 0.0) {
        return false;
    }
    if (intensity >= normalization) {
        alpha = 0.01f;
        return true;
    }
    // Bound a negative ratio before division, including tiny positive divisors.
    if (intensity < 0.0 && normalization <
            -intensity / static_cast<double>(std::numeric_limits<float>::max())) {
        return false;
    }

    float baseValue = 0.0f;
    if (!dommy::numeric::checkedFloatCast(
            1.0 - intensity / normalization, baseValue)) {
        return false;
    }
    return dommy::numeric::checkedFloatCast(
        0.01 + static_cast<double>(baseValue), alpha);
}

} // namespace

DPLightGen::DPLightGen() {}
DPLightGen::~DPLightGen() {}

void* DPLightGen::creator()
{
    return ::dommy::maya::createObject("DPLightGen", [&]() {
        return new DPLightGen();
    });
}



MStatus DPLightGen::exportArnoldLight(
	MDataBlock& data,
	const DPData* inputDPData,
	const std::string& filename) {
	

	
	
	
	//get point
	const DPointList pts = inputDPData->getPositionConst();
	if (pts.size() > std::numeric_limits<unsigned int>::max()) {
		MGlobal::displayWarning("DPLightGen: point count exceeds Maya array limits.");
		return MS::kFailure;
	}
	MStatus status;
	
	//get attribute
    const MDataHandle aExportLogHandle = data.inputValue(aExportLog, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const bool exportLog = aExportLogHandle.asBool();
	const MDataHandle aLightTypeHandle = data.inputValue(aLightType, &status);
	DOMMY_RETURN_IF_FAILURE(status);
	const short lightTypeVal = aLightTypeHandle.asShort();
	//distant
	const MDataHandle aDistantAngleHandle = data.inputValue(aDistantAngle, &status);
	DOMMY_RETURN_IF_FAILURE(status);
	const double distAngleVal = aDistantAngleHandle.asDouble();
	
	//spot
	const MDataHandle aLensRadiusHandle = data.inputValue(aLensRadius, &status);
	DOMMY_RETURN_IF_FAILURE(status);
	const double lensRadiusVal = aLensRadiusHandle.asDouble();
	const MDataHandle aConeAngleHandle = data.inputValue(aConeAngle, &status);
	DOMMY_RETURN_IF_FAILURE(status);
	const double coneAngleVal = aConeAngleHandle.asDouble();
	const MDataHandle aPenumbraAngleHandle = data.inputValue(aPenumbraAngle, &status);
	DOMMY_RETURN_IF_FAILURE(status);
	const double penumbraAngleVal = aPenumbraAngleHandle.asDouble();
	const MDataHandle aCosinePowerHandle = data.inputValue(aCosinePower, &status);
	DOMMY_RETURN_IF_FAILURE(status);
	const double cosinePowerVal = aCosinePowerHandle.asDouble();
	const MDataHandle aAspectRatioHandle = data.inputValue(aAspectRatio, &status);
	DOMMY_RETURN_IF_FAILURE(status);
	const double aspectRatioVal = aAspectRatioHandle.asDouble();
	
	//spot gobo
	const MDataHandle aUseGoboHandle = data.inputValue(aUseGobo, &status);
	DOMMY_RETURN_IF_FAILURE(status);
	const bool useGoboVal = aUseGoboHandle.asBool();
	const MDataHandle aGoboTextureHandle = data.inputValue(aGoboTexture, &status);
	DOMMY_RETURN_IF_FAILURE(status);
	const std::string goboTexName = aGoboTextureHandle.asString().asChar();
	const MDataHandle aGoboFilterHandle = data.inputValue(aGoboFilter, &status);
	DOMMY_RETURN_IF_FAILURE(status);
	const short goboFilterTypeVal = aGoboFilterHandle.asShort();
	const MDataHandle aGoboDensityHandle = data.inputValue(aGoboDensity, &status);
	DOMMY_RETURN_IF_FAILURE(status);
	const double goboDensityVal = aGoboDensityHandle.asDouble();
	const MDataHandle goboOffsetHandle =
		data.inputValue(aGoboOffset, &status);
	if (!status) return status;
	const MDataHandle goboScaleHandle =
		data.inputValue(aGoboScale, &status);
	if (!status) return status;
	const double2& goboOffsetVal = goboOffsetHandle.asDouble2();
	const double2& goboScaleVal = goboScaleHandle.asDouble2();
	const MDataHandle aGoboWrapHandle = data.inputValue(aGoboWrap, &status);
	DOMMY_RETURN_IF_FAILURE(status);
	const short goboWrapTypeVal = aGoboWrapHandle.asShort();
	

	
	//area quad cylinder disk
	const MDataHandle aResolutionHandle = data.inputValue(aResolution, &status);
	DOMMY_RETURN_IF_FAILURE(status);
	const int resolutionVal = aResolutionHandle.asInt();
	const MDataHandle aSpreadHandle = data.inputValue(aSpread, &status);
	DOMMY_RETURN_IF_FAILURE(status);
	const double spreadVal = aSpreadHandle.asDouble();
	const MDataHandle aLightTextureHandle = data.inputValue(aLightTexture, &status);
	DOMMY_RETURN_IF_FAILURE(status);
	const std::string lightTextureName = aLightTextureHandle.asString().asChar();
	const MDataHandle repeatTextureHandle =
		data.inputValue(aRepeatTex, &status);
	if (!status) return status;
	const MDataHandle offsetTextureHandle =
		data.inputValue(aOffsetTex, &status);
	if (!status) return status;
	const double2& repeatTexVal = repeatTextureHandle.asDouble2();
	const double2& offsetTexVal = offsetTextureHandle.asDouble2();
	
	
	//photometric
	const MDataHandle aPhotometricFileNameHandle = data.inputValue(aPhotometricFileName, &status);
	DOMMY_RETURN_IF_FAILURE(status);
	const std::string photometricFileName = aPhotometricFileNameHandle.asString().asChar();
	
	//common
	const MDataHandle aIntensityHandle = data.inputValue(aIntensity, &status);
	DOMMY_RETURN_IF_FAILURE(status);
	const double intensity = aIntensityHandle.asDouble();
	const MDataHandle aExposureHandle = data.inputValue(aExposure, &status);
	DOMMY_RETURN_IF_FAILURE(status);
	const double exposure = aExposureHandle.asDouble();
	const MDataHandle aColorHandle = data.inputValue(aColor, &status);
	DOMMY_RETURN_IF_FAILURE(status);
	MVector colorVal = aColorHandle.asFloatVector();
	
	const MDataHandle aDecayTypeHandle = data.inputValue(aDecayType, &status);
	DOMMY_RETURN_IF_FAILURE(status);
	const short decayTypeVal = aDecayTypeHandle.asShort();
	const MDataHandle aSamplesHandle = data.inputValue(aSamples, &status);
	DOMMY_RETURN_IF_FAILURE(status);
	const int sampleVal = aSamplesHandle.asInt();
	const MDataHandle aLightRadiusHandle = data.inputValue(aLightRadius, &status);
	DOMMY_RETURN_IF_FAILURE(status);
	const double lightRadiusVal = aLightRadiusHandle.asDouble();
	const MDataHandle aNormalizeHandle = data.inputValue(aNormalize, &status);
	DOMMY_RETURN_IF_FAILURE(status);
	const bool normalizeVal = aNormalizeHandle.asBool();
	
	const MDataHandle aCastShadowHandle = data.inputValue(aCastShadow, &status);
	DOMMY_RETURN_IF_FAILURE(status);
	const bool castShadowVal = aCastShadowHandle.asBool();
	const MDataHandle aShadowColorHandle = data.inputValue(aShadowColor, &status);
	DOMMY_RETURN_IF_FAILURE(status);
	MVector shadowColorVal = aShadowColorHandle.asFloatVector();
	const MDataHandle aShadowDensityHandle = data.inputValue(aShadowDensity, &status);
	DOMMY_RETURN_IF_FAILURE(status);
	const double shadowDensityVal = aShadowDensityHandle.asDouble();
	
	const MDataHandle aAffectVolumetricsHandle = data.inputValue(aAffectVolumetrics, &status);
	DOMMY_RETURN_IF_FAILURE(status);
	const bool affectVolumetricsVal = aAffectVolumetricsHandle.asBool();
	const MDataHandle aCastVolumeShadowHandle = data.inputValue(aCastVolumeShadow, &status);
	DOMMY_RETURN_IF_FAILURE(status);
	const bool castVolumeShadowVal = aCastVolumeShadowHandle.asBool();
	const MDataHandle aVolumeSamplesHandle = data.inputValue(aVolumeSamples, &status);
	DOMMY_RETURN_IF_FAILURE(status);
	const int volumeSamplesVal = aVolumeSamplesHandle.asInt();
	
	const MDataHandle aAffectDiffuseHandle = data.inputValue(aAffectDiffuse, &status);
	DOMMY_RETURN_IF_FAILURE(status);
	const bool affectDiffuseVal = aAffectDiffuseHandle.asBool();
	const MDataHandle aAffectSpecularHandle = data.inputValue(aAffectSpecular, &status);
	DOMMY_RETURN_IF_FAILURE(status);
	const bool affectSpecularVal = aAffectSpecularHandle.asBool();
	const MDataHandle aDiffuseHandle = data.inputValue(aDiffuse, &status);
	DOMMY_RETURN_IF_FAILURE(status);
	const double diffuseVal = aDiffuseHandle.asDouble();
	const MDataHandle aSpecularHandle = data.inputValue(aSpecular, &status);
	DOMMY_RETURN_IF_FAILURE(status);
	const double specularVal = aSpecularHandle.asDouble();
	const MDataHandle aSssHandle = data.inputValue(aSss, &status);
	DOMMY_RETURN_IF_FAILURE(status);
	const double sssVal = aSssHandle.asDouble();
	const MDataHandle aIndirectHandle = data.inputValue(aIndirect, &status);
	DOMMY_RETURN_IF_FAILURE(status);
	const double indirectVal = aIndirectHandle.asDouble();
	const MDataHandle aVolumeHandle = data.inputValue(aVolume, &status);
	DOMMY_RETURN_IF_FAILURE(status);
	const double volumeVal = aVolumeHandle.asDouble();
	
	//mac bounce
	const MDataHandle aMaxBouncesHandle = data.inputValue(aMaxBounces, &status);
	DOMMY_RETURN_IF_FAILURE(status);
	const int maxBouncesVal = aMaxBouncesHandle.asInt();
	
	//aov
	const MDataHandle aAOVLightGroupHandle = data.inputValue(aAOVLightGroup, &status);
	DOMMY_RETURN_IF_FAILURE(status);
	const std::string aovLightGroupName = aAOVLightGroupHandle.asString().asChar();
	
	//direction
	const MDataHandle aDirectionTypeHandle = data.inputValue(aDirectionType, &status);
	DOMMY_RETURN_IF_FAILURE(status);
	const short dirTypeVal = aDirectionTypeHandle.asShort();
	const MDataHandle baseDirectionHandle =
		data.inputValue(aBaseDirection, &status);
	if (!status) return status;
	const double3& baseVecVal = baseDirectionHandle.asDouble3();
	const MDataHandle aDirectionAttributeHandle = data.inputValue(aDirectionAttribute, &status);
	DOMMY_RETURN_IF_FAILURE(status);
	const std::string directionAttributeName = aDirectionAttributeHandle.asString().asChar();
	
	//attribute inherit
	const MDataHandle aIntensityAttributeHandle = data.inputValue(aIntensityAttribute, &status);
	DOMMY_RETURN_IF_FAILURE(status);
	const std::string intensityAttributeName = aIntensityAttributeHandle.asString().asChar();
	const MDataHandle aColorAttributeHandle = data.inputValue(aColorAttribute, &status);
	DOMMY_RETURN_IF_FAILURE(status);
	const std::string colorAttributeName = aColorAttributeHandle.asString().asChar();
	const MDataHandle aSpotAngleAttributeHandle = data.inputValue(aSpotAngleAttribute, &status);
	DOMMY_RETURN_IF_FAILURE(status);
	const std::string spotAngleAttributeName = aSpotAngleAttributeHandle.asString().asChar();
	const MDataHandle aScaleAttributeHandle = data.inputValue(aScaleAttribute, &status);
	DOMMY_RETURN_IF_FAILURE(status);
	const std::string scaleAttributeName = aScaleAttributeHandle.asString().asChar();
	const MDataHandle aAnimMatrixAttributeHandle = data.inputValue(aAnimMatrixAttribute, &status);
	DOMMY_RETURN_IF_FAILURE(status);
	const std::string animMatAttributeName = aAnimMatrixAttributeHandle.asString().asChar();
	
	
	//animation
	const MDataHandle aApplyAnimMatTypeHandle = data.inputValue(aApplyAnimMatType, &status);
	DOMMY_RETURN_IF_FAILURE(status);
	const int applyAnimMatType = aApplyAnimMatTypeHandle.asInt();
	const MDataHandle lookAtHandle =
		data.inputValue(aAnimLookPos, &status);
	if (!status) return status;
	const double3& lookAtVal = lookAtHandle.asDouble3();
	MVector lookAtVec(lookAtVal[0],lookAtVal[1],lookAtVal[2]);
	

	
#pragma mark read attribute
	
	
	
	MDoubleArray intensityArray(pts.size(), intensity);
	if (!intensityAttributeName.empty() && inputDPData->isExistAttr(intensityAttributeName)) {
        if (!getCheckedMRealArrayFromAttribute(
                *inputDPData, intensityAttributeName, pts.size(), intensityArray)) {
            MGlobal::displayError("DPLightGen: unable to read the numeric intensity attribute.");
            return MS::kFailure;
        }
	}
	
	MVectorArray colorArray(pts.size(), colorVal);
	if (!colorAttributeName.empty() && inputDPData->isExistAttr(colorAttributeName)) {
        if (!getCheckedMVectorArrayFromAttribute(
                *inputDPData, colorAttributeName, pts.size(), colorArray)) {
            MGlobal::displayError("DPLightGen: unable to read the Vec3 color attribute.");
            return MS::kFailure;
        }
	}
	
	MDoubleArray spotAngleArray(pts.size(), coneAngleVal);
	if (!spotAngleAttributeName.empty() && inputDPData->isExistAttr(spotAngleAttributeName)) {
        if (!getCheckedMRealArrayFromAttribute(
                *inputDPData, spotAngleAttributeName, pts.size(), spotAngleArray)) {
            MGlobal::displayError("DPLightGen: unable to read the numeric cone-angle attribute.");
            return MS::kFailure;
        }
	}
	
	MDoubleArray scaleArray(pts.size(), 1.0);
	if (!scaleAttributeName.empty() && inputDPData->isExistAttr(scaleAttributeName)) {
        if (!getCheckedMRealArrayFromAttribute(
                *inputDPData, scaleAttributeName, pts.size(), scaleArray)) {
            MGlobal::displayError("DPLightGen: unable to read the numeric scale attribute.");
            return MS::kFailure;
        }
	}
	
	MMatrixArray animMatArray(pts.size());
	if (!animMatAttributeName.empty() && inputDPData->isExistAttr(animMatAttributeName)) {
        if (!getCheckedMMatrixArrayFromAttribute(
                *inputDPData, animMatAttributeName, pts.size(), animMatArray)) {
            MGlobal::displayError("DPLightGen: unable to read the Matrix animation attribute.");
            return MS::kFailure;
        }
	}
	
	
	
#pragma mark read direction for transform
	
	//transform
	const MDataHandle globalScaleHandle =
		data.inputValue(aGlobalScale, &status);
	if (!status) return status;
	const double3& gScaleVal = globalScaleHandle.asDouble3();

	const double scalarValues[] = {
		distAngleVal, lensRadiusVal, coneAngleVal, penumbraAngleVal,
		cosinePowerVal, aspectRatioVal, goboDensityVal,
		goboOffsetVal[0], goboOffsetVal[1],
		goboScaleVal[0], goboScaleVal[1], spreadVal,
		repeatTexVal[0], repeatTexVal[1],
		offsetTexVal[0], offsetTexVal[1],
		intensity, exposure, lightRadiusVal, shadowDensityVal,
		diffuseVal, specularVal, sssVal, indirectVal, volumeVal,
		baseVecVal[0], baseVecVal[1], baseVecVal[2],
		lookAtVal[0], lookAtVal[1], lookAtVal[2],
		gScaleVal[0], gScaleVal[1], gScaleVal[2],
		colorVal[0], colorVal[1], colorVal[2],
		shadowColorVal[0], shadowColorVal[1], shadowColorVal[2]};
	for (size_t index = 0;
		 index < sizeof(scalarValues) / sizeof(scalarValues[0]); ++index) {
		if (!std::isfinite(scalarValues[index])) {
			MGlobal::displayError(
				"DPLightGen: a light parameter is not finite.");
			return MS::kInvalidParameter;
		}
	}
	if (lightTypeVal < 0 || lightTypeVal > 6 ||
		goboFilterTypeVal < 0 || goboFilterTypeVal > 4 ||
		goboWrapTypeVal < 0 || goboWrapTypeVal > 4 ||
		decayTypeVal < 0 || decayTypeVal > 1 ||
		dirTypeVal < 0 || dirTypeVal > 1 ||
		applyAnimMatType < DP_MAT_APPLY_NONE ||
		applyAnimMatType >= DP_MAT_APPLY_TYPE_COUNT ||
		resolutionVal < 0 || sampleVal < 0 || volumeSamplesVal < 0 ||
		maxBouncesVal < 0 || lensRadiusVal < 0.0 ||
		lightRadiusVal < 0.0 || aspectRatioVal <= 0.0) {
		MGlobal::displayError("DPLightGen: a light parameter is invalid.");
		return MS::kInvalidParameter;
	}
		
		
		MVectorArray lightDirVecArray(pts.size(), MVector(baseVecVal[0],baseVecVal[1],baseVecVal[2]));
	
	if (dirTypeVal==1 && !directionAttributeName.empty() && inputDPData->isExistAttr(directionAttributeName)) {
        if (!getCheckedMVectorArrayFromAttribute(
                *inputDPData, directionAttributeName, pts.size(), lightDirVecArray)) {
            MGlobal::displayError("DPLightGen: unable to read the Vec3 direction attribute.");
            return MS::kFailure;
        }
	}

	const unsigned int pointCount = static_cast<unsigned int>(pts.size());
	if (intensityArray.length() != pointCount || colorArray.length() != pointCount ||
	    spotAngleArray.length() != pointCount || scaleArray.length() != pointCount ||
	    animMatArray.length() != pointCount || lightDirVecArray.length() != pointCount) {
		MGlobal::displayWarning(
			"DPLightGen: an inherited attribute count does not match the point count.");
		return MS::kFailure;
	}
	for (size_t index = 0; index < pts.size(); ++index) {
		const MVector& direction = lightDirVecArray[index];
		const MVector& pointColor = colorArray[index];
		if (!std::isfinite(pts[index][0]) ||
			!std::isfinite(pts[index][1]) ||
			!std::isfinite(pts[index][2]) ||
			!std::isfinite(intensityArray[index]) ||
			!std::isfinite(pointColor.x) ||
			!std::isfinite(pointColor.y) ||
			!std::isfinite(pointColor.z) ||
			!std::isfinite(spotAngleArray[index]) ||
			!std::isfinite(scaleArray[index]) ||
			!std::isfinite(direction.x) ||
			!std::isfinite(direction.y) ||
			!std::isfinite(direction.z) ||
			direction.length() <= 1.0e-12) {
			MGlobal::displayError(
				"DPLightGen: inherited point attributes are invalid.");
			return MS::kInvalidParameter;
		}
		for (unsigned int row = 0; row < 4U; ++row) {
			for (unsigned int column = 0; column < 4U; ++column) {
				if (!std::isfinite(animMatArray[index][row][column])) {
					MGlobal::displayError(
						"DPLightGen: animation matrix contains invalid values.");
					return MS::kInvalidParameter;
				}
			}
		}
	}
	
	
	
#pragma mark Export arnold
	
	
	
	const std::string lightTextureNodeName = "dlightTexture";
	const std::string goboNodeName = "dlightGobo";
	dommy::arnold::Session arnoldSession;
	
    if (exportLog) {
        AiMsgSetLogFileName("dlightOut.log");
        AiMsgSetConsoleFlags(AI_LOG_ALL);
    }
	
	
		dommy::arnold::NodeOwner textureNodeOwner;
		dommy::arnold::NodeOwner goboNodeOwner;
		AtNode* texNode = nullptr;
		AtNode* goboNode = nullptr;
		
		if(lightTypeVal==2 && useGoboVal && !goboTexName.empty()) {
			//create Gobo

			textureNodeOwner.reset(AiNode("image"));
			if (!textureNodeOwner) {
				MGlobal::displayError(
					"DPLightGen: failed to create the Arnold gobo image node.");
				return MS::kFailure;
			}
			texNode = textureNodeOwner.get();
			AiNodeSetStr(texNode, "name", lightTextureNodeName.c_str());
		AiNodeSetStr(texNode, "filename", goboTexName.c_str());
		AiNodeSetStr(texNode, "filter", "smart_bicubic");
		AiNodeSetInt(texNode, "mipmap_bias", 0);
		AiNodeSetBool(texNode, "single_channel", false);
		AiNodeSetByte(texNode, "start_channel", 0);
		AiNodeSetStr(texNode, "swrap", "periodic");
		AiNodeSetStr(texNode, "twrap", "periodic");
		AiNodeSetFlt(texNode, "sscale", 1);
		AiNodeSetFlt(texNode, "tscale", 1);
		AiNodeSetBool(texNode, "sflip", false);
		AiNodeSetBool(texNode, "tflip", false);
		AiNodeSetFlt(texNode, "soffset", 0);
		AiNodeSetFlt(texNode, "toffset", 0);
		AiNodeSetBool(texNode, "swap_st", false);
		AiNodeSetVec2(texNode, "uvcoords", 0,0);
		AiNodeSetStr(texNode, "uvset", "");
		AiNodeSetRGB(texNode, "multiply", 1,1,1);
		AiNodeSetRGB(texNode, "offset", 0,0,0);
		AiNodeSetBool(texNode, "ignore_missing_tiles", false);
		AiNodeSetRGBA(texNode, "missing_tile_color", 0, 0, 0, 0);
		
		
			goboNodeOwner.reset(AiNode("gobo"));
			if (!goboNodeOwner) {
				MGlobal::displayError(
					"DPLightGen: failed to create the Arnold gobo node.");
				return MS::kFailure;
			}
			goboNode = goboNodeOwner.get();
		
		AiNodeSetStr(goboNode, "name", goboNodeName.c_str());
		
		AiNodeSetVec2(goboNode, "offset", dommy::arnold::checkedFloat(goboOffsetVal[0]),dommy::arnold::checkedFloat(goboOffsetVal[1]));
		AiNodeSetFlt(goboNode, "density", dommy::arnold::checkedFloat(goboDensityVal));
		
		if (goboFilterTypeVal==0) AiNodeSetStr(goboNode, "filter_mode", "blend");
		else if (goboFilterTypeVal==1) AiNodeSetStr(goboNode, "filter_mode", "replace");
		else if (goboFilterTypeVal==2) AiNodeSetStr(goboNode, "filter_mode", "add");
		else if (goboFilterTypeVal==3) AiNodeSetStr(goboNode, "filter_mode", "sub");
		else if (goboFilterTypeVal==4) AiNodeSetStr(goboNode, "filter_mode", "mix");
		
		AiNodeSetFlt(goboNode, "scale_s", dommy::arnold::checkedFloat(goboScaleVal[0]));
		AiNodeSetFlt(goboNode, "scale_t", dommy::arnold::checkedFloat(goboScaleVal[1]));
		
		
		if (goboWrapTypeVal==0){
			AiNodeSetStr(goboNode, "wrap_s", "periodic");
			AiNodeSetStr(goboNode, "wrap_t", "periodic");
		} else if (goboWrapTypeVal==1) {
			AiNodeSetStr(goboNode, "wrap_s", "black");
			AiNodeSetStr(goboNode, "wrap_t", "black");
		} else if (goboWrapTypeVal==2) {
			AiNodeSetStr(goboNode, "wrap_s", "clamp");
			AiNodeSetStr(goboNode, "wrap_t", "clamp");
		} else if (goboWrapTypeVal==3) {
			AiNodeSetStr(goboNode, "wrap_s", "mirror");
			AiNodeSetStr(goboNode, "wrap_t", "mirror");
		} else if (goboWrapTypeVal==4) {
			AiNodeSetStr(goboNode, "wrap_s", "file");
			AiNodeSetStr(goboNode, "wrap_t", "file");
		}
		
		
		//link file to gobo
        if (!AiNodeLink(texNode, "slidemap", goboNode)) {
            MGlobal::displayError("DPLightGen: gobo texture link failed.");
            return MS::kFailure;
        }
		
		} else if (lightTypeVal==3 && !lightTextureName.empty()) {
			//create Area light texture
			textureNodeOwner.reset(AiNode("image"));
			if (!textureNodeOwner) {
				MGlobal::displayError(
					"DPLightGen: failed to create the Arnold light image node.");
				return MS::kFailure;
			}
			texNode = textureNodeOwner.get();
			AiNodeSetStr(texNode, "name", lightTextureNodeName.c_str());
		
		AiNodeSetStr(texNode, "filename", lightTextureName.c_str());
		AiNodeSetStr(texNode, "filter", "smart_bicubic");
		
		AiNodeSetInt(texNode, "mipmap_bias", 0);
		AiNodeSetBool(texNode, "single_channel", false);
		AiNodeSetByte(texNode, "start_channel", 0);
		AiNodeSetStr(texNode, "swrap", "periodic");
		AiNodeSetStr(texNode, "twrap", "periodic");
			AiNodeSetFlt(texNode, "sscale", dommy::arnold::checkedTextureReciprocal(repeatTexVal[0]));
			AiNodeSetFlt(texNode, "tscale", dommy::arnold::checkedTextureReciprocal(repeatTexVal[1]));
		AiNodeSetBool(texNode, "sflip", false);
		AiNodeSetBool(texNode, "tflip", false);
		AiNodeSetFlt(texNode, "soffset", dommy::arnold::checkedFloat(offsetTexVal[0]));
		AiNodeSetFlt(texNode, "toffset", dommy::arnold::checkedFloat(offsetTexVal[1]));
		AiNodeSetBool(texNode, "swap_st", false);
		AiNodeSetVec2(texNode, "uvcoords", 0,0);
		AiNodeSetStr(texNode, "uvset", "");
		AiNodeSetRGB(texNode, "multiply", 1,1,1);
		AiNodeSetRGB(texNode, "offset", 0,0,0);
		AiNodeSetBool(texNode, "ignore_missing_tiles", false);
		AiNodeSetRGBA(texNode, "missing_tile_color", 0, 0, 0, 0);
	}
	
	
		std::vector<dommy::arnold::NodeOwner> lightNodeOwners;
		lightNodeOwners.reserve(pts.size());
		for (size_t i = 0; i < pts.size(); ++i) {
			// create light node
			dommy::arnold::NodeOwner lightNodeOwner;
			
			float finMatVal[4][4];
            MMatrix lightMatrix;
            status = dommy::light::buildTransform(
                lightTypeVal, applyAnimMatType,
                MVector(pts[i][0], pts[i][1], pts[i][2]), lightDirVecArray[i],
                MVector(gScaleVal[0], gScaleVal[1], gScaleVal[2]), scaleArray[i],
                animMatArray[i], lookAtVec, lightMatrix);
            if (!status) {
                MGlobal::displayError("DPLightGen: light transform could not be prepared.");
                return status;
            }
            dommy::arnold::checkedFloatMatrix(lightMatrix, finMatVal);
		
		
		//for arnold5
		AtMatrix finMat = {{{ finMatVal[0][0],finMatVal[0][1],finMatVal[0][2],finMatVal[0][3]},
							{ finMatVal[1][0],finMatVal[1][1],finMatVal[1][2],finMatVal[1][3]},
							{ finMatVal[2][0],finMatVal[2][1],finMatVal[2][2],finMatVal[2][3]},
							{ finMatVal[3][0],finMatVal[3][1],finMatVal[3][2],finMatVal[3][3]}}};
			const char* lightNodeType = "point_light";
			if (lightTypeVal == 0) {
				lightNodeType = "distant_light";
			} else if (lightTypeVal == 2) {
				lightNodeType = "spot_light";
			} else if (lightTypeVal == 3) {
				lightNodeType = "quad_light";
			} else if (lightTypeVal == 4) {
				lightNodeType = "cylinder_light";
			} else if (lightTypeVal == 5) {
				lightNodeType = "disk_light";
			} else if (lightTypeVal == 6) {
				lightNodeType = "photometric_light";
			}
			lightNodeOwner.reset(AiNode(lightNodeType));
			if (!lightNodeOwner) {
				MGlobal::displayError(
					"DPLightGen: failed to create an Arnold light node.");
				return MS::kFailure;
			}
			AtNode* const light = lightNodeOwner.get();

			if (lightTypeVal==0) {
				AiNodeSetMatrix(light, "matrix", finMat);
				AiNodeSetFlt(light, "angle", dommy::arnold::checkedFloat(distAngleVal));
			
				
			} else if(lightTypeVal==1) {
				//position
			AiNodeSetVec(light, "position", dommy::arnold::checkedFloat(pts[i][0]), dommy::arnold::checkedFloat(pts[i][1]), dommy::arnold::checkedFloat(pts[i][2]));
			AiNodeSetFlt(light, "radius", dommy::arnold::checkedFloat(lightRadiusVal));
				
			} else if(lightTypeVal==2) {
				AiNodeSetMatrix(light, "matrix", finMat);
			
			AiNodeSetFlt(light, "lens_radius", dommy::arnold::checkedFloat(lensRadiusVal));
			AiNodeSetFlt(light, "cone_angle", dommy::arnold::checkedFloat(spotAngleArray[i]));
			AiNodeSetFlt(light, "penumbra_angle", dommy::arnold::checkedFloat(penumbraAngleVal));
			AiNodeSetFlt(light, "cosine_power", dommy::arnold::checkedFloat(cosinePowerVal));
			AiNodeSetFlt(light, "aspect_ratio", dommy::arnold::checkedFloat(aspectRatioVal));
			
			
			AiNodeSetFlt(light, "radius", dommy::arnold::checkedFloat(lightRadiusVal));
				
			} else if(lightTypeVal==3) {
				AiNodeSetMatrix(light, "matrix", finMat);
			
			AiNodeSetInt(light, "resolution", resolutionVal);
			AiNodeSetFlt(light, "spread", dommy::arnold::checkedFloat(spreadVal));
			
			
				
			} else if(lightTypeVal==4) {
				AiNodeSetMatrix(light, "matrix", finMat);
			//cylinder radius はxzのスケール値の最小値を設定
			if (gScaleVal[0]>gScaleVal[2]) {
				AiNodeSetFlt(light, "radius", dommy::arnold::checkedFloatProduct(gScaleVal[0], scaleArray[i]));
			} else {
				AiNodeSetFlt(light, "radius", dommy::arnold::checkedFloatProduct(gScaleVal[2], scaleArray[i]));
			}
			
				
			} else if(lightTypeVal==5) {
				AiNodeSetMatrix(light, "matrix", finMat);
			
			AiNodeSetFlt(light, "spread", dommy::arnold::checkedFloat(spreadVal));
			
			//disk radius はxyのスケール値の最小値を設定
			if (gScaleVal[0]>gScaleVal[1]) {
				AiNodeSetFlt(light, "radius", dommy::arnold::checkedFloatProduct(gScaleVal[0], scaleArray[i]));
			} else {
				AiNodeSetFlt(light, "radius", dommy::arnold::checkedFloatProduct(gScaleVal[1], scaleArray[i]));
			}
			
				
			} else if(lightTypeVal==6) {
				AiNodeSetMatrix(light, "matrix", finMat);
			AiNodeSetStr(light, "filename", photometricFileName.c_str());
			AiNodeSetFlt(light, "radius", dommy::arnold::checkedFloat(lightRadiusVal));
				
			} else {
				AiNodeSetVec(light, "position", dommy::arnold::checkedFloat(pts[i][0]), dommy::arnold::checkedFloat(pts[i][1]), dommy::arnold::checkedFloat(pts[i][2]));
		}
		
		
		//name
		std::ostringstream stream;
		stream.imbue(std::locale::classic());
		stream << "dlight" << i;
		std::string lightNameStr = dommy::text::checkedStreamText(stream, "Arnold light name formatting failed");
		
		
		AiNodeSetStr(light, "name", lightNameStr.c_str());
		
		AiNodeSetFlt(light, "intensity", dommy::arnold::checkedFloat(intensityArray[i]));
		
		if (lightTypeVal==3 && !lightTextureName.empty() && texNode) {
			//quadlight color is texture
            if (!AiNodeLink(texNode, "color", light)) {
                MGlobal::displayError("DPLightGen: color texture link failed.");
                return MS::kFailure;
            }
		} else {
			AiNodeSetRGB(light, "color", dommy::arnold::checkedFloat(colorArray[i][0]), dommy::arnold::checkedFloat(colorArray[i][1]), dommy::arnold::checkedFloat(colorArray[i][2]));
		}
		
		
		//			AiNodeSetRGB(light, "color", colorVal[0], colorVal[1], colorVal[2]);
		AiNodeSetFlt(light, "exposure", dommy::arnold::checkedFloat(exposure)); // alternatively, use 'exposure'
		
		
		if (decayTypeVal==0) {
			AiNodeSetStr(light, "decay_type", "constant");
		} else {
			AiNodeSetStr(light, "decay_type", "quadratic");
		}
		
		AiNodeSetInt(light, "samples", sampleVal);
		
		AiNodeSetBool(light, "normalize", normalizeVal);
		
		
		AiNodeSetBool(light, "cast_shadows", castShadowVal);
		AiNodeSetRGB(
			light, "shadow_color",
			dommy::arnold::checkedFloat(shadowColorVal[0]), dommy::arnold::checkedFloat(shadowColorVal[1]), dommy::arnold::checkedFloat(shadowColorVal[2]));
		AiNodeSetFlt(light, "shadow_density", dommy::arnold::checkedFloat(shadowDensityVal));
		
		
		AiNodeSetBool(light, "affect_volumetrics", affectVolumetricsVal);
		AiNodeSetBool(light, "cast_volumetric_shadows", castVolumeShadowVal);
		AiNodeSetInt(light, "volume_samples", volumeSamplesVal);
		
		
		AiNodeSetBool(light, "affect_diffuse", affectDiffuseVal);
		AiNodeSetBool(light, "affect_specular", affectSpecularVal);
		AiNodeSetFlt(light, "diffuse", dommy::arnold::checkedFloat(diffuseVal));
		AiNodeSetFlt(light, "specular", dommy::arnold::checkedFloat(specularVal));
		AiNodeSetFlt(light, "sss", dommy::arnold::checkedFloat(sssVal));
		AiNodeSetFlt(light, "indirect", dommy::arnold::checkedFloat(indirectVal));
		AiNodeSetFlt(light, "volume", dommy::arnold::checkedFloat(volumeVal));
		
		AiNodeSetInt(light, "max_bounces", maxBouncesVal);
		
		
		AiNodeSetStr(light, "aov", aovLightGroupName.c_str());
		
		//set gobo
			if(lightTypeVal==2 && useGoboVal && !goboTexName.empty() && goboNode) {
			dommy::arnold::ArrayOwner filterArray(
				AiArrayAllocate(1, 1, AI_TYPE_NODE));
			if (!filterArray) {
				AiMsgError(
					"[dommyTool] Arnold could not allocate the "
					"light-filter array.");
				return MS::kFailure;
			}
			dommy::arnold::requireArrayWrite(AiArraySetPtr(filterArray.get(), 0, goboNode));
				AiNodeSetArray(light, "filters", filterArray.release());
			}

			lightNodeOwners.push_back(std::move(lightNodeOwner));
		}
		
		
	//write ass file
	return dommy::arnold::writeAssFile(filename, false, "DPLightGen");
}


MStatus DPLightGen::compute( const MPlug& plug, MDataBlock& data ) try
{
	if(plug == aNodeInfo) {
		MStatus stat;
		
		std::stringstream infoStr;
		
		const MPlug inputPlug(thisMObject(), inDP);
		const bool connected = inputPlug.isConnected(&stat);
		DOMMY_RETURN_IF_FAILURE(stat);
		const DPData* dp = nullptr;
		if (connected) {
			DOMMY_RETURN_IF_FAILURE(getValidInputDP(inDP, data, dp));
			if (!dp) return MS::kFailure;
		}
		if (dp) {
				
				uint count = dp->numberOfPoints();
				
#pragma mark write node info
				//position
				infoStr << "DPoint Info\n";
				infoStr << "	Create light count: " << count << "\n";
				
				
				//attribute
				size_t attrCount = dp->numberOfAttributes();
				infoStr << "	attribute count: " << attrCount << "\n";
				
				for(auto itr = dp->beginAttr(); itr != dp->endAttr(); ++itr) {
					infoStr << "		@" << itr->first << " : " << itr->second->typeName() <<  "\n";
				}
				
				//group
				size_t grpCount = dp->numberOfGroups();
				infoStr << "	group count: " << grpCount << "\n";
				
				for(auto itr = dp->beginGrp(); itr != dp->endGrp(); ++itr) {
					infoStr << "		*" << itr->first << " : " << itr->second->numberOfIds() <<  "\n";
				}
				
#pragma mark check export file
                    // Do not start an external export after known formatting
                    // failure. Final text is checked again before publication.
                    if (!infoStr) return MS::kFailure;
				
				
				
					const MDataHandle exportModeHandle = data.inputValue(aExportMode, &stat);
					DOMMY_RETURN_IF_FAILURE(stat);
					const short exportModeVal = exportModeHandle.asShort();
					if (exportModeVal < 0 || exportModeVal > 2) {
						MGlobal::displayError(
							"DPLightGen: ExportMode is invalid.");
						return MS::kInvalidParameter;
					}

#if DOMMY_WITH_DG_FILE_WRITERS
					if (exportModeVal != 2) {
						const MDataHandle filePathHandle = data.inputValue(aFilePath, &stat);
						DOMMY_RETURN_IF_FAILURE(stat);
						const MString srcFileName = filePathHandle.asString();
						if (srcFileName.length() == 0) {
							MGlobal::displayWarning(
								"DPLightGen: FilePath must not be empty.");
							return MS::kFailure;
						}

						MTime evaluationTime;
						stat = dommy::maya::getEvaluationTime(
							data, evaluationTime);
						DOMMY_RETURN_IF_FAILURE(stat);
						MString resolvedPath;
						if (!getSequenceFileName(
								srcFileName,
								evaluationTime.value(),
								resolvedPath)) {
							MGlobal::displayError(
								"DPLightGen: FilePath contains an invalid "
								"frame token.");
							return MS::kFailure;
						}
						std::string filename = resolvedPath.asChar();
						if (!dommy::file::isSafeExportPath(
								filename, ".ass")) {
							MGlobal::displayError(
								"DPLightGen: output path must be a bounded "
								".ass path without control characters or "
								"'..' components.");
							return MS::kInvalidParameter;
						}
						MFileObject fileObj;
						DOMMY_RETURN_IF_FAILURE(fileObj.setRawFullName(filename.c_str()));
                        filename = fileObj.resolvedFullName().asChar();
                        if (!dommy::file::isSafeExportPath(filename, ".ass"))
                            return MS::kInvalidParameter;

						if (exportModeVal == 0 ||
							(exportModeVal == 1 && !fileObj.exists())) {
							DOMMY_RETURN_IF_FAILURE(
								exportArnoldLight(data, dp, filename));
							infoStr << "	ASS export completed.\n";
						}
					}
#else
					if (exportModeVal != 2) {
						infoStr
							<< "	ASS export is disabled by this build "
							<< "profile.\n";
					}
#endif
		} else {
			infoStr << "DPoint Info\n";
			infoStr << "DPoint is not connected.";
			
			
		}
		
		MString str = dommy::text::checkedStreamText(infoStr, "DPLightGen info formatting failed").c_str();
		MDataHandle strHandle = data.outputValue(aNodeInfo, &stat);
		DOMMY_RETURN_IF_FAILURE(stat);
		strHandle.set(str);
		
		return data.setClean(aNodeInfo);
		
		
	}
	return MS::kUnknownParameter;
}
DOMMY_COMPUTE_CATCH("DPLightGen")

void DPLightGen::draw( M3dView & view, const MDagPath & path,
						 M3dView::DisplayStyle style,
						 M3dView::DisplayStatus status )
{
}


bool DPLightGen::isBounded() const
{
	return true;
}

MBoundingBox DPLightGen::boundingBox() const try
{
	
    MPlug dpPlug(thisMObject(), inDP);
    
    MStatus connectionStatus;
    const bool connected = dpPlug.isConnected(&connectionStatus);
    if (!connectionStatus) {
        MGlobal::displayError("DPLightGen: unable to query input connection for bounds.");
    } else if (connected) {
        
        MObject obj;
        const DPData* dp =  getInputDPfromPlug(dpPlug, obj);
        
        if(dp) {
            MBoundingBox bbox = dp->getBoundingBox();
            
            return bbox;
        }
    }
    
    return lightFallbackBounds();
} catch (...) {
    dommy::maya::reportViewportCallbackException(
        "DPLightGen", "boundingBox", std::current_exception());
    return lightFallbackBounds();
}


MStatus DPLightGen::initialize()
try {
	MStatus				stat;
	MFnNumericAttribute	nAttr;
	MFnTypedAttribute	tAttr;
	MFnEnumAttribute eAttr;
	
	MFnStringData fnStringData;
	MObject emptyStr = fnStringData.create("", &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	
	// ----------------------- INPUTS --------------------------
	inDP = tAttr.create("inDP", "idp", DPData::id, MObject::kNullObj, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(tAttr.setReadable(false));
	DOMMY_RETURN_IF_FAILURE(tAttr.setStorable( false ));
	DOMMY_RETURN_IF_FAILURE(addAttribute(inDP));
	
	
	aNodeInfo = tAttr.create("NodeInfo", "info", MFnData::kString, emptyStr, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
	DOMMY_RETURN_IF_FAILURE(tAttr.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setAffectsAppearance(false));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aNodeInfo));
	
#pragma mark export
	
	MObject defaultFileStringData = fnStringData.create("dlightOut.ass", &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	aFilePath = tAttr.create("FilePath", "file", MFnData::kString, defaultFileStringData, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aFilePath));
	
	
	
	
	aExportMode = eAttr.create("ExportMode", "expm", 2, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(eAttr.addField("Overwrite", 0));
	DOMMY_RETURN_IF_FAILURE(eAttr.addField("Overwrite when no file", 1));
	DOMMY_RETURN_IF_FAILURE(eAttr.addField("No export", 2));
	DOMMY_RETURN_IF_FAILURE(eAttr.setConnectable(false));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aExportMode));
    
    aExportLog = nAttr.create( "ExportLog", "exl", MFnNumericData::kBoolean , 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aExportLog));
    
    
	
    MFnEnumAttribute visTypeEnumFn;
    aLightVisType = visTypeEnumFn.create( "LightVisType", "lvist", 2, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(visTypeEnumFn.addField( "Point", 0 ));
    DOMMY_RETURN_IF_FAILURE(visTypeEnumFn.addField( "Intensity Bar", 1 ));
    DOMMY_RETURN_IF_FAILURE(visTypeEnumFn.addField( "Light Shape", 2 ));
    DOMMY_RETURN_IF_FAILURE(visTypeEnumFn.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(visTypeEnumFn.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aLightVisType));
	
	
	
	aColorVis = nAttr.create( "ColorVis", "colv", MFnNumericData::kBoolean , 0, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(false));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aColorVis));
	
    aIntensityVis = nAttr.create( "IntensityVis", "intv", MFnNumericData::kBoolean , 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aIntensityVis));
    
    aIntensityNormalize = nAttr.create( "IntensityNormalize", "intnorm", MFnNumericData::kDouble , 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(100.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aIntensityNormalize));
    
    
    aPointSize = nAttr.create( "PointSize", "ptsi", MFnNumericData::kFloat , 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0.01));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0.01));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(10.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(3.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aPointSize));
    
    
#pragma mark light type
    MFnEnumAttribute lightEnumFn;
    aLightType = lightEnumFn.create( "LightType", "lt", 1, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(lightEnumFn.addField( "Directional", 0 ));
    DOMMY_RETURN_IF_FAILURE(lightEnumFn.addField( "Point", 1 ));
    DOMMY_RETURN_IF_FAILURE(lightEnumFn.addField( "Spot", 2 ));
    DOMMY_RETURN_IF_FAILURE(lightEnumFn.addField( "Quad", 3 ));
    DOMMY_RETURN_IF_FAILURE(lightEnumFn.addField( "Cylinder", 4 ));
    DOMMY_RETURN_IF_FAILURE(lightEnumFn.addField( "Disk", 5 ));
    DOMMY_RETURN_IF_FAILURE(lightEnumFn.addField( "Photometric", 6 ));
    DOMMY_RETURN_IF_FAILURE(lightEnumFn.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(lightEnumFn.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aLightType));
    
    
    
#pragma mark diatant
    aDistantAngle = nAttr.create( "DistantAngle", "dang", MFnNumericData::kDouble , 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMax(180.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(10.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aDistantAngle));
    
    
    
#pragma mark spot
    aLensRadius = nAttr.create( "LensRadius", "lensr", MFnNumericData::kDouble , 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMax(100.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(10.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aLensRadius));
    
    aConeAngle = nAttr.create( "ConeAngle", "cang", MFnNumericData::kDouble , 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0.006));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMax(179.994));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0.006));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(179.994));
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(40.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aConeAngle));
    
    aPenumbraAngle = nAttr.create( "PenumbraAngle", "pang", MFnNumericData::kDouble , 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(-179.994));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMax(179.994));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(-10.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(10.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aPenumbraAngle));
    
    aCosinePower = nAttr.create( "CosinePower", "cpow", MFnNumericData::kDouble , 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMax(255.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(10.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aCosinePower));
    
    aAspectRatio = nAttr.create( "AspectRatio", "ar", MFnNumericData::kDouble , 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMax(100.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(1.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aAspectRatio));
    
	
	
	aUseGobo = nAttr.create( "UseGobo", "usegobo", MFnNumericData::kBoolean , 0, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(false));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aUseGobo));
	
	MObject gobotextureName = fnStringData.create("", &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	aGoboTexture = tAttr.create("GoboTexture", "gotex", MFnData::kString, gobotextureName, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aGoboTexture));
	
	MFnEnumAttribute goboEnumFn;
	aGoboFilter = goboEnumFn.create( "GoboFilter", "gfil", 0, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(goboEnumFn.addField( "blend", 0 ));
	DOMMY_RETURN_IF_FAILURE(goboEnumFn.addField( "replace", 1 ));
	DOMMY_RETURN_IF_FAILURE(goboEnumFn.addField( "add", 2 ));
	DOMMY_RETURN_IF_FAILURE(goboEnumFn.addField( "sub", 3 ));
	DOMMY_RETURN_IF_FAILURE(goboEnumFn.addField( "mix", 4 ));
	DOMMY_RETURN_IF_FAILURE(goboEnumFn.setKeyable( true ));
	DOMMY_RETURN_IF_FAILURE(goboEnumFn.setStorable(true));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aGoboFilter));
	
	aGoboDensity = nAttr.create( "GoboDensity", "gdens", MFnNumericData::kDouble , 0, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0.0));
	DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0.0));
	DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(10));
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aGoboDensity));
	
	aGoboOffset = nAttr.create( "GoboOffset", "goff", MFnNumericData::k2Double , 0, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0,0.0));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aGoboOffset));

	aGoboScale = nAttr.create( "GoboScale", "gsca", MFnNumericData::k2Double , 0, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0.0));
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1.0,1.0));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aGoboScale));
	
	MFnEnumAttribute gwrapEnumFn;
	aGoboWrap = gwrapEnumFn.create( "GoboWrap", "gwra", 2, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(gwrapEnumFn.addField( "periodic", 0 ));
	DOMMY_RETURN_IF_FAILURE(gwrapEnumFn.addField( "black", 1 ));
	DOMMY_RETURN_IF_FAILURE(gwrapEnumFn.addField( "clamp", 2 ));
	DOMMY_RETURN_IF_FAILURE(gwrapEnumFn.addField( "mirror", 3 ));
	DOMMY_RETURN_IF_FAILURE(gwrapEnumFn.addField( "file", 4 ));
	DOMMY_RETURN_IF_FAILURE(gwrapEnumFn.setKeyable( true ));
	DOMMY_RETURN_IF_FAILURE(gwrapEnumFn.setStorable(true));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aGoboWrap));
	
	
    
#pragma mark area quad cylinder disk
    
    aResolution = nAttr.create( "Resolution", "res", MFnNumericData::kInt , 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(4096));
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(512));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aResolution));
    
    aSpread = nAttr.create( "Spread", "spr", MFnNumericData::kDouble , 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMax(1.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(1.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aSpread));
	
	
	MObject ltextureName = fnStringData.create("", &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	aLightTexture = tAttr.create("LightTexture", "ltex", MFnData::kString, ltextureName, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aLightTexture));
	
		aRepeatTex = nAttr.create( "RepeatTex", "reptex", MFnNumericData::k2Double , 0, &stat);
		DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1.0,1.0));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aRepeatTex));
	
	aOffsetTex = nAttr.create( "OffsetTex", "offtex", MFnNumericData::k2Double , 0, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0,0.0));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aOffsetTex));
	
	
	
    
#pragma mark photometric
    MObject photoName = fnStringData.create("", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aPhotometricFileName = tAttr.create("PhotometricFileName", "phf", MFnData::kString, photoName, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aPhotometricFileName));
    
    
    
#pragma mark common
    aIntensity = nAttr.create( "Intensity", "inten", MFnNumericData::kDouble , 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(100.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aIntensity));
    
    aExposure = nAttr.create( "Exposure", "exp", MFnNumericData::kDouble , 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(20.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aExposure));
    
    
    
    aColor = nAttr.createColor("Color", "col", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1.0, 1.0, 1.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aColor));
    
    
    
    MFnEnumAttribute decayEnumFn;
    aDecayType = decayEnumFn.create( "DecayType", "dect", 1, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(decayEnumFn.addField( "constant", 0 ));
    DOMMY_RETURN_IF_FAILURE(decayEnumFn.addField( "quadratic", 1 ));
    DOMMY_RETURN_IF_FAILURE(decayEnumFn.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(decayEnumFn.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aDecayType));
    
    aSamples = nAttr.create( "Samples", "sam", MFnNumericData::kInt , 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMax(100));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(10));
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aSamples));
    
    aLightRadius = nAttr.create( "LightRadius", "lrad", MFnNumericData::kDouble , 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(10.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aLightRadius));
    
    aNormalize = nAttr.create( "Normalize", "lnorm", MFnNumericData::kBoolean , 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aNormalize));
    
    
    aCastShadow = nAttr.create( "CastShadow", "cash", MFnNumericData::kBoolean , 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aCastShadow));
    
    aShadowColor = nAttr.createColor("ShadowColor", "shcol", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0, 0.0, 0.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aShadowColor));
    
    aShadowDensity = nAttr.create( "ShadowDensity", "shd", MFnNumericData::kDouble , 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMax(1.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(1.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aShadowDensity));
    
    
    aAffectVolumetrics = nAttr.create( "AffectVolumetrics", "afvol", MFnNumericData::kBoolean , 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aAffectVolumetrics));
    
    aCastVolumeShadow = nAttr.create( "CastVolumeShadow", "cvolsh", MFnNumericData::kBoolean , 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aCastVolumeShadow));
    
    
    aVolumeSamples = nAttr.create( "VolumeSamples", "volsam", MFnNumericData::kInt , 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMax(100));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(10));
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(2));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aVolumeSamples));
    
    
    
    aAffectDiffuse = nAttr.create( "AffectDiffuse", "afdif", MFnNumericData::kBoolean , 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aAffectDiffuse));
    
    aAffectSpecular = nAttr.create( "AffectSpecular", "afspec", MFnNumericData::kBoolean , 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aAffectSpecular));
    
    
    aDiffuse = nAttr.create( "Diffuse", "dif", MFnNumericData::kDouble , 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(1.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aDiffuse));
    
    aSpecular = nAttr.create( "Specular", "spec", MFnNumericData::kDouble , 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(1.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aSpecular));
    
    aSss = nAttr.create( "Sss", "s", MFnNumericData::kDouble , 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(1.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aSss));
    
    aIndirect = nAttr.create( "Indirect", "idr", MFnNumericData::kDouble , 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(1.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aIndirect));
    
    aVolume = nAttr.create( "Volume", "vol", MFnNumericData::kDouble , 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(1.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aVolume));
    
    aMaxBounces = nAttr.create( "MaxBounces", "mbo", MFnNumericData::kInt , 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(1.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(999));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aMaxBounces));
    
    
    
    MObject attrName = fnStringData.create("", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aAOVLightGroup = tAttr.create("AOVLightGroup", "aovl", MFnData::kString, attrName, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aAOVLightGroup));
    
    
    
#pragma mark direction
    
    MFnEnumAttribute dirEnumFn;
    aDirectionType = dirEnumFn.create( "DirectionType", "dirt", 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(dirEnumFn.addField( "Base Vector", 0 ));
    DOMMY_RETURN_IF_FAILURE(dirEnumFn.addField( "From Attribute", 1 ));
    DOMMY_RETURN_IF_FAILURE(dirEnumFn.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(dirEnumFn.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aDirectionType));
    
    aBaseDirection = nAttr.create( "BaseDirection", "bvec", MFnNumericData::k3Double , 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0,1.0,0.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aBaseDirection));
    
    MObject dirAttrName = fnStringData.create("", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aDirectionAttribute = tAttr.create("DirectionAttribute", "dirattr", MFnData::kString, dirAttrName, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aDirectionAttribute));
    
    
    
    MObject emptyAttrName = fnStringData.create("", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aIntensityAttribute = tAttr.create("IntensityAttribute", "intattr", MFnData::kString, emptyAttrName, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aIntensityAttribute));
    
    aColorAttribute = tAttr.create("ColorAttribute", "colattr", MFnData::kString, emptyAttrName, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aColorAttribute));
    
    aSpotAngleAttribute = tAttr.create("SpotAngleAttribute", "sptattr", MFnData::kString, emptyAttrName, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aSpotAngleAttribute));
    
    aScaleAttribute = tAttr.create("ScaleAttribute", "sclattr", MFnData::kString, emptyAttrName, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aScaleAttribute));
    
    aAnimMatrixAttribute = aAnimMatrixAttribute = tAttr.create("AnimMatrixAttribute", "amata", MFnData::kString, emptyAttrName, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aAnimMatrixAttribute));
    
#pragma mark animation
	
	
	MFnEnumAttribute aniTypeEnumFn;
	aApplyAnimMatType = aniTypeEnumFn.create( "ApplyAnimMatType", "aamt", DP_MAT_PRE_TRANSLATE, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(aniTypeEnumFn.addField( "None", DP_MAT_APPLY_NONE ));
	DOMMY_RETURN_IF_FAILURE(aniTypeEnumFn.addField( "Apply Matrix(Ignored All)", DP_MAT_FULL_MAT_IGNORE_ALL ));
	DOMMY_RETURN_IF_FAILURE(aniTypeEnumFn.addField( "Apply Matrix(Ignored Dir)", DP_MAT_FULL_MAT_IGNORE_DIR ));
	DOMMY_RETURN_IF_FAILURE(aniTypeEnumFn.addField( "Pre Translate Matrix", DP_MAT_PRE_TRANSLATE ));
	DOMMY_RETURN_IF_FAILURE(aniTypeEnumFn.addField( "Use Position and LookAt", DP_MAT_LOOKAT ));
	DOMMY_RETURN_IF_FAILURE(aniTypeEnumFn.setKeyable( true ));
	DOMMY_RETURN_IF_FAILURE(aniTypeEnumFn.setStorable(true));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aApplyAnimMatType));
	
	
	aAnimLookPos = nAttr.create( "AnimLookPos", "alkat", MFnNumericData::k3Double , 0, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0,0.0,0.0));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aAnimLookPos));
	
	
#pragma mark global transform
    
    aGlobalScale = nAttr.create( "GlobalScale", "gscal", MFnNumericData::k3Double , 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1.0,1.0,1.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aGlobalScale));
    
	
#pragma mark affect attribute
	DOMMY_RETURN_IF_FAILURE(attributeAffects(inDP, aNodeInfo));
	
	DOMMY_RETURN_IF_FAILURE(attributeAffects(aFilePath, aNodeInfo));
	DOMMY_RETURN_IF_FAILURE(attributeAffects(aExportMode, aNodeInfo));
	DOMMY_RETURN_IF_FAILURE(attributeAffects(aExportLog, aNodeInfo));
    
	DOMMY_RETURN_IF_FAILURE(attributeAffects(aLightVisType, aNodeInfo));
	DOMMY_RETURN_IF_FAILURE(attributeAffects(aColorVis, aNodeInfo));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aIntensityVis, aNodeInfo));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aIntensityNormalize, aNodeInfo));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aPointSize, aNodeInfo));
    
    //type
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aLightType, aNodeInfo ));
    
    //distant
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aDistantAngle, aNodeInfo ));
    
    //spot
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aLensRadius, aNodeInfo ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aConeAngle, aNodeInfo ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aPenumbraAngle, aNodeInfo ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aCosinePower, aNodeInfo ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aAspectRatio, aNodeInfo ));
	
	DOMMY_RETURN_IF_FAILURE(attributeAffects( aUseGobo, aNodeInfo ));
	DOMMY_RETURN_IF_FAILURE(attributeAffects( aGoboTexture, aNodeInfo ));
	DOMMY_RETURN_IF_FAILURE(attributeAffects( aGoboFilter, aNodeInfo ));
	DOMMY_RETURN_IF_FAILURE(attributeAffects( aGoboDensity, aNodeInfo ));
	DOMMY_RETURN_IF_FAILURE(attributeAffects( aGoboOffset, aNodeInfo ));
	DOMMY_RETURN_IF_FAILURE(attributeAffects( aGoboScale, aNodeInfo ));
	DOMMY_RETURN_IF_FAILURE(attributeAffects( aGoboWrap, aNodeInfo ));
	
	
	
    //area quad cylinder disk
	DOMMY_RETURN_IF_FAILURE(attributeAffects( aResolution, aNodeInfo ));
	DOMMY_RETURN_IF_FAILURE(attributeAffects( aSpread, aNodeInfo ));
	DOMMY_RETURN_IF_FAILURE(attributeAffects( aLightTexture, aNodeInfo ));
	DOMMY_RETURN_IF_FAILURE(attributeAffects( aRepeatTex, aNodeInfo ));
	DOMMY_RETURN_IF_FAILURE(attributeAffects( aOffsetTex, aNodeInfo ));
	
	
	
    //photometric
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aPhotometricFileName, aNodeInfo ));
    
    //common
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aIntensity, aNodeInfo ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aExposure, aNodeInfo ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aColor, aNodeInfo ));
    
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aDecayType, aNodeInfo ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aSamples, aNodeInfo ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aLightRadius, aNodeInfo ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aNormalize, aNodeInfo ));
    
    
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aCastShadow, aNodeInfo ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aShadowColor, aNodeInfo ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aShadowDensity, aNodeInfo ));
    
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aAffectVolumetrics, aNodeInfo ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aCastVolumeShadow, aNodeInfo ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aVolumeSamples, aNodeInfo ));
    
    
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aAffectDiffuse, aNodeInfo ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aAffectSpecular, aNodeInfo ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aDiffuse, aNodeInfo ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aSpecular, aNodeInfo ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aSss, aNodeInfo ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aIndirect, aNodeInfo ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aVolume, aNodeInfo ));
    
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aMaxBounces, aNodeInfo ));
    
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aAOVLightGroup, aNodeInfo ));
    
    //direction
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aDirectionType, aNodeInfo ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aBaseDirection, aNodeInfo ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aDirectionAttribute, aNodeInfo ));
    
    
    //attribute inerit
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aIntensityAttribute, aNodeInfo ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aColorAttribute, aNodeInfo ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aSpotAngleAttribute, aNodeInfo ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aScaleAttribute, aNodeInfo ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aAnimMatrixAttribute, aNodeInfo ));
	
	//animation
	DOMMY_RETURN_IF_FAILURE(attributeAffects( aApplyAnimMatType, aNodeInfo ));
	DOMMY_RETURN_IF_FAILURE(attributeAffects( aAnimLookPos, aNodeInfo ));
	
    //transform
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aGlobalScale, aNodeInfo ));
    
    
	return MS::kSuccess;
}
catch (...) {
    return dommy::maya::reportEntryPointException(
        "DPLightGen", "initialize", std::current_exception());
}



//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
// Viewport 2.0 override implementation
//---------------------------------------------------------------------------
//---------------------------------------------------------------------------



#pragma mark DPLightGenOverride method

DPLightGenOverride::DPLightGenOverride(const MObject& obj)
: MHWRender::MPxDrawOverride(obj, nullptr, false)
, mNodeHandle(obj)
, fModelEditorChangedCbId(0)
{
	MStatus status;
	const MCallbackId callbackId = MEventMessage::addEventCallback(
		"modelEditorChanged", OnModelEditorChanged, this, &status);
	if (status == MS::kSuccess) {
		fModelEditorChangedCbId = callbackId;
	}
}

DPLightGenOverride::~DPLightGenOverride()
{
	const MCallbackId callbackId = fModelEditorChangedCbId;
	fModelEditorChangedCbId = 0;
	if (callbackId != 0)
	{
		MMessage::removeCallback(callbackId);
	}
}

void DPLightGenOverride::OnModelEditorChanged(void *clientData)
{
	try {
		// Mark the node as being dirty so that it can update on display
		// appearance switches among wireframe and shaded.
		DPLightGenOverride *ovr =
			static_cast<DPLightGenOverride*>(clientData);
		if (ovr && ovr->mNodeHandle.isValid() && ovr->mNodeHandle.isAlive())
		{
			MHWRender::MRenderer::setGeometryDrawDirty(
				ovr->mNodeHandle.object());
		}
	} catch (...) {
		(void)dommy::maya::reportEntryPointException(
			"DPLightGenOverride", "modelEditorChanged callback",
			std::current_exception());
	}
}

MHWRender::DrawAPI DPLightGenOverride::supportedDrawAPIs() const
{
	// this plugin supports both GL and DX
	return (MHWRender::kOpenGL | MHWRender::kDirectX11 | MHWRender::kOpenGLCoreProfile);
}


bool DPLightGenOverride::isBounded(const MDagPath& /*objPath*/,
														const MDagPath& /*cameraPath*/) const
{
	return true;
}

MBoundingBox DPLightGenOverride::boundingBox(
													const MDagPath& objPath,
													const MDagPath& /*cameraPath*/) const
try
{
		
	    
	    MStatus stat;
	    MObject lightGenNode = objPath.node(&stat);
	    if (stat == MS::kSuccess && !lightGenNode.isNull()) {
		    MPlug dpPlug(lightGenNode, DPLightGen::inDP);
            const bool connected = !dpPlug.isNull() && dpPlug.isConnected(&stat);
            if (!stat) {
                MGlobal::displayError("DPLightGen: unable to query draw input connection for bounds.");
                return lightFallbackBounds();
            }
		    if (connected) {
	        
		        MObject obj;
		        const DPData* dp = getInputDPfromPlug(dpPlug, obj);
	        
		        if(dp) {
		            MBoundingBox bbox = dp->getBoundingBox();
	            
		            return bbox;
		        }
		    }
	    }
    
    return lightFallbackBounds();
} catch (...) {
    dommy::maya::reportViewportCallbackException(
        "DPLightGenOverride", "boundingBox", std::current_exception());
    return lightFallbackBounds();
}




// Called by Maya each time the object needs to be drawn.
MUserData* DPLightGenOverride::prepareForDraw(
											 const MDagPath& objPath,
											 const MDagPath& /*cameraPath*/,
											 const MHWRender::MFrameContext& /*frameContext*/,
											 MUserData* oldData)
try {
	// Stage per-instance dependency-graph data before publishing a cache for
	// addUIDrawables(). Reused Maya user data remains non-owning on every path.
	
	
	
    DPLightGenData staged;
    DPLightGenData* const data = &staged;
    DPLightGenData* cachedData = dynamic_cast<DPLightGenData*>(oldData);
    std::unique_ptr<DPLightGenData> newData;
    if (!cachedData) {
        newData.reset(new DPLightGenData());
        cachedData = newData.get();
    }

    const auto finishWithCurrentData =
        [&newData, data, cachedData]() -> MUserData* {
        if (data->matrixArray.length() != 0U && !validLightDrawData(*data))
            return NULL;
        if (!dommy::maya::copyCheckedArray(data->matrixArray, cachedData->matrixArray) ||
            !dommy::maya::copyCheckedArray(data->intensityArray, cachedData->intensityArray) ||
            !dommy::maya::copyCheckedArray(data->spotAngleArray, cachedData->spotAngleArray) ||
            !dommy::maya::copyCheckedArray(data->colorArray, cachedData->colorArray)) {
            return NULL;
        }
        cachedData->lightType = data->lightType;
        cachedData->useConeAngleVis = data->useConeAngleVis;
        cachedData->lightVisType = data->lightVisType;
        cachedData->colorVis = data->colorVis;
        cachedData->intensityVis = data->intensityVis;
        cachedData->intensityNormalizeVal = data->intensityNormalizeVal;
        cachedData->pointSize = data->pointSize;
        newData.release();
        return cachedData;
    };
    
    MStatus status;
    MObject lightGenNode = objPath.node(&status);
    if (status != MS::kSuccess || lightGenNode.isNull()) {
        return NULL;
    }
    
    
    MPlug dpPlug (lightGenNode, DPLightGen::inDP);
    
    
    const bool connected = dpPlug.isConnected(&status);
    if (!status) {
        MGlobal::displayError("DPLightGen: unable to query input connection for drawing.");
        return NULL;
    }
    if (connected) {
        
        dommy::maya::ScopedPlugDataHandle inputHandleOwner;
        MDataHandle& inputHandle = inputHandleOwner.handle();
        if (inputHandleOwner.read(dpPlug) != MS::kSuccess) {
            return NULL;
        }
        
        const DPData* const dp(getInputDPfromHandle(inputHandle));
        if (!dp) return NULL;
        
        if(dp) {
            
            const DPointList pts = dp->getPositionConst();
			
			bool colorVis = false;
			if (readPlugValue(lightGenNode, DPLightGen::aColorVis, colorVis) !=
                MS::kSuccess) {
                return NULL;
            }
			data->colorVis = colorVis;
			
			int lightVisType = 2;
			if (readPlugValue(lightGenNode, DPLightGen::aLightVisType,
                              lightVisType) != MS::kSuccess) {
                return NULL;
            }
			data->lightVisType = lightVisType;
            
            bool intensityVis = false;
            if (readPlugValue(lightGenNode, DPLightGen::aIntensityVis,
                              intensityVis) != MS::kSuccess) {
                return NULL;
            }
            data->intensityVis = intensityVis;
			
            double intensityNorm = 1.0;
            if (readPlugValue(lightGenNode, DPLightGen::aIntensityNormalize,
                              intensityNorm) != MS::kSuccess) {
                return NULL;
            }
            data->intensityNormalizeVal = intensityNorm;
            
            double psize = 3.0;
            if (readPlugValue(lightGenNode, DPLightGen::aPointSize, psize) !=
                MS::kSuccess) {
                return NULL;
            }
            if (!dommy::numeric::checkedFloatCast(psize, data->pointSize) ||
                data->pointSize <= 0.0f) {
                return NULL;
            }
            
			
            //light type
            int lightTypeVal = -1;
            if (readPlugValue(lightGenNode, DPLightGen::aLightType,
                              lightTypeVal) != MS::kSuccess) {
                return NULL;
            }
            data->lightType = lightTypeVal;
			
			//direction
			int dirTypeVal = 0;
			if (readPlugValue(lightGenNode, DPLightGen::aDirectionType,
                              dirTypeVal) != MS::kSuccess) {
                return NULL;
            }
			MVector baseDirVal(0.0, 0.0, -1.0);
			if (readPlugValue(lightGenNode, DPLightGen::aBaseDirection,
                              baseDirVal) != MS::kSuccess) {
                return NULL;
            }
			
			//check use attribute
			MVectorArray lightDirVecArray(pts.size(), baseDirVal);
			MString dirAttreNameStr;
			if (readPlugValue(lightGenNode, DPLightGen::aDirectionAttribute,
                              dirAttreNameStr) != MS::kSuccess) {
                return NULL;
            }
			std::string directionAttributeName = dirAttreNameStr.asChar();
			if (dirTypeVal==1 && !directionAttributeName.empty() && dp->isExistAttr(directionAttributeName)) {
                if (!getCheckedMVectorArrayFromAttribute(
                        *dp, directionAttributeName, pts.size(), lightDirVecArray)) {
                    MGlobal::displayError("DPLightGen: unable to read the Vec3 direction attribute.");
                    return NULL;
                }
			}
			
			
			//color
            MVector baseColorVal(1.0, 1.0, 1.0);
            if (readPlugValue(lightGenNode, DPLightGen::aColor,
                              baseColorVal) != MS::kSuccess) {
                return NULL;
            }
			
			//check use color attribute
			MVectorArray colorVecArray(pts.size(), baseColorVal);
			MString colAttreNameStr;
			if (readPlugValue(lightGenNode, DPLightGen::aColorAttribute,
                              colAttreNameStr) != MS::kSuccess) {
                return NULL;
            }
			std::string colAttributeName = colAttreNameStr.asChar();
			if (!colAttributeName.empty() && dp->isExistAttr(colAttributeName)) {
                if (!getCheckedMVectorArrayFromAttribute(
                        *dp, colAttributeName, pts.size(), colorVecArray)) {
                    MGlobal::displayError("DPLightGen: unable to read the Vec3 color attribute.");
                    return NULL;
                }
			}
			if (!dommy::maya::copyCheckedArray(colorVecArray, data->colorArray)) return NULL;
			
            
            //Intensity
            double intensity = 1.0;
            if (readPlugValue(lightGenNode, DPLightGen::aIntensity,
                              intensity) != MS::kSuccess) {
                return NULL;
            }
			
			//check use intensity attribute
			MDoubleArray intensityArray(pts.size(), intensity);
			MString intenAttreNameStr;
			if (readPlugValue(lightGenNode, DPLightGen::aIntensityAttribute,
                              intenAttreNameStr) != MS::kSuccess) {
                return NULL;
            }
			std::string intenAttributeName = intenAttreNameStr.asChar();
			if (!intenAttributeName.empty() && dp->isExistAttr(intenAttributeName)) {
                if (!getCheckedMRealArrayFromAttribute(
                        *dp, intenAttributeName, pts.size(), intensityArray)) {
                    MGlobal::displayError("DPLightGen: unable to read the numeric intensity attribute.");
                    return NULL;
                }
			}
			if (!dommy::maya::copyCheckedArray(intensityArray, data->intensityArray)) return NULL;
			
			//spotLight Cone angle
			if(lightTypeVal==2) {
				double spotAngle = 0.0;
				if (readPlugValue(lightGenNode, DPLightGen::aConeAngle,
                                  spotAngle) != MS::kSuccess) {
                    return NULL;
                }
				
				MDoubleArray spotArray(pts.size(), spotAngle);
				MString spotAttrNameStr;
				if (readPlugValue(lightGenNode,
                                  DPLightGen::aSpotAngleAttribute,
                                  spotAttrNameStr) != MS::kSuccess) {
                    return NULL;
                }
				std::string spotAttrName = spotAttrNameStr.asChar();
				if (!spotAttrName.empty() && dp->isExistAttr(spotAttrName)) {
					data->useConeAngleVis = true;
                    if (!getCheckedMRealArrayFromAttribute(
                            *dp, spotAttrName, pts.size(), spotArray)) {
                        MGlobal::displayError("DPLightGen: unable to read the numeric cone-angle attribute.");
                        return NULL;
                    }
				}
				if (!dommy::maya::copyCheckedArray(spotArray, data->spotAngleArray)) return NULL;
			}
            
            //Scale
            MVector gScaleVal(1.0,1.0,1.0);
            if (readPlugValue(lightGenNode, DPLightGen::aGlobalScale,
                              gScaleVal) != MS::kSuccess) {
                return NULL;
            }
            MString scaleAttributeNameStr;
            if (readPlugValue(lightGenNode, DPLightGen::aScaleAttribute,
                              scaleAttributeNameStr) != MS::kSuccess) {
                return NULL;
            }
            std::string attrName = scaleAttributeNameStr.asChar();
            MDoubleArray scaleArray(pts.size(), 1.0);
            if (!attrName.empty() && dp->isExistAttr(attrName)) {
                if (!getCheckedMRealArrayFromAttribute(
                        *dp, attrName, pts.size(), scaleArray)) {
                    MGlobal::displayError("DPLightGen: unable to read the numeric scale attribute.");
                    return NULL;
                }
            }
			
			//animationMatrix
			MString aniMatAttrNameStr;
			if (readPlugValue(lightGenNode,
                              DPLightGen::aAnimMatrixAttribute,
                              aniMatAttrNameStr) != MS::kSuccess) {
                return NULL;
            }
			std::string aniMatAttrName = aniMatAttrNameStr.asChar();
			MMatrixArray animMatArray(pts.size());
			if (!aniMatAttrName.empty() && dp->isExistAttr(aniMatAttrName)) {
                if (!getCheckedMMatrixArrayFromAttribute(
                        *dp, aniMatAttrName, pts.size(), animMatArray)) {
                    MGlobal::displayError("DPLightGen: unable to read the Matrix animation attribute.");
                    return NULL;
                }
			}
			
			//animation Type
			int applyAnimMatType = 0;
			if (readPlugValue(lightGenNode, DPLightGen::aApplyAnimMatType,
                              applyAnimMatType) != MS::kSuccess) {
                return NULL;
            }
			
			//lookat
			MVector lookAtVec(0.0, 0.0, 0.0);
			if (readPlugValue(lightGenNode, DPLightGen::aAnimLookPos,
                              lookAtVec) != MS::kSuccess) {
                return NULL;
            }
			
			
            if (lightTypeVal < 0 || lightTypeVal > 6 ||
                lightVisType < 0 || lightVisType > 2 ||
                dirTypeVal < 0 || dirTypeVal > 1 ||
                applyAnimMatType < DP_MAT_APPLY_NONE ||
                applyAnimMatType >= DP_MAT_APPLY_TYPE_COUNT ||
                lightDirVecArray.length() != pts.size() ||
                scaleArray.length() != pts.size() ||
                animMatArray.length() != pts.size()) {
                return NULL;
            }

            status = dommy::maya::buildCheckedArray(
                pts.size(),
                [&](unsigned int i, MMatrix& matrix) -> MStatus {
                    return dommy::light::buildTransform(
                        lightTypeVal, applyAnimMatType,
                        MVector(pts[i][0], pts[i][1], pts[i][2]), lightDirVecArray[i],
                        gScaleVal, scaleArray[i], animMatArray[i], lookAtVec, matrix);
                }, data->matrixArray);
            if (!status) return NULL;
            
        }
    
    
    }
    
	
	
	
    
	
	return finishWithCurrentData();
}
DOMMY_VIEWPORT_POINTER_CATCH("DPLightGenOverride", "prepareForDraw")


// addUIDrawables() provides access to the MUIDrawManager, which can be used
// to queue up operations for drawing simple UI elements such as lines, circles and
// text. To enable addUIDrawables(), override hasUIDrawables() and make it return true.
void DPLightGenOverride::addUIDrawables(
										   const MDagPath& objPath,
										   MHWRender::MUIDrawManager& drawManager,
										   const MHWRender::MFrameContext& /*frameContext*/,
										   const MUserData* data)
try {
	// Get data cached by prepareForDraw() for each drawable instance, then MUIDrawManager
	// can draw simple UI by these data.
	
	
	
	const DPLightGenData* lData =
		dynamic_cast<const DPLightGenData*>(data);
	if (!lData || !validLightDrawData(*lData))
	{
		return;
	}
	
	(void)objPath;
	dommy::maya::ScopedUIDrawable drawable(drawManager);
	
	// Draw the foot print solid/wireframe
//	drawManager.setColor( pLocatorData->fColor );
	
	drawManager.setLineWidth(1.0);
	drawManager.setPointSize(lData->pointSize);
	drawManager.setDepthPriority(5);
	
	
	
	//Light Shape
	dommy::maya::CheckedArray<MPointArray> pts;
	dommy::maya::CheckedArray<MPointArray> subPts;
	
	//distantLight
	if (lData->lightVisType==2) {
		if (lData->lightType==0) {
			float distantLightSize = 0.5;
			float distantLightSpace = 0.2;
			pts.append(MPoint(distantLightSpace, distantLightSpace, distantLightSize));
			pts.append(MPoint(distantLightSpace, distantLightSpace, -distantLightSize));
			pts.append(MPoint(-distantLightSpace, distantLightSpace, distantLightSize));
			pts.append(MPoint(-distantLightSpace, distantLightSpace, -distantLightSize));
			pts.append(MPoint(0.0, -distantLightSpace, distantLightSize));
			pts.append(MPoint(0.0, -distantLightSpace, -distantLightSize));
		} else if (lData->lightType==1) {
			//pointLightLight
			float pointLightSize = 0.25;
			pts.append(MPoint(0.0, pointLightSize, 0.0));
			pts.append(MPoint(pointLightSize, 0.0, 0.0));
			pts.append(MPoint(0.0, -pointLightSize, 0.0));
			pts.append(MPoint(-pointLightSize, 0.0, 0.0));
			pts.append(MPoint(0.0, pointLightSize, 0.0));
		} else if (lData->lightType==2) {
			//spotLight
			
			if (!lData->useConeAngleVis) {
				MVector baseCAngleVec(0.0,0.0,-1.0);
				
				//angle line
				float halfAngle = lData->spotAngleArray[0]*.5;
				MVector rotVec = baseCAngleVec.rotateBy(MVector::kYaxis, dommy::math::degreesToRadians(halfAngle));
				MVector invXVec = MVector(-rotVec.x,rotVec.y,rotVec.z);
				MVector YVec = MVector(rotVec.y,rotVec.x,rotVec.z);
				MVector invYVec = MVector(rotVec.y,-rotVec.x,rotVec.z);
				pts.append(MPoint(0.0, 0.0, 0.0));
				pts.append(MPoint(rotVec));
				pts.append(MPoint(0.0, 0.0, 0.0));
				pts.append(MPoint(invXVec));
				pts.append(MPoint(0.0, 0.0, 0.0));
				pts.append(MPoint(YVec));
				pts.append(MPoint(0.0, 0.0, 0.0));
				pts.append(MPoint(invYVec));
				
				//angle circle
				MVector baseCircleVec(rotVec);
				float stepAngle=360.0/16.0;
				for (uint i=0; i<16; i++) {
					subPts.append(baseCircleVec.rotateBy(MVector::kZaxis, dommy::math::degreesToRadians(stepAngle*i)));
				}
				subPts.append(baseCircleVec);
				
			}
			
		} else if (lData->lightType==3) {
			//quadLight
			float quadSize = 1.0;
			
			pts.append(MPoint(-quadSize, -quadSize, 0.0));
			pts.append(MPoint(-quadSize, quadSize, 0.0));
			pts.append(MPoint(quadSize, quadSize, 0.0));
			pts.append(MPoint(quadSize, -quadSize, 0.0));
			pts.append(MPoint(-quadSize, -quadSize, 0.0));
			
		} else if (lData->lightType==4) {
			//cylinder_light
			float diskSize = 1.0;
			
			//top circle
			MVector topBaseVec(0.0,1.0,-diskSize);
			float stepAngle=360.0/16.0;
			for (uint i=0; i<16; i++) {
				pts.append(topBaseVec.rotateBy(MVector::kYaxis, dommy::math::degreesToRadians(stepAngle*i)));
			}
			pts.append(topBaseVec);
			
			MVector bottomBaseVec(0.0,-1.0,-diskSize);
			for (uint i=0; i<16; i++) {
				pts.append(bottomBaseVec.rotateBy(MVector::kYaxis, dommy::math::degreesToRadians(stepAngle*i)));
			}
			pts.append(bottomBaseVec);
			
			for (uint i=1;i<16; i++) {
				subPts.append(pts[i]);
				subPts.append(pts[i+17]);
			}
			
			
		}  else if (lData->lightType==5) {
			//disk_light
			float diskSize = 1.0;
			
			MVector baseVec(0.0,diskSize,0.0);
			float stepAngle=360.0/16.0;
			for (uint i=0; i<16; i++) {
				pts.append(baseVec.rotateBy(MVector::kZaxis, dommy::math::degreesToRadians(stepAngle*i)));
			}
			pts.append(baseVec);
			
		} else {
			//photometricLight
			float quadSize = 1.0;
			
			pts.append(MPoint(-quadSize, -quadSize, 0.0));
			pts.append(MPoint(-quadSize, quadSize, 0.0));
			pts.append(MPoint(quadSize, quadSize, 0.0));
			pts.append(MPoint(quadSize, -quadSize, 0.0));
			pts.append(MPoint(-quadSize, -quadSize, 0.0));
		}
	}
	
	
	
	
	//Intensity
    dommy::maya::CheckedArray<MPointArray> intensityPts;
    intensityPts.append(MPoint(0.0, 0.0, 0.0));
    intensityPts.append(MPoint(0.0, 0.0, -1.0));
	
	
	MVector baseCAngleVec(0.0,0.0,-1.0);//spot
	
//    MColor col = MHWRender::MGeometryUtilities::wireframeColor(objPath);
	drawManager.setColor( MColor(0.0,1.0,1.0) );
	
    for (uint i=0; i<lData->matrixArray.length(); i++) {
        
        float alphaVal = 1.0;
        if (lData->intensityVis) {
            (void)lightIntensityAlpha(lData->intensityArray[i],
                                      lData->intensityNormalizeVal, alphaVal);
        }
        if (lData->colorVis) {
            drawManager.setColor( MColor(lData->colorArray[i].x,
                                         lData->colorArray[i].y,
                                         lData->colorArray[i].z,
                                         alphaVal));
        } else if (lData->intensityVis) {
            drawManager.setColor( MColor(0.0, 1.0, 1.0,alphaVal) );
        }
        
        
        if(lData->lightVisType==0) {
            //Point
            MPoint basePt(0.0,0.0,0.0);
            const MPoint point = basePt * lData->matrixArray[i];
            if (!dommy::maya::isFloatRepresentablePoint(point)) return;
            drawManager.point(point);
            
            
        } else {
            MPointArray transformedPts;
            
            
            if (lData->lightVisType==2) {
                if (!transformLightPoints(pts, lData->matrixArray[i], transformedPts)) return;
                
                if (lData->lightType==0) {//distant
                    drawManager.lineList(transformedPts, /*2d*/false);
                } else if (lData->lightType==2) {//spot
                    
                    if (!lData->useConeAngleVis) {
                        drawManager.lineList(transformedPts, /*2d*/false);
                        
                        MPointArray transSubPts;
                        if (!transformLightPoints(subPts, lData->matrixArray[i], transSubPts)) return;
                        
                        drawManager.lineStrip(transSubPts, /*2d*/false);
                        
                    } else {
                        
                        
                        //angle line
                        float halfAngle = lData->spotAngleArray[i]*.5;
                        MVector rotVec = baseCAngleVec.rotateBy(MVector::kYaxis, dommy::math::degreesToRadians(halfAngle));
                        MVector invXVec = MVector(-rotVec.x,rotVec.y,rotVec.z);
                        MVector YVec = MVector(rotVec.y,rotVec.x,rotVec.z);
                        MVector invYVec = MVector(rotVec.y,-rotVec.x,rotVec.z);
                        pts.setLength(0U);
                        pts.append(MPoint(0.0, 0.0, 0.0));
                        pts.append(MPoint(rotVec));
                        pts.append(MPoint(0.0, 0.0, 0.0));
                        pts.append(MPoint(invXVec));
                        pts.append(MPoint(0.0, 0.0, 0.0));
                        pts.append(MPoint(YVec));
                        pts.append(MPoint(0.0, 0.0, 0.0));
                        pts.append(MPoint(invYVec));
                        
                        MPointArray conePts;
                        if (!transformLightPoints(pts, lData->matrixArray[i], conePts)) return;
                        drawManager.lineList(conePts, /*2d*/false);
                        
                        //angle circle
                        MVector baseCircleVec(rotVec);
                        float stepAngle=360.0/16.0;
                        subPts.setLength(0U);
                        for (uint i=0; i<16; i++) {
                            subPts.append(baseCircleVec.rotateBy(MVector::kZaxis, dommy::math::degreesToRadians(stepAngle*i)));
                        }
                        subPts.append(baseCircleVec);
                        
                        MPointArray transSubPts;
                        if (!transformLightPoints(subPts, lData->matrixArray[i], transSubPts)) return;
                        
                        drawManager.lineStrip(transSubPts, /*2d*/false);
                        
                    }
                    
                    
                    
                    
                    
                } else if (lData->lightType==4) {//cylinder
                    
                    drawManager.lineStrip(transformedPts, /*2d*/false);
                    
                    MPointArray transSubPts;
                    if (!transformLightPoints(subPts, lData->matrixArray[i], transSubPts)) return;
                    
                    drawManager.lineList(transSubPts, /*2d*/false);
                    
                } else {
                    drawManager.lineStrip(transformedPts, /*2d*/false);
                }
            }
            
            
            
            
            //intensity
            if (lData->lightType!=1 && lData->lightType!=4) {
                transformedPts.clear();
                if (!transformLightPoints(intensityPts, lData->matrixArray[i], transformedPts)) return;
                drawManager.lineList(transformedPts, /*2d*/false);
            }
        }
        
    }
    
}
DOMMY_VIEWPORT_VOID_CATCH("DPLightGenOverride", "addUIDrawables")
