//
//  DPInstancer.cpp
//  dommyTool
//
//  Created by yuzo on 2017/06/24.
//

#include "DPInstancer.h"
#include "common/utility/DMayaEntryPointGuard.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DMayaPlugDataHandle.h"
#include "common/utility/DComputeGuard.h"


#include "DMayaHeaders.h"


#include "DPointDataHeaders.h"
#include "DNoiseOp.h"
#include "DBBoxOp.h"
#include "DDrawHelper.h"
#include "DLogConfig.h"
#include "DMayaDebug.h"
#include "common/utility/DCheckedSize.h"
#include "common/utility/DAttributeBlendMath.h"
#include "common/utility/DMayaPluginData.h"
#include "common/utility/DViewportBuffer.h"
#include "common/utility/DViewportRenderItem.h"
#include "common/utility/DViewportGeometryBudget.h"
#include "common/utility/DMayaMeshTraversal.h"
#include "common/utility/DPointTransform.h"
#include "common/utility/DMayaValueValidation.h"
#include "common/utility/DViewportCallbackGuard.h"
#include "common/file/DFileSize.h"
#include "common/file/DBoundedTextRead.h"
#include "common/file/DNumericTextRead.h"


#include "dommyMath.h"

#include <ai.h>
#include <ai_msg.h>
#include <ai_nodes.h>
#include <ai_universe.h>

#include <fstream>
#include <cmath>
#include <limits>
#include <locale>
#include <sstream>

namespace {

// Diagnostics must not invalidate an otherwise constructed override. The
// static labels require no fallible Maya array construction or unchecked enum
// indexing, and a missing stock shader simply has no parameters to report.
void reportInstancerShaderParameters(const MHWRender::MShaderInstance* shader) noexcept
{
#if DOMMY_ENABLE_DEBUG_LOGS
    if (!shader) return;
    try {
        const char* const typeNames[] = {
            "kInvalid", "kBoolean", "kInteger", "kFloat", "kFloat2", "kFloat3",
            "kFloat4", "kFloat4x4Row", "kFloat4x4Col", "kTexture1", "kTexture2",
            "kTexture3", "kTextureCube", "kSampler"
        };
        MStringArray parameters;
        shader->parameterList(parameters);
        for (unsigned int i = 0U; i < parameters.length(); ++i) {
            const int type = static_cast<int>(shader->parameterType(parameters[i]));
            const char* const typeName = type >= 0 &&
                static_cast<unsigned int>(type) < sizeof(typeNames) / sizeof(typeNames[0])
                ? typeNames[type] : "unknown";
            DOMMY_MAYA_DEBUG("DPInstancer", i << ":" << parameters[i].asChar());
            DOMMY_MAYA_DEBUG("DPInstancer", "\ttype:" << typeName);
            DOMMY_MAYA_DEBUG("DPInstancer", "\tisArray:" << shader->isArrayParameter(parameters[i]));
            DOMMY_MAYA_DEBUG("DPInstancer", "\tsemantic:" << shader->semantic(parameters[i]).asChar());
        }
    } catch (...) {
        dommy::maya::reportViewportCallbackException(
            "DPInstancerOverride", "shader diagnostics", std::current_exception());
    }
#else
    (void)shader;
#endif
}

const std::size_t kMaxAssTocLineBytes = 1024U * 1024U;
const unsigned long long kMaxAssTocFileBytes = 16U * 1024U * 1024U;
const std::size_t kMaxAssTocLineCount = 100000U;

bool validConnectionIndices(const MIntArray& connections, std::size_t pointCount) {
    for (unsigned int index = 0; index < connections.length(); ++index) {
        const int value = connections[index];
        if (value < 0 || static_cast<std::size_t>(value) >= pointCount) {
            return false;
        }
    }
    return true;
}

bool resizeInstanceBuffers(std::size_t basePointCount,
                           std::size_t baseIndexCount,
                           std::size_t instanceCount,
                           bool includeNormals,
                           std::vector<float>& points,
                           std::vector<unsigned int>& indices,
                           std::vector<float>& normals) {
    std::size_t totalPointCount = 0;
    std::size_t scalarCount = 0;
    std::size_t totalIndexCount = 0;
    if (!dommy::viewport::checkedInstanceLayout(basePointCount, baseIndexCount, instanceCount,
                                               totalPointCount, scalarCount, totalIndexCount) ||
        scalarCount > points.max_size() || totalIndexCount > indices.max_size() ||
        (includeNormals && scalarCount > normals.max_size())) {
        return false;
    }

    std::vector<float> candidatePoints(scalarCount);
    std::vector<unsigned int> candidateIndices(totalIndexCount);
    std::vector<float> candidateNormals(includeNormals ? scalarCount : 0U);
    points.swap(candidatePoints);
    indices.swap(candidateIndices);
    normals.swap(candidateNormals);
    return true;
}

bool buildInstanceBuffers(const MPointArray& sourcePoints, const MIntArray& connections,
                           const MFloatVectorArray* sourceNormals,
                           const std::vector<MMatrix>& matrices,
                           std::vector<float>& points, std::vector<unsigned int>& indices,
                           std::vector<float>& normals)
{
    if (sourcePoints.length() > dommy::viewport::kMaxViewportVertexCount ||
        connections.length() > dommy::viewport::kMaxViewportIndexCount ||
        !validConnectionIndices(connections, sourcePoints.length()) ||
        (sourceNormals && sourceNormals->length() != sourcePoints.length()) ||
        matrices.size() > dommy::generation::kMaxGeneratedTransformCount) return false;
    std::vector<float> candidatePoints, candidateNormals;
    std::vector<unsigned int> candidateIndices;
    if (!resizeInstanceBuffers(sourcePoints.length(), connections.length(), matrices.size(),
                                sourceNormals != NULL, candidatePoints, candidateIndices,
                                candidateNormals)) return false;
    std::size_t position = 0U, index = 0U, normal = 0U, pointOffset = 0U;
    for (const auto& matrix : matrices) {
        if (!dommy::numeric::isFiniteAffineTransform(matrix)) return false;
        for (unsigned int vertex = 0U; vertex < sourcePoints.length(); ++vertex) {
            std::array<float, 3> transformed;
            if (!dommy::numeric::checkedTransformPoint(sourcePoints[vertex], matrix, transformed))
                return false;
            for (float value : transformed) candidatePoints[position++] = value;
            if (sourceNormals) {
                const MFloatVector& source = (*sourceNormals)[vertex];
                if ((source[0] == 0 && source[1] == 0 && source[2] == 0) ||
                    !dommy::numeric::checkedTransformNormal(source, matrix, transformed))
                    return false;
                for (float value : transformed) candidateNormals[normal++] = value;
            }
        }
        for (unsigned int vertex = 0U; vertex < connections.length(); ++vertex)
            candidateIndices[index++] = static_cast<unsigned int>(
                pointOffset + static_cast<std::size_t>(connections[vertex]));
        pointOffset += sourcePoints.length();
    }
    points.swap(candidatePoints);
    indices.swap(candidateIndices);
    normals.swap(candidateNormals);
    return true;
}

MStatus readDataHandle(
    const MObject& node,
    const MObject& attribute,
    dommy::maya::ScopedPlugDataHandle& owner)
{
    const MPlug plug(node, attribute);
    if (plug.isNull()) {
        return MS::kFailure;
    }
    return owner.read(plug);
}

} // namespace



#define DP_INSTANCER_DEBUG DOMMY_ENABLE_DEBUG_LOGS





MTypeId DPInstancer::typeId( ID_DP_INSTANCER );
const MString DPInstancer::typeName("DPInstancer");
MString	DPInstancer::classification("drawdb/geometry/DPInstancer");
MString	DPInstancer::registrantId("DPInstancerPlugin");




MObject DPInstancer::inDP;

MObject DPInstancer::aCopyObjectType;
MObject DPInstancer::inMesh;

//Ass
MObject DPInstancer::aReadAssFile;
MObject DPInstancer::aAssPrefix;
MObject DPInstancer::aIsOverrideShader;
MObject DPInstancer::aIsVerboseNodeList;

//Ass Sequence
MObject DPInstancer::aTimeRange;
MObject DPInstancer::aTimeOffset;
MObject DPInstancer::aTimeAttribute;

//Display
MObject DPInstancer::aDisplayType;

//Edit

MObject DPInstancer::aAimVector;
MObject DPInstancer::aAxisVector;

MObject DPInstancer::aNormalVector;
MObject DPInstancer::aNormalAttribute;

MObject DPInstancer::aTangentVector;
MObject DPInstancer::aTangentAttribute;




MObject DPInstancer::aAnimationType;
MObject DPInstancer::aAnimMatrixAttribute;
MObject DPInstancer::aInAnimMatrix;

