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
// Source comparison reference: https://github.com/AcademySoftwareFoundation/openvdb/blob/a79df2f32b8ebb5d15fd73325c46cee845a81e49/openvdb_houdini/houdini/SOP_OpenVDB_Combine.cc

//
//  DOpenVDBCombine.cpp
//  dommyTool
//
//  Created by yuzo on 4/19/16.
//
//

#include "DOpenVDBCombine.h"
#include "common/utility/DMayaEntryPointGuard.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DMayaPluginDataOutput.h"


#include <maya/MFnTypedAttribute.h>
#include <maya/MFnStringData.h>
#include <maya/MGlobal.h>
#include <maya/MFnEnumAttribute.h>
#include <maya/MFnNumericAttribute.h>
#include <maya/MFnGenericAttribute.h>

#include <openvdb/openvdb.h>
#include <openvdb/tools/Composite.h>
#include <openvdb/tools/GridTransformer.h> // for resampleToMatch()
#include <openvdb/tools/LevelSetRebuild.h> // for levelSetRebuild()
#include <openvdb/tools/Morphology.h> // for deactivate()
#include <openvdb/tools/SignedFloodFill.h>
#include <openvdb/tools/ChangeBackground.h>
#include <openvdb/tools/Prune.h>
#include <openvdb/util/NullInterrupter.h>

#include <boost/math/special_functions/fpclassify.hpp>

#include "dvdb_ids.h"
#include "../Common/DVDBComputeGuard.h"
#include "../Common/DVdbCombinePlan.h"

#include <cmath>



const MTypeId DOpenVDBCombine::typeId( ID_VDB_DOPENVDB_COMBINE );

MObject DOpenVDBCombine::vdbInputA;
MObject DOpenVDBCombine::vdbInputB;
MObject DOpenVDBCombine::selectedGridNamesA;
MObject DOpenVDBCombine::selectedGridNamesB;


MObject DOpenVDBCombine::aVdbOutput;

MObject DOpenVDBCombine::isFlatten;
MObject DOpenVDBCombine::isPairs;
MObject DOpenVDBCombine::operationType;
MObject DOpenVDBCombine::multiplierA;
MObject DOpenVDBCombine::multiplierB;
MObject DOpenVDBCombine::resampleType;
MObject DOpenVDBCombine::interporationType;
MObject DOpenVDBCombine::deactivate;
MObject DOpenVDBCombine::bgtolerance;
MObject DOpenVDBCombine::isPrune;
MObject DOpenVDBCombine::tolerance;
MObject DOpenVDBCombine::flood;


void* DOpenVDBCombine::creator()
{
    return ::dommy::maya::createObject("DOpenVDBCombine", [&]() {
        return new DOpenVDBCombine();
    });
}




