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
// Source comparison reference: https://github.com/AcademySoftwareFoundation/openvdb/blob/a79df2f32b8ebb5d15fd73325c46cee845a81e49/openvdb_houdini/houdini/SOP_OpenVDB_Vector_Split.cc

//
//  DOpenVDBVectorSplit.cpp
//  dommyTool
//
//  Created by yuzo on 1/7/17.
//
//

#include "common/utility/DMayaEntryPointGuard.h"
#include "DOpenVDBVectorSplit.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DCheckedSize.h"
#include "../Common/DOpenVDBLimits.h"

#include <set>
#include <sstream>

#include "DMayaHeaders.h"

#include "DOpenVDBData.h"
#include "DOpenVDBUtil.h"

#include "DStringOp.h"

#include "dvdb_ids.h"
#include "../Common/DVDBComputeGuard.h"

const MTypeId DOpenVDBVectorSplit::typeId(ID_VDB_DOPENVDB_VEC_SPLIT);

MObject DOpenVDBVectorSplit::aVdbInput;

MObject DOpenVDBVectorSplit::aVdbSelectedGridNames;

MObject DOpenVDBVectorSplit::aRemoveSource;
MObject DOpenVDBVectorSplit::aCopyInactiveValue;

MObject DOpenVDBVectorSplit::aVdbOutput;

namespace {

class VectorGridSplitter {
  private:
    GridPtr mXGrid, mYGrid, mZGrid;
    bool mCopyInactiveValues;

  public:
    explicit VectorGridSplitter(bool copyInactiveValues)
        : mCopyInactiveValues(copyInactiveValues) {}

    const GridPtr& getXGrid() const {
        return mXGrid;
    }
    const GridPtr& getYGrid() const {
        return mYGrid;
    }
    const GridPtr& getZGrid() const {
        return mZGrid;
    }

    template <typename VecGridT> void operator()(const VecGridT& vecGrid) {
        typedef typename VecGridT::ValueType VecT;
        typedef
            typename VecGridT::TreeType::template ValueConverter<typename VecT::value_type>::Type
                ScalarTreeT;
        typedef typename openvdb::Grid<ScalarTreeT> ScalarGridT;
        typedef typename ScalarGridT::Ptr ScalarGridPtr;

        const VecT bkgd = vecGrid.background();

        // Construct the output scalar grids, with background values taken from
        // the components of the input vector grid's background value.
        ScalarGridPtr xGrid = ScalarGridT::create(bkgd.x()), yGrid = ScalarGridT::create(bkgd.y()),
                      zGrid = ScalarGridT::create(bkgd.z());
        mXGrid = xGrid;
        mYGrid = yGrid;
        mZGrid = zGrid;

        // The output scalar grids share the input vector grid's transform.
        if (openvdb::math::Transform::Ptr xform = vecGrid.transform().copy()) {
            xGrid->setTransform(xform);
            yGrid->setTransform(xform);
            zGrid->setTransform(xform);
        }

        // Use accessors for fast sequential voxel access.
        typename ScalarGridT::Accessor xAccessor = xGrid->getAccessor(),
                                       yAccessor = yGrid->getAccessor(),
                                       zAccessor = zGrid->getAccessor();

        // For each tile or voxel value in the input vector tree,
        // set a corresponding value in each of the output scalar trees.
        openvdb::CoordBBox bbox;
        if (mCopyInactiveValues) {
            for (typename VecGridT::ValueAllCIter it = vecGrid.cbeginValueAll(); it; ++it) {
                if (!it.getBoundingBox(bbox))
                    continue;

                const VecT& val = it.getValue();
                const bool active = it.isValueOn();

                if (it.isTileValue()) {
                    xGrid->fill(bbox, val.x(), active);
                    yGrid->fill(bbox, val.y(), active);
                    zGrid->fill(bbox, val.z(), active);
                } else { // it.isVoxelValue()
                    xAccessor.setValue(bbox.min(), val.x());
                    yAccessor.setValue(bbox.min(), val.y());
                    zAccessor.setValue(bbox.min(), val.z());
                    if (!active) {
                        xAccessor.setValueOff(bbox.min());
                        yAccessor.setValueOff(bbox.min());
                        zAccessor.setValueOff(bbox.min());
                    }
                }
            }
        } else {
            for (typename VecGridT::ValueOnCIter it = vecGrid.cbeginValueOn(); it; ++it) {
                if (!it.getBoundingBox(bbox))
                    continue;

                const VecT& val = it.getValue();

                if (it.isTileValue()) {
                    xGrid->fill(bbox, val.x());
                    yGrid->fill(bbox, val.y());
                    zGrid->fill(bbox, val.z());
                } else { // it.isVoxelValue()
                    xAccessor.setValueOn(bbox.min(), val.x());
                    yAccessor.setValueOn(bbox.min(), val.y());
                    zAccessor.setValueOn(bbox.min(), val.z());
                }
            }
        }
    }
}; // class VectorGridSplitter

} // unnamed namespace

void* DOpenVDBVectorSplit::creator() {
    return ::dommy::maya::createObject("DOpenVDBVectorSplit", [&]() {
        return new DOpenVDBVectorSplit();
    });
}

