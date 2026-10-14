//
//  applyVtxColorFromFileNode.cpp
//  dommyTool
//
//  Created by yuzo on 5/29/15.
//
//

#include "common/utility/DMayaMeshColorOutput.h"
#include "applyVtxColorFromFileNode.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DNumericValidation.h"
#include "common/utility/DGenerationLimits.h"
#include "common/utility/DMayaEntryPointGuard.h"
#include "common/file/DLegacyTextData.h"
#include "common/file/DSequencePath.h"


#include <fstream>
#include <string>
#include <vector>

#include <maya/MColorArray.h>
#include <maya/MDataHandle.h>
#include <maya/MFnGenericAttribute.h>
#include <maya/MFnMesh.h>
#include <maya/MFnMeshData.h>
#include <maya/MFnNumericAttribute.h>
#include <maya/MFnStringData.h>
#include <maya/MFnTypedAttribute.h>
#include <maya/MGlobal.h>
#include <maya/MIntArray.h>
#include <maya/MStringArray.h>



const MTypeId applyVtxColorFromFileNode::typeId( 0x4001A );



MObject applyVtxColorFromFileNode::inputMesh;

MObject applyVtxColorFromFileNode::filePath;

MObject applyVtxColorFromFileNode::frame;
MObject applyVtxColorFromFileNode::offset;
MObject applyVtxColorFromFileNode::padding;

MObject applyVtxColorFromFileNode::outputMesh;



void *applyVtxColorFromFileNode::creator() {
    return ::dommy::maya::createObject("applyVtxColorFromFileNode", [&]() {
        return new applyVtxColorFromFileNode();
    });
}



MStatus applyVtxColorFromFileNode::compute( const MPlug& plug, MDataBlock& block ) try
{
	if (plug != outputMesh) {
		return MS::kUnknownParameter;
	}

	MStatus stat;
	MDataHandle meshHnd = block.inputValue(inputMesh, &stat);
	CHECK_MSTATUS_AND_RETURN_IT(stat);
	if (meshHnd.type() != MFnData::kMesh || meshHnd.data().isNull()) {
		return MS::kFailure;
	}

	const MObject inputMeshObject = meshHnd.data();
	MFnMesh inMeshFn(inputMeshObject, &stat);
	CHECK_MSTATUS_AND_RETURN_IT(stat);
	MFnMeshData meshDataFn;
	MObject newOutMeshData = meshDataFn.create(&stat);
	CHECK_MSTATUS_AND_RETURN_IT(stat);
	if (newOutMeshData.isNull()) return MS::kFailure;
	MObject copiedMesh = inMeshFn.copy(inputMeshObject, newOutMeshData, &stat);
	CHECK_MSTATUS_AND_RETURN_IT(stat);
	if (copiedMesh.isNull()) return MS::kFailure;
	MDataHandle outputMeshHnd = block.outputValue(outputMesh, &stat);
	CHECK_MSTATUS_AND_RETURN_IT(stat);

	const MDataHandle pathHandle = block.inputValue(filePath, &stat);
	CHECK_MSTATUS_AND_RETURN_IT(stat);
	std::string fileName = pathHandle.asString().asChar();
	const MDataHandle frameHandle = block.inputValue(frame, &stat);
	CHECK_MSTATUS_AND_RETURN_IT(stat);
	const MDataHandle offsetHandle = block.inputValue(offset, &stat);
	CHECK_MSTATUS_AND_RETURN_IT(stat);
	const MDataHandle paddingHandle = block.inputValue(padding, &stat);
	CHECK_MSTATUS_AND_RETURN_IT(stat);
	const long long frameValue =
		static_cast<long long>(frameHandle.asInt()) +
		static_cast<long long>(offsetHandle.asInt());
	const int paddingValue = paddingHandle.asInt();
	if (paddingValue < 0 || !dommy::file::replaceFrameToken(
	        fileName, frameValue, static_cast<unsigned int>(paddingValue), fileName)) {
		MGlobal::displayError("applyVtxColorFromFileNode: invalid sequence path settings.");
		return MS::kFailure;
	}

	if (fileName.empty()) {
		DOMMY_RETURN_IF_FAILURE(outputMeshHnd.set(newOutMeshData));
		outputMeshHnd.setClean();
		return block.setClean(plug);
	}

	std::ifstream input(fileName.c_str());
	std::vector<dommy::file::Triple> colors;
	if (!input || !dommy::file::readVertexColors(input, colors)) {
		MGlobal::displayError("applyVtxColorFromFileNode: failed to read valid vertex color data.");
		return MS::kFailure;
	}

	MFnMesh newMeshFn(copiedMesh, &stat);
	CHECK_MSTATUS_AND_RETURN_IT(stat);
	const int vertexCount = newMeshFn.numVertices(&stat);
	CHECK_MSTATUS_AND_RETURN_IT(stat);
	if (vertexCount < 0 || static_cast<std::size_t>(vertexCount) != colors.size()) {
		MGlobal::displayError("applyVtxColorFromFileNode: color count does not match the mesh vertex count.");
		return MS::kFailure;
	}

	MColorArray colDataArray;
	for (int index = 0; index < vertexCount; ++index) {
		const dommy::file::Triple& color = colors[static_cast<std::size_t>(index)];
		float red = 0.0f, green = 0.0f, blue = 0.0f;
		if (!dommy::numeric::checkedFloatCast(color[0], red) ||
			!dommy::numeric::checkedFloatCast(color[1], green) ||
			!dommy::numeric::checkedFloatCast(color[2], blue)) {
			MGlobal::displayError("applyVtxColorFromFileNode: color exceeds the finite float range.");
			return MS::kInvalidParameter;
		}
		DOMMY_RETURN_IF_FAILURE(colDataArray.append(MColor(red, green, blue, 1.0f)));
	}
	if (colDataArray.length() != colors.size()) return MS::kFailure;

	DOMMY_RETURN_IF_FAILURE(dommy::maya::setMeshDataVertexColors(
        newMeshFn, colDataArray, MString("colorSet1")));
	DOMMY_RETURN_IF_FAILURE(outputMeshHnd.set(newOutMeshData));
	outputMeshHnd.setClean();
	return block.setClean(plug);
}
DOMMY_COMPUTE_CATCH("applyVtxColorFromFileNode")




