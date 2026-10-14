//
//  DMeshTranslatorAi.cpp
//  dommyTool
//
//  Created by yuzo on 5/5/17.
//
//

#include "DMeshTranslatorAi.h"
#include "common/utility/DMayaPlugDataHandle.h"
#include "common/utility/DMayaNameList.h"
#include "common/utility/DAttributeExport.h"




#include <ai_msg.h>
#include <ai_nodes.h>

#include "DMayaHeaders.h"
#include "DMeshDataHeaders.h"

#include "DNodeOperation.h"
#include "DMeshDisplay.h"
#include "integrations/arnold/DArnoldArrayBudget.h"
#include "integrations/arnold/DArnoldMotionKey.h"
#include "integrations/arnold/DArnoldEntryPointGuard.h"
#include "integrations/arnold/DArnoldMayaPlug.h"
#include "integrations/arnold/DArnoldResources.h"
#include "integrations/arnold/DArnoldUserParameters.h"
#include "integrations/arnold/DArnoldNumeric.h"
#include "integrations/arnold/DArnoldRayFlags.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DWorkBudget.h"
#include "common/utility/DMeshMergePlan.h"
#include <maya/MFnMesh.h>
#include <maya/MFnMeshData.h>
#include <maya/MPointArray.h>
#include <maya/MIntArray.h>
#include <cmath>
#include <cstring>
#include <limits>
#include <memory>
#include <stdexcept>
#include <utility>

namespace {
// These renderer parameters use only the first one/three names. Unlike the
// full name-list helper, avoid allocating tokens that will never be used.
// The caller supplies a fresh local array and discards it on failure.
MStatus splitRenderGroupNames(const MString& value, unsigned int limit,
                             MStringArray& names, bool& hasMore)
{
    if (value.length() > dommy::generation::kMaxGeneratedStringByteCount)
        return MS::kInvalidParameter;
    const std::size_t bytes = std::strlen(value.asChar());
    std::size_t estimate = 0U;
    if (bytes > dommy::generation::kMaxGeneratedStringByteCount ||
        !dommy::generation::checkedProduct(bytes, 4U,
            dommy::mesh::kMaxMeshOperationBytes, estimate)) return MS::kInvalidParameter;
    const std::string text = value.asChar();
    std::size_t position = 0U;
    hasMore = false;
    while ((position = text.find_first_not_of(' ', position)) != std::string::npos) {
        if (names.length() == limit) { hasMore = true; break; }
        const std::size_t end = text.find(' ', position);
        const std::string token = text.substr(position, end == std::string::npos ? end : end - position);
        DOMMY_RETURN_IF_FAILURE(names.append(MString(token.c_str())));
        if (end == std::string::npos) break;
        position = end + 1U;
    }
    return MS::kSuccess;
}

struct GroupMatteRow {
    std::string name;
    // Moving rows must not invoke Maya's unchecked whole-array copy.
    std::unique_ptr<MStringArray> groups;
};

MStatus readGroupMatteRows(const MPlug& table, std::size_t faces, std::size_t meshes,
                         std::vector<GroupMatteRow>& output)
{
    std::vector<GroupMatteRow> candidate;
    if (table.isNull()) { output.swap(candidate); return MS::kSuccess; }
    MStatus status;
    const bool array = table.isArray(&status);
    DOMMY_RETURN_IF_FAILURE(status);
    if (!array) return MS::kInvalidParameter;
    const unsigned int count = table.numElements(&status);
    DOMMY_RETURN_IF_FAILURE(status);
    if (count > dommy::generation::kMaxGeneratedStringCount) return MS::kInvalidParameter;
    dommy::generation::WorkBudget work;
    if (!work.consume(count)) return MS::kInvalidParameter;
    std::size_t bytes = 0U, stringBytes = 0U;
    const std::size_t maximum = dommy::mesh::kMaxMeshOperationBytes;
    for (unsigned int index = 0U; index < count; ++index) {
        const MPlug row = table.elementByPhysicalIndex(index, &status);
        DOMMY_RETURN_IF_FAILURE(status);
        const MPlug enable = row.child(DMeshDisplay::aGroupMatteEnable, &status);
        DOMMY_RETURN_IF_FAILURE(status);
        bool enabled = false;
        DOMMY_RETURN_IF_FAILURE(readPlugValue(enable, enabled));
        if (!enabled) continue;
        const MPlug name = row.child(DMeshDisplay::aGroupMatteName, &status);
        DOMMY_RETURN_IF_FAILURE(status);
        const MPlug groups = row.child(DMeshDisplay::aMatteGroups, &status);
        DOMMY_RETURN_IF_FAILURE(status);
        MString nameValue, groupValue;
        DOMMY_RETURN_IF_FAILURE(readPlugValue(name, nameValue));
        DOMMY_RETURN_IF_FAILURE(readPlugValue(groups, groupValue));
        if (nameValue.length() > dommy::generation::kMaxGeneratedStringByteCount ||
            groupValue.length() > dommy::generation::kMaxGeneratedStringByteCount)
            return MS::kInvalidParameter;
        std::size_t part = 0U, estimate = 0U;
        if (!dommy::generation::checkedSum(stringBytes, std::strlen(nameValue.asChar()),
                dommy::generation::kMaxGeneratedStringByteCount, stringBytes) ||
            !dommy::generation::checkedSum(stringBytes, std::strlen(groupValue.asChar()),
                dommy::generation::kMaxGeneratedStringByteCount, stringBytes) ||
            !dommy::generation::checkedProduct(stringBytes, 4U, maximum, estimate))
            return MS::kInvalidParameter;
        if (nameValue.length() == 0U || groupValue.length() == 0U) continue;
        if (!work.consumeProduct(faces, 6U) || !work.consumeProduct(meshes, 3U) ||
            !dommy::generation::checkedProduct(faces, 24U * 4U, maximum, part) ||
            !dommy::generation::checkedSum(bytes, part, maximum, bytes) ||
            !dommy::generation::checkedSum(bytes, sizeof(GroupMatteRow), maximum, bytes) ||
            !dommy::generation::checkedSum(bytes, estimate, maximum, part)) return MS::kInvalidParameter;
        GroupMatteRow value;
        value.name = nameValue.asChar();
        value.groups.reset(new MStringArray());
        bool ignoredGroups = false;
        DOMMY_RETURN_IF_FAILURE(splitRenderGroupNames(groupValue, 3U, *value.groups, ignoredGroups));
        if (value.groups->length() != 0U) candidate.push_back(std::move(value));
    }
    output.swap(candidate);
    return MS::kSuccess;
}

template <typename Owner>
MStatus collectCreaseIndices(const Owner& source, const MString& name, DPIndexList& output)
{
    DPIndexList candidate;
    std::size_t pointOffset = 0U;
    dommy::generation::WorkBudget work;
    if (source.numberOfMesh() > dommy::generation::kMaxGeneratedMeshCount ||
        !work.consume(source.numberOfMesh())) return MS::kInvalidParameter;
    dommy::mesh::MeshMergePlan geometry;
    for (auto mesh = source.beginMeshes(); mesh != source.endMeshes(); ++mesh) {
        if (!*mesh || (*mesh)->numFace() < 0) return MS::kInvalidParameter;
        const std::size_t points = (*mesh)->numberOfPoints();
        std::size_t nextOffset = 0U;
        if (!dommy::generation::checkedSum(pointOffset, points,
                dommy::generation::kMaxGeneratedPointCount, nextOffset)) return MS::kInvalidParameter;
        if ((*mesh)->isExistCompGrp(D_COMP_EDGE, name.asChar())) {
            const DPGroupData::ConstPtr group = (*mesh)->getCompGrp(D_COMP_EDGE, name.asChar());
            if (!group) return MS::kInvalidParameter;
            const DPIdList& selected = group->getGroupIds();
            if ((*mesh)->numFace() == 0) {
                if (!selected.empty()) return MS::kInvalidParameter;
                pointOffset = nextOffset;
                continue;
            }
            const std::size_t previousWork = geometry.workItems;
            if (!dommy::mesh::appendMeshMergePlan(points, (*mesh)->numFace(),
                    (*mesh)->numberOfConnections(), 0U, geometry) ||
                !dommy::mesh::validMeshOperationStorage(geometry) ||
                !work.consume(geometry.workItems - previousWork)) return MS::kInvalidParameter;
            MStatus status;
            MFnMeshData data;
            const MObject owner = data.create(&status);
            DOMMY_RETURN_IF_FAILURE(status);
            if (owner.isNull()) return MS::kFailure;
            MFnMesh nativeMesh;
            const MObject created = nativeMesh.create(static_cast<int>(points), (*mesh)->numFace(),
                (*mesh)->getMPosition(), (*mesh)->getFaceList(), (*mesh)->getConnectList(), owner, &status);
            DOMMY_RETURN_IF_FAILURE(status);
            if (created.isNull()) return MS::kFailure;
            const int edges = nativeMesh.numEdges(&status);
            DOMMY_RETURN_IF_FAILURE(status);
            if (edges < 0 || selected.size() != static_cast<std::size_t>(edges) ||
                !work.consume(static_cast<std::size_t>(edges))) return MS::kInvalidParameter;
            for (int edge = 0; edge < edges; ++edge) {
                if (!selected[static_cast<std::size_t>(edge)]) continue;
                int2 vertices;
                DOMMY_RETURN_IF_FAILURE(nativeMesh.getEdgeVertices(edge, vertices));
                if (vertices[0] < 0 || vertices[1] < 0 ||
                    static_cast<std::size_t>(vertices[0]) >= points ||
                    static_cast<std::size_t>(vertices[1]) >= points) return MS::kInvalidParameter;
                std::size_t count = 0U, bytes = 0U;
                if (!dommy::generation::checkedSum(candidate.size(), 2U,
                        dommy::arnold::kMaxArnoldArrayValues, count) ||
                    !dommy::generation::checkedProduct(count, sizeof(std::size_t) * 4U,
                        dommy::mesh::kMaxMeshOperationBytes, bytes) ||
                    !dommy::mesh::validMeshOperationStorage(geometry, bytes)) return MS::kInvalidParameter;
                candidate.push_back(pointOffset + static_cast<std::size_t>(vertices[0]));
                candidate.push_back(pointOffset + static_cast<std::size_t>(vertices[1]));
            }
        }
        pointOffset = nextOffset;
    }
    output.swap(candidate);
    return MS::kSuccess;
}
} // namespace