MStatus DOpenVDBCombine::compute(const MPlug& plug, MDataBlock& data) try
{
	if (plug != aVdbOutput) return MS::kUnknownParameter;

	MStatus stat;
	MDataHandle handle = data.inputValue(isFlatten, &stat);
	if (!stat) return stat;
	const bool flatten = handle.asBool();
	handle = data.inputValue(isPairs, &stat);
	if (!stat) return stat;
	const bool pairs = handle.asBool();
	handle = data.inputValue(operationType, &stat);
	if (!stat) return stat;
	const int operation = handle.asShort();
	if (operation < kFirst || operation > kLast) {
		MGlobal::displayError("OpenVDB Combine operation is invalid.");
		return MS::kFailure;
	}
	const Operation op = asOp(operation);
	handle = data.inputValue(multiplierA, &stat);
	if (!stat) return stat;
	const float multiplierAVal = handle.asFloat();
	handle = data.inputValue(multiplierB, &stat);
	if (!stat) return stat;
	const float multiplierBVal = handle.asFloat();
	handle = data.inputValue(resampleType, &stat);
	if (!stat) return stat;
	const int resampleValue = handle.asInt();
	if (resampleValue < RESAMPLE_MODE_FIRST ||
		resampleValue > RESAMPLE_MODE_LAST) {
		MGlobal::displayError("OpenVDB Combine resample mode is invalid.");
		return MS::kFailure;
	}
	const ResampleMode resample = asResampleMode(resampleValue);
	handle = data.inputValue(interporationType, &stat);
	if (!stat) return stat;
	const int interpolation = handle.asInt();
	if (interpolation < INTERP_POINT || interpolation > INTERP_QUADRATIC) {
		MGlobal::displayError("OpenVDB Combine interpolation mode is invalid.");
		return MS::kFailure;
	}
	const InterporateMode interporationVal =
		static_cast<InterporateMode>(interpolation);
	handle = data.inputValue(deactivate, &stat);
	if (!stat) return stat;
	const bool deactivateVal = handle.asBool();
	handle = data.inputValue(bgtolerance, &stat);
	if (!stat) return stat;
	const float bgtoleranceVal = handle.asFloat();
	handle = data.inputValue(isPrune, &stat);
	if (!stat) return stat;
	const bool isPruneVal = handle.asBool();
	handle = data.inputValue(tolerance, &stat);
	if (!stat) return stat;
	const float toleranceVal = handle.asFloat();
	handle = data.inputValue(flood, &stat);
	if (!stat) return stat;
	const bool floodVal = handle.asBool();

	if (!std::isfinite(static_cast<double>(multiplierAVal)) ||
		!std::isfinite(static_cast<double>(multiplierBVal)) ||
		!std::isfinite(static_cast<double>(bgtoleranceVal)) ||
		!std::isfinite(static_cast<double>(toleranceVal)) ||
		bgtoleranceVal < 0.0f || toleranceVal < 0.0f) {
		MGlobal::displayError("OpenVDB Combine received invalid numeric parameters.");
		return MS::kFailure;
	}

	MDataHandle selectionHandleA = data.inputValue(selectedGridNamesA, &stat);
	if (!stat) return stat;
	MDataHandle selectionHandleB = data.inputValue(selectedGridNamesB, &stat);
	if (!stat) return stat;
	const std::string aGroupStr = selectionHandleA.asString().asChar();
	const std::string bGroupStr = selectionHandleB.asString().asChar();

	const bool needA = needAGrid(op);
	const bool needB = needBGrid(op);
	const DOpenVDBData* inputVdbA = needA ? getInputVDB(vdbInputA, data) : nullptr;
	const DOpenVDBData* inputVdbB = needB ? getInputVDB(vdbInputB, data) : nullptr;

	GridCPtrVec gridsA;
	GridCPtrVec gridsB;
	if (inputVdbA) getGrids(gridsA, *inputVdbA, aGroupStr);
	if (inputVdbB) getGrids(gridsB, *inputVdbB, bGroupStr);

	dommy::vdb::CombinePlan plan;
	const dommy::vdb::CombinePlanStatus planStatus =
		dommy::vdb::buildCombinePlan(
			gridsA.size(), gridsB.size(), needA, needB, flatten, pairs, plan);
	if (planStatus != dommy::vdb::kCombinePlanSuccess) {
		MGlobal::displayError(dommy::vdb::combinePlanStatusMessage(planStatus));
		return MS::kFailure;
	}

	if (plan.unusedA || plan.unusedB) {
		std::ostringstream message;
		message << "OpenVDB Combine did not process all selected "
			<< (plan.unusedA ? "A" : "B") << " grids for this collation mode.";
		MGlobal::displayWarning(message.str().c_str());
	}

	MFnPluginData pluginData;
	DOpenVDBData* outputVdb = nullptr;
	stat = createOpenVDBOutputData(pluginData, outputVdb);
	if (!stat) return stat;

	GridPtr previousOutput;
	std::string previousOutputName;
	for (std::vector<dommy::vdb::CombineStep>::const_iterator it =
		 plan.steps.begin(); it != plan.steps.end(); ++it) {
		const dommy::vdb::CombineStep& step = *it;
		GridCPtr aGrid;
		GridCPtr bGrid;
		if (step.usePreviousA) {
			if (!previousOutput) {
				MGlobal::displayError("OpenVDB Combine flattening lost its intermediate grid.");
				return MS::kFailure;
			}
			aGrid = previousOutput;
		} else if (step.aIndex != dommy::vdb::kNoCombineGrid) {
			aGrid = gridsA.at(step.aIndex);
		}
		if (step.bIndex != dommy::vdb::kNoCombineGrid) {
			bGrid = gridsB.at(step.bIndex);
		}

		const std::string aGridName = aGrid && !aGrid->getName().empty()
			? aGrid->getName() : "A";
		const std::string bGridName = bGrid && !bGrid->getName().empty()
			? bGrid->getName() : "B";
		const std::string outputName = needA
			? (step.usePreviousA ? previousOutputName : aGrid->getName())
			: bGrid->getName();

		GridPtr outGrid = combineGrids(
			op, aGrid, bGrid, aGridName, bGridName, resample,
			interporationVal, multiplierAVal, multiplierBVal, isPruneVal,
			deactivateVal, bgtoleranceVal, toleranceVal, floodVal);
		if (!outGrid) {
			if (flatten) {
				MGlobal::displayError("OpenVDB Combine could not complete flattening.");
				return MS::kFailure;
			}
			continue;
		}

		outGrid->setName(outputName);
		if (flatten) {
			previousOutput = outGrid;
			previousOutputName = outputName;
		}
		if (step.emitOutput) outputVdb->insert(outGrid);
	}

	return publishOpenVDBOutput(data, aVdbOutput, plug, pluginData.object());
}
DOMMY_VDB_COMPUTE_CATCH("DOpenVDBCombine")




