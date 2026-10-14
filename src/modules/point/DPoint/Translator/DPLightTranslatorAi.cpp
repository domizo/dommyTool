//
//  DPLightTranslatorAi.cpp
//  dommyTool
//
//  Created by yuzo on 2017/03/27.
//
//

#include "DPLightTranslatorAi.h"
#include "common/utility/DMayaPlugDataHandle.h"
#include "common/string/DStreamFormat.h"


#include <ai_msg.h>
#include <ai_node_entry.h>
#include <ai_nodes.h>
#include <ai_universe.h>
#include <maya/MUuid.h>
#include "DMayaHeaders.h"
#include "DPointDataHeaders.h"
#include "modules/point/DPoint/Common/DPAttributeAccess.h"
#include "common/utility/DAttributeArrayConversion.h"

#include "DPLightGen.h"

#include "DNodeOperation.h"
#include "DType.h"
#include "integrations/arnold/DArnoldEntryPointGuard.h"
#include "integrations/arnold/DArnoldResources.h"
#include "integrations/arnold/DArnoldArrayBudget.h"
#include "integrations/arnold/DArnoldUserParameters.h"
#include "integrations/arnold/DArnoldNumeric.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <initializer_list>
#include <locale>
#include <sstream>
#include <string>
#include <utility>
#include <vector>
#include <unordered_set>

namespace {

const char* const kLightOwnerTag = "dlightTag";

bool allFinite(std::initializer_list<double> values)
{
    for (std::initializer_list<double>::const_iterator it = values.begin();
         it != values.end(); ++it) {
        if (!std::isfinite(*it)) {
            return false;
        }
    }
    return true;
}

bool isFiniteVector(const MVector& value)
{
    return allFinite({value.x, value.y, value.z});
}

bool isFiniteMatrix(const MMatrix& value)
{
    for (unsigned int row = 0; row < 4U; ++row) {
        for (unsigned int column = 0; column < 4U; ++column) {
            if (!std::isfinite(value[row][column])) {
                return false;
            }
        }
    }
    return true;
}


std::string generatedNodePrefix(const MUuid& nodeId)
{
    std::string prefix("dommy_dlight_");
    prefix += nodeId.asString().asChar();
    prefix += "_";
    std::replace(prefix.begin(), prefix.end(), '-', '_');
    return prefix;
}

bool collectGeneratedNodes(
    const std::string& ownerId, std::vector<AtNode*>& nodes)
{
    if (ownerId.empty()) return false;
    dommy::arnold::NodeIteratorPtr allNodes(
        AiUniverseGetNodeIterator(AI_NODE_ALL));
    if (!allNodes) {
        AiMsgError("[dommyTool] Unable to enumerate Arnold nodes.");
        return false;
    }
    std::vector<AtNode*> staged;
    std::unordered_set<AtNode*> seen;
    while (!AiNodeIteratorFinished(allNodes.get())) {
        AtNode* const candidate = AiNodeIteratorGetNext(allNodes.get());
        if (!candidate) continue;
        // Current generated lights, textures and filters and legacy lights
        // carry this tag. A name prefix is never deletion authority.
        const AtUserParamEntry* const parameter =
            AiNodeLookUpUserParameter(candidate, kLightOwnerTag);
        if (!parameter || AiUserParamGetType(parameter) != AI_TYPE_STRING ||
            AiUserParamGetCategory(parameter) != AI_USERDEF_CONSTANT) continue;
        const AtString tag = AiNodeGetStr(candidate, kLightOwnerTag);
        if (tag && ownerId == tag.c_str() && seen.insert(candidate).second) {
            staged.push_back(candidate);
        }
    }
    nodes.swap(staged);
    return true;
}

bool destroyGeneratedNodes(const std::vector<AtNode*>& nodes)
{
    // These are the translator's fixed generated roles, not an arbitrary shader
    // graph: lights consume gobo/image, and gobo consumes image. Classify and
    // allocate the complete plan before any deletion; universe iteration order
    // and node names do not establish dependency order or deletion authority.
    std::vector<std::pair<int, AtNode*>> ordered;
    ordered.reserve(nodes.size());
    std::unordered_set<AtNode*> seen;
    for (AtNode* const node : nodes) {
        if (!node) return false;
        if (!seen.insert(node).second) continue;
        const AtNodeEntry* const entry = AiNodeGetNodeEntry(node);
        if (!entry) return false;
        const int type = AiNodeEntryGetType(entry);
        int priority = -1;
        if (type == AI_NODE_LIGHT) {
            priority = 0;
        } else if (type == AI_NODE_SHADER) {
            const char* const role = AiNodeEntryGetName(entry);
            if (role && std::strcmp(role, "gobo") == 0) priority = 1;
            else if (role && std::strcmp(role, "image") == 0) priority = 2;
        }
        if (priority < 0) {
            AiMsgWarning("[dommyTool] Unknown generated light-node role; refusing retirement.");
            return false;
        }
        ordered.emplace_back(priority, node);
    }
    std::sort(ordered.begin(), ordered.end(),
        [](const std::pair<int, AtNode*>& a, const std::pair<int, AtNode*>& b) {
            return a.first < b.first;
        });
    for (const auto& candidate : ordered) {
        if (!AiNodeDestroy(candidate.second)) {
            AiMsgWarning("[dommyTool] Failed to delete a generated light node.");
            // A failed child and its dependencies retain their ownership tags
            // for a fresh live-universe collection on the next update.
            return false;
        }
    }
    return true;
}

void tagGeneratedNode(AtNode* node, const std::string& ownerId)
{
    dommy::arnold::requireOwnershipTag(node, kLightOwnerTag);
    AiNodeSetStr(node, kLightOwnerTag, ownerId.c_str());
}

}  // namespace