MStatus applyVtxColorFromFileNode::initialize() try {
	MStatus stat;
	MFnNumericAttribute nAttr;
	MFnGenericAttribute gAttr;
	MFnTypedAttribute tAttr;
	
	
	inputMesh= tAttr.create( "inputMesh", "imo",MFnData::kMesh, MObject::kNullObj, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
	
	
	MFnStringData fileFnStringData;
	MObject fileNameDefaultObject = fileFnStringData.create("", &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	filePath = tAttr.create("filePath", "fp", MFnData::kString, fileNameDefaultObject, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(true));
	DOMMY_RETURN_IF_FAILURE(tAttr.setUsedAsFilename(true));
	
		
	frame  = nAttr.create( "frame", "fra", MFnNumericData::kInt, 1, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
	DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
	
	offset  = nAttr.create( "offset", "off", MFnNumericData::kInt, 0, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
	DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
	
	
	padding  = nAttr.create( "padding", "pad", MFnNumericData::kInt, 4, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0));
	DOMMY_RETURN_IF_FAILURE(nAttr.setMax(10));
	DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
	DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
	
	
	
	
	outputMesh = tAttr.create( "outputMesh", "opm",MFnData::kMesh, MObject::kNullObj, &stat );
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(tAttr.setHidden( true ));
	DOMMY_RETURN_IF_FAILURE(tAttr.setStorable( false ));
	
	
	
	
	DOMMY_RETURN_IF_FAILURE(addAttribute(inputMesh));
	DOMMY_RETURN_IF_FAILURE(addAttribute(filePath));
	DOMMY_RETURN_IF_FAILURE(addAttribute(frame));
	DOMMY_RETURN_IF_FAILURE(addAttribute(offset));
	DOMMY_RETURN_IF_FAILURE(addAttribute(padding));
	DOMMY_RETURN_IF_FAILURE(addAttribute(outputMesh));
	
	DOMMY_RETURN_IF_FAILURE(attributeAffects(inputMesh, outputMesh));
	DOMMY_RETURN_IF_FAILURE(attributeAffects(filePath, outputMesh));
	DOMMY_RETURN_IF_FAILURE(attributeAffects(frame, outputMesh));
	DOMMY_RETURN_IF_FAILURE(attributeAffects(offset, outputMesh));
	DOMMY_RETURN_IF_FAILURE(attributeAffects(padding, outputMesh));
	
	return MS::kSuccess;
}
catch (...) {
	return dommy::maya::reportEntryPointException(
		"applyVtxColorFromFileNode", "initialize", std::current_exception());
}
