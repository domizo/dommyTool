//
//  DPInstancerTranslatorAi.cpp
//  dommyTool
//
//  Created by yuzo on 2017/06/25.
//

#include "DPInstancerTranslatorAi.h"
#include "common/utility/DMayaPlugDataHandle.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DMayaNameList.h"



#include <ai_msg.h>
#include <ai_nodes.h>
#include <ai_universe.h>
#include <maya/MUuid.h>

#include "DMayaHeaders.h"
#include "DPointDataHeaders.h"

#include "DNodeOperation.h"
#include "DStringOp.h"

#include "DPInstancer.h"
#include "DLogConfig.h"
#include "integrations/arnold/DArnoldArrayBudget.h"
#include "integrations/arnold/DArnoldEntryPointGuard.h"
#include "integrations/arnold/DArnoldMayaPlug.h"
#include "integrations/arnold/DArnoldResources.h"
#include "integrations/arnold/DArnoldUserParameters.h"
#include "integrations/arnold/DArnoldNumeric.h"
#include "integrations/arnold/DArnoldRayFlags.h"
#include "common/utility/DMayaMeshImport.h"
#include "common/utility/DMeshInputValidation.h"
#include "common/utility/DMeshUVTransfer.h"

#include <algorithm>
#include <cstring>
#include <sstream>
#include <utility>
#include <unordered_set>

#define INST_VERBOSE DOMMY_ENABLE_DEBUG_LOGS
#define INST_DEBUG(streamExpression)                                         \
    do {                                                                     \
        if (INST_VERBOSE) {                                                  \
            std::ostringstream dommyArnoldDebugStream;                       \
            dommyArnoldDebugStream << streamExpression;                      \
            AiMsgDebug(                                                      \
                "[DPInstancerTranslatorAi] %s",                              \
                dommyArnoldDebugStream.str().c_str());                       \
        }                                                                    \
    } while (false)

struct ReplaceShaderData
{
	AtNode* shaderNode = nullptr;
	std::vector<std::string> shapeNameList;
};

namespace {

// References have already passed live ownership validation. Resolve each one
// once, preserving first-row/first-match priority without copying its name or
// retaining a new persistent cache. No generated instance exists at this stage.
bool buildReplacementShaderPlan(
    const std::vector<AtNode*>& references,
    const std::vector<ReplaceShaderData>& rows,
    dommy::maya::NameListBudget& budget, std::vector<AtNode*>& result)
{
    using namespace dommy::generation;
    if (references.size() > kMaxGeneratedTransformCount ||
        budget.bytes > kMaxGeneratedStringByteCount ||
        budget.names > kMaxGeneratedStringCount ||
        budget.workItems > kMaxWorkItemCount) {
        return false;
    }
    if (rows.empty()) {
        result.clear();
        return true;
    }
    dommy::maya::NameListBudget candidate = budget;
    WorkBudget work(kMaxWorkItemCount - candidate.workItems);
    if (!work.consume(references.size())) {
        return false;
    }
    std::vector<AtNode*> staged;
    staged.reserve(references.size());
    for (AtNode* const reference : references) {
        if (!reference) {
            return false;
        }
        const char* const name = AiNodeGetName(reference);
        if (!name) {
            return false;
        }
        std::size_t bytes = 0U;
        const std::size_t remainingBytes = kMaxGeneratedStringByteCount - candidate.bytes;
        while (name[bytes] != '\0') {
            if (bytes >= remainingBytes || !work.consume(1U)) {
                return false;
            }
            ++bytes;
        }
        candidate.bytes += bytes; // Bounded above before every increment.
        AtNode* selected = nullptr;
        bool matched = false;
        for (const ReplaceShaderData& row : rows) {
            if (!work.consume(1U)) {
                return false;
            }
            for (const std::string& target : row.shapeNameList) {
                if (!work.consume(1U)) {
                    return false;
                }
                if (target.size() != bytes) {
                    continue;
                }
                if (!work.consume(bytes)) {
                    return false;
                }
                if (std::memcmp(name, target.data(), bytes) == 0) {
                    selected = row.shaderNode;
                    matched = true;
                    break;
                }
            }
            if (matched) {
                break;
            }
        }
        staged.push_back(selected);
    }
    candidate.workItems += work.used(); // WorkBudget admits the complete sum.
    result.swap(staged);
    budget = candidate;
    return true;
}

} // namespace


AtNode* DPInstancerTranslatorAi::CreateArnoldNodes() try
{
	return AddArnoldNode("points");
}
catch (...) {
    ::dommy::arnold::reportTranslatorException(
        "DPInstancerTranslatorAi::CreateArnoldNodes", nullptr, std::current_exception());
    return nullptr;
}

void* DPInstancerTranslatorAi::creator()
{
    return ::dommy::arnold::createObject("DPInstancerTranslatorAi", [&]() {
        return new DPInstancerTranslatorAi();
    });
}


void DPInstancerTranslatorAi::Export(AtNode* node) try
{
	INST_DEBUG("Export");
    
    shouldRefreshAss = true;
    
	AiMsgDebug("[dommyTool] Exporting %s", GetMayaNodeName().asChar());
	Update(node);
}
DOMMY_ARNOLD_TRANSLATOR_CATCH("DPInstancerTranslatorAi::Export", node)

void DPInstancerTranslatorAi::NodeChanged(MObject& node, MPlug& plug) try
{
    INST_DEBUG("NodeChanged");
    MStatus status;
    const MFnDependencyNode nodeFn(node, &status);
    if (!status) {
        AiMsgError("[DPInstancerTranslatorAi] Unable to inspect the changed Maya node.");
        return;
    }
    const MString pName = plug.partialName(
        false, false, false, false, false, true, &status);
    if (!status) {
        AiMsgError("[DPInstancerTranslatorAi] Unable to read the changed Maya plug name.");
        return;
    }
    const bool hasAttribute = nodeFn.hasAttribute(pName, &status);
    if (!status) {
        AiMsgError("[DPInstancerTranslatorAi] Unable to inspect the changed Maya attribute.");
        return;
    }
    if (!hasAttribute) return;

    if (pName == "CopyObjectType" ||
        pName == "ReadAssFile" ||
        pName == "AssPrefix" ||
        pName == "IsOverrideShader" ||
        pName == "IsVerboseNodeList") {
        shouldRefreshAss = true;
    }
    SetUpdateMode(CNodeTranslator::AI_RECREATE_NODE);
    RequestUpdate();
}
DOMMY_ARNOLD_TRANSLATOR_CATCH("DPInstancerTranslatorAi::NodeChanged", nullptr)