MObject DPInstancer::aLocalTranslate;
MObject DPInstancer::aGlobalTranslate;
MObject DPInstancer::aLocalRotate;
MObject DPInstancer::aGlobalRotate;
MObject DPInstancer::aGlobalScale;
MObject DPInstancer::aUniformScale;

MObject DPInstancer::aLocalTranslateAttribute;
MObject DPInstancer::aGlobalTranslateAttribute;
MObject DPInstancer::aLocalRotateAttribute;
MObject DPInstancer::aGlobalRotateAttribute;
MObject DPInstancer::aGlobalScaleAttribute;
MObject DPInstancer::aUniformScaleAttribute;



//random
MObject DPInstancer::aSeed;
MObject DPInstancer::aLocalTranslateRandomMin;
MObject DPInstancer::aLocalTranslateRandomMax;
MObject DPInstancer::aGlobalTranslateRandomMin;
MObject DPInstancer::aGlobalTranslateRandomMax;
MObject DPInstancer::aLocalRotateRandomMin;
MObject DPInstancer::aLocalRotateRandomMax;
MObject DPInstancer::aGlobalRotateRandomMin;
MObject DPInstancer::aGlobalRotateRandomMax;
MObject DPInstancer::aGlobalScaleRandomMin;
MObject DPInstancer::aGlobalScaleRandomMax;
MObject DPInstancer::aUniformScaleRandomMinMax;


//render attribute
MObject DPInstancer::aIsRender;

MObject DPInstancer::aiOpaque;
MObject DPInstancer::aiMatte;
MObject DPInstancer::aiSelfShadows;
MObject DPInstancer::aiReceiveShadows;

MObject DPInstancer::aSidedness;
MObject DPInstancer::aInvertNormals;

MObject DPInstancer::aExportAttribute;

//shader
MObject DPInstancer::aShader;

MObject DPInstancer::aTargetNodeNames;
MObject DPInstancer::aReplaceShader;
MObject DPInstancer::aReplaceShaderList;




void* DPInstancer::creator()
{
    return ::dommy::maya::createObject("DPInstancer", [&]() {
        return new DPInstancer();
    });
}


MStatus DPInstancer::compute(const MPlug& /*plug*/, MDataBlock& /*data*/) try
{
    // Geometry and rendering settings are consumed by the viewport override;
    // this locator implements no computed output of its own.
    return MS::kUnknownParameter;
}
DOMMY_COMPUTE_CATCH("DPInstancer")



bool DPInstancer::isBounded() const
{
	return true;
}


MBoundingBox DPInstancer::boundingBox() const try
{
	
	MPoint corner1( -0.5, -0.5, -0.5 );
	MPoint corner2( 0.5, 0.5, 0.5 );
	
	MPlug dpPlug = MPlug(thisMObject(), DPInstancer::inDP);
	
    MStatus connectionStatus;
    const bool connected = dpPlug.isConnected(&connectionStatus);
    if (!connectionStatus) {
        MGlobal::displayError("DPInstancer: unable to query input connection for bounds.");
    } else if (connected) {
		
        MObject obj;
        const DPData* dp =  getInputDPfromPlug(dpPlug, obj);
        
        if(dp) {
            MBoundingBox bbox = dp->getBoundingBox();
            
            return bbox;
        }
		}
	
	return MBoundingBox( corner1, corner2 );
}
DOMMY_VIEWPORT_BOUNDING_BOX_CATCH("DPInstancer")

void DPInstancer::draw( M3dView & view, const MDagPath & /*path*/,
						 M3dView::DisplayStyle style,
						 M3dView::DisplayStatus status )
{
	
}



MStatus DPInstancer::initialize()
try {
	MStatus stat;
	MFnNumericAttribute nAttr;
	MFnTypedAttribute tAttr;
	MFnGenericAttribute gAttr;
	MFnStringData fnStringData;
	
	inDP = tAttr.create("inDP", "idp", DPData::id, MObject::kNullObj, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(tAttr.setReadable(false));
	DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
	DOMMY_RETURN_IF_FAILURE(addAttribute(inDP));
	
	inMesh = gAttr.create( "inMesh", "inm" , &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(gAttr.addDataAccept( MFnData::kMesh ));
	DOMMY_RETURN_IF_FAILURE(gAttr.setStorable(false));
	DOMMY_RETURN_IF_FAILURE(gAttr.setReadable( false ));
	DOMMY_RETURN_IF_FAILURE(addAttribute(inMesh));
	
	MFnEnumAttribute typeEnumFn;
	aCopyObjectType = typeEnumFn.create( "CopyObjectType", "cot", DP_INST_COPY_MESH, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(typeEnumFn.addField( "Input Mesh", DP_INST_COPY_MESH ));
	DOMMY_RETURN_IF_FAILURE(typeEnumFn.addField( "From Ass File", DP_INST_COPY_ASS ));
	DOMMY_RETURN_IF_FAILURE(typeEnumFn.addField( "From Ass Sequence File", DP_INST_COPY_ASS_SEQUENCE ));
	DOMMY_RETURN_IF_FAILURE(typeEnumFn.setKeyable( true ));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aCopyObjectType));
	
	
	const MObject aReadAssFileDefaultData = fnStringData.create("", &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	aReadAssFile = tAttr.create("ReadAssFile", "rassf", MFnData::kString, aReadAssFileDefaultData, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aReadAssFile));
	
	const MObject aAssPrefixDefaultData = fnStringData.create("", &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	aAssPrefix = tAttr.create("AssPrefix", "asspre", MFnData::kString, aAssPrefixDefaultData, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aAssPrefix));
	
	aIsOverrideShader = nAttr.create( "IsOverrideShader", "iovsh", MFnNumericData::kBoolean , 0, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(false));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aIsOverrideShader));
	
	aIsVerboseNodeList = nAttr.create( "IsVerboseNodeList", "ivbon", MFnNumericData::kBoolean , 0, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(false));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aIsVerboseNodeList));
	
	
	
	//Ass Sequence
	aTimeRange = nAttr.create( "TimeRange", "tmrng", MFnNumericData::k2Int , 0, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1,10));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aTimeRange));
	
	aTimeOffset = nAttr.create( "TimeOffset", "tmoff", MFnNumericData::kInt , 0, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aTimeOffset));
	
	const MObject aTimeAttributeDefaultData = fnStringData.create("", &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	aTimeAttribute = tAttr.create("TimeAttribute", "tmattr", MFnData::kString, aTimeAttributeDefaultData, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aTimeAttribute));
	
	
	
	
#pragma mark display attribute
	
	MFnEnumAttribute enumFn;
	aDisplayType = enumFn.create( "DisplayType", "dspty", DP_INST_POINT, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(enumFn.addField( "None", DP_INST_NONE ));
	DOMMY_RETURN_IF_FAILURE(enumFn.addField( "Point", DP_INST_POINT ));
	DOMMY_RETURN_IF_FAILURE(enumFn.addField( "Bounding box", DP_INST_BBOX ));
	DOMMY_RETURN_IF_FAILURE(enumFn.addField( "WireFrame", DP_INST_WIRE ));
	DOMMY_RETURN_IF_FAILURE(enumFn.addField( "Smooth Shade", DP_INST_SHADE));
	DOMMY_RETURN_IF_FAILURE(enumFn.setKeyable( true ));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aDisplayType));
	
#pragma mark edit attribute
    
    aAimVector = nAttr.create( "AimVector", "aimvec", MFnNumericData::k3Double , 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0,1.0,0.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aAimVector));
    
    aAxisVector = nAttr.create( "AxisVector", "axsvec", MFnNumericData::k3Double , 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1.0,0.0,0.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aAxisVector));
    
    
	
	aNormalVector = nAttr.create( "NormalVector", "nvec", MFnNumericData::k3Double , 0, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0,1.0,0.0));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aNormalVector));
	
	const MObject aNormalAttributeDefaultData = fnStringData.create("N", &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	aNormalAttribute = tAttr.create("NormalAttribute", "nattr", MFnData::kString, aNormalAttributeDefaultData, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aNormalAttribute));
	
	aTangentVector = nAttr.create( "TangentVector", "tvec", MFnNumericData::k3Double , 0, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1.0,0.0,0.0));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aTangentVector));
	
	const MObject aTangentAttributeDefaultData = fnStringData.create("T", &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	aTangentAttribute = tAttr.create("TangentAttribute", "tattr", MFnData::kString, aTangentAttributeDefaultData, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aTangentAttribute));
	
