//
//  DPMathUtility.h
//  dommyTool
//
//  Created by yuzo on 2/14/17.
//
//

#pragma once

#include "DMayaHeaders.h"

#include <tbb/blocked_range.h>
#include <tbb/parallel_reduce.h>
#include <tbb/parallel_for.h>

#include "DPointDataHeaders.h"
#include "DCurveDataHeaders.h"
#include "DMeshDataHeaders.h"

#include "DNodeOperation.h"
#include "DNumeric.h"
#include "DMathEvaluation.h"
#include "DProceduralMath.h"
#include "DVectorMath.h"


enum {
	DP_MATH_IN_INVAILED = -1,
	DP_MATH_IN_CONSTANT,
	DP_MATH_IN_MAYAPLUG,
	DP_MATH_IN_DPDATA,
	DP_MATH_INPUT_TYPE_COUNT
};typedef int DP_MATH_INPUT_TYPE;


enum {
    DP_MATH_ADD = 0,
    DP_MATH_SUBTRACT,
    DP_MATH_MULTIPLY,
    DP_MATH_DIVIDE,
    DP_MATH_NEGATIVE,
    DP_MATH_SURPLUS,//余り
    DP_MATH_MIN,
    DP_MATH_MAX,
    DP_MATH_SQRT,
    DP_MATH_FLOOR,
    DP_MATH_CEILING,
    DP_MATH_ROUND_INTEGER,
    DP_MATH_POW,
    DP_MATH_EXP,
    DP_MATH_AVERAGE,
    DP_MATH_ABSOLUTE,
    DP_MATH_TYPE_COUNT
};typedef int DP_MATH_TYPE;


#pragma mark math function


#pragma mark Utilities