MStatus DOpenVDBCombine::initialize()
try {
	MStatus stat;
	
	// attributes
	
	MFnTypedAttribute tAttr;
	MFnNumericAttribute nAttr;
	MFnGenericAttribute gAttr;
	
	MFnStringData strData;
	
	const MObject selectedGridNamesADefaultData = strData.create("*", &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	selectedGridNamesA = tAttr.create("selectedGridNamesA", "sela", MFnData::kString, selectedGridNamesADefaultData, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
	DOMMY_RETURN_IF_FAILURE(addAttribute(selectedGridNamesA));
	
	const MObject selectedGridNamesBDefaultData = strData.create("*", &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	selectedGridNamesB = tAttr.create("selectedGridNamesB", "selb", MFnData::kString, selectedGridNamesBDefaultData, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
	DOMMY_RETURN_IF_FAILURE(addAttribute(selectedGridNamesB));
	
	
	isFlatten = nAttr.create("isFlatten", "ifl", MFnNumericData::kBoolean, 0, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(false));
	DOMMY_RETURN_IF_FAILURE(addAttribute(isFlatten));
	
	
	isPairs = nAttr.create("isPairs", "ipr", MFnNumericData::kBoolean, 0, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(true));
	DOMMY_RETURN_IF_FAILURE(addAttribute(isPairs));
	
	
	MFnEnumAttribute eAttr;
	operationType = eAttr.create("operation", "op", 0, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(eAttr.addField("Copy A", kCopyA));
	DOMMY_RETURN_IF_FAILURE(eAttr.addField("Copy B", kCopyB));
	DOMMY_RETURN_IF_FAILURE(eAttr.addField("Invert A", kInvert));
	DOMMY_RETURN_IF_FAILURE(eAttr.addField("Add", kAdd));
	DOMMY_RETURN_IF_FAILURE(eAttr.addField("Subtract", kSubtract));
	DOMMY_RETURN_IF_FAILURE(eAttr.addField("Multiply", kMultiply));
	DOMMY_RETURN_IF_FAILURE(eAttr.addField("Divide", kDivide));
	DOMMY_RETURN_IF_FAILURE(eAttr.addField("Maximum", kMaximum));
	DOMMY_RETURN_IF_FAILURE(eAttr.addField("Minimum", kMinimum));
	DOMMY_RETURN_IF_FAILURE(eAttr.addField("(1 - A) * B", kBlendA));
	DOMMY_RETURN_IF_FAILURE(eAttr.addField("A + (1 - A) *B", kBlendB));
	DOMMY_RETURN_IF_FAILURE(eAttr.addField("SDF Union", kUnion));
	DOMMY_RETURN_IF_FAILURE(eAttr.addField("SDF Intersection", kIntersection));
	DOMMY_RETURN_IF_FAILURE(eAttr.addField("SDF Difference", kDifference));
	DOMMY_RETURN_IF_FAILURE(eAttr.addField("Replace A with Active B", kReplace));
	DOMMY_RETURN_IF_FAILURE(eAttr.addField("Activity Union", kTopologyUnion));
	DOMMY_RETURN_IF_FAILURE(eAttr.addField("Activity Intersection", kTopologyIntersection));
	DOMMY_RETURN_IF_FAILURE(eAttr.addField("Activity Difference", kTopologyDifference));
	DOMMY_RETURN_IF_FAILURE(eAttr.setConnectable(false));
	DOMMY_RETURN_IF_FAILURE(addAttribute(operationType));
	
	
	
	multiplierA = nAttr.create("multiplierA", "mula", MFnNumericData::kFloat, 0, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1.0));
	DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(-10.0));
	DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(10.0));
	DOMMY_RETURN_IF_FAILURE(addAttribute(multiplierA));
	
	multiplierB = nAttr.create("multiplierB", "mulb", MFnNumericData::kFloat, 0, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1.0));
	DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(-10.0));
	DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(10.0));
	DOMMY_RETURN_IF_FAILURE(addAttribute(multiplierB));
	
	resampleType = eAttr.create("resampleType", "rty", 1, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(eAttr.addField("Off", RESAMPLE_OFF));
	DOMMY_RETURN_IF_FAILURE(eAttr.addField("B to Match A", RESAMPLE_B));
	DOMMY_RETURN_IF_FAILURE(eAttr.addField("A to Match B", RESAMPLE_A));
	DOMMY_RETURN_IF_FAILURE(eAttr.addField("Higher-res to Match Lower-res", RESAMPLE_HI_RES));
	DOMMY_RETURN_IF_FAILURE(eAttr.addField("Lower-res to Match Higher-res", RESAMPLE_LO_RES));
	DOMMY_RETURN_IF_FAILURE(eAttr.setConnectable(false));
	DOMMY_RETURN_IF_FAILURE(addAttribute(resampleType));
	
	interporationType = eAttr.create("interporationType", "int", 1, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(eAttr.addField("Nearest", INTERP_POINT));
	DOMMY_RETURN_IF_FAILURE(eAttr.addField("Linear", INTERP_LINEAR));
	DOMMY_RETURN_IF_FAILURE(eAttr.addField("Quadratic", INTERP_QUADRATIC));
	DOMMY_RETURN_IF_FAILURE(eAttr.setConnectable(false));
	DOMMY_RETURN_IF_FAILURE(addAttribute(interporationType));
	
	
	deactivate = nAttr.create("deactivate", "dact", MFnNumericData::kBoolean, 0, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(false));
	DOMMY_RETURN_IF_FAILURE(addAttribute(deactivate));
	
	
	
	bgtolerance = nAttr.create("bgtolerance", "btor", MFnNumericData::kFloat, 0, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0));
	DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0));
	DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0.0));
	DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(1.0));
	DOMMY_RETURN_IF_FAILURE(addAttribute(bgtolerance));
	
	
	isPrune = nAttr.create("isPrune", "ipn", MFnNumericData::kBoolean, 0, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(true));
	DOMMY_RETURN_IF_FAILURE(addAttribute(isPrune));
	
	tolerance = nAttr.create("tolerance", "tor", MFnNumericData::kFloat, 0, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0));
	DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0));
	DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0.0));
	DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(1.0));
	DOMMY_RETURN_IF_FAILURE(addAttribute(tolerance));
	
	
	flood = nAttr.create("flood", "flo", MFnNumericData::kBoolean, 0, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(false));
	DOMMY_RETURN_IF_FAILURE(addAttribute(flood));
	
	
	
	
	// input / output
	vdbInputA = tAttr.create("vdbInputA", "vdbinputA", DOpenVDBData::id, MObject::kNullObj, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(true));
	DOMMY_RETURN_IF_FAILURE(addAttribute(vdbInputA));
	
	vdbInputB = tAttr.create("vdbInputB", "vdbinputB", DOpenVDBData::id, MObject::kNullObj, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(true));
	DOMMY_RETURN_IF_FAILURE(addAttribute(vdbInputB));
	
	
	aVdbOutput = tAttr.create("VdbOutput", "vdboutput", DOpenVDBData::id, MObject::kNullObj, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	if (stat != MS::kSuccess) return stat;
	
	DOMMY_RETURN_IF_FAILURE(tAttr.setWritable(false));
	DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aVdbOutput));
	
	
	
	// attribute dependencies
	DOMMY_RETURN_IF_FAILURE(attributeAffects(selectedGridNamesA, aVdbOutput));
	DOMMY_RETURN_IF_FAILURE(attributeAffects(selectedGridNamesB, aVdbOutput));
	DOMMY_RETURN_IF_FAILURE(attributeAffects(vdbInputA, aVdbOutput));
	DOMMY_RETURN_IF_FAILURE(attributeAffects(vdbInputB, aVdbOutput));
	
	
	DOMMY_RETURN_IF_FAILURE(attributeAffects(isFlatten, aVdbOutput));
	DOMMY_RETURN_IF_FAILURE(attributeAffects(isPairs, aVdbOutput));
	DOMMY_RETURN_IF_FAILURE(attributeAffects(operationType, aVdbOutput));
	DOMMY_RETURN_IF_FAILURE(attributeAffects(multiplierA, aVdbOutput));
	DOMMY_RETURN_IF_FAILURE(attributeAffects(multiplierB, aVdbOutput));
	DOMMY_RETURN_IF_FAILURE(attributeAffects(resampleType, aVdbOutput));
	DOMMY_RETURN_IF_FAILURE(attributeAffects(interporationType, aVdbOutput));
	DOMMY_RETURN_IF_FAILURE(attributeAffects(deactivate, aVdbOutput));
	DOMMY_RETURN_IF_FAILURE(attributeAffects(bgtolerance, aVdbOutput));
	DOMMY_RETURN_IF_FAILURE(attributeAffects(isPrune, aVdbOutput));
	DOMMY_RETURN_IF_FAILURE(attributeAffects(tolerance, aVdbOutput));
	DOMMY_RETURN_IF_FAILURE(attributeAffects(flood, aVdbOutput));
	
	return MS::kSuccess;
}
catch (...) {
    return dommy::maya::reportEntryPointException(
        "DOpenVDBCombine", "initialize", std::current_exception());
}


