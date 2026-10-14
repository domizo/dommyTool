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
// Source comparison reference: https://github.com/AcademySoftwareFoundation/openvdb/blob/a79df2f32b8ebb5d15fd73325c46cee845a81e49/openvdb_houdini/houdini/SOP_OpenVDB_Remap.cc

//
//  DOpenVDBRemap.cpp
//  dommyTool
//
//  Created by yuzo on 2017/01/12.
//
//

#include "common/utility/DMayaEntryPointGuard.h"
#include "DOpenVDBRemap.h"
#include "DOpenVDBValuePolicy.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DMayaPluginData.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DMayaDefaultRamp.h"
#include "../Common/DVDBComputeGuard.h"

#include "DOpenVDBData.h"
#include "DOpenVDBUtil.h"

#include "DMayaHeaders.h"

#include <tbb/blocked_range.h>
#include <tbb/parallel_for.h>

#include <openvdb/math/Math.h> // Tolerance
#include <openvdb/tools/ValueTransformer.h>

#include <cmath>
#include <string>
#include <stdexcept>
#include <vector>

#include "dvdb_ids.h"

const MTypeId DOpenVDBRemap::typeId(ID_VDB_DOPENVDB_REMAP);

MObject DOpenVDBRemap::aVdbInput;
MObject DOpenVDBRemap::aVdbSelectedGridNames;

MObject DOpenVDBRemap::aBelowMinimum;
MObject DOpenVDBRemap::aAboveMaximum;

MObject DOpenVDBRemap::aInputRange;
MObject DOpenVDBRemap::aOutputRange;

MObject DOpenVDBRemap::aTransferFunction;
MObject DOpenVDBRemap::aDeactivateBackgroundVoxels;

MObject DOpenVDBRemap::aVdbOutput;

void* DOpenVDBRemap::creator() {
    return ::dommy::maya::createObject("DOpenVDBRemap", [&]() {
        return new DOpenVDBRemap();
    });
}

void DOpenVDBRemap::postConstructor() {
    dommy::maya::initializeNodeRamps("DOpenVDBRemap", [&]() -> MStatus {
        DOMMY_RETURN_IF_FAILURE(dommy::maya::initializeDefaultCurve(
            thisMObject(), aTransferFunction, {0.0f, 1.0f}, {0.0f, 1.0f}));
        return MS::kSuccess;
    });
}

////////////////////////////////////////

// Local Utility Methods

namespace {

class RampLookup {
  public:
    static const size_t kIntervalCount = 4096;

    explicit RampLookup(MRampAttribute& ramp) : mSamples(kIntervalCount + 1, 0.0f) {
        for (size_t index = 0; index <= kIntervalCount; ++index) {
            const float position = static_cast<float>(index) / static_cast<float>(kIntervalCount);
            MStatus status;
            ramp.getValueAtPosition(position, mSamples[index], &status);
            if (status != MS::kSuccess || !std::isfinite(mSamples[index])) {
                throw std::runtime_error("Unable to sample the VDB remap transfer function");
            }
        }
    }

    long double sample(const long double position, const bool extrapolate) const {
        if (!std::isfinite(position))
            throw std::overflow_error("VDB remap position is not finite");
        if (position <= 0) {
            if (!extrapolate)
                return mSamples.front();
            const long double slope =
                (static_cast<long double>(mSamples[1]) - mSamples[0]) * kIntervalCount;
            return std::fma(position, slope, static_cast<long double>(mSamples.front()));
        }
        if (position >= 1) {
            if (!extrapolate)
                return mSamples.back();
            const long double slope = (static_cast<long double>(mSamples[kIntervalCount]) -
                                       mSamples[kIntervalCount - 1]) *
                                      kIntervalCount;
            return std::fma(position - 1, slope, static_cast<long double>(mSamples.back()));
        }
        const long double scaled = position * kIntervalCount;
        const size_t lower = static_cast<size_t>(scaled);
        const long double fraction = scaled - lower;
        return std::fma(fraction, static_cast<long double>(mSamples[lower + 1]) - mSamples[lower],
                        static_cast<long double>(mSamples[lower]));
    }