#pragma mark animation attribute
	MFnEnumAttribute animEnumFn;
	aAnimationType = animEnumFn.create( "AnimationType", "anit", MAT_ANIM_NONE, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(animEnumFn.addField( "None", MAT_ANIM_NONE ));
	DOMMY_RETURN_IF_FAILURE(animEnumFn.addField( "From Attribute", MAT_ANIM_ATTRIBUTE ));
	DOMMY_RETURN_IF_FAILURE(animEnumFn.addField( "Input Data", MAT_ANIM_INPUT_DATA ));
	DOMMY_RETURN_IF_FAILURE(animEnumFn.setKeyable( true ));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aAnimationType));
	
	const MObject aAnimMatrixAttributeDefaultData = fnStringData.create("amat", &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	aAnimMatrixAttribute = tAttr.create("AnimMatrixAttribute", "amata", MFnData::kString, aAnimMatrixAttributeDefaultData, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aAnimMatrixAttribute));
	
	aInAnimMatrix = gAttr.create( "InAnimMatrix", "inamat",  &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(gAttr.addDataAccept(MFnData::kMatrixArray));
	DOMMY_RETURN_IF_FAILURE(gAttr.setStorable( false ));
	DOMMY_RETURN_IF_FAILURE(gAttr.setWritable( true ));
	DOMMY_RETURN_IF_FAILURE(gAttr.setReadable( false ));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aInAnimMatrix));
	
	
#pragma mark global transform attribute
	
	aLocalTranslate = nAttr.create( "LocalTranslate", "ltrans", MFnNumericData::k3Double , 0, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0,0.0,0.0));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aLocalTranslate));
	
	aGlobalTranslate = nAttr.create( "GlobalTranslate", "gtrans", MFnNumericData::k3Double , 0, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0,0.0,0.0));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aGlobalTranslate));
	
	aLocalRotate = nAttr.create( "LocalRotate", "lrot", MFnNumericData::k3Double , 0, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0,0.0,0.0));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aLocalRotate));
	
	
    aGlobalRotate = nAttr.create( "GlobalRotate", "grot", MFnNumericData::k3Double , 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0,0.0,0.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aGlobalRotate));
    
    aGlobalScale = nAttr.create( "GlobalScale", "gscal", MFnNumericData::k3Double , 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1.0,1.0,1.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aGlobalScale));
    
    
    aUniformScale = nAttr.create( "UniformScale", "uscal", MFnNumericData::kDouble , 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(1.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aUniformScale));
	
#pragma mark attribute
	const MObject aLocalTranslateAttributeDefaultData = fnStringData.create("", &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	aLocalTranslateAttribute = tAttr.create("LocalTranslateAttribute", "ltattr", MFnData::kString, aLocalTranslateAttributeDefaultData, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aLocalTranslateAttribute));
	
	const MObject aGlobalTranslateAttributeDefaultData = fnStringData.create("", &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	aGlobalTranslateAttribute = tAttr.create("GlobalTranslateAttribute", "gtattr", MFnData::kString, aGlobalTranslateAttributeDefaultData, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aGlobalTranslateAttribute));
	
	const MObject aLocalRotateAttributeDefaultData = fnStringData.create("", &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	aLocalRotateAttribute = tAttr.create("LocalRotateAttribute", "lrattr", MFnData::kString, aLocalRotateAttributeDefaultData, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aLocalRotateAttribute));
	
	const MObject aGlobalRotateAttributeDefaultData = fnStringData.create("", &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	aGlobalRotateAttribute = tAttr.create("GlobalRotateAttribute", "grattr", MFnData::kString, aGlobalRotateAttributeDefaultData, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aGlobalRotateAttribute));
	
	const MObject aGlobalScaleAttributeDefaultData = fnStringData.create("", &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	aGlobalScaleAttribute = tAttr.create("GlobalScaleAttribute", "gsattr", MFnData::kString, aGlobalScaleAttributeDefaultData, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aGlobalScaleAttribute));
	
	const MObject aUniformScaleAttributeDefaultData = fnStringData.create("", &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	aUniformScaleAttribute = tAttr.create("UniformScaleAttribute", "usattr", MFnData::kString, aUniformScaleAttributeDefaultData, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aUniformScaleAttribute));
	
	
#pragma mark random attribute
	
	aSeed = nAttr.create( "Seed", "se", MFnNumericData::kInt , 0, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1));
	DOMMY_RETURN_IF_FAILURE(nAttr.setMin(1));
	DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(100));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aSeed));
	
	
	
	aLocalTranslateRandomMin = nAttr.create( "LocalTranslateRandomMin", "ltransrndmin", MFnNumericData::k3Double , 0, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0,0.0,0.0));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aLocalTranslateRandomMin));
	
	aLocalTranslateRandomMax = nAttr.create( "LocalTranslateRandomMax", "ltransrndmax", MFnNumericData::k3Double , 0, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0,0.0,0.0));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aLocalTranslateRandomMax));
	
	
	aGlobalTranslateRandomMin = nAttr.create( "GlobalTranslateRandomMin", "gtransrndmin", MFnNumericData::k3Double , 0, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0,0.0,0.0));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aGlobalTranslateRandomMin));
	
	aGlobalTranslateRandomMax = nAttr.create( "GlobalTranslateRandomMax", "gtransrndmax", MFnNumericData::k3Double , 0, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0,0.0,0.0));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aGlobalTranslateRandomMax));
	
	
	
	aLocalRotateRandomMin = nAttr.create( "LocalRotateRandomMin", "lrotrndmin", MFnNumericData::k3Double , 0, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0,0.0,0.0));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aLocalRotateRandomMin));
	
	aLocalRotateRandomMax = nAttr.create( "LocalRotateRandomMax", "lrotrndmax", MFnNumericData::k3Double , 0, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0,0.0,0.0));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aLocalRotateRandomMax));
	
	aGlobalRotateRandomMin = nAttr.create( "GlobalRotateRandomMin", "grotrndmin", MFnNumericData::k3Double , 0, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0,0.0,0.0));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aGlobalRotateRandomMin));
	
	aGlobalRotateRandomMax = nAttr.create( "GlobalRotateRandomMax", "grotrndmax", MFnNumericData::k3Double , 0, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0,0.0,0.0));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aGlobalRotateRandomMax));
	
	
	aGlobalScaleRandomMin = nAttr.create( "GlobalScaleRandomMin", "gscalrndmin", MFnNumericData::k3Double , 0, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0,0.0,0.0));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aGlobalScaleRandomMin));
	
	aGlobalScaleRandomMax = nAttr.create( "GlobalScaleRandomMax", "gscalrndmax", MFnNumericData::k3Double , 0, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0,0.0,0.0));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aGlobalScaleRandomMax));
    
    aUniformScaleRandomMinMax = nAttr.create( "UniformScaleRandomMinMax", "usckmima", MFnNumericData::k2Double , 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0,0.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aUniformScaleRandomMinMax));
    
#pragma mark Render Attribute
    
    aIsRender = nAttr.create("IsRender", "isren", MFnNumericData::kBoolean, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aIsRender));
    
    aiOpaque = nAttr.create("aiOpaque", "aiopq", MFnNumericData::kBoolean, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aiOpaque));
    
    aiMatte = nAttr.create("aiMatte", "aimat", MFnNumericData::kBoolean, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aiMatte));
    
    aiSelfShadows = nAttr.create("aiSelfShadows", "aisshdw", MFnNumericData::kBoolean, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aiSelfShadows));
    
    aiReceiveShadows = nAttr.create("aiReceiveShadows", "airshdw", MFnNumericData::kBoolean, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aiReceiveShadows));
    
    
    aSidedness = nAttr.create("Sidedness", "sdns", MFnNumericData::kBoolean, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aSidedness));
    
    aInvertNormals = nAttr.create("InvertNormals", "invn", MFnNumericData::kBoolean, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aInvertNormals));
    
    const MObject aExportAttributeDefaultData = fnStringData.create("", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aExportAttribute = tAttr.create("ExportAttribute", "exattr", MFnData::kString, aExportAttributeDefaultData, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aExportAttribute));
    
#pragma mark Shader
    
    
    aShader = nAttr.createColor("Shader", "sh", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1.0, 1.0, 1.0, 1.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aShader));
	
	
	const MObject aTargetNodeNamesDefaultData = fnStringData.create("", &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	aTargetNodeNames = tAttr.create("TargetNodeNames", "tgtndls", MFnData::kString, aTargetNodeNamesDefaultData, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aTargetNodeNames));
	
	aReplaceShader = nAttr.createColor("ReplaceShader", "repsh", &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1.0, 1.0, 1.0, 1.0));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aReplaceShader));
	
	MFnCompoundAttribute cmpAttr;
	aReplaceShaderList = cmpAttr.create("ReplaceShaderList", "repshls", &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(cmpAttr.addChild(aTargetNodeNames));
	DOMMY_RETURN_IF_FAILURE(cmpAttr.addChild(aReplaceShader));
	DOMMY_RETURN_IF_FAILURE(cmpAttr.setArray( true ));
	DOMMY_RETURN_IF_FAILURE(cmpAttr.setUsesArrayDataBuilder(true));
	DOMMY_RETURN_IF_FAILURE(cmpAttr.setWritable( true ));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aReplaceShaderList));
	
	
	return MS::kSuccess;
}
catch (...) {
    return dommy::maya::reportEntryPointException(
        "DPInstancer", "initialize", std::current_exception());
}