struct DPLightTranslatorAi::Parameters {
    short lightType = 0;
    double distantAngle = 0.0;
    double lensRadius = 0.0;
    double coneAngle = 0.0;
    double penumbraAngle = 0.0;
    double cosinePower = 0.0;
    double aspectRatio = 1.0;
    bool useGobo = false;
    std::string goboTexture;
    short goboFilter = 0;
    double goboDensity = 1.0;
    std::array<double, 2> goboOffset = {{0.0, 0.0}};
    std::array<double, 2> goboScale = {{1.0, 1.0}};
    short goboWrap = 0;
    int resolution = 0;
    double spread = 0.0;
    std::string lightTexture;
    std::array<double, 2> repeatTexture = {{1.0, 1.0}};
    std::array<double, 2> offsetTexture = {{0.0, 0.0}};
    std::string photometricFilename;
    double intensity = 1.0;
    double exposure = 0.0;
    MVector color;
    short decayType = 0;
    int samples = 0;
    double lightRadius = 0.0;
    bool normalize = false;
    bool castShadow = false;
    MVector shadowColor;
    double shadowDensity = 1.0;
    bool affectVolumetrics = false;
    bool castVolumeShadow = false;
    int volumeSamples = 0;
    bool affectDiffuse = true;
    bool affectSpecular = true;
    double diffuse = 1.0;
    double specular = 1.0;
    double sss = 1.0;
    double indirect = 1.0;
    double volume = 1.0;
    int maxBounces = 0;
    std::string aovLightGroup;
    short directionType = 0;
    MVector baseDirection;
    std::string directionAttribute;
    std::string intensityAttribute;
    std::string colorAttribute;
    std::string spotAngleAttribute;
    std::string scaleAttribute;
    std::string animationMatrixAttribute;
    int applyAnimationMatrixType = 0;
    MVector animationLookAt;
    MVector globalScale;
};

template <typename Value>
bool DPLightTranslatorAi::readParameter(const char* name, Value& value)
{
    const MPlug plug = FindMayaPlug(name);
    if (plug.isNull() || readPlugValue(plug, value) != MS::kSuccess) {
        AiMsgError("[dommyTool] Unable to read DPLight parameter: %s", name);
        return false;
    }
    return true;
}

bool DPLightTranslatorAi::readVector2Parameter(
    const char* name, std::array<double, 2>& value)
{
    const MPlug plug = FindMayaPlug(name);
    MStatus status;
    if (plug.isNull() || plug.numChildren(&status) != 2U ||
        status != MS::kSuccess) {
        AiMsgError("[dommyTool] Invalid DPLight vector2 parameter: %s", name);
        return false;
    }

    std::array<double, 2> candidate = {{0.0, 0.0}};
    for (unsigned int index = 0; index < 2U; ++index) {
        const MPlug child = plug.child(index, &status);
        if (status != MS::kSuccess || child.isNull() ||
            child.getValue(candidate[index]) != MS::kSuccess) {
            AiMsgError("[dommyTool] Unable to read DPLight parameter: %s", name);
            return false;
        }
    }
    value = candidate;
    return true;
}