  private:
    std::vector<float> mSamples;
};
template <typename NodeType> struct Deactivate {
    typedef typename NodeType::ValueType ValueType;

    Deactivate(std::vector<NodeType*>& nodes, ValueType background)
        : mNodes(nodes.data()), mBackground(background) {}

    void operator()(const tbb::blocked_range<size_t>& range) const {
        const ValueType background(mBackground),
            delta = openvdb::math::Tolerance<ValueType>::value();
        for (size_t n = range.begin(), N = range.end(); n < N; ++n) {
            for (typename NodeType::ValueOnIter it = mNodes[n]->beginValueOn(); it; ++it) {
                if (dommy::vdb::ValueComponents<ValueType>::matchesBackground(background, *it,
                                                                              delta)) {
                    it.setValueOff();
                }
            }
        }
    }

    NodeType* const* const mNodes;
    ValueType mBackground;
};
template <typename TreeType> void deactivateBackgroundValues(TreeType& tree) {
    { // eval voxels
        typedef typename TreeType::LeafNodeType LeafNodeType;
        std::vector<LeafNodeType*> nodes;
        tree.getNodes(nodes);

        Deactivate<LeafNodeType> op(nodes, tree.background());
        tbb::parallel_for(tbb::blocked_range<size_t>(0, nodes.size()), op);
    }

    { // eval first tiles
        typedef typename TreeType::RootNodeType RootNodeType;
        typedef typename RootNodeType::NodeChainType NodeChainType;
        typedef typename boost::mpl::at<NodeChainType, boost::mpl::int_<1>>::type InternalNodeType;

        std::vector<InternalNodeType*> nodes;
        tree.getNodes(nodes);

        Deactivate<InternalNodeType> op(nodes, tree.background());
        tbb::parallel_for(tbb::blocked_range<size_t>(0, nodes.size()), op);
    }

    { // eval remaining tiles
        typedef typename TreeType::ValueType ValueType;
        const ValueType background(tree.background()),
            delta = openvdb::math::Tolerance<ValueType>::value();
        typename TreeType::ValueOnIter it(tree);
        it.setMaxDepth(TreeType::ValueAllCIter::LEAF_DEPTH - 2);
        for (; it; ++it) {
            if (dommy::vdb::ValueComponents<ValueType>::matchesBackground(background, *it, delta)) {
                it.setValueOff();
            }
        }
    }
}

////////////////////////////////////////

struct RemapGridValues {

    enum Extrapolation { CLAMP, PRESERVE, EXTRAPOLATE };

    RemapGridValues(Extrapolation belowExt, Extrapolation aboveExt, const RampLookup& ramp,
                    const float inMin, const float inMax, const float outMin, const float outMax,
                    bool deactivate)
        : mBelowExtrapolation(belowExt), mAboveExtrapolation(aboveExt), mRamp(&ramp), mInMin(inMin),
          mInMax(inMax), mOutMin(outMin), mOutMax(outMax), mDeactivate(deactivate) {}

    template <typename GridType> void operator()(GridType& grid) {
        typedef typename GridType::TreeType::LeafNodeType LeafNodeType;

        std::vector<LeafNodeType*> leafnodes;
        grid.tree().getNodes(leafnodes);

        ValueTransform<GridType> op(*mRamp, leafnodes, mBelowExtrapolation, mAboveExtrapolation,
                                    mInMin, mInMax, mOutMin, mOutMax);

        // update voxels
        tbb::parallel_for(tbb::blocked_range<size_t>(0, leafnodes.size()), op);

        // update tiles
        typename GridType::ValueAllIter it = grid.beginValueAll();
        it.setMaxDepth(GridType::ValueAllIter::LEAF_DEPTH - 1);
        openvdb::tools::foreach (it, op, true);

        // update background value
        grid.tree().root().setBackground(op.map(grid.background()), /*updateChildNodes=*/false);
        grid.setGridClass(openvdb::GRID_UNKNOWN);

        if (mDeactivate) {
            deactivateBackgroundValues(grid.tree());
        }
    }

