//
//  DOpenVDBCombine.hpp
//  dommyTool
//
//  Created by yuzo on 4/19/16.
//
//

#ifndef DOpenVDBCombine_h
#define DOpenVDBCombine_h

#include <string>

#include <maya/MPxNode.h>

#include "DOpenVDBData.h"
#include "DOpenVDBUtil.h"

class DOpenVDBCombine final : public MPxNode
{
public:
	DOpenVDBCombine() {}
	~DOpenVDBCombine() override = default;
	
	MStatus compute(const MPlug& plug, MDataBlock& block) override;
	
	static void* creator();
	static MStatus initialize();
	static const MTypeId typeId;
	
	enum Operation {
		kCopyA = 0,               // A
		kCopyB,                   // B
		kInvert,                  // 1 - A
		kAdd,                     // A + B
		kSubtract,                // A - B
		kMultiply,                // A * B
		kDivide,                  // A / B
		kMaximum,                 // max(A, B)
		kMinimum,                 // min(A, B)
		kBlendA,                  // (1 - A) * B
		kBlendB,                  // A + (1 - A) * B
		kUnion,                   // CSG A u B
		kIntersection,            // CSG A n B
		kDifference,              // CSG A / B
		kReplace,                 // replace A with B
		kTopologyUnion,           // A u active(B)
		kTopologyIntersection,    // A n active(B)
		kTopologyDifference       // A / active(B)
	};
	
	enum { kFirst = kCopyA, kLast = kTopologyDifference };
	
	static Operation asOp(int i, Operation defaultOp = kCopyA)
	{
		return (i >= kFirst && i <= kLast)
		? static_cast<Operation>(i) : defaultOp;
	}
	
	enum {
		RESAMPLE_OFF,    // don't auto-resample grids
		RESAMPLE_B,      // resample B to match A
		RESAMPLE_A,      // resample A to match B
		RESAMPLE_HI_RES, // resample higher-res grid to match lower-res
		RESAMPLE_LO_RES  // resample lower-res grid to match higher-res
	};typedef int ResampleMode;
	enum { RESAMPLE_MODE_FIRST = RESAMPLE_OFF, RESAMPLE_MODE_LAST = RESAMPLE_LO_RES };
	
	
	enum {
		INTERP_POINT,
		INTERP_LINEAR,
		INTERP_QUADRATIC
	};typedef int InterporateMode;
	
	static ResampleMode asResampleMode(int i, ResampleMode defaultMode = RESAMPLE_B)
	{
		return (i >= RESAMPLE_MODE_FIRST && i <= RESAMPLE_MODE_LAST)
		? static_cast<ResampleMode>(i) : defaultMode;
	}
	
	
	
	
	
	
	
	
	static MObject selectedGridNamesA;
	static MObject selectedGridNamesB;
	static MObject vdbInputA;
	static MObject vdbInputB;
	
	
	static MObject aVdbOutput;
	
	static MObject isFlatten;
	static MObject isPairs;
	static MObject operationType;
	static MObject multiplierA;
	static MObject multiplierB;
	static MObject resampleType;
	static MObject interporationType;
	static MObject deactivate;
	static MObject bgtolerance;
	static MObject isPrune;
	static MObject tolerance;
	static MObject flood;
	
	
	
	//attribute transfer no implement
private:
	
	template<typename> struct DispatchOp;
	struct CombineOp;
	
	
	GridPtr combineGrids(Operation op,
						 GridCPtr aGrid, GridCPtr bGrid,
						 const std::string& aGridName, const std::string& bGridName,
						 ResampleMode resample,
						 InterporateMode interp,
						 float multA,
						 float multB,
						 bool isPrune,
						 bool isDeactivate,
						 float bgtoleranceVal,
						 float toleranceVal,
						 bool isFlood
						 );
	
	
	bool needAGrid(Operation op) const
	{ return (op != kCopyB); }
	bool needBGrid(Operation op) const
	{ return (op != kCopyA && op != kInvert); }
	bool needLevelSets(Operation op) const
	{ return (op == kUnion || op == kIntersection || op == kDifference); }
};



#endif /* DOpenVDBCombine_h */
