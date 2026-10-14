// DOMMY_THIRD_PARTY_NOTICE_BEGIN
///////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2012-2018 DreamWorks Animation LLC
//
// All rights reserved. This software is distributed under the
// Mozilla Public License 2.0 ( http://www.mozilla.org/MPL/2.0/ )
//
// Redistributions of source code must retain the above copyright
// and license notice and the following restrictions and disclaimer.
//
// *     Neither the name of DreamWorks Animation nor the names of
// its contributors may be used to endorse or promote products derived
// from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
// IN NO EVENT SHALL THE COPYRIGHT HOLDERS' AND CONTRIBUTORS' AGGREGATE
// LIABILITY FOR ALL CLAIMS REGARDLESS OF THEIR BASIS EXCEED US$250.00.
//
///////////////////////////////////////////////////////////////////////////
// DOMMY_THIRD_PARTY_NOTICE_END
// Modified for dommyTool; not the original upstream version.
// Source comparison reference: https://github.com/AcademySoftwareFoundation/openvdb/blob/a79df2f32b8ebb5d15fd73325c46cee845a81e49/openvdb_houdini/houdini/SOP_OpenVDB_Write.cc

//
//  DWriteOpenVDB.cpp
//  dommyTool
//
//  Created by yuzo on 4/17/16.
//
//

#include "DWriteOpenVDB.h"
#include "common/utility/DMayaEntryPointGuard.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DMayaPluginDataOutput.h"



#include "DMayaHeaders.h"

#include <openvdb/io/Stream.h>
#include <openvdb/math/Math.h>


#include "DFileOperation.h"
#include "common/file/DAtomicFile.h"
#include "common/file/DExportPath.h"
#include "common/file/DFileSize.h"
#include "common/utility/DMayaPluginData.h"

#include "DOpenVDBData.h"
#include "../Common/DOpenVDBLimits.h"
#include "../Common/DVDBTimeMetadata.h"
#include "DOpenVDBUtil.h"

#include "dvdb_ids.h"
#include "../Common/DVDBComputeGuard.h"

#include <exception>

const MTypeId DWriteOpenVDB::typeId( ID_VDB_DOPENVDB_WRITE );

MObject DWriteOpenVDB::aVdbInput;

MObject DWriteOpenVDB::aVdbFilePath;
MObject DWriteOpenVDB::aWriteFile;
MObject DWriteOpenVDB::aFrameNumbering;
MObject DWriteOpenVDB::aCompression;
MObject DWriteOpenVDB::aInputTime;

MObject DWriteOpenVDB::aNodeInfo;

MObject DWriteOpenVDB::aVdbOutput;

void* DWriteOpenVDB::creator()
{
    return ::dommy::maya::createObject("DWriteOpenVDB", [&]() {
        return new DWriteOpenVDB();
    });
}