bool DPInstancerTranslatorAi::refreshReferenceFile()
{
    static const AtString visParam("visibility");
    dommy::arnold::PlugReader plugs(
        [this](const char* name) -> MPlug { return FindMayaPlug(name); },
        "DPInstancerTranslatorAi::refreshReferenceFile");
    std::string filename;
    std::string prefixStr;
    short copyTypeValue = DP_INST_COPY_MESH;
    bool isVerboseNodeList = false;
    if (!plugs.readString("ReadAssFile", filename) ||
        !plugs.readString("AssPrefix", prefixStr) ||
        !plugs.read("CopyObjectType", copyTypeValue) ||
        !plugs.read("IsVerboseNodeList", isVerboseNodeList) ||
        copyTypeValue < DP_INST_COPY_MESH ||
        copyTypeValue >= DP_INST_COPY_TYPE_COUNT) {
        AiMsgError("[DPInstancerTranslatorAi] ASS parameters are invalid.");
        return false;
    }
    const DP_INST_COPY_TYPE copyType =
        static_cast<DP_INST_COPY_TYPE>(copyTypeValue);
    MFileObject fileObj;
    if (!fileObj.setRawFullName(filename.c_str())) {
        AiMsgError("[DPInstancerTranslatorAi] Unable to resolve the ASS file path.");
        return false;
    }
    filename = fileObj.resolvedFullName().asChar();
    if (!fileObj.exists() || prefixStr.empty()) {
        AiMsgError("[DPInstancerTranslatorAi] ASS file or prefix is invalid.");
        return false;
    }
    if (copyType != DP_INST_COPY_ASS) return true;
    if (!ownedReferenceNodes.empty() || !refNodeList.empty()) {
        AiMsgError("[DPInstancerTranslatorAi] Previous ASS references must be retired before loading.");
        return false;
    }

    // An imported reference is owned by this translator, not by its name.
    // Never adopt pre-existing prefix matches or overwrite a shared reference.
    std::unordered_set<AtNode*> previousNodes;
    {
        dommy::arnold::NodeIteratorPtr iterator(
            AiUniverseGetNodeIterator(AI_NODE_ALL));
        if (!iterator) {
            AiMsgError("[DPInstancerTranslatorAi] Unable to enumerate Arnold nodes.");
            return false;
        }
        while (!AiNodeIteratorFinished(iterator.get())) {
            AtNode* const candidate = AiNodeIteratorGetNext(iterator.get());
            if (!candidate) continue;
            const char* const name = AiNodeGetName(candidate);
            if (name && isMatchPrefixString(name, prefixStr)) {
                AiMsgError("[DPInstancerTranslatorAi] ASS prefix collides with an existing node: %s", name);
                return false;
            }
            previousNodes.insert(candidate);
        }
    }
    // Keep the retirement marker even after a failed/partial import. Successfully
    // enumerated new nodes remain tracked for the next cleanup attempt.
    referenceNodePrefix = prefixStr;
    const int success = AiASSLoad(
        filename.c_str(), (AI_NODE_SHAPE | AI_NODE_SHADER));
    {
        dommy::arnold::NodeIteratorPtr iterator(
            AiUniverseGetNodeIterator(AI_NODE_ALL));
        if (!iterator) {
            AiMsgError("[DPInstancerTranslatorAi] Unable to enumerate imported Arnold nodes.");
            return false;
        }
        while (!AiNodeIteratorFinished(iterator.get())) {
            AtNode* const candidate = AiNodeIteratorGetNext(iterator.get());
            if (!candidate || previousNodes.count(candidate)) continue;
            const AtNodeEntry* const entry = AiNodeGetNodeEntry(candidate);
            if (!entry) continue;
            const int type = AiNodeEntryGetType(entry);
            if (type != AI_NODE_SHAPE && type != AI_NODE_SHADER) continue;
            const char* const name = AiNodeGetName(candidate);
            if (!name || !isMatchPrefixString(name, prefixStr)) continue;
            // References are bases, not independently visible geometry. Hide
            // admitted new shapes before cache allocation or later generation
            // admission can fail. Never touch pre-existing/unrelated shapes.
            if (type == AI_NODE_SHAPE) {
                AiNodeSetByte(candidate, visParam, 0);
            }
            // The dedicated prefix limits the snapshot difference to this
            // asset. Unrelated concurrently added nodes and unprefixed shared
            // dependencies must never become this translator's deletion targets.
            ownedReferenceNodes.push_back(candidate);
        }
    }
    for (AtNode* const candidate : ownedReferenceNodes) {
        const AtNodeEntry* const entry = AiNodeGetNodeEntry(candidate);
        const char* const name = AiNodeGetName(candidate);
        if (entry && AiNodeEntryGetType(entry) == AI_NODE_SHAPE &&
            name && isMatchPrefixString(name, prefixStr)) {
            refNodeList.push_back(candidate);
        }
    }
    if (success == -1 || refNodeList.empty()) {
        AiMsgError("[DPInstancerTranslatorAi] ASS load failed or contains no matching shape nodes: %s",
                   filename.c_str());
        return false;
    }
    if (isVerboseNodeList) {
        std::ostringstream info;
        info << "ASS node info\nShape list:";
        for (AtNode* const shape : refNodeList) {
            const char* const name = AiNodeGetName(shape);
            if (name) info << " " << name;
        }
        info << "\nShader list:";
        for (AtNode* const candidate : ownedReferenceNodes) {
            const AtNodeEntry* const entry = AiNodeGetNodeEntry(candidate);
            if (entry && AiNodeEntryGetType(entry) == AI_NODE_SHADER) {
                const char* const name = AiNodeGetName(candidate);
                if (name) info << " " << name;
            }
        }
        AiMsgInfo("[DPInstancerTranslatorAi] %s", info.str().c_str());
    }
    return true;
}

bool DPInstancerTranslatorAi::deleteInstancedNodes(bool deleteReferenceNodes)
{
    static const AtString visParam("visibility");
    MStatus status;
    const MFnDependencyNode nodeFn(GetMayaObject(), &status);
    if (!status) {
        AiMsgError("[DPInstancerTranslatorAi] Unable to inspect the Maya owner.");
        return false;
    }
    const MUuid nodeId = nodeFn.uuid(&status);
    if (!status || !nodeId.valid()) {
        AiMsgError("[DPInstancerTranslatorAi] Invalid Maya owner identity.");
        return false;
    }
    const std::string ownerId = nodeId.asString().asChar();
    const std::unordered_set<AtNode*> referenceNodes(
        ownedReferenceNodes.begin(), ownedReferenceNodes.end());
    std::unordered_set<AtNode*> liveNodes;
    std::vector<std::pair<int, AtNode*>> candidates;
    std::vector<AtNode*> retainedReferenceShapes;
    {
        dommy::arnold::NodeIteratorPtr iterator(
            AiUniverseGetNodeIterator(AI_NODE_ALL));
        if (!iterator) {
            AiMsgError("[DPInstancerTranslatorAi] Unable to enumerate Arnold nodes.");
            return false;
        }
        // Finish all enumeration/allocation before changing caches or nodes.
        while (!AiNodeIteratorFinished(iterator.get())) {
            AtNode* const candidate = AiNodeIteratorGetNext(iterator.get());
            if (!candidate || !liveNodes.insert(candidate).second) continue;
            const bool managedReference = referenceNodes.count(candidate) != 0U;
            const AtNodeEntry* const entry = AiNodeGetNodeEntry(candidate);
            const int type = entry ? AiNodeEntryGetType(entry) : 0;
            if (managedReference) {
                const char* const name = AiNodeGetName(candidate);
                if ((type != AI_NODE_SHAPE && type != AI_NODE_SHADER) ||
                    !name || referenceNodePrefix.empty() ||
                    !isMatchPrefixString(name, referenceNodePrefix)) {
                    AiMsgError("[DPInstancerTranslatorAi] A cached ASS reference changed identity; refusing retirement.");
                    return false;
                }
                if (!deleteReferenceNodes && type == AI_NODE_SHAPE) {
                    retainedReferenceShapes.push_back(candidate);
                }
            }
            if (!entry) continue;
            int priority = -1;
            if (managedReference) {
                if (deleteReferenceNodes) priority = type == AI_NODE_SHADER ? 3 : 2;
            } else if (type == AI_NODE_SHAPE) {
                if (dommy::arnold::hasOwnershipTagSchema(candidate, "dInstObjTag")) {
                    const AtString objectTag = AiNodeGetStr(candidate, "dInstObjTag");
                    if (objectTag && ownerId == objectTag.c_str()) priority = 0;
                }
                if (priority < 0 &&
                    dommy::arnold::hasOwnershipTagSchema(candidate, "dInstMeshTag")) {
                    const AtString meshTag = AiNodeGetStr(candidate, "dInstMeshTag");
                    if (meshTag && ownerId == meshTag.c_str()) priority = 1;
                }
            }
            if (priority >= 0) candidates.emplace_back(priority, candidate);
        }
    }
    // Instances go first, then generated bases and imported shapes, then shaders.
    std::sort(candidates.begin(), candidates.end(),
        [](const std::pair<int, AtNode*>& a, const std::pair<int, AtNode*>& b) {
            return a.first < b.first;
        });
    const auto removeExpired = [&liveNodes](std::vector<AtNode*>& cache) {
        cache.erase(std::remove_if(cache.begin(), cache.end(),
            [&liveNodes](AtNode* value) { return !liveNodes.count(value); }),
            cache.end());
    };
    removeExpired(refNodeList);
    removeExpired(ownedReferenceNodes);
    // Use verified live ownership, not the raw cached addresses or a prefix.
    // This also repairs a retained reference whose visibility changed externally.
    for (AtNode* const reference : retainedReferenceShapes) {
        AiNodeSetByte(reference, visParam, 0);
    }
    for (const auto& candidate : candidates) {
        AtNode* const value = candidate.second;
        if (AiNodeDestroy(value)) {
            // Erasure cannot allocate. Never retain a destroyed pointer, even if
            // a later destroy or diagnostic call fails or throws.
            refNodeList.erase(std::remove(refNodeList.begin(), refNodeList.end(), value),
                              refNodeList.end());
            ownedReferenceNodes.erase(
                std::remove(ownedReferenceNodes.begin(), ownedReferenceNodes.end(), value),
                ownedReferenceNodes.end());
        } else {
            AiMsgWarning("[DPInstancerTranslatorAi] Failed to delete a managed Arnold node.");
            // Do not retire bases/references/shaders while a failed consumer
            // remains live. Successful deletions are already removed from the
            // caches; everything still live remains available for the next retry.
            return false;
        }
    }
    if (deleteReferenceNodes && ownedReferenceNodes.empty() && refNodeList.empty()) {
        referenceNodePrefix.clear();
    }
    return true;
}