#pragma mark DPInstancerOverride


DPInstancerOverride::DPInstancerOverride(const MObject& obj)
: MHWRender::MPxGeometryOverride(obj)
, mBaseUIShader(NULL)
, mPointUIShader(NULL)
, mWireUIShader(NULL)
, mInstancerNode(obj)
, mDataChanged(true)

{
    dommy::viewport::StockShaderAcquisition shaderAcquisition(mShaderUpdateFailed, "DPInstancerOverride");
    shaderAcquisition.acquire(MHWRender::MShaderManager::k3dDefaultMaterialShader, mBaseUIShader);
    shaderAcquisition.acquire(MHWRender::MShaderManager::k3dFatPointShader, mPointUIShader);
    shaderAcquisition.acquire(MHWRender::MShaderManager::k3dThickLineShader, mWireUIShader);
    if (DP_INSTANCER_DEBUG) reportInstancerShaderParameters(mBaseUIShader);
}


DPInstancerOverride::~DPInstancerOverride()
{
    dommy::viewport::StockShaderRelease shaderRelease("DPInstancerOverride");
    shaderRelease.release(mBaseUIShader);
    shaderRelease.release(mPointUIShader);
    shaderRelease.release(mWireUIShader);
}


MHWRender::DrawAPI DPInstancerOverride::supportedDrawAPIs() const
{
	return (MHWRender::kOpenGL | MHWRender::kDirectX11 | MHWRender::kOpenGLCoreProfile);
}


/////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////

static const MString DPInstItemName_       = "DPInstItem";
static const MString DPInstWireItemName_ 	= "DPInstWireItem";
static const MString DPInstPointItemName_ 	= "DPInstPointItem";


MStatus DPInstancerOverride::evalParms(DTransformParams& parms) {
	
	//animation matrix
    MString attributeName;
    DOMMY_RETURN_IF_FAILURE(readPlugValue(MPlug(mInstancerNode, DPInstancer::aAnimationType), parms.mAnimMatrixType));
    DOMMY_RETURN_IF_FAILURE(readPlugValue(mInstancerNode, DPInstancer::aAnimMatrixAttribute, attributeName));
    parms.mAnimationAttribute = attributeName.asChar();
	parms.mNodeObj = mInstancerNode;
	parms.mAnim_Mat_InputData_Object = DPInstancer::aInAnimMatrix;
	
	//normal and tangent
	    dommy::maya::ScopedPlugDataHandle aimVecHndOwner;
	    MDataHandle& aimVecHnd = aimVecHndOwner.handle();
	    dommy::maya::ScopedPlugDataHandle axisVecHndOwner;
	    MDataHandle& axisVecHnd = axisVecHndOwner.handle();
	    MStatus status = readDataHandle(
	        mInstancerNode, DPInstancer::aAimVector, aimVecHndOwner);
	    if (!status) return status;
	    status = readDataHandle(
	        mInstancerNode, DPInstancer::aAxisVector, axisVecHndOwner);
	    if (!status) return status;
	    parms.mNormalBaseAxis = aimVecHnd.asVector();
	    parms.mTangentBaseAxis = axisVecHnd.asVector();
	    
		dommy::maya::ScopedPlugDataHandle normalVecHndOwner;
		MDataHandle& normalVecHnd = normalVecHndOwner.handle();
		dommy::maya::ScopedPlugDataHandle tangentHndOwner;
		MDataHandle& tangentHnd = tangentHndOwner.handle();
		status = readDataHandle(
			mInstancerNode, DPInstancer::aNormalVector, normalVecHndOwner);
		if (!status) return status;
		status = readDataHandle(
			mInstancerNode, DPInstancer::aTangentVector, tangentHndOwner);
		if (!status) return status;
	parms.mNormalVec = normalVecHnd.asVector();
	parms.mTangentVec = tangentHnd.asVector();
	if(DP_INSTANCER_DEBUG) DOMMY_MAYA_DEBUG("DPInstancer", "normalVec : " << parms.mNormalVec);
	if(DP_INSTANCER_DEBUG) DOMMY_MAYA_DEBUG("DPInstancer", "tangentVec : " << parms.mTangentVec);
	
    DOMMY_RETURN_IF_FAILURE(readPlugValue(mInstancerNode, DPInstancer::aNormalAttribute, attributeName));
    parms.mNormalVecAttribute = attributeName.asChar();
    DOMMY_RETURN_IF_FAILURE(readPlugValue(mInstancerNode, DPInstancer::aTangentAttribute, attributeName));
    parms.mTangentVecAttribute = attributeName.asChar();
	
	//global transform
	dommy::maya::ScopedPlugDataHandle lTransHndOwner;
	MDataHandle& lTransHnd = lTransHndOwner.handle();
	dommy::maya::ScopedPlugDataHandle gTransHndOwner;
	MDataHandle& gTransHnd = gTransHndOwner.handle();
	dommy::maya::ScopedPlugDataHandle lRotHndOwner;
	MDataHandle& lRotHnd = lRotHndOwner.handle();
	dommy::maya::ScopedPlugDataHandle gRotHndOwner;
	MDataHandle& gRotHnd = gRotHndOwner.handle();
	dommy::maya::ScopedPlugDataHandle gScalHndOwner;
	MDataHandle& gScalHnd = gScalHndOwner.handle();
	status = readDataHandle(
		mInstancerNode, DPInstancer::aLocalTranslate, lTransHndOwner);
	if (!status) return status;
	status = readDataHandle(
		mInstancerNode, DPInstancer::aGlobalTranslate, gTransHndOwner);
	if (!status) return status;
	status = readDataHandle(
		mInstancerNode, DPInstancer::aLocalRotate, lRotHndOwner);
	if (!status) return status;
	status = readDataHandle(
		mInstancerNode, DPInstancer::aGlobalRotate, gRotHndOwner);
	if (!status) return status;
	status = readDataHandle(
		mInstancerNode, DPInstancer::aGlobalScale, gScalHndOwner);
	if (!status) return status;
    int seed = 1;
    DOMMY_RETURN_IF_FAILURE(readPlugValue(mInstancerNode, DPInstancer::aSeed, seed));
    parms.mSeed = static_cast<std::size_t>(seed);
	const double3 &lTransVal = lTransHnd.asDouble3();
    parms.mLocalTranslate = MVector(lTransVal);
	const double3 &gTransVal = gTransHnd.asDouble3();
	parms.mGlobalTranslate = MVector(gTransVal);
	const double3 &lRotateVal = lRotHnd.asDouble3();
	parms.mLocalRot = MVector(lRotateVal);
	const double3 &gRotateVal = gRotHnd.asDouble3();
	parms.mGlobalRot = MVector(gRotateVal);
	const double3 &gScaleVal = gScalHnd.asDouble3();
	parms.mGlobalScale = MVector(gScaleVal);
    DOMMY_RETURN_IF_FAILURE(readPlugValue(mInstancerNode, DPInstancer::aUniformScale, parms.mUniformScale));
	
	
    DOMMY_RETURN_IF_FAILURE(readPlugValue(mInstancerNode, DPInstancer::aLocalTranslateAttribute, attributeName));
    parms.mLocalTransAttribute = attributeName.asChar();
    DOMMY_RETURN_IF_FAILURE(readPlugValue(mInstancerNode, DPInstancer::aGlobalTranslateAttribute, attributeName));
    parms.mGlobalTransAttribute = attributeName.asChar();
    DOMMY_RETURN_IF_FAILURE(readPlugValue(mInstancerNode, DPInstancer::aLocalRotateAttribute, attributeName));
    parms.mLocalRotAttribute = attributeName.asChar();
    DOMMY_RETURN_IF_FAILURE(readPlugValue(mInstancerNode, DPInstancer::aGlobalRotateAttribute, attributeName));
    parms.mGlobalRotAttribute = attributeName.asChar();
    DOMMY_RETURN_IF_FAILURE(readPlugValue(mInstancerNode, DPInstancer::aGlobalScaleAttribute, attributeName));
    parms.mGlobalScaleAttribute = attributeName.asChar();
    DOMMY_RETURN_IF_FAILURE(readPlugValue(mInstancerNode, DPInstancer::aUniformScaleAttribute, attributeName));
    parms.mUniformScaleAttribute = attributeName.asChar();
	
	
	//random attribute
	dommy::maya::ScopedPlugDataHandle lTransRndHndMinOwner;
	MDataHandle& lTransRndHndMin = lTransRndHndMinOwner.handle();
	dommy::maya::ScopedPlugDataHandle lTransRndHndMaxOwner;
	MDataHandle& lTransRndHndMax = lTransRndHndMaxOwner.handle();
	dommy::maya::ScopedPlugDataHandle gTransRndHndMinOwner;
	MDataHandle& gTransRndHndMin = gTransRndHndMinOwner.handle();
	dommy::maya::ScopedPlugDataHandle gTransRndHndMaxOwner;
	MDataHandle& gTransRndHndMax = gTransRndHndMaxOwner.handle();//translate
	status = readDataHandle(
		mInstancerNode, DPInstancer::aLocalTranslateRandomMin,
		lTransRndHndMinOwner);
	if (!status) return status;
	status = readDataHandle(
		mInstancerNode, DPInstancer::aLocalTranslateRandomMax,
		lTransRndHndMaxOwner);
	if (!status) return status;
	status = readDataHandle(
		mInstancerNode, DPInstancer::aGlobalTranslateRandomMin,
		gTransRndHndMinOwner);
	if (!status) return status;
	status = readDataHandle(
		mInstancerNode, DPInstancer::aGlobalTranslateRandomMax,
		gTransRndHndMaxOwner);
	if (!status) return status;
	const double3 &lTransMinRndVal = lTransRndHndMin.asDouble3();
	parms.mLocalTransRndMin = MVector(lTransMinRndVal);
	const double3 &lTransMaxRndVal = lTransRndHndMax.asDouble3();
	parms.mLocalTransRndMax = MVector(lTransMaxRndVal);
	const double3 &gTransMinRndVal = gTransRndHndMin.asDouble3();
	parms.mGlobalTransRndMin = MVector(gTransMinRndVal);
	const double3 &gTransMaxRndVal = gTransRndHndMax.asDouble3();
	parms.mGlobalTransRndMax = MVector(gTransMaxRndVal);
	
	dommy::maya::ScopedPlugDataHandle lRotRndHndMinOwner;
	MDataHandle& lRotRndHndMin = lRotRndHndMinOwner.handle();
	dommy::maya::ScopedPlugDataHandle lRotRndHndMaxOwner;
	MDataHandle& lRotRndHndMax = lRotRndHndMaxOwner.handle();
	dommy::maya::ScopedPlugDataHandle gRotRndHndMinOwner;
	MDataHandle& gRotRndHndMin = gRotRndHndMinOwner.handle();
	dommy::maya::ScopedPlugDataHandle gRotRndHndMaxOwner;
	MDataHandle& gRotRndHndMax = gRotRndHndMaxOwner.handle();//rotate
	status = readDataHandle(
		mInstancerNode, DPInstancer::aLocalRotateRandomMin,
		lRotRndHndMinOwner);
	if (!status) return status;
	status = readDataHandle(
		mInstancerNode, DPInstancer::aLocalRotateRandomMax,
		lRotRndHndMaxOwner);
	if (!status) return status;
	status = readDataHandle(
		mInstancerNode, DPInstancer::aGlobalRotateRandomMin,
		gRotRndHndMinOwner);
	if (!status) return status;
	status = readDataHandle(
		mInstancerNode, DPInstancer::aGlobalRotateRandomMax,
		gRotRndHndMaxOwner);
	if (!status) return status;
	const double3 &lRotateMinRndVal = lRotRndHndMin.asDouble3();
	parms.mLocalRotRndMin = MVector(lRotateMinRndVal);
	const double3 &lRotateMaxRndVal = lRotRndHndMax.asDouble3();
	parms.mLocalRotRndMax = MVector(lRotateMaxRndVal);
	const double3 &gRotateMinRndVal = gRotRndHndMin.asDouble3();
	parms.mGlobalRotRndMin = MVector(gRotateMinRndVal);
	const double3 &gRotateMaxRndVal = gRotRndHndMax.asDouble3();
	parms.mGlobalRotRndMax = MVector(gRotateMaxRndVal);
	
	dommy::maya::ScopedPlugDataHandle gScalRndHndMinOwner;
	MDataHandle& gScalRndHndMin = gScalRndHndMinOwner.handle();
	dommy::maya::ScopedPlugDataHandle gScalRndHndMaxOwner;
	MDataHandle& gScalRndHndMax = gScalRndHndMaxOwner.handle();
	dommy::maya::ScopedPlugDataHandle uniScalRndHndOwner;
	MDataHandle& uniScalRndHnd = uniScalRndHndOwner.handle();//scale
	status = readDataHandle(
		mInstancerNode, DPInstancer::aGlobalScaleRandomMin,
		gScalRndHndMinOwner);
	if (!status) return status;
	status = readDataHandle(
		mInstancerNode, DPInstancer::aGlobalScaleRandomMax,
		gScalRndHndMaxOwner);
	if (!status) return status;
	status = readDataHandle(
		mInstancerNode, DPInstancer::aUniformScaleRandomMinMax,
		uniScalRndHndOwner);
	if (!status) return status;
	const double3 &gScaleMinVals = gScalRndHndMin.asDouble3();
	parms.mGlobalScaleRndMin = MVector(gScaleMinVals);
	const double3 &gScaleMaxVals = gScalRndHndMax.asDouble3();
	parms.mGlobalScaleRndMax = MVector(gScaleMaxVals);
    const double2 &uniScaleMinMaxVals = uniScalRndHnd.asDouble2();
    parms.mUniformScaleMinMax = MVector(uniScaleMinMaxVals[0], uniScaleMinMaxVals[1], 0.0);
	
	
    
	return MS::kSuccess;
}