////////////////////////////////////////

namespace {
	
	/// Functor to compute scale * grid + offset, for scalars scale and offset
	template<typename GridT>
	struct MulAdd
	{
		typedef typename GridT::ValueType ValueT;
		typedef typename GridT::Ptr GridPtrT;
		
		float scale, offset;
		
		explicit MulAdd(float s, float t = 0.0): scale(s), offset(t) {}
		
		void operator()(const ValueT& a, const ValueT&, ValueT& out) const
		{ out = ValueT(a * scale + offset); }
		
		/// @return true if the scale is 1 and the offset is 0
		bool isIdentity() const
		{
			return (openvdb::math::isApproxEqual(scale, 1.f, 1.0e-6f)
					&& openvdb::math::isApproxEqual(offset, 0.f, 1.0e-6f));
		}
		
		/// Compute dest = src * scale + offset
		void process(const GridT& src, GridPtrT& dest) const
		{
			if (isIdentity()) {
				dest = src.deepCopy();
			} else {
				if (!dest) dest = GridT::create(src); // same transform, new tree
				ValueT bg;
				(*this)(src.background(), ValueT(), bg);
				openvdb::tools::changeBackground(dest->tree(), bg);
				dest->tree().combine2(src.tree(), src.tree(), *this, /*prune=*/false);
			}
		}
	};
	
	
	////////////////////////////////////////
	
	
	/// Functor to compute (1 - A) * B for grids A and B
	template<typename ValueT>
	struct Blend1
	{
		float aMult, bMult;
		const ValueT ONE;
		explicit Blend1(float a = 1.0, float b = 1.0):
		aMult(a), bMult(b), ONE(openvdb::zeroVal<ValueT>() + 1) {}
		void operator()(const ValueT& a, const ValueT& b, ValueT& out) const
		{ out = ValueT((ONE - aMult * a) * bMult * b); }
	};
	
	
	////////////////////////////////////////
	
	
	/// Functor to compute A + (1 - A) * B for grids A and B
	template<typename ValueT>
	struct Blend2
	{
		float aMult, bMult;
		const ValueT ONE;
		explicit Blend2(float a = 1.0, float b = 1.0):
		aMult(a), bMult(b), ONE(openvdb::zeroVal<ValueT>() + 1) {}
		void operator()(const ValueT& a, const ValueT& b, ValueT& out) const
		{ out = ValueT(a*aMult); out = out + ValueT((ONE - out) * bMult*b); }
	};
	
	
	////////////////////////////////////////
	
	
	// Helper class to compare both scalar and vector values
	template<typename ValueT>
	struct ApproxEq
	{
		const ValueT &a, &b;
		ApproxEq(const ValueT& _a, const ValueT& _b): a(_a), b(_b) {}
		operator bool() const {
			return openvdb::math::isRelOrApproxEqual(
													 a, b, /*rel*/ValueT(1e-6f), /*abs*/ValueT(1e-8f));
		}
	};
	
	
	// Specialization for Vec2
	template<typename T>
	struct ApproxEq<openvdb::math::Vec2<T> >
	{
		typedef openvdb::math::Vec2<T> VecT;
		typedef typename VecT::value_type ValueT;
		const VecT &a, &b;
		ApproxEq(const VecT& _a, const VecT& _b): a(_a), b(_b) {}
		operator bool() const { return a.eq(b, /*abs=*/ValueT(1e-8f)); }
	};
	
	
	// Specialization for Vec3
	template<typename T>
	struct ApproxEq<openvdb::math::Vec3<T> >
	{
		typedef openvdb::math::Vec3<T> VecT;
		typedef typename VecT::value_type ValueT;
		const VecT &a, &b;
		ApproxEq(const VecT& _a, const VecT& _b): a(_a), b(_b) {}
		operator bool() const { return a.eq(b, /*abs=*/ValueT(1e-8f)); }
	};
	
	
	// Specialization for Vec4
	template<typename T>
	struct ApproxEq<openvdb::math::Vec4<T> >
	{
		typedef openvdb::math::Vec4<T> VecT;
		typedef typename VecT::value_type ValueT;
		const VecT &a, &b;
		ApproxEq(const VecT& _a, const VecT& _b): a(_a), b(_b) {}
		operator bool() const { return a.eq(b, /*abs=*/ValueT(1e-8f)); }
	};
	
	
	template<typename T> inline bool isFinite(const T& val) { return boost::math::isfinite(val); }
	inline bool isFinite(bool) { return true; }
	
} // unnamed namespace