bool DPInstancerTranslatorAi::retireGeneratedNodes()
{
    // Retry incomplete staging before enumerating tagged nodes, so collection
    // cannot retain pointers destroyed by the pending-batch retirement.
    if (!pendingNodes_.retire()) {
        AiMsgError("[DPInstancerTranslatorAi] Previous incomplete nodes could not be retired.");
        return false;
    }
    return deleteInstancedNodes(false);
}


void DPInstancerTranslatorAi::Update(AtNode* node) try
{
    
	INST_DEBUG("Update");
	
	
	
    MStatus ownerStatus;
    const MFnDependencyNode nodeFn(GetMayaObject(), &ownerStatus);
    if (!ownerStatus) {
        AiMsgError("[DPInstancerTranslatorAi] Unable to inspect the Maya owner.");
        return;
    }
    const MUuid nodeId = nodeFn.uuid(&ownerStatus);
    if (!ownerStatus || !nodeId.valid()) {
        AiMsgError("[DPInstancerTranslatorAi] Invalid Maya owner identity.");
        return;
    }
    dommy::arnold::PlugReader plugs(
        [this](const char* name) -> MPlug { return FindMayaPlug(name); },
        "DPInstancerTranslatorAi");
    const auto retireInactive = [this, node]() {
        // Cleanup must not depend on unused render controls, matrix export or
        // placeholder setters. Retained ASS references still use live ownership.
        const bool retired = retireGeneratedNodes();
        dommy::arnold::hideIncompleteUpdate(node);
        if (!retired) {
            AiMsgError("[DPInstancerTranslatorAi] Unable to retire inactive instances.");
        }
    };
    bool isRender = false;
    if (!plugs.read("IsRender", isRender)) {
        return; // Unknown state is not permission to retire the previous graph.
    }
    if (!isRender) {
        retireInactive();
        return;
    }

#pragma mark generate instance
    const MPlug dpPlug = FindMayaPlug("inDP");
    if (dpPlug.isNull()) {
        AiMsgError("[DPInstancerTranslatorAi] Unable to find the DPoint input.");
        return;
    }
    bool pointConnected = false;
    if (!plugs.connection(dpPlug, "inDP", pointConnected)) return;
    if (!pointConnected) {
        retireInactive();
        return;
    }
	{ // Borrowed point/mesh data stays alive through synchronous generation.
		
		//get dpdata
		dommy::maya::ScopedPlugDataHandle dPointHndOwner;
		MDataHandle& dPointHnd = dPointHndOwner.handle();
		if (dPointHndOwner.read(dpPlug) != MS::kSuccess) {
			AiMsgError("[dommyTool] Unable to read instancer DPoint input.");
			return;
		}
		const DPData* const dPoints(getInputDPfromHandle(dPointHnd));
        if (!dPoints) {
            AiMsgError("[DPInstancerTranslatorAi] Input does not contain valid DPoint data.");
            return;
        }
        const std::size_t instCount = dPoints->numberOfPoints();
        if (instCount == 0U) {
            retireInactive();
            return;
        }

        short copyTypeValue = DP_INST_COPY_MESH;
        if (!plugs.read("CopyObjectType", copyTypeValue) ||
            copyTypeValue < DP_INST_COPY_MESH || copyTypeValue >= DP_INST_COPY_TYPE_COUNT) {
            AiMsgError("[DPInstancerTranslatorAi] Copy mode is invalid.");
            return;
        }
        const DP_INST_COPY_TYPE copyType = static_cast<DP_INST_COPY_TYPE>(copyTypeValue);
        MPlug inmeshPlug;
        if (copyType == DP_INST_COPY_MESH) {
            inmeshPlug = FindMayaPlug("inMesh");
            if (inmeshPlug.isNull()) {
                AiMsgError("[DPInstancerTranslatorAi] Unable to find the mesh input.");
                return;
            }
            bool meshConnected = false;
            if (!plugs.connection(inmeshPlug, "inMesh", meshConnected)) return;
            if (!meshConnected) {
                retireInactive();
                return;
            }
        }
        
		// ASS mode does not acquire, inspect or require the optional mesh input.
		dommy::maya::ScopedPlugDataHandle inMeshHndOwner;
		MDataHandle& inMeshHnd = inMeshHndOwner.handle();
        if (copyType == DP_INST_COPY_MESH) {
            if (inMeshHndOwner.read(inmeshPlug) != MS::kSuccess) {
                AiMsgError("[dommyTool] Unable to read instancer mesh input.");
                return;
            }
            if (inMeshHnd.type() != MFnData::kMesh) {
                AiMsgError("[DPInstancerTranslatorAi] Input does not contain mesh data.");
                return;
            }
        }
		
		
		{ // All required inputs for the selected copy mode are valid and nonempty.
			
            bool isOverrideShader = false;
            bool sidedness = false;
            bool invertNormals = false;
            bool opaque = false;
            bool matte = false;
            bool selfShadows = false;
            bool receiveShadows = false;
            MString exportAttributeNames;
            if (!plugs.read("IsOverrideShader", isOverrideShader) ||
                !plugs.read("Sidedness", sidedness) ||
                !plugs.read("InvertNormals", invertNormals) ||
                !plugs.read("aiOpaque", opaque) ||
                !plugs.read("aiMatte", matte) ||
                !plugs.read("aiSelfShadows", selfShadows) ||
                !plugs.read("aiReceiveShadows", receiveShadows) ||
                !plugs.read("ExportAttribute", exportAttributeNames)) {
                AiMsgError("[DPInstancerTranslatorAi] Render parameters are invalid.");
                return;
            }
            std::string nodeNamePrefix("dommy_instancer_");
            nodeNamePrefix += nodeId.asString().asChar();
            std::replace(nodeNamePrefix.begin(), nodeNamePrefix.end(), '-', '_');
            const unsigned int numMotionStep = GetNumMotionSteps();
            static const AtString visParam("visibility");
            ExportMatrix(node);
            AiNodeSetByte(node, visParam, false);
            AiNodeSetFlt(node, "radius", 0.001);

#pragma mark Export arnold
			
			INST_DEBUG("Start Export Arnold.");
			
			
			
			
#pragma mark generate matrix data
			INST_DEBUG("Generate animation Matrix.");
            
			DTransformParams params;
			if (evalAnimMatrixParms(params) != MS::kSuccess) {
				AiMsgError("[dommyTool] Unable to read instancer transform parameters.");
				return;
			}
			
			// Shared generation admits the complete payload before allocating it.
			DMatrixArrayPtr matrixArray(new std::vector<MMatrix>());
			
			if (DGenerateTransformMatrixFromDP(
			        &*dPoints, params, *matrixArray) != MS::kSuccess) {
				AiMsgError("[dommyTool] Failed to generate valid instance transforms.");
				return;
			}
            // MtoA stores matrices as Float. Admit every generated frame before
            // deleting old instances; the framework conversion then stays safe.
            for (const MMatrix& matrix : *matrixArray) {
                float validatedMatrix[4][4];
                dommy::arnold::checkedFloatMatrix(matrix, validatedMatrix);
            }
            
            
            
            
#pragma mark common attribute
	            INST_DEBUG("Get Common Attribute.");
	            
	            static const AtString sidednessParam("sidedness");
	            
	            static const AtString invNormalParam("invert_normals");
	            
	            static const AtString aiOpaqueParam("opaque");
	            
	            static const AtString aiMatteParam("matte");
	            
	            static const AtString aiSelfShadowsParam("self_shadows");
	            
	            static const AtString aiReceiveShadowsParam("receive_shadows");
	            
	            
	            MStringArray exAttrNameArray;
				dommy::maya::NameListBudget nameListBudget;
				if (dommy::maya::splitCheckedNameList(
				        exportAttributeNames, instCount, nameListBudget, exAttrNameArray) != MS::kSuccess) {
					AiMsgError("[DPInstancerTranslatorAi] Unable to parse the export attribute list.");
					return;
				}
				for (unsigned int attributeIndex = 0;
				     attributeIndex < exAttrNameArray.length();
				     ++attributeIndex) {
					const std::string attributeName =
						exAttrNameArray[attributeIndex].asChar();
					if (!dPoints->isExistAttr(attributeName)) {
						continue;
					}
					const DPAttributeData::ConstPtr attribute =
						dPoints->getAttr(attributeName);
					if (!attribute ||
					    attribute->attrSize() < instCount) {
						AiMsgError(
							"[DPInstancerTranslatorAi] Instance attribute '%s' "
							"does not contain one value per point.",
							attributeName.c_str());
						return;
					}
				}
	            
				std::size_t attributeWorkItems = 0U;
				if (!dommy::generation::checkedProduct(
						instCount, exAttrNameArray.length(),
						dommy::generation::kMaxWorkItemCount, attributeWorkItems)) {
					AiMsgError("Arnold instance attribute validation exceeds the work budget.");
					return;
				}
				for (std::size_t index = 0U; index < instCount; ++index) {
					instanceAssignDPAttribute(dPoints, exAttrNameArray, nullptr, index);
				}
	            INST_DEBUG("exAttrNameArray:" << exAttrNameArray.length());
            
            
#pragma mark Get Shader data
            INST_DEBUG("Get Shader data.");
            
            MPlug shaderPlug = FindMayaPlug("Shader");
			MPlug shPlug;
			if (!plugs.source(shaderPlug, "Shader", shPlug)) return;
            INST_DEBUG("ShaderPlug:" << !shPlug.isNull());
	            AtNode* aiSgNode = nullptr;
            static const AtString shaderParam("shader");
            
				if (!shPlug.isNull()) {
					aiSgNode = ExportConnectedNode(shPlug);
					if (!aiSgNode) {
						AiMsgError("[DPInstancerTranslatorAi] Unable to export the connected shader.");
						return;
					}
				}

				// Resolve the entire replacement table before deleting the old
				// instance graph. Empty targets/unconnected rows remain optional.
				std::vector<ReplaceShaderData> repShaderDataList;
				if (copyType == DP_INST_COPY_ASS) {
					MStatus stat;
					const MPlug listPlug = FindMayaPlug("ReplaceShaderList");
					const bool isArray = listPlug.isArray(&stat);
					if (!stat || !isArray) {
						AiMsgError("[DPInstancerTranslatorAi] Invalid replacement-shader table.");
						return;
					}
					const unsigned int rowCount = listPlug.numElements(&stat);
					if (!stat) {
						AiMsgError("[DPInstancerTranslatorAi] Unable to count replacement-shader rows.");
						return;
					}
                    if (rowCount > dommy::generation::kMaxGeneratedStringCount ||
                        !dommy::generation::checkedSum(
                            nameListBudget.workItems, rowCount,
                            dommy::generation::kMaxWorkItemCount, nameListBudget.workItems)) {
                        AiMsgError("[DPInstancerTranslatorAi] Replacement-shader table exceeds its row/work budget.");
                        return;
                    }
					for (unsigned int i = 0; i < rowCount; ++i) {
						const MPlug row = listPlug.elementByPhysicalIndex(i, &stat);
						if (!stat) {
							AiMsgError("[DPInstancerTranslatorAi] Unable to read a replacement-shader row.");
							return;
						}
						const MPlug targets = row.child(DPInstancer::aTargetNodeNames, &stat);
						if (!stat) {
							AiMsgError("[DPInstancerTranslatorAi] Unable to resolve replacement targets.");
							return;
						}
						MString names;
						if (readPlugValue(targets, names) != MS::kSuccess) {
							AiMsgError("[DPInstancerTranslatorAi] Unable to read replacement targets.");
							return;
						}
                        std::vector<std::string> selections;
                        if (dommy::maya::splitCheckedNonemptyNameList(
                                names, 1U, nameListBudget, selections) != MS::kSuccess) {
                            AiMsgError("[DPInstancerTranslatorAi] Replacement target names exceed their string/work budget.");
                            return;
                        }
						if (selections.empty()) continue;
						const MPlug replacement = row.child(DPInstancer::aReplaceShader, &stat);
						if (!stat) {
							AiMsgError("[DPInstancerTranslatorAi] Unable to resolve a replacement shader.");
							return;
						}
						MPlug replacementSource;
						if (!plugs.source(replacement, "ReplaceShader", replacementSource)) return;
						if (replacementSource.isNull()) continue;
						AtNode* const replacementNode = ExportConnectedNode(replacementSource);
						if (!replacementNode) {
							AiMsgError("[DPInstancerTranslatorAi] Unable to export a replacement shader.");
							return;
						}
						ReplaceShaderData data;
						data.shapeNameList = std::move(selections);
						data.shaderNode = replacementNode;
						repShaderDataList.push_back(std::move(data));
					}
				}
				INST_DEBUG("repShaderDataList count: " << repShaderDataList.size());

                dommy::arnold::NodeGenerationLayout generationLayout;
                const auto preflightInstanceGeneration = [&]() -> bool {
                    std::size_t replacementTargets = 0U;
                    for (const ReplaceShaderData& row : repShaderDataList) {
                        if (!dommy::generation::checkedSum(
                                replacementTargets, row.shapeNameList.size(),
                                dommy::generation::kMaxWorkItemCount,
                                replacementTargets)) return false;
                    }
                    return dommy::arnold::checkedNodeGenerationLayout(
                        instCount, copyType == DP_INST_COPY_MESH ? 1U : refNodeList.size(),
                        exAttrNameArray.length(), replacementTargets,
                        copyType == DP_INST_COPY_MESH ? 1U : 0U, generationLayout);
                };
                // Reused references and mesh mode can be admitted before old
                // graph retirement. Fresh ASS counts are checked after loading.
                if ((copyType == DP_INST_COPY_MESH || !shouldRefreshAss) &&
                    !preflightInstanceGeneration()) {
                    AiMsgError("[DPInstancerTranslatorAi] Instance generation exceeds its node/work budget.");
                    return;
                }

				// Keep the previous Arnold graph alive until all Maya inputs and
				// instance transforms have been read successfully.
					if (copyType == DP_INST_COPY_ASS && shouldRefreshAss) {
						std::string assFilename;
						std::string assPrefix;
						if (!plugs.readString("ReadAssFile", assFilename) ||
						    !plugs.readString("AssPrefix", assPrefix) ||
						    assPrefix.empty()) {
							AiMsgError(
								"[DPInstancerTranslatorAi] ASS refresh "
								"parameters are invalid.");
							return;
						}
						MFileObject assFile;
						if (!assFile.setRawFullName(assFilename.c_str())) {
                            AiMsgError("[DPInstancerTranslatorAi] Unable to resolve the ASS refresh path.");
                            return;
                        }
						if (!assFile.exists()) {
							AiMsgError(
								"[DPInstancerTranslatorAi] ASS file does not "
								"exist: %s",
								assFilename.c_str());
							return;
						}
					}
                    if (!pendingNodes_.retire()) {
                        AiMsgError("[DPInstancerTranslatorAi] Previous incomplete nodes could not be retired.");
                        return;
                    }
                    dommy::arnold::NodeBatchScope generationScope(pendingNodes_);
                    if (copyType == DP_INST_COPY_MESH)
                        pendingNodes_.reserve(generationLayout.nodeCount);
                    AtNode* preparedMesh = nullptr;
                    dommy::arnold::NodeOwner preparedMeshOwner(
                        copyType == DP_INST_COPY_MESH ?
                            generateAiMeshFromMPlug(inMeshHnd, numMotionStep) : nullptr);
					if (copyType == DP_INST_COPY_MESH) {
						if (!preparedMeshOwner) {
							AiMsgError("Unable to prepare the Arnold instancer base mesh.");
							return;
						}
						// Declare the tag before retirement, but leave its owner value
						// unset so old-node enumeration cannot collect this candidate.
                        preparedMesh = preparedMeshOwner.get();
                        pendingNodes_.adopt(std::move(preparedMeshOwner));
                        dommy::arnold::requireOwnershipTag(preparedMesh, "dInstMeshTag");
					}
					if (!deleteInstancedNodes(
						    shouldRefreshAss &&
						    !referenceNodePrefix.empty())) {
						return;
					}
					if (copyType == DP_INST_COPY_ASS && shouldRefreshAss) {
						INST_DEBUG("PRE refNodeList:" << refNodeList.size());
						if (!refreshReferenceFile()) {
							return;
						}
						shouldRefreshAss = false;
					}
                if (!preflightInstanceGeneration()) {
                    AiMsgError("[DPInstancerTranslatorAi] Instance generation exceeds its node/work budget.");
                    return;
                }
                std::vector<AtNode*> replacementShaders;
                if (copyType == DP_INST_COPY_ASS &&
                    !buildReplacementShaderPlan(
                        refNodeList, repShaderDataList, nameListBudget, replacementShaders)) {
                    AiMsgError("[DPInstancerTranslatorAi] Replacement-shader plan exceeds its name/work budget or has an invalid reference.");
                    return;
                }
                if (copyType == DP_INST_COPY_ASS)
                    pendingNodes_.reserve(generationLayout.nodeCount);
				for (std::vector<AtNode*>::const_iterator it =
						 refNodeList.begin(); it != refNodeList.end(); ++it) {
					AiNodeSetByte(*it, visParam, false);
				}
				INST_DEBUG("refNodeList:" << refNodeList.size());
	            
	            
#pragma mark create inst data
			
			if (copyType == DP_INST_COPY_MESH) {
#pragma mark /////////////////////// Mesh instance ///////////////////////
				INST_DEBUG("/////////////////////// Mesh instance ///////////////////////:");
                
#pragma mark create mesh data
                INST_DEBUG("Create mesh data.");
                
                //create arnold node
					if (!preparedMesh) {
						AiMsgError(
							"[DPInstancerTranslatorAi] Unable to create the base mesh.");
						return;
					}
					AtNode* const baseMesh = preparedMesh;
                
                
                
#pragma mark set name
	                const std::string meshName =
						nodeNamePrefix + "_base_mesh";
	                AiNodeSetStr(baseMesh, "name", meshName.c_str());
                
#pragma mark set base mesh tag
                dommy::arnold::requireOwnershipTag(baseMesh, "dInstMeshTag");
                AiNodeSetStr(baseMesh, "dInstMeshTag", nodeId.asString().asChar());
                //visiblity
                AiNodeSetByte( baseMesh, visParam, false );
                
                
                
                size_t instIndex =0;
					for (auto pIt=dPoints->beginPt();pIt!=dPoints->endPt();++pIt) {
						// create inst node
						AtNode* const inst = pendingNodes_.create("ginstance");
						if (!inst) {
							AiMsgError(
								"[DPInstancerTranslatorAi] Unable to create an instance.");
							return;
						}
					
                    
					AiNodeSetByte( inst, visParam, ComputeVisibility());
					
					//set base mesh
					AiNodeSetPtr(inst, "node", baseMesh);
					
#pragma mark set name
					const std::string instanceName =
						nodeNamePrefix + "_mesh_instance_" +
						std::to_string(instIndex);
					AiNodeSetStr(inst, "name", instanceName.c_str());
#pragma mark set base mesh tag
					dommy::arnold::requireOwnershipTag(inst, "dInstObjTag");
					AiNodeSetStr(inst, "dInstObjTag", nodeId.asString().asChar());
					
					
#pragma mark set common attribute
					AiNodeSetByte(inst, sidednessParam, dommy::arnold::sidednessMask(sidedness));
					
					AiNodeSetBool(inst, invNormalParam, invertNormals);
					
					AiNodeSetBool(inst, aiOpaqueParam, opaque);
					AiNodeSetBool(inst, aiMatteParam, matte);
					AiNodeSetBool(inst, aiSelfShadowsParam, selfShadows);
					AiNodeSetBool(inst, aiReceiveShadowsParam, receiveShadows);
					
					
					//Set Matrix
					AtMatrix finMat;
					ConvertMatrix(finMat, (*matrixArray)[instIndex]);
					
					AiNodeSetMatrix(inst, "matrix", finMat);
					
					//Assign shader
					if(aiSgNode){
						AiNodeSetPtr( inst, shaderParam, aiSgNode);
					}
					
					
					//add user attribute
						if (exAttrNameArray.length()) {
							instanceAssignDPAttribute(&*dPoints, exAttrNameArray, inst, instIndex);
						}
	                    
						instIndex++;
					}//loop point
                
			} else if (copyType == DP_INST_COPY_ASS) {
                
#pragma mark ///////////////////////ASS instance///////////////////////
				
				
#pragma mark Start Instance
					size_t instIndex =0;
					size_t generatedInstanceIndex = 0;
					for (auto pIt=dPoints->beginPt();pIt!=dPoints->endPt();++pIt) {
					
					
					
					
						for (auto nIt=refNodeList.begin(); nIt!=refNodeList.end(); ++nIt) {
							
							// create inst node
							AtNode* const inst = pendingNodes_.create("ginstance");
							if (!inst) {
								AiMsgError(
									"[DPInstancerTranslatorAi] Unable to create an instance.");
								return;
							}
						
						AiNodeSetByte( inst, visParam, ComputeVisibility());
						
						
						
						//set base mesh
						AiNodeSetPtr(inst, "node", *nIt);
						
#pragma mark set name
							const std::string instanceName =
								nodeNamePrefix + "_ass_instance_" +
								std::to_string(generatedInstanceIndex++);
						AiNodeSetStr(inst, "name", instanceName.c_str());
#pragma mark set base mesh tag
						dommy::arnold::requireOwnershipTag(inst, "dInstObjTag");
						AiNodeSetStr(inst, "dInstObjTag", nodeId.asString().asChar());
						
						
#pragma mark set common attribute
						AiNodeSetByte(inst, sidednessParam, dommy::arnold::sidednessMask(sidedness));
						
						AiNodeSetBool(inst, invNormalParam, invertNormals);
						
						AiNodeSetBool(inst, aiOpaqueParam, opaque);
						AiNodeSetBool(inst, aiMatteParam, matte);
						AiNodeSetBool(inst, aiSelfShadowsParam, selfShadows);
						AiNodeSetBool(inst, aiReceiveShadowsParam, receiveShadows);
						
						
						
						//Set Matrix
						AtMatrix finMat;
						ConvertMatrix(finMat, (*matrixArray)[instIndex]);
						
						
						AiNodeSetMatrix(inst, "matrix", finMat);
						
						
						//Assign shader
						if(isOverrideShader && aiSgNode){
							AiNodeSetPtr( inst, "shader", aiSgNode);
						}
						
                        // The aligned plan is resolved once per reference.
                        if (!replacementShaders.empty()) {
                            AtNode* const replacement = replacementShaders[
                                static_cast<std::size_t>(nIt - refNodeList.begin())];
                            if (replacement) {
                                AiNodeSetPtr(inst, "shader", replacement);
                            }
                        }
						
						
						
						
						//add user attribute
						
							if (exAttrNameArray.length()) {
								
								instanceAssignDPAttribute(&*dPoints, exAttrNameArray, inst, instIndex);
							}
						}
					
					
					
					
					instIndex++;
				}//loop point
			}//copy type
			
			
			
			
			
                generationScope.commit();
			}//generation scope
	}//borrowed input scope
}
DOMMY_ARNOLD_TRANSLATOR_CATCH("DPInstancerTranslatorAi::Update", node)