#pragma mark base math
	//Add
    inline DScalarData DAdd(const DScalarData &a, const DVec2Data &b){ return a + b[0]; }
	inline DScalarData DAdd(const DScalarData &a, const DVec3Data &b){ return a + b[0]; }
	inline DVec2Data DAdd(const DVec2Data &a, const DScalarData &b){ return DVec2Data(a[0]+b, a[1]+b); }
	inline DVec2Data DAdd(const DVec2Data &a, const DVec2Data &b){ return DVec2Data(a[0]+b[0], a[1]+b[1]); }
    inline DVec2Data DAdd(const DVec2Data &a, const DVec3Data &b){ return DVec2Data(a[0]+b[0], a[1]+b[1]); }
	inline DVec3Data DAdd(const DVec3Data &a, const DScalarData &b){ return DVec3Data(a[0]+b, a[1]+b, a[2]+b); }
    inline DVec3Data DAdd(const DVec3Data &a, const DVec2Data &b){ return DVec3Data(a[0]+b[0], a[1]+b[1], a[2]); }
	inline DVec3Data DAdd(const DVec3Data &a, const DVec3Data &b){ return DVec3Data(a[0]+b[0], a[1]+b[1], a[2]+b[2]); }


	//Subtract
    inline DScalarData DSub(const DScalarData &a, const DVec2Data &b){ return a - b[0]; }
	inline DScalarData DSub(const DScalarData &a, const DVec3Data &b){ return a - b[0]; }
	inline DVec2Data DSub(const DVec2Data &a, const DScalarData &b){ return DVec2Data(a[0]-b, a[1]-b); }
	inline DVec2Data DSub(const DVec2Data &a, const DVec2Data &b){ return DVec2Data(a[0]-b[0], a[1]-b[1]); }
    inline DVec2Data DSub(const DVec2Data &a, const DVec3Data &b){ return DVec2Data(a[0]-b[0], a[1]-b[1]); }
	inline DVec3Data DSub(const DVec3Data &a, const DScalarData &b){ return DVec3Data(a[0]-b, a[1]-b, a[2]-b); }
    inline DVec3Data DSub(const DVec3Data &a, const DVec2Data &b){ return DVec3Data(a[0]-b[0], a[1]-b[1], a[2]); }
	inline DVec3Data DSub(const DVec3Data &a, const DVec3Data &b){ return DVec3Data(a[0]-b[0], a[1]-b[1], a[2]-b[2]); }


	//Multiply
    inline DScalarData DMult(const DScalarData &a, const DVec2Data &b){ return a * b[0]; }
	inline DScalarData DMult(const DScalarData &a, const DVec3Data &b){ return a * b[0]; }
	inline DVec2Data DMult(const DVec2Data &a, const DScalarData &b){ return a*b; }
	inline DVec2Data DMult(const DVec2Data &a, const DVec2Data &b){ return DVec2Data(a[0]*b[0], a[1]*b[1]); }
    inline DVec2Data DMult(const DVec2Data &a, const DVec3Data &b){ return DVec2Data(a[0]*b[0], a[1]*b[1]); }
	inline DVec3Data DMult(const DVec3Data &a, const DScalarData &b){ return a*b; }
    inline DVec3Data DMult(const DVec3Data &a, const DVec2Data &b){ return DVec3Data(a[0]*b[0], a[1]*b[1], a[2]); }
	inline DVec3Data DMult(const DVec3Data &a, const DVec3Data &b){ return DVec3Data(a[0]*b[0], a[1]*b[1], a[2]*b[2]); }
	
	
	//Divide
    inline DScalarData DDiv(const DScalarData &a, const DVec2Data &b){ return dommy::math::divideOrZero(a, b[0]); }
	inline DScalarData DDiv(const DScalarData &a, const DVec3Data &b){ return dommy::math::divideOrZero(a, b[0]); }
	inline DVec2Data DDiv(const DVec2Data &a, const DScalarData &b){ return DVec2Data(dommy::math::divideOrZero(a[0], b), dommy::math::divideOrZero(a[1], b)); }
	inline DVec2Data DDiv(const DVec2Data &a, const DVec2Data &b){ return DVec2Data(dommy::math::divideOrZero(a[0], b[0]), dommy::math::divideOrZero(a[1], b[1])); }
    inline DVec2Data DDiv(const DVec2Data &a, const DVec3Data &b){ return DVec2Data(dommy::math::divideOrZero(a[0], b[0]), dommy::math::divideOrZero(a[1], b[1])); }
	inline DVec3Data DDiv(const DVec3Data &a, const DScalarData &b){ return DVec3Data(dommy::math::divideOrZero(a[0], b), dommy::math::divideOrZero(a[1], b), dommy::math::divideOrZero(a[2], b)); }
    inline DVec3Data DDiv(const DVec3Data &a, const DVec2Data &b){ return DVec3Data(dommy::math::divideOrZero(a[0], b[0]), dommy::math::divideOrZero(a[1], b[1]), a[2]); }
	inline DVec3Data DDiv(const DVec3Data &a, const DVec3Data &b){ return DVec3Data(dommy::math::divideOrZero(a[0], b[0]), dommy::math::divideOrZero(a[1], b[1]), dommy::math::divideOrZero(a[2], b[2])); }


	//fMod余り fmodは、C99 の規格にある fmod（より正確には math.h ヘッダの fmod、fmodf、fmodl の 3 つ。それぞれ C++ の double、float、long double バージョンに相当）と同等である。
    inline DScalarData DMod(const DScalarData &a, const DVec2Data &b){ return dommy::math::moduloOrZero(a, b[0]); }
	inline DScalarData DMod(const DScalarData &a, const DVec3Data &b){ return dommy::math::moduloOrZero(a, b[0]); }
	inline DVec2Data DMod(const DVec2Data &a, const DScalarData &b){ return DVec2Data(dommy::math::moduloOrZero(a[0], b), dommy::math::moduloOrZero(a[1], b)); }
	inline DVec2Data DMod(const DVec2Data &a, const DVec2Data &b){ return DVec2Data(dommy::math::moduloOrZero(a[0], b[0]), dommy::math::moduloOrZero(a[1], b[1])); }
    inline DVec2Data DMod(const DVec2Data &a, const DVec3Data &b){ return DVec2Data(dommy::math::moduloOrZero(a[0], b[0]), dommy::math::moduloOrZero(a[1], b[1])); }
	inline DVec3Data DMod(const DVec3Data &a, const DScalarData &b){ return DVec3Data(dommy::math::moduloOrZero(a[0], b), dommy::math::moduloOrZero(a[1], b), dommy::math::moduloOrZero(a[2], b)); }
    inline DVec3Data DMod(const DVec3Data &a, const DVec2Data &b){ return DVec3Data(dommy::math::moduloOrZero(a[0], b[0]), dommy::math::moduloOrZero(a[1], b[1]), a[2]); }
	inline DVec3Data DMod(const DVec3Data &a, const DVec3Data &b){ return DVec3Data(dommy::math::moduloOrZero(a[0], b[0]), dommy::math::moduloOrZero(a[1], b[1]), dommy::math::moduloOrZero(a[2], b[2])); }


	//Pow
    inline DScalarData DPow(const DScalarData &a, const DVec2Data &b){ return dommy::math::powerOrZero(a, b[0]); }
	inline DScalarData DPow(const DScalarData &a, const DVec3Data &b){ return dommy::math::powerOrZero(a, b[0]); }
	inline DVec2Data DPow(const DVec2Data &a, const DScalarData &b){ return DVec2Data(dommy::math::powerOrZero(a[0], b), dommy::math::powerOrZero(a[1], b)); }
	inline DVec2Data DPow(const DVec2Data &a, const DVec2Data &b){ return DVec2Data(dommy::math::powerOrZero(a[0], b[0]), dommy::math::powerOrZero(a[1], b[1])); }
    inline DVec2Data DPow(const DVec2Data &a, const DVec3Data &b){ return DVec2Data(dommy::math::powerOrZero(a[0], b[0]), dommy::math::powerOrZero(a[1], b[1])); }
	inline DVec3Data DPow(const DVec3Data &a, const DScalarData &b){ return DVec3Data(dommy::math::powerOrZero(a[0], b), dommy::math::powerOrZero(a[1], b), dommy::math::powerOrZero(a[2], b)); }
    inline DVec3Data DPow(const DVec3Data &a, const DVec2Data &b){ return DVec3Data(dommy::math::powerOrZero(a[0], b[0]), dommy::math::powerOrZero(a[1], b[1]), a[2]); }
	inline DVec3Data DPow(const DVec3Data &a, const DVec3Data &b){ return DVec3Data(dommy::math::powerOrZero(a[0], b[0]), dommy::math::powerOrZero(a[1], b[1]), dommy::math::powerOrZero(a[2], b[2])); }


	//Min
    inline DScalarData DMin(const DScalarData &a, const DVec2Data &b){ return  fminf(a,b[0]); }
	inline DScalarData DMin(const DScalarData &a, const DVec3Data &b){ return  fminf(a,b[0]); }
	inline DVec2Data DMin(const DVec2Data &a, const DScalarData &b){ return DVec2Data(fminf(a[0],b), fminf(a[1],b)); }
	inline DVec2Data DMin(const DVec2Data &a, const DVec2Data &b){ return DVec2Data(fminf(a[0],b[0]), fminf(a[1],b[1])); }
    inline DVec2Data DMin(const DVec2Data &a, const DVec3Data &b){ return DVec2Data(fminf(a[0],b[0]), fminf(a[1],b[1])); }
	inline DVec3Data DMin(const DVec3Data &a, const DScalarData &b){ return DVec3Data(fminf(a[0],b), fminf(a[1],b), fminf(a[2],b)); }
    inline DVec3Data DMin(const DVec3Data &a, const DVec2Data &b){ return DVec3Data(fminf(a[0],b[0]), fminf(a[1],b[1]), a[2]); }
	inline DVec3Data DMin(const DVec3Data &a, const DVec3Data &b){ return DVec3Data(fminf(a[0],b[0]), fminf(a[1],b[1]), fminf(a[2],b[2])); }
	
	
	//Max
    inline DScalarData DMax(const DScalarData &a, const DVec2Data &b){ return  fmaxf(a,b[0]); }
	inline DScalarData DMax(const DScalarData &a, const DVec3Data &b){ return  fmaxf(a,b[0]); }
	inline DVec2Data DMax(const DVec2Data &a, const DScalarData &b){ return DVec2Data(fmaxf(a[0],b), fmaxf(a[1],b)); }
	inline DVec2Data DMax(const DVec2Data &a, const DVec2Data &b){ return DVec2Data(fmaxf(a[0],b[0]), fmaxf(a[1],b[1])); }
    inline DVec2Data DMax(const DVec2Data &a, const DVec3Data &b){ return DVec2Data(fmaxf(a[0],b[0]), fmaxf(a[1],b[1])); }
	inline DVec3Data DMax(const DVec3Data &a, const DScalarData &b){ return DVec3Data(fmaxf(a[0],b), fmaxf(a[1],b), fmaxf(a[2],b)); }
    inline DVec3Data DMax(const DVec3Data &a, const DVec2Data &b){ return DVec3Data(fmaxf(a[0],b[0]), fmaxf(a[1],b[1]), a[2]); }
	inline DVec3Data DMax(const DVec3Data &a, const DVec3Data &b){ return DVec3Data(fmaxf(a[0],b[0]), fmaxf(a[1],b[1]), fmaxf(a[2],b[2])); }