template<typename AGridT>
struct DOpenVDBCombine::DispatchOp
{
	DOpenVDBCombine::CombineOp* combineOp;
	
	DispatchOp(DOpenVDBCombine::CombineOp& op): combineOp(&op) {}
	
	template<typename BGridT> void operator()(typename BGridT::ConstPtr);
}; // struct DispatchOp


// Helper class for use with processVdbGrid()
struct DOpenVDBCombine::CombineOp
{
	DOpenVDBCombine* self;
	Operation op;
	ResampleMode resample;
	std::string aGridName, bGridName;
	GridCPtr aBaseGrid, bBaseGrid;
	GridPtr outGrid;
	
	float multiplierAVal;
	float multiplierBVal;
	InterporateMode interporationVal;
	bool deactivateVal;
	float bgtoleranceVal;
	bool isPruneVal;
	float toleranceVal;
	bool floodVal;
	
	openvdb::util::NullInterrupter interrupt;
	
	CombineOp(): self(NULL) {}
	
	// Functor for use with processVdbScalarGrid() to return
	// a scalar grid's background value as a floating-point quantity
	struct BackgroundOp {
		double value;
		BackgroundOp(): value(0.0) {}
		template<typename GridT> void operator()(const GridT& grid) {
			value = static_cast<double>(grid.background());
		}
	};
	static double getScalarBackgroundValue(const Grid& baseGrid)
	{
		BackgroundOp bgOp;
		processVdbScalarGrid(getVdbGridType(baseGrid), baseGrid, bgOp);
		return bgOp.value;
	}
	
	template<typename GridT>
	typename GridT::Ptr resampleToMatch(const GridT& src, const Grid& ref, int order)
	{
		typedef typename GridT::ValueType ValueT;
		const ValueT ZERO = openvdb::zeroVal<ValueT>();
		
		const openvdb::math::Transform& refXform = ref.constTransform();
		
		typename GridT::Ptr dest;
		if (src.getGridClass() == openvdb::GRID_LEVEL_SET) {
			// For level set grids, use the level set rebuild tool to both resample the
			// source grid to match the reference grid and to rebuild the resulting level set.
			const ValueT halfWidth = ((ref.getGridClass() == openvdb::GRID_LEVEL_SET)
									  ? ValueT(ZERO + this->getScalarBackgroundValue(ref) * (1.0 / ref.voxelSize()[0]))
									  : ValueT(src.background() * (1.0 / src.voxelSize()[0])));
			
			if (!isFinite(halfWidth)) {
				std::stringstream msg;
				msg << "Resample to match: Illegal narrow band width = " << halfWidth
				<< ", caused by grid '" << src.getName() << "' with background "
				<< this->getScalarBackgroundValue(ref);
				throw std::invalid_argument(msg.str());
			}
			
			try {
				dest = openvdb::tools::doLevelSetRebuild(src, /*iso=*/ZERO, /*exWidth=*/halfWidth, /*inWidth=*/halfWidth, &refXform, &interrupt);

			} catch (openvdb::TypeError&) {
				MGlobal::displayWarning(("skipped rebuild of level set grid " + src.getName() + " of type " + src.type()).c_str());
				dest.reset();
			}
		}
		if (!dest && src.constTransform() != refXform) {
			// For non-level set grids or if level set rebuild failed due to an unsupported
			// grid type, use the grid transformer tool to resample the source grid to match
			// the reference grid.
			dest = src.copyWithNewTree();
			dest->setTransform(refXform.copy());
			using namespace openvdb;
			switch (order) {
				case 0: tools::resampleToMatch<tools::PointSampler>(src, *dest, interrupt); break;
				case 1: tools::resampleToMatch<tools::BoxSampler>(src, *dest, interrupt); break;
				case 2: tools::resampleToMatch<tools::QuadraticSampler>(src, *dest, interrupt); break;
			}
		}
		return dest;
	}
	
