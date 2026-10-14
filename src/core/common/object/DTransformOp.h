//
//  DTransformOp.hpp
//  dommyTool
//
//  Created by yuzo on 2017/07/02.
//


#pragma once


#include <stdio.h>
#include <cmath>
#include <vector>

#include "DMayaHeaders.h"
#include "DPData.h"



enum {
	MAT_ANIM_NONE = 0,
	MAT_ANIM_ATTRIBUTE,
	MAT_ANIM_INPUT_DATA,
	MAT_ANIM_TYPE_COUNT
};typedef short MatrixAnimationType;

struct DTransformParams
{
    DTransformParams()
    : mLocalTranslate(MVector(0.0,0.0,0.0))
	, mGlobalTranslate(MVector(0.0,0.0,0.0))
    , mLocalRot(MVector(0.0,0.0,0.0))
    , mGlobalRot(MVector(0.0,0.0,0.0))
    , mGlobalScale(MVector(1.0,1.0,1.0))
	, mUniformScale(1.0)
	, mLocalTransAttribute("")
	, mGlobalTransAttribute("")
	, mLocalRotAttribute("")
	, mGlobalRotAttribute("")
	, mGlobalScaleAttribute("")
	, mUniformScaleAttribute("")
    , mNormalBaseAxis(MVector(0.0,1.0,0.0))
    , mTangentBaseAxis(MVector(1.0,0.0,0.0))
    , mNormalVec(MVector(0.0,1.0,0.0))
    , mTangentVec(MVector(1.0,0.0,0.0))
	, mNormalVecAttribute("")
	, mTangentVecAttribute("")
    , mSeed(1)
	, mLocalTransRndMin(MVector(0.0,0.0,0.0))
	, mLocalTransRndMax(MVector(0.0,0.0,0.0))
	, mGlobalTransRndMin(MVector(0.0,0.0,0.0))
	, mGlobalTransRndMax(MVector(0.0,0.0,0.0))
    , mLocalRotRndMin(MVector(0.0,0.0,0.0))
    , mLocalRotRndMax(MVector(0.0,0.0,0.0))
    , mGlobalRotRndMin(MVector(0.0,0.0,0.0))
    , mGlobalRotRndMax(MVector(0.0,0.0,0.0))
    , mGlobalScaleRndMin(MVector(1.0,1.0,1.0))
    , mGlobalScaleRndMax(MVector(1.0,1.0,1.0))
    , mUniformScaleMinMax(MVector(1.0,1.0,1.0))
    , mExpressionStr("")
	, mAnimMatrixType(MAT_ANIM_NONE)
    , mAnimationInputIsData(false)
    { }
    
    
    //base transform
	MVector mLocalTranslate;
    MVector mGlobalTranslate;
    
    MVector mLocalRot;
    MVector mGlobalRot;
    
    MVector mGlobalScale;
	double mUniformScale;
	
	std::string mLocalTransAttribute;
	std::string mGlobalTransAttribute;
	std::string mLocalRotAttribute;
	std::string mGlobalRotAttribute;
	
	std::string mGlobalScaleAttribute;
	std::string mUniformScaleAttribute;
	
    //align
    MVector mNormalBaseAxis;
    MVector mTangentBaseAxis;
    
    MVector mNormalVec;
    MVector mTangentVec;
	
	std::string mNormalVecAttribute;
    std::string mTangentVecAttribute;
    
    //random
    size_t mSeed;
	MVector mLocalTransRndMin;
	MVector mLocalTransRndMax;
	MVector mGlobalTransRndMin;
	MVector mGlobalTransRndMax;
    MVector mLocalRotRndMin;
    MVector mLocalRotRndMax;
    MVector mGlobalRotRndMin;
    MVector mGlobalRotRndMax;
    MVector mGlobalScaleRndMin;
    MVector mGlobalScaleRndMax;
    MVector mUniformScaleMinMax;
	
    //expression
    std::string mExpressionStr;
    
	//for animation
	MObject mNodeObj;
	MatrixAnimationType mAnimMatrixType;
	std::string mAnimationAttribute;
	MObject mAnim_Mat_InputData_Object;

    // If true, mAnim_Mat_InputData_Object is a typed-data snapshot from a
    // compute block, not a plug attribute. Consume while that block is alive.
    bool mAnimationInputIsData;
	
	
    
};

inline void DMatrixDivide(const double &value, MMatrix &xform) {
    double m[4][4] = {  {xform[0][0]/value,xform[0][1]/value,xform[0][2]/value,xform[0][3]/value},
                        {xform[1][0]/value,xform[1][1]/value,xform[1][2]/value,xform[1][3]/value},
                        {xform[2][0]/value,xform[2][1]/value,xform[2][2]/value,xform[2][3]/value},
                        {xform[3][0]/value,xform[3][1]/value,xform[3][2]/value,xform[3][3]/value}
    };
    xform = MMatrix(m);
}

inline void DMakeTransform(const MVector &zAxis, const MVector &yAxis, MTransformationMatrix &xform ) {
    MVector z = zAxis;
    if (z.length() <= 1.0e-12) {
        xform = MTransformationMatrix();
        return;
    }
    z.normalize();

    MVector x = yAxis ^ z;
    if (x.length() <= 1.0e-12) {
        const MVector fallback =
            std::fabs(z * MVector::yAxis) < 0.9
                ? MVector::yAxis
                : MVector::xAxis;
        x = fallback ^ z;
    }
    if (x.length() <= 1.0e-12) {
        xform = MTransformationMatrix();
        return;
    }
    x.normalize();
    MVector y = z ^ x;
    if (y.length() <= 1.0e-12) {
        xform = MTransformationMatrix();
        return;
    }
    y.normalize();

    double values[4][4] = {
        {x.x, x.y, x.z, 0.0},
        {y.x, y.y, y.z, 0.0},
        {z.x, z.y, z.z, 0.0},
        {0.0, 0.0, 0.0, 1.0}
    };
    xform = MTransformationMatrix(MMatrix(values));
}


MStatus DGenerateTransformMatrixFromDP(const DPData* dpdata,
                                       const DTransformParams& param,
                                       std::vector<MMatrix>& matrixArray);