#pragma mark source only math
	
	
	//Negative ///Source only
	inline DVec2Data DNegative(DVec2Data a){ return  -a; }
	inline DVec3Data DNegative(DVec3Data a){ return  -a; }
	
	
	//Squrt ///Source only
	inline DVec2Data DSqrt(DVec2Data a){ return DVec2Data(dommy::math::sqrtOrZero(a[0]), dommy::math::sqrtOrZero(a[1])); }
	inline DVec3Data DSqrt(DVec3Data a){ return DVec3Data(dommy::math::sqrtOrZero(a[0]), dommy::math::sqrtOrZero(a[1]), dommy::math::sqrtOrZero(a[2])); }
	
	
	//Floor ///Source only
	inline DVec2Data DFloor(DVec2Data a){ return  DVec2Data(floorf(a[0]), floorf(a[1])); }
	inline DVec3Data DFloor(DVec3Data a){ return  DVec3Data(floorf(a[0]), floorf(a[1]), floorf(a[2])); }
	
	//Round ///Source only
	inline DVec2Data DRound(DVec2Data a){ return  DVec2Data(roundf(a[0]), roundf(a[1])); }
	inline DVec3Data DRound(DVec3Data a){ return  DVec3Data(roundf(a[0]), roundf(a[1]), roundf(a[2])); }
	
	//Ceil ///Source only
	inline DVec2Data DCeil(DVec2Data a){ return  DVec2Data(ceilf(a[0]), ceilf(a[1])); }
	inline DVec3Data DCeil(DVec3Data a){ return  DVec3Data(ceilf(a[0]), ceilf(a[1]), ceilf(a[2])); }
	
	//Exp ///Source only
	inline DVec2Data DExponential(DVec2Data a){ return  DVec2Data(dommy::math::exponentialOrZero(a[0]), dommy::math::exponentialOrZero(a[1])); }
	inline DVec3Data DExponential(DVec3Data a){ return  DVec3Data(dommy::math::exponentialOrZero(a[0]), dommy::math::exponentialOrZero(a[1]), dommy::math::exponentialOrZero(a[2])); }
	
	
	//abs ///Source only
	inline DVec2Data DAbsolute(DVec2Data a){ return  DVec2Data(fabsf(a[0]), fabsf(a[1])); }
	inline DVec3Data DAbsolute(DVec3Data a){ return  DVec3Data(fabsf(a[0]), fabsf(a[1]), fabsf(a[2])); }