	// If necessary, resample one grid so that its index space registers
	// with the other grid's.
	// Note that one of the grid pointers might change as a result.
	template<typename AGridT, typename BGridT>
	void resampleGrids(const AGridT*& aGrid, const BGridT*& bGrid)
	{
		if (!aGrid || !bGrid) return;
		
		const bool
		needA = self->needAGrid(op),
		needB = self->needBGrid(op),
		needBoth = needA && needB;
		
		const int samplingOrder = interporationVal;
		
		// One of RESAMPLE_A, RESAMPLE_B or RESAMPLE_OFF, specifying whether
		// grid A, grid B or neither grid was resampled
		int resampleWhich = RESAMPLE_OFF;
		
		// Determine which of the two grids should be resampled.
		if (resample == RESAMPLE_HI_RES || resample == RESAMPLE_LO_RES) {
			const openvdb::Vec3d
			aVoxSize = aGrid->voxelSize(),
			bVoxSize = bGrid->voxelSize();
			const double
			aVoxVol = aVoxSize[0] * aVoxSize[1] * aVoxSize[2],
			bVoxVol = bVoxSize[0] * bVoxSize[1] * bVoxSize[2];
			resampleWhich = ((aVoxVol > bVoxVol && resample == RESAMPLE_LO_RES)
							 || (aVoxVol < bVoxVol && resample == RESAMPLE_HI_RES))
			? RESAMPLE_A : RESAMPLE_B;
		} else {
			resampleWhich = resample;
		}
		
		if (aGrid->constTransform() != bGrid->constTransform()) {
			// If the A and B grid transforms don't match, one of the grids
			// should be resampled into the other's index space.
			if (resample == RESAMPLE_OFF) {
				if (needBoth) {
					// Resampling is disabled.  Just log a warning.
					std::ostringstream ostr;
					ostr << aGridName << " and " << bGridName << " transforms don't match";
					MGlobal::displayWarning(ostr.str().c_str());
				}
			} else {
				if (needA && resampleWhich == RESAMPLE_A) {
					// Resample grid A into grid B's index space.
					aBaseGrid = this->resampleToMatch(*aGrid, *bGrid, samplingOrder);
					aGrid = static_cast<const AGridT*>(aBaseGrid.get());
				} else if (needB && resampleWhich == RESAMPLE_B) {
					// Resample grid B into grid A's index space.
					bBaseGrid = this->resampleToMatch(*bGrid, *aGrid, samplingOrder);
					bGrid = static_cast<const BGridT*>(bBaseGrid.get());
				}
			}
		}
		
		if (aGrid->getGridClass() == openvdb::GRID_LEVEL_SET &&
			bGrid->getGridClass() == openvdb::GRID_LEVEL_SET)
		{
			// If both grids are level sets, ensure that their background values match.
			// (If one of the grids was resampled, then the background values should
			// already match.)
			const double
			a = this->getScalarBackgroundValue(*aGrid),
			b = this->getScalarBackgroundValue(*bGrid);
			if (!ApproxEq<double>(a, b)) {
				if (resample == RESAMPLE_OFF) {
					if (needBoth) {
						// Resampling/rebuilding is disabled.  Just log a warning.
						std::ostringstream ostr;
						ostr << aGridName << " and " << bGridName
						<< " background values don't match ("
						<< std::setprecision(3) << a << " vs. " << b << ");\n"
						<< "                 the output grid will not be a valid level set";
						MGlobal::displayWarning(ostr.str().c_str());
					}
				} else {
					// One of the two grids needs a level set rebuild.
					if (needA && resampleWhich == RESAMPLE_A) {
						// Rebuild A to match B's background value.
						aBaseGrid = this->resampleToMatch(*aGrid, *bGrid, samplingOrder);
						aGrid = static_cast<const AGridT*>(aBaseGrid.get());
					} else if (needB && resampleWhich == RESAMPLE_B) {
						// Rebuild B to match A's background value.
						bBaseGrid = this->resampleToMatch(*bGrid, *aGrid, samplingOrder);
						bGrid = static_cast<const BGridT*>(bBaseGrid.get());
					}
				}
			}
		}
	}
	
	void checkVectorTypes(const Grid* aGrid, const Grid* bGrid)
	{
		if (!aGrid || !bGrid || !self->needAGrid(op) || !self->needBGrid(op)) return;
		
		switch (op) {
			case kTopologyUnion:
			case kTopologyIntersection:
			case kTopologyDifference:
				// No need to warn about different vector types for topology-only operations.
				break;
				
			default:
			{
				const openvdb::VecType
				aVecType = aGrid->getVectorType(),
				bVecType = bGrid->getVectorType();
				if (aVecType != bVecType) {
					std::ostringstream ostr;
					ostr << aGridName << " and " << bGridName
					<< " have different vector types\n"
					<< "                 (" << Grid::vecTypeToString(aVecType)
					<< " vs. " << Grid::vecTypeToString(bVecType) << ")";
					MGlobal::displayWarning(ostr.str().c_str());
				}
				break;
			}
		}
	}
	
