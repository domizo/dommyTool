//
//  DVDBTranslatorAi.cpp
//  dommyTool
//
//  Created by yuzo on 3/26/17.
//
//

#include "DVDBTranslatorAi.h"
#include "common/utility/DMayaPlugDataHandle.h"
#include "common/utility/DMayaNameList.h"

#include <ai_msg.h>
#include <ai_nodes.h>

#include <openvdb/io/Stream.h>
#include <openvdb/math/Math.h>

#include "DMayaHeaders.h"

#include "DNodeOperation.h"
#include "DOpenVDBData.h"
#include "DOpenVDBCommon.h"
#include "DOpenVDBUtil.h"

#include "DOpenVDBDisplayVP2.h"
#include "../Common/DOpenVDBLimits.h"
#include "common/file/DAtomicFile.h"
#include "common/file/DFileSize.h"
#include "integrations/arnold/DArnoldArrayBudget.h"
#include "integrations/arnold/DArnoldEntryPointGuard.h"
#include "integrations/arnold/DArnoldMayaPlug.h"
#include "integrations/arnold/DArnoldResources.h"
#include "integrations/arnold/DArnoldUserParameters.h"

#include <cmath>
#include <cstdio>
#include <stdexcept>

namespace {

std::string makeVdbCacheBase(
	const MString& temporaryDirectory, const MString& nodeName)
{
	std::string base = temporaryDirectory.asChar();
	if (base.empty()) {
		throw std::runtime_error("Maya returned an empty temporary directory");
	}
	const char last = base[base.size() - 1U];
	if (last != '/' && last != '\\') base += '/';
	base += "dommytool-vdb-";
	base += dommy::file::sanitizeFileNameComponent(nodeName.asChar());
	return base;
}

std::string writeVdbCache(
	const std::string& basePath, const openvdb::GridPtrSet& grids)
{
	// openvdb::io::File reopens a pathname. Maya's cache directory and its
	// ancestors must be trusted; exclusive reservation alone does not establish
	// protection against writable-directory substitution (including ACLs).
	const std::string path =
		dommy::file::createTemporarySiblingFileWithSuffix(basePath, ".vdb");
	try {
		openvdb::io::File file(path);
		file.setCompression(openvdb::io::COMPRESS_ZIP);
		file.write(grids);
		file.close();
		if (!dommy::file::fileSizeWithinLimit(
				path, static_cast<unsigned long long>(
					dommy::vdb::kMaxVdbPayloadBytes))) {
			throw std::length_error(
				"Arnold VDB cache exceeds the file-size limit");
		}
		return path;
	} catch (...) {
		std::remove(path.c_str());
		throw;
	}
}

} // namespace

DVDBTranslatorAi::~DVDBTranslatorAi()
{
	if (!m_cacheFilePath.empty()) {
		std::remove(m_cacheFilePath.c_str());
	}
}


void DVDBTranslatorAi::Export(AtNode* node) try
{
	AiMsgDebug("[dommyTool] Exporting %s", GetMayaNodeName().asChar());
	Update(node);
}
DOMMY_ARNOLD_TRANSLATOR_CATCH("DVDBTranslatorAi::Export", node)

void DVDBTranslatorAi::NodeChanged(MObject& node, MPlug& plug) try
{
    MStatus status;
    MFnDependencyNode nodeFn(node, &status);
    if (!status) {
        AiMsgError("[DVDBTranslatorAi] Unable to inspect the changed Maya node.");
        return;
    }
    const MString pName = plug.partialName(
        false, false, false, false, false, true, &status);
    if (!status) {
        AiMsgError("[DVDBTranslatorAi] Unable to read the changed Maya plug name.");
        return;
    }
	if(pName == "VolumeMode" || pName == "VDBInput" ||
	   pName == "aiOpaque" || pName == "aiMatte" || pName == "aiSelfShadows" || pName == "aiReceiveShadows" ||
       pName == "Sidedness" || pName == "InvertNormals" || pName == "RayBias" ||
	   pName == "StepSize" || pName == "StepScale" ||pName == "VolumePadding" ||pName == "IsCompress" ||
	   pName == "ExportGrid" ||
	   pName == "ExportVelocityGrid" || pName == "VelocityScale" || pName == "VelocityFps" ||
       pName == "VelocityShutterStart" || pName == "VelocityShutterEnd" || pName == "VelocityThreshold" ||
       pName == "MinBox" || pName == "MaxBox" || pName == "FieldChannel" || pName == "Samples" || pName == "Solver" || pName == "Solver" ||pName == "Isovalue") {
		 
        
		SetUpdateMode(CNodeTranslator::AI_RECREATE_TRANSLATOR);
		
		RequestUpdate();
	}
}
DOMMY_ARNOLD_TRANSLATOR_CATCH("DVDBTranslatorAi::NodeChanged", nullptr)