bool readAssBoundingBoxFromStream(std::istream& in, MPoint& minPt, MPoint& maxPt)
{
	MPoint parsedMin;
	MPoint parsedMax;
	bool foundBounds = false;
	std::string line;
	std::size_t lineCount = 0U;
	std::size_t totalBytes = 0U;
	while (dommy::file::readBoundedLine(
			in, line, totalBytes, static_cast<std::size_t>(kMaxAssTocFileBytes),
			kMaxAssTocLineBytes, dommy::file::UnterminatedLinePolicy::PhysicalBytes)) {
		if (++lineCount > kMaxAssTocLineCount) {
			return false;
		}

		std::istringstream values(line);
		values.imbue(std::locale::classic());
		std::string keyword;
		values >> keyword;
		if (keyword != "bounds") {
			continue;
		}

		double* coordinates[] = {&parsedMin.x, &parsedMin.y, &parsedMin.z,
		                         &parsedMax.x, &parsedMax.y, &parsedMax.z};
		for (double* coordinate : coordinates) {
			if (dommy::file::readFiniteDoubleToken(values, *coordinate, true) !=
			    dommy::file::NumericTokenRead::Value) return false;
		}
		if (!std::isfinite(parsedMin.x) || !std::isfinite(parsedMin.y) ||
		    !std::isfinite(parsedMin.z) || !std::isfinite(parsedMax.x) ||
		    !std::isfinite(parsedMax.y) || !std::isfinite(parsedMax.z) ||
		    parsedMin.x > parsedMax.x || parsedMin.y > parsedMax.y ||
		    parsedMin.z > parsedMax.z) {
			return false;
		}
		foundBounds = true;
		break;
	}

	if (!foundBounds || in.bad()) {
		return false;
	}

    for (unsigned int axis = 0U; axis < 3U; ++axis) {
        double center = 0.0;
        if (!dommy::attribute::checkedLinearBlend(parsedMin[axis], parsedMax[axis], 0.5, center))
            return false;
        parsedMin[axis] -= center;
        parsedMax[axis] -= center;
        if (!dommy::numeric::canRepresentAsFloat(parsedMin[axis]) ||
            !dommy::numeric::canRepresentAsFloat(parsedMax[axis])) return false;
    }
	minPt = parsedMin;
	maxPt = parsedMax;
	return true;
}

bool loadAssBoundingBoxFromFile(const std::string& filename, MPoint& minPt, MPoint& maxPt)
{
    if (!dommy::file::fileSizeWithinLimit(filename, kMaxAssTocFileBytes)) return false;
    // Count physical bytes consistently, including CRLF on Windows.
    std::ifstream in(filename, std::ios::binary);
    return in && readAssBoundingBoxFromStream(in, minPt, maxPt);
}

