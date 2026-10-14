//
//  readDPointDataNode.cpp
//  dommyTool
//
//  Created by yuzo on 3/14/15.
//
//

#include "ReadDPointDataNode.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DMayaEntryPointGuard.h"



#include <fstream>
#include <vector>


#include "dpoint_ids.h"
#include "common/file/DLegacyTextData.h"

#include <maya/MDataHandle.h>
#include <maya/MFileObject.h>
#include <maya/MFnPointArrayData.h>
#include <maya/MFnStringData.h>
#include <maya/MFnTypedAttribute.h>
#include <maya/MGlobal.h>
#include <maya/MPointArray.h>

const MTypeId ReadDPointDataNode::typeId( ID_DP_READ );


MObject ReadDPointDataNode::filePath;
MObject ReadDPointDataNode::outputPointArray;


void *ReadDPointDataNode::creator() {
    return ::dommy::maya::createObject("ReadDPointDataNode", [&]() {
        return new ReadDPointDataNode();
    });
}


MStatus ReadDPointDataNode::compute( const MPlug& plug, MDataBlock& block ) try
{
	if (plug != outputPointArray) {
		return MS::kUnknownParameter;
	}

	MStatus stat;
	const MDataHandle pathHandle = block.inputValue(filePath, &stat);
	CHECK_MSTATUS_AND_RETURN_IT(stat);
	const MString rawPath = pathHandle.asString();
	MFileObject fileObject;
	DOMMY_RETURN_IF_FAILURE(fileObject.setRawFullName(rawPath));
	const MString fileName = fileObject.resolvedFullName();

	std::vector<dommy::file::Triple> decodedPoints;
	if (fileName.length()) {
		std::ifstream input(fileName.asChar());
		if (!input || !dommy::file::readPointData(input, decodedPoints)) {
			MGlobal::displayError("ReadDPointDataNode: failed to read valid point data.");
			return MS::kFailure;
		}
	}

	MPointArray points;
	for (std::size_t index = 0; index < decodedPoints.size(); ++index) {
		const dommy::file::Triple& point = decodedPoints[index];
		DOMMY_RETURN_IF_FAILURE(points.append(MPoint(point[0], point[1], point[2])));
	}
	if (points.length() != decodedPoints.size()) return MS::kFailure;

	MFnPointArrayData pointDataFn;
	MObject pointData = pointDataFn.create(points, &stat);
	CHECK_MSTATUS_AND_RETURN_IT(stat);
	if (pointData.isNull()) return MS::kFailure;
	MDataHandle outputData = block.outputValue(outputPointArray, &stat);
	CHECK_MSTATUS_AND_RETURN_IT(stat);
	DOMMY_RETURN_IF_FAILURE(outputData.set(pointData));
	return block.setClean(plug);
}
DOMMY_COMPUTE_CATCH("ReadDPointDataNode")




MStatus ReadDPointDataNode::initialize() try {
	MStatus stat;
	MFnTypedAttribute tAttr;
	
	
	MFnStringData fileFnStringData;
	MObject fileNameDefaultObject = fileFnStringData.create("", &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	filePath = tAttr.create("filePath", "fp", MFnData::kString, fileNameDefaultObject, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(true));
	DOMMY_RETURN_IF_FAILURE(tAttr.setUsedAsFilename(true));
	
    
    
    outputPointArray = tAttr.create( "outputPointArray", "opa", MFnPointArrayData::kPointArray, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);

    DOMMY_RETURN_IF_FAILURE(tAttr.setReadable(true));
    DOMMY_RETURN_IF_FAILURE(tAttr.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(true));
	
	
    MPointArray			defaultPoints;
    MFnPointArrayData	defaultArray;
    MObject				defaultAttr;
    
    DOMMY_RETURN_IF_FAILURE(defaultPoints.clear()); // Empty array
    defaultAttr = defaultArray.create (defaultPoints, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setDefault(defaultAttr));
	
	
	
	DOMMY_RETURN_IF_FAILURE(addAttribute(filePath));
	DOMMY_RETURN_IF_FAILURE(addAttribute(outputPointArray));
	
	DOMMY_RETURN_IF_FAILURE(attributeAffects( filePath, outputPointArray ));
	
	
	return MS::kSuccess;
}
catch (...) {
	return dommy::maya::reportEntryPointException(
		"ReadDPointDataNode", "initialize", std::current_exception());
}