bool DPInstancerTranslatorAi::RequiresMotionData()
{
    // Generated instances have no deformation-sample exporter. Keep their
    // historical static result without requesting unimplemented motion data.
    return false;
}

void DPInstancerTranslatorAi::ExportMotion(AtNode* /*node*/) {
    // Static instancing only; native motion support is a separate feature.
}
	
	

MStatus DPInstancerTranslatorAi::evalAnimMatrixParms(DTransformParams& output) {
	DTransformParams parms = output;
	auto readHandle = [this](const char* name, dommy::maya::ScopedPlugDataHandle& value) {
		return value.read(FindMayaPlug(name));
	};
	auto readString = [this](const char* name, std::string& value) {
		MString candidate;
		const MStatus status = readPlugValue(FindMayaPlug(name), candidate);
		if (status == MS::kSuccess) {
			value = candidate.asChar();
		}
		return status;
	};

	short animationType = 0;
	if (readPlugValue(FindMayaPlug("AnimationType"), animationType) !=
	        MS::kSuccess ||
	    readString("AnimMatrixAttribute", parms.mAnimationAttribute) !=
	        MS::kSuccess) {
		return MS::kFailure;
	}
	parms.mAnimMatrixType = animationType;
	parms.mNodeObj = GetMayaObject();
	parms.mAnimationInputIsData = false;
	parms.mAnim_Mat_InputData_Object = MObject::kNullObj;
	if (parms.mNodeObj.isNull()) {
		return MS::kFailure;
	}
	if (animationType < MAT_ANIM_NONE || animationType >= MAT_ANIM_TYPE_COUNT) {
		return MS::kInvalidParameter;
	}
	if (animationType == MAT_ANIM_INPUT_DATA) {
		const MPlug matrixPlug = FindMayaPlug("InAnimMatrix");
		if (matrixPlug.isNull()) return MS::kFailure;
		MStatus status;
		parms.mAnim_Mat_InputData_Object = matrixPlug.attribute(&status);
		DOMMY_RETURN_IF_FAILURE(status);
		if (parms.mAnim_Mat_InputData_Object.isNull()) return MS::kFailure;
	}

	dommy::maya::ScopedPlugDataHandle aimVecHndOwner;
	MDataHandle& aimVecHnd = aimVecHndOwner.handle();
	dommy::maya::ScopedPlugDataHandle axisVecHndOwner;
	MDataHandle& axisVecHnd = axisVecHndOwner.handle();
	dommy::maya::ScopedPlugDataHandle normalVecHndOwner;
	MDataHandle& normalVecHnd = normalVecHndOwner.handle();
	dommy::maya::ScopedPlugDataHandle tangentHndOwner;
	MDataHandle& tangentHnd = tangentHndOwner.handle();
	if (readHandle("AimVector", aimVecHndOwner) != MS::kSuccess ||
	    readHandle("AxisVector", axisVecHndOwner) != MS::kSuccess ||
	    readHandle("NormalVector", normalVecHndOwner) != MS::kSuccess ||
	    readHandle("TangentVector", tangentHndOwner) != MS::kSuccess) {
		return MS::kFailure;
	}
	parms.mNormalBaseAxis = aimVecHnd.asVector();
	parms.mTangentBaseAxis = axisVecHnd.asVector();
	parms.mNormalVec = normalVecHnd.asVector();
	parms.mTangentVec = tangentHnd.asVector();
	if (readString("NormalAttribute", parms.mNormalVecAttribute) !=
	        MS::kSuccess ||
	    readString("TangentAttribute", parms.mTangentVecAttribute) !=
	        MS::kSuccess) {
		return MS::kFailure;
	}

	dommy::maya::ScopedPlugDataHandle localTranslateHandleOwner;
	MDataHandle& localTranslateHandle = localTranslateHandleOwner.handle();
	dommy::maya::ScopedPlugDataHandle globalTranslateHandleOwner;
	MDataHandle& globalTranslateHandle = globalTranslateHandleOwner.handle();
	dommy::maya::ScopedPlugDataHandle localRotateHandleOwner;
	MDataHandle& localRotateHandle = localRotateHandleOwner.handle();
	dommy::maya::ScopedPlugDataHandle globalRotateHandleOwner;
	MDataHandle& globalRotateHandle = globalRotateHandleOwner.handle();
	dommy::maya::ScopedPlugDataHandle globalScaleHandleOwner;
	MDataHandle& globalScaleHandle = globalScaleHandleOwner.handle();
	int seed = 0;
	if (readHandle("LocalTranslate", localTranslateHandleOwner) != MS::kSuccess ||
	    readHandle("GlobalTranslate", globalTranslateHandleOwner) != MS::kSuccess ||
	    readHandle("LocalRotate", localRotateHandleOwner) != MS::kSuccess ||
	    readHandle("GlobalRotate", globalRotateHandleOwner) != MS::kSuccess ||
	    readHandle("GlobalScale", globalScaleHandleOwner) != MS::kSuccess ||
	    readPlugValue(FindMayaPlug("Seed"), seed) != MS::kSuccess ||
	    seed < 0 ||
	    readPlugValue(FindMayaPlug("UniformScale"), parms.mUniformScale) !=
	        MS::kSuccess) {
		return MS::kFailure;
	}
	parms.mSeed = static_cast<std::size_t>(seed);
	parms.mLocalTranslate = MVector(localTranslateHandle.asDouble3());
	parms.mGlobalTranslate = MVector(globalTranslateHandle.asDouble3());
	parms.mLocalRot = MVector(localRotateHandle.asDouble3());
	parms.mGlobalRot = MVector(globalRotateHandle.asDouble3());
	parms.mGlobalScale = MVector(globalScaleHandle.asDouble3());

	if (readString("LocalTranslateAttribute", parms.mLocalTransAttribute) !=
	        MS::kSuccess ||
	    readString("GlobalTranslateAttribute", parms.mGlobalTransAttribute) !=
	        MS::kSuccess ||
	    readString("LocalRotateAttribute", parms.mLocalRotAttribute) !=
	        MS::kSuccess ||
	    readString("GlobalRotateAttribute", parms.mGlobalRotAttribute) !=
	        MS::kSuccess ||
	    readString("GlobalScaleAttribute", parms.mGlobalScaleAttribute) !=
	        MS::kSuccess ||
	    readString("UniformScaleAttribute", parms.mUniformScaleAttribute) !=
	        MS::kSuccess) {
		return MS::kFailure;
	}

	dommy::maya::ScopedPlugDataHandle localTranslateRandomMinOwner;
	MDataHandle& localTranslateRandomMin = localTranslateRandomMinOwner.handle();
	dommy::maya::ScopedPlugDataHandle localTranslateRandomMaxOwner;
	MDataHandle& localTranslateRandomMax = localTranslateRandomMaxOwner.handle();
	dommy::maya::ScopedPlugDataHandle globalTranslateRandomMinOwner;
	MDataHandle& globalTranslateRandomMin = globalTranslateRandomMinOwner.handle();
	dommy::maya::ScopedPlugDataHandle globalTranslateRandomMaxOwner;
	MDataHandle& globalTranslateRandomMax = globalTranslateRandomMaxOwner.handle();
	if (readHandle("LocalTranslateRandomMin", localTranslateRandomMinOwner) !=
	        MS::kSuccess ||
	    readHandle("LocalTranslateRandomMax", localTranslateRandomMaxOwner) !=
	        MS::kSuccess ||
	    readHandle("GlobalTranslateRandomMin", globalTranslateRandomMinOwner) !=
	        MS::kSuccess ||
	    readHandle("GlobalTranslateRandomMax", globalTranslateRandomMaxOwner) !=
	        MS::kSuccess) {
		return MS::kFailure;
	}
	parms.mLocalTransRndMin =
	    MVector(localTranslateRandomMin.asDouble3());
	parms.mLocalTransRndMax =
	    MVector(localTranslateRandomMax.asDouble3());
	parms.mGlobalTransRndMin =
	    MVector(globalTranslateRandomMin.asDouble3());
	parms.mGlobalTransRndMax =
	    MVector(globalTranslateRandomMax.asDouble3());

	dommy::maya::ScopedPlugDataHandle localRotateRandomMinOwner;
	MDataHandle& localRotateRandomMin = localRotateRandomMinOwner.handle();
	dommy::maya::ScopedPlugDataHandle localRotateRandomMaxOwner;
	MDataHandle& localRotateRandomMax = localRotateRandomMaxOwner.handle();
	dommy::maya::ScopedPlugDataHandle globalRotateRandomMinOwner;
	MDataHandle& globalRotateRandomMin = globalRotateRandomMinOwner.handle();
	dommy::maya::ScopedPlugDataHandle globalRotateRandomMaxOwner;
	MDataHandle& globalRotateRandomMax = globalRotateRandomMaxOwner.handle();
	if (readHandle("LocalRotateRandomMin", localRotateRandomMinOwner) !=
	        MS::kSuccess ||
	    readHandle("LocalRotateRandomMax", localRotateRandomMaxOwner) !=
	        MS::kSuccess ||
	    readHandle("GlobalRotateRandomMin", globalRotateRandomMinOwner) !=
	        MS::kSuccess ||
	    readHandle("GlobalRotateRandomMax", globalRotateRandomMaxOwner) !=
	        MS::kSuccess) {
		return MS::kFailure;
	}
	parms.mLocalRotRndMin = MVector(localRotateRandomMin.asDouble3());
	parms.mLocalRotRndMax = MVector(localRotateRandomMax.asDouble3());
	parms.mGlobalRotRndMin = MVector(globalRotateRandomMin.asDouble3());
	parms.mGlobalRotRndMax = MVector(globalRotateRandomMax.asDouble3());

	dommy::maya::ScopedPlugDataHandle globalScaleRandomMinOwner;
	MDataHandle& globalScaleRandomMin = globalScaleRandomMinOwner.handle();
	dommy::maya::ScopedPlugDataHandle globalScaleRandomMaxOwner;
	MDataHandle& globalScaleRandomMax = globalScaleRandomMaxOwner.handle();
	dommy::maya::ScopedPlugDataHandle uniformScaleRandomMinMaxOwner;
	MDataHandle& uniformScaleRandomMinMax = uniformScaleRandomMinMaxOwner.handle();
	if (readHandle("GlobalScaleRandomMin", globalScaleRandomMinOwner) !=
	        MS::kSuccess ||
	    readHandle("GlobalScaleRandomMax", globalScaleRandomMaxOwner) !=
	        MS::kSuccess ||
	    readHandle("UniformScaleRandomMinMax", uniformScaleRandomMinMaxOwner) !=
	        MS::kSuccess) {
		return MS::kFailure;
	}
	parms.mGlobalScaleRndMin = MVector(globalScaleRandomMin.asDouble3());
	parms.mGlobalScaleRndMax = MVector(globalScaleRandomMax.asDouble3());
	const double2& uniformScaleValues =
	    uniformScaleRandomMinMax.asDouble2();
	parms.mUniformScaleMinMax =
	    MVector(uniformScaleValues[0], uniformScaleValues[1], 0.0);
	output = std::move(parms);
	return MS::kSuccess;
}