bool DPLightTranslatorAi::readStringParameter(
    const char* name, std::string& value)
{
    MString candidate;
    if (!readParameter(name, candidate)) {
        return false;
    }
    value = candidate.asChar();
    return true;
}

bool DPLightTranslatorAi::readParameters(Parameters& parameters)
{
    if (!readParameter("LightType", parameters.lightType) ||
        !readParameter("DistantAngle", parameters.distantAngle) ||
        !readParameter("LensRadius", parameters.lensRadius) ||
        !readParameter("ConeAngle", parameters.coneAngle) ||
        !readParameter("PenumbraAngle", parameters.penumbraAngle) ||
        !readParameter("CosinePower", parameters.cosinePower) ||
        !readParameter("AspectRatio", parameters.aspectRatio) ||
        !readParameter("UseGobo", parameters.useGobo) ||
        !readStringParameter("GoboTexture", parameters.goboTexture) ||
        !readParameter("GoboFilter", parameters.goboFilter) ||
        !readParameter("GoboDensity", parameters.goboDensity) ||
        !readVector2Parameter("GoboOffset", parameters.goboOffset) ||
        !readVector2Parameter("GoboScale", parameters.goboScale) ||
        !readParameter("GoboWrap", parameters.goboWrap) ||
        !readParameter("Resolution", parameters.resolution) ||
        !readParameter("Spread", parameters.spread) ||
        !readStringParameter("LightTexture", parameters.lightTexture) ||
        !readVector2Parameter("RepeatTex", parameters.repeatTexture) ||
        !readVector2Parameter("OffsetTex", parameters.offsetTexture) ||
        !readStringParameter(
            "PhotometricFileName", parameters.photometricFilename) ||
        !readParameter("Intensity", parameters.intensity) ||
        !readParameter("Exposure", parameters.exposure) ||
        !readParameter("Color", parameters.color) ||
        !readParameter("DecayType", parameters.decayType) ||
        !readParameter("Samples", parameters.samples) ||
        !readParameter("LightRadius", parameters.lightRadius) ||
        !readParameter("Normalize", parameters.normalize) ||
        !readParameter("CastShadow", parameters.castShadow) ||
        !readParameter("ShadowColor", parameters.shadowColor) ||
        !readParameter("ShadowDensity", parameters.shadowDensity) ||
        !readParameter(
            "AffectVolumetrics", parameters.affectVolumetrics) ||
        !readParameter(
            "CastVolumeShadow", parameters.castVolumeShadow) ||
        !readParameter("VolumeSamples", parameters.volumeSamples) ||
        !readParameter("AffectDiffuse", parameters.affectDiffuse) ||
        !readParameter("AffectSpecular", parameters.affectSpecular) ||
        !readParameter("Diffuse", parameters.diffuse) ||
        !readParameter("Specular", parameters.specular) ||
        !readParameter("Sss", parameters.sss) ||
        !readParameter("Indirect", parameters.indirect) ||
        !readParameter("Volume", parameters.volume) ||
        !readParameter("MaxBounces", parameters.maxBounces) ||
        !readStringParameter("AOVLightGroup", parameters.aovLightGroup) ||
        !readParameter("DirectionType", parameters.directionType) ||
        !readParameter("BaseDirection", parameters.baseDirection) ||
        !readStringParameter(
            "DirectionAttribute", parameters.directionAttribute) ||
        !readStringParameter(
            "IntensityAttribute", parameters.intensityAttribute) ||
        !readStringParameter("ColorAttribute", parameters.colorAttribute) ||
        !readStringParameter(
            "SpotAngleAttribute", parameters.spotAngleAttribute) ||
        !readStringParameter("ScaleAttribute", parameters.scaleAttribute) ||
        !readStringParameter(
            "AnimMatrixAttribute", parameters.animationMatrixAttribute) ||
        !readParameter(
            "ApplyAnimMatType", parameters.applyAnimationMatrixType) ||
        !readParameter("AnimLookPos", parameters.animationLookAt) ||
        !readParameter("GlobalScale", parameters.globalScale)) {
        return false;
    }

    const bool finiteScalars = allFinite({
        parameters.distantAngle,
        parameters.lensRadius,
        parameters.coneAngle,
        parameters.penumbraAngle,
        parameters.cosinePower,
        parameters.aspectRatio,
        parameters.goboDensity,
        parameters.goboOffset[0],
        parameters.goboOffset[1],
        parameters.goboScale[0],
        parameters.goboScale[1],
        parameters.spread,
        parameters.repeatTexture[0],
        parameters.repeatTexture[1],
        parameters.offsetTexture[0],
        parameters.offsetTexture[1],
        parameters.intensity,
        parameters.exposure,
        parameters.lightRadius,
        parameters.shadowDensity,
        parameters.diffuse,
        parameters.specular,
        parameters.sss,
        parameters.indirect,
        parameters.volume});

    if (!finiteScalars || !isFiniteVector(parameters.color) ||
        !isFiniteVector(parameters.shadowColor) ||
        !isFiniteVector(parameters.baseDirection) ||
        !isFiniteVector(parameters.animationLookAt) ||
        !isFiniteVector(parameters.globalScale) ||
        parameters.lightType < 0 || parameters.lightType > 6 ||
        parameters.goboFilter < 0 || parameters.goboFilter > 4 ||
        parameters.goboWrap < 0 || parameters.goboWrap > 4 ||
        parameters.decayType < 0 || parameters.decayType > 1 ||
        parameters.directionType < 0 || parameters.directionType > 1 ||
        parameters.applyAnimationMatrixType < DP_MAT_APPLY_NONE ||
        parameters.applyAnimationMatrixType >= DP_MAT_APPLY_TYPE_COUNT ||
        parameters.resolution < 0 || parameters.samples < 0 ||
        parameters.volumeSamples < 0 || parameters.maxBounces < 0 ||
        parameters.lensRadius < 0.0 || parameters.lightRadius < 0.0 ||
        parameters.aspectRatio <= 0.0) {
        AiMsgError("[dommyTool] DPLight contains invalid parameter values.");
        return false;
    }
    return true;
}

