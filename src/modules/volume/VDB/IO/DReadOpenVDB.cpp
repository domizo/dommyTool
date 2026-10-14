//
//  DReadOpenVDB.cpp
//  dommyTool
//
//  Created by yuzo on 4/16/16.
//
//

#include "DReadOpenVDB.h"
#include "common/utility/DMayaEntryPointGuard.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DMayaPluginDataOutput.h"

#include "DOpenVDBData.h"
#include <openvdb/io/Stream.h>

#include <maya/MFnTypedAttribute.h>
#include <maya/MFnStringData.h>
#include <maya/MFnNumericAttribute.h>
#include <maya/MFnUnitAttribute.h>

#include <fstream>
#include <maya/MFileObject.h>

#include <maya/MGlobal.h>
#include <maya/MTime.h>

#include <exception>


#include "DFileOperation.h"
#include "common/file/DFileSize.h"

#include "dvdb_ids.h"
#include "../Common/DOpenVDBLimits.h"
#include "../Common/DVDBComputeGuard.h"

const MTypeId DReadOpenVDB::typeId( ID_VDB_DOPENVDB_READ );


MObject DReadOpenVDB::offsetTime;
MObject DReadOpenVDB::aTime;

MObject DReadOpenVDB::aVdbFilePath;
MObject DReadOpenVDB::aVdbOutput;



void* DReadOpenVDB::creator()
{
    return ::dommy::maya::createObject("DReadOpenVDB", [&]() {
        return new DReadOpenVDB();
    });
}




MStatus DReadOpenVDB::initialize()
try {
	MStatus stat;
	MFnTypedAttribute tAttr;
	
	MFnStringData fnStringData;
	MObject defaultStringData = fnStringData.create("", &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	
    MFnNumericAttribute nAttr;
	// Setup the input attributes
    
    
    offsetTime = nAttr.create("offsetTime", "oft", MFnNumericData::kInt, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(offsetTime));

    
	
	aVdbFilePath = tAttr.create("VdbFilePath", "file", MFnData::kString, defaultStringData, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	
	DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aVdbFilePath));
	
	// Setup the output attributes
	
	aVdbOutput = tAttr.create("VdbOutput", "vdb", DOpenVDBData::id, MObject::kNullObj, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	
	DOMMY_RETURN_IF_FAILURE(tAttr.setWritable(false));
	DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aVdbOutput));
    
    
    MFnUnitAttribute uniAttr;
    aTime = uniAttr.create("CurrentTime", "time", MFnUnitAttribute::kTime,  0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(addAttribute(aTime));
	
	
	
	// Set the attribute dependencies
    DOMMY_RETURN_IF_FAILURE(attributeAffects(offsetTime, aVdbOutput));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aTime, aVdbOutput));
	DOMMY_RETURN_IF_FAILURE(attributeAffects(aVdbFilePath, aVdbOutput));
	
	
	return MS::kSuccess;
}
catch (...) {
    return dommy::maya::reportEntryPointException(
        "DReadOpenVDB", "initialize", std::current_exception());
}


////////////////////////////////////////


MStatus DReadOpenVDB::compute(const MPlug& plug, MDataBlock& data) try
{
	if (plug != aVdbOutput)
		return MS::kUnknownParameter;

	MStatus stat;
	const MDataHandle timeHandle = data.inputValue(aTime, &stat);
	if (stat != MS::kSuccess)
		return stat;
	const MTime evaluationTime = timeHandle.asTime();
	const MDataHandle filePathHandle = data.inputValue(aVdbFilePath, &stat);
	if (stat != MS::kSuccess)
		return stat;
	const MString sourcePath = filePathHandle.asString();
	std::string filename;
	if (sourcePath.length() != 0U) {
		const MDataHandle offsetHandle = data.inputValue(offsetTime, &stat);
		DOMMY_RETURN_IF_FAILURE(stat);
		const int offsetValue = offsetHandle.asInt();
		MString resolvedPath;
		if (!getSequenceFileName(sourcePath,
				evaluationTime.value() + static_cast<double>(offsetValue), resolvedPath)) {
			MGlobal::displayError("OpenVDB input path contains an invalid frame token.");
			return MS::kFailure;
		}
		filename = resolvedPath.asChar();
	}

	// Stage before publication: admission/construction failures retain the old
	// output. Missing/unreadable files still produce valid empty data.
	DOMMY_RETURN_IF_FAILURE(dommy::maya::setPluginDataOutput<DOpenVDBData>(
		data, aVdbOutput, DOpenVDBData::id, [&](DOpenVDBData& outputVdb) -> MStatus {
			if (sourcePath.length() == 0U) {
				MGlobal::displayWarning("OpenVDB input path is empty; returning empty VDB data.");
				return MS::kSuccess;
			}
			MFileObject fileObject;
			DOMMY_RETURN_IF_FAILURE(fileObject.setRawFullName(filename.c_str()));
			if (!fileObject.exists()) {
				MGlobal::displayWarning(
					MString("OpenVDB file does not exist; returning empty VDB data: ") +
					filename.c_str());
				return MS::kSuccess;
			}
			if (!dommy::file::fileSizeWithinLimit(filename,
					static_cast<unsigned long long>(dommy::vdb::kMaxVdbPayloadBytes))) {
				MGlobal::displayWarning(
					"OpenVDB input is not a regular file or exceeds the file-size limit; "
					"returning empty VDB data.");
				return MS::kSuccess;
			}
			try {
				std::ifstream input(filename.c_str(), std::ios_base::binary);
				if (!input) {
					MGlobal::displayWarning(
						MString("Unable to open OpenVDB file; returning empty VDB data: ") +
						filename.c_str());
					return MS::kSuccess;
				}
				openvdb::GridPtrVecPtr grids = openvdb::io::Stream(input).getGrids();
				if (grids)
					outputVdb.insert(*grids);
			} catch (const openvdb::Exception& exception) {
				MGlobal::displayWarning(
					MString("Failed to read OpenVDB data; returning empty data: ") + exception.what());
			} catch (const std::exception& exception) {
				MGlobal::displayWarning(
					MString("Failed to read OpenVDB data; returning empty data: ") + exception.what());
			} catch (...) {
				MGlobal::displayWarning(
					"Failed to read OpenVDB data because of an unknown error; returning empty data.");
			}
			return MS::kSuccess;
		}));

	return data.setClean(plug);
}
DOMMY_VDB_COMPUTE_CATCH("DReadOpenVDB")