void DPInstancerTranslatorAi::instanceAssignDPAttribute(
    const DPData* dPoints, const MStringArray& attributeNameArray,
    AtNode* inst, size_t instIndex)
{
    // A null destination performs the same conversions without renderer writes.
    // Update uses this validation pass before retiring the previous instances.
    for (unsigned int index = 0U; index < attributeNameArray.length(); ++index) {
        const std::string attrName = attributeNameArray[index].asChar();
        if (!dPoints->isExistAttr(attrName)) continue;
        const DP_ATTR_DATA_TYPE type = dPoints->getAttr(attrName)->getAttrType();
        switch (type) {
        case DP_ATTR_INT: {
            const int32_t value = dPoints->attrListValueWithIndex<int32_t>(attrName, instIndex);
            dommy::arnold::requireInstanceUserParameter(inst, attrName.c_str(), "constant INT");
            if (inst != nullptr) {
                AiNodeSetInt(inst, attrName.c_str(), value);
            }
            break;
        }
        case DP_ATTR_UINT32: {
            const uint32_t value = dPoints->attrListValueWithIndex<uint32_t>(attrName, instIndex);
            dommy::arnold::requireInstanceUserParameter(inst, attrName.c_str(), "constant UINT");
            if (inst != nullptr) {
                AiNodeSetUInt(inst, attrName.c_str(), value);
            }
            break;
        }
        case DP_ATTR_INT64: {
            const int64_t value = dPoints->attrListValueWithIndex<int64_t>(attrName, instIndex);
            int narrowed = 0;
            if (!dommy::numeric::checkedScalarCast(value, narrowed))
                throw std::invalid_argument("Arnold instance Int64 attribute exceeds the INT range.");
            dommy::arnold::requireInstanceUserParameter(inst, attrName.c_str(), "constant INT");
            if (inst != nullptr) {
                AiNodeSetInt(inst, attrName.c_str(), narrowed);
            }
            break;
        }
        case DP_ATTR_FLOAT: {
            const float value = dPoints->attrListValueWithIndex<float>(attrName, instIndex);
            const float narrowed = dommy::arnold::checkedFloat(value);
            dommy::arnold::requireInstanceUserParameter(inst, attrName.c_str(), "constant FLOAT");
            if (inst != nullptr) {
                AiNodeSetFlt(inst, attrName.c_str(), narrowed);
            }
            break;
        }
        case DP_ATTR_DOUBLE: {
            const double value = dPoints->attrListValueWithIndex<double>(attrName, instIndex);
            const float narrowed = dommy::arnold::checkedFloat(value);
            dommy::arnold::requireInstanceUserParameter(inst, attrName.c_str(), "constant FLOAT");
            if (inst != nullptr) {
                AiNodeSetFlt(inst, attrName.c_str(), narrowed);
            }
            break;
        }
        case DP_ATTR_VEC2I: {
            const Imath::V2i value = dPoints->attrListValueWithIndex<Imath::V2i>(attrName, instIndex);
            Imath::V2f narrowed;
            if (!dommy::numeric::checkedValueCast(value, narrowed))
                throw std::invalid_argument("Arnold instance vector attribute exceeds the finite Float range.");
            dommy::arnold::requireInstanceUserParameter(inst, attrName.c_str(), "constant POINT2");
            if (inst != nullptr) {
                AiNodeSetVec2(inst, attrName.c_str(), narrowed[0], narrowed[1]);
            }
            break;
        }
        case DP_ATTR_VEC2F: {
            const Imath::V2f value = dPoints->attrListValueWithIndex<Imath::V2f>(attrName, instIndex);
            Imath::V2f narrowed;
            if (!dommy::numeric::checkedValueCast(value, narrowed))
                throw std::invalid_argument("Arnold instance vector attribute exceeds the finite Float range.");
            dommy::arnold::requireInstanceUserParameter(inst, attrName.c_str(), "constant POINT2");
            if (inst != nullptr) {
                AiNodeSetVec2(inst, attrName.c_str(), narrowed[0], narrowed[1]);
            }
            break;
        }
        case DP_ATTR_VEC2D: {
            const Imath::V2d value = dPoints->attrListValueWithIndex<Imath::V2d>(attrName, instIndex);
            Imath::V2f narrowed;
            if (!dommy::numeric::checkedValueCast(value, narrowed))
                throw std::invalid_argument("Arnold instance vector attribute exceeds the finite Float range.");
            dommy::arnold::requireInstanceUserParameter(inst, attrName.c_str(), "constant POINT2");
            if (inst != nullptr) {
                AiNodeSetVec2(inst, attrName.c_str(), narrowed[0], narrowed[1]);
            }
            break;
        }
        case DP_ATTR_VEC3I: {
            const Imath::V3i value = dPoints->attrListValueWithIndex<Imath::V3i>(attrName, instIndex);
            Imath::V3f narrowed;
            if (!dommy::numeric::checkedValueCast(value, narrowed))
                throw std::invalid_argument("Arnold instance vector attribute exceeds the finite Float range.");
            dommy::arnold::requireInstanceUserParameter(inst, attrName.c_str(), "constant VECTOR");
            if (inst != nullptr) {
                AiNodeSetVec(inst, attrName.c_str(), narrowed[0], narrowed[1], narrowed[2]);
            }
            break;
        }
        case DP_ATTR_VEC3F: {
            const Imath::V3f value = dPoints->attrListValueWithIndex<Imath::V3f>(attrName, instIndex);
            Imath::V3f narrowed;
            if (!dommy::numeric::checkedValueCast(value, narrowed))
                throw std::invalid_argument("Arnold instance vector attribute exceeds the finite Float range.");
            dommy::arnold::requireInstanceUserParameter(inst, attrName.c_str(), "constant VECTOR");
            if (inst != nullptr) {
                AiNodeSetVec(inst, attrName.c_str(), narrowed[0], narrowed[1], narrowed[2]);
            }
            break;
        }
        case DP_ATTR_VEC3D: {
            const Imath::V3d value = dPoints->attrListValueWithIndex<Imath::V3d>(attrName, instIndex);
            Imath::V3f narrowed;
            if (!dommy::numeric::checkedValueCast(value, narrowed))
                throw std::invalid_argument("Arnold instance vector attribute exceeds the finite Float range.");
            dommy::arnold::requireInstanceUserParameter(inst, attrName.c_str(), "constant VECTOR");
            if (inst != nullptr) {
                AiNodeSetVec(inst, attrName.c_str(), narrowed[0], narrowed[1], narrowed[2]);
            }
            break;
        }
        default:
            break; // Preserve unsupported/Bool attribute handling.
        }
    }
}