// Only successful, validated reads form a cache entry. A missing/invalid file
// invalidates the key so correcting the same filename is retried next update.
// Bounds themselves stay staged until the filename copy also succeeds.
bool readAssBoundingBoxCache(const std::string& filename, std::string& cachedFilename,
                             MPoint& minPt, MPoint& maxPt)
{
    const std::string tocFilename = filename + "toc";
    if (filename.empty() ||
        !dommy::file::fileSizeWithinLimit(tocFilename, kMaxAssTocFileBytes)) {
        cachedFilename.clear();
        return false;
    }
    if (filename == cachedFilename) return true;

    MPoint candidateMin, candidateMax;
    if (!loadAssBoundingBoxFromFile(tocFilename, candidateMin, candidateMax)) {
        cachedFilename.clear();
        return false;
    }
    std::string candidateFilename(filename);
    minPt = candidateMin;
    maxPt = candidateMax;
    cachedFilename.swap(candidateFilename);
    return true;
}




void DPInstancerOverride::updateDG()
{
    try {
        updateDGImpl();
    } catch (...) {
        clearDrawData();
        mDataChanged = true;
        dommy::maya::reportViewportCallbackException(
            "DPInstancerOverride", "updateDG", std::current_exception());
    }
}

void DPInstancerOverride::clearDrawData() noexcept
{
    dData.pointList.clear();
    dData.indexList.clear();
    dData.normalList.clear();
    dData.displayType = 0;
}