MStatus DOpenVDBVectorSplit::compute(const MPlug& plug, MDataBlock& data) try {
    if (plug != aVdbOutput)
        return MS::kUnknownParameter;

    const DOpenVDBData* inputVdb = getInputVDB(aVdbInput, data);
    if (!inputVdb || !inputVdb->hasValidData())
        return MS::kInvalidParameter;

    MStatus stat;
    const MDataHandle copyInactiveHandle = data.inputValue(aCopyInactiveValue, &stat);
    if (!stat)
        return stat;
    const bool copyInactiveValues = copyInactiveHandle.asBool();

    const MDataHandle removeSourceHandle = data.inputValue(aRemoveSource, &stat);
    if (!stat)
        return stat;
    const bool removeSourceGrids = removeSourceHandle.asBool();

    const MDataHandle selectionHandle = data.inputValue(aVdbSelectedGridNames, &stat);
    if (!stat)
        return stat;
    const std::string selection = selectionHandle.asString().asChar();

    GridCPtrVec selectedGrids;
    getSelectedGrids(selectedGrids, selection, *inputVdb);

    // Count every generated triplet and every retained input before invoking
    // the sparse-tree splitter. Names need not be unique; source removal is
    // identity-based, including repeated references to the same grid.
    GridCPtrVec splitGrids;
    std::set<const openvdb::GridBase*> splitSources;
    for (GridCPtrVecCIter it = selectedGrids.begin(); it != selectedGrids.end(); ++it) {
        if (getVdbGridTupleSize(getVdbGridType(**it)) != 3)
            continue;
        splitGrids.push_back(*it);
        splitSources.insert(it->get());
    }
    std::size_t retainedCount = 0U;
    for (std::size_t index = 0U; index < inputVdb->numberOfGrids(); ++index) {
        const GridCPtr source = inputVdb->gridPtr(index);
        if (!removeSourceGrids || splitSources.count(source.get()) == 0U)
            ++retainedCount;
    }
    std::size_t outputCount = 0U;
    if (!dommy::memory::checkedMultiply(splitGrids.size(), 3U, outputCount) ||
        !dommy::memory::checkedAdd(outputCount, retainedCount, outputCount) ||
        outputCount > dommy::vdb::kMaxVdbGridCount) {
        MGlobal::displayError("DVDBVectorSplit output grid count exceeds the safety limit.");
        return MS::kInvalidParameter;
    }
    GridCPtrVec resultGrids;
    resultGrids.reserve(outputCount);
    for (GridCPtrVecCIter it = splitGrids.begin(); it != splitGrids.end(); ++it) {
        const openvdb::GridBase& sourceGrid = **it;
        VectorGridSplitter splitter(copyInactiveValues);
        if (!processVdbVectorGrid(getVdbGridType(sourceGrid), sourceGrid, splitter)) {
            return MS::kInvalidParameter;
        }

        GridPtr xGrid = splitter.getXGrid();
        GridPtr yGrid = splitter.getYGrid();
        GridPtr zGrid = splitter.getZGrid();
        if (!xGrid || !yGrid || !zGrid)
            return MS::kFailure;

        const std::string sourceName = sourceGrid.getName();
        xGrid->setName(sourceName.empty() ? "x" : sourceName + ".x");
        yGrid->setName(sourceName.empty() ? "y" : sourceName + ".y");
        zGrid->setName(sourceName.empty() ? "z" : sourceName + ".z");
        resultGrids.push_back(xGrid);
        resultGrids.push_back(yGrid);
        resultGrids.push_back(zGrid);
    }

    for (std::size_t index = 0; index < inputVdb->numberOfGrids(); ++index) {
        const GridCPtr source = inputVdb->gridPtr(index);
        if (removeSourceGrids && splitSources.count(source.get()) != 0U) {
            continue;
        }
        resultGrids.push_back(source);
    }

    MFnPluginData pluginData;
    DOpenVDBData* outputVdb = nullptr;
    stat = createOpenVDBOutputData(pluginData, outputVdb);
    if (!stat)
        return stat;
    if (!outputVdb)
        return MS::kFailure;
    outputVdb->insert(resultGrids);
    return publishOpenVDBOutput(data, aVdbOutput, plug, pluginData.object());
}
DOMMY_VDB_COMPUTE_CATCH("DOpenVDBVectorSplit")

MStatus DOpenVDBVectorSplit::initialize() try {
    MStatus stat;

    // attributes
    MFnNumericAttribute nAttr;
    MFnTypedAttribute tAttr;

    aVdbInput = tAttr.create("VdbInput", "vdbinput", DOpenVDBData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aVdbInput));

    MFnStringData strData;
    const MObject aVdbSelectedGridNamesDefault = strData.create("*", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aVdbSelectedGridNames = tAttr.create("SelectedGridNames", "grids", MFnData::kString,
                                         aVdbSelectedGridNamesDefault, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aVdbSelectedGridNames));

    aRemoveSource = nAttr.create("RemoveSource", "rmsrc", MFnNumericData::kBoolean, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aRemoveSource));

    aCopyInactiveValue =
        nAttr.create("CopyInactiveValue", "cpiactval", MFnNumericData::kBoolean, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aCopyInactiveValue));

    aVdbOutput = tAttr.create("VdbOutput", "vdboutput", DOpenVDBData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);

    DOMMY_RETURN_IF_FAILURE(tAttr.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aVdbOutput));

    // attribute dependencies

    DOMMY_RETURN_IF_FAILURE(attributeAffects(aVdbInput, aVdbOutput));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aVdbSelectedGridNames, aVdbOutput));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(aRemoveSource, aVdbOutput));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aCopyInactiveValue, aVdbOutput));

    return MS::kSuccess;
} catch (...) {
    return dommy::maya::reportEntryPointException("DVDBVectorSplit", "initialize",
                                                  std::current_exception());
}
