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
// Source comparison reference: https://github.com/AcademySoftwareFoundation/openvdb/blob/a79df2f32b8ebb5d15fd73325c46cee845a81e49/openvdb_houdini/houdini/SOP_OpenVDB_Vector_Merge.cc

//
//  DOpenVDBVectorMerge.cpp
//  dommyTool
//
//  Created by yuzo on 1/7/17.
//
//

#include "common/utility/DMayaEntryPointGuard.h"
#include "DOpenVDBVectorMerge.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DMayaPluginDataOutput.h"

#include <boost/bind.hpp>
#include <boost/function.hpp>
#include <boost/regex.hpp>
#include <boost/shared_ptr.hpp>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include <openvdb/tools/ValueTransformer.h> // for tools::foreach()
#include <openvdb/tools/Prune.h>
#include <openvdb/util/NullInterrupter.h>

#include "DMayaHeaders.h"

#include "DOpenVDBData.h"
#include "DOpenVDBUtil.h"

#include "DStringOp.h"

#include "dvdb_ids.h"
#include "../Common/DVDBComputeGuard.h"

const MTypeId DOpenVDBVectorMerge::typeId(ID_VDB_DOPENVDB_VEC_MERGE);

MObject DOpenVDBVectorMerge::aVdbInput;

MObject DOpenVDBVectorMerge::aXGroup;
MObject DOpenVDBVectorMerge::aYGroup;
MObject DOpenVDBVectorMerge::aZGroup;

MObject DOpenVDBVectorMerge::aUseXName;
MObject DOpenVDBVectorMerge::aMergedName;
MObject DOpenVDBVectorMerge::aVectorType;

MObject DOpenVDBVectorMerge::aRemoveSource;
MObject DOpenVDBVectorMerge::aCopyInactiveValue;

MObject DOpenVDBVectorMerge::aVdbOutput;