#pragma mark function enum


	//source only
	extern DScalarData (* const ScalarMathFncForSourceOps[])(DScalarData);
	extern DVec2Data (* const Vec2DMathFncForSourceOps[])(DVec2Data);
	extern DVec3Data (* const Vec3DMathFncForSourceOps[])(DVec3Data);


	//scalar method
	extern DScalarData (* const ScalarMathFncForScalarOps[])(const DScalarData&, const DScalarData&);
    extern DScalarData (* const ScalarMathFncForVec2Ops[])(const DScalarData&, const DVec2Data&);
	extern DScalarData (* const ScalarMathFncForVectorOps[])(const DScalarData&, const DVec3Data&);
	
	//2d vector method
	extern DVec2Data (* const Vec2DMathFncForScalarOps[])(const DVec2Data&, const DScalarData&);
	extern DVec2Data (* const Vec2DMathFncForVec2DOps[])(const DVec2Data&, const DVec2Data&);
    extern DVec2Data (* const Vec2DMathFncForVectorOps[])(const DVec2Data&, const DVec3Data&);
	
	//3d vector method
	extern DVec3Data (* const Vec3DMathFncForScalarOps[])(const DVec3Data&, const DScalarData&);
    extern DVec3Data (* const Vec3DMathFncForVec2DOps[])(const DVec3Data&, const DVec2Data&);
	extern DVec3Data (* const Vec3DMathFncForVectorOps[])(const DVec3Data&, const DVec3Data&);