MStatus DWriteOpenVDB::compute(const MPlug& plug, MDataBlock& data) try
{

	if (plug == aVdbOutput || plug == aNodeInfo) {
		try {
			MStatus stat;

        const MDataHandle writeFileHandle = data.inputValue(aWriteFile, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        const bool writeFile = writeFileHandle.asBool();
		if (!writeFile) {
			MDataHandle inputHandle =
				data.inputValue(aVdbInput, &stat);
			if (stat != MS::kSuccess) return stat;
			const DOpenVDBData* inputVdb = nullptr;
			stat = dommy::maya::getPluginDataFromHandle(
				inputHandle, DOpenVDBData::id, inputVdb);
			if (stat != MS::kSuccess || inputVdb == nullptr) {
				return stat != MS::kSuccess ? stat : MStatus(MS::kFailure);
			}

			MFnPluginData outputCreator;
			DOpenVDBData* outputVdb = nullptr;
			stat = dommy::maya::createPluginData(
				outputCreator, DOpenVDBData::id, outputVdb);
			if (stat != MS::kSuccess || outputVdb == nullptr) {
				return stat != MS::kSuccess ? stat : MStatus(MS::kFailure);
			}
            DOMMY_RETURN_IF_FAILURE(outputVdb->copyFrom(*inputVdb));
			MDataHandle outputHandle =
				data.outputValue(aVdbOutput, &stat);
			if (stat != MS::kSuccess) return stat;
			stat =
				dommy::maya::publishTypedPluginDataObject<DOpenVDBData>(
					outputHandle, outputCreator.object(), DOpenVDBData::id);
			if (stat != MS::kSuccess) return stat;
			outputHandle.setClean();

			std::stringstream disabledInfo;
			disabledInfo << "File writing is disabled. Enable WriteFile "
						 << "to perform an explicit export.\n";
            DOMMY_RETURN_IF_FAILURE(updateNodeInfo(disabledInfo, data, aNodeInfo));
			return data.setClean(plug);
		}
        
        const MDataHandle numberingHandle = data.inputValue(aFrameNumbering, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        const int numberingScheme = numberingHandle.asInt();
		
		//get filepath
		MDataHandle filePathHandle = data.inputValue(aVdbFilePath, &stat);
		if (stat != MS::kSuccess) return stat;
		
		
		const MString srcFileName = filePathHandle.asString();
		if (srcFileName.length()==0) {
			MGlobal::displayWarning("DWriteOpenVDB output path is empty.");
			return MS::kFailure;
		}
        
        const MDataHandle timeHandle = data.inputValue(aInputTime, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        const MTime time = timeHandle.asTime();
        std::string filename = srcFileName.asChar();
        if (!insertFrameNumber(filename, time, numberingScheme)) {
            MGlobal::displayError(
                "DWriteOpenVDB output path or frame-numbering settings are invalid.");
            return MS::kFailure;
        }
		if (!dommy::file::isSafeExportPath(filename, ".vdb")) {
			MGlobal::displayError(
				"DWriteOpenVDB requires a bounded .vdb path without "
				"control characters or '..' traversal components.");
			return MS::kInvalidParameter;
		}
        dommy::vdb::TimeMetadata timeMetadata;
        const MTime oneSecond(1.0, MTime::kSeconds);
        if (!dommy::vdb::checkedTimeMetadata(
                time.as(MTime::uiUnit()), oneSecond.as(MTime::uiUnit()),
                timeMetadata)) {
            MGlobal::displayError(
                "DWriteOpenVDB time metadata exceeds the supported Int32 range.");
            return MS::kInvalidParameter;
        }

        std::stringstream infoStr;
        infoStr << "File: " << filename << "\n";
		
		
		MDataHandle inputVdbHandle = data.inputValue(aVdbInput, &stat);
		if (stat != MS::kSuccess) return stat;
		const DOpenVDBData* vdb = nullptr;
		stat = dommy::maya::getPluginDataFromHandle(
			inputVdbHandle, DOpenVDBData::id, vdb);
		if (stat != MS::kSuccess || vdb == nullptr) {
			MGlobal::displayWarning(
				"DWriteOpenVDB received incompatible or empty input data.");
			return stat != MS::kSuccess ? stat : MStatus(MS::kFailure);
		}

				// The output object belongs to Maya and must remain distinct from the
				// input MPxData object, even though the grids themselves are shared.
				MFnPluginData outputDataCreator;
				DOpenVDBData* outputVdb = nullptr;
				stat = dommy::maya::createPluginData(
					outputDataCreator, DOpenVDBData::id, outputVdb);
				if (stat != MS::kSuccess || outputVdb == nullptr)
					return stat != MS::kSuccess
						? stat : MStatus(MS::kFailure);
                DOMMY_RETURN_IF_FAILURE(outputVdb->copyFrom(*vdb));
				MDataHandle outHandle = data.outputValue(aVdbOutput, &stat);
				if (stat != MS::kSuccess)
					return stat;
				DOMMY_RETURN_IF_FAILURE(
					dommy::maya::publishTypedPluginDataObject<DOpenVDBData>(
						outHandle, outputDataCreator.object(),
						DOpenVDBData::id));
				
				// Add file-level metadata.
				openvdb::MetaMap outMeta;
				outMeta.insertMeta("creator", openvdb::StringMetadata("Maya/DWriteOpenVDB"));
                
                outMeta.insertMeta("frame", openvdb::DoubleMetadata(timeMetadata.frame));
                outMeta.insertMeta("tick", openvdb::Int32Metadata(timeMetadata.tick));
                outMeta.insertMeta("frames_per_second",
                    openvdb::Int32Metadata(timeMetadata.framesPerSecond));
                outMeta.insertMeta("ticks_per_frame",
                    openvdb::Int32Metadata(timeMetadata.ticksPerFrame));
                outMeta.insertMeta("ticks_per_second", openvdb::Int32Metadata(6000));
                
				
				
				// Set of names of grids that the user selected for both 16-bit
				// and full-precision conversion
				StringSet conflicts;
				
				// Collect pointers to grids from VDB primitives found in the geometry.
				openvdb::GridPtrSet outGrids;
				GridCPtrVec grids;
				getGrids(grids, *vdb, "*");
				
				for (GridCPtrVecIter it = grids.begin(); it != grids.end(); ++it) {
					
//					const openvdb::GridBase& gridRef = **it;
					
					GridPtr grid = openvdb::ConstPtrCast<Grid>((*it)->copyGrid());
					grid->removeMeta("is_vdb");
				
//					const std::string gridName = gridRef.getName();
					
					outGrids.insert(grid);
				}
				
				if (outGrids.empty()) {
					MGlobal::displayWarning( ("No grids were written to " + filename).c_str());
				}
				
				reportFloatPrecisionConflicts(conflicts);
				
				
                const MDataHandle compressionHandle = data.inputValue(aCompression, &stat);
                DOMMY_RETURN_IF_FAILURE(stat);
                const short compressionType = compressionHandle.asShort();
                if (compressionType < 0 || compressionType > 2) return MS::kInvalidParameter;
				
				
				// Write completely to an exclusively reserved sibling before
				// replacing the destination.
				dommy::file::PendingOutputFile pending(filename,
                    dommy::file::PendingOutputFile::Access::TrustedPath);
				openvdb::io::File file(pending.trustedTemporaryPath());
				
				
				
				
				uint32_t compressionFlags = file.compression();
				if (compressionType == 0) {
					compressionFlags &= ~(openvdb::io::COMPRESS_ZIP | openvdb::io::COMPRESS_BLOSC);
				} else if (compressionType == 2) {
					compressionFlags &= ~openvdb::io::COMPRESS_ZIP;
					compressionFlags |= openvdb::io::COMPRESS_BLOSC;
				} else if (compressionType == 1) {
					compressionFlags |= openvdb::io::COMPRESS_ZIP;
					compressionFlags &= ~openvdb::io::COMPRESS_BLOSC;
				}
				
				file.setCompression(compressionFlags);
				
				file.write(outGrids, outMeta);
				
//				vdb->write(file, outMeta);
				
				//file.write(vdb->grids(), outMeta);
				file.close();
				if (!dommy::file::fileSizeWithinLimit(
						pending.trustedTemporaryPath(),
						static_cast<unsigned long long>(
							dommy::vdb::kMaxVdbPayloadBytes))) {
					throw std::length_error(
						"DWriteOpenVDB output exceeds the file-size limit");
				}
				if (!pending.commit()) {
					throw std::runtime_error(
						"DWriteOpenVDB could not commit its temporary output");
				}
				
			infoStr << "Frame: " << timeMetadata.frame << "\n";
			printGridInfo(infoStr, *vdb);
            DOMMY_RETURN_IF_FAILURE(updateNodeInfo(infoStr, data, aNodeInfo));
			return data.setClean(plug);
		} catch (const openvdb::Exception& exception) {
			MGlobal::displayError(MString("DWriteOpenVDB failed: ") + exception.what());
			return MS::kFailure;
		} catch (const std::exception& exception) {
			MGlobal::displayError(MString("DWriteOpenVDB failed: ") + exception.what());
			return MS::kFailure;
		} catch (...) {
			MGlobal::displayError("DWriteOpenVDB failed because of an unknown error.");
			return MS::kFailure;
		}
	}
	
	return MS::kUnknownParameter;
}
DOMMY_VDB_COMPUTE_CATCH("DWriteOpenVDB")




MStatus DWriteOpenVDB::initialize()
try {
	MStatus stat;
	MFnTypedAttribute tAttr;
	MFnNumericAttribute nAttr;
	
	MFnStringData fnStringData;
	MObject defaultStringData = fnStringData.create("volume.vdb", &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	
	// Setup the input attributes
	
	aVdbFilePath = tAttr.create("VdbFilePath", "file", MFnData::kString, defaultStringData, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aVdbFilePath));

	aWriteFile = nAttr.create(
		"WriteFile", "wrt", MFnNumericData::kBoolean,
		false, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(false));
	DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(false));
	DOMMY_RETURN_IF_FAILURE(nAttr.setConnectable(false));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aWriteFile));
    
    MFnEnumAttribute eAttr;
    aFrameNumbering = eAttr.create("FrameNumbering", "numbering", 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(eAttr.addField("Frame.SubTick", 0));
    DOMMY_RETURN_IF_FAILURE(eAttr.addField("Fractional frame values", 1));
    DOMMY_RETURN_IF_FAILURE(eAttr.addField("Global ticks", 2));
    DOMMY_RETURN_IF_FAILURE(eAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aFrameNumbering));
	
	
	MFnEnumAttribute compAttr;
	aCompression = compAttr.create("Compression", "cmpr", 2, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(compAttr.addField("None", 0));
	DOMMY_RETURN_IF_FAILURE(compAttr.addField("Zip", 1));
	DOMMY_RETURN_IF_FAILURE(compAttr.addField("Blosc", 2));
	DOMMY_RETURN_IF_FAILURE(compAttr.setConnectable(false));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aCompression));

	
	
	
    MFnUnitAttribute unitAttr;
    aInputTime = unitAttr.create("inputTime", "int", MTime(0.0, MTime::kFilm), &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(unitAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(unitAttr.setReadable(true));
    DOMMY_RETURN_IF_FAILURE(unitAttr.setWritable(true));
    DOMMY_RETURN_IF_FAILURE(unitAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aInputTime));
	
	
	
	
    
    
	aVdbInput = tAttr.create("VdbInput", "vdbinput", DOpenVDBData::id, MObject::kNullObj, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(true));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aVdbInput));
	
	// Setup the output attributes
	
	aVdbOutput = tAttr.create("VdbOutput", "vdboutput", DOpenVDBData::id, MObject::kNullObj, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(tAttr.setWritable(false));
	DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aVdbOutput));
	
    MObject emptyStr = fnStringData.create("", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aNodeInfo = tAttr.create("NodeInfo", "info", MFnData::kString, emptyStr, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aNodeInfo));
	
	// Set the attribute dependencies
	
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aVdbFilePath, aVdbOutput));
	DOMMY_RETURN_IF_FAILURE(attributeAffects(aWriteFile, aVdbOutput));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aFrameNumbering, aVdbOutput));
	DOMMY_RETURN_IF_FAILURE(attributeAffects(aCompression, aVdbOutput));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aInputTime, aVdbOutput));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aVdbInput, aVdbOutput));
    
    
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aVdbFilePath, aNodeInfo));
	DOMMY_RETURN_IF_FAILURE(attributeAffects(aWriteFile, aNodeInfo));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aFrameNumbering, aNodeInfo));
	DOMMY_RETURN_IF_FAILURE(attributeAffects(aCompression, aNodeInfo));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aInputTime, aNodeInfo));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aVdbInput, aNodeInfo));
    
    
	return MS::kSuccess;
}
catch (...) {
    return dommy::maya::reportEntryPointException(
        "DWriteOpenVDB", "initialize", std::current_exception());
}


void DWriteOpenVDB::reportFloatPrecisionConflicts(const StringSet& conflicts)
{
	if (conflicts.empty()) return;
	
	std::ostringstream ostr;
	if (conflicts.size() == 1) {
		ostr << "For grid \"" << *conflicts.begin() << "\"";
	} else {
		StringSet::const_iterator i = conflicts.begin(), e = conflicts.end();
		ostr << "For grids \"" << *i << "\"";
		// Join grid names into a string of the form "grid1, grid2 and grid3".
		size_t count = conflicts.size(), n = 1;
		for (++i; i != e; ++i, ++n) {
			if (n + 1 < count) ostr << ", "; else ostr << " and ";
			ostr << "\"" << *i << "\"";
		}
	}
	ostr << ", specify either 16-bit output or full-precision output"
	<< " or neither, but not both.";
	
	// Word wrap the message at 60 columns and indent the first line.
	const std::string prefix(20, '#');
	MGlobal::displayInfo(prefix.c_str());
	MGlobal::displayInfo(ostr.str().c_str());
	
}