	// Combine two grids of the same type.
	template<typename GridT>
	void combineSameType()
	{
		typedef typename GridT::ValueType ValueT;
		
		const bool
		needA = self->needAGrid(op),
		needB = self->needBGrid(op);
		const float
		aMult = multiplierAVal,
		bMult = multiplierBVal;
		
		const GridT *aGrid = NULL, *bGrid = NULL;
		if (aBaseGrid) aGrid = vdbGridCast<GridT>(aBaseGrid).get();
		if (bBaseGrid) bGrid = vdbGridCast<GridT>(bBaseGrid).get();
		if (needA && !aGrid) throw std::runtime_error("missing A grid");
		if (needB && !bGrid) throw std::runtime_error("missing B grid");
		
		// Warn if combining vector grids with different vector types.
		if (needA && needB && openvdb::VecTraits<ValueT>::IsVec) {
			this->checkVectorTypes(aGrid, bGrid);
		}
		
		// If necessary, resample one grid so that its index space
		// registers with the other grid's.
		if (aGrid && bGrid) this->resampleGrids(aGrid, bGrid);
		
		const ValueT ZERO = openvdb::zeroVal<ValueT>();
		
		// A temporary grid is needed for binary operations, because they
		// cannibalize the B grid.
		typename GridT::Ptr resultGrid, tempGrid;
		
		switch (op) {
			case kCopyA:
				MulAdd<GridT>(aMult).process(*aGrid, resultGrid);
				break;
				
			case kCopyB:
				MulAdd<GridT>(bMult).process(*bGrid, resultGrid);
				break;
				
			case kInvert:
				MulAdd<GridT>(-aMult, 1.0).process(*aGrid, resultGrid);
				break;
				
			case kAdd:
				MulAdd<GridT>(aMult).process(*aGrid, resultGrid);
				MulAdd<GridT>(bMult).process(*bGrid, tempGrid);
				openvdb::tools::compSum(*resultGrid, *tempGrid);
				break;
				
			case kSubtract:
				MulAdd<GridT>(aMult).process(*aGrid, resultGrid);
				MulAdd<GridT>(-bMult).process(*bGrid, tempGrid);
				openvdb::tools::compSum(*resultGrid, *tempGrid);
				break;
				
			case kMultiply:
				MulAdd<GridT>(aMult).process(*aGrid, resultGrid);
				MulAdd<GridT>(bMult).process(*bGrid, tempGrid);
				openvdb::tools::compMul(*resultGrid, *tempGrid);
				break;
				
			case kDivide:
				MulAdd<GridT>(aMult).process(*aGrid, resultGrid);
				MulAdd<GridT>(bMult).process(*bGrid, tempGrid);
				openvdb::tools::compDiv(*resultGrid, *tempGrid);
				break;
				
			case kMaximum:
				MulAdd<GridT>(aMult).process(*aGrid, resultGrid);
				MulAdd<GridT>(bMult).process(*bGrid, tempGrid);
				openvdb::tools::compMax(*resultGrid, *tempGrid);
				break;
				
			case kMinimum:
				MulAdd<GridT>(aMult).process(*aGrid, resultGrid);
				MulAdd<GridT>(bMult).process(*bGrid, tempGrid);
				openvdb::tools::compMin(*resultGrid, *tempGrid);
				break;
				
			case kBlendA: // (1 - A) * B
			{
				const Blend1<ValueT> comp(aMult, bMult);
				ValueT bg;
				comp(aGrid->background(), ZERO, bg);
				resultGrid = aGrid->copyWithNewTree();
				openvdb::tools::changeBackground(resultGrid->tree(), bg);
				resultGrid->tree().combine2(aGrid->tree(), bGrid->tree(), comp, /*prune=*/false);
				break;
			}
			case kBlendB: // A + (1 - A) * B
			{
				const Blend2<ValueT> comp(aMult, bMult);
				ValueT bg;
				comp(aGrid->background(), ZERO, bg);
				resultGrid = aGrid->copyWithNewTree();
				openvdb::tools::changeBackground(resultGrid->tree(), bg);
				resultGrid->tree().combine2(aGrid->tree(), bGrid->tree(), comp, /*prune=*/false);
				break;
			}
				
			case kUnion:
				MulAdd<GridT>(aMult).process(*aGrid, resultGrid);
				MulAdd<GridT>(bMult).process(*bGrid, tempGrid);
				openvdb::tools::csgUnion(*resultGrid, *tempGrid);
				break;
				
			case kIntersection:
				MulAdd<GridT>(aMult).process(*aGrid, resultGrid);
				MulAdd<GridT>(bMult).process(*bGrid, tempGrid);
				openvdb::tools::csgIntersection(*resultGrid, *tempGrid);
				break;
				
			case kDifference:
				MulAdd<GridT>(aMult).process(*aGrid, resultGrid);
				MulAdd<GridT>(bMult).process(*bGrid, tempGrid);
				openvdb::tools::csgDifference(*resultGrid, *tempGrid);
				break;
				
			case kReplace:
				MulAdd<GridT>(aMult).process(*aGrid, resultGrid);
				MulAdd<GridT>(bMult).process(*bGrid, tempGrid);
				openvdb::tools::compReplace(*resultGrid, *tempGrid);
				break;
				
			case kTopologyUnion:
				MulAdd<GridT>(aMult).process(*aGrid, resultGrid);
				// Note: no need to scale the B grid for topology-only operations.
				resultGrid->topologyUnion(*bGrid);
				break;
				
			case kTopologyIntersection:
				MulAdd<GridT>(aMult).process(*aGrid, resultGrid);
				resultGrid->topologyIntersection(*bGrid);
				break;
				
			case kTopologyDifference:
				MulAdd<GridT>(aMult).process(*aGrid, resultGrid);
				resultGrid->topologyDifference(*bGrid);
				break;
		}
		
		outGrid = this->postprocess<GridT>(resultGrid);
	}
	
	// Combine two grids of different types.
	/// @todo Currently, only topology operations can be performed on grids of different types.
	template<typename AGridT, typename BGridT>
	void combineDifferentTypes()
	{
		const bool
		needA = self->needAGrid(op),
		needB = self->needBGrid(op);
		
		const AGridT* aGrid = NULL;
		const BGridT* bGrid = NULL;
		if (aBaseGrid) aGrid = vdbGridCast<AGridT>(aBaseGrid).get();
		if (bBaseGrid) bGrid = vdbGridCast<BGridT>(bBaseGrid).get();
		if (needA && !aGrid) throw std::runtime_error("missing A grid");
		if (needB && !bGrid) throw std::runtime_error("missing B grid");
		
		// Warn if combining vector grids with different vector types.
		if (needA && needB && openvdb::VecTraits<typename AGridT::ValueType>::IsVec
			&& openvdb::VecTraits<typename BGridT::ValueType>::IsVec)
		{
			this->checkVectorTypes(aGrid, bGrid);
		}
		
		// If necessary, resample one grid so that its index space
		// registers with the other grid's.
		if (aGrid && bGrid) this->resampleGrids(aGrid, bGrid);
		
		const float aMult = multiplierAVal;
		
		typename AGridT::Ptr resultGrid;
		
		switch (op) {
			case kTopologyUnion:
				MulAdd<AGridT>(aMult).process(*aGrid, resultGrid);
				// Note: no need to scale the B grid for topology-only operations.
				resultGrid->topologyUnion(*bGrid);
				break;
				
			case kTopologyIntersection:
				MulAdd<AGridT>(aMult).process(*aGrid, resultGrid);
				resultGrid->topologyIntersection(*bGrid);
				break;
				
			case kTopologyDifference:
				MulAdd<AGridT>(aMult).process(*aGrid, resultGrid);
				resultGrid->topologyDifference(*bGrid);
				break;
				
			default:
			{
				std::ostringstream ostr;
				ostr << "can't combine grid " << aGridName << " of type " << aGrid->type()
				<< "\n                 with grid " << bGridName
				<< " of type " << bGrid->type();
				throw std::runtime_error(ostr.str());
				break;
			}
		}
		
		outGrid = this->postprocess<AGridT>(resultGrid);
	}
	