#pragma mark vector math
	
	//Dot Product
	inline DScalarData DDotProduct(DVec2Data a, DVec2Data b){ return a.dot(b); }
	inline DScalarData DDotProduct(DVec3Data a, DVec3Data b){ return a.dot(b); }
	inline DScalarData DDotProduct(MVector a, MVector b){ return a*b; }
	
    //Angle
    inline DScalarData DAngle(DVec2Data a, DVec2Data b){ return dommy::math::angleRadians(a.dot(b), a.length(), b.length()); }
    inline DScalarData DAngle(DVec3Data a, DVec3Data b){ return dommy::math::angleRadians(a.dot(b), a.length(), b.length()); }
    inline DScalarData DAngle(MVector a, MVector b){ return dommy::math::angleRadians(a*b, a.length(), b.length()); }
    
	//length
	inline DScalarData DLength(DVec2Data a){ return a.length(); }
	inline DScalarData DLength(DVec3Data a){ return a.length(); }
	inline DScalarData DLength(MVector a){ return a.length(); }
	
	//min component
	inline DScalarData DMinComponent(DVec2Data a){ return fminf(a[0], a[1]); }
	inline DScalarData DMinComponent(DVec3Data a){ return fminf(fminf(a[0], a[1]),a[2]); }
	inline DScalarData DMinComponent(MVector a){ return fminf(fminf(a[0], a[1]),a[2]); }
	
	//max component
	inline DScalarData DMaxComponent(DVec2Data a){ return fmaxf(a[0], a[1]); }
	inline DScalarData DMaxComponent(DVec3Data a){ return fmaxf(fmaxf(a[0], a[1]),a[2]); }
	inline DScalarData DMaxComponent(MVector a){ return fmaxf(fmaxf(a[0], a[1]),a[2]); }
	
	//avg component
	inline DScalarData DAverageComponent(DVec2Data a){ return (a[0]+a[1])*0.5; }
	inline DScalarData DAverageComponent(DVec3Data a){ return (a[0]+a[1]+a[2])/3.0; }
	inline DScalarData DAverageComponent(MVector a){ return (a[0]+a[1]+a[2])/3.0; }
	
	//Normalize
	inline DVec2Data DNormalize(DVec2Data a){
		DScalarData x = 0.0f;
		DScalarData y = 0.0f;
		dommy::math::normalized2OrZero(a[0], a[1], x, y);
		return DVec2Data(x, y);
	}
	inline DVec3Data DNormalize(DVec3Data a){
		DScalarData x = 0.0f;
		DScalarData y = 0.0f;
		DScalarData z = 0.0f;
		dommy::math::normalized3OrZero(a[0], a[1], a[2], x, y, z);
		return DVec3Data(x, y, z);
	}
	inline MVector DNormalize(MVector a){
		double x = 0.0;
		double y = 0.0;
		double z = 0.0;
		dommy::math::normalized3OrZero(a[0], a[1], a[2], x, y, z);
		return MVector(x, y, z);
	}
    
    //Cross Product
    inline DVec3Data DCrossProduct(DVec3Data a, DVec3Data b){ return a.cross(b); }
	inline DVec3Data DCrossProduct(MVector a, MVector b){ MVector c=a^b; return DVec3Data(c[0],c[1],c[2]);  }
	
	
	enum {
		D_VEC_LENGTH = 0,
		D_VEC_MIN_COMP,
		D_VEC_MAX_COMP,
		D_VEC_AVG_COMP,
		D_VEC_COUNT
	};typedef int D_VEC_MATH_ENUM;
	
	extern DScalarData (* const Vec3DMathFncForSingleOps[])(DVec3Data);
    extern DScalarData (* const MVec3DMathFncForSingleOps[])(MVector);
    
	
#pragma mark other math
	
	template <typename T>
	inline void DFitRange(T smin, T smax, T dmin, T dmax, T value, T &result) {
		result = dommy::math::fitRange(smin, smax, dmin, dmax, value);
	}
	
    template <typename T>
    inline void DFitRangeClamp(T smin, T smax, T dmin, T dmax, T value, T &result) {
        result = dommy::math::fitRangeClamped(smin, smax, dmin, dmax, value);
    }