void DMeshTranslatorAi::Export(AtNode* node) try
{
	//    cout << "Export" << endl;
	AiMsgDebug("[dommyTool] Exporting %s", GetMayaNodeName().asChar());
	Update(node);
}
DOMMY_ARNOLD_TRANSLATOR_CATCH("DMeshTranslatorAi::Export", node)



void DMeshTranslatorAi::NodeChanged(MObject& node, MPlug& plug) try
{
	MStatus status;
    MFnDependencyNode nodeFn(node, &status);
    if (!status) {
        AiMsgError("[DMeshTranslatorAi] Unable to inspect the changed Maya node.");
        return;
    }
	
	
	const MString pName = plug.partialName(
        false, false, false, false, false, true, &status);
    if (!status) {
        AiMsgError("[DMeshTranslatorAi] Unable to read the changed Maya plug name.");
        return;
    }
	if(pName == "inDM" ||
	   pName == "IsRender" ||
	   pName == "aiOpaque" ||
       pName == "aiSelfShadows" ||
       pName == "aiMatte" ||
	   pName == "IsStaticObject" ||
	   pName == "Smoothing" ||
	   pName == "CreaseEdgeGroup" ||
	   pName == "ExportUVs" ||
	   pName == "SubdivType" ||
	   pName == "SubdivIterations" ||
	   pName == "SubdivAdaptiveError" ||
	   pName == "SubdivDicingCamera" ||
	   pName == "SubdivAdaptiveMetric" ||
	   pName == "SubdivUVSmoothing" ||
	   pName == "SubdivAdaptiveSpace" ||
	   pName == "SubdivSmoothDerivs" ||
	   pName == "DispMap" ||
	   pName == "DispHeight" ||
	   pName == "DispZeroValue" ||
	   pName == "DispPadding" ||
	   pName == "DispAutobump" ||
	   pName == "Sidedness" ||
	   pName == "InvertNormals" ||
	   
	   pName == "ExportMeshAttribute" ||
	   pName == "ExportPointAttribute" ||
	   pName == "GroupMatte" ||
	   pName == "GroupMatteEnable" ||
	   pName == "GroupMatteName" ||
	   pName == "MatteGroups" ||
	   pName == "MeshShader" ||
	   pName == "DisplacementShader") {
		
		
		SetUpdateMode(CNodeTranslator::AI_RECREATE_TRANSLATOR);
		
		RequestUpdate();
	}
	
	
}

DOMMY_ARNOLD_TRANSLATOR_CATCH("DMeshTranslatorAi::NodeChanged", nullptr)