namespace {

bool validMergeInputs(const Grid* x, const Grid* y, const Grid* z) {
    if (!x || !y || !z)
        return false;
    const DVdbGridType type = getVdbGridType(*x);
    if (type != getVdbGridType(*y) || type != getVdbGridType(*z))
        return false;
    return type == DVdbGridType::kFloat || type == DVdbGridType::kDouble ||
           type == DVdbGridType::kInt32 || type == DVdbGridType::kInt64;
}

template <typename ScalarGridT>
const typename ScalarGridT::TreeType* checkedMergeTree(const Grid* grid) {
    if (!grid)
        return nullptr;
    if (!grid->template isType<ScalarGridT>()) {
        throw std::invalid_argument("VDB vector-merge component type mismatch");
    }
    return &vdbGridCast<ScalarGridT>(grid)->tree();
}

// Mapping from scalar ValueTypes to Vec3::value_types of registered vector-valued Grid types
template <typename T> struct VecValueTypeMap {
    typedef T Type;
    static const bool Changed = false;
};

struct InterruptException {};

// Functor to merge inactive tiles and voxels from up to three
// scalar-valued trees into one vector-valued tree
template <typename VectorTreeT, typename ScalarTreeT> class MergeActiveOp {
  public:
    typedef typename VectorTreeT::ValueType VectorValueT;
    typedef typename ScalarTreeT::ValueType ScalarValueT;
    typedef typename openvdb::tree::ValueAccessor<const ScalarTreeT> ScalarAccessor;

    MergeActiveOp(const ScalarTreeT* xTree, const ScalarTreeT* yTree, const ScalarTreeT* zTree,
                  openvdb::util::NullInterrupter* interrupt)
        : mDummyTree(new ScalarTreeT), mXAcc(xTree ? *xTree : *mDummyTree),
          mYAcc(yTree ? *yTree : *mDummyTree), mZAcc(zTree ? *zTree : *mDummyTree),
          mInterrupt(interrupt) {
        /// @todo Consider initializing x, y and z dummy trees with background values
        /// taken from the vector tree.  Currently, though, missing components are always
        /// initialized to zero.

        // Note that copies of this op share the original dummy tree.
        // That's OK, since this op doesn't modify the dummy tree.
    }

    MergeActiveOp(const MergeActiveOp& other)
        : mDummyTree(other.mDummyTree), mXAcc(other.mXAcc), mYAcc(other.mYAcc), mZAcc(other.mZAcc),
          mInterrupt(other.mInterrupt) {}

    // Given an active tile or voxel in the vector tree, set its value based on
    // the values of corresponding tiles or voxels in the scalar trees.
    void operator()(const typename VectorTreeT::ValueOnIter& it) const {
        if (openvdb::util::wasInterrupted(mInterrupt)) {
            throw InterruptException();
        }
        const openvdb::Coord xyz = it.getCoord();
        ScalarValueT xval = mXAcc.getValue(xyz), yval = mYAcc.getValue(xyz),
                     zval = mZAcc.getValue(xyz);
        it.setValue(VectorValueT(xval, yval, zval));
    }

  private:
    boost::shared_ptr<const ScalarTreeT> mDummyTree;
    ScalarAccessor mXAcc, mYAcc, mZAcc;
    openvdb::util::NullInterrupter* mInterrupt;
}; // MergeActiveOp

// Functor to merge inactive tiles and voxels from up to three
// scalar-valued trees into one vector-valued tree
template <typename VectorTreeT, typename ScalarTreeT> struct MergeInactiveOp {
    typedef typename VectorTreeT::ValueType VectorValueT;
    typedef typename VectorValueT::value_type VectorElemT;
    typedef typename ScalarTreeT::ValueType ScalarValueT;
    typedef typename openvdb::tree::ValueAccessor<VectorTreeT> VectorAccessor;
    typedef typename openvdb::tree::ValueAccessor<const ScalarTreeT> ScalarAccessor;

    MergeInactiveOp(const ScalarTreeT* xTree, const ScalarTreeT* yTree, const ScalarTreeT* zTree,
                    VectorTreeT& vecTree, openvdb::util::NullInterrupter* interrupt)
        : mDummyTree(new ScalarTreeT), mXAcc(xTree ? *xTree : *mDummyTree),
          mYAcc(yTree ? *yTree : *mDummyTree), mZAcc(zTree ? *zTree : *mDummyTree),
          mVecAcc(vecTree), mInterrupt(interrupt) {
        /// @todo Consider initializing x, y and z dummy trees with background values
        /// taken from the vector tree.  Currently, though, missing components are always
        /// initialized to zero.

        // Note that copies of this op share the original dummy tree.
        // That's OK, since this op doesn't modify the dummy tree.
    }

    MergeInactiveOp(const MergeInactiveOp& other)
        : mDummyTree(other.mDummyTree), mXAcc(other.mXAcc), mYAcc(other.mYAcc), mZAcc(other.mZAcc),
          mVecAcc(other.mVecAcc), mInterrupt(other.mInterrupt) {}

    // Given an inactive tile or voxel in a scalar tree, activate the corresponding
    // tile or voxel in the vector tree.
    void operator()(const typename ScalarTreeT::ValueOffCIter& it) const {
        if (openvdb::util::wasInterrupted(mInterrupt)) {
            throw InterruptException();
        }
        // Skip background tiles and voxels, since the output vector tree
        // is assumed to be initialized with the correct background.
        if (openvdb::math::isExactlyEqual(it.getValue(), it.getTree()->background()))
            return;

        const openvdb::Coord& xyz = it.getCoord();
        if (it.isVoxelValue()) {
            mVecAcc.setActiveState(xyz, true);
        } else {
            // Because the vector tree was constructed with the same node configuration
            // as the scalar trees, tiles can be transferred directly between the two.
            mVecAcc.addTile(it.getLevel(), xyz, openvdb::zeroVal<VectorValueT>(), /*active=*/true);
        }
    }

    // Given an active tile or voxel in the vector tree, set its value based on
    // the values of corresponding tiles or voxels in the scalar trees.
    void operator()(const typename VectorTreeT::ValueOnIter& it) const {
        if (openvdb::util::wasInterrupted(mInterrupt)) {
            throw InterruptException();
        }
        const openvdb::Coord xyz = it.getCoord();
        ScalarValueT xval = mXAcc.getValue(xyz), yval = mYAcc.getValue(xyz),
                     zval = mZAcc.getValue(xyz);
        it.setValue(VectorValueT(xval, yval, zval));
    }

    // Deactivate all voxels in a leaf node of the vector tree.
    void operator()(const typename VectorTreeT::LeafIter& it) const {
        if (openvdb::util::wasInterrupted(mInterrupt)) {
            throw InterruptException();
        }
        it->setValuesOff();
    }

  private:
    boost::shared_ptr<const ScalarTreeT> mDummyTree;
    ScalarAccessor mXAcc, mYAcc, mZAcc;
    mutable VectorAccessor mVecAcc;
    openvdb::util::NullInterrupter* mInterrupt;
}; // MergeInactiveOp

////////////////////////////////////////

class ScalarGridMerger {
  public:
    typedef boost::function<void(const char*)> WarnFunc;