#pragma mark Get constant value

MStatus DPMGetConstantValues(
    MDataBlock& data,
    const MObject& plug,
    const size_t& count,
    std::vector<DMathVec3DataList>& targetVectorList);

#pragma mark Get Input data value

MStatus DPMGetInputDataValues(
    MDataBlock& data,
    const MObject& plug,
    const size_t& count,
    std::vector<DMathScalarDataList>& targetScalarList,
    std::vector<DMathVec3DataList>& targetVectorList);


#pragma mark  DPoint attribute
MStatus DPMGetDPointAttributeValues(
    MDataBlock& data,
    const MObject& plug,
    const size_t& count,
    DPData& dpdata,
    std::vector<DMathScalarDataList>& targetScalarList,
    std::vector<DMathVec2DataList>& targetVec2DList,
    std::vector<DMathVec3DataList>& targetVectorList);


#pragma mark DCurve prim attribute
MStatus DPMGetDCurvePrimAttributeValues(
    MDataBlock& data,
    const MObject& plug,
    DCurveData& dcdata,
    std::vector<DMathScalarDataList>& targetScalarList,
    std::vector<DMathVec2DataList>& targetVec2DList,
    std::vector<DMathVec3DataList>& targetVectorList);


#pragma mark DCurve point attribute
MStatus DPMGetDCurvePointAttributeValues(
    MDataBlock& data,
    const MObject& plug,
    DCurveData& dcdata,
    std::vector<std::vector<DMathScalarDataList> >& targetScalarList,
    std::vector<std::vector<DMathVec2DataList> >& targetVec2DList,
    std::vector<std::vector<DMathVec3DataList> >& targetVectorList);


#pragma mark DMesh Prim Attribute

MStatus DPMGetDMeshPrimAttributeValues(
    MDataBlock& data,
    const MObject& plug,
    DMeshData& dmdata,
    std::vector<DMathScalarDataList>& targetScalarList,
    std::vector<DMathVec2DataList>& targetVec2DList,
    std::vector<DMathVec3DataList>& targetVectorList);


#pragma mark DMesh Point Attribute

MStatus DPMGetDMeshPointAttributeValues(
    MDataBlock& data,
    const MObject& plug,
    DMeshData& dmdata,
    std::vector<std::vector<DMathScalarDataList> >& targetScalarList,
    std::vector<std::vector<DMathVec2DataList> >& targetVec2DList,
    std::vector<std::vector<DMathVec3DataList> >& targetVectorList);


#pragma mark single arg calc method
MStatus sourceCalcScalarValue(const D_SRC_SINGLE_MATH_ENUM &calcType, DMathScalarDataList &result);
MStatus sourceCalcVec2DValue(const D_SRC_SINGLE_MATH_ENUM &calcType, DMathVec2DataList &result);
MStatus sourceCalcVec3DValue(const D_SRC_SINGLE_MATH_ENUM &calcType, DMathVec3DataList &result);


#pragma mark multi arg calc calc method

MStatus calcScalarValue(const D_CALC_MATH_ENUM &calcType,
					 const std::vector<DMathScalarDataList> &targetScalarList,
                     const std::vector<DMathVec2DataList> &targetVec2DList,
                     const std::vector<DMathVec3DataList> &targetVectorList,
                     DMathScalarDataList &result);

MStatus calc2DVecValue(const D_CALC_MATH_ENUM &calcType,
					const std::vector<DMathScalarDataList> &targetScalarList,
                    const std::vector<DMathVec2DataList> &targetVec2DList,
                    const std::vector<DMathVec3DataList> &targetVectorList,
                    DMathVec2DataList &result);

MStatus calcVectorValue(const D_CALC_MATH_ENUM &calcType,
					 const std::vector<DMathScalarDataList> &targetScalarList,
                     const std::vector<DMathVec2DataList> &targetVec2DList,
                     const std::vector<DMathVec3DataList> &targetVectorList,
                     DMathVec3DataList &result);


#pragma mark single arg process


//DPoint single routine
MStatus processDPointMathFncSingle(DPData &dpdata,
								const std::string &nodeName,
								const std::string &sourceAttrName,
								const std::string &outAttrName,
								const bool &isOutAttr,
								const bool &isDeleteOrg,
								MDataBlock& data,
								const D_SRC_SINGLE_MATH_ENUM &calcType);