  private:
    template <typename GridType> struct ValueTransform {
        typedef typename GridType::TreeType::LeafNodeType LeafNodeType;

        ValueTransform(const RampLookup& lookup, std::vector<LeafNodeType*>& leafnodes,
                       Extrapolation belowExt, Extrapolation aboveExt, const float inMin,
                       const float inMax, const float outMin, const float outMax)
            : ramp(&lookup), nodes(leafnodes.data()), belowExtrapolation(belowExt),
              aboveExtrapolation(aboveExt), xMin(inMin), xMax(inMax), yMin(outMin), yMax(outMax) {}

        inline void operator()(const tbb::blocked_range<size_t>& range) const {
            for (size_t n = range.begin(), N = range.end(); n < N; ++n) {
                typename GridType::ValueType* data = nodes[n]->buffer().data();
                for (size_t i = 0, I = LeafNodeType::SIZE; i < I; ++i) {
                    data[i] = map(data[i]);
                }
            }
        }

        inline void operator()(const typename GridType::ValueAllIter& it) const {
            it.setValue(map(*it));
        }

        template <typename T> T map(const T source) const {
            return dommy::vdb::remapValue(source, *ramp, belowExtrapolation, aboveExtrapolation,
                                          xMin, xMax, yMin, yMax);
        }

        template <typename T>
        inline openvdb::math::Vec3<T> map(const openvdb::math::Vec3<T>& v) const {
            openvdb::math::Vec3<T> out;
            out[0] = map(v[0]);
            out[1] = map(v[1]);
            out[2] = map(v[2]);
            return out;
        }

        const RampLookup* ramp;
        LeafNodeType* const* const nodes;
        const Extrapolation belowExtrapolation, aboveExtrapolation;
        float xMin, xMax, yMin, yMax;
    }; // struct ValueTransform

    //////////

    Extrapolation mBelowExtrapolation, mAboveExtrapolation;
    const RampLookup* mRamp;
    const float mInMin, mInMax, mOutMin, mOutMax;
    const bool mDeactivate;
}; // struct RemapGridValues

} // unnamed namespace

////////////////////////////////////////