    ScalarGridMerger(const Grid* x, const Grid* y, const Grid* z, const std::string& outGridName,
                     bool copyInactiveValues, WarnFunc warn,
                     openvdb::util::NullInterrupter* interrupt = nullptr)
        : mOutGridName(outGridName), mCopyInactiveValues(copyInactiveValues), mWarn(warn),
          mInterrupt(interrupt) {
        mInGrid[0] = x;
        mInGrid[1] = y;
        mInGrid[2] = z;
    }

    const GridPtr& getGrid() {
        return mOutGrid;
    }

    template <typename ScalarGridT> void operator()(const ScalarGridT& /*ignored*/) {
        if (!mInGrid[0] && !mInGrid[1] && !mInGrid[2])
            return;

        typedef typename ScalarGridT::TreeType ScalarTreeT;

        // Retrieve a scalar tree from each input grid.
        const ScalarTreeT* inTree[3] = {nullptr, nullptr, nullptr};
        inTree[0] = checkedMergeTree<ScalarGridT>(mInGrid[0]);
        inTree[1] = checkedMergeTree<ScalarGridT>(mInGrid[1]);
        inTree[2] = checkedMergeTree<ScalarGridT>(mInGrid[2]);
        if (!inTree[0] && !inTree[1] && !inTree[2])
            return;

        // Get the type of the output vector tree.
        // 1. ScalarT is the input scalar tree's value type.
        typedef typename ScalarTreeT::ValueType ScalarT;
        // 2. VecT is Vec3<ScalarT>, provided that there is a registered Tree with that
        //    value type.  If not, use the closest match (e.g., vec3i when ScalarT = bool).
        typedef VecValueTypeMap<ScalarT> MappedVecT;
        typedef openvdb::math::Vec3<typename MappedVecT::Type> VecT;
        // 3. VecTreeT is the type of a tree with the same height and node dimensions
        //    as the input scalar tree, but with value type VecT instead of ScalarT.
        typedef typename ScalarTreeT::template ValueConverter<VecT>::Type VecTreeT;
        typedef typename openvdb::Grid<VecTreeT> VecGridT;

        if (MappedVecT::Changed && mWarn) {
            std::ostringstream ostr;
            ostr << "grids of type vec3<" << openvdb::typeNameAsString<ScalarT>()
                 << "> are not supported; using " << openvdb::typeNameAsString<VecT>()
                 << " instead";
            if (!mOutGridName.empty())
                ostr << " for " << mOutGridName;
            mWarn(ostr.str().c_str());
        }

        // Determine the background value and the transform.
        VecT bkgd(0, 0, 0);
        const openvdb::math::Transform* xform = nullptr;
        for (int i = 0; i < 3; ++i) {
            if (inTree[i])
                bkgd[i] = inTree[i]->background();
            if (mInGrid[i] && !xform)
                xform = &(mInGrid[i]->transform());
        }
        openvdb::math::Transform::Ptr outXform;
        if (xform)
            outXform = xform->copy();

        // Construct the output vector grid, with a background value whose
        // components are the background values of the input scalar grids.
        typename VecGridT::Ptr vecGrid = VecGridT::create(bkgd);
        mOutGrid = vecGrid;

        if (outXform) {
            mOutGrid->setTransform(outXform);

            // Check that all three input grids have the same transform.
            bool xformMismatch = false;
            for (int i = 0; i < 3 && !xformMismatch; ++i) {
                if (mInGrid[i]) {
                    const openvdb::math::Transform* inXform = &(mInGrid[i]->transform());
                    if (*outXform != *inXform)
                        xformMismatch = true;
                }
            }
            if (xformMismatch && mWarn) {
                mWarn("component grids have different transforms");
            }
        }

        if (mCopyInactiveValues) {
            try {
                MergeInactiveOp<VecTreeT, ScalarTreeT> op(inTree[0], inTree[1], inTree[2],
                                                          vecGrid->tree(), mInterrupt);

                // 1. For each non-background inactive value in each scalar tree,
                //    activate the corresponding region in the vector tree.
                for (int i = 0; i < 3; ++i) {
                    if (openvdb::util::wasInterrupted(mInterrupt)) {
                        mOutGrid.reset();
                        return;
                    }
                    if (!inTree[i])
                        continue;
                    // Because this is a topology-modifying operation, it must be done serially.
                    openvdb::tools::foreach (inTree[i]->cbeginValueOff(), op, /*threaded=*/false,
                                             /*shareOp=*/false);
                }

                // 2. For each active value in the vector tree, set v = (x, y, z).
                openvdb::tools::foreach (vecGrid->beginValueOn(), op, /*threaded=*/true,
                                         /*shareOp=*/false);

                // 3. Deactivate all values in the vector tree, by processing
                //    leaf nodes in parallel (which is safe) and tiles serially.
                openvdb::tools::foreach (vecGrid->tree().beginLeaf(), op, /*threaded=*/true);
                typename VecTreeT::ValueOnIter tileIt = vecGrid->beginValueOn();
                tileIt.setMaxDepth(tileIt.getLeafDepth() - 1); // skip leaf nodes
                for (int count = 0; tileIt; ++tileIt, ++count) {
                    if (count % 100 == 0 && openvdb::util::wasInterrupted(mInterrupt)) {
                        mOutGrid.reset();
                        return;
                    }
                    tileIt.setValueOff();
                }
            } catch (InterruptException&) {
                mOutGrid.reset();
                return;
            }
        }

        // Activate voxels (without setting their values) in the output vector grid so that
        // its tree topology is the union of the topologies of the input scalar grids.
        // Transferring voxel values from the scalar grids to the vector grid can then
        // be done safely from multiple threads.
        for (int i = 0; i < 3; ++i) {
            if (openvdb::util::wasInterrupted(mInterrupt)) {
                mOutGrid.reset();
                return;
            }
            if (!inTree[i])
                continue;
            vecGrid->tree().topologyUnion(*inTree[i]);
        }

        // Set a new value for each active tile or voxel in the output vector grid.
        try {
            MergeActiveOp<VecTreeT, ScalarTreeT> op(inTree[0], inTree[1], inTree[2], mInterrupt);
            openvdb::tools::foreach (vecGrid->beginValueOn(), op, /*threaded=*/true,
                                     /*shareOp=*/false);
        } catch (InterruptException&) {
            mOutGrid.reset();
            return;
        }
        openvdb::tools::prune(vecGrid->tree());
    }

