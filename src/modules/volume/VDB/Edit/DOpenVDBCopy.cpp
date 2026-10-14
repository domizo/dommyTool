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
// Source comparison reference: https://github.com/AcademySoftwareFoundation/openvdb/blob/a79df2f32b8ebb5d15fd73325c46cee845a81e49/openvdb_maya/maya/OpenVDBCopyNode.cc

//
//  DOpenVDBCopy.cpp
//  dommyTool
//
//  Created by yuzo on 1/1/17.
//
//

#include "common/utility/DMayaEntryPointGuard.h"
#include "DOpenVDBCopy.h"
#include "common/utility/DMayaPluginDataOutput.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DStatusOperation.h"

#include <boost/math/constants/constants.hpp>

#include "DMayaHeaders.h"

#include "DOpenVDBData.h"
#include "DOpenVDBUtil.h"
#include "../Common/DVDBComputeGuard.h"

#include "dvdb_ids.h"

const MTypeId DOpenVDBCopy::typeId(ID_VDB_DOPENVDB_COPY);

MObject DOpenVDBCopy::aVdbInputA;
MObject DOpenVDBCopy::aVdbInputB;
MObject DOpenVDBCopy::aVdbSelectedGridNamesA;
MObject DOpenVDBCopy::aVdbSelectedGridNamesB;
MObject DOpenVDBCopy::aVdbOutput;

void* DOpenVDBCopy::creator() {
    return ::dommy::maya::createObject("DOpenVDBCopy", [&]() {
        return new DOpenVDBCopy();
    });
}

MStatus DOpenVDBCopy::compute(const MPlug& plug, MDataBlock& data) try {
    if (plug != aVdbOutput) {
        return MS::kUnknownParameter;
    }

    const DOpenVDBData* inputVdbA = getInputVDB(aVdbInputA, data);
    const DOpenVDBData* inputVdbB = getInputVDB(aVdbInputB, data);

    MStatus stat;
    MFnPluginData pluginData;
    pluginData.create(DOpenVDBData::id, &stat);
    if (stat != MS::kSuccess) {
        return stat;
    }

    DOpenVDBData* outputVdb = nullptr;
    stat = dommy::maya::getPluginData(pluginData, outputVdb);
    if (!stat)
        return stat;
    if (stat != MS::kSuccess || !outputVdb) {
        return MS::kFailure;
    }

    MDataHandle output = data.outputValue(aVdbOutput, &stat);
    if (stat != MS::kSuccess) {
        return stat;
    }

    if (inputVdbA) {

        MDataHandle selectionHandle = data.inputValue(aVdbSelectedGridNamesA, &stat);
        if (stat != MS::kSuccess) {
            return stat;
        }
        const std::string selectionStr = selectionHandle.asString().asChar();

        GridCPtrVec grids;
        getSelectedGrids(grids, selectionStr, *inputVdbA);

        for (GridCPtrVecIter it = grids.begin(); it != grids.end(); ++it) {
            outputVdb->insert((*it)->copyGrid()); // shallow copy
        }
    }

    if (inputVdbB) {

        MDataHandle selectionHandle = data.inputValue(aVdbSelectedGridNamesB, &stat);
        if (stat != MS::kSuccess) {
            return stat;
        }
        const std::string selectionStr = selectionHandle.asString().asChar();

        GridCPtrVec grids;
        getSelectedGrids(grids, selectionStr, *inputVdbB);

        for (GridCPtrVecIter it = grids.begin(); it != grids.end(); ++it) {
            outputVdb->insert((*it)->copyGrid()); // shallow copy
        }
    }

    stat = dommy::maya::publishTypedPluginDataObject<DOpenVDBData>(output, pluginData.object(),
                                                                   DOpenVDBData::id);
    if (stat != MS::kSuccess) {
        return stat;
    }
    return data.setClean(plug);
}
DOMMY_VDB_COMPUTE_CATCH("DOpenVDBCopy")

MStatus DOpenVDBCopy::initialize() try {
    MStatus stat;

    // attributes

    MFnTypedAttribute tAttr;
    MFnStringData strData;

    const MObject aVdbSelectedGridNamesADefault = strData.create("*", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aVdbSelectedGridNamesA = tAttr.create("GridsFromA", "anames", MFnData::kString,
                                          aVdbSelectedGridNamesADefault, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);

    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    stat = addAttribute(aVdbSelectedGridNamesA);
    DOMMY_RETURN_IF_FAILURE(stat);

    const MObject aVdbSelectedGridNamesBDefault = strData.create("*", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aVdbSelectedGridNamesB = tAttr.create("GridsFromB", "bnames", MFnData::kString,
                                          aVdbSelectedGridNamesBDefault, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);

    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    stat = addAttribute(aVdbSelectedGridNamesB);
    DOMMY_RETURN_IF_FAILURE(stat);

    // input / output

    aVdbInputA = tAttr.create("VdbInputA", "a", DOpenVDBData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);

    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(true));
    stat = addAttribute(aVdbInputA);
    DOMMY_RETURN_IF_FAILURE(stat);

    aVdbInputB = tAttr.create("VdbInputB", "b", DOpenVDBData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);

    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(true));
    stat = addAttribute(aVdbInputB);
    DOMMY_RETURN_IF_FAILURE(stat);

    aVdbOutput = tAttr.create("VdbOutput", "vdboutput", DOpenVDBData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);

    DOMMY_RETURN_IF_FAILURE(tAttr.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    stat = addAttribute(aVdbOutput);
    DOMMY_RETURN_IF_FAILURE(stat);

    // attribute dependencies

    stat = attributeAffects(aVdbInputA, aVdbOutput);
    DOMMY_RETURN_IF_FAILURE(stat);

    stat = attributeAffects(aVdbInputB, aVdbOutput);
    DOMMY_RETURN_IF_FAILURE(stat);

    stat = attributeAffects(aVdbSelectedGridNamesA, aVdbOutput);
    DOMMY_RETURN_IF_FAILURE(stat);

    stat = attributeAffects(aVdbSelectedGridNamesB, aVdbOutput);
    DOMMY_RETURN_IF_FAILURE(stat);

    return MS::kSuccess;
} catch (...) {
    return dommy::maya::reportEntryPointException("DVDBCopy", "initialize",
                                                  std::current_exception());
}