void DMeshTranslatorAi::Update(AtNode* node) try
{
	unsigned int numMotionStep = GetNumMotionSteps();
	dommy::arnold::PlugReader plugs(
		[this](const char* name) -> MPlug { return FindMayaPlug(name); },
		"DMeshTranslatorAi");
	// Export the transform matrix
	//	ExportMatrix(node, 0);
	    bool isRender = false;
	if (!plugs.read("IsRender", isRender)) {
		return;
	}
    
    static const AtString visParam("visibility");
    

	
    MPlug dmPlug;
    bool inputConnected = false;
    if (isRender) {
        dmPlug = FindMayaPlug("inDM");
        if (!plugs.connection(dmPlug, "inDM", inputConnected)) return;
    }
    ExportMatrix(node);
    AiNodeSetByte(node, visParam, isRender);
	if (isRender && inputConnected) {
		
		dommy::arnold::UpdateVisibilityGuard updateVisibility(node);
		dommy::maya::ScopedPlugDataHandle dMeshHndOwner;
		MDataHandle& dMeshHnd = dMeshHndOwner.handle();
		if (dMeshHndOwner.read(dmPlug) != MS::kSuccess) {
			AiMsgError("[dommyTool] Unable to read Arnold DMesh input.");
			return;
		}
		const DMeshData* const dMeshes(getInputDMfromHandle(dMeshHnd));
		if (!dMeshes) {
		    AiMsgError("[dommyTool] Arnold DMesh input has invalid type or topology.");
		    return;
		}
        
        
		if (dMeshes && dMeshes->numberOfMesh()) {
			if (!dMeshes->hasValidMeshTopology()) {
				AiMsgError("Arnold mesh export rejected invalid DMesh data.");
				AiNodeSetByte(node, visParam, 0);
				return;
			}

			bool smoothing = false;
			bool sidedness = false;
			bool invertNormals = false;
			short subdivType = 0;
			int subdivIterations = 0;
			float subdivAdaptiveError = 0.0f;
			short subdivAdaptiveMetric = 0;
			short subdivUvSmoothing = 0;
			short subdivAdaptiveSpace = 0;
			bool subdivSmoothDerivs = false;
			float displacementHeight = 0.0f;
			float displacementZeroValue = 0.0f;
			float displacementPadding = 0.0f;
			bool displacementAutobump = false;
			bool exportUvs = false;
			MString exportMeshAttributeNames;
			MString exportPointAttributeNames;
			MString creaseEdgeGroupNames;
			if (!plugs.read("Smoothing", smoothing) ||
				!plugs.read("Sidedness", sidedness) ||
				!plugs.read("InvertNormals", invertNormals) ||
				!plugs.read("SubdivType", subdivType) ||
				!plugs.read("SubdivIterations", subdivIterations) ||
				!plugs.read(
					"SubdivAdaptiveError", subdivAdaptiveError) ||
				!plugs.read(
					"SubdivAdaptiveMetric", subdivAdaptiveMetric) ||
				!plugs.read("SubdivUVSmoothing", subdivUvSmoothing) ||
				!plugs.read(
					"SubdivAdaptiveSpace", subdivAdaptiveSpace) ||
				!plugs.read("SubdivSmoothDerivs", subdivSmoothDerivs) ||
				!plugs.read("DispHeight", displacementHeight) ||
				!plugs.read("DispZeroValue", displacementZeroValue) ||
				!plugs.read("DispPadding", displacementPadding) ||
				!plugs.read("DispAutobump", displacementAutobump) ||
				!plugs.read("ExportUVs", exportUvs) ||
				!plugs.read(
					"ExportMeshAttribute", exportMeshAttributeNames) ||
				!plugs.read(
					"ExportPointAttribute", exportPointAttributeNames) ||
				!plugs.read("CreaseEdgeGroup", creaseEdgeGroupNames) ||
				subdivType < 0 || subdivType > 2 ||
				subdivIterations < 0 || subdivIterations > 255 ||
				subdivAdaptiveMetric < 0 || subdivAdaptiveMetric > 2 ||
				subdivUvSmoothing < 0 || subdivUvSmoothing > 3 ||
				subdivAdaptiveSpace < 0 || subdivAdaptiveSpace > 1 ||
				!std::isfinite(subdivAdaptiveError) ||
				subdivAdaptiveError < 0.0f ||
				!std::isfinite(displacementHeight) ||
				!std::isfinite(displacementZeroValue) ||
				!std::isfinite(displacementPadding) ||
				displacementPadding < 0.0f) {
				AiMsgError("[dommyTool] DMesh render parameters are invalid.");
				AiNodeSetByte(node, visParam, 0);
				return;
			}
			
			
#pragma mark preflight export data
			size_t totalVertexCount = 0U;
			size_t totalFaceCount = 0U;
			size_t totalConnectCount = 0U;
			dommy::arnold::ArrayLayout pointLayout;
			dommy::arnold::ArrayLayout pointAttributeLayout;
			dommy::arnold::ArrayLayout faceLayout;
			dommy::arnold::ArrayLayout connectLayout;
			if (!dMeshes->getCheckedTotalVertex(totalVertexCount) ||
				!dMeshes->getCheckedTotalFace(totalFaceCount) ||
				!dMeshes->getCheckedTotalConnect(totalConnectCount) ||
				!dommy::arnold::checkedArrayLayout(
					totalVertexCount, numMotionStep, pointLayout) ||
				!dommy::arnold::checkedArrayLayout(
					totalVertexCount, 1U, pointAttributeLayout) ||
				!dommy::arnold::checkedArrayLayout(
					totalFaceCount, 1U, faceLayout) ||
				!dommy::arnold::checkedArrayLayout(
					totalConnectCount, 1U, connectLayout)) {
				AiMsgError("Arnold mesh array exceeds the supported allocation budget.");
				AiNodeSetByte(node, visParam, 0);
				return;
			}
			dommy::maya::NameListBudget nameListBudget;
			MStringArray meshAttrNameArray;
			MStringArray pointAttrNameArray;
			if (dommy::maya::splitCheckedNameList(
			        exportMeshAttributeNames, totalFaceCount, nameListBudget, meshAttrNameArray) != MS::kSuccess) {
				AiMsgError("[DMeshTranslatorAi] Unable to parse the mesh attribute list.");
				return;
			}
			if (dommy::maya::splitCheckedNameList(
			        exportPointAttributeNames, totalVertexCount, nameListBudget, pointAttrNameArray) != MS::kSuccess) {
				AiMsgError("[DMeshTranslatorAi] Unable to parse the point attribute list.");
				return;
			}

#pragma mark make option
			static const AtString smoothingParam("smoothing");
			AiNodeSetBool(node, smoothingParam, smoothing);
			
			static const AtString sidednessParam("sidedness");
			AiNodeSetByte(node, sidednessParam, dommy::arnold::sidednessMask(sidedness));
			
			static const AtString invNormalParam("invert_normals");
			AiNodeSetBool(node, invNormalParam, invertNormals);
			
#pragma mark set subdiv
			static const AtString subdivTypeParam("subdiv_type");
			
			if(subdivType>0) {
				switch (subdivType) {
					case 0:
					
						break;
					case 1:
					{
						AtString cParam("linear");
						AiNodeSetStr( node, subdivTypeParam, cParam );
					}
						break;
					case 2:
					{
						AtString cParam("catclark");
						AiNodeSetStr( node, subdivTypeParam, cParam );
					}
						break;
					default:
						break;
				}
				
				
				static const AtString subdivIterParam("subdiv_iterations");
				AiNodeSetByte(node, subdivIterParam, subdivIterations);
				
				static const AtString subdivadaptiveerrorParam("subdiv_adaptive_error");
				AiNodeSetFlt(
					node, subdivadaptiveerrorParam, subdivAdaptiveError);
				
				static const AtString subdivadaptivemetricParam("subdiv_adaptive_metric");
				
				switch (subdivAdaptiveMetric) {
					case 0:
					{
						AtString cParam("auto");
						AiNodeSetStr( node, subdivadaptivemetricParam, cParam );
					}
						break;
					case 1:
					{
						AtString cParam("flatness");
						AiNodeSetStr( node, subdivadaptivemetricParam, cParam );
					}
						break;
					case 2:
					{
						AtString cParam("edge_length");
						AiNodeSetStr( node, subdivadaptivemetricParam, cParam );
					}
						break;
					default:
						break;
				}
				
				static const AtString subdivuvsmoothingParam("subdiv_uv_smoothing");
				
				switch (subdivUvSmoothing) {
					case 0:
					{
						AtString cParam("linear");
						AiNodeSetStr( node, subdivuvsmoothingParam, cParam );
					}
						break;
					case 1:
					{
						AtString cParam("pin_borders");
						AiNodeSetStr( node, subdivuvsmoothingParam, cParam );
					}
						break;
					case 2:
					{
						AtString cParam("pin_corners");
						AiNodeSetStr( node, subdivuvsmoothingParam, cParam );
					}
						break;
					case 3:
					{
						AtString cParam("smooth");
						AiNodeSetStr( node, subdivuvsmoothingParam, cParam );
					}
						break;
					default:
						break;
				}
				
				static const AtString subdivadaptivespaceParam("subdiv_adaptive_space");
				
				switch (subdivAdaptiveSpace) {
					case 0:
					{
						AtString cParam("raster");
						AiNodeSetStr( node, subdivadaptivespaceParam, cParam );
					}
						break;
					case 1:
					{
						AtString cParam("object");
						AiNodeSetStr( node, subdivadaptivespaceParam, cParam );
					}
						break;
					default:
						break;
				}
				
				
				static const AtString subdivsmoothderivParam("subdiv_smooth_derivs");
				AiNodeSetBool(
					node, subdivsmoothderivParam, subdivSmoothDerivs);
				
				
			}
			
#pragma mark make displacement
			
			
			static const AtString dispHeightParam("disp_height");
			AiNodeSetFlt(node, dispHeightParam, displacementHeight);
			
			static const AtString dispzeroParam("disp_zero_value");
			AiNodeSetFlt(node, dispzeroParam, displacementZeroValue);
			
			static const AtString disppadParam("disp_padding");
			AiNodeSetFlt(node, disppadParam, displacementPadding);
			
			
			static const AtString dispAutobumpParam("disp_autobump");
			AiNodeSetBool(node, dispAutobumpParam, displacementAutobump);
			
			
			
#pragma mark make polygon data
			
			dommy::arnold::ArrayOwner pointarray(AiArrayAllocate(
				pointLayout.elementCount, pointLayout.keyCount, AI_TYPE_VECTOR));
			if (!pointarray) {
				AiMsgError("Unable to allocate the Arnold mesh point array.");
				return;
			}
			dommy::arnold::ArrayOwner faceArray(AiArrayAllocate(
				faceLayout.elementCount, faceLayout.keyCount, AI_TYPE_UINT));
			dommy::arnold::ArrayOwner connectArray(AiArrayAllocate(
				connectLayout.elementCount, connectLayout.keyCount,
				AI_TYPE_UINT));
			if (!faceArray || !connectArray) {
				AiMsgError("Unable to allocate the Arnold mesh topology arrays.");
				return;
			}
			
			size_t pointOffset=0;
			size_t faceOffset=0;
			size_t connectOffset=0;
			size_t meshPtOffset=0;
			
			for (auto mIt=dMeshes->beginMeshes(); mIt!=dMeshes->endMeshes(); ++mIt) {
				
                DPointListPtr pts;
                MMatrix mat;
                if (!(*mIt)->isIdentity()) {
                    pts.reset(new DPointList((*mIt)->getPointsInWorld()));
                } else {
                    pts.reset(new DPointList(
                        *(*mIt)->getPositionPtrConst()));
                }
				//addd vertex
                for(auto &pIt:*pts) {
					
					AtVector aVec;
					aVec.x = pIt[0];
					aVec.y = pIt[1];
					aVec.z = pIt[2];
					
					dommy::arnold::requireArrayWrite(AiArraySetVec(pointarray.get(), pointOffset++, aVec));
				}
				
				//add face count
				MIntArray faceCounts = (*mIt)->getFaceList();
				for(size_t f=0;f<faceCounts.length();++f) {
					dommy::arnold::requireArrayWrite(AiArraySetUInt(faceArray.get(), faceOffset++, faceCounts[f]));
				}
				
				//add face connect
				MIntArray faceConnects = (*mIt)->getConnectList();
				
				for(size_t f=0;f<faceConnects.length();++f) {
					dommy::arnold::requireArrayWrite(AiArraySetUInt(
						connectArray.get(), connectOffset++,
						faceConnects[f] + meshPtOffset));
				}
				
				meshPtOffset+=(*mIt)->numberOfPoints();
				
			}
			
			
			//set face data
			
			
			static const AtString vlistParam("vlist");
			static const AtString nsidesParam("nsides");
			static const AtString vidxsParam("vidxs");
			
			AiNodeSetArray(node, vlistParam, pointarray.release());
			AiNodeSetArray(node, nsidesParam, faceArray.release());
			AiNodeSetArray(node, vidxsParam, connectArray.release());
			
			
#pragma mark uv output
			
			
			
			
			if (exportUvs) {
				
				MFloatArray uvals,vvals;
//				MIntArray uvCounts;
				MIntArray uvConnects;
				size_t offset=0;
				for (auto mIt=dMeshes->beginMeshes(); mIt!=dMeshes->endMeshes(); ++mIt) {
					
					bool hasUV;
					(*mIt)->hasUVData(hasUV);
					
					if(hasUV) {
						MFloatArray tmpUval,tmpVval;
						MIntArray tmpCounts,tmpConnects;
						if ((*mIt)->getUvData(tmpUval, tmpVval, tmpCounts, tmpConnects) !=
							MS::kSuccess) {
							AiMsgError("Unable to copy the DMesh UV arrays.");
							AiNodeSetByte(node, visParam, 0);
							return;
						}
						
						
						for (size_t i=0; i<tmpUval.length(); ++i) {
							if (uvals.append(tmpUval[i]) != MS::kSuccess) {
								AiMsgError("Arnold U coordinate construction failed.");
								AiNodeSetByte(node, visParam, 0);
								return;
							}
						}
						for (size_t i=0; i<tmpVval.length(); ++i) {
							if (vvals.append(tmpVval[i]) != MS::kSuccess) {
								AiMsgError("Arnold V coordinate construction failed.");
								AiNodeSetByte(node, visParam, 0);
								return;
							}
						}
//						for (size_t i=0; i<tmpCounts.length(); ++i) {
//							uvCounts.append(tmpCounts[i]);
//						}
						std::size_t nextOffset = 0U;
						if (!dommy::memory::checkedAdd(
								offset, static_cast<std::size_t>(tmpUval.length()),
								nextOffset) ||
							nextOffset > static_cast<std::size_t>(
								std::numeric_limits<int>::max())) {
							AiMsgError("Arnold UV array exceeds Maya index limits.");
							AiNodeSetByte(node, visParam, 0);
							return;
						}
						for (size_t i=0; i<tmpConnects.length(); ++i) {
							const std::size_t shifted = offset +
								static_cast<std::size_t>(tmpConnects[i]);
							if (shifted > static_cast<std::size_t>(
									std::numeric_limits<int>::max()) ||
								uvConnects.append(static_cast<int>(shifted)) !=
									MS::kSuccess) {
								AiMsgError("Arnold UV index construction failed.");
								AiNodeSetByte(node, visParam, 0);
								return;
							}
						}
						
						
						offset = nextOffset;
					}
					
				}
				
				
				
				if (uvals.length() && vvals.length() &&
                    uvConnects.length()) {
					dommy::arnold::ArrayLayout uvLayout;
					dommy::arnold::ArrayLayout uvIndexLayout;
					if (!dommy::arnold::checkedArrayLayout(
							uvals.length(), 1U, uvLayout) ||
						!dommy::arnold::checkedArrayLayout(
							uvConnects.length(), 1U, uvIndexLayout)) {
						AiMsgError("Arnold UV arrays exceed the allocation budget.");
						AiNodeSetByte(node, visParam, 0);
						return;
					}
					dommy::arnold::ArrayOwner uvArray(AiArrayAllocate(
						uvLayout.elementCount, uvLayout.keyCount, AI_TYPE_VECTOR2));
					dommy::arnold::ArrayOwner uvIndexArray(AiArrayAllocate(
						uvIndexLayout.elementCount, uvIndexLayout.keyCount,
						AI_TYPE_UINT));
					if (!uvArray || !uvIndexArray) {
						AiMsgError("Unable to allocate the Arnold UV arrays.");
						AiNodeSetByte(node, visParam, 0);
						return;
					}
					
					for(size_t i=0; i<uvals.length(); ++i) {
						AtVector2 uvVal;
						uvVal.x = uvals[i];
						uvVal.y = vvals[i];
						
						dommy::arnold::requireArrayWrite(AiArraySetVec2(uvArray.get(), i, uvVal));
					}
					for(size_t i=0; i<uvConnects.length(); ++i) {
						dommy::arnold::requireArrayWrite(AiArraySetUInt(uvIndexArray.get(), i, uvConnects[i]));
					}
					
					static const AtString uvParam("uvlist");
					static const AtString uvIdxParam("uvidxs");
					
					AiNodeSetArray(node, uvParam, uvArray.release());
					AiNodeSetArray(node, uvIdxParam, uvIndexArray.release());
					
				}
				
				
			}
			
			
			
			
#pragma mark export mesh attribute
            
            ///////////////////////////////////////////////////////////////
            //constant values exist on a per-object basis
            //uniform values exist on a per-face basis
            //varying values exist on a per-vertex basis
            ///////////////////////////////////////////////////////////////
            
            
            
            
            //primitive attribute はfaceで書き出し
            
            
            
            for (uint i=0; i<meshAttrNameArray.length(); i++) {
                std::string attrName = meshAttrNameArray[i].asChar();
                
                size_t offset=0;
                
                if (dMeshes->isExistAttr(attrName)) {
                    DP_ATTR_DATA_TYPE type = dMeshes->getAttr(attrName)->getAttrType();
                    
                    if (type == DP_ATTR_INT) {
                        std::vector<int32_t> tmp = dMeshes->attrListValue<int32_t>(attrName);
                        
                        dommy::arnold::ArrayOwner outArray(AiArrayAllocate(
                            faceLayout.elementCount, faceLayout.keyCount,
                            AI_TYPE_INT));
						if (!outArray) {
							AiMsgError("Unable to allocate an Arnold integer attribute.");
							return;
						}
                        
                        size_t meshoffset=0;
                        for (auto mIt=dMeshes->beginMeshes(); mIt!=dMeshes->endMeshes(); ++mIt) {
                            for (size_t f = 0; f < (*mIt)->numFace(); f++) {
                                dommy::arnold::requireArrayWrite(AiArraySetInt(
									outArray.get(), offset++, tmp[meshoffset]));
                            }
                            meshoffset++;
                        }
                        
                        
                        dommy::arnold::requireUserParameter(node, attrName.c_str(), "uniform INT");
                        AiNodeSetArray(
							node, attrName.c_str(), outArray.release());
                        
                    } else if (type == DP_ATTR_FLOAT) {
                        
                        std::vector<float> tmp = dMeshes->attrListValue<float>(attrName);
                        
                        dommy::arnold::ArrayOwner outArray(AiArrayAllocate(
                            faceLayout.elementCount, faceLayout.keyCount,
                            AI_TYPE_FLOAT));
						if (!outArray) {
							AiMsgError("Unable to allocate an Arnold float attribute.");
							return;
						}
                        
                        size_t meshoffset=0;
                        for (auto mIt=dMeshes->beginMeshes(); mIt!=dMeshes->endMeshes(); ++mIt) {
                            for (size_t f = 0; f < (*mIt)->numFace(); f++) {
                                dommy::arnold::requireArrayWrite(AiArraySetFlt(
									outArray.get(), offset++, tmp[meshoffset]));
                            }
                            meshoffset++;
                        }
                        
                        dommy::arnold::requireUserParameter(node, attrName.c_str(), "uniform FLOAT");
                        AiNodeSetArray(
							node, attrName.c_str(), outArray.release());
                        
                        
                    } else if (type == DP_ATTR_DOUBLE) {
                        
                        std::vector<double> tmp = dMeshes->attrListValue<double>(attrName);
                        
                        dommy::arnold::ArrayOwner outArray(AiArrayAllocate(
                            faceLayout.elementCount, faceLayout.keyCount,
                            AI_TYPE_FLOAT));
						if (!outArray) {
							AiMsgError("Unable to allocate an Arnold float attribute.");
							return;
						}
                        
                        size_t meshoffset=0;
                        for (auto mIt=dMeshes->beginMeshes(); mIt!=dMeshes->endMeshes(); ++mIt) {
                            for (size_t f = 0; f < (*mIt)->numFace(); f++) {
                                dommy::arnold::requireArrayWrite(AiArraySetFlt(
									outArray.get(), offset++,
									dommy::arnold::checkedFloat(tmp[meshoffset])));
                            }
                            meshoffset++;
                        }
                        
                        dommy::arnold::requireUserParameter(node, attrName.c_str(), "uniform FLOAT");
                        AiNodeSetArray(
							node, attrName.c_str(), outArray.release());
                        
                        
                    } else if (type == DP_ATTR_VEC2I) {
                        std::vector<Imath::V2i> tmp = dMeshes->attrListValue<Imath::V2i>(attrName);
                        
                        dommy::arnold::ArrayOwner outArray(AiArrayAllocate(
                            faceLayout.elementCount, faceLayout.keyCount,
                            AI_TYPE_VECTOR2));
						if (!outArray) {
							AiMsgError("Unable to allocate an Arnold vector2 attribute.");
							return;
						}
                        
                        size_t meshoffset=0;
                        for (auto mIt=dMeshes->beginMeshes(); mIt!=dMeshes->endMeshes(); ++mIt) {
                            for (size_t f = 0; f < (*mIt)->numFace(); f++) {
                                AtVector2 pt2;
                                pt2.x = dommy::arnold::checkedFloat(tmp[meshoffset][0]);
                                pt2.y = dommy::arnold::checkedFloat(tmp[meshoffset][1]);
                                dommy::arnold::requireArrayWrite(AiArraySetVec2(
									outArray.get(), offset++, pt2));
                            }
                            meshoffset++;
                        }
                        
                        
                        dommy::arnold::requireUserParameter(node, attrName.c_str(), "uniform POINT2");
                        AiNodeSetArray(
							node, attrName.c_str(), outArray.release());
                        
                    } else if (type == DP_ATTR_VEC2F) {
                        std::vector<Imath::V2f> tmp = dMeshes->attrListValue<Imath::V2f>(attrName);
                        
                        dommy::arnold::ArrayOwner outArray(AiArrayAllocate(
                            faceLayout.elementCount, faceLayout.keyCount,
                            AI_TYPE_VECTOR2));
						if (!outArray) {
							AiMsgError("Unable to allocate an Arnold vector2 attribute.");
							return;
						}
                        
                        size_t meshoffset=0;
                        for (auto mIt=dMeshes->beginMeshes(); mIt!=dMeshes->endMeshes(); ++mIt) {
                            for (size_t f = 0; f < (*mIt)->numFace(); f++) {
                                AtVector2 pt2;
                                pt2.x = dommy::arnold::checkedFloat(tmp[meshoffset][0]);
                                pt2.y = dommy::arnold::checkedFloat(tmp[meshoffset][1]);
                                dommy::arnold::requireArrayWrite(AiArraySetVec2(
									outArray.get(), offset++, pt2));
                            }
                            meshoffset++;
                        }
                        
                        dommy::arnold::requireUserParameter(node, attrName.c_str(), "uniform POINT2");
                        AiNodeSetArray(
							node, attrName.c_str(), outArray.release());
                        
                    } else if (type == DP_ATTR_VEC2D) {
                        std::vector<Imath::V2d> tmp = dMeshes->attrListValue<Imath::V2d>(attrName);
                        
                        dommy::arnold::ArrayOwner outArray(AiArrayAllocate(
                            faceLayout.elementCount, faceLayout.keyCount,
                            AI_TYPE_VECTOR2));
						if (!outArray) {
							AiMsgError("Unable to allocate an Arnold vector2 attribute.");
							return;
						}
                        
                        size_t meshoffset=0;
                        for (auto mIt=dMeshes->beginMeshes(); mIt!=dMeshes->endMeshes(); ++mIt) {
                            for (size_t f = 0; f < (*mIt)->numFace(); f++) {
                                AtVector2 pt2;
                                pt2.x = dommy::arnold::checkedFloat(tmp[meshoffset][0]);
                                pt2.y = dommy::arnold::checkedFloat(tmp[meshoffset][1]);
                                dommy::arnold::requireArrayWrite(AiArraySetVec2(
									outArray.get(), offset++, pt2));
                            }
                            meshoffset++;
                        }
                        
                        
                        dommy::arnold::requireUserParameter(node, attrName.c_str(), "uniform POINT2");
                        AiNodeSetArray(
							node, attrName.c_str(), outArray.release());
                        
                    } else if (type == DP_ATTR_VEC3I) {
                        std::vector<Imath::V3i> tmp = dMeshes->attrListValue<Imath::V3i>(attrName);
                        
                        dommy::arnold::ArrayOwner outArray(AiArrayAllocate(
                            faceLayout.elementCount, faceLayout.keyCount,
                            AI_TYPE_VECTOR));
						if (!outArray) {
							AiMsgError("Unable to allocate an Arnold vector attribute.");
							return;
						}
                        
                        size_t meshoffset=0;
                        for (auto mIt=dMeshes->beginMeshes(); mIt!=dMeshes->endMeshes(); ++mIt) {
                            for (size_t f = 0; f < (*mIt)->numFace(); f++) {
                                AtVector vec;
                                vec.x = dommy::arnold::checkedFloat(tmp[meshoffset][0]);
                                vec.y = dommy::arnold::checkedFloat(tmp[meshoffset][1]);
                                vec.z = dommy::arnold::checkedFloat(tmp[meshoffset][2]);
                                dommy::arnold::requireArrayWrite(AiArraySetVec(outArray.get(), offset++, vec));
                            }
                            meshoffset++;
                        }
                        
                        
                        
                        dommy::arnold::requireUserParameter(node, attrName.c_str(), "uniform VECTOR");
                        AiNodeSetArray(
							node, attrName.c_str(), outArray.release());
                        
                    } else if (type == DP_ATTR_VEC3F) {
                        std::vector<Imath::V3f> tmp = dMeshes->attrListValue<Imath::V3f>(attrName);
                        
                        dommy::arnold::ArrayOwner outArray(AiArrayAllocate(
                            faceLayout.elementCount, faceLayout.keyCount,
                            AI_TYPE_VECTOR));
						if (!outArray) {
							AiMsgError("Unable to allocate an Arnold vector attribute.");
							return;
						}
                        
                        size_t meshoffset=0;
                        for (auto mIt=dMeshes->beginMeshes(); mIt!=dMeshes->endMeshes(); ++mIt) {
                            for (size_t f = 0; f < (*mIt)->numFace(); f++) {
                                AtVector vec;
                                vec.x = dommy::arnold::checkedFloat(tmp[meshoffset][0]);
                                vec.y = dommy::arnold::checkedFloat(tmp[meshoffset][1]);
                                vec.z = dommy::arnold::checkedFloat(tmp[meshoffset][2]);
                                dommy::arnold::requireArrayWrite(AiArraySetVec(outArray.get(), offset++, vec));
                            }
                            meshoffset++;
                            
                        }
                        
                        dommy::arnold::requireUserParameter(node, attrName.c_str(), "uniform VECTOR");
                        AiNodeSetArray(
							node, attrName.c_str(), outArray.release());
                        
                    } else if (type == DP_ATTR_VEC3D) {
                        std::vector<Imath::V3d> tmp = dMeshes->attrListValue<Imath::V3d>(attrName);
                        
                        dommy::arnold::ArrayOwner outArray(AiArrayAllocate(
                            faceLayout.elementCount, faceLayout.keyCount,
                            AI_TYPE_VECTOR));
						if (!outArray) {
							AiMsgError("Unable to allocate an Arnold vector attribute.");
							return;
						}
                        
                        size_t meshoffset=0;
                        for (auto mIt=dMeshes->beginMeshes(); mIt!=dMeshes->endMeshes(); ++mIt) {
                            for (size_t f = 0; f < (*mIt)->numFace(); f++) {
                                AtVector vec;
                                vec.x = dommy::arnold::checkedFloat(tmp[meshoffset][0]);
                                vec.y = dommy::arnold::checkedFloat(tmp[meshoffset][1]);
                                vec.z = dommy::arnold::checkedFloat(tmp[meshoffset][2]);
                                dommy::arnold::requireArrayWrite(AiArraySetVec(outArray.get(), offset++, vec));
                            }
                            meshoffset++;
                        }
                        
                        dommy::arnold::requireUserParameter(node, attrName.c_str(), "uniform VECTOR");
                        AiNodeSetArray(
							node, attrName.c_str(), outArray.release());
                        
                    } else {
                        AiMsgWarning("Failed export attribute %s!", attrName.c_str());
                    }
                    
                    
                    
                } else {
                    AiMsgWarning("Export attribute (%s) is not exist!", attrName.c_str());
                    
                }
                
            }// mesh atribute
            
            
            
            
#pragma mark export point attribute
            
            
            
            for (uint i=0; i<pointAttrNameArray.length(); i++) {
                std::string attrName = pointAttrNameArray[i].asChar();
                
                // Owners may omit a field. Select the first present schema,
                // then let the shared export builder preserve every partition
                // and validate all present fields before renderer publication.
                DPAttributeData::ConstPtr attribute;
                for (auto owner = dMeshes->beginMeshes(); owner != dMeshes->endMeshes(); ++owner) {
                    attribute = (*owner)->getAttr(attrName);
                    if (attribute) break;
                }
                if (attribute) {
                    const DP_ATTR_DATA_TYPE type = attribute->getAttrType();
                    
                    if (type == DP_ATTR_INT) {
                        std::vector<int32_t> tmp;
                        if (!dommy::attribute::buildExportAttributeValues<int32_t, int32_t>(
                                dMeshes->beginMeshes(), dMeshes->endMeshes(), type, attrName, tmp) ||
                            tmp.size() != pointAttributeLayout.elementCount) {
                            AiMsgError("Invalid merged point attribute (%s).", attrName.c_str());
                            return;
                        }
                        
                        dommy::arnold::ArrayOwner outArray(AiArrayAllocate(
                            pointAttributeLayout.elementCount,
                            pointAttributeLayout.keyCount, AI_TYPE_INT));
						if (!outArray) {
							AiMsgError("Unable to allocate an Arnold integer attribute.");
							return;
						}
                        
                        for (std::size_t point = 0U; point < tmp.size(); ++point) {
                            dommy::arnold::requireArrayWrite(AiArraySetInt(outArray.get(), point, tmp[point]));
                        }
                        
                        dommy::arnold::requireUserParameter(node, attrName.c_str(), "varying INT");
                        AiNodeSetArray(
							node, attrName.c_str(), outArray.release());
                        
                    } else if (type == DP_ATTR_FLOAT || type == DP_ATTR_DOUBLE) {
                        std::vector<float> tmp;
                        const bool valid = type == DP_ATTR_FLOAT
                            ? dommy::attribute::buildExportAttributeValues<float, float>(
                                dMeshes->beginMeshes(), dMeshes->endMeshes(), type, attrName, tmp)
                            : dommy::attribute::buildExportAttributeValues<double, float>(
                                dMeshes->beginMeshes(), dMeshes->endMeshes(), type, attrName, tmp);
                        if (!valid || tmp.size() != pointAttributeLayout.elementCount) {
                            AiMsgError("Invalid merged point attribute (%s).", attrName.c_str());
                            return;
                        }
                        
                        dommy::arnold::ArrayOwner outArray(AiArrayAllocate(
                            pointAttributeLayout.elementCount,
                            pointAttributeLayout.keyCount, AI_TYPE_FLOAT));
						if (!outArray) {
							AiMsgError("Unable to allocate an Arnold float attribute.");
							return;
						}
                        
                        for (std::size_t point = 0U; point < tmp.size(); ++point) {
                            dommy::arnold::requireArrayWrite(AiArraySetFlt(outArray.get(), point, tmp[point]));
                        }
                        
                        dommy::arnold::requireUserParameter(node, attrName.c_str(), "varying FLOAT");
                        AiNodeSetArray(
							node, attrName.c_str(), outArray.release());
                        
                    } else if (type == DP_ATTR_VEC2I || type == DP_ATTR_VEC2F || type == DP_ATTR_VEC2D) {
                        std::vector<Imath::V2f> tmp;
                        const bool valid = type == DP_ATTR_VEC2I
                            ? dommy::attribute::buildExportAttributeValues<Imath::V2i, Imath::V2f>(
                                dMeshes->beginMeshes(), dMeshes->endMeshes(), type, attrName, tmp)
                            : type == DP_ATTR_VEC2F
                            ? dommy::attribute::buildExportAttributeValues<Imath::V2f, Imath::V2f>(
                                dMeshes->beginMeshes(), dMeshes->endMeshes(), type, attrName, tmp)
                            : dommy::attribute::buildExportAttributeValues<Imath::V2d, Imath::V2f>(
                                dMeshes->beginMeshes(), dMeshes->endMeshes(), type, attrName, tmp);
                        if (!valid || tmp.size() != pointAttributeLayout.elementCount) {
                            AiMsgError("Invalid merged point attribute (%s).", attrName.c_str());
                            return;
                        }
                        
                        dommy::arnold::ArrayOwner outArray(AiArrayAllocate(
                            pointAttributeLayout.elementCount,
                            pointAttributeLayout.keyCount, AI_TYPE_VECTOR2));
						if (!outArray) {
							AiMsgError("Unable to allocate an Arnold vector2 attribute.");
							return;
						}
                        
                        for (std::size_t point = 0U; point < tmp.size(); ++point) {
                            const AtVector2 value = {tmp[point][0], tmp[point][1]};
                            dommy::arnold::requireArrayWrite(AiArraySetVec2(outArray.get(), point, value));
                        }
                        
                        dommy::arnold::requireUserParameter(node, attrName.c_str(), "varying POINT2");
                        AiNodeSetArray(
							node, attrName.c_str(), outArray.release());
                        
                    } else if (type == DP_ATTR_VEC3I || type == DP_ATTR_VEC3F || type == DP_ATTR_VEC3D) {
                        std::vector<Imath::V3f> tmp;
                        const bool valid = type == DP_ATTR_VEC3I
                            ? dommy::attribute::buildExportAttributeValues<Imath::V3i, Imath::V3f>(
                                dMeshes->beginMeshes(), dMeshes->endMeshes(), type, attrName, tmp)
                            : type == DP_ATTR_VEC3F
                            ? dommy::attribute::buildExportAttributeValues<Imath::V3f, Imath::V3f>(
                                dMeshes->beginMeshes(), dMeshes->endMeshes(), type, attrName, tmp)
                            : dommy::attribute::buildExportAttributeValues<Imath::V3d, Imath::V3f>(
                                dMeshes->beginMeshes(), dMeshes->endMeshes(), type, attrName, tmp);
                        if (!valid || tmp.size() != pointAttributeLayout.elementCount) {
                            AiMsgError("Invalid merged point attribute (%s).", attrName.c_str());
                            return;
                        }
                        
                        dommy::arnold::ArrayOwner outArray(AiArrayAllocate(
                            pointAttributeLayout.elementCount,
                            pointAttributeLayout.keyCount, AI_TYPE_VECTOR));
						if (!outArray) {
							AiMsgError("Unable to allocate an Arnold vector attribute.");
							return;
						}
                        
                        for (std::size_t point = 0U; point < tmp.size(); ++point) {
                            const AtVector value = {tmp[point][0], tmp[point][1], tmp[point][2]};
                            dommy::arnold::requireArrayWrite(AiArraySetVec(outArray.get(), point, value));
                        }
                        
                        dommy::arnold::requireUserParameter(node, attrName.c_str(), "varying VECTOR");
                        AiNodeSetArray(
							node, attrName.c_str(), outArray.release());
                        
                    } else {
                        AiMsgWarning("Not Support point attribute (%s)!",attrName.c_str());
                        continue;
                    }
                } else {
                    AiMsgWarning("Not Exist point attribute (%s)!",attrName.c_str());
                    continue;
                }
                
            }
            
            
			
			
#pragma mark Group Matte
			
			//Export Group Matte
			//Support group type is Primitive and Face
			
            std::vector<GroupMatteRow> matteRows;
            const MStatus matteStatus = readGroupMatteRows(
                FindMayaPlug("GroupMatte"), totalFaceCount, dMeshes->numberOfMesh(), matteRows);
            if (!matteStatus) {
                AiMsgError("Unable to read the Arnold Group Matte table or its budget was exceeded.");
                AiNodeSetByte(node, visParam, 0);
                return;
            }
            for (const GroupMatteRow& row : matteRows) {
                const std::string& grpMatteName = row.name;
                const MStringArray& grpListNameArray = *row.groups;
                const std::size_t maxGrpCount = grpListNameArray.length();
                std::vector<float> redIds(totalFaceCount, 0.0f);
                std::vector<float> greenIds(totalFaceCount, 0.0f);
                std::vector<float> blueIds(totalFaceCount, 0.0f);
                bool isAllExist = true;
                for (std::size_t i = 0U; i < maxGrpCount; ++i) {
                    std::vector<float>* colIds = i == 0U ? &redIds : i == 1U ? &greenIds : &blueIds;
                    if (dMeshes->isExistGrp(grpListNameArray[i].asChar())) {
                        const DPGroupData::ConstPtr grpdata = dMeshes->getGrp(grpListNameArray[i].asChar());
                        const DPIdList idsList = grpdata->getGroupIds();
                        std::size_t meshoffset = 0U, faceoffset = 0U;
                        for (auto mIt = dMeshes->beginMeshes(); mIt != dMeshes->endMeshes(); ++mIt) {
                            if (meshoffset >= idsList.size() ||
                                faceoffset + (*mIt)->numFace() > colIds->size()) {
                                isAllExist = false;
                                break;
                            }
                            if (idsList[meshoffset]) {
                                for (std::size_t f = 0U; f < (*mIt)->numFace(); ++f)
                                    (*colIds)[faceoffset++] = 1.0f;
                            } else {
                                faceoffset += (*mIt)->numFace();
                            }
                            ++meshoffset;
                        }
                    } else if (dMeshes->isExistCompGrp(D_COMP_FACE, grpListNameArray[i].asChar())) {
                        std::size_t meshOffset = 0U;
                        for (auto mesh = dMeshes->beginMeshes(); mesh != dMeshes->endMeshes(); ++mesh) {
                            const std::size_t faces = (*mesh)->numFace();
                            if (meshOffset > colIds->size() || faces > colIds->size() - meshOffset) {
                                isAllExist = false;
                                break;
                            }
                            if ((*mesh)->isExistCompGrp(D_COMP_FACE, grpListNameArray[i].asChar())) {
                                const DPGroupData::ConstPtr group =
                                    (*mesh)->getCompGrp(D_COMP_FACE, grpListNameArray[i].asChar());
                                if (!group || group->getGroupIds().size() != faces) {
                                    isAllExist = false;
                                    break;
                                }
                                const DPIdList& ids = group->getGroupIds();
                                for (std::size_t face = 0U; face < ids.size(); ++face) {
                                    // A false bit is a valid unselected face, not an error.
                                    (*colIds)[meshOffset + face] = ids[face] ? 1.0f : 0.0f;
                                }
                            }
                            meshOffset += faces;
                        }
                    } else {
                        isAllExist = false;
                    }
                }
                if (isAllExist) {
                    dommy::arnold::ArrayOwner outArray(AiArrayAllocate(
                        faceLayout.elementCount, faceLayout.keyCount, AI_TYPE_VECTOR));
                    if (!outArray) {
                        AiMsgError("Unable to allocate the Arnold group matte array.");
                        return;
                    }
                    for (std::size_t face = 0U; face < totalFaceCount; ++face) {
                        AtVector value;
                        value.x = redIds[face];value.y = greenIds[face];value.z = blueIds[face];
                        dommy::arnold::requireArrayWrite(AiArraySetVec(outArray.get(), face, value));
                    }
                    dommy::arnold::requireUserParameter(node, grpMatteName.c_str(), "uniform VECTOR");
                    AiNodeSetArray(node, grpMatteName.c_str(), outArray.release());
                } else {
                    AiMsgWarning("Not exist group for Group Matte %s", grpMatteName.c_str());
                }
            }

#pragma mark Crease Edge
			
			MStringArray creaseEdgeGroupArray;
			
            bool additionalCreaseGroups = false;
            const MStatus creaseNamesStatus = splitRenderGroupNames(
                creaseEdgeGroupNames, 1U, creaseEdgeGroupArray, additionalCreaseGroups);
            if (!creaseNamesStatus) {
                AiMsgError("Unable to read the Arnold crease group name.");
                AiNodeSetByte(node, visParam, 0);
                return;
            }

			if(creaseEdgeGroupArray.length()>0) {
				
				if (additionalCreaseGroups) {
					AiMsgWarning("Crease edge group not support multi edge group. Please merge edge group.");
				}
				
				
                DPIndexList edgeVtxIdList;
                const MStatus creaseStatus = collectCreaseIndices(
                    *dMeshes, creaseEdgeGroupArray[0], edgeVtxIdList);
                if (!creaseStatus) {
                    AiMsgError("Unable to build the Arnold crease indices or their budget was exceeded.");
                    AiNodeSetByte(node, visParam, 0);
                    return;
                }

				//output arnold
				if(edgeVtxIdList.size()) {
					dommy::arnold::ArrayLayout creaseLayout;
					if (!dommy::arnold::checkedArrayLayout(
							edgeVtxIdList.size(), 1U, creaseLayout)) {
						AiMsgError("Arnold crease array exceeds the allocation budget.");
						AiNodeSetByte(node, visParam, 0);
						return;
					}
					dommy::arnold::ArrayOwner creaseEdgeIdArray(AiArrayAllocate(
						creaseLayout.elementCount, creaseLayout.keyCount,
						AI_TYPE_UINT));
					if (!creaseEdgeIdArray) {
						AiMsgError("Unable to allocate the Arnold crease array.");
						AiNodeSetByte(node, visParam, 0);
						return;
					}
					
					for (size_t e=0; e<edgeVtxIdList.size(); ++e) {
						dommy::arnold::requireArrayWrite(AiArraySetUInt(
							creaseEdgeIdArray.get(), e, edgeVtxIdList[e]));
					}
					
					static const AtString creaseIdxsParam("crease_idxs");
					
					AiNodeSetArray(
						node, creaseIdxsParam, creaseEdgeIdArray.release());
				}
				
				
				
			}//crease edge
			
			
			
			
			
			
#pragma mark shader and render flag
			
			MPlug MeshShaderPlug = FindMayaPlug("MeshShader");
			MPlug dispShaderPlug = FindMayaPlug("DisplacementShader");
			MPlug shPlug, dshPlug;
			if (!plugs.source(MeshShaderPlug, "MeshShader", shPlug) ||
			    !plugs.source(dispShaderPlug, "DisplacementShader", dshPlug)) {
				return;
			}
			AtNode* const shaderNode =
				shPlug.isNull() ? nullptr : ExportConnectedNode(shPlug);
			if (!shPlug.isNull() && !shaderNode) {
				AiMsgError("[DMeshTranslatorAi] Unable to export the connected shader.");
				return;
			}
			AtNode* const displacementNode =
				dshPlug.isNull() ? nullptr : ExportConnectedNode(dshPlug);
			if (!dshPlug.isNull() && !displacementNode) {
				AiMsgError("[DMeshTranslatorAi] Unable to export the connected displacement.");
				return;
			}
			// Admit both material bindings before replacing either pointer.
			if (shaderNode) {
				static const AtString shParam("shader");
				AiNodeSetPtr(node, shParam, shaderNode);
			} else {
				AiNodeResetParameter(node, "shader");
			}
			if (displacementNode) {
				static const AtString dispParam("disp_map");
				AiNodeSetPtr(node, dispParam, displacementNode);
			} else {
				AiNodeResetParameter(node, "disp_map");
			}
			
			
			
			
			ProcessRenderFlags(node);
			ExportLightLinking(node);
			
			
			
			
			
		}//export main
		
		
		updateVisibility.complete();
	}//plug check
			
}
DOMMY_ARNOLD_TRANSLATOR_CATCH("DMeshTranslatorAi::Update", node)