void DPInstancerOverride::updateDGImpl()
{
    clearDrawData();
    mDataChanged = true;
    MStatus status;
	MFnDependencyNode nodeFn(mInstancerNode, &status);
    if (!status) return;
	if(DP_INSTANCER_DEBUG) DOMMY_MAYA_DEBUG("DPInstancer", "nodeFn : " << nodeFn.name().asChar());
	MPlug dpPlug = nodeFn.findPlug(DPInstancer::inDP, true, &status);
    if (!status) return;
	if(DP_INSTANCER_DEBUG) DOMMY_MAYA_DEBUG("DPInstancer", "plug name : " << dpPlug.name().asChar());
	
	
    if (readPlugValue(MPlug(mInstancerNode, DPInstancer::aDisplayType), dData.displayType) != MS::kSuccess ||
        readPlugValue(MPlug(mInstancerNode, DPInstancer::aCopyObjectType), dData.mCopyType) != MS::kSuccess) return;
	
	if(dData.displayType==0)
	{
		//Diaply None
		mDataChanged = true;
		return;
	}
	
    
    const bool pointConnected = !dpPlug.isNull() && dpPlug.isConnected(&status);
    if (!status) {
        MGlobal::displayError("DPInstancer: unable to query DPoint input connection.");
        return;
    }
    if(DP_INSTANCER_DEBUG) DOMMY_MAYA_DEBUG("DPInstancer", "DPInstancer::inDP : " << pointConnected);
	// dpoint
	if (pointConnected) {
	        MObject dataObject;
	        const DPData* dpdata = nullptr;
	        const MStatus dataStatus =
	            dommy::maya::getPluginDataFromPlug<DPData>(
	                dpPlug, DPData::id, dataObject, dpdata);
	        if (!dataStatus || !dpdata) {
				EchoWarning(nodeFn.name().asChar(), "Input DPoint data is invalid.");
				mDataChanged = true;
				return;
		}

        const size_t instCount = dpdata->numberOfPoints();
        if (instCount > dommy::viewport::kMaxViewportVertexCount ||
            (dData.displayType != DP_INST_POINT &&
             instCount > dommy::generation::kMaxGeneratedTransformCount) ||
            !dpdata->hasValidPointTopology()) {
            EchoWarning(nodeFn.name().asChar(), "Invalid or excessive instance count.");
            return;
        }
        if(instCount) {
			
			
			
			
			
            if (dData.mCopyType==DP_INST_COPY_MESH) {
				
#pragma mark /////////////// From Input Mesd data ///////////////
                //Display From Mesh
                
                //check mesh
                MPlug inmeshPlug(mInstancerNode, DPInstancer::inMesh);
                const bool meshConnected = !inmeshPlug.isNull() && inmeshPlug.isConnected(&status);
                if (!status) {
                    MGlobal::displayError("DPInstancer: unable to query mesh input connection.");
                    return;
                }
	                if (meshConnected) {
	                    dommy::maya::ScopedPlugDataHandle meshHndOwner;
	                    MDataHandle& meshHnd = meshHndOwner.handle();
	                    if (meshHndOwner.read(inmeshPlug) != MS::kSuccess) {
	                        EchoWarning(
	                            nodeFn.name().asChar(),
	                            "Input mesh data could not be evaluated.");
	                        mDataChanged = true;
	                        return;
	                    }
                    
                    //check mesh and dp
                    if (meshHnd.type() == MFnData::kMesh) {
						
                        
                        if (dData.displayType==DP_INST_POINT) {
                            
#pragma mark /////Display Point data
                            
                            if (!resizeInstanceBuffers(1, 1, instCount, false,
                                                       dData.pointList, dData.indexList,
                                                       dData.normalList)) {
								EchoWarning(nodeFn.name().asChar(), "Viewport point buffer is too large.");
								mDataChanged = true;
								return;
							}
                            
                            size_t vIndex=0;
                            size_t iIndex=0;
                            for (auto pIt=dpdata->beginPt();pIt!=dpdata->endPt();++pIt) {
                                dData.pointList[vIndex++] = (*pIt)[0];
                                dData.pointList[vIndex++] = (*pIt)[1];
                                dData.pointList[vIndex++] = (*pIt)[2];
                                
                                dData.indexList[iIndex] = iIndex;
                                iIndex++;
                            }
                            
                            
                        } else {
                            
                            
#pragma mark Get parameter
                            DTransformParams params;
                            
                            if (evalParms(params) != MS::kSuccess) {
                                EchoWarning(nodeFn.name().asChar(), "Failed read parameter.");
                                return;
                            }
                            
                            
                            MStatus status;
                            MObject meshObject = meshHnd.data();
                            MFnMesh baseMeshFn(meshObject, &status);
                            if (!status) return;
                            std::size_t basePointCount = 8U, baseIndexCount = 24U;
                            std::size_t faceCount = 0U, triangleCount = 0U;
                            int edgeCount = 0;
                            if (dData.displayType != DP_INST_BBOX) {
                                const int vertices = baseMeshFn.numVertices(&status);
                                if (!status || !dommy::generation::checkedCount(
                                        vertices, dommy::viewport::kMaxViewportVertexCount,
                                        basePointCount)) return;
                                if (dData.displayType == DP_INST_WIRE) {
                                    edgeCount = baseMeshFn.numEdges(&status);
                                    if (!status || edgeCount < 0 ||
                                        !dommy::memory::checkedMultiply(
                                            static_cast<std::size_t>(edgeCount), 2U,
                                            baseIndexCount)) return;
                                } else if (dData.displayType == DP_INST_SHADE) {
                                    status = dommy::maya::preflightMeshTriangles(
                                        baseMeshFn, meshObject, faceCount, triangleCount);
                                    if (!status || !dommy::memory::checkedMultiply(
                                            triangleCount, 3U, baseIndexCount)) return;
                                } else return;
                            }
                            std::size_t totalPoints = 0U, totalScalars = 0U, totalIndices = 0U;
                            if (!dommy::viewport::checkedInstanceLayout(
                                    basePointCount, baseIndexCount, instCount,
                                    totalPoints, totalScalars, totalIndices)) {
                                EchoWarning(nodeFn.name().asChar(), "Instance geometry exceeds the viewport budget.");
                                return;
                            }

                            MPointArray pts;
                            MIntArray connections;
                            MFloatVectorArray normals;
                            if (dData.displayType == DP_INST_BBOX) {
                                MPlugArray sourceConnections;
                                inmeshPlug.connectedTo(sourceConnections, true, true, &status);
                                if (!status) return;
                                MBoundingBox box(MPoint(-.5,-.5,-.5), MPoint(.5,.5,.5));
                                if (sourceConnections.length()) {
                                    const MObject sourceNode = sourceConnections[0].node(&status);
                                    if (!status) return;
                                    MFnDagNode dag(sourceNode, &status);
                                    if (!status) return;
                                    box = dag.boundingBox(&status);
                                    if (!status) return;
                                }
                                status = DGenerateDisplayBBoxDataFromMinMaxPoint(
                                    box.min(), box.max(), pts, connections);
                                if (!status) return;
                            } else {
                                status = baseMeshFn.getPoints(pts, MSpace::kObject);
                                if (!status || pts.length() != basePointCount) return;
                                if (dData.displayType == DP_INST_WIRE) {
                                    status = connections.setLength(static_cast<unsigned int>(baseIndexCount));
                                    if (!status || connections.length() != baseIndexCount) return;
                                    MItMeshEdge edges(meshObject, &status);
                                    if (!status) return;
                                    std::size_t offset = 0U;
                                    status = dommy::maya::visitMeshElements(
                                        edges, static_cast<std::size_t>(edgeCount),
                                        [&](MItMeshEdge& edge) -> MStatus {
                                            if (offset > connections.length() ||
                                                connections.length() - offset < 2U)
                                                return MS::kFailure;
                                            MStatus queried;
                                            const int first = edge.index(0, &queried);
                                            if (!queried) return queried;
                                            const int second = edge.index(1, &queried);
                                            if (!queried) return queried;
                                            if (first < 0 || second < 0 ||
                                                static_cast<std::size_t>(first) >= basePointCount ||
                                                static_cast<std::size_t>(second) >= basePointCount)
                                                return MS::kInvalidParameter;
                                            connections[static_cast<unsigned int>(offset++)] = first;
                                            connections[static_cast<unsigned int>(offset++)] = second;
                                            return MS::kSuccess;
                                        });
                                    if (!status || offset != connections.length()) return;
                                } else {
                                    MIntArray triangleCounts;
                                    status = baseMeshFn.getTriangles(triangleCounts, connections);
                                    if (!status || !dommy::maya::validMeshTriangleArrays(
                                            pts, triangleCounts, connections, basePointCount,
                                            faceCount, triangleCount)) return;
                                    status = normals.setLength(pts.length());
                                    if (!status || normals.length() != pts.length()) return;
                                    for (unsigned int vertex = 0U; vertex < pts.length(); ++vertex) {
                                        MVector normal;
                                        status = baseMeshFn.getVertexNormal(
                                            static_cast<int>(vertex), false, normal, MSpace::kObject);
                                        if (!status || !dommy::maya::isFloatRepresentableVector(normal)) return;
                                        normals[vertex] = normal;
                                    }
                                }
                            }
                            std::vector<MMatrix> matrices;
                            // Let the generator validate before allocating its staging matrices.
                            status = DGenerateTransformMatrixFromDP(dpdata, params, matrices);
                            if (!status || matrices.size() != instCount ||
                                !buildInstanceBuffers(pts, connections,
                                    dData.displayType == DP_INST_SHADE ? &normals : NULL,
                                    matrices, dData.pointList, dData.indexList, dData.normalList)) {
                                EchoWarning(nodeFn.name().asChar(), "Invalid instance transforms or viewport geometry.");
                                return;
                            }
                            
                        }// check point or object display
                        
                        
                        
                        
                        
						
                    }//check mesh and dp
                    
                }
            } else if (dData.mCopyType == DP_INST_COPY_ASS) {
                
#pragma mark //////////// from Ass file ////////////
                
                MString assFilename;
                if (readPlugValue(mInstancerNode, DPInstancer::aReadAssFile, assFilename) != MS::kSuccess) return;
                const std::string filename(assFilename.asChar());
				if(DP_INSTANCER_DEBUG) DOMMY_MAYA_DEBUG("DPInstancer", "Ass filename:" << filename);
				
                if (readAssBoundingBoxCache(filename, dData.assFile, dData.minPt, dData.maxPt)) {
					
					
					//set Bounding box data
					MPointArray bboxPts;
					MIntArray connectArray;
					if (DGenerateDisplayBBoxDataFromMinMaxPoint(
                            dData.minPt, dData.maxPt, bboxPts, connectArray) != MS::kSuccess)
                        return;
                    std::size_t totalPoints = 0U, totalScalars = 0U, totalIndices = 0U;
                    if (!dommy::viewport::checkedInstanceLayout(
                            bboxPts.length(), connectArray.length(), instCount,
                            totalPoints, totalScalars, totalIndices)) return;
					
					dData.displayType = DP_INST_BBOX;//Ass fileの時は強制的にBounding Box
					
					
#pragma mark Get parameter
					DTransformParams params;
					
					if (evalParms(params) != MS::kSuccess) {
						EchoWarning(nodeFn.name().asChar(), "Failed read parameter.");
						return;
					}
					
					
#pragma mark Generate Matrix
                    std::vector<MMatrix> matrices;
					
					
					if (DGenerateTransformMatrixFromDP(
					        dpdata, params, matrices) != MS::kSuccess || matrices.size() != instCount) {
						EchoWarning(nodeFn.name().asChar(),
						            "Failed to generate valid instance transforms.");
						mDataChanged = true;
						return;
					}
					
					
#pragma mark Set Ass File BBox Data
					if (!buildInstanceBuffers(bboxPts, connectArray, NULL, matrices,
                            dData.pointList, dData.indexList, dData.normalList)) {
						EchoWarning(nodeFn.name().asChar(), "Viewport Ass bounding-box buffer is invalid or too large.");
						mDataChanged = true;
						return;
					}
					
					
					
                } else {
                    EchoWarning(nodeFn.name().asChar(), "Failed to read valid bounds from the Ass toc file.");
                }
                
				
                
			}  else if (dData.mCopyType == DP_INST_COPY_ASS_SEQUENCE) {
				
#pragma mark //////////// from Ass Sequence file ////////////
				
					//get time attribute
					dommy::maya::ScopedPlugDataHandle timeRangeHndOwner;
					MDataHandle& timeRangeHnd = timeRangeHndOwner.handle();
					if (readDataHandle(
							mInstancerNode, DPInstancer::aTimeRange,
							timeRangeHndOwner) != MS::kSuccess) {
						EchoWarning(
							nodeFn.name().asChar(),
							"Ass sequence time range could not be evaluated.");
						mDataChanged = true;
						return;
					}
				const int2 &timeRangeVals = timeRangeHnd.asInt2();
				
                int timeOffset = 0;
                MString timeAttribute;
                if (readPlugValue(mInstancerNode, DPInstancer::aTimeOffset, timeOffset) != MS::kSuccess ||
                    readPlugValue(mInstancerNode, DPInstancer::aTimeAttribute, timeAttribute) != MS::kSuccess) return;
                const std::string timeAttributeStr(timeAttribute.asChar());
				
				int minTime , maxTime;
				if (timeRangeVals[0] < timeRangeVals[1]) {
					minTime = timeRangeVals[0];
					maxTime = timeRangeVals[1];
				} else {
					minTime = timeRangeVals[1];
					maxTime = timeRangeVals[0];
				}
				
				if(DP_INSTANCER_DEBUG) DOMMY_MAYA_DEBUG("DPInstancer", "minTime:" << minTime);
				if(DP_INSTANCER_DEBUG) DOMMY_MAYA_DEBUG("DPInstancer", "maxTime:" << maxTime);
				if(DP_INSTANCER_DEBUG) DOMMY_MAYA_DEBUG("DPInstancer", "timeOffset:" << timeOffset);
				if(DP_INSTANCER_DEBUG) DOMMY_MAYA_DEBUG("DPInstancer", "timeAttribute:" << timeAttributeStr);

                // ASS-sequence rendering remains an archival placeholder.
                (void)minTime;
                (void)maxTime;
                (void)timeOffset;
                (void)timeAttributeStr;
				
				
				
				
				
				
			}//check copy type
			
			
			if(DP_INSTANCER_DEBUG) DOMMY_MAYA_DEBUG("DPInstancer", "pointList count:" << dData.pointList.size());
			if(DP_INSTANCER_DEBUG) DOMMY_MAYA_DEBUG("DPInstancer", "normalList count:" << dData.normalList.size());
			if(DP_INSTANCER_DEBUG) DOMMY_MAYA_DEBUG("DPInstancer", "indexList count:" << dData.indexList.size());
			
			
        }//check dpoint
        
	}
	
	mDataChanged = true;
}