MStatus processDCurveMathFncSingle(DCurveData &dcdata,
								const std::string &nodeName,
								const std::string &sourceAttrName,
								const std::string &outAttrName,
								const bool &isOutAttr,
								const bool &isDeleteOrg,
								MDataBlock& data,
								const D_SRC_SINGLE_MATH_ENUM &calcType);

MStatus processDMeshMathFncSingle(DMeshData &dmdata,
							   const std::string &nodeName,
							   const std::string &sourceAttrName,
							   const std::string &outAttrName,
							   const bool &isOutAttr,
							   const bool &isDeleteOrg,
							   MDataBlock& data,
							   const D_SRC_SINGLE_MATH_ENUM &calcType);

MStatus processDDataMathOutputSingle(
    const MObject& inputDataAttribute,
    const MObject& outputDataAttribute,
    const std::string& nodeName,
    const std::string& sourceAttrName,
    const std::string& outAttrName,
    bool isOutAttr,
    bool isDeleteOrg,
    MDataBlock& data,
    const MPlug& requestedPlug,
    D_SRC_SINGLE_MATH_ENUM calcType);

MStatus processMayaNumericMathFuncSingle(MObject node,
								   const std::string &nodeName,
								   const MObject &targetSrcDataPlug,
								   const MObject &outdataPlug,
								   MDataBlock& data,
								   const MPlug& plug,
								   const D_SRC_SINGLE_MATH_ENUM &calcType);

MStatus processMayaNumericMathFuncForVector(
    const MObject& node,
    const std::string& nodeName,
    const MObject& sourceDataPlug,
    short targetType,
    const MObject& targetConstPlug,
    const MObject& targetInputDataPlug,
    DVec3DataList& sourceVectors,
    DVec3DataList& targetVectors,
    MDataBlock& data);

#pragma mark multi arg process

MStatus processDPointMathFnc(DPData &dpdata,
                          const std::string &nodeName,
                          const std::string &sourceAttrName,
                          const std::string &outAttrName,
                          const bool &isOutAttr,
                          const bool &isDeleteOrg,
                          const MObject &targetConstPlug,
                          const MObject &targetInputDataPlug,
                          const MObject &targetAttrPlug,
                          MDataBlock& data,
                          const D_CALC_MATH_ENUM &calcType);


MStatus processDCurveMathFnc(DCurveData &dcdata,
                          const std::string &nodeName,
                          const std::string &sourceAttrName,
                          const std::string &outAttrName,
                          const bool &isOutAttr,
                          const bool &isDeleteOrg,
                          const MObject &targetConstPlug,
                          const MObject &targetInputDataPlug,
                          const MObject &targetAttrPlug,
                          MDataBlock& data,
                          const D_CALC_MATH_ENUM &calcType);


MStatus processDMeshMathFnc(DMeshData &dmdata,
                         const std::string &nodeName,
                         const std::string &sourceAttrName,
                         const std::string &outAttrName,
                         const bool &isOutAttr,
                         const bool &isDeleteOrg,
                         const MObject &targetConstPlug,
                         const MObject &targetInputDataPlug,
                         const MObject &targetAttrPlug,
                         MDataBlock& data,
                         const D_CALC_MATH_ENUM &calcType);

MStatus processDDataMathOutput(
    const MObject& inputDataAttribute,
    const MObject& outputDataAttribute,
    const std::string& nodeName,
    const std::string& sourceAttrName,
    const std::string& outAttrName,
    bool isOutAttr,
    bool isDeleteOrg,
    const MObject& targetConstPlug,
    const MObject& targetInputDataPlug,
    const MObject& targetAttrPlug,
    MDataBlock& data,
    const MPlug& requestedPlug,
    D_CALC_MATH_ENUM calcType);


MStatus processMayaNumericMathFunc(const MObject &node,
                                   const std::string &nodeName,
                                    MObject &targetSrcDataPlug,
                                    MObject &outdataPlug,
                                    MObject &targetConstPlug,
                                    MObject &targetInputDataPlug,
                                   MDataBlock& data,
                                   const MPlug& plug,
                                   const D_CALC_MATH_ENUM &calcType);