AtNode* DPInstancerTranslatorAi::generateAiMeshFromMPlug(MDataHandle inMeshHnd, unsigned int /*numMotionStep*/)
{
    MStatus status;
    MFnMesh baseMeshFn(inMeshHnd.data(), &status);
    if (!status) {
        AiMsgError("Unable to bind the Arnold instancer mesh input.");
        return nullptr;
    }
    dommy::mesh::MeshMergePlan importPlan;
    if (dommy::maya::preflightMeshImport(baseMeshFn, importPlan) != MS::kSuccess) {
        AiMsgError("Arnold instancer mesh counts exceed the input budget or could not be read.");
        return nullptr;
    }
    MPointArray pts;
    status = baseMeshFn.getPoints(pts, MSpace::kWorld);
    if (!status || pts.length() != importPlan.points) {
        AiMsgError("Unable to read the complete Arnold instancer vertex array.");
        return nullptr;
    }
    MIntArray faceCountArray, connectArray;
    status = baseMeshFn.getVertices(faceCountArray, connectArray);
    if (!status || faceCountArray.length() != importPlan.faces ||
        connectArray.length() != importPlan.connections ||
        !dommy::mesh::validPolygonArrays(pts.length(), faceCountArray, connectArray)) {
        AiMsgError("Arnold instancer mesh topology is invalid or could not be read.");
        return nullptr;
    }
    for (unsigned int point = 0U; point < pts.length(); ++point) {
        for (unsigned int axis = 0U; axis < 3U; ++axis) {
            if (!dommy::numeric::canRepresentAsFloat(pts[point][axis])) {
                AiMsgError("Arnold instancer vertex is outside the finite Float range.");
                return nullptr;
            }
        }
    }
    MFloatArray uvals, vvals;
    MIntArray uv_count_list, uv_connect_list;
    if (importPlan.uvs != 0U) {
        status = baseMeshFn.getUVs(uvals, vvals);
        if (!status || uvals.length() != importPlan.uvs || vvals.length() != importPlan.uvs) {
            AiMsgError("Unable to read the complete Arnold instancer UV array.");
            return nullptr;
        }
        status = baseMeshFn.getAssignedUVs(uv_count_list, uv_connect_list);
        if (!status) {
            AiMsgError("Unable to read Arnold instancer UV assignments.");
            return nullptr;
        }
    }
    bool validUVs = true;
    if (uv_connect_list.length() == 0U) {
        // A Maya UV set may contain unused coordinates but no assignments.
        // Preserve the historical geometry-only export for this valid case.
        validUVs = uv_count_list.length() == 0U ||
                   uv_count_list.length() == faceCountArray.length();
        for (unsigned int face = 0U; face < uv_count_list.length(); ++face)
            if (uv_count_list[face] != 0) validUVs = false;
        for (unsigned int uv = 0U; uv < uvals.length(); ++uv)
            if (!std::isfinite(uvals[uv]) || !std::isfinite(vvals[uv])) validUVs = false;
    } else {
        validUVs = dommy::mesh::validFullMeshUVArrays(
            faceCountArray, uvals, vvals, uv_count_list, uv_connect_list);
    }
    if (!validUVs) {
        AiMsgError("Arnold instancer UV topology is invalid.");
        return nullptr;
    }
    const std::size_t vertexNum = importPlan.points;
    const std::size_t faceNum = importPlan.faces;
    const std::size_t connectCount = importPlan.connections;

    dommy::arnold::ArrayLayout pointLayout;
    dommy::arnold::ArrayLayout faceLayout;
    dommy::arnold::ArrayLayout connectLayout;
    if (!dommy::arnold::checkedArrayLayout(
            vertexNum, 1U, pointLayout) ||
        !dommy::arnold::checkedArrayLayout(
            faceNum, 1U, faceLayout) ||
        !dommy::arnold::checkedArrayLayout(
            connectCount, 1U, connectLayout)) {
        AiMsgError(
            "Arnold instancer mesh arrays exceed the allocation budget.");
        return nullptr;
    }

    dommy::arnold::NodeOwner baseMeshOwner(AiNode("polymesh"));
    if (!baseMeshOwner) return nullptr;
    AtNode* const baseMesh = baseMeshOwner.get();
    // The prepared mesh must remain invisible until used by instances.
    AiNodeSetByte(baseMesh, "visibility", 0);
        
    dommy::arnold::ArrayOwner pointArray(AiArrayAllocate(
        pointLayout.elementCount, pointLayout.keyCount, AI_TYPE_VECTOR));
    dommy::arnold::ArrayOwner faceArray(AiArrayAllocate(
        faceLayout.elementCount, faceLayout.keyCount, AI_TYPE_UINT));
    dommy::arnold::ArrayOwner connectArrayOwner(AiArrayAllocate(
        connectLayout.elementCount, connectLayout.keyCount, AI_TYPE_UINT));
    if (!pointArray || !faceArray || !connectArrayOwner) {
        AiMsgError("Arnold could not allocate the instancer mesh arrays.");
        return nullptr;
    }
        
    size_t pointOffset=0;
    size_t faceOffset=0;
    size_t connectOffset=0;
        
    // Add vertices.
    for(size_t i=0; i<pts.length(); ++i) {
        AtVector aVec;
        aVec.x = pts[i][0];
        aVec.y = pts[i][1];
        aVec.z = pts[i][2];
            
        dommy::arnold::requireArrayWrite(AiArraySetVec(pointArray.get(), pointOffset++, aVec));
    }
        
    // Add face counts.
    for(size_t f=0;f<faceCountArray.length();++f) {
        dommy::arnold::requireArrayWrite(AiArraySetUInt(faceArray.get(), faceOffset++, faceCountArray[f]));
    }
        
    // Add vertex connections.
    for(size_t f=0;f<connectArray.length();++f) {
        dommy::arnold::requireArrayWrite(AiArraySetUInt(
            connectArrayOwner.get(), connectOffset++, connectArray[f]));
    }
        
    // Transfer polygon data ownership to Arnold.
        
    static const AtString vlistParam("vlist");
    static const AtString nsidesParam("nsides");
    static const AtString vidxsParam("vidxs");
        
    AiNodeSetArray(baseMesh, vlistParam, pointArray.release());
    AiNodeSetArray(baseMesh, nsidesParam, faceArray.release());
    AiNodeSetArray(baseMesh, vidxsParam, connectArrayOwner.release());
        
#pragma mark uv output
        
        
    if(uvals.length() && vvals.length() &&
       uv_count_list.length() && uv_connect_list.length()) {
        dommy::arnold::ArrayLayout uvLayout;
        dommy::arnold::ArrayLayout uvIndexLayout;
        if (!dommy::arnold::checkedArrayLayout(
                uvals.length(), 1U, uvLayout) ||
            !dommy::arnold::checkedArrayLayout(
                uv_connect_list.length(), 1U, uvIndexLayout)) {
            AiMsgWarning(
                "Skipping Arnold instancer UVs that exceed the allocation budget.");
            return baseMeshOwner.release();
        }
        dommy::arnold::ArrayOwner uvArray(AiArrayAllocate(
            uvLayout.elementCount, uvLayout.keyCount, AI_TYPE_VECTOR2));
        dommy::arnold::ArrayOwner uvConnectArray(AiArrayAllocate(
            uvIndexLayout.elementCount, uvIndexLayout.keyCount,
            AI_TYPE_UINT));
        if (!uvArray || !uvConnectArray) {
            AiMsgWarning(
                "Arnold could not allocate the instancer UV arrays; "
                "continuing without UVs.");
            return baseMeshOwner.release();
        }
            
        // Set UV values.
        for(size_t i=0; i<uvals.length(); ++i) {
            AtVector2 uvVal;
            uvVal.x = uvals[i];
            uvVal.y = vvals[i];
                
            dommy::arnold::requireArrayWrite(AiArraySetVec2(uvArray.get(), i, uvVal));
        }
        // Set UV connections.
        for(size_t i=0; i<uv_connect_list.length(); ++i) {
            dommy::arnold::requireArrayWrite(AiArraySetUInt(uvConnectArray.get(), i, uv_connect_list[i]));
        }
            
        static const AtString uvParam("uvlist");
        static const AtString uvIdxParam("uvidxs");
            
        AiNodeSetArray(baseMesh, uvParam, uvArray.release());
        AiNodeSetArray(baseMesh, uvIdxParam, uvConnectArray.release());
    }

    return baseMeshOwner.release();
}

#undef INST_DEBUG
#undef INST_VERBOSE
