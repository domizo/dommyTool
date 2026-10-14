// DOMMY_THIRD_PARTY_NOTICE_BEGIN
//-*****************************************************************************
//
// Copyright (c) 2009-2012,
//  Sony Pictures Imageworks Inc. and
//  Industrial Light & Magic, a division of Lucasfilm Entertainment Company Ltd.
//
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
// *       Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
// *       Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
// *       Neither the name of Sony Pictures Imageworks, nor
// Industrial Light & Magic, nor the names of their contributors may be used
// to endorse or promote products derived from this software without specific
// prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
//-*****************************************************************************
// DOMMY_THIRD_PARTY_NOTICE_END
// Modified for dommyTool; not the original upstream version.
// Source comparison reference: https://github.com/alembic/alembic/blob/895a7c6a8b3396a21c6ba03ddf91ed91514d7b93/maya/AbcExport/AbcWriteJob.h

//
//  DCAbcWriteJob.hpp
//  dommyTool
//
//  Created by yuzo on 3/16/17.
//
//

#pragma once


#include "DMayaHeaders.h"

#include "DPointWriter.h"
#include "DCurveWriter.h"
#include "DMeshWriter.h"
#include "MayaTransformWriter.h"
#include "MayaAttributeWriter.h"
#include "MayaLocatorWriter.h"

#include "DDataExportUtil.h"
#include "common/utility/DWorkBudget.h"
#include "common/utility/DCheckedSize.h"
#include <cstdint>
#include <stdexcept>
#include <functional>
#include <vector>


typedef Alembic::Util::shared_ptr < DPointWriter > DPointWriterPtr;
typedef Alembic::Util::shared_ptr < DCurveWriter > DCurveWriterPtr;
typedef Alembic::Util::shared_ptr < DMeshWriter > DMeshWriterPtr;
typedef Alembic::Util::shared_ptr < MayaLocatorWriter > MayaLocatorWriterPtr;


struct AbcWriteJobStatistics
{
	AbcWriteJobStatistics()
	{
		
		mDCStaticNum = 0;
		mDCStaticCurves = 0;
		mDCAnimNum = 0;
		mDCAnimCurves = 0;
		mDCStaticCVs = 0;
        mDCAnimCVs = 0;
        mDPAnimPts = 0;
        mDMAnimVtxs = 0;
        
		mTransStaticNum = 0;
		mTransAnimNum = 0;
		
		mLocatorStaticNum = 0;
		mLocatorAnimNum = 0;
		
	}
	
	// for the statistic string
	std::uint64_t mDCStaticNum;
	std::uint64_t mDCStaticCurves;
	std::uint64_t mDCAnimNum;
	std::uint64_t mDCAnimCurves;
	std::uint64_t mDCStaticCVs;
    
	std::uint64_t mDCAnimCVs;
	std::uint64_t mDPAnimPts;
    std::uint64_t mDMAnimVtxs;
	
	std::uint64_t mTransStaticNum;
	std::uint64_t mTransAnimNum;
	
	std::uint64_t mLocatorStaticNum;
	std::uint64_t mLocatorAnimNum;

    static void add(std::uint64_t& total, std::uint64_t value) {
        if (!dommy::memory::checkedUnsignedAdd(total, value, total))
            throw std::overflow_error("Alembic statistics overflow");
    }
	
};

class DCAbcWriteJob
{
public:
	
	DCAbcWriteJob(const char * iFileName, bool asOgawa,
				  const std::set<double> & iTransFrames,
				  Alembic::AbcCoreAbstract::TimeSamplingPtr iTransTime,
				  const std::set<double> & iShapeFrames,
				  Alembic::AbcCoreAbstract::TimeSamplingPtr iShapeTime,
				  Alembic::AbcCoreAbstract::TimeSamplingPtr iBoundsTime,
				  const JobArgs & iArgs);
	
	~DCAbcWriteJob();
	
	// returns true if eval has been called on the last frame
	bool eval(double iFrame);
	
private:
	
	void perFrameCallback(double iFrame);
	void postCallback(double iFrame);
	
	MBoundingBox getBoundingBox() const;
	void setup(double iFrame, MayaTransformWriterPtr iParent, std::size_t parentIndex,
               std::size_t depth, dommy::generation::WorkBudget& work);
    void trackShapeBounds(std::function<MBoundingBox()> read, std::size_t parentIndex);
    struct TransformBoundsEntry {
        MayaTransformWriterPtr writer;
        std::size_t parent;
    };
    struct ShapeBoundsEntry {
        std::function<MBoundingBox()> read;
        std::size_t parent;
    };
    // Retain static writers too: bounds must use the last exported samples,
    // not re-evaluate viewport fallback boxes or unsampled Maya geometry.
    std::vector<TransformBoundsEntry> mBoundsTransforms;
    std::vector<ShapeBoundsEntry> mBoundsShapes;
	
	std::vector< MayaTransformWriterPtr > mTransList;
	std::vector< MayaAttributeWriterPtr > mTransAttrList;
	
    std::vector< DPointWriterPtr > mPointList;
	std::vector< DCurveWriterPtr > mCurveList;
    std::vector< DMeshWriterPtr > mMeshList;
    
	std::vector< MayaAttributeWriterPtr > mShapeAttrList;
	std::vector< MayaLocatorWriterPtr > mLocatorList;
	
	// helper dag path for recursive calculations
	MDagPath mCurDag;
	
	// the root world node of the scene
	Alembic::Abc::OArchive mRoot;
	
	std::string mFileName;
	bool mAsOgawa;
	
	MSelectionList mSList;
	std::set<double> mShapeFrames;
	Alembic::AbcCoreAbstract::TimeSamplingPtr mShapeTime;
	Alembic::Util::uint32_t mShapeTimeIndex;
	Alembic::Util::uint32_t mShapeSamples;
	
	std::set<double> mTransFrames;
	Alembic::AbcCoreAbstract::TimeSamplingPtr mTransTime;
	Alembic::Util::uint32_t mTransTimeIndex;
	Alembic::Util::uint32_t mTransSamples;
	
	// when eval is called and the time is the first frame
	// then we run the setup
	double mFirstFrame;
	
	// when eval is called and the time is the last frame
	// then we also call the post callback
	double mLastFrame;
	
	Alembic::Abc::OBox3dProperty mBoxProp;
    Alembic::AbcCoreAbstract::TimeSamplingPtr mBoundsTime;
    Alembic::Util::uint32_t mBoundsTimeIndex;
    Alembic::Util::uint32_t mBoundsSamples;
	
	AbcWriteJobStatistics mStats;
	JobArgs mArgs;
};

typedef Alembic::Util::shared_ptr < DCAbcWriteJob > DCAbcWriteJobPtr;