MStatus DOpenVDBRemap::compute(const MPlug& plug, MDataBlock& data) try {
    if (plug != aVdbOutput)
        return MS::kUnknownParameter;

    MStatus stat;
    const MDataHandle inputHandle = data.inputValue(aVdbInput, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    const DOpenVDBData* inputVdb = nullptr;
    DOMMY_RETURN_IF_FAILURE(
        dommy::maya::getPluginDataFromHandle(inputHandle, DOpenVDBData::id, inputVdb));
    if (!inputVdb || !inputVdb->hasValidData())
        return MS::kInvalidParameter;
    const MDataHandle inputRangeHandle = data.inputValue(aInputRange, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    const float2& inputRange = inputRangeHandle.asFloat2();
    const MDataHandle outputRangeHandle = data.inputValue(aOutputRange, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    const float2& outputRange = outputRangeHandle.asFloat2();
    const MDataHandle deactivateHandle = data.inputValue(aDeactivateBackgroundVoxels, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    const bool deactivate = deactivateHandle.asBool();
    const MDataHandle belowHandle = data.inputValue(aBelowMinimum, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    const short below = belowHandle.asShort();
    const MDataHandle aboveHandle = data.inputValue(aAboveMaximum, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    const short above = aboveHandle.asShort();

    const MDataHandle selectionHandle = data.inputValue(aVdbSelectedGridNames, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    const std::string selection = selectionHandle.asString().asChar();

    const float inMin = inputRange[0], inMax = inputRange[1];
    const float outMin = outputRange[0], outMax = outputRange[1];
    if (!dommy::vdb::validRemapSettings(below, above, inMin, inMax, outMin, outMax))
        return MS::kInvalidParameter;
    GridCPtrVec selected;
    if (!getSelectedGrids(selected, selection, *inputVdb))
        MGlobal::displayWarning(
            "No VDB grids match the remap selection; input was passed through.");
    std::size_t work = 0;
    std::vector<bool> supported;
    for (const auto& source : selected) {
        dommy::vdb::ValuePreflight preflight(work, deactivate ? 3 : 2);
        supported.push_back(processVdbGrid(getVdbGridType(*source), *source, preflight));
        if (!preflight.valid)
            return MS::kInvalidParameter;
    }
    // Maya ramp reads stay on the calling thread, never inside TBB workers.
    MPlug rampPlug(thisMObject(), aTransferFunction);
    MRampAttribute ramp(rampPlug, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    const RampLookup rampLookup(ramp);
    RemapGridValues remap(static_cast<RemapGridValues::Extrapolation>(below),
                          static_cast<RemapGridValues::Extrapolation>(above), rampLookup, inMin,
                          inMax, outMin, outMax, deactivate);
    MFnPluginData pluginData;
    DOpenVDBData* outputVdb = nullptr;
    DOMMY_RETURN_IF_FAILURE(createOpenVDBOutputData(pluginData, outputVdb));
    if (!outputVdb)
        return MS::kFailure;
    getSelectedGrids(selected, selection, *inputVdb, *outputVdb);
    for (std::size_t index = 0; index < selected.size(); ++index) {
        const auto& source = selected[index];
        if (!supported[index]) {
            MGlobal::displayWarning(
                ("Passed through '" + source->getName() + "', unsupported remap type.").c_str());
            outputVdb->insert(*source);
            continue;
        }
        auto copy = source->deepCopyGrid();
        if (!copy || !processVdbGrid(getVdbGridType(*source), *copy, remap))
            return MS::kFailure;
        outputVdb->insert(copy);
    }
    if (!outputVdb->hasValidData())
        return MS::kFailure;
    return publishOpenVDBOutput(data, aVdbOutput, plug, pluginData.object());
}
DOMMY_VDB_COMPUTE_CATCH("DOpenVDBRemap")

MStatus DOpenVDBRemap::initialize() try {
    MStatus stat;

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

    MFnEnumAttribute minAttr;
    aBelowMinimum = minAttr.create("BelowMinimum", "blwmin", 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(minAttr.addField("Clamp", 0));
    DOMMY_RETURN_IF_FAILURE(minAttr.addField("Preserve", 1));
    DOMMY_RETURN_IF_FAILURE(minAttr.addField("Extrapolate", 2));
    DOMMY_RETURN_IF_FAILURE(minAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aBelowMinimum));

    MFnEnumAttribute maxAttr;
    aAboveMaximum = maxAttr.create("AboveMaximum", "abvmax", 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(maxAttr.addField("Clamp", 0));
    DOMMY_RETURN_IF_FAILURE(maxAttr.addField("Preserve", 1));
    DOMMY_RETURN_IF_FAILURE(maxAttr.addField("Extrapolate", 2));
    DOMMY_RETURN_IF_FAILURE(maxAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aAboveMaximum));

    aInputRange = nAttr.create("InputRange", "inrng", MFnNumericData::k2Float, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0, 1.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aInputRange));

    aOutputRange = nAttr.create("OutputRange", "outrng", MFnNumericData::k2Float, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0, 1.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aOutputRange));

    aTransferFunction = MRampAttribute::createCurveRamp("TransferFunction", "trfnc", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(addAttribute(aTransferFunction));

    aDeactivateBackgroundVoxels = nAttr.create("DeactivateBackgroundVoxels", "dactbfvx",
                                               MFnNumericData::kBoolean, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aDeactivateBackgroundVoxels));

    aVdbOutput = tAttr.create("VdbOutput", "vdboutput", DOpenVDBData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);

    DOMMY_RETURN_IF_FAILURE(tAttr.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aVdbOutput));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(aVdbInput, aVdbOutput));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aVdbSelectedGridNames, aVdbOutput));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(aBelowMinimum, aVdbOutput));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aAboveMaximum, aVdbOutput));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(aInputRange, aVdbOutput));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aOutputRange, aVdbOutput));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(aTransferFunction, aVdbOutput));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aDeactivateBackgroundVoxels, aVdbOutput));

    return MS::kSuccess;
} catch (...) {
    return dommy::maya::reportEntryPointException("DOpenVDBRemap", "initialize",
                                                  std::current_exception());
}