	template<typename GridT>
	typename GridT::Ptr postprocess(typename GridT::Ptr resultGrid)
	{
		typedef typename GridT::ValueType ValueT;
		const ValueT ZERO = openvdb::zeroVal<ValueT>();
		
		const bool
		prune = isPruneVal,
		flood = floodVal,
		deactivate = deactivateVal;
		
		if (deactivate) {
			const float deactivationTolerance = bgtoleranceVal;
			
			// Mark active output tiles and voxels as inactive if their
			// values match the output grid's background value.
			// Do this first to facilitate pruning.
			openvdb::tools::deactivate(*resultGrid, resultGrid->background(),
									   ValueT(ZERO + deactivationTolerance));
		}
		
		if (flood && resultGrid->getGridClass() == openvdb::GRID_LEVEL_SET) {
			openvdb::tools::signedFloodFill(resultGrid->tree());
		}
		if (prune) {
			const float tolerance = toleranceVal;
			openvdb::tools::prune(resultGrid->tree(), ValueT(ZERO + tolerance));
		}
		
		return resultGrid;
	}
	
	template<typename AGridT>
	void operator()(typename AGridT::ConstPtr)
	{
		const bool
		needA = self->needAGrid(op),
		needB = self->needBGrid(op),
		needBoth = needA && needB;
		
		if (!needBoth || !aBaseGrid || !bBaseGrid || aBaseGrid->type() == bBaseGrid->type()) {
			this->combineSameType<AGridT>();
		} else {
			DispatchOp<AGridT> dispatcher(*this);
			// Dispatch on the B grid's type.
			int success = processVdbTopologyGrid(getVdbGridType(*bBaseGrid), bBaseGrid, dispatcher);
			if (!success) {
				std::ostringstream ostr;
				ostr << "grid " << bGridName << " has unsupported type " << bBaseGrid->type();
				MGlobal::displayWarning(ostr.str().c_str());
			}
		}
	}
}; // struct CombineOp


template<typename AGridT>
template<typename BGridT>
void
DOpenVDBCombine::DispatchOp<AGridT>::operator()(typename BGridT::ConstPtr)
{
	combineOp->combineDifferentTypes<AGridT, BGridT>();
}




GridPtr DOpenVDBCombine::combineGrids(Operation op,
									  GridCPtr aGrid, GridCPtr bGrid,
									  const std::string& aGridName, const std::string& bGridName,
									  ResampleMode resample,
									  InterporateMode interpolateVal,
									  float multA,
									  float multB,
									  bool isPrune,
									  bool isDeactivate,
									  float bgtoleranceVal,
									  float toleranceVal,
									  bool isFlood)
{
	GridPtr outGrid;
	
	const bool
	needA = needAGrid(op),
	needB = needBGrid(op),
	needLS = needLevelSets(op);
	
	if (!needA && !needB) throw std::runtime_error("nothing to do");
	if (needA && !aGrid) throw std::runtime_error("missing A grid");
	if (needB && !bGrid) throw std::runtime_error("missing B grid");
	
	if (needLS &&
		((aGrid && aGrid->getGridClass() != openvdb::GRID_LEVEL_SET) ||
		 (bGrid && bGrid->getGridClass() != openvdb::GRID_LEVEL_SET)))
	{
		std::ostringstream ostr;
		ostr << "expected level set grids for the operation,\n                 found "
		<< Grid::gridClassToString(aGrid->getGridClass()) << " (" << aGridName << ") and "
		<< Grid::gridClassToString(bGrid->getGridClass()) << " (" << bGridName
		<< ");\n                 the output grid will not be a valid level set";
		MGlobal::displayWarning(ostr.str().c_str());
		
	}
	
	if (needA && needB && aGrid->type() != bGrid->type()
		&& op != kTopologyUnion && op != kTopologyIntersection && op != kTopologyDifference)
	{
		std::ostringstream ostr;
		ostr << "can't combine grid " << aGridName << " of type " << aGrid->type()
		<< "\n                 with grid " << bGridName << " of type " << bGrid->type();
		MGlobal::displayWarning(ostr.str().c_str());
		return outGrid;
	}
	
	
	CombineOp compOp;
	compOp.self = this;
	compOp.op = op;
	compOp.resample = resample;
	compOp.aBaseGrid = aGrid;
	compOp.bBaseGrid = bGrid;
	compOp.aGridName = aGridName;
	compOp.bGridName = bGridName;
	compOp.interporationVal = interpolateVal;
	compOp.multiplierAVal = multA;
	compOp.multiplierBVal = multB;
	compOp.isPruneVal = isPrune;
	compOp.deactivateVal = isDeactivate;
	compOp.bgtoleranceVal = bgtoleranceVal;
	compOp.toleranceVal = toleranceVal;
	compOp.floodVal = isFlood;
	
	
	compOp.interrupt = openvdb::util::NullInterrupter();
	
	
	const GridCPtr dispatchGrid = needA ? aGrid : bGrid;
	int success = processVdbTopologyGrid(
		getVdbGridType(*dispatchGrid), dispatchGrid, compOp);
	
	if (!success || !compOp.outGrid) {
		std::ostringstream ostr;
		if (needA && needB && aGrid->type() == bGrid->type()) {
			ostr << "grids " << aGridName << " and " << bGridName
			<< " have unsupported type " << aGrid->type();
		} else {
			ostr << "grid " << (needA ? aGridName : bGridName)
			<< " has unsupported type " << dispatchGrid->type();
		}
		MGlobal::displayWarning(ostr.str().c_str());
		
	}
	return compOp.outGrid;
}