bool DMeshTranslatorAi::RequiresMotionData() try
{
	return IsMotionBlurEnabled( MTOA_MBLUR_DEFORM ) && IsLocalMotionBlurEnabled();
}
catch (...) {
    ::dommy::arnold::reportTranslatorException(
        "DMeshTranslatorAi::RequiresMotionData", nullptr, std::current_exception());
    return false;
}



void DMeshTranslatorAi::ExportMotion(AtNode* node) try {
	

	
	
	unsigned int motionstep = GetMotionStep();
	unsigned int numMotionStep = GetNumMotionSteps();
	dommy::arnold::PlugReader plugs(
		[this](const char* name) -> MPlug { return FindMayaPlug(name); },
		"DMeshTranslatorAi::ExportMotion");
	//    motionstep++;
	
	
	
	if( !IsMotionBlurEnabled())
	{
		return;
	}
	
	
    bool isRender = false;
	if (!plugs.read("IsRender", isRender)) {
		return;
	}
    
    static const AtString visParam("visibility");
    
    MPlug dmPlug;
    bool inputConnected = false;
    if (isRender) {
        dmPlug = FindMayaPlug("inDM");
        if (!plugs.connection(dmPlug, "inDM", inputConnected)) return;
    }
    ExportMatrix(node);
    AiNodeSetByte(node, visParam, isRender);
	if (isRender && inputConnected) {
		
		dommy::arnold::UpdateVisibilityGuard updateVisibility(node);
		dommy::maya::ScopedPlugDataHandle dMeshHndOwner;
		MDataHandle& dMeshHnd = dMeshHndOwner.handle();
		if (dMeshHndOwner.read(dmPlug) != MS::kSuccess) {
			AiMsgError("[dommyTool] Unable to read Arnold DMesh motion input.");
			return;
		}
		const DMeshData* const dMeshes(getInputDMfromHandle(dMeshHnd));
		if (!dMeshes) {
		    AiMsgError("[dommyTool] Arnold DMesh input has invalid type or topology.");
		    return;
		}
        
        
		if (dMeshes && dMeshes->numberOfMesh()) {
			
			if (!dMeshes->hasValidMeshTopology()) {
				AiMsgError("Arnold mesh motion export rejected invalid DMesh data.");
				AiNodeSetByte(node, visParam, 0);
				return;
			}
			size_t totalVertexCount = 0U;
			dommy::arnold::ArrayLayout pointLayout;
			if (!dMeshes->getCheckedTotalVertex(totalVertexCount) ||
				!dommy::arnold::checkedArrayLayout(
					totalVertexCount, 1U, pointLayout)) {
				AiMsgError(
					"Arnold mesh motion array exceeds the allocation budget.");
				AiNodeSetByte(node, visParam, 0);
				return;
			}
			dommy::arnold::ArrayOwner pointarray(AiArrayAllocate(
				pointLayout.elementCount, pointLayout.keyCount, AI_TYPE_VECTOR));
			if (!pointarray) {
				AiMsgError("Unable to allocate the Arnold mesh motion array.");
				return;
			}
			size_t pointOffset=0;
			
			
			//generate vertex motion
			
			for (auto mIt=dMeshes->beginMeshes(); mIt!=dMeshes->endMeshes(); ++mIt) {
                
                DPointListPtr pts;
                MMatrix mat;
                if (!(*mIt)->isIdentity()) {
                    pts.reset(new DPointList((*mIt)->getPointsInWorld()));
                } else {
                    pts.reset(new DPointList(
                        *(*mIt)->getPositionPtrConst()));
                }
				
				//addd vertex
                for(auto &pIt : *pts) {
					
					AtVector aVec;
					aVec.x = pIt[0];
					aVec.y = pIt[1];
					aVec.z = pIt[2];
					
					dommy::arnold::requireArrayWrite(AiArraySetVec(pointarray.get(), pointOffset++, aVec));
				}
				
			}
			
			
			static const AtString vlistParam("vlist");
			
			AtArray* const destinationPoints = AiNodeGetArray(node, vlistParam);
			if (!dommy::arnold::setCheckedVectorMotionKey(
			        destinationPoints, pointarray.get(), motionstep, numMotionStep)) {
				AiMsgError("[DMeshTranslatorAi] Incompatible motion arrays or failed key write.");
				return;
			}
			
			
		}//export main
		
		updateVisibility.complete();
	}//plug check
	
}
DOMMY_ARNOLD_TRANSLATOR_CATCH("DMeshTranslatorAi::ExportMotion", node)


AtNode* DMeshTranslatorAi::CreateArnoldNodes() try
{
	return AddArnoldNode("polymesh");
}
catch (...) {
    ::dommy::arnold::reportTranslatorException(
        "DMeshTranslatorAi::CreateArnoldNodes", nullptr, std::current_exception());
    return nullptr;
}

void* DMeshTranslatorAi::creator()
{
    return ::dommy::arnold::createObject("DMeshTranslatorAi", [&]() {
        return new DMeshTranslatorAi();
    });
}