AtNode* DPLightTranslatorAi::CreateArnoldNodes() try
{
    return AddArnoldNode("points");
}
catch (...) {
    ::dommy::arnold::reportTranslatorException(
        "DPLightTranslatorAi::CreateArnoldNodes", nullptr, std::current_exception());
    return nullptr;
}

void* DPLightTranslatorAi::creator()
{
    return ::dommy::arnold::createObject("DPLightTranslatorAi", [&]() {
        return new DPLightTranslatorAi();
    });
}


void DPLightTranslatorAi::Export(AtNode* node) try
{
    AiMsgDebug("[dommyTool] Exporting %s", GetMayaNodeName().asChar());
    Update(node);
}
DOMMY_ARNOLD_TRANSLATOR_CATCH("DPLightTranslatorAi::Export", node)

void DPLightTranslatorAi::NodeChanged(MObject& node, MPlug& plug) try
{
    MStatus status;
    MFnDependencyNode nodeFn(node, &status);
    if (!status) {
        AiMsgError("[DPLightTranslatorAi] Unable to inspect the changed Maya node.");
        return;
    }
    
    const MString pName = plug.partialName(
        false, false, false, false, false, true, &status);
    if (!status) {
        AiMsgError("[DPLightTranslatorAi] Unable to read the changed Maya plug name.");
        return;
    }
    if(pName == "inDP" ||
       pName == "LightType" || pName == "DistantAngle" || pName == "LensRadius" || pName == "ConeAngle" ||
       pName == "PenumbraAngle" || pName == "CosinePower" || pName == "AspectRatio" ||
       pName == "UseGobo" || pName == "GoboTexture" || pName == "GoboFilter" ||
       pName == "GoboDensity" || pName == "GoboOffset" || pName == "GoboScale" || pName == "GoboWrap" ||
       pName == "Resolution" || pName == "Spread" || pName == "LightTexture" || pName == "RepeatTex" || pName == "OffsetTex" ||
       pName == "PhotometricFileName" ||
       pName == "Intensity" || pName == "Exposure" || pName == "Color" ||
       pName == "DecayType" || pName == "Samples" || pName == "LightRadius" || pName == "Normalize" ||
       pName == "CastShadow" || pName == "ShadowColor" || pName == "ShadowDensity" ||
       pName == "AffectVolumetrics" || pName == "CastVolumeShadow" || pName == "VolumeSamples" ||
       pName == "AffectDiffuse" || pName == "AffectSpecular" ||
       pName == "Diffuse" || pName == "Specular" || pName == "Sss" || pName == "Indirect" || pName == "Volume" ||
       pName == "MaxBounces" ||
       pName == "AOVLightGroup" ||
       pName == "DirectionType" || pName == "BaseDirection" || pName == "DirectionAttribute" ||
       pName == "IntensityAttribute" || pName == "ColorAttribute" || pName == "SpotAngleAttribute" || pName == "ScaleAttribute" || pName == "AnimMatrixAttribute" ||
       pName == "ApplyAnimMatType" || pName == "AnimLookPos" ||
	   pName == "GlobalScale") {

        
        SetUpdateMode(CNodeTranslator::AI_RECREATE_TRANSLATOR);
//        SetUpdateMode(CNodeTranslator::AI_DELETE_NODE);
        RequestUpdate();
    }
    
}
DOMMY_ARNOLD_TRANSLATOR_CATCH("DPLightTranslatorAi::NodeChanged", nullptr)
void DPLightTranslatorAi::Update(AtNode* node) try
{
    MStatus inputStatus;
    const MFnDependencyNode nodeFn(GetMayaObject(), &inputStatus);
    if (!inputStatus) {
        AiMsgError("[dommyTool] Unable to bind the Arnold light Maya node.");
        return;
    }
    const MUuid nodeId = nodeFn.uuid(&inputStatus);
    if (!inputStatus || !nodeId.valid()) {
        AiMsgError("[dommyTool] Unable to identify the Arnold light owner.");
        return;
    }
    MPlug dpPlug = FindMayaPlug("inDP");
    if (dpPlug.isNull()) {
        AiMsgError("[dommyTool] Unable to find the Arnold light DPoint input.");
        return;
    }
    const bool inputConnected = dpPlug.isConnected(&inputStatus);
    if (!inputStatus) {
        AiMsgError("[dommyTool] Unable to query the Arnold light input connection.");
        return;
    }
	const std::string ownerId = nodeId.asString().asChar();
	const std::string nodePrefix = generatedNodePrefix(nodeId);
	// Retire failed staging before collecting old tagged nodes. Otherwise the
	// collection could retain pointers destroyed by the pending-batch retry.
	if (!pendingNodes_.retire()) {
		AiMsgError("[DPLightTranslatorAi] Previous incomplete nodes could not be retired.");
		return;
	}
	std::vector<AtNode*> previousNodes;
	if (!collectGeneratedNodes(ownerId, previousNodes)) {
		return;
	}
	
    ExportMatrix(node);
    AiNodeSetByte( node, "visibility", ComputeVisibility() );
    
    
    // Only a successful query may retire the previous generated lights.
    if (!inputConnected) {
		destroyGeneratedNodes(previousNodes);
		return;
	}
        
        dommy::maya::ScopedPlugDataHandle dPointHndOwner;
        MDataHandle& dPointHnd = dPointHndOwner.handle();
        if (dPointHndOwner.read(dpPlug) != MS::kSuccess) {
            AiMsgError("[dommyTool] Unable to read Arnold light DPoint input.");
            return;
        }
        const DPData* const dPoints(getInputDPfromHandle(dPointHnd));
		if (dPoints == nullptr) {
			AiMsgError("[dommyTool] DPLight input does not contain valid DPoint data.");
			return;
		}
		if (dPoints->numberOfPoints() == 0U) {
			destroyGeneratedNodes(previousNodes);
			return;
		}
        
        {
            
//            size_t totalCount = dPoints->numberOfPoints();
			
            
            
            
            //get point
            const DPointList *ptsPtr = dPoints->getPositionPtrConst();
            
			Parameters parameters;
			if (!readParameters(parameters)) {
				return;
			}
            const std::size_t auxiliaryNodes =
                parameters.lightType == 2 && parameters.useGobo && !parameters.goboTexture.empty() ? 2U :
                parameters.lightType == 3 && !parameters.lightTexture.empty() ? 1U : 0U;
            dommy::arnold::NodeGenerationLayout generationLayout;
            if (!dommy::arnold::checkedNodeGenerationLayout(
                    ptsPtr->size(), 1U, 0U, 0U, auxiliaryNodes, generationLayout)) {
                AiMsgError("[DPLightTranslatorAi] Light generation exceeds its node/work budget.");
                return;
            }
            dommy::arnold::NodeBatchScope generationScope(pendingNodes_);
            pendingNodes_.reserve(generationLayout.nodeCount);

            //get attribute
            const short lightTypeVal = parameters.lightType;
            //distant
            const double distAngleVal = parameters.distantAngle;
            
            //spot
            const double lensRadiusVal = parameters.lensRadius;
            const double coneAngleVal = parameters.coneAngle;
            const double penumbraAngleVal = parameters.penumbraAngle;
            const double cosinePowerVal = parameters.cosinePower;
            const double aspectRatioVal = parameters.aspectRatio;
            
            //spot gobo
            const bool useGoboVal = parameters.useGobo;
            const std::string& goboTexName = parameters.goboTexture;
            const short goboFilterTypeVal = parameters.goboFilter;
            const double goboDensityVal = parameters.goboDensity;
            const std::array<double, 2>& goboOffsetVal = parameters.goboOffset;
            const std::array<double, 2>& goboScaleVal = parameters.goboScale;
            const short goboWrapTypeVal = parameters.goboWrap;
            
            
            
            //area quad cylinder disk
            const int resolutionVal = parameters.resolution;
            const double spreadVal = parameters.spread;
            const std::string& lightTextureName = parameters.lightTexture;
            //	const double rotateTex = data.inputValue(aRotateTex).asDouble();
            const std::array<double, 2>& repeatTexVal =
				parameters.repeatTexture;
            const std::array<double, 2>& offsetTexVal =
				parameters.offsetTexture;
            
            
            //photometric
            const std::string& photometricFileName =
				parameters.photometricFilename;
            
            //common
            const double intensity = parameters.intensity;
            const double exposure = parameters.exposure;
            const MVector colorVal = parameters.color;
            
            const short decayTypeVal = parameters.decayType;
            const int sampleVal = parameters.samples;
            const double lightRadiusVal = parameters.lightRadius;
            const bool normalizeVal = parameters.normalize;
            
            const bool castShadowVal = parameters.castShadow;
            const MVector shadowColorVal = parameters.shadowColor;
            const double shadowDensityVal = parameters.shadowDensity;
            
            const bool affectVolumetricsVal = parameters.affectVolumetrics;
            const bool castVolumeShadowVal = parameters.castVolumeShadow;
            const int volumeSamplesVal = parameters.volumeSamples;
            
            const bool affectDiffuseVal = parameters.affectDiffuse;
            const bool affectSpecularVal = parameters.affectSpecular;
            const double diffuseVal = parameters.diffuse;
            const double specularVal = parameters.specular;
            const double sssVal = parameters.sss;
            const double indirectVal = parameters.indirect;
            const double volumeVal = parameters.volume;
            
            //mac bounce
            const int maxBouncesVal = parameters.maxBounces;
            
            //aov
            const std::string& aovLightGroupName = parameters.aovLightGroup;
            
            //direction
            const short dirTypeVal = parameters.directionType;
            const MVector baseDirection = parameters.baseDirection;
            const std::string& directionAttributeName =
				parameters.directionAttribute;
            
            //attribute inherit
            const std::string& intensityAttributeName =
				parameters.intensityAttribute;
            const std::string& colorAttributeName = parameters.colorAttribute;
            const std::string& spotAngleAttributeName =
				parameters.spotAngleAttribute;
            const std::string& scaleAttributeName = parameters.scaleAttribute;
            const std::string& animMatAttributeName =
				parameters.animationMatrixAttribute;
            
            
            //animation
            const int applyAnimMatType =
				parameters.applyAnimationMatrixType;
            const MVector lookAtVec = parameters.animationLookAt;
            
            
            
#pragma mark read attribute
            
            
            
            DScalarDataList intensityArray(ptsPtr->size(), 0.0f);
            if (dPoints->isExistAttr(intensityAttributeName)) {
                if (!getCheckedDScalarArrayFromAttribute(
                        *dPoints, intensityAttributeName, ptsPtr->size(), intensityArray)) {
                    AiMsgError("[DPLightTranslatorAi] Unable to read the numeric intensity attribute.");
                    return;
                }
            } else {
                std::fill(intensityArray.begin(), intensityArray.end(),
                          dommy::arnold::checkedFloat(intensity));
            }
            
            DVec3DataList colorArray(ptsPtr->size(), DVec3Data(
                dommy::arnold::checkedFloat(colorVal[0]),
                dommy::arnold::checkedFloat(colorVal[1]),
                dommy::arnold::checkedFloat(colorVal[2])));
            if (dPoints->isExistAttr(colorAttributeName)) {
                if (!dommy::attribute::convertVector3Attribute(
                        dPoints->getAttr(colorAttributeName), ptsPtr->size(), colorArray)) {
                    AiMsgError("[DPLightTranslatorAi] Unable to read the Vec3 color attribute.");
                    return;
                }
            }
            
            DScalarDataList spotAngleArray(ptsPtr->size(), dommy::arnold::checkedFloat(coneAngleVal));
            if (dPoints->isExistAttr(spotAngleAttributeName)) {
                if (!getCheckedDScalarArrayFromAttribute(
                        *dPoints, spotAngleAttributeName, ptsPtr->size(), spotAngleArray)) {
                    AiMsgError("[DPLightTranslatorAi] Unable to read the numeric cone-angle attribute.");
                    return;
                }
            }
            
            DScalarDataList scaleArray(ptsPtr->size(), 1.0);
            if (dPoints->isExistAttr(scaleAttributeName)) {
                if (!getCheckedDScalarArrayFromAttribute(
                        *dPoints, scaleAttributeName, ptsPtr->size(), scaleArray)) {
                    AiMsgError("[DPLightTranslatorAi] Unable to read the numeric scale attribute.");
                    return;
                }
            }
            
            MMatrixArray animMatArray(ptsPtr->size());
            if (dPoints->isExistAttr(animMatAttributeName)) {
                if (!getCheckedMMatrixArrayFromAttribute(
                        *dPoints, animMatAttributeName, ptsPtr->size(), animMatArray)) {
                    AiMsgError("[DPLightTranslatorAi] Unable to read the Matrix animation attribute.");
                    return;
                }
            }
            
            
#pragma mark read direction for transform
            
            //transform
            const MVector globalScale = parameters.globalScale;
			
            
            MVectorArray lightDirVecArray(ptsPtr->size(), baseDirection);
            
            if (dirTypeVal==1 && dPoints->isExistAttr(directionAttributeName)) {
                if (!getCheckedMVectorArrayFromAttribute(
                        *dPoints, directionAttributeName, ptsPtr->size(), lightDirVecArray)) {
                    AiMsgError("[DPLightTranslatorAi] Unable to read the Vec3 direction attribute.");
                    return;
                }
            }

			for (size_t index = 0; index < ptsPtr->size(); ++index) {
				const DPoint& point = (*ptsPtr)[index];
				const DVec3Data& pointColor = colorArray[index];
				if (!allFinite({
						point[0], point[1], point[2],
						intensityArray[index],
						pointColor[0], pointColor[1], pointColor[2],
						spotAngleArray[index], scaleArray[index]}) ||
					!isFiniteVector(lightDirVecArray[index]) ||
					!std::isfinite(lightDirVecArray[index].length()) ||
					lightDirVecArray[index].length() <= 1.0e-12 ||
					!isFiniteMatrix(animMatArray[index])) {
					AiMsgError(
						"[dommyTool] DPLight point attributes contain invalid values.");
					return;
				}
			}

            
#pragma mark Export arnold
            
            
            
            const std::string lightTextureNodeName = nodePrefix + "texture";
            const std::string goboNodeName = nodePrefix + "gobo";

            AtNode* texNode = nullptr;
            AtNode* goboNode = nullptr;
            
            if(lightTypeVal==2 && useGoboVal && !goboTexName.empty()) {
                //create Gobo
                
                texNode = pendingNodes_.create("image");
				if (!texNode) {
					AiMsgError("[dommyTool] Unable to create the gobo image node.");
					return;
				}
				tagGeneratedNode(texNode, ownerId);
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
                
                
                goboNode = pendingNodes_.create("gobo");
				if (!goboNode) {
					AiMsgError("[dommyTool] Unable to create the gobo filter node.");
					return;
				}
				tagGeneratedNode(goboNode, ownerId);
                
                AiNodeSetVec2(goboNode, "offset", dommy::arnold::checkedFloat(goboOffsetVal[0]), dommy::arnold::checkedFloat(goboOffsetVal[1]));
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
                    AiMsgError("[dommyTool] DPLight gobo texture link failed.");
                    return;
                }
                
            } else if (lightTypeVal==3 && !lightTextureName.empty()) {
                //create Area light texture
                texNode = pendingNodes_.create("image");
				if (!texNode) {
					AiMsgError("[dommyTool] Unable to create the light image node.");
					return;
				}
				tagGeneratedNode(texNode, ownerId);
                
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
            
            
#pragma mark create light data
			std::vector<AtNode*> lightNodes;
			lightNodes.reserve(ptsPtr->size());
			std::vector<std::string> lightNodeNames;
			lightNodeNames.reserve(ptsPtr->size());
			size_t i=0;
			for (auto pIt=dPoints->beginPt();pIt!=dPoints->endPt();++pIt) {
                // create light node
                
                float finMatVal[4][4];
                MMatrix lightMatrix;
                const MStatus transformStatus = dommy::light::buildTransform(
                    lightTypeVal, applyAnimMatType,
                    MVector((*pIt)[0], (*pIt)[1], (*pIt)[2]), lightDirVecArray[i],
                    globalScale, scaleArray[i], animMatArray[i], lookAtVec, lightMatrix);
                if (!transformStatus) {
                    AiMsgError("[dommyTool] DPLight transform could not be prepared.");
                    return;
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
					AtNode* const light = pendingNodes_.create(lightNodeType);
					if (!light) {
						AiMsgError("[dommyTool] Unable to create an Arnold light node.");
						return;
					}

	                if (lightTypeVal==0) {
	                    AiNodeSetMatrix(light, "matrix", finMat);
	                    AiNodeSetFlt(light, "angle", dommy::arnold::checkedFloat(distAngleVal));
                    
	                    
	                } else if(lightTypeVal==1) {
	                    //position
	                    AiNodeSetVec(light, "position", (*pIt)[0], (*pIt)[1], (*pIt)[2]);
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
	                    if (globalScale[0]>globalScale[2]) {
	                        AiNodeSetFlt(light, "radius", dommy::arnold::checkedFloatProduct(globalScale[0], scaleArray[i]));
	                    } else {
	                        AiNodeSetFlt(light, "radius", dommy::arnold::checkedFloatProduct(globalScale[2], scaleArray[i]));
	                    }
                    
	                    
	                } else if(lightTypeVal==5) {
	                    AiNodeSetMatrix(light, "matrix", finMat);
                    
                    AiNodeSetFlt(light, "spread", dommy::arnold::checkedFloat(spreadVal));
	                    
	                    //disk radius はxyのスケール値の最小値を設定
	                    if (globalScale[0]>globalScale[1]) {
	                        AiNodeSetFlt(light, "radius", dommy::arnold::checkedFloatProduct(globalScale[0], scaleArray[i]));
	                    } else {
	                        AiNodeSetFlt(light, "radius", dommy::arnold::checkedFloatProduct(globalScale[1], scaleArray[i]));
	                    }
                    
	                    
	                } else if(lightTypeVal==6) {
	                    AiNodeSetMatrix(light, "matrix", finMat);
                    AiNodeSetStr(light, "filename", photometricFileName.c_str());
                    AiNodeSetFlt(light, "radius", dommy::arnold::checkedFloat(lightRadiusVal));
	                    
	                } else {
	                    AiNodeSetVec(light, "position", (*pIt)[0], (*pIt)[1], (*pIt)[2]);
	                }
                
                
#pragma mark set name
	                std::ostringstream stream;
	                stream.imbue(std::locale::classic());
	                stream << nodePrefix << "light_" << i;
	                lightNodeNames.push_back(dommy::text::checkedStreamText(stream, "Arnold light name formatting failed"));
					
#pragma mark set light tag
					
					tagGeneratedNode(light, ownerId);
				
				
                AiNodeSetFlt(light, "intensity", dommy::arnold::checkedFloat(intensityArray[i]));
                
                if (lightTypeVal==3 && !lightTextureName.empty() && texNode) {
                    //quadlight color is texture
                    if (!AiNodeLink(texNode, "color", light)) {
                        AiMsgError("[dommyTool] DPLight color texture link failed.");
                        return;
                    }
                } else {
                    AiNodeSetRGB(light, "color", colorArray[i][0], colorArray[i][1], colorArray[i][2]);
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
					shadowColorVal[0], shadowColorVal[1], shadowColorVal[2]);
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
                        return;
                    }
                    dommy::arnold::requireArrayWrite(AiArraySetPtr(filterArray.get(), 0, goboNode));
                    AiNodeSetArray(
                        light, "filters", filterArray.release());
                    
                    
                }
				lightNodes.push_back(light);
				i++;
					
            }

			if (!destroyGeneratedNodes(previousNodes)) {
				AiMsgError(
					"[dommyTool] DPLight update was not committed because "
					"the previous generated nodes could not be removed.");
				return;
			}

			if (texNode) {
				AiNodeSetStr(
					texNode, "name",
					lightTextureNodeName.c_str());
			}
			if (goboNode) {
				AiNodeSetStr(
					goboNode, "name", goboNodeName.c_str());
			}
			for (size_t index = 0; index < lightNodes.size(); ++index) {
				AiNodeSetStr(
					lightNodes[index], "name",
					lightNodeNames[index].c_str());
			}
            generationScope.commit();
				
        }//check point count
	    
}
DOMMY_ARNOLD_TRANSLATOR_CATCH("DPLightTranslatorAi::Update", node)