void DVDBTranslatorAi::Update(AtNode* node) try
{
	dommy::arnold::PlugReader plugs(
		[this](const char* name) -> MPlug { return FindMayaPlug(name); },
		"DVDBTranslatorAi");
	
	
	
	
	MPlug vdbPlug = FindMayaPlug("VDBInput");
    bool inputConnected = false;
    if (!plugs.connection(vdbPlug, "VDBInput", inputConnected)) return;
    ExportMatrix(node);
    AiNodeSetByte(node, "visibility", ComputeVisibility());
	if (inputConnected) {
		
		dommy::arnold::UpdateVisibilityGuard updateVisibility(node);
		dommy::maya::ScopedPlugDataHandle vdbHndOwner;
		MDataHandle& vdbHnd = vdbHndOwner.handle();
		if (vdbHndOwner.read(vdbPlug) != MS::kSuccess) {
			AiMsgError("[dommyTool] Unable to read Arnold OpenVDB input.");
			return;
		}
        const DOpenVDBData* const vdb(getInputVDBfromHandle(vdbHnd));
        if (!vdb) {
            AiMsgError("[dommyTool] Arnold OpenVDB input has invalid type or payload.");
            return;
        }
        
			if (vdb) {
				MString additionalGridsString;
				MString velocityGridsString;
				float velocityScale = 0.0f;
				float velocityFps = 0.0f;
				float velocityShutterStart = 0.0f;
				float velocityShutterEnd = 0.0f;
				float velocityThreshold = 0.0f;
				bool selfShadows = false;
				bool receiveShadows = false;
				float stepSize = 0.0f;
				float stepScale = 0.0f;
				bool compress = false;
				std::string fieldChannel;
				short volumeMode = 0;
				float isoValue = 0.0f;
				int samples = 0;
				short solver = 0;
				float volumePadding = 0.0f;
				if (!plugs.read("ExportGrid", additionalGridsString) ||
					!plugs.read(
						"ExportVelocityGrid", velocityGridsString) ||
					!plugs.read("VelocityScale", velocityScale) ||
					!plugs.read("VelocityFps", velocityFps) ||
					!plugs.read(
						"VelocityShutterStart", velocityShutterStart) ||
					!plugs.read(
						"VelocityShutterEnd", velocityShutterEnd) ||
					!plugs.read("VelocityThreshold", velocityThreshold) ||
					!plugs.read("aiSelfShadows", selfShadows) ||
					!plugs.read("aiReceiveShadows", receiveShadows) ||
					!plugs.read("StepSize", stepSize) ||
					!plugs.read("StepScale", stepScale) ||
					!plugs.read("IsCompress", compress) ||
					!plugs.readString("FieldChannel", fieldChannel) ||
					!plugs.read("VolumeMode", volumeMode) ||
					!plugs.read("Isovalue", isoValue) ||
					!plugs.read("Samples", samples) ||
					!plugs.read("Solver", solver) ||
					!plugs.read("VolumePadding", volumePadding) ||
					!std::isfinite(velocityScale) ||
					!std::isfinite(velocityFps) ||
					!std::isfinite(velocityShutterStart) ||
					!std::isfinite(velocityShutterEnd) ||
					!std::isfinite(velocityThreshold) ||
					!std::isfinite(stepSize) || !std::isfinite(stepScale) ||
					!std::isfinite(isoValue) ||
					!std::isfinite(volumePadding) ||
					velocityFps <= 0.0f || stepSize < 0.0f ||
					stepScale < 0.0f || volumePadding < 0.0f ||
					samples < 0 || volumeMode < 0 || volumeMode > 1 ||
					solver < 0 || solver > 1) {
					AiMsgError(
						"[dommyTool] Arnold VDB parameters are invalid.");
					return;
				}
				
				dommy::maya::NameListBudget nameListBudget;
				MStringArray additional_grids;
				MStringArray velocity_grids;
				if (dommy::maya::splitCheckedNameList(
				        additionalGridsString, 1U, nameListBudget, additional_grids) != MS::kSuccess) {
					AiMsgError("[DVDBTranslatorAi] Unable to parse the exported grid list.");
					return;
				}
				if (dommy::maya::splitCheckedNameList(
				        velocityGridsString, 1U, nameListBudget, velocity_grids) != MS::kSuccess) {
					AiMsgError("[DVDBTranslatorAi] Unable to parse the velocity-grid list.");
					return;
				}

				MFnDependencyNode vdbNode(GetMayaObject());
			
			////////////////////////////////////////////////////////////////////////////////////////////////
			
			//set out grids
			openvdb::GridPtrSet outGrids;
			GridCPtrVec grids;
			getGrids(grids, *vdb, "*");
			
			for (GridCPtrVecIter it = grids.begin(); it != grids.end(); ++it) {
				GridPtr grid = openvdb::ConstPtrCast<Grid>((*it)->copyGrid());
				grid->removeMeta("is_vdb");
				outGrids.insert(grid);
			}
			
			if (outGrids.empty()) {
				AiMsgWarning("Grid is empty! : (%s)", vdbNode.name().asChar());
			}
			
//			reportFloatPrecisionConflicts(conflicts);
			
			
			MString temporaryDirectory;
			const MStatus temporaryStatus = MGlobal::executeCommand(
				"internalVar -userTmpDir", temporaryDirectory, false, false);
			if (temporaryStatus != MS::kSuccess) {
				AiMsgError("Unable to query Maya's user temporary directory.");
				return;
			}
			std::string cacheFilePath;
			try {
				cacheFilePath = writeVdbCache(
					makeVdbCacheBase(temporaryDirectory, vdbNode.name()), outGrids);
			} catch (const std::exception& error) {
				AiMsgError("Unable to write Arnold VDB cache: %s", error.what());
				return;
			} catch (...) {
				AiMsgError("Unable to write Arnold VDB cache.");
				return;
			}
			
			////////////////////////////////////////////////////////////////////////////////////////////////
			
			
				//set file
				AiNodeSetStr(node, "filename", cacheFilePath.c_str());
			if (!m_cacheFilePath.empty() && m_cacheFilePath != cacheFilePath) {
				std::remove(m_cacheFilePath.c_str());
			}
			m_cacheFilePath.swap(cacheFilePath);
			
			
	            
	            //check output grid
			std::set<std::string> out_grids;
            
            const unsigned int additional_grids_count = additional_grids.length();
            for (unsigned int i = 0; i < additional_grids_count; ++i) {
                const MString additional_grid = additional_grids[i];
                if (additional_grid.length()) {
                    out_grids.insert(additional_grid.asChar());
                }
            }
            
            
            
            dommy::arnold::ArrayLayout gridNameLayout;
            if (!dommy::arnold::checkedArrayLayout(
                    out_grids.size(), 1U, gridNameLayout)) {
                AiMsgError("Arnold VDB grid-name array exceeds the allocation budget.");
                return;
            }
            dommy::arnold::ArrayOwner gridNames(AiArrayAllocate(
                gridNameLayout.elementCount, gridNameLayout.keyCount,
                AI_TYPE_STRING));
            if (!gridNames) {
                AiMsgError("Arnold could not allocate the VDB grid-name array.");
                return;
            }
            
            unsigned int id = 0;
            for (const auto& out_grid : out_grids) {
                dommy::arnold::requireArrayWrite(AiArraySetStr(gridNames.get(), id, out_grid.c_str()));
                ++id;
            }
            
	            AiNodeSetArray(node, "grids", gridNames.release());
            
            
            const unsigned int velocity_grids_count = velocity_grids.length();
            if (velocity_grids_count > 0) {
                dommy::arnold::requireVelocityGridsParameter(node);
                dommy::arnold::ArrayLayout velocityLayout;
                if (!dommy::arnold::checkedArrayLayout(
                        velocity_grids_count, 1U, velocityLayout)) {
                    AiMsgError(
                        "Arnold velocity-grid array exceeds the allocation budget.");
                    return;
                }
                dommy::arnold::ArrayOwner velocityGridNames(AiArrayAllocate(
                    velocityLayout.elementCount, velocityLayout.keyCount,
                    AI_TYPE_STRING));
                if (!velocityGridNames) {
                    AiMsgError(
                        "Arnold could not allocate the velocity-grid array.");
                    return;
                }
                for (unsigned int i = 0; i < velocity_grids_count; ++i)
                    dommy::arnold::requireArrayWrite(AiArraySetStr(
                        velocityGridNames.get(), i, velocity_grids[i].asChar()));
                AiNodeSetArray(
                    node, "velocity_grids", velocityGridNames.release());
                
	                AiNodeSetFlt(node, "velocity_scale", velocityScale);
	                
	                AiNodeSetFlt(node, "velocity_fps", velocityFps);
	                
	                AiNodeSetFlt(
						node, "velocity_shutter_start", velocityShutterStart);
	                
	                AiNodeSetFlt(
						node, "velocity_shutter_end", velocityShutterEnd);
	                
	                AiNodeSetFlt(
						node, "velocity_outlier_threshold", velocityThreshold);
            } else {
                // Only a successfully admitted empty list disables velocity.
                // Reset these known controls, not unrelated node/user state.
                AiNodeResetParameter(node, "velocity_grids");
                AiNodeResetParameter(node, "velocity_scale");
                AiNodeResetParameter(node, "velocity_fps");
                AiNodeResetParameter(node, "velocity_shutter_start");
                AiNodeResetParameter(node, "velocity_shutter_end");
                AiNodeResetParameter(node, "velocity_outlier_threshold");
            }
            
            
            
			
			
#pragma mark option
            
            static const AtString selfShadowParam("self_shadows");
            AiNodeSetBool(node, selfShadowParam, selfShadows);
            static const AtString receiveShadowParam("receive_shadows");
            AiNodeSetBool(node, receiveShadowParam, receiveShadows);
            
	            static const AtString stepSizeParam("step_size");
            AiNodeSetFlt(node, stepSizeParam, stepSize);
            static const AtString stepScaleParam("step_scale");
            AiNodeSetFlt(node, stepScaleParam, stepScale);
            
            
            
            static const AtString compressParam("compress");
            AiNodeSetBool(node, compressParam, compress);
            
            static const AtString volPaddingParam("volume_padding");
            
            static const AtString thresholdParam("threshold");
            static const AtString samplesParam("samples");
            static const AtString field_channelParam("field_channel");
            static const AtString solverParam("solver");
     //                static const AtString fieldParam("field");
            
            
            if(volumeMode==1) {//implicit
                
                
                
                AiNodeSetFlt(node, thresholdParam, isoValue);
                AiNodeSetUInt(
					node, samplesParam, static_cast<unsigned int>(samples));
                
//                ProcessParameter(node, "min", AI_TYPE_VECTOR, "MinBox");
//                ProcessParameter(node, "max", AI_TYPE_VECTOR, "MaxBox");
                
                AiNodeSetStr(node, field_channelParam, fieldChannel.c_str());
                

                
//                ProcessParameter(node, "solver", AI_TYPE_ENUM, "Solver");
                
                if (solver==0) {
                    AiNodeSetStr(node, solverParam, "uniform");
                } else {
                    AiNodeSetStr(node, solverParam, "levelset");
                }
                
                
            } else {
                
                AiNodeSetFlt(node, volPaddingParam, volumePadding);
                
                
                
            }
#pragma mark shader and render flag
			
			MPlug VdbShaderPlug = FindMayaPlug("VdbShader");
			MPlug shPlug;
			if (!plugs.source(VdbShaderPlug, "VdbShader", shPlug)) return;
			
			if (!shPlug.isNull()) {
				AtNode* aiSgNode = ExportConnectedNode(shPlug);
				if (!aiSgNode) {
					AiMsgError("[DVDBTranslatorAi] Unable to export the connected shader.");
					return;
				}
                AiNodeSetPtr( node, "shader", aiSgNode);
			} else {
				AiNodeResetParameter(node, "shader");
			}
			
			ProcessRenderFlags(node);
			ExportLightLinking(node);
			
			
		}
		updateVisibility.complete();
	}
	
	
	
	
	
}
DOMMY_ARNOLD_TRANSLATOR_CATCH("DVDBTranslatorAi::Update", node)







AtNode* DVDBTranslatorAi::CreateArnoldNodes() try
{
	dommy::arnold::PlugReader plugs(
		[this](const char* name) -> MPlug { return FindMayaPlug(name); },
		"DVDBTranslatorAi::CreateArnoldNodes");
	short volumeMode = 0;
	if (!plugs.read("VolumeMode", volumeMode) ||
		volumeMode < 0 || volumeMode > 1) {
		AiMsgError("[dommyTool] Arnold VDB volume mode is invalid.");
		return nullptr;
	}
    if(volumeMode==0){
//        cout << "volume" << endl;
        return AddArnoldNode("volume");
    } else {
//        cout << "volume_implicit" << endl;
        return AddArnoldNode("volume_implicit");
    }
    
//    return AddArnoldNode("volume");
}
catch (...) {
    ::dommy::arnold::reportTranslatorException(
        "DVDBTranslatorAi::CreateArnoldNodes", nullptr, std::current_exception());
    return nullptr;
}

void* DVDBTranslatorAi::creator()
{
    return ::dommy::arnold::createObject("DVDBTranslatorAi", [&]() {
        return new DVDBTranslatorAi();
    });
}