void DPInstancerOverride::updateRenderItems( const MDagPath& path, MHWRender::MRenderItemList& list )
try
{
    dommy::viewport::RenderItemUpdateScope renderUpdate(mShaderUpdateFailed);
    dommy::viewport::StockShaderAcquisition shaderAcquisition(mShaderUpdateFailed, "DPInstancerOverride");
    shaderAcquisition.acquire(MHWRender::MShaderManager::k3dDefaultMaterialShader, mBaseUIShader);
    shaderAcquisition.acquire(MHWRender::MShaderManager::k3dFatPointShader, mPointUIShader);
    shaderAcquisition.acquire(MHWRender::MShaderManager::k3dThickLineShader, mWireUIShader);
	
	
#pragma mark mesh shader
	MHWRender::MRenderItem* baseShader = NULL;
	
	int index = list.indexOf(DPInstItemName_);
	
	if (index < 0)
	{
		dommy::viewport::PendingRenderItem pendingItem(
            MHWRender::MRenderItem::Create(DPInstItemName_, MHWRender::MRenderItem::MaterialSceneItem, MHWRender::MGeometry::kTriangles),
            dommy::viewport::RenderItemDeleter{"DPInstancerOverride"});
        if (!pendingItem) return;
        pendingItem->setDrawMode(MHWRender::MGeometry::kAll);
        if (!list.append(pendingItem.get())) return;
        baseShader = pendingItem.release();
	}
	else
	{
		baseShader = list.itemAt(index);
	}
	
	if(baseShader)
	{
        dommy::viewport::RenderItemShaderUpdate shaderUpdate(*baseShader, mShaderUpdateFailed);
		if (mBaseUIShader)
		{
			const float  solidColor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
			shaderUpdate.setParameter(mBaseUIShader, "diffuseColor", solidColor);
			shaderUpdate.setParameter(mBaseUIShader, "specularPower", 0.0f);
			shaderUpdate.setShader(mBaseUIShader, &DPInstItemName_);
			
		}
		shaderUpdate.enable(!dData.pointList.empty() && dData.displayType==DP_INST_SHADE);
	}
	
#pragma mark wire shader
	MHWRender::MRenderItem* wireShader = NULL;
	
	index = list.indexOf(DPInstWireItemName_);
	
	if (index < 0)
	{
		dommy::viewport::PendingRenderItem pendingItem(
            MHWRender::MRenderItem::Create(DPInstWireItemName_,
												   MHWRender::MRenderItem::DecorationItem,
												   MHWRender::MGeometry::kLines),
            dommy::viewport::RenderItemDeleter{"DPInstancerOverride"});
        if (!pendingItem) return;
        pendingItem->setDrawMode(MHWRender::MGeometry::kAll);
		pendingItem->depthPriority(MHWRender::MRenderItem::sDormantWireDepthPriority);
        if (!list.append(pendingItem.get())) return;
        wireShader = pendingItem.release();
	}
	else
	{
		wireShader = list.itemAt(index);
	}
	
	if(wireShader)
	{
        dommy::viewport::RenderItemShaderUpdate shaderUpdate(*wireShader, mShaderUpdateFailed);
		if (mWireUIShader)
		{
			const float lineWidth[2] = { 1.0, 1.0 };
			MColor wireColor = MHWRender::MGeometryUtilities::wireframeColor(path);
			const float  wirefloat[4] = { wireColor.r, wireColor.g, wireColor.b, 1.0f };
			shaderUpdate.setParameter(mWireUIShader, "solidColor", wirefloat);
			shaderUpdate.setParameter(mWireUIShader, "lineWidth", lineWidth);
			
				shaderUpdate.setShader(mWireUIShader, &DPInstWireItemName_);
			}
			shaderUpdate.enable(!dData.pointList.empty() &&
                                (dData.displayType==DP_INST_BBOX || dData.displayType==DP_INST_WIRE));
	}
	
	
#pragma mark Point shader
	
	MHWRender::MRenderItem* pointShader = NULL;
	
	index = list.indexOf(DPInstPointItemName_);
	
	if (index < 0)
	{
		dommy::viewport::PendingRenderItem pendingItem(
            MHWRender::MRenderItem::Create(DPInstPointItemName_,
													MHWRender::MRenderItem::DecorationItem,
													MHWRender::MGeometry::kPoints),
            dommy::viewport::RenderItemDeleter{"DPInstancerOverride"});
        if (!pendingItem) return;
        pendingItem->setDrawMode(MHWRender::MGeometry::kAll);
		pendingItem->depthPriority(MHWRender::MRenderItem::sDormantWireDepthPriority);
        if (!list.append(pendingItem.get())) return;
        pointShader = pendingItem.release();
	}
	else
	{
		pointShader = list.itemAt(index);
	}
	
	if(pointShader)
	{
        dommy::viewport::RenderItemShaderUpdate shaderUpdate(*pointShader, mShaderUpdateFailed);
		if (mPointUIShader)
		{
			MColor wireColor = MHWRender::MGeometryUtilities::wireframeColor(path);
			const float  wirefloat[4] = { wireColor.r, wireColor.g, wireColor.b, 1.0f };
			shaderUpdate.setParameter(mPointUIShader, "solidColor", wirefloat);
			const float2 pSize = {3.0, 3.0};//point size
			shaderUpdate.setParameter(mPointUIShader, "pointSize", pSize);
			shaderUpdate.setShader(mPointUIShader, &DPInstPointItemName_);
			
		}
		
		shaderUpdate.enable(!dData.pointList.empty() && dData.displayType==DP_INST_POINT);
	}
	
    renderUpdate.complete();
}
DOMMY_VIEWPORT_VOID_CATCH("DPInstancerOverride", "updateRenderItems")


void DPInstancerOverride::populateGeometry(const MHWRender::MGeometryRequirements& requirements,
											 const MHWRender::MRenderItemList& renderItems,
											 MHWRender::MGeometry& data) try
{
    mDataChanged = true;
		if(DP_INSTANCER_DEBUG)DOMMY_MAYA_DEBUG("DPInstancer", "DPInstancerOverride::populateGeometry");
		
		
	//data list
	//cvs
	MHWRender::MVertexBuffer* verticesBuffer = NULL;
	float* vertices = NULL;
	MHWRender::MVertexBuffer* normalBuffer = NULL;
	float* normals = NULL;
	
	
	
	// An empty, valid draw set has no GPU buffers to populate.
    if (dData.pointList.empty()) { mDataChanged = false; return; }

	// fill vertex buffer
	if(DP_INSTANCER_DEBUG)DOMMY_MAYA_DEBUG("DPInstancer", "descList count:" << requirements.vertexRequirements().length());
    const bool streamsReady = dommy::viewport::forEachVertexRequirement(
        requirements, [&](const MHWRender::MVertexBufferDescriptor& desc) -> bool {
            if(DP_INSTANCER_DEBUG)DOMMY_MAYA_DEBUG("DPInstancer", "semanticName:" << desc.semanticName().asChar());

            switch (desc.semantic())
            {
                case MHWRender::MGeometry::kPosition:
                {
                    if (!verticesBuffer)
                    {
                        verticesBuffer = data.createVertexBuffer(desc);
                        if (!verticesBuffer) return false;

                        if (verticesBuffer)
                            {
                                    vertices = dommy::viewport::acquireFloatBuffer(
                                        verticesBuffer, desc, dData.pointList.size(), 3);
                            }
                        }
                    }
                        break;
                case MHWRender::MGeometry::kNormal:
                {
                    if (!normalBuffer && !dData.normalList.empty())
                    {
                        normalBuffer = data.createVertexBuffer(desc);
                        if (!normalBuffer) return false;

                        if (normalBuffer)
                        {
                                normals = dommy::viewport::acquireFloatBuffer(
                                    normalBuffer, desc, dData.normalList.size(), 3);
                        }
                    }

                }
                    break;
                default:
                    // This stream semantic has no project buffer.
                    break;
            }
            return true;
        });
    if (!streamsReady) return;
	
if ((verticesBuffer && !vertices) || (normalBuffer && !normals)) return;

#pragma mark commit points
	
	if (vertices) {
		
		size_t i=0;
		
		for (auto fIdx=dData.pointList.begin(); fIdx!=dData.pointList.end(); fIdx++) {
			vertices[i++] = (*fIdx);
		}
		
		if(verticesBuffer && vertices)
		{
			verticesBuffer->commit(vertices);
		}
	}
	
	if (dData.normalList.size()) {
		if(normals)
		{
			size_t indexOffset =0;
			for(auto it=dData.normalList.begin(); it!=dData.normalList.end(); ++it)
			{
				normals[indexOffset++] = (*it);
			}
		}
		
		if(normalBuffer && normals)
		{
			normalBuffer->commit(normals);
		}
	}
	
	
#pragma mark index commit
	// fill index buffer
	for (int i = 0; i < renderItems.length(); ++i)
	{
			const MHWRender::MRenderItem* item = renderItems.itemAt(i);
			if (!item) continue;
			
					size_t numIndex = dData.indexList.size();
                    if (numIndex == 0U) continue;
				
				unsigned int* indices = NULL;
				MHWRender::MIndexBuffer* outIndexBuffer =
					dommy::viewport::createIndexBuffer(
						data, numIndex, indices);
				if (!outIndexBuffer) return;
			
			for(size_t c=0; c<numIndex; c++)
			{
				indices[c] = dData.indexList[c];
			}
			
				outIndexBuffer->commit(indices);
				if (!item->associateWithIndexBuffer(outIndexBuffer)) return;
		}
	
	mDataChanged = false;
}
DOMMY_VIEWPORT_VOID_CATCH("DPInstancerOverride", "populateGeometry")