  private:
    const Grid* mInGrid[3];
    GridPtr mOutGrid;
    std::string mOutGridName;
    bool mCopyInactiveValues;
    WarnFunc mWarn;
    openvdb::util::NullInterrupter* mInterrupt;
}; // class ScalarGridMerger

} // unnamed namespace

void* DOpenVDBVectorMerge::creator() {
    return ::dommy::maya::createObject("DOpenVDBVectorMerge", [&]() {
        return new DOpenVDBVectorMerge();
    });
}

MStatus DOpenVDBVectorMerge::compute(const MPlug& plug, MDataBlock& data) try {
    if (plug != aVdbOutput)
        return MS::kUnknownParameter;
    const DOpenVDBData* inputVdb = getInputVDB(aVdbInput, data);
    if (!inputVdb || !inputVdb->hasValidData())
        return MS::kInvalidParameter;

    MStatus status;
    const MDataHandle copyInactiveHandle = data.inputValue(aCopyInactiveValue, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const bool copyInactiveValues = copyInactiveHandle.asBool();
    const MDataHandle removeSourceHandle = data.inputValue(aRemoveSource, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const bool removeSourceGrids = removeSourceHandle.asBool();
    const MDataHandle vectorTypeHandle = data.inputValue(aVectorType, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const short vectorType = vectorTypeHandle.asShort();
    if (vectorType < 0 || vectorType >= openvdb::NUM_VEC_TYPES)
        return MS::kInvalidParameter;
    const MDataHandle mergedNameHandle = data.inputValue(aMergedName, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const std::string mergeName = mergedNameHandle.asString().asChar();
    const MDataHandle useXNameHandle = data.inputValue(aUseXName, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const bool useXName = useXNameHandle.asBool();
    const MDataHandle xGroupHandle = data.inputValue(aXGroup, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const std::string xGroup = xGroupHandle.asString().asChar();
    const MDataHandle yGroupHandle = data.inputValue(aYGroup, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const std::string yGroup = yGroupHandle.asString().asChar();
    const MDataHandle zGroupHandle = data.inputValue(aZGroup, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const std::string zGroup = zGroupHandle.asString().asChar();

    GridCPtrVec xGrids, yGrids, zGrids;
    getSelectedGrids(xGrids, xGroup, *inputVdb);
    getSelectedGrids(yGrids, yGroup, *inputVdb);
    getSelectedGrids(zGrids, zGroup, *inputVdb);

    GridCPtrVec resultGrids;
    std::set<const Grid*> mergedSources;
    if (!xGrids.empty() && !yGrids.empty() && !zGrids.empty()) {
        // Retain the historical first-match rule, with immutable source grids.
        const Grid* xGrid = xGrids.front().get();
        const Grid* yGrid = yGrids.front().get();
        const Grid* zGrid = zGrids.front().get();
        if (!validMergeInputs(xGrid, yGrid, zGrid)) {
            MGlobal::displayError("DVDBVectorMerge requires three matching scalar grid types.");
            return MS::kInvalidParameter;
        }

        std::string outGridName =
            boost::regex_replace(mergeName, boost::regex("#+"), std::string("1"));
        if (useXName) {
            std::string xName = xGrid->getName();
            removeLastExtention(xName);
            if (!xName.empty())
                outGridName = xName;
        }

        openvdb::util::NullInterrupter progress;
        ScalarGridMerger merger(xGrid, yGrid, zGrid, outGridName, copyInactiveValues,
                                boost::bind(&DOpenVDBVectorMerge::addWarningMessage, this, _1),
                                &progress);
        if (!processVdbScalarGrid(getVdbGridType(*xGrid), *xGrid, merger)) {
            return MS::kInvalidParameter;
        }
        GridPtr merged = merger.getGrid();
        if (!merged)
            return MS::kFailure;
        merged->setName(outGridName);
        merged->setVectorType(static_cast<openvdb::VecType>(vectorType));
        resultGrids.push_back(merged);
        // Names need not be unique. Only actually merged source objects are removed.
        mergedSources.insert(xGrid);
        mergedSources.insert(yGrid);
        mergedSources.insert(zGrid);
    } else {
        // Preserve historical pass-through behavior for a missing component selection.
        MGlobal::displayInfo("x y z grid not found.");
    }

    for (std::size_t index = 0; index < inputVdb->numberOfGrids(); ++index) {
        const GridCPtr source = inputVdb->gridPtr(index);
        if (removeSourceGrids && mergedSources.count(source.get()) != 0U)
            continue;
        resultGrids.push_back(source);
    }

    MFnPluginData pluginData;
    DOpenVDBData* outputVdb = nullptr;
    DOMMY_RETURN_IF_FAILURE(createOpenVDBOutputData(pluginData, outputVdb));
    if (!outputVdb)
        return MS::kFailure;
    outputVdb->insert(resultGrids);
    return publishOpenVDBOutput(data, aVdbOutput, plug, pluginData.object());
}
DOMMY_VDB_COMPUTE_CATCH("DOpenVDBVectorMerge")

MStatus DOpenVDBVectorMerge::initialize() try {
    MStatus stat;

    // attributes
    MFnNumericAttribute nAttr;
    MFnTypedAttribute tAttr;

    aVdbInput = tAttr.create("VdbInput", "vdbinput", DOpenVDBData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aVdbInput));

    MFnStringData strData;

    const MObject aXGroupDefault = strData.create("@name=*.x", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aXGroup = tAttr.create("XGroup", "xgrp", MFnData::kString, aXGroupDefault, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aXGroup));

    const MObject aYGroupDefault = strData.create("@name=*.y", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aYGroup = tAttr.create("YGroup", "ygrp", MFnData::kString, aYGroupDefault, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aYGroup));

    const MObject aZGroupDefault = strData.create("@name=*.z", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aZGroup = tAttr.create("ZGroup", "zgrp", MFnData::kString, aZGroupDefault, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aZGroup));

    aUseXName = nAttr.create("UseXName", "uxnm", MFnNumericData::kBoolean, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aUseXName));

    const MObject aMergedNameDefault = strData.create("merged#", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aMergedName = tAttr.create("MergedName", "mrgnm", MFnData::kString, aMergedNameDefault, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aMergedName));

    MFnEnumAttribute eAttr;
    aVectorType = eAttr.create("VectorType", "vecty", 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    for (int i = 0; i < openvdb::NUM_VEC_TYPES; ++i) {
        DOMMY_RETURN_IF_FAILURE(eAttr.addField(
            openvdb::GridBase::vecTypeExamples(openvdb::VecType(i)).c_str(), openvdb::VecType(i)));
    }
    DOMMY_RETURN_IF_FAILURE(eAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aVectorType));

    aRemoveSource = nAttr.create("RemoveSource", "rmsrc", MFnNumericData::kBoolean, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aRemoveSource));

    aCopyInactiveValue =
        nAttr.create("CopyInactiveValue", "cpiactval", MFnNumericData::kBoolean, 0, &stat);
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

    DOMMY_RETURN_IF_FAILURE(attributeAffects(aXGroup, aVdbOutput));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aYGroup, aVdbOutput));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aZGroup, aVdbOutput));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(aUseXName, aVdbOutput));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aMergedName, aVdbOutput));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aVectorType, aVdbOutput));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(aRemoveSource, aVdbOutput));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aCopyInactiveValue, aVdbOutput));

    return MS::kSuccess;
} catch (...) {
    return dommy::maya::reportEntryPointException("DVDBVectorMerge", "initialize",
                                                  std::current_exception());
}
